#pragma once
// MAX7219 4-in-1 (32x8) driven over hardware SPI through the SN74AHCT125N.
// Text is drawn like the simulator's matrix: its 5x7 font, centred.

#include <stdint.h>

// Module layouts differ between sellers. If text looks mirrored, upside down
// or with the four blocks in the wrong order, flip these and run "test display".
constexpr bool kMatrixReverseModules = false;  // false: module nearest DIN shows the rightmost 8 columns
constexpr bool kMatrixMirrorColumns = false;
constexpr bool kMatrixFlipRows = false;
constexpr uint8_t kMatrixIntensity = 4;  // 0..15

void displayBegin();
void displayText(const char* text);  // centred; A-Z 0-9 . + - : space (others show as space)
void displayFrequency(float hz);
void displayTestPattern();
int displayTextWidth(const char* text);  // columns used, for checks
// Rewrites the control registers and the current picture. A MAX7219 that lost
// power or caught noise comes back by itself; call about once a second.
void displayKeepAlive();
