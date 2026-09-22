#include "BlePaddleKeyboard.h"
#include "PaddleInput.h"
#include "PaddleLog.h"
#include "ReportSession.h"
#include "KeyboardDescriptor.h"

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <atomic>

#if !defined(CONFIG_IDF_TARGET_ESP32S3) || !defined(CONFIG_NIMBLE_ENABLED)
#error "MorseBridge requires ESP32-S3 with the Arduino 3.3.8 NimBLE stack"
#endif

namespace MorseBle {
namespace {

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
MorseBridge::ReportSession session;
bool initialized = false;
bool advertisingMissing = false;
uint32_t advertisingMissingSince = 0;
uint32_t queuedReports = 0;
uint8_t lastQueuedModifiers = 0;

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
    // Advertising can stop before the connection callback becomes visible.
    // The loop checks for a sustained outage instead of latching an error here.
    PaddleLog::println("BLE: advertising ended; checking for connection");
  }
}

bool startAdvertising() {
  advertisingMissing = false;
  // NimBLE returns the start result synchronously, unlike Bluedroid GAP events.
  if (BLEDevice::getAdvertising()->start(0, advertisingComplete)) return true;
  failed = true;
  PaddleLog::println("BLE: advertising request failed");
  return false;
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *device, ble_gap_conn_desc *desc) override {
    // These are cleared before advertising and on disconnect. Do not erase
    // security/subscription events that arrived before this callback.
    clearReport();
    connected = true;
    ++connectionEpoch;
    if (!active) {
      device->disconnect(desc->conn_handle);
      return;
    }
    PaddleLog::println("BLE: connected; waiting for encrypted HID subscription");
    if (!device->requestConnParams(desc->conn_handle, 12, 24, 0, 400)) {
      PaddleLog::println("BLE: connection interval request failed; using host interval");
    }
  }

  void onDisconnect(BLEServer *) override {
    authenticated = false;
    subscribed = false;
    suspended = false;
    connected = false;
    ++connectionEpoch;
    restartAdvertising = active.load();
    PaddleLog::println("BLE: disconnected");
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
      PaddleLog::printf("BLE: pairing failed (encrypted=%u, bonded=%u); forget device and restart\n",
                    unsigned(desc->sec_state.encrypted), unsigned(desc->sec_state.bonded));
      server->disconnect(desc->conn_handle);
    } else {
      PaddleLog::println("BLE: encrypted and bonded");
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
      PaddleLog::printf("BLE: HID notification failed (%d, %lu)\n",
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
  const bool accepted = reportAccepted.load();
  if (accepted) {
    ++queuedReports;
    lastQueuedModifiers = report.modifiers;
  }
  return accepted;
}

}

bool begin() {
  if (connected || (initialized && BLEDevice::getAdvertising()->isAdvertising())) {
    PaddleLog::println("BLE: previous session is still closing; restart if it persists");
    return false;
  }
  if (!initialized) {
    if (!BLEDevice::init("MorseBridge")) {
      failed = true;
      PaddleLog::println("BLE: initialization failed");
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
    hid->manufacturer()->setValue("MorseBridge");
    // Unassigned vendor/product IDs; do not impersonate a commercial keyboard.
    hid->pnp(0x02, 0x0000, 0x0000, 0x0100);
    hid->hidInfo(0x00, 0x02);
    hid->reportMap(MorseBridge::keyboardDescriptor, sizeof(MorseBridge::keyboardDescriptor));
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
  authenticated = false;
  subscribed = false;
  suspended = false;
  active = true;
  restartAdvertising = false;
  session.reset(millis());
  clearReport();
  if (!startAdvertising()) {
    active = false;
    failed = true;
    PaddleLog::println("BLE: advertising failed");
    return false;
  }
  PaddleLog::println("BLE: pair with MorseBridge in iPhone Bluetooth settings");
  return true;
}

void update(bool ditDown, bool dahDown, bool selected) {
  if (!active) return;
  if (restartAdvertising.exchange(false) && !failed && !connected) {
    if (!startAdvertising()) {
      failed = true;
      PaddleLog::println("BLE: could not restart advertising; restart the ESP32");
    }
  }
  const uint32_t now = millis();
  if (!connected && !BLEDevice::getAdvertising()->isAdvertising() && !failed) {
    if (!advertisingMissing) {
      advertisingMissing = true;
      advertisingMissingSince = now;
    } else if (uint32_t(now - advertisingMissingSince) >= 2000 && !connected) {
      failed = true;
      PaddleLog::println("BLE: not advertising for 2 seconds; restart the ESP32");
    }
  } else {
    advertisingMissing = false;
  }
  session.update(ditDown, dahDown, now, selected, connected, ready(), connectionEpoch,
    [](const KeyboardReport &report) {
      return sendReport(report) ? MorseBridge::SendResult::Accepted
                                : MorseBridge::SendResult::Failed;
    },
    [] {
      if (failed) return;
      failed = true;
      PaddleLog::println("BLE: HID release/send failed; disconnecting to release keys");
      server->disconnect(server->getConnId());
    });
}

bool released() {
  return session.released(connected);
}

Status status() {
  if (failed) return Status::Error;
  if (!connected) return Status::Waiting;
  if (!ready()) return Status::Connecting;
  return session.armed() ? Status::Ready : Status::ReleasePaddles;
}

Diagnostics diagnostics() {
  return {connected.load(), authenticated.load(), subscribed.load(),
          suspended.load(), ready() && session.armed(),
          queuedReports, lastQueuedModifiers};
}

bool end() {
  if (!initialized) return true;
  // Stop accepting connections before disconnecting, including a connection
  // arriving while the final neutral report is in flight.
  active = false;
  restartAdvertising = false;
  bool success = BLEDevice::getAdvertising()->stop();
  if (!success) PaddleLog::println("BLE: failed to stop advertising");
  if (connected) {
    if (authenticated && subscribed) {
      const KeyboardReport neutral;
      bool released = false;
      for (int attempt = 0; attempt < 3 && connected && !released; ++attempt) {
        released = sendReport(neutral);
        delay(30);
      }
      if (!released) PaddleLog::println("BLE: release failed; forcing disconnect");
    }
    server->disconnect(server->getConnId());
  }
  const uint32_t started = millis();
  while ((connected || BLEDevice::getAdvertising()->isAdvertising())
         && uint32_t(millis() - started) < 1000) delay(1);
  if (connected || BLEDevice::getAdvertising()->isAdvertising()) {
    PaddleLog::println("BLE: shutdown timed out; restart the ESP32");
    success = false;
  }
  clearReport();
  session.reset(millis());
  return success;
}

}
