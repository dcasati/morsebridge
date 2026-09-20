#include "ble_fakes/BLEDevice.h"
#include "../MorsePaddle/MorsePaddle.ino"

#include <cstdio>
#include <string>

void runLoops(unsigned count) {
  for (unsigned i = 0; i < count; ++i) loop();
}

int main(int argc, char **argv) {
  const std::string scenario = argc > 1 ? argv[1] : "normal";
  Fake::pinValues.fill(HIGH);
  if (scenario == "init-failure") Fake::initSucceeds = false;
  if (scenario == "advertising-failure") Fake::startSucceeds = false;
  if (scenario == "advertising-timeout") Fake::silentlyStopAdvertising = true;
  setup();
  assert(Fake::now == 0);  // No wait for USB/Serial, even without a host.
  assert(Serial.baud == 115200 && Serial.timeout == 0);
  assert(Fake::pinModes[4] == INPUT_PULLUP && Fake::pinModes[5] == INPUT_PULLUP);
  assert(Fake::pinModes[19] == 0 && Fake::pinModes[20] == 0);
  assert(Fake::pinModes[21] == 0);
  assert(Fake::deviceName == "Morse Paddle");

  if (scenario == "advertising-timeout") {
    runLoops(2001);
    assert(MorseBle::status() == MorseBle::Status::Error);
    assert(Serial.lines.back().find("ERROR") != std::string::npos);
  } else if (scenario != "normal") {
    assert(MorseBle::status() == MorseBle::Status::Error);
    assert(Serial.lines.back().find("ERROR") != std::string::npos);
    const auto logs = Serial.lines.size();
    runLoops(100);
    assert(Serial.lines.size() == logs);
    assert(Fake::pinReads == 0);
  } else {
    assert(Fake::advertisingStarts == 1 && Fake::advertising);
    const auto waitingLogs = Serial.lines.size();
    runLoops(100);
    assert(Serial.lines.size() == waitingLogs);  // No idle log spam.
    assert(Fake::pinReads == 200);

    Fake::pinValues[4] = LOW;
    Fake::pinValues[5] = LOW;
    Fake::server.connect();
    loop();
    assert(MorseBle::status() == MorseBle::Status::Connecting);
    Fake::authenticate();
    Fake::input.subscribe(1);
    runLoops(10);
    assert(MorseBle::status() == MorseBle::Status::ReleasePaddles);
    assert(Fake::input.reports.back()[0] == 0);
    Fake::pinValues[4] = HIGH;
    Fake::pinValues[5] = HIGH;
    runLoops(6);
    assert(MorseBle::status() == MorseBle::Status::Ready);
    const auto readyLogs = Serial.lines.size();

    Fake::pinValues[4] = LOW;
    runLoops(5);
    assert(Fake::input.reports.back()[0] == 0);
    loop();
    assert(Fake::input.reports.back()[0] == 0x01);
    const auto heldReports = Fake::input.reports.size();
    runLoops(100);
    assert(Fake::input.reports.size() == heldReports);
    Fake::pinValues[5] = LOW;
    runLoops(6);
    assert(Fake::input.reports.back()[0] == 0x11);
    Fake::pinValues[4] = HIGH;
    runLoops(6);
    assert(Fake::input.reports.back()[0] == 0x10);
    Fake::pinValues[5] = HIGH;
    runLoops(6);
    assert(Fake::input.reports.back()[0] == 0);
    assert(Serial.lines.size() == readyLogs);

    Fake::server.disconnect(1);
    loop();
    assert(Fake::advertisingStarts == 2);
    assert(MorseBle::status() == MorseBle::Status::Waiting);
    assert(Fake::hidCreations == 1);
  }
  std::printf("Headless tests passed (%s)\n", scenario.c_str());
}
