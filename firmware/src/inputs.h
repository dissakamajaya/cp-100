#pragma once
// Four momentary visitor pads and an optional physical MUTE button.
#include <stdint.h>
struct InputEvents { uint8_t padDown = 0, padUp = 0; bool mutePressed = false; };
void inputsBegin();
InputEvents inputsPoll();
