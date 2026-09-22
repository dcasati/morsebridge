#pragma once

#include "BlePaddleKeyboard.h"

namespace MorseUsb {

struct Diagnostics {
  bool configured = false;
  bool suspended = false;
  bool armed = false;
  uint32_t queuedReports = 0;
  uint8_t lastQueuedModifiers = 0;
};

bool begin();
bool configured();
void update(bool dit, bool dah, bool selected);
MorseBle::Status status();
Diagnostics diagnostics();

}
