#pragma once
#include "Arduino.h"

class USBCDC {
public:
  void begin(unsigned baud) { Serial.begin(baud); }
  void setTxTimeoutMs(uint32_t timeout) { Serial.setTxTimeoutMs(timeout); }
  int availableForWrite() { return Serial.availableForWrite(); }
  size_t write(const uint8_t *data, size_t length) {
    // Match pinned core behavior: zero timeout expires before its first write.
    return Serial.timeout ? Serial.write(data, length) : 0;
  }
};
