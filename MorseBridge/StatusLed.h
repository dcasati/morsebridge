#pragma once

#include <Arduino.h>
#include <esp32-hal-rmt.h>
#include "BlePaddleKeyboard.h"
#include "PaddleLog.h"

class StatusLed {
public:
  void begin() {
    enabled = rmtInit(pin, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000);
    PaddleLog::println(enabled ? "LED: GPIO21 status indicator enabled"
                           : "LED: initialization failed; BLE remains available");
  }

  void update(MorseBle::Status status, uint32_t now) {
    if (!enabled) return;
    if (pending) {
      if (!rmtTransmitCompleted(pin)) {
        if (uint32_t(now - startedAt) >= 20) {
          enabled = false;
          PaddleLog::println("LED: transmission timed out; indicator disabled");
        }
        return;
      }
      pending = false;
    }
    const uint32_t color = colorFor(status);
    if (color == lastColor) return;
    // The S3-Zero onboard LED uses RGB (not GRB), MSB first, at 100 ns/tick.
    for (unsigned bit = 0; bit < 24; ++bit) {
      const bool one = (color & (uint32_t(1) << (23 - bit))) != 0;
      frame[bit].level0 = 1;
      frame[bit].duration0 = one ? 8 : 4;
      frame[bit].level1 = 0;
      frame[bit].duration1 = one ? 4 : 8;
    }
    frame[24].level0 = 0;
    frame[24].duration0 = 1500;
    frame[24].level1 = 0;
    frame[24].duration1 = 1500;  // 300 us LOW latch/reset.
    if (!rmtWriteAsync(pin, frame, 25)) {
      enabled = false;
      PaddleLog::println("LED: transmission failed; indicator disabled");
      return;
    }
    lastColor = color;
    startedAt = now;
    pending = true;
  }

private:
  static uint32_t colorFor(MorseBle::Status status) {
    switch (status) {
      case MorseBle::Status::Waiting: return 0x000008;        // Dim blue.
      case MorseBle::Status::Connecting:
      case MorseBle::Status::ReleasePaddles: return 0x080400; // Dim amber.
      case MorseBle::Status::Ready: return 0x000800;          // Dim green.
      case MorseBle::Status::Error: return 0x080000;          // Dim red.
    }
    return 0x080000;
  }

  static constexpr int pin = 21;
  bool enabled = false;
  bool pending = false;
  uint32_t startedAt = 0;
  uint32_t lastColor = UINT32_MAX;
  // Async RMT retains this buffer until completion; do not use stack storage.
  rmt_data_t frame[25] = {};
};
