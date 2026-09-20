#pragma once

#include "Arduino.h"
#include <cassert>
#include <vector>

constexpr uint8_t ESP_IO_CAP_NONE = 3;
constexpr int HID_KEYBOARD = 0x03C1;
struct ble_gap_conn_desc {
  uint16_t conn_handle = 1;
  struct { bool encrypted = false; bool bonded = false; } sec_state;
};

namespace Fake {
inline bool advertising = false;
}

class BLECharacteristic;
class BLECharacteristicCallbacks {
public:
  enum Status { SUCCESS_NOTIFY, ERROR_GATT };
  virtual ~BLECharacteristicCallbacks() = default;
  virtual void onStatus(BLECharacteristic *, Status, uint32_t) {}
  virtual void onWrite(BLECharacteristic *) {}
  virtual void onSubscribe(BLECharacteristic *, ble_gap_conn_desc *, uint16_t) {}
};
class BLECharacteristic {
public:
  BLECharacteristicCallbacks *callbacks = nullptr;
  std::vector<uint8_t> value;
  std::vector<std::vector<uint8_t>> reports;
  bool notifySucceeds = true;
  unsigned attempts = 0;

  void setValue(const uint8_t *data, size_t size) { value.assign(data, data + size); }
  void setValue(const char *) {}
  void setCallbacks(BLECharacteristicCallbacks *cb) { callbacks = cb; }
  void subscribe(uint16_t value) {
    ble_gap_conn_desc desc;
    callbacks->onSubscribe(this, &desc, value);
  }
  size_t getLength() { return value.size(); }
  uint8_t *getData() { return value.data(); }
  void notify() {
    ++attempts;
    if (notifySucceeds) reports.push_back(value);
    callbacks->onStatus(this, notifySucceeds ? BLECharacteristicCallbacks::SUCCESS_NOTIFY
                                          : BLECharacteristicCallbacks::ERROR_GATT, 0);
  }
};
class BLEServer;
class BLEServerCallbacks {
public:
  virtual ~BLEServerCallbacks() = default;
  virtual void onConnect(BLEServer *, ble_gap_conn_desc *) {}
  virtual void onDisconnect(BLEServer *) {}
};
class BLEServer {
public:
  BLEServerCallbacks *callbacks = nullptr;
  void setCallbacks(BLEServerCallbacks *cb) { callbacks = cb; }
  void advertiseOnDisconnect(bool enabled) { assert(!enabled); }
  bool requestConnParams(uint16_t, int min, int max, int latency, int) {
    assert(min == 12 && max == 24 && latency == 0);
    return true;
  }
  uint16_t getConnId() { return 1; }
  void disconnect(uint16_t) { callbacks->onDisconnect(this); }
  void connect() {
    Fake::advertising = false;
    ble_gap_conn_desc param;
    callbacks->onConnect(this, &param);
  }
};
class BLESecurityCallbacks {
public:
  virtual ~BLESecurityCallbacks() = default;
  virtual bool onSecurityRequest() = 0;
  virtual uint32_t onPassKeyRequest() = 0;
  virtual void onPassKeyNotify(uint32_t) = 0;
  virtual bool onConfirmPIN(uint32_t) = 0;
  virtual void onAuthenticationComplete(ble_gap_conn_desc *) = 0;
};

namespace Fake {
inline BLESecurityCallbacks *security = nullptr;
inline BLEServer server;
inline BLECharacteristic input, output, control, manufacturer;
inline bool initSucceeds = true;
inline bool startSucceeds = true;
inline bool silentlyStopAdvertising = false;
inline unsigned hidCreations = 0;
inline unsigned advertisingStarts = 0;
inline std::string deviceName;
inline void authenticate(bool encrypted = true, bool bonded = true) {
  ble_gap_conn_desc desc;
  desc.sec_state.encrypted = encrypted;
  desc.sec_state.bonded = bonded;
  security->onAuthenticationComplete(&desc);
}
}

class BLEAdvertising {
public:
  void (*onComplete)(BLEAdvertising *) = nullptr;
  void setAppearance(int appearance) { assert(appearance == HID_KEYBOARD); }
  void addServiceUUID(int) {}
  void setScanResponse(bool) {}
  void setMinPreferred(int) {}
  void setMaxPreferred(int) {}
  bool isAdvertising() { return Fake::advertising; }
  bool start(uint32_t duration, void (*complete)(BLEAdvertising *)) {
    assert(duration == 0);
    onComplete = complete;
    ++Fake::advertisingStarts;
    if (!Fake::startSucceeds) return false;
    Fake::advertising = !Fake::silentlyStopAdvertising;
    return true;
  }
  bool stop() {
    Fake::advertising = false;
    return true;
  }
};
class BLEDevice {
public:
  static bool init(const char *name) {
    Fake::deviceName = name;
    return Fake::initSucceeds;
  }
  static void setSecurityCallbacks(BLESecurityCallbacks *cb) { Fake::security = cb; }
  static BLEServer *createServer() { return &Fake::server; }
  static BLEAdvertising *getAdvertising() {
    static BLEAdvertising advertising;
    return &advertising;
  }
};
