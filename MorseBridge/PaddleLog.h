#pragma once

#include <Arduino.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>

namespace PaddleLog {

inline std::atomic_flag writing = ATOMIC_FLAG_INIT;
inline std::atomic<uint32_t> dropped{0};

inline bool tryWrite(const char *text, size_t length) {
  if (writing.test_and_set()) return false;
  // HWCDC 3.3.8 can underflow its zero-timeout retry counter on a full buffer.
  // Serialize all producers and reserve enough room by checking before writing.
  const int space = Serial.availableForWrite();
  const bool sent = space >= 0 && static_cast<size_t>(space) >= length
      && Serial.write(reinterpret_cast<const uint8_t *>(text), length) == length;
  writing.clear();
  return sent;
}

inline void printf(const char *format, ...) {
  char line[160];
  va_list args;
  va_start(args, format);
  const int length = std::vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  if (length < 0 || static_cast<size_t>(length) >= sizeof(line)
      || !tryWrite(line, static_cast<size_t>(length))) {
    ++dropped;
  }
}

inline void println(const char *text) {
  printf("%s\n", text);
}

inline void poll() {
  const uint32_t count = dropped.load();
  if (!count) return;
  char line[80];
  const int length = std::snprintf(line, sizeof(line),
      "LOG: dropped %lu diagnostic lines (USB busy or line too long)\n",
      static_cast<unsigned long>(count));
  if (tryWrite(line, static_cast<size_t>(length))) dropped.fetch_sub(count);
}

}
