#include "../MorseBridge/PaddleInput.h"

#include <cassert>
#include <cstdio>

using namespace MorseBle;

int main() {
  PaddleInput input;
  input.reset(0);
  assert(input.sample(false, false, 4).modifiers == 0);
  assert(!input.isArmed());
  input.sample(false, false, 5);
  assert(input.isArmed());

  assert(input.sample(true, false, 10).modifiers == 0);
  assert(input.sample(true, false, 14).modifiers == 0);
  assert(input.sample(true, false, 15).modifiers == ditModifier);
  assert(input.sample(true, false, 100).modifiers == ditModifier);
  assert(input.sample(true, true, 101).modifiers == ditModifier);
  assert(input.sample(true, true, 106).modifiers == (ditModifier | dahModifier));
  assert(input.sample(false, true, 110).modifiers == (ditModifier | dahModifier));
  assert(input.sample(false, true, 115).modifiers == dahModifier);
  assert(input.sample(false, false, 120).modifiers == dahModifier);
  const auto released = input.sample(false, false, 125);
  assert(released.modifiers == 0);
  assert(released.reserved == 0);
  for (uint8_t key : released.keys) assert(key == 0);

  // Chatter on either edge must not generate a new press/release.
  assert(input.sample(true, false, 130).modifiers == 0);
  assert(input.sample(false, false, 132).modifiers == 0);
  assert(input.sample(true, false, 134).modifiers == 0);
  assert(input.sample(true, false, 138).modifiers == 0);
  assert(input.sample(true, false, 139).modifiers == ditModifier);
  assert(input.sample(false, false, 140).modifiers == ditModifier);
  assert(input.sample(true, false, 142).modifiers == ditModifier);
  assert(input.sample(false, false, 144).modifiers == ditModifier);
  assert(input.sample(false, false, 149).modifiers == 0);

  // Reconnection/entry while squeezed: never replay a held paddle.
  input.reset(200);
  assert(input.sample(true, true, 200).modifiers == 0);
  assert(input.sample(true, true, 300).modifiers == 0);
  assert(!input.isArmed());
  assert(input.sample(false, true, 310).modifiers == 0);
  assert(input.sample(false, true, 320).modifiers == 0);
  assert(!input.isArmed());
  input.sample(false, false, 330);
  input.sample(false, false, 334);
  assert(!input.isArmed());
  input.sample(false, false, 335);
  assert(input.isArmed());
  input.sample(true, true, 340);
  assert(input.sample(true, true, 345).modifiers == 0x11);

  // Debounce and rearming must work across millis() rollover.
  input.reset(UINT32_MAX - 3);
  input.sample(false, false, 2);
  assert(input.isArmed());
  input.reset(UINT32_MAX - 20);
  input.sample(false, false, UINT32_MAX - 15);
  input.sample(false, true, UINT32_MAX - 2);
  assert(input.sample(false, true, 1).modifiers == 0);
  assert(input.sample(false, true, 2).modifiers == dahModifier);
  input.reset(3);
  assert(input.sample(false, false, 3).modifiers == 0);
  assert(!input.isArmed());
  std::puts("Paddle input tests passed");
}
