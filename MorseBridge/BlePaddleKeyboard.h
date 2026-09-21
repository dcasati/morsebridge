#pragma once

#include <stdint.h>

namespace MorseBle {

enum class Status { Waiting, Connecting, ReleasePaddles, Ready, Error };

struct Diagnostics {
  bool connected;
  bool authenticated;
  bool subscribed;
  bool suspended;
  bool armed;
  uint32_t queuedReports;
  uint8_t lastQueuedModifiers;
};

bool begin();
void update(bool ditDown, bool dahDown);
bool end();
Status status();
Diagnostics diagnostics();

}
