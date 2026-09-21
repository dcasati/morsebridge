#include "ble_fakes/BLEDevice.h"
#include "../MorseBridge/BlePaddleKeyboard.h"

#include <cstdio>

using MorseBle::Status;

void sample(uint32_t now, bool dit, bool dah) {
  Fake::now = now;
  MorseBle::update(dit, dah);
}

void expectReport(uint8_t modifiers) {
  const auto &report = Fake::input.reports.back();
  assert(report.size() == 8);
  assert(report[0] == modifiers);
  for (size_t i = 1; i < report.size(); ++i) assert(report[i] == 0);
}

void authenticate() {
  Fake::authenticate();
  Fake::input.subscribe(1);
}

int main() {
  assert(MorseBle::end());
  Fake::initSucceeds = false;
  assert(!MorseBle::begin());
  assert(MorseBle::status() == Status::Error);
  Fake::initSucceeds = true;
  assert(MorseBle::begin());
  assert(Fake::deviceName == "MorseBridge");
  const std::string manufacturer(Fake::manufacturer.value.begin(),
                                 Fake::manufacturer.value.end());
  assert(manufacturer == "MorseBridge");
  assert(Fake::advertising && Fake::hidCreations == 1);
  assert(MorseBle::status() == Status::Waiting);
  assert(Fake::security->onSecurityRequest());

  Fake::server.connect();
  sample(0, true, true);
  assert(Fake::input.reports.empty());
  assert(MorseBle::status() == Status::Connecting);
  Fake::authenticate();
  sample(1, true, true);
  assert(Fake::input.reports.empty());  // Not subscribed yet.
  Fake::input.subscribe(1);
  sample(2, true, true);
  expectReport(0);
  assert(MorseBle::diagnostics().queuedReports == 1);
  assert(MorseBle::diagnostics().lastQueuedModifiers == 0);
  assert(MorseBle::diagnostics().connected && MorseBle::diagnostics().authenticated);
  assert(MorseBle::diagnostics().subscribed && !MorseBle::diagnostics().armed);
  assert(MorseBle::status() == Status::ReleasePaddles);
  sample(100, true, true);
  assert(Fake::input.reports.size() == 1);
  sample(101, false, false);
  sample(106, false, false);
  assert(MorseBle::status() == Status::Ready);

  sample(110, true, false);
  sample(115, true, false);
  expectReport(0x01);
  sample(116, true, true);
  sample(121, true, true);  // Six ms later: must not be throttled away.
  expectReport(0x11);
  assert(MorseBle::diagnostics().lastQueuedModifiers == 0x11);
  assert(MorseBle::diagnostics().armed);
  sample(122, false, true);
  sample(127, false, true);
  expectReport(0x10);
  const auto reportCount = Fake::input.reports.size();
  sample(200, false, true);
  assert(Fake::input.reports.size() == reportCount);

  // Reconnect even between update calls: cached/first reports must be neutral.
  Fake::server.disconnect(1);
  MorseBle::update(false, true);
  assert(!MorseBle::diagnostics().connected && !MorseBle::diagnostics().armed);
  assert(Fake::advertisingStarts == 2);
  Fake::server.connect();
  assert(Fake::input.value[0] == 0);
  authenticate();
  sample(210, false, true);
  expectReport(0);
  sample(220, false, false);
  sample(225, false, false);
  sample(230, true, true);
  sample(235, true, true);
  expectReport(0x11);

  uint8_t suspend = 0;
  Fake::control.setValue(&suspend, 1);
  Fake::control.callbacks->onWrite(&Fake::control);
  sample(240, true, true);
  assert(MorseBle::status() == Status::Connecting);
  suspend = 1;
  Fake::control.setValue(&suspend, 1);
  Fake::control.callbacks->onWrite(&Fake::control);
  sample(245, true, true);
  expectReport(0);
  assert(MorseBle::status() == Status::ReleasePaddles);

  sample(250, false, false);
  sample(255, false, false);
  sample(260, true, false);
  sample(265, true, false);
  expectReport(1);
  assert(MorseBle::end());
  expectReport(0);
  assert(!Fake::advertising);
  assert(!Fake::security->onSecurityRequest());
  const auto afterExit = Fake::input.reports.size();
  sample(400, true, true);
  assert(Fake::input.reports.size() == afterExit);

  assert(MorseBle::begin());
  assert(Fake::hidCreations == 1);  // Reuse services on repeated entry.
  Fake::server.connect();
  authenticate();
  sample(410, false, false);
  sample(415, false, false);
  Fake::input.notifySucceeds = false;
  const auto queuedBeforeFailure = MorseBle::diagnostics().queuedReports;
  sample(420, true, false);
  sample(425, true, false);
  assert(MorseBle::diagnostics().queuedReports == queuedBeforeFailure);
  const auto attempts = Fake::input.attempts;
  sample(426, true, false);
  assert(Fake::input.attempts == attempts);  // Failed report retry is bounded.
  Fake::input.notifySucceeds = true;
  sample(435, true, false);
  expectReport(1);
  Fake::input.notifySucceeds = false;
  sample(440, false, false);
  sample(445, false, false);
  sample(455, false, false);
  sample(465, false, false);
  assert(MorseBle::status() == Status::Error);
  assert(MorseBle::end());
  Fake::input.notifySucceeds = true;

  assert(MorseBle::begin());
  Fake::server.connect();
  Fake::authenticate(false, false);
  assert(MorseBle::status() == Status::Error);
  assert(MorseBle::end());

  assert(MorseBle::begin());
  Fake::server.connect();
  Fake::authenticate(true, false);
  assert(MorseBle::status() == Status::Error);  // Encryption alone is not a bond.
  assert(MorseBle::end());

  // A failed initial neutral must never allow a press to become the first report.
  assert(MorseBle::begin());
  Fake::server.connect();
  authenticate();
  Fake::input.notifySucceeds = false;
  sample(600, false, false);
  sample(605, false, false);
  sample(606, true, true);
  Fake::input.notifySucceeds = true;
  sample(611, true, true);
  expectReport(0);
  assert(MorseBle::status() == Status::ReleasePaddles);
  sample(620, true, true);
  expectReport(0);
  sample(625, false, false);
  sample(630, false, false);
  assert(MorseBle::status() == Status::Ready);
  sample(635, true, false);
  sample(640, true, false);
  expectReport(1);

  // Unsubscribe/resubscribe between loop calls still requires neutral/release.
  Fake::input.subscribe(0);
  Fake::input.subscribe(1);
  sample(645, true, false);
  expectReport(0);
  assert(MorseBle::status() == Status::ReleasePaddles);
  sample(650, false, false);
  sample(655, false, false);
  assert(MorseBle::status() == Status::Ready);
  Fake::input.subscribe(2);  // Indications alone are not a keyboard subscription.
  sample(656, false, false);
  assert(MorseBle::status() == Status::Connecting);
  Fake::input.subscribe(1);
  sample(657, false, false);
  sample(662, false, false);
  assert(MorseBle::status() == Status::Ready);
  sample(663, true, false);
  sample(668, true, false);
  expectReport(1);

  // A suspend/resume pair between loop calls must not replay a held modifier.
  suspend = 0;
  Fake::control.setValue(&suspend, 1);
  Fake::control.callbacks->onWrite(&Fake::control);
  suspend = 1;
  Fake::control.setValue(&suspend, 1);
  Fake::control.callbacks->onWrite(&Fake::control);
  sample(669, true, false);
  expectReport(0);
  assert(MorseBle::status() == Status::ReleasePaddles);
  sample(670, false, false);
  sample(675, false, false);
  sample(676, true, false);
  sample(681, true, false);
  expectReport(1);

  const auto startsBeforeFastReconnect = Fake::advertisingStarts;
  Fake::server.disconnect(1);
  Fake::server.connect();  // No update between disconnect and reconnect.
  authenticate();
  sample(682, true, false);
  assert(Fake::advertisingStarts == startsBeforeFastReconnect);
  expectReport(0);
  assert(MorseBle::status() == Status::ReleasePaddles);
  assert(MorseBle::end());

  Fake::startSucceeds = false;
  assert(!MorseBle::begin());
  assert(MorseBle::status() == Status::Error);
  assert(MorseBle::end());
  Fake::startSucceeds = true;
  assert(MorseBle::begin());
  Fake::advertising = false;
  BLEDevice::getAdvertising()->onComplete(BLEDevice::getAdvertising());
  sample(10000, false, false);
  assert(MorseBle::status() == Status::Waiting);
  sample(11999, false, false);
  assert(MorseBle::status() == Status::Waiting);
  sample(12000, false, false);
  assert(MorseBle::status() == Status::Error);
  assert(MorseBle::end());
  assert(!Fake::advertising);

  // A bonded host can finish security/subscription before the connect callback.
  assert(MorseBle::begin());
  authenticate();
  Fake::server.connect();
  assert(MorseBle::diagnostics().authenticated);
  assert(MorseBle::diagnostics().subscribed);
  sample(800, false, false);
  sample(805, false, false);
  assert(MorseBle::status() == Status::Ready);
  sample(810, true, false);
  sample(815, true, false);
  expectReport(1);
  assert(MorseBle::end());
  assert(!MorseBle::diagnostics().authenticated);
  assert(!MorseBle::diagnostics().subscribed);

  // A long advertising period must not consume the connection handoff grace.
  assert(MorseBle::begin());
  sample(20000, false, false);
  Fake::advertising = false;
  BLEDevice::getAdvertising()->onComplete(BLEDevice::getAdvertising());
  sample(30000, false, false);
  assert(MorseBle::status() == Status::Waiting);
  Fake::server.connect();
  authenticate();
  sample(30001, false, false);
  sample(30006, false, false);
  assert(MorseBle::status() == Status::Ready);
  assert(MorseBle::end());

  // Recovery restarts the outage timer, including across millis rollover.
  assert(MorseBle::begin());
  Fake::advertising = false;
  sample(UINT32_MAX - 999, false, false);
  sample(999, false, false);
  assert(MorseBle::status() == Status::Waiting);
  Fake::advertising = true;
  sample(1000, false, false);
  Fake::advertising = false;
  sample(1001, false, false);
  sample(3000, false, false);
  assert(MorseBle::status() == Status::Waiting);
  sample(3001, false, false);
  assert(MorseBle::status() == Status::Error);
  assert(MorseBle::end());
  std::puts("BLE keyboard lifecycle tests passed");
}
