#pragma once
#include "tusb.h"

class USBHIDDevice {
public:
  virtual ~USBHIDDevice() = default;
  virtual uint16_t _onGetDescriptor(uint8_t *) = 0;
};
class USBHID {
public:
  explicit USBHID(uint8_t protocol) { assert(protocol == HID_ITF_PROTOCOL_KEYBOARD); }
  static bool addDevice(USBHIDDevice *device, uint16_t size) {
    assert(!FakeUsb::started);
    if (!FakeUsb::descriptorSucceeds) return false;
    FakeUsb::descriptor.resize(size);
    assert(device->_onGetDescriptor(FakeUsb::descriptor.data()) == size);
    return true;
  }
  void begin() {}
};
