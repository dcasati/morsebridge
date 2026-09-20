#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#define CONFIG_IDF_TARGET_ESP32S3 1
#define CONFIG_NIMBLE_ENABLED 1
#define ARDUINO_USB_CDC_ON_BOOT 1
#define ARDUINO_USB_MODE 1

constexpr int INPUT_PULLUP = 2;
constexpr int LOW = 0;
constexpr int HIGH = 1;

namespace Fake {
inline uint32_t now = 0;
inline void (*onDelay)() = nullptr;
inline std::array<int, 49> pinModes{};
inline std::array<int, 49> pinValues{};
inline unsigned pinReads = 0;
}

inline void pinMode(uint8_t pin, int mode) { Fake::pinModes.at(pin) = mode; }
inline int digitalRead(uint8_t pin) {
  ++Fake::pinReads;
  return Fake::pinValues.at(pin);
}
inline uint32_t millis() { return Fake::now; }
inline void delay(uint32_t ms) {
  Fake::now += ms;
  if (Fake::onDelay) Fake::onDelay();
}
struct FakeSerial {
  unsigned baud = 0;
  uint32_t timeout = 1000;
  std::vector<std::string> lines;
  void begin(unsigned rate) { baud = rate; }
  void setTxTimeoutMs(uint32_t ms) { timeout = ms; }
  void println(const char *text) { lines.emplace_back(text); }
  template <typename... Args> void printf(const char *, Args...) {}
};
inline FakeSerial Serial;
