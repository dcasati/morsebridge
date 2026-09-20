#pragma once

namespace MorseBle {

enum class Status { Waiting, Connecting, ReleasePaddles, Ready, Error };

bool begin();
void update(bool ditDown, bool dahDown);
bool end();
Status status();

}
