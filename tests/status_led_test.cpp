#include "../MorseBridge/StatusLed.h"

#include <cassert>
#include <cstdio>

uint32_t color() {
  const auto &frame = FakeRmt::frames.back();
  uint32_t value = 0;
  for (unsigned bit = 0; bit < 24; ++bit) {
    assert(frame[bit].level0 == 1 && frame[bit].level1 == 0);
    assert(frame[bit].duration0 + frame[bit].duration1 == 12);
    value = (value << 1) | (frame[bit].duration0 == 8 ? 1 : 0);
  }
  assert(frame[24].level0 == 0 && frame[24].level1 == 0);
  assert(frame[24].duration0 + frame[24].duration1 == 3000);
  return value;
}

int main() {
  using MorseBle::Status;
  StatusLed led;
  led.begin();
  assert(FakeRmt::pin == 21);
  led.update(Status::Waiting, 0);
  assert(color() == 0x000008);
  for (uint32_t now = 1; now < 100; ++now) led.update(Status::Waiting, now);
  assert(FakeRmt::frames.size() == 1);
  led.update(Status::Connecting, 100);
  assert(color() == 0x080400);
  led.update(Status::ReleasePaddles, 101);
  assert(FakeRmt::frames.size() == 2);
  led.update(Status::UsbReady, 101);
  assert(color() == 0x000808);
  led.update(Status::Ready, 102);
  assert(color() == 0x000800);  // S3-Zero green is the second byte, not the first.
  led.update(Status::Error, 103);
  assert(color() == 0x080000);

  // Do not alter an in-flight buffer even when the desired status changes.
  FakeRmt::completed = false;
  const auto count = FakeRmt::frames.size();
  led.update(Status::Ready, 104);
  assert(FakeRmt::frames.size() == count);
  for (unsigned i = 0; i < 25; ++i) {
    assert(FakeRmt::inFlight[i].duration0 == FakeRmt::frames.back()[i].duration0);
    assert(FakeRmt::inFlight[i].duration1 == FakeRmt::frames.back()[i].duration1);
  }
  led.update(Status::Ready, 122);
  assert(FakeRmt::frames.size() == count);
  led.update(Status::Ready, 123);
  assert(Serial.lines.back().find("timed out") != std::string::npos);
  FakeRmt::completed = true;
  led.update(Status::Ready, 124);
  assert(FakeRmt::frames.size() == count);  // A fault is latched, not spammed.
  assert(Fake::now == 0);                  // No blocking delay.

  FakeRmt::initSucceeds = false;
  StatusLed badInit;
  badInit.begin();
  badInit.update(Status::Ready, 0);
  assert(Serial.lines.back().find("initialization failed") != std::string::npos);
  assert(FakeRmt::frames.size() == count);
  FakeRmt::initSucceeds = true;

  FakeRmt::writeSucceeds = false;
  StatusLed badWrite;
  badWrite.begin();
  badWrite.update(Status::Ready, 0);
  assert(Serial.lines.back().find("transmission failed") != std::string::npos);
  FakeRmt::writeSucceeds = true;
  badWrite.update(Status::Ready, 100);
  assert(FakeRmt::frames.size() == count);

  StatusLed rollover;
  rollover.begin();
  rollover.update(Status::Waiting, UINT32_MAX - 9);
  FakeRmt::completed = false;
  rollover.update(Status::Ready, 9);
  assert(Serial.lines.back().find("enabled") != std::string::npos);
  rollover.update(Status::Ready, 10);
  assert(Serial.lines.back().find("timed out") != std::string::npos);
  std::puts("Status LED tests passed");
}
