#include "UsbPaddleKeyboard.h"
#include "PaddleLog.h"
#include "ReportSession.h"
#include "KeyboardDescriptor.h"

#include <USB.h>
#include <USBHID.h>
#include <tusb.h>
#include <atomic>
#include <cstring>

#if ARDUINO_USB_MODE != 0 || ARDUINO_USB_CDC_ON_BOOT
#error "Select USB-OTG (TinyUSB), USB CDC On Boot Disabled; MorseBridge starts CDC manually"
#endif

namespace MorseUsb {
namespace {

class PaddleKeyboard : public USBHIDDevice {
  uint16_t _onGetDescriptor(uint8_t *buffer) override {
    std::memcpy(buffer, MorseBridge::keyboardDescriptor, sizeof(MorseBridge::keyboardDescriptor));
    return sizeof(MorseBridge::keyboardDescriptor);
  }
};

USBHID hid(HID_ITF_PROTOCOL_KEYBOARD);
PaddleKeyboard keyboard;
MorseBridge::ReportSession session;
std::atomic<uint32_t> epoch{0};
bool started = false;
bool failed = false;
bool previousMounted = false;
bool previousSuspended = false;
uint32_t queuedReports = 0;
uint8_t lastQueuedModifiers = 0;

void usbEvent(void *, esp_event_base_t, int32_t event, void *) {
  if (event == ARDUINO_USB_STARTED_EVENT || event == ARDUINO_USB_STOPPED_EVENT
      || event == ARDUINO_USB_SUSPEND_EVENT || event == ARDUINO_USB_RESUME_EVENT) {
    ++epoch;
  }
}

void abortReports() {
  if (failed) return;
  failed = true;
  PaddleLog::println("USB: HID send/release stalled or failed; detaching USB, reset required");
  // Removing the USB device releases host-side keyboard state, even without key-up.
  tud_disconnect();
}

MorseBridge::SendResult send(const MorseBle::KeyboardReport &report) {
  if (!tud_hid_n_ready(0)) return MorseBridge::SendResult::Busy;
  const uint8_t id = tud_hid_n_get_protocol(0) == HID_PROTOCOL_BOOT ? 0 : 1;
  // The core's SendReport waits for completion; TinyUSB copies/queues without waiting.
  if (!tud_hid_n_report(0, id, &report, sizeof(report))) {
    PaddleLog::println("USB: could not queue HID report");
    return MorseBridge::SendResult::Failed;
  }
  ++queuedReports;
  lastQueuedModifiers = report.modifiers;
  return MorseBridge::SendResult::Accepted;
}

}

bool begin() {
  if (started) return !failed;
  if (!USBHID::addDevice(&keyboard, sizeof(MorseBridge::keyboardDescriptor))
      || !USB.productName("MorseBridge")
      || !USB.manufacturerName("MorseBridge")
      || !USB.usbPower(250)) {
    failed = true;
    PaddleLog::println("USB: descriptor configuration failed");
    return false;
  }
  USB.onEvent(usbEvent);
  hid.begin();
  started = USB.begin();
  failed = !started;
  session.reset(millis());
  PaddleLog::println(started ? "USB: MorseBridge HID + diagnostic CDC started"
                            : "USB: initialization failed");
  return started;
}

bool configured() {
  return started && tud_mounted();
}

void update(bool dit, bool dah, bool selected) {
  const bool mounted = configured();
  const bool suspended = mounted && tud_suspended();
  // Poll as well as handling events: routing need not wait for the event task.
  if (mounted != previousMounted || suspended != previousSuspended) ++epoch;
  previousMounted = mounted;
  previousSuspended = suspended;
  session.update(dit, dah, millis(), selected, mounted,
                 mounted && !suspended && !failed, epoch, send, abortReports);
}

MorseBle::Status status() {
  if (failed) return MorseBle::Status::Error;
  if (!configured()) return MorseBle::Status::Waiting;
  if (tud_suspended()) return MorseBle::Status::Connecting;
  return session.armed() ? MorseBle::Status::UsbReady : MorseBle::Status::ReleasePaddles;
}

Diagnostics diagnostics() {
  return {configured(), configured() && tud_suspended(),
          status() == MorseBle::Status::UsbReady, queuedReports, lastQueuedModifiers};
}

}
