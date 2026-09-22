#pragma once
#include "USB.h"
#include <cstring>

constexpr uint8_t HID_PROTOCOL_BOOT = 0;
constexpr uint8_t HID_ITF_PROTOCOL_KEYBOARD = 1;
inline bool tud_cdc_n_connected(uint8_t instance) {
  assert(instance == 0);
  return FakeUsb::cdcConnected;
}
inline uint32_t tud_cdc_n_write_available(uint8_t instance) {
  assert(instance == 0);
  return Serial.availableForWrite();
}
inline uint32_t tud_cdc_n_write(uint8_t instance, const void *data, uint32_t length) {
  assert(instance == 0);
  return Serial.write(static_cast<const uint8_t *>(data), length);
}
inline uint32_t tud_cdc_n_write_flush(uint8_t instance) {
  assert(instance == 0);
  return 0;
}
inline bool tud_mounted() { return FakeUsb::mounted; }
inline bool tud_suspended() { return FakeUsb::suspended; }
inline bool tud_hid_n_ready(uint8_t instance) {
  assert(instance == 0);
  return FakeUsb::mounted && !FakeUsb::suspended && !FakeUsb::detached && FakeUsb::endpointReady;
}
inline uint8_t tud_hid_n_get_protocol(uint8_t instance) {
  assert(instance == 0);
  return FakeUsb::bootProtocol ? HID_PROTOCOL_BOOT : 1;
}
inline bool tud_hid_n_report(uint8_t instance, uint8_t id, const void *data, uint16_t len) {
  assert(instance == 0 && len == 8);
  ++FakeUsb::attempts;
  if (!FakeUsb::queueSucceeds) return false;
  std::array<uint8_t, 8> report{};
  std::memcpy(report.data(), data, len);
  for (unsigned i = 1; i < report.size(); ++i) assert(report[i] == 0);
  FakeUsb::reports.push_back(report);
  FakeUsb::reportIds.push_back(id);
  return true;
}
inline bool tud_disconnect() {
  ++FakeUsb::disconnects;
  FakeUsb::detached = true;
  // TinyUSB may retain its mounted flag until a subsequent bus event.
  return true;
}
