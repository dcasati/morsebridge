#pragma once

#include "BlePaddleKeyboard.h"
#include "UsbPaddleKeyboard.h"

namespace MorseBridge {

enum class Route { None, Bluetooth, Usb };

class PaddleRouter {
public:
  void update(bool dit, bool dah) {
    // A suspended USB host retains priority: do not surprise it with BLE input.
    const bool usb = MorseUsb::configured();
    MorseBle::update(dit, dah, !usb);
    const bool released = MorseBle::released();
    MorseUsb::update(dit, dah, usb && released);
    selected = usb ? Route::Usb
                  : (MorseBle::diagnostics().connected ? Route::Bluetooth : Route::None);
    if (usb) {
      current = released ? MorseUsb::status()
                         : (MorseBle::status() == MorseBle::Status::Error
                            ? MorseBle::Status::Error : MorseBle::Status::Connecting);
    } else if (MorseBle::status() != MorseBle::Status::Waiting
               && MorseBle::status() != MorseBle::Status::Error) {
      current = MorseBle::status();
    } else if (MorseUsb::status() == MorseBle::Status::Error) {
      current = MorseBle::Status::Error;
    } else {
      current = MorseBle::status();
    }
  }

  Route route() const { return selected; }
  MorseBle::Status status() const { return current; }

private:
  Route selected = Route::None;
  MorseBle::Status current = MorseBle::Status::Waiting;
};

}
