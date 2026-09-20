#pragma once
#include "BLEDevice.h"

class BLEService {
public:
  int getUUID() { return 0x1812; }
};
class BLEHIDDevice {
public:
  explicit BLEHIDDevice(BLEServer *) { ++Fake::hidCreations; }
  BLECharacteristic *manufacturer() { return &Fake::manufacturer; }
  void pnp(uint8_t, uint16_t, uint16_t, uint16_t) {}
  void hidInfo(uint8_t, uint8_t) {}
  void reportMap(uint8_t *data, uint16_t size) {
    assert(size == 65 && data[6] == 0x85 && data[7] == 1);
  }
  BLECharacteristic *inputReport(uint8_t id) { assert(id == 1); return &Fake::input; }
  BLECharacteristic *outputReport(uint8_t id) { assert(id == 1); return &Fake::output; }
  BLECharacteristic *hidControl() { return &Fake::control; }
  void setBatteryLevel(uint8_t) {}
  void startServices() {}
  BLEService *hidService() { static BLEService service; return &service; }
};
