#pragma once
// CO5300 480x480 AMOLED over QSPI. Pixels are RGB565 in native byte order.
#include <stdint.h>
void lcdBegin();
// Push full-width rows [y0, y1) from a 480-pixel-wide framebuffer.
void lcdPushRows(const uint16_t* frame, int y0, int y1);
void lcdBrightness(uint8_t level);
