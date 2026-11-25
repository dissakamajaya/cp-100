#pragma once
// CST9217 single-point touch, mapped to screen coordinates.
#include <stdint.h>
struct TouchPoint { bool down = false; int16_t x = 0, y = 0; };
void touchBegin();
TouchPoint touchRead();
