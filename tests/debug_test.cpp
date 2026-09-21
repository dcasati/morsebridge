#include "../MorseBridge/PaddleDebug.h"

#include <cassert>
#include <cstdio>
#include <string>

bool contains(const char *text) {
  return !Serial.lines.empty() && Serial.lines.back().find(text) != std::string::npos;
}

int main() {
  MorseBle::Diagnostics ble{};
  PaddleDebug debug;
  Serial.txSpace = 256;
  debug.sample(false, false, 0, ble);
  assert(contains("GPIO4=HIGH GPIO5=HIGH edges=0/0"));
  assert(contains("C=0 A=0 S=0 U=0 R=0 Q=0 M=00"));
  assert(Serial.lines.back().size() < 128);
  debug.sample(true, false, 10, ble);
  debug.sample(false, false, 20, ble);
  debug.sample(false, true, 30, ble);
  debug.sample(false, false, 40, ble);
  assert(Serial.lines.size() == 1);
  debug.sample(false, false, 100, ble);
  assert(contains("GPIO4=HIGH GPIO5=HIGH edges=2/2"));  // Short taps aren't lost.
  debug.sample(false, false, 2099, ble);
  assert(Serial.lines.size() == 2);
  debug.sample(false, false, 2100, ble);
  assert(Serial.lines.size() == 3);  // Two-second idle heartbeat.

  Serial.txSpace = 0;
  debug.sample(true, true, 2200, ble);
  debug.sample(false, false, 2300, ble);
  assert(Serial.lines.size() == 3);  // No host/backpressure never waits.
  assert(Fake::now == 0);
  Serial.txSpace = 16;
  debug.sample(false, false, 2400, ble);
  assert(Serial.lines.size() == 3);  // Don't knowingly emit partial lines.
  Serial.txSpace = 256;
  debug.sample(false, false, 2500, ble);
  assert(contains("edges=4/4"));

  ble = {true, true, true, false, true, 12, 0x11};
  debug.sample(true, true, 2600, ble);
  assert(contains("GPIO4=LOW GPIO5=LOW edges=5/5"));
  assert(contains("C=1 A=1 S=1 U=0 R=1 Q=12 M=11"));
  const auto heldLogs = Serial.lines.size();
  for (uint32_t now = 2601; now < 4600; ++now) {
    debug.sample(true, true, now, ble);
  }
  assert(Serial.lines.size() == heldLogs);
  debug.sample(true, true, 4600, ble);
  assert(Serial.lines.size() == heldLogs + 1);

  // Fixed-size log line also fits when the lifetime report counter is maximal.
  ble.queuedReports = UINT32_MAX;
  debug.sample(false, false, 4700, ble);
  assert(contains("Q=4294967295"));
  assert(Serial.lines.back().size() < 128);

  PaddleDebug rollover;
  rollover.sample(false, false, UINT32_MAX - 49, ble);
  const auto beforeWrap = Serial.lines.size();
  rollover.sample(true, false, 0, ble);
  assert(Serial.lines.size() == beforeWrap);
  rollover.sample(true, false, 50, ble);
  assert(Serial.lines.size() == beforeWrap + 1);
  assert(contains("GPIO4=LOW GPIO5=HIGH edges=1/0"));
  std::puts("Paddle diagnostic tests passed");
}
