#include "screen.h"

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <math.h>
#include <string.h>

#include "service_config.h"
#include "board.h"
#include "lcd.h"

namespace {
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
// Simulator dark palette.
constexpr uint16_t kBg = rgb(0, 0, 0), kPanel = rgb(26, 26, 26), kLine = rgb(54, 54, 54);
constexpr uint16_t kInk = rgb(224, 224, 224), kMuted = rgb(137, 137, 137);
constexpr uint16_t kPink = rgb(242, 140, 189), kTosca = rgb(100, 218, 207), kWarn = rgb(184, 131, 64);
constexpr uint8_t kBright = 0xD0, kDim = 0x30;
constexpr uint32_t kDimAfterMs = 120000;  // AMOLED: dim a static screen

constexpr int kMargin = 20, kGap = 16, kTop = 84, kBottom = 404;
constexpr int kPadW = (kScreenW - 2 * kMargin - kGap) / 2;
constexpr int kPadH = (kBottom - kTop - kGap) / 2;

GFXcanvas16* canvas = nullptr;
char shown[160] = {};
uint32_t lastActivity = 0;
bool dimmed = false;

void padRect(int i, int* x, int* y) {
  *x = kMargin + (i % 2) * (kPadW + kGap);
  *y = kTop + (i / 2) * (kPadH + kGap);
}
void text(const char* s, int x, int y, const GFXfont* f, uint16_t c, bool right = false) {
  canvas->setFont(f); canvas->setTextColor(c);
  if (right) {
    int16_t bx, by; uint16_t w, h;
    canvas->getTextBounds(s, 0, 0, &bx, &by, &w, &h);
    x -= w + bx;
  }
  canvas->setCursor(x, y); canvas->print(s);
}
void chip(const char* s, int right, int y, uint16_t fg, uint16_t bg) {
  int16_t bx, by; uint16_t w, h;
  canvas->setFont(&FreeSans9pt7b);
  canvas->getTextBounds(s, 0, 0, &bx, &by, &w, &h);
  const int cw = w + 24;
  canvas->fillRoundRect(right - cw, y, cw, 30, 15, bg);
  text(s, right - cw + 12 - bx, y + 21, &FreeSans9pt7b, fg);
}
const char* waveLabel(Wave w) { return w == Wave::Saw ? "SAW" : w == Wave::Triangle ? "TRIANGLE" : "SINE"; }
const char* harmLabel(Harmonics h) { return h == Harmonics::Even ? "EVEN" : h == Harmonics::Both ? "ODD+EVEN" : "ODD"; }

void draw(const ControlSnapshot& snap, int touchedPad) {
  const State& s = snap.state;
  canvas->fillScreen(kBg);
  text("CP-100", kMargin, 50, &FreeSansBold18pt7b, kInk);

  const bool service = s.mode == Mode::Service;
  const bool playing = !s.muted;
  int right = kScreenW - kMargin;
  const char* status = playing ? "PLAYING" : "MUTE";
  chip(status, right, 26, playing ? kBg : kInk, playing ? kTosca : kLine);
  int16_t bx, by; uint16_t w, h;
  canvas->setFont(&FreeSans9pt7b); canvas->getTextBounds(status, 0, 0, &bx, &by, &w, &h);
  right -= w + 24 + 8;
  chip(service ? "SERVICE" : "RUN", right, 26, service ? kBg : kInk, service ? kWarn : kLine);

  for (int i = 0; i < kPadCount; ++i) {
    int x, y; padRect(i, &x, &y);
    const auto led = ledState(s, i);
    const uint16_t accent = led.color == LedState::Tosca ? kTosca : kPink;
    const Preset& p = s.presets[i];
    const bool usable = p.used && inRange(p.parameters, s.setup);
    const uint16_t fill = led.active ? accent : kPanel;
    const uint16_t fg = led.active ? kBg : usable ? kInk : kMuted;
    canvas->fillRoundRect(x, y, kPadW, kPadH, 22, fill);
    canvas->drawRoundRect(x, y, kPadW, kPadH, 22, touchedPad == i && !led.active ? kInk : led.active ? accent : kLine);
    char buf[24];
    snprintf(buf, sizeof buf, "PAD %d", i + 1);
    text(buf, x + 18, y + 32, &FreeSans9pt7b, led.active ? kBg : kMuted);
    if (!p.used) {
      text("KOSONG", x + 18, y + 90, &FreeSansBold12pt7b, kMuted);
      continue;
    }
    const float hz = p.parameters.frequencyHz;
    if (hz == floorf(hz)) snprintf(buf, sizeof buf, "%d", int(hz));
    else snprintf(buf, sizeof buf, "%.1f", hz);
    text(buf, x + 18, y + 92, &FreeSansBold18pt7b, fg);
    canvas->setFont(&FreeSansBold18pt7b); canvas->getTextBounds(buf, 0, 0, &bx, &by, &w, &h);
    text("Hz", x + 18 + bx + w + 8, y + 92, &FreeSans9pt7b, fg);
    if (usable) snprintf(buf, sizeof buf, "%s  %s  %d%%", waveLabel(p.parameters.wave), harmLabel(p.parameters.harmonics), p.parameters.levelPct);
    else snprintf(buf, sizeof buf, "DI LUAR SWEEP");
    text(buf, x + 18, y + 128, &FreeSans9pt7b, led.active ? kBg : usable ? kMuted : kWarn);
  }

  canvas->drawFastHLine(kMargin, 420, kScreenW - 2 * kMargin, kLine);
  if (snap.playback == Playback::Gather) text("GATHER berjalan", kMargin, 452, &FreeSans9pt7b, kTosca);
  else if (snap.playback != Playback::Idle) text("SEQUENCE berjalan", kMargin, 452, &FreeSans9pt7b, kTosca);
  else if (snap.owner) text("HP tersambung", kMargin, 452, &FreeSans9pt7b, kWarn);
  else text(kServiceSsid, kMargin, 452, &FreeSans9pt7b, kMuted);
  text("192.168.4.1", kScreenW - kMargin, 452, &FreeSans9pt7b, kMuted, true);
}
}  // namespace

void screenBegin() {
  lcdBegin();
  canvas = new GFXcanvas16(kScreenW, kScreenH);
  canvas->fillScreen(kBg);
  lcdPushRows(canvas->getBuffer(), 0, kScreenH);
  lcdBrightness(kBright);
}

int screenPadAt(int x, int y) {
  for (int i = 0; i < kPadCount; ++i) {
    int px, py; padRect(i, &px, &py);
    if (x >= px - kGap / 2 && x < px + kPadW + kGap / 2 && y >= py - kGap / 2 && y < py + kPadH + kGap / 2) return i;
  }
  return -1;
}

void screenUpdate(const ControlSnapshot& snap, int touchedPad, uint32_t now, bool touched) {
  const State& s = snap.state;
  char sig[sizeof shown];
  int n = snprintf(sig, sizeof sig, "%d%d%d%d%d%d%d%.1f%.1f", int(s.mode), s.muted, s.activePad, touchedPad,
                   int(snap.playback), snap.owner != 0, int(s.setup.ledColor), s.setup.minHz, s.setup.maxHz);
  for (int i = 0; i < kPadCount && n < int(sizeof sig) - 24; ++i) {
    const auto& p = s.presets[i].parameters;
    n += snprintf(sig + n, sizeof sig - n, "|%d%.1f%d%d%d", s.presets[i].used, p.frequencyHz, int(p.wave), int(p.harmonics), p.levelPct);
  }
  const bool changed = strcmp(sig, shown) != 0;
  if (changed || touched) lastActivity = now;
  if (changed) {
    strcpy(shown, sig);
    draw(snap, touchedPad);
    lcdPushRows(canvas->getBuffer(), 0, kScreenH);
  }
  const bool wantDim = now - lastActivity > kDimAfterMs;
  if (wantDim != dimmed) { dimmed = wantDim; lcdBrightness(dimmed ? kDim : kBright); }
}
