#include "../MorseBridge/PaddleLog.h"

#include <cassert>
#include <cstdio>
#include <string>

int main() {
  Serial.txSpace = 0;
  PaddleLog::println("no USB reader");
  PaddleLog::printf("BLE error %u\n", 42u);
  assert(Serial.lines.empty());
  assert(PaddleLog::dropped == 2);
  assert(Fake::now == 0);

  Serial.txSpace = 3;
  PaddleLog::println("abc");  // Complete line retained across FIFO-sized chunks.
  assert(Serial.lines.empty());
  assert(PaddleLog::dropped == 2 && DebugSerial.availableForWrite() == 0);
  Serial.txSpace = 256;
  PaddleLog::poll();
  assert(Serial.lines.front() == "abc\n");
  assert(Serial.lines.back().find("dropped 2") != std::string::npos);
  assert(PaddleLog::dropped == 0);
  const auto count = Serial.lines.size();
  PaddleLog::poll();
  assert(Serial.lines.size() == count);

  // A BLE callback racing another producer skips output rather than waiting.
  Serial.onWrite = [] { PaddleLog::println("concurrent callback"); };
  PaddleLog::println("outer");
  assert(Serial.lines.back() == "outer\n");
  assert(PaddleLog::dropped == 1);
  Serial.onWrite = nullptr;
  PaddleLog::poll();
  assert(PaddleLog::dropped == 0);

  const std::string oversized(160, 'x');
  PaddleLog::println(oversized.c_str());
  assert(PaddleLog::dropped == 1);
  Serial.txSpace = 0;
  PaddleLog::poll();
  assert(PaddleLog::dropped == 1);  // Keep loss accounting while USB is full.
  Serial.txSpace = 256;
  PaddleLog::poll();
  assert(PaddleLog::dropped == 0);

  // Real core uses a 64-byte FIFO: long lines must not drop or interleave.
  Serial.txSpace = 64;
  const std::string longLine(150, 'y');
  const auto beforeLong = Serial.lines.size();
  PaddleLog::println(longLine.c_str());
  assert(Serial.lines.size() == beforeLong);
  PaddleLog::println("cannot interleave");
  assert(PaddleLog::dropped == 1);
  PaddleLog::poll();
  assert(Serial.lines.size() == beforeLong);
  PaddleLog::poll();
  assert(Serial.lines[beforeLong] == longLine + "\n");
  assert(Serial.lines.back().find("dropped 1") != std::string::npos);
  assert(PaddleLog::dropped == 0);

  // Unplug discards an incomplete diagnostic line and accounts for its loss.
  PaddleLog::println(longLine.c_str());
  FakeUsb::cdcConnected = false;
  Serial.pendingLine.clear();  // The old host's stream ends on disconnect.
  PaddleLog::poll();
  assert(PaddleLog::dropped == 1);
  FakeUsb::cdcConnected = true;
  PaddleLog::poll();
  assert(PaddleLog::dropped == 0);

  Serial.consumeSpace = true;
  for (unsigned i = 0; i < 1000; ++i) PaddleLog::println("fill");
  assert(Serial.txSpace == 0);
  assert(PaddleLog::dropped == 987);  // 12 complete lines plus one retained tail.
  Serial.consumeSpace = false;
  Serial.txSpace = 64;
  PaddleLog::poll();
  assert(PaddleLog::dropped == 0 && Serial.pendingLine.empty());
  assert(Fake::now == 0);
  std::puts("Bounded USB logging tests passed");
}
