#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tests
cxx="${CXX:-c++}"
flags=(-std=c++17 -Wall -Wextra -Werror -pedantic -Itests/ble_fakes)
"$cxx" "${flags[@]}" tests/paddle_input_test.cpp -o build/tests/paddle_input_test
"$cxx" "${flags[@]}" tests/ble_keyboard_test.cpp MorsePaddle/BlePaddleKeyboard.cpp \
  -o build/tests/ble_keyboard_test
"$cxx" "${flags[@]}" tests/headless_test.cpp MorsePaddle/BlePaddleKeyboard.cpp \
  -o build/tests/headless_test
build/tests/paddle_input_test
build/tests/ble_keyboard_test
for scenario in normal init-failure advertising-failure advertising-timeout; do
  build/tests/headless_test "$scenario"
done
