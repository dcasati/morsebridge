#pragma once
#include "Arduino.h"

using esp_event_base_t = const char *;
enum {
  ARDUINO_USB_STARTED_EVENT, ARDUINO_USB_STOPPED_EVENT,
  ARDUINO_USB_SUSPEND_EVENT, ARDUINO_USB_RESUME_EVENT
};

namespace FakeUsb {
inline bool mounted = false;
inline bool suspended = false;
inline bool endpointReady = true;
inline bool queueSucceeds = true;
inline bool beginSucceeds = true;
inline bool descriptorSucceeds = true;
inline bool started = false;
inline bool detached = false;
inline bool bootProtocol = false;
inline bool cdcConnected = true;
inline unsigned disconnects = 0;
inline unsigned attempts = 0;
inline std::string product;
inline std::string manufacturer;
inline std::vector<uint8_t> descriptor;
inline std::vector<std::array<uint8_t, 8>> reports;
inline std::vector<uint8_t> reportIds;
inline void (*handler)(void *, esp_event_base_t, int32_t, void *) = nullptr;
inline void event(int32_t id) {
  if (id == ARDUINO_USB_STARTED_EVENT) { mounted = true; detached = false; }
  if (id == ARDUINO_USB_STOPPED_EVENT) { mounted = false; suspended = false; }
  if (id == ARDUINO_USB_SUSPEND_EVENT) suspended = true;
  if (id == ARDUINO_USB_RESUME_EVENT) suspended = false;
  if (handler) handler(nullptr, "USB", id, nullptr);
}
}

class ESPUSB {
public:
  bool productName(const char *name) { FakeUsb::product = name; return !FakeUsb::started; }
  bool manufacturerName(const char *name) { FakeUsb::manufacturer = name; return !FakeUsb::started; }
  bool usbPower(uint16_t ma) { assert(ma == 250); return !FakeUsb::started; }
  void onEvent(void (*fn)(void *, esp_event_base_t, int32_t, void *)) { FakeUsb::handler = fn; }
  bool begin() {
    assert(!FakeUsb::descriptor.empty());
    FakeUsb::started = FakeUsb::beginSucceeds;
    return FakeUsb::started;
  }
};
inline ESPUSB USB;
