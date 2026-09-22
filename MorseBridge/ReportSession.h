#pragma once

#include "PaddleInput.h"

namespace MorseBridge {

enum class SendResult { Accepted, Busy, Failed };

class ReportSession {
public:
  void reset(uint32_t now) {
    paddles.reset(now);
    wasReady = false;
    possiblyHeld = false;
    lastModifiers = 0xFF;
    failures = 0;
    waiting = false;
    uncertain = false;
    lastAttempt = now - 10;
  }

  template <typename Send, typename Abort>
  void update(bool dit, bool dah, uint32_t now, bool selected, bool connected,
              bool ready, uint32_t epoch, Send send, Abort abort) {
    if (!connected) {
      reset(now);
      return;
    }
    if (!selected) {
      wasReady = false;
      paddles.reset(now);
      if (possiblyHeld) {
        // A suspended/unsubscribed old host cannot receive a key-up: disconnect.
        if (!ready) abort();
        else transmit(MorseBle::KeyboardReport{}, now, send, abort);
      }
      return;
    }
    if (!ready) {
      wasReady = false;
      paddles.reset(now);
      return;
    }
    if (!wasReady || epoch != observedEpoch) {
      paddles.reset(now);
      lastModifiers = 0xFF;
      failures = 0;
      waiting = false;
      lastAttempt = now - 10;
      observedEpoch = epoch;
      wasReady = true;
    }
    const auto report = lastModifiers == 0xFF
        ? MorseBle::KeyboardReport{} : paddles.sample(dit, dah, now);
    if (report.modifiers != lastModifiers || uncertain) {
      const bool firstNeutral = lastModifiers == 0xFF;
      if (transmit(report, now, send, abort) && firstNeutral) paddles.reset(now);
    } else {
      // A busy, never-queued edge may have been cancelled by a later contact edge.
      waiting = false;
      failures = 0;
    }
  }

  bool armed() const { return wasReady && paddles.isArmed(); }
  bool released(bool connected) const { return !connected || !possiblyHeld; }

private:
  template <typename Send, typename Abort>
  bool transmit(const MorseBle::KeyboardReport &report, uint32_t now,
                Send send, Abort abort) {
    if (failures && uint32_t(now - lastAttempt) < 10) return false;
    lastAttempt = now;
    const auto result = send(report);
    if (result == SendResult::Accepted) {
      possiblyHeld = report.modifiers != 0;
      lastModifiers = report.modifiers;
      failures = 0;
      waiting = false;
      uncertain = false;
      return true;
    }
    if (result == SendResult::Failed) {
      // A failed completion may still have delivered the report.
      uncertain = true;
      if (report.modifiers) possiblyHeld = true;
      if (++failures >= 3) abort();
    } else {
      if (!waiting) {
        waiting = true;
        waitingSince = now;
      } else if (uint32_t(now - waitingSince) >= 250) {
        abort();
      }
    }
    return false;
  }

  MorseBle::PaddleInput paddles;
  bool wasReady = false;
  bool possiblyHeld = false;
  bool waiting = false;
  bool uncertain = false;
  uint8_t lastModifiers = 0xFF;
  uint8_t failures = 0;
  uint32_t lastAttempt = 0;
  uint32_t waitingSince = 0;
  uint32_t observedEpoch = 0;
};

}
