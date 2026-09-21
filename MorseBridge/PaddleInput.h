#pragma once

#include <stdint.h>

namespace MorseBle {

constexpr uint32_t debounceMs = 5;
constexpr uint8_t ditModifier = 0x01;  // HID Left Control
constexpr uint8_t dahModifier = 0x10;  // HID Right Control

struct KeyboardReport {
  uint8_t modifiers = 0;
  uint8_t reserved = 0;
  uint8_t keys[6] = {};
};
static_assert(sizeof(KeyboardReport) == 8, "HID keyboard report must be 8 bytes");

class PaddleInput {
public:
  void reset(uint32_t now) {
    dit = Contact{};
    dah = Contact{};
    lastActivity = now;
    armed = false;
  }

  KeyboardReport sample(bool ditDown, bool dahDown, uint32_t now) {
    dit.update(ditDown, now);
    dah.update(dahDown, now);
    if (ditDown || dahDown) lastActivity = now;
    // A newly connected host must see neutral before accepting paddle presses.
    if (!armed && !ditDown && !dahDown && !dit.stable && !dah.stable
        && uint32_t(now - lastActivity) >= debounceMs) {
      armed = true;
    }
    KeyboardReport report;
    if (armed) {
      report.modifiers = (dit.stable ? ditModifier : 0)
                       | (dah.stable ? dahModifier : 0);
    }
    return report;
  }

  bool isArmed() const { return armed; }

private:
  struct Contact {
    bool raw = false;
    bool stable = false;
    uint32_t changedAt = 0;

    void update(bool down, uint32_t now) {
      if (down != raw) {
        raw = down;
        changedAt = now;
      }
      if (uint32_t(now - changedAt) >= debounceMs) stable = raw;
    }
  };

  Contact dit;
  Contact dah;
  uint32_t lastActivity = 0;
  bool armed = false;
};

}
