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
  PaddleLog::println("abc");  // Must reserve the newline too.
  assert(Serial.lines.empty());
  assert(PaddleLog::dropped == 3);
  Serial.txSpace = 4;
  PaddleLog::println("abc");
  assert(Serial.lines.back() == "abc\n");

  Serial.txSpace = 256;
  PaddleLog::poll();
  assert(Serial.lines.back().find("dropped 3") != std::string::npos);
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

  Serial.consumeSpace = true;
  for (unsigned i = 0; i < 1000; ++i) PaddleLog::println("fill");
  assert(Serial.txSpace == 1);
  assert(PaddleLog::dropped == 949);
  assert(Fake::now == 0);
  std::puts("Bounded USB logging tests passed");
}
