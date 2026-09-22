#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#define CONFIG_IDF_TARGET_ESP32S3 1
#define CONFIG_NIMBLE_ENABLED 1
#define ARDUINO_USB_CDC_ON_BOOT 0
#define ARDUINO_USB_MODE 0

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
  int txSpace = 256;
  bool consumeSpace = false;
  void (*onWrite)() = nullptr;
  std::string pendingLine;
  std::vector<std::string> lines;
  void begin(unsigned rate) { baud = rate; }
  void setTxTimeoutMs(uint32_t ms) { timeout = ms; }
  int availableForWrite() { return txSpace; }
  size_t write(const uint8_t *data, size_t size) {
    // Never exceed available FIFO capacity; USB packets may split a log line.
    assert(txSpace >= 0 && size <= static_cast<size_t>(txSpace));
    if (onWrite) onWrite();
    const size_t written = size < static_cast<size_t>(txSpace)
        ? size : static_cast<size_t>(txSpace);
    pendingLine.append(reinterpret_cast<const char *>(data), written);
    size_t newline;
    while ((newline = pendingLine.find('\n')) != std::string::npos) {
      lines.push_back(pendingLine.substr(0, newline + 1));
      pendingLine.erase(0, newline + 1);
    }
    if (consumeSpace) txSpace -= static_cast<int>(written);
    return written;
  }
};
inline FakeSerial Serial;
