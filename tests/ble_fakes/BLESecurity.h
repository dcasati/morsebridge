#pragma once
#include "BLEDevice.h"

class BLESecurity {
public:
  void setCapability(uint8_t capability) { assert(capability == ESP_IO_CAP_NONE); }
  void setAuthenticationMode(bool bond, bool mitm, bool secure) {
    assert(bond && !mitm && secure);
  }
};
