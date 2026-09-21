#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tests
cxx="${CXX:-c++}"
flags=(-std=c++17 -Wall -Wextra -Werror -pedantic -Itests/ble_fakes)
"$cxx" "${flags[@]}" tests/paddle_input_test.cpp -o build/tests/paddle_input_test
"$cxx" "${flags[@]}" tests/ble_keyboard_test.cpp MorseBridge/BlePaddleKeyboard.cpp \
  -o build/tests/ble_keyboard_test
"$cxx" "${flags[@]}" tests/headless_test.cpp MorseBridge/BlePaddleKeyboard.cpp \
  -o build/tests/headless_test
"$cxx" "${flags[@]}" tests/debug_test.cpp -o build/tests/debug_test
"$cxx" "${flags[@]}" tests/status_led_test.cpp -o build/tests/status_led_test
"$cxx" "${flags[@]}" tests/log_test.cpp -o build/tests/log_test
"$cxx" "${flags[@]}" -DMORSEBRIDGE_DEBUG=0 \
  tests/headless_test.cpp MorseBridge/BlePaddleKeyboard.cpp \
  -o build/tests/headless_no_debug_test
build/tests/paddle_input_test
build/tests/ble_keyboard_test
build/tests/debug_test
build/tests/status_led_test
build/tests/log_test
for scenario in normal init-failure advertising-failure advertising-timeout led-init-failure led-write-failure serial-full serial-limited serial-fills; do
  build/tests/headless_test "$scenario"
  build/tests/headless_no_debug_test "$scenario"
done
