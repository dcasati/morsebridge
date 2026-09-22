#pragma once

#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include "BlePaddleKeyboard.h"
#include "PaddleLog.h"
#include "PaddleRouter.h"

#ifndef MORSEBRIDGE_DEBUG
#ifdef MORSE_PADDLE_DEBUG
#define MORSEBRIDGE_DEBUG MORSE_PADDLE_DEBUG
#else
#define MORSEBRIDGE_DEBUG 1
#endif
#endif

class PaddleDebug {
public:
  void sample(bool ditDown, bool dahDown, uint32_t now,
              const MorseBle::Diagnostics &ble,
              const MorseUsb::Diagnostics &usb = {},
              MorseBridge::Route route = MorseBridge::Route::Bluetooth) {
    if (sampled) {
      if (ditDown != lastDit) ++ditEdges;
      if (dahDown != lastDah) ++dahEdges;
    }
    sampled = true;
    lastDit = ditDown;
    lastDah = dahDown;

    if (logged && uint32_t(now - lastLog) < 100) return;
    if (DebugSerial.availableForWrite() == 0) return;
    char line[192];
    const int length = std::snprintf(
        line, sizeof(line),
        "DBG GPIO4=%s GPIO5=%s edges=%lu/%lu C=%u A=%u S=%u U=%u R=%u Q=%lu M=%02X"
        " T=%s UC=%u US=%u UR=%u UQ=%lu UM=%02X\n",
        ditDown ? "LOW" : "HIGH", dahDown ? "LOW" : "HIGH",
        static_cast<unsigned long>(ditEdges), static_cast<unsigned long>(dahEdges),
        unsigned(ble.connected), unsigned(ble.authenticated),
        unsigned(ble.subscribed), unsigned(ble.suspended), unsigned(ble.armed),
        static_cast<unsigned long>(ble.queuedReports), unsigned(ble.lastQueuedModifiers),
        route == MorseBridge::Route::Usb ? "USB"
          : route == MorseBridge::Route::Bluetooth ? "BLE" : "NONE",
        unsigned(usb.configured), unsigned(usb.suspended), unsigned(usb.armed),
        static_cast<unsigned long>(usb.queuedReports), unsigned(usb.lastQueuedModifiers));
    if (length < 0 || static_cast<size_t>(length) >= sizeof(line)) {
      PaddleLog::println("DBG: snapshot formatting failed");
      return;
    }
    if (logged && std::strcmp(line, lastLine) == 0
        && uint32_t(now - lastLog) < 2000) return;
    // Never wait for USB space. Keep edge totals until a complete snapshot fits.
    if (!PaddleLog::tryWrite(line, static_cast<size_t>(length))) return;
    std::memcpy(lastLine, line, static_cast<size_t>(length) + 1);
    lastLog = now;
    logged = true;
  }

private:
  bool sampled = false;
  bool logged = false;
  bool lastDit = false;
  bool lastDah = false;
  uint32_t ditEdges = 0;
  uint32_t dahEdges = 0;
  uint32_t lastLog = 0;
  char lastLine[192] = {};
};
