#pragma once
// Touch UI: status header, 2x2 pad grid, service footer.
#include "../controller.h"
void screenBegin();
// Pad index under a screen point, or -1.
int screenPadAt(int x, int y);
// Redraws only when what is shown changed. touchedPad = pad under the finger.
void screenUpdate(const ControlSnapshot& s, int touchedPad, uint32_t now, bool touched);
