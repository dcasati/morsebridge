#include "BlePaddleKeyboard.h"
#include "PaddleInput.h"

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <atomic>

#if !defined(CONFIG_IDF_TARGET_ESP32S3) || !defined(CONFIG_NIMBLE_ENABLED)
#error "Morse Paddle requires ESP32-S3 with the Arduino 3.3.8 NimBLE stack"
#endif

namespace MorseBle {
namespace {

// Standard keyboard report: modifiers, reserved byte, six key usages.
uint8_t reportMap[] = {
  0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,
  0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
  0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
  0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
  0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01,
  0x29, 0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
  0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
  0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0
};

BLEServer *server = nullptr;
BLECharacteristic *input = nullptr;
std::atomic<bool> subscribed{false};
std::atomic<bool> active{false};
std::atomic<bool> connected{false};
std::atomic<bool> authenticated{false};
std::atomic<bool> suspended{false};
std::atomic<bool> restartAdvertising{false};
std::atomic<bool> failed{false};
std::atomic<bool> reportAccepted{false};
std::atomic<uint32_t> connectionEpoch{0};
PaddleInput paddles;
bool initialized = false;
bool wasReady = false;
uint8_t lastModifiers = 0xFF;
uint32_t lastAttempt = 0;
uint32_t observedEpoch = 0;
uint8_t reportFailures = 0;
uint32_t advertisingStartedAt = 0;

bool ready() {
  return active && connected && authenticated && !suspended && !failed
      && subscribed;
}

void clearReport() {
  const KeyboardReport neutral;
  input->setValue(reinterpret_cast<const uint8_t *>(&neutral), sizeof(neutral));
}

void advertisingComplete(BLEAdvertising *) {
  if (active && !connected) {
    failed = true;
    Serial.println("BLE: advertising stopped unexpectedly; restart the ESP32");
  }
}

bool startAdvertising() {
  advertisingStartedAt = millis();
  // NimBLE returns the start result synchronously, unlike Bluedroid GAP events.
  if (BLEDevice::getAdvertising()->start(0, advertisingComplete)) return true;
  failed = true;
  Serial.println("BLE: advertising request failed");
  return false;
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *device, ble_gap_conn_desc *desc) override {
    authenticated = false;
    subscribed = false;
    suspended = false;
    clearReport();
    connected = true;
    ++connectionEpoch;
    if (!active) {
      device->disconnect(desc->conn_handle);
      return;
    }
    Serial.println("BLE: connected; waiting for encrypted HID subscription");
    if (!device->requestConnParams(desc->conn_handle, 12, 24, 0, 400)) {
      Serial.println("BLE: connection interval request failed; using host interval");
    }
  }

  void onDisconnect(BLEServer *) override {
    authenticated = false;
    subscribed = false;
    connected = false;
    ++connectionEpoch;
    restartAdvertising = active.load();
    Serial.println("BLE: disconnected");
  }
};

class SecurityCallbacks : public BLESecurityCallbacks {
  bool onSecurityRequest() override { return active.load(); }
  uint32_t onPassKeyRequest() override { return 0; }
  void onPassKeyNotify(uint32_t) override {}
  bool onConfirmPIN(uint32_t) override { return false; }

  void onAuthenticationComplete(ble_gap_conn_desc *desc) override {
    authenticated = desc->sec_state.encrypted && desc->sec_state.bonded;
    if (!authenticated) {
      failed = true;
      Serial.printf("BLE: pairing failed (encrypted=%u, bonded=%u); forget device and restart\n",
                    unsigned(desc->sec_state.encrypted), unsigned(desc->sec_state.bonded));
      server->disconnect(desc->conn_handle);
    } else {
      Serial.println("BLE: encrypted and bonded");
    }
  }
};

class ReportCallbacks : public BLECharacteristicCallbacks {
  void onSubscribe(BLECharacteristic *, ble_gap_conn_desc *, uint16_t value) override {
    subscribed = (value & 0x01) != 0;
    // Even an unsubscribe/resubscribe between loop iterations must rearm.
    ++connectionEpoch;
  }

  void onStatus(BLECharacteristic *, Status result, uint32_t code) override {
    reportAccepted = result == SUCCESS_NOTIFY;
    if (result != SUCCESS_NOTIFY) {
      Serial.printf("BLE: HID notification failed (%d, %lu)\n",
                    int(result), static_cast<unsigned long>(code));
    }
  }
};

class ControlCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *control) override {
    if (control->getLength() == 1 && control->getData()[0] <= 1) {
      const bool suspend = control->getData()[0] == 0;
      if (suspended.exchange(suspend) != suspend) ++connectionEpoch;
    }
  }
};

ServerCallbacks serverCallbacks;
SecurityCallbacks securityCallbacks;
ReportCallbacks reportCallbacks;
ControlCallbacks controlCallbacks;

bool sendReport(const KeyboardReport &report) {
  input->setValue(reinterpret_cast<const uint8_t *>(&report), sizeof(report));
  reportAccepted = false;
  input->notify();
  return reportAccepted.load();
}

}

bool begin() {
  if (connected || (initialized && BLEDevice::getAdvertising()->isAdvertising())) {
    Serial.println("BLE: previous session is still closing; restart if it persists");
    return false;
  }
  if (!initialized) {
    if (!BLEDevice::init("Morse Paddle")) {
      failed = true;
      Serial.println("BLE: initialization failed");
      return false;
    }
    static BLESecurity security;
    security.setCapability(ESP_IO_CAP_NONE);
    security.setAuthenticationMode(true, false, true);
    BLEDevice::setSecurityCallbacks(&securityCallbacks);
    server = BLEDevice::createServer();
    server->setCallbacks(&serverCallbacks);
    server->advertiseOnDisconnect(false);
    auto *hid = new BLEHIDDevice(server);
    hid->manufacturer()->setValue("Morse Paddle");
    // Unassigned vendor/product IDs; do not impersonate a commercial keyboard.
    hid->pnp(0x02, 0x0000, 0x0000, 0x0100);
    hid->hidInfo(0x00, 0x02);
    hid->reportMap(reportMap, sizeof(reportMap));
    input = hid->inputReport(1);
    input->setCallbacks(&reportCallbacks);
    clearReport();
    uint8_t leds = 0;
    hid->outputReport(1)->setValue(&leds, 1);
    hid->hidControl()->setCallbacks(&controlCallbacks);
    hid->setBatteryLevel(100);  // USB-powered, not a battery measurement.
    hid->startServices();
    auto *advertising = BLEDevice::getAdvertising();
    advertising->setAppearance(HID_KEYBOARD);
    advertising->addServiceUUID(hid->hidService()->getUUID());
    advertising->setScanResponse(true);
    advertising->setMinPreferred(12);
    advertising->setMaxPreferred(24);
    initialized = true;
  }
  failed = false;
  active = true;
  restartAdvertising = false;
  wasReady = false;
  paddles.reset(millis());
  clearReport();
  if (!startAdvertising()) {
    active = false;
    failed = true;
    Serial.println("BLE: advertising failed");
    return false;
  }
  Serial.println("BLE: pair with Morse Paddle in iPhone Bluetooth settings");
  return true;
}

void update(bool ditDown, bool dahDown) {
  if (!active) return;
  if (restartAdvertising.exchange(false) && !failed && !connected) {
    if (!startAdvertising()) {
      failed = true;
      Serial.println("BLE: could not restart advertising; restart the ESP32");
    }
  }
  const uint32_t now = millis();
  if (!connected && !BLEDevice::getAdvertising()->isAdvertising() && !failed
      && uint32_t(now - advertisingStartedAt) >= 2000) {
    failed = true;
    Serial.println("BLE: not advertising; restart the ESP32");
  }
  const uint32_t epoch = connectionEpoch.load();
  if (!ready()) {
    wasReady = false;
    paddles.reset(now);
    return;
  }
  if (!wasReady || epoch != observedEpoch) {
    paddles.reset(now);
    lastModifiers = 0xFF;
    lastAttempt = now - 10;
    reportFailures = 0;
    observedEpoch = epoch;
    wasReady = true;
  }
  // Do not arm until the initial neutral notification has actually succeeded.
  const KeyboardReport report = lastModifiers == 0xFF
      ? KeyboardReport{} : paddles.sample(ditDown, dahDown, now);
  if (report.modifiers != lastModifiers
      && (reportFailures == 0 || uint32_t(now - lastAttempt) >= 10)) {
    lastAttempt = now;
    if (sendReport(report)) {
      if (lastModifiers == 0xFF) paddles.reset(now);
      lastModifiers = report.modifiers;
      reportFailures = 0;
    } else if (++reportFailures >= 3) {
      failed = true;
      Serial.println("BLE: repeated HID failures; disconnecting to release keys");
      server->disconnect(server->getConnId());
    }
  }
}

Status status() {
  if (failed) return Status::Error;
  if (!connected) return Status::Waiting;
  if (!ready()) return Status::Connecting;
  return paddles.isArmed() ? Status::Ready : Status::ReleasePaddles;
}

bool end() {
  if (!initialized) return true;
  // Stop accepting connections before disconnecting, including a connection
  // arriving while the final neutral report is in flight.
  active = false;
  restartAdvertising = false;
  bool success = BLEDevice::getAdvertising()->stop();
  if (!success) Serial.println("BLE: failed to stop advertising");
  if (connected) {
    if (authenticated && subscribed) {
      const KeyboardReport neutral;
      bool released = false;
      for (int attempt = 0; attempt < 3 && connected && !released; ++attempt) {
        released = sendReport(neutral);
        delay(30);
      }
      if (!released) Serial.println("BLE: release failed; forcing disconnect");
    }
    server->disconnect(server->getConnId());
  }
  const uint32_t started = millis();
  while ((connected || BLEDevice::getAdvertising()->isAdvertising())
         && uint32_t(millis() - started) < 1000) delay(1);
  if (connected || BLEDevice::getAdvertising()->isAdvertising()) {
    Serial.println("BLE: shutdown timed out; restart the ESP32");
    success = false;
  }
  clearReport();
  wasReady = false;
  paddles.reset(millis());
  return success;
}

}
