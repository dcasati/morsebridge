#pragma once

#include <USBCDC.h>
#include <tusb.h>
#include <atomic>
#include <cstring>

class DiagnosticSerial {
public:
  void begin(unsigned baud) {
    cdc.begin(baud);
    cdc.setTxTimeoutMs(0);
  }

  int availableForWrite() const {
    return remaining == 0 && tud_cdc_n_connected(0) && tud_cdc_n_write_available(0)
        ? sizeof(pending) : 0;
  }

  size_t write(const uint8_t *data, size_t length) {
    // USBCDC::write in 3.3.8 expires before writing with timeout 0.
    // Reserve a whole line: the pinned core's hardware TX FIFO is only 64 bytes.
    if (length > static_cast<size_t>(availableForWrite())) return 0;
    std::memcpy(pending, data, length);
    offset = 0;
    remaining = length;
    return poll() ? 0 : length;
  }

  // PaddleLog's writer guard serializes write/poll; return discarded line count.
  unsigned poll() {
    const size_t count = remaining.load();
    if (!count) return 0;
    if (!tud_cdc_n_connected(0)) {
      remaining = 0;
      return 1;
    }
    const size_t space = tud_cdc_n_write_available(0);
    const size_t chunk = count < space ? count : space;
    if (chunk) {
      const size_t written = tud_cdc_n_write(0, pending + offset, chunk);
      offset += written;
      remaining = count - written;
      tud_cdc_n_write_flush(0);
    }
    return 0;
  }

private:
  // Register diagnostic CDC before USB.begin(), alongside HID.
  USBCDC cdc;
  uint8_t pending[192] = {};
  size_t offset = 0;
  std::atomic<size_t> remaining{0};
};

inline DiagnosticSerial DebugSerial;
