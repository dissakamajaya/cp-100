#include "display.h"

#include <Arduino.h>
#include <SPI.h>
#include <string.h>

#include "pins.h"

namespace {
constexpr int kModules = 4;
constexpr int kColumns = kModules * 8;

SPIClass spi(FSPI);
const SPISettings kSpi(1000000, MSBFIRST, SPI_MODE0);
uint8_t columns[kColumns];  // bit 0 = top row

struct Glyph {
  char ch;
  uint8_t width;
  uint8_t columns[5];  // bit 0 = top row
};
// "sim" glyphs are the simulator's font (app.js); "extra" fills letters it lacks.
const Glyph kFont[] = {
    {' ', 2, {0x00, 0x00}},  // sim
    {'+', 5, {0x08, 0x08, 0x3E, 0x08, 0x08}},  // sim
    {'-', 3, {0x08, 0x08, 0x08}},  // extra
    {'.', 1, {0x40}},  // sim
    {'0', 5, {0x3E, 0x51, 0x49, 0x45, 0x3E}},  // sim
    {'1', 5, {0x00, 0x42, 0x7F, 0x40, 0x00}},  // sim
    {'2', 5, {0x42, 0x61, 0x51, 0x49, 0x46}},  // sim
    {'3', 5, {0x41, 0x49, 0x49, 0x49, 0x36}},  // sim
    {'4', 5, {0x18, 0x14, 0x12, 0x7F, 0x10}},  // sim
    {'5', 5, {0x4F, 0x49, 0x49, 0x49, 0x31}},  // sim
    {'6', 5, {0x3C, 0x4A, 0x49, 0x49, 0x30}},  // sim
    {'7', 5, {0x01, 0x71, 0x09, 0x05, 0x03}},  // sim
    {'8', 5, {0x36, 0x49, 0x49, 0x49, 0x36}},  // sim
    {'9', 5, {0x06, 0x49, 0x49, 0x29, 0x1E}},  // sim
    {':', 1, {0x24}},  // extra
    {'A', 5, {0x7E, 0x09, 0x09, 0x09, 0x7E}},  // sim
    {'B', 5, {0x7F, 0x49, 0x49, 0x49, 0x36}},  // extra
    {'C', 5, {0x3E, 0x41, 0x41, 0x41, 0x22}},  // extra
    {'D', 5, {0x7F, 0x41, 0x41, 0x41, 0x3E}},  // sim
    {'E', 5, {0x7F, 0x49, 0x49, 0x49, 0x41}},  // sim
    {'F', 5, {0x7F, 0x09, 0x09, 0x09, 0x01}},  // sim
    {'G', 5, {0x3E, 0x41, 0x49, 0x49, 0x7A}},  // extra
    {'H', 5, {0x7F, 0x08, 0x08, 0x08, 0x7F}},  // extra
    {'I', 5, {0x00, 0x41, 0x7F, 0x41, 0x00}},  // sim
    {'J', 5, {0x20, 0x40, 0x41, 0x3F, 0x01}},  // extra
    {'K', 5, {0x7F, 0x08, 0x14, 0x22, 0x41}},  // extra
    {'L', 5, {0x7F, 0x40, 0x40, 0x40, 0x40}},  // sim
    {'M', 5, {0x7F, 0x02, 0x0C, 0x02, 0x7F}},  // sim
    {'N', 5, {0x7F, 0x02, 0x04, 0x08, 0x7F}},  // sim
    {'O', 5, {0x3E, 0x41, 0x41, 0x41, 0x3E}},  // sim
    {'P', 5, {0x7F, 0x09, 0x09, 0x09, 0x06}},  // sim
    {'Q', 5, {0x3E, 0x41, 0x51, 0x21, 0x5E}},  // extra
    {'R', 5, {0x7F, 0x09, 0x19, 0x29, 0x46}},  // sim
    {'S', 5, {0x46, 0x49, 0x49, 0x49, 0x31}},  // sim
    {'T', 5, {0x01, 0x01, 0x7F, 0x01, 0x01}},  // sim
    {'U', 5, {0x3F, 0x40, 0x40, 0x40, 0x3F}},  // sim
    {'V', 5, {0x1F, 0x20, 0x40, 0x20, 0x1F}},  // sim
    {'W', 5, {0x7F, 0x20, 0x18, 0x20, 0x7F}},  // sim
    {'X', 5, {0x63, 0x14, 0x08, 0x14, 0x63}},  // extra
    {'Y', 5, {0x03, 0x04, 0x78, 0x04, 0x03}},  // sim
    {'Z', 5, {0x61, 0x51, 0x49, 0x45, 0x43}},  // extra
};

const Glyph& glyph(char c) {
  if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
  for (const Glyph& g : kFont)
    if (g.ch == c) return g;
  return kFont[0];  // unknown characters render as a space, as in the simulator
}

// Same register on every module in the chain. The first word shifted out
// ends up in the module farthest from DIN.
void writeAll(uint8_t reg, const uint8_t data[kModules]) {
  spi.beginTransaction(kSpi);
  digitalWrite(PIN_MATRIX_CS, LOW);
  for (int m = kModules - 1; m >= 0; m--) {
    spi.transfer(reg);
    spi.transfer(data[m]);
  }
  digitalWrite(PIN_MATRIX_CS, HIGH);
  spi.endTransaction();
}

void writeRegister(uint8_t reg, uint8_t value) {
  const uint8_t data[kModules] = {value, value, value, value};
  writeAll(reg, data);
}

void writeControlRegisters() {
  writeRegister(0x0F, 0);  // display test off
  writeRegister(0x09, 0);  // no BCD decode
  writeRegister(0x0B, 7);  // scan all 8 rows
  writeRegister(0x0A, kMatrixIntensity);
  writeRegister(0x0C, 1);  // leave shutdown
}

// data[m] is the module m steps away from DIN.
void flush() {
  for (int row = 0; row < 8; row++) {
    const int srcRow = kMatrixFlipRows ? 7 - row : row;
    uint8_t data[kModules];
    for (int m = 0; m < kModules; m++) {
      const int block = kMatrixReverseModules ? m : kModules - 1 - m;  // screen block, 0 = left
      uint8_t bits = 0;
      for (int c = 0; c < 8; c++) {
        const int x = block * 8 + (kMatrixMirrorColumns ? 7 - c : c);
        if (columns[x] & (1 << srcRow)) bits |= 0x80 >> c;
      }
      data[m] = bits;
    }
    writeAll(row + 1, data);
  }
}
}  // namespace

void displayBegin() {
  pinMode(PIN_MATRIX_CS, OUTPUT);
  digitalWrite(PIN_MATRIX_CS, HIGH);
  spi.begin(PIN_MATRIX_CLK, -1, PIN_MATRIX_DIN, -1);
  // With MISO = -1 the 2.0.17 core still routes FSPI MISO to its default,
  // GPIO13 (LEVEL A), and drops the pull-up. Give the pin back.
  pinMode(PIN_LEVEL_A, INPUT_PULLUP);
  writeControlRegisters();
  memset(columns, 0, sizeof(columns));
  flush();
}

int displayTextWidth(const char* text) {
  int width = -1;
  for (const char* p = text; *p; p++) width += glyph(*p).width + 1;
  return width < 0 ? 0 : width;
}

void displayText(const char* text) {
  memset(columns, 0, sizeof(columns));
  const int spare = kColumns - displayTextWidth(text);
  int x = spare >= 0 ? spare / 2 : -((1 - spare) / 2);  // floor(spare / 2), as in the simulator
  for (const char* p = text; *p; p++) {
    const Glyph& g = glyph(*p);
    for (int i = 0; i < g.width; i++, x++)
      if (x >= 0 && x < kColumns) columns[x] = g.columns[i];
    x++;
  }
  flush();
}

void displayFrequency(float hz) {
  char text[12];
  snprintf(text, sizeof(text), "%.1f", hz);
  displayText(text);
}

void displayTestPattern() {
  // One digit per module, 1 on the left: shows module order and mirroring at once.
  memset(columns, 0, sizeof(columns));
  for (int m = 0; m < kModules; m++) {
    const Glyph& g = glyph('1' + m);
    for (int i = 0; i < g.width; i++) columns[m * 8 + 1 + i] = g.columns[i];
  }
  columns[0] |= 0x01;  // top-left corner pixel
  flush();
}

void displayKeepAlive() {
  writeControlRegisters();
  flush();
}
