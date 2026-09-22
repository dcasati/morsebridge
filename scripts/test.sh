#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tests
cxx="${CXX:-c++}"
flags=(-std=c++17 -Wall -Wextra -Werror -pedantic -Itests/ble_fakes)
"$cxx" "${flags[@]}" tests/paddle_input_test.cpp -o build/tests/paddle_input_test
"$cxx" "${flags[@]}" tests/report_session_test.cpp -o build/tests/report_session_test
"$cxx" "${flags[@]}" tests/ble_keyboard_test.cpp MorseBridge/BlePaddleKeyboard.cpp \
  -o build/tests/ble_keyboard_test
"$cxx" "${flags[@]}" tests/headless_test.cpp MorseBridge/BlePaddleKeyboard.cpp MorseBridge/UsbPaddleKeyboard.cpp \
  -o build/tests/headless_test
"$cxx" "${flags[@]}" tests/debug_test.cpp -o build/tests/debug_test
"$cxx" "${flags[@]}" tests/status_led_test.cpp -o build/tests/status_led_test
"$cxx" "${flags[@]}" tests/log_test.cpp -o build/tests/log_test
"$cxx" "${flags[@]}" -DMORSEBRIDGE_DEBUG=0 \
  tests/headless_test.cpp MorseBridge/BlePaddleKeyboard.cpp MorseBridge/UsbPaddleKeyboard.cpp \
  -o build/tests/headless_no_debug_test
for debug in 0 1; do
  "$cxx" "${flags[@]}" -DMORSEBRIDGE_DEBUG="$debug" \
    tests/dual_transport_test.cpp MorseBridge/BlePaddleKeyboard.cpp MorseBridge/UsbPaddleKeyboard.cpp \
    -o "build/tests/dual_transport_$debug"
done
build/tests/paddle_input_test
build/tests/report_session_test
build/tests/ble_keyboard_test
build/tests/debug_test
build/tests/status_led_test
build/tests/log_test
for scenario in normal init-failure advertising-failure advertising-timeout led-init-failure led-write-failure serial-full serial-limited serial-fills; do
  build/tests/headless_test "$scenario"
  build/tests/headless_no_debug_test "$scenario"
done
for scenario in both usb-only power-only ble-init-failure usb-init-failure descriptor-failure \
  serial-full serial-fills serial-64 no-cdc handoff-held handoff-retry handoff-failed handoff-suspended \
  handoff-unsubscribed initial-busy busy-timeout send-failure boot-protocol suspend \
  rapid-resume rapid-remount unplug-held release-busy release-timeout; do
  build/tests/dual_transport_0 "$scenario"
  build/tests/dual_transport_1 "$scenario"
done
