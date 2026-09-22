#include "ble_fakes/BLEDevice.h"
#include "ble_fakes/USB.h"
#include "../MorseBridge/MorseBridge.ino"
#include "../MorseBridge/KeyboardDescriptor.h"

#include <cstdio>

using MorseBle::Status;
using MorseBridge::Route;

void loops(unsigned count) {
  const auto before = Fake::now;
  for (unsigned i = 0; i < count; ++i) loop();
  assert(uint32_t(Fake::now - before) == count);
}

void contacts(bool dit, bool dah) {
  Fake::pinValues[4] = dit ? LOW : HIGH;
  Fake::pinValues[5] = dah ? LOW : HIGH;
  loops(6);
}

void connectBle() {
  Fake::server.connect();
  Fake::authenticate();
  Fake::input.subscribe(1);
  loops(10);
}

void mount() {
  FakeUsb::event(ARDUINO_USB_STARTED_EVENT);
  loops(10);
}

void neutralOnly() {
  for (const auto &report : FakeUsb::reports) assert(report[0] == 0);
}

bool logged(const char *text) {
  for (const auto &line : Serial.lines) {
    if (line.find(text) != std::string::npos) return true;
  }
  return false;
}

int main(int argc, char **argv) {
  assert(argc == 2);
  const std::string scenario = argv[1];
  Fake::pinValues.fill(HIGH);
  if (scenario == "ble-init-failure") Fake::initSucceeds = false;
  if (scenario == "usb-init-failure") FakeUsb::beginSucceeds = false;
  if (scenario == "descriptor-failure") FakeUsb::descriptorSucceeds = false;
  if (scenario == "serial-full") Serial.txSpace = 0;
  if (scenario == "serial-64") Serial.txSpace = 64;
  if (scenario == "serial-fills") Serial.consumeSpace = true;
  if (scenario == "no-cdc") FakeUsb::cdcConnected = false;
  setup();
  assert(Fake::now == 0);
  if (scenario == "usb-init-failure" || scenario == "descriptor-failure") {
    assert(MorseUsb::status() == Status::Error);
    assert(router.status() == Status::Error);
    assert(logged(scenario == "usb-init-failure" ? "initialization failed"
                                               : "descriptor configuration failed"));
    connectBle();
    assert(router.status() == Status::Ready);
    contacts(true, false);
    assert(Fake::input.reports.back()[0] == 1);
  } else {
    assert(FakeUsb::product == "MorseBridge" && FakeUsb::manufacturer == "MorseBridge");
    assert(FakeUsb::descriptor == std::vector<uint8_t>(
        std::begin(MorseBridge::keyboardDescriptor), std::end(MorseBridge::keyboardDescriptor)));
    if (scenario != "usb-only" && scenario != "ble-init-failure") connectBle();
    if (scenario == "power-only") {
      assert(!MorseUsb::configured());
      assert(router.route() == Route::Bluetooth && router.status() == Status::Ready);
      contacts(true, true);
      assert(Fake::input.reports.back()[0] == 0x11 && FakeUsb::reports.empty());
    } else if (scenario == "handoff-held" || scenario == "handoff-retry"
               || scenario == "handoff-failed" || scenario == "handoff-suspended"
               || scenario == "handoff-unsubscribed") {
      contacts(true, true);
      assert(Fake::input.reports.back()[0] == 0x11);
      Fake::server.deferDisconnect = true;
      if (scenario == "handoff-retry" || scenario == "handoff-failed") {
        Fake::input.notifySucceeds = false;
      }
      if (scenario == "handoff-suspended") {
        const uint8_t suspend = 0;
        Fake::control.setValue(&suspend, 1);
        Fake::control.callbacks->onWrite(&Fake::control);
      }
      if (scenario == "handoff-unsubscribed") Fake::input.subscribe(0);
      mount();
      if (scenario == "handoff-retry") {
        assert(FakeUsb::reports.empty());
        Fake::input.notifySucceeds = true;
        loops(10);
        assert(Fake::input.reports.back()[0] == 0);
      } else if (scenario != "handoff-held") {
        loops(30);
        assert(Fake::server.disconnectPending && Fake::server.disconnects == 1);
        assert(FakeUsb::reports.empty());
        assert(router.status() == Status::Error);
        Fake::server.completeDisconnect();
        loops(10);
      } else {
        assert(Fake::input.reports.back()[0] == 0);
      }
      neutralOnly();
      assert(router.status() == Status::ReleasePaddles);
      contacts(false, false);
      assert(router.status() == Status::UsbReady);
      contacts(true, false);
      assert(FakeUsb::reports.back()[0] == 1);
    } else {
      if (scenario == "initial-busy" || scenario == "busy-timeout") FakeUsb::endpointReady = false;
      if (scenario == "send-failure") FakeUsb::queueSucceeds = false;
      if (scenario == "boot-protocol") FakeUsb::bootProtocol = true;
      mount();
      if (scenario == "initial-busy") {
        assert(FakeUsb::reports.empty() && router.status() == Status::ReleasePaddles);
        contacts(true, false);
        FakeUsb::endpointReady = true;
        loops(10);
        neutralOnly();
        assert(router.status() == Status::ReleasePaddles);
        contacts(false, false);
        assert(router.status() == Status::UsbReady);
      } else if (scenario == "busy-timeout" || scenario == "send-failure") {
        loops(300);
        assert(router.status() == Status::Error && FakeUsb::disconnects == 1);
        assert(FakeUsb::detached && FakeUsb::reports.empty());
        if (scenario == "send-failure") assert(FakeUsb::attempts == 3);
        else assert(FakeUsb::attempts == 0);
        assert(logged("reset required"));
      } else {
        assert(router.route() == Route::Usb && router.status() == Status::UsbReady);
        const auto bleReports = Fake::input.reports.size();
        contacts(true, false);
        assert(FakeUsb::reports.back()[0] == 1);
        contacts(true, true);
        assert(FakeUsb::reports.back()[0] == 0x11);
        contacts(false, true);
        assert(FakeUsb::reports.back()[0] == 0x10);
        const auto heldReports = FakeUsb::reports.size();
        loops(100);
        assert(FakeUsb::reports.size() == heldReports);
        assert(Fake::input.reports.size() == bleReports);
        if (scenario == "suspend" || scenario == "rapid-resume") {
          FakeUsb::event(ARDUINO_USB_SUSPEND_EVENT);
          if (scenario == "suspend") {
            loops(30);
            assert(router.route() == Route::Usb && router.status() == Status::Connecting);
            assert(FakeUsb::reports.size() == heldReports);
            assert(Fake::input.reports.size() == bleReports);
          }
          FakeUsb::event(ARDUINO_USB_RESUME_EVENT);
          loops(10);
          assert(FakeUsb::reports.back()[0] == 0);
          assert(router.status() == Status::ReleasePaddles);
          contacts(false, false);
          assert(router.status() == Status::UsbReady);
        } else if (scenario == "rapid-remount") {
          FakeUsb::event(ARDUINO_USB_STOPPED_EVENT);
          FakeUsb::event(ARDUINO_USB_STARTED_EVENT);
          loops(10);
          assert(FakeUsb::reports.back()[0] == 0);
          assert(router.status() == Status::ReleasePaddles);
          assert(Fake::input.reports.size() == bleReports);
        } else if (scenario == "unplug-held") {
          FakeUsb::event(ARDUINO_USB_STOPPED_EVENT);
          loops(10);
          assert(router.route() == Route::Bluetooth && router.status() == Status::ReleasePaddles);
          assert(Fake::input.reports.back()[0] == 0);
          assert(FakeUsb::reports.size() == heldReports);
          contacts(false, false);
          assert(router.status() == Status::Ready);
          contacts(true, false);
          assert(Fake::input.reports.back()[0] == 1);
        } else if (scenario == "release-busy") {
          FakeUsb::endpointReady = false;
          contacts(false, false);
          assert(FakeUsb::reports.back()[0] == 0x10);
          assert(router.status() == Status::UsbReady);
          FakeUsb::endpointReady = true;
          loops(1);
          assert(FakeUsb::reports.back()[0] == 0);
        } else if (scenario == "release-timeout") {
          FakeUsb::endpointReady = false;
          contacts(false, false);
          loops(260);
          assert(router.status() == Status::Error && FakeUsb::detached);
          assert(Fake::input.reports.size() == bleReports);
          assert(FakeUsb::disconnects == 1);
        } else {
          contacts(false, false);
          assert(FakeUsb::reports.back()[0] == 0);
          for (const auto id : FakeUsb::reportIds) assert(id == (FakeUsb::bootProtocol ? 0 : 1));
          assert(router.status() == Status::UsbReady);
          assert(Fake::input.reports.size() == bleReports);
#if MORSEBRIDGE_DEBUG
          if ((Serial.txSpace == 256 || scenario == "serial-64") && FakeUsb::cdcConnected) {
            assert(logged("T=USB UC=1 US=0 UR=1"));
          }
#endif
          if (!FakeUsb::cdcConnected) assert(Serial.lines.empty());
        }
      }
    }
  }
  std::printf("Dual transport tests passed (%s)\n", scenario.c_str());
}
