#include "ble_fakes/BLEDevice.h"
#include "../MorseBridge/MorseBridge.ino"

#include <cstdio>
#include <string>

void runLoops(unsigned count) {
  for (unsigned i = 0; i < count; ++i) loop();
}

size_t transitionLogs() {
  size_t count = 0;
  for (const auto &line : Serial.lines) {
    if (line.find("DBG ") != 0 && line.find("LOG: ") != 0) ++count;
  }
  return count;
}

bool hasLog(const char *text) {
  for (const auto &line : Serial.lines) {
    if (line.find(text) != std::string::npos) return true;
  }
  return false;
}

int main(int argc, char **argv) {
  const std::string scenario = argc > 1 ? argv[1] : "normal";
  Fake::pinValues.fill(HIGH);
  if (scenario == "init-failure") Fake::initSucceeds = false;
  if (scenario == "advertising-failure") Fake::startSucceeds = false;
  if (scenario == "advertising-timeout") Fake::silentlyStopAdvertising = true;
  if (scenario == "led-init-failure") FakeRmt::initSucceeds = false;
  if (scenario == "led-write-failure") FakeRmt::writeSucceeds = false;
  if (scenario == "serial-full") Serial.txSpace = 0;
  if (scenario == "serial-limited") Serial.txSpace = 8;
  if (scenario == "serial-fills") Serial.consumeSpace = true;
  setup();
  assert(Fake::now == 0);  // No wait for USB/Serial, even without a host.
  assert(Serial.baud == 115200 && Serial.timeout == 0);
  assert(Fake::pinModes[4] == INPUT_PULLUP && Fake::pinModes[5] == INPUT_PULLUP);
  assert(Fake::pinModes[19] == 0 && Fake::pinModes[20] == 0);
  assert(FakeRmt::pin == 21);
  assert(Fake::deviceName == "MorseBridge");

  if (scenario == "advertising-timeout") {
    runLoops(2001);
    assert(MorseBle::status() == MorseBle::Status::Error);
    assert(hasLog("Status: ERROR"));
  } else if (scenario == "init-failure" || scenario == "advertising-failure") {
    assert(MorseBle::status() == MorseBle::Status::Error);
    assert(hasLog("Status: ERROR"));
    const auto logs = transitionLogs();
    runLoops(100);
    assert(transitionLogs() == logs);
    assert(Fake::pinReads == 200);  // Wiring diagnostics survive BLE startup failure.
  } else {
    assert(Fake::advertisingStarts == 1 && Fake::advertising);
    const auto waitingLogs = transitionLogs();
    runLoops(100);
    assert(transitionLogs() == waitingLogs);  // No idle transition log spam.
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
    const auto readyLogs = transitionLogs();
    const auto readyLedWrites = FakeRmt::frames.size();

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
    assert(transitionLogs() == readyLogs);
    assert(FakeRmt::frames.size() == readyLedWrites);  // No flashing on paddle edges.

    Fake::server.disconnect(1);
    loop();
    assert(Fake::advertisingStarts == 2);
    assert(MorseBle::status() == MorseBle::Status::Waiting);
    assert(Fake::hidCreations == 1);
  }
  std::printf("Headless tests passed (%s)\n", scenario.c_str());
}
