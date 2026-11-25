#include "screen.h"

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <math.h>
#include <string.h>

#include "board.h"
#include "lcd.h"
#include "service_config.h"

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

// SERVICE layout.
struct Box { int x, y, w, h; bool has(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; } };
constexpr Box kDone = {340, 16, 120, 44};
constexpr int kTabY = 76, kTabH = 48, kTabW = (kScreenW - 2 * kMargin - 3 * 10) / 4;
constexpr int kFreqY = 140;                       // label + value
constexpr int kStepY = 196, kStepH = 52, kStepW = (kScreenW - 2 * kMargin - 5 * 8) / 6;
constexpr float kSteps[6] = {-10, -1, -0.1f, 0.1f, 1, 10};
constexpr const char* kStepLabels[6] = {"-10", "-1", "-0.1", "+0.1", "+1", "+10"};
constexpr Box kLevelBar = {kMargin, 284, kScreenW - 2 * kMargin, 40};
constexpr int kWaveY = 338, kWaveH = 50, kWaveW = (kScreenW - 2 * kMargin - 2 * 10) / 3;
constexpr Box kTest = {kMargin, 402, 214, 62};
constexpr Box kSave = {kScreenW - kMargin - 214, 402, 214, 62};

GFXcanvas16* canvas = nullptr;
char shown[200] = {};
uint32_t lastActivity = 0;
bool dimmed = false;

Box tabBox(int i) { return {kMargin + i * (kTabW + 10), kTabY, kTabW, kTabH}; }
Box stepBox(int i) { return {kMargin + i * (kStepW + 8), kStepY, kStepW, kStepH}; }
Box waveBox(int i) { return {kMargin + i * (kWaveW + 10), kWaveY, kWaveW, kWaveH}; }
void padRect(int i, int* x, int* y) {
  *x = kMargin + (i % 2) * (kPadW + kGap);
  *y = kTop + (i / 2) * (kPadH + kGap);
}
void measure(const char* s, const GFXfont* f, int16_t* bx, uint16_t* w, uint16_t* h = nullptr) {
  int16_t by; uint16_t hh;
  canvas->setFont(f); canvas->getTextBounds(s, 0, 0, bx, &by, w, &hh);
  if (h) *h = hh;
}
void text(const char* s, int x, int y, const GFXfont* f, uint16_t c, int align = 0) {  // align: 0 left, 1 center, 2 right
  int16_t bx; uint16_t w;
  measure(s, f, &bx, &w);
  if (align == 1) x -= (w / 2) + bx;
  if (align == 2) x -= w + bx;
  canvas->setFont(f); canvas->setTextColor(c);
  canvas->setCursor(x, y); canvas->print(s);
}
// Rounded button with a centered label.
void button(const Box& b, const char* s, bool on, uint16_t accent, const GFXfont* f = &FreeSans9pt7b) {
  canvas->fillRoundRect(b.x, b.y, b.w, b.h, 14, on ? accent : kPanel);
  canvas->drawRoundRect(b.x, b.y, b.w, b.h, 14, on ? accent : kLine);
  int16_t bx; uint16_t w, h;
  measure(s, f, &bx, &w, &h);
  text(s, b.x + b.w / 2, b.y + (b.h + h) / 2 - 1, f, on ? kBg : kInk, 1);
}
void chip(const char* s, int right, int y, uint16_t fg, uint16_t bg, int* width) {
  int16_t bx; uint16_t w;
  measure(s, &FreeSans9pt7b, &bx, &w);
  const int cw = w + 24;
  canvas->fillRoundRect(right - cw, y, cw, 30, 15, bg);
  text(s, right - cw + 12 - bx, y + 21, &FreeSans9pt7b, fg);
  *width = cw;
}
const char* waveLabel(Wave w) { return w == Wave::Saw ? "SAW" : w == Wave::Triangle ? "TRI" : "SINE"; }
const char* harmLabel(Harmonics h) { return h == Harmonics::Even ? "EVEN" : h == Harmonics::Both ? "ODD+EVEN" : "ODD"; }
void formatHz(char* buf, size_t n, float hz, bool decimal) {
  if (decimal || hz != floorf(hz)) snprintf(buf, n, "%.1f", hz);
  else snprintf(buf, n, "%d", int(hz));
}

void drawRun(const ControlSnapshot& snap, const LocalView& v) {
  const State& s = snap.state;
  text("CP-100", kMargin, 50, &FreeSansBold18pt7b, kInk);
  const bool playing = !s.muted;
  int right = kScreenW - kMargin, w;
  chip(playing ? "PLAYING" : "MUTE", right, 26, playing ? kBg : kInk, playing ? kTosca : kLine, &w);
  right -= w + 8;
  const bool service = s.mode == Mode::Service;
  chip(service ? "SERVICE" : "RUN", right, 26, service ? kBg : kInk, service ? kWarn : kLine, &w);

  for (int i = 0; i < kPadCount; ++i) {
    int x, y; padRect(i, &x, &y);
    const auto led = ledState(s, i);
    const uint16_t accent = led.color == LedState::Tosca ? kTosca : kPink;
    const Preset& p = s.presets[i];
    const bool usable = p.used && inRange(p.parameters, s.setup);
    const uint16_t fg = led.active ? kBg : usable ? kInk : kMuted;
    canvas->fillRoundRect(x, y, kPadW, kPadH, 22, led.active ? accent : kPanel);
    canvas->drawRoundRect(x, y, kPadW, kPadH, 22, v.pressed == i && !led.active ? kInk : led.active ? accent : kLine);
    char buf[32];
    snprintf(buf, sizeof buf, "PAD %d", i + 1);
    text(buf, x + 18, y + 32, &FreeSans9pt7b, led.active ? kBg : kMuted);
    if (!p.used) { text("KOSONG", x + 18, y + 90, &FreeSansBold12pt7b, kMuted); continue; }
    formatHz(buf, sizeof buf, p.parameters.frequencyHz, false);
    text(buf, x + 18, y + 92, &FreeSansBold18pt7b, fg);
    int16_t bx; uint16_t tw;
    measure(buf, &FreeSansBold18pt7b, &bx, &tw);
    text("Hz", x + 18 + bx + tw + 8, y + 92, &FreeSans9pt7b, fg);
    if (usable) snprintf(buf, sizeof buf, "%s  %s  %d%%", waveLabel(p.parameters.wave), harmLabel(p.parameters.harmonics), p.parameters.levelPct);
    else snprintf(buf, sizeof buf, "DI LUAR SWEEP");
    text(buf, x + 18, y + 128, &FreeSans9pt7b, led.active ? kBg : usable ? kMuted : kWarn);
  }

  canvas->drawFastHLine(kMargin, 420, kScreenW - 2 * kMargin, kLine);
  if (snap.playback == Playback::Gather) text("GATHER berjalan", kMargin, 452, &FreeSans9pt7b, kTosca);
  else if (snap.playback != Playback::Idle) text("SEQUENCE berjalan", kMargin, 452, &FreeSans9pt7b, kTosca);
  else if (snap.owner && snap.owner != kLocalOwner) text("HP tersambung", kMargin, 452, &FreeSans9pt7b, kWarn);
  else text("Tahan CP-100 3 dtk: servis", kMargin, 452, &FreeSans9pt7b, kMuted);
  text(kServiceSsid, kScreenW - kMargin, 452, &FreeSans9pt7b, kMuted, 2);
}

void drawService(const ControlSnapshot& snap, const LocalView& v) {
  const State& s = snap.state;
  const Parameters& p = s.parameters;
  text(v.toast ? v.toast : "SERVICE", kMargin, 48, v.toast ? &FreeSansBold12pt7b : &FreeSansBold18pt7b, v.toast ? kTosca : kWarn);
  button(kDone, "Selesai", false, kInk, &FreeSansBold12pt7b);

  char buf[32];
  for (int i = 0; i < kPadCount; ++i) {
    snprintf(buf, sizeof buf, "PAD %d%s", i + 1, s.presets[i].used ? "" : " -");
    button(tabBox(i), buf, v.pad == i, kInk);
  }

  text("FREQ", kMargin, kFreqY + 34, &FreeSans9pt7b, kMuted);
  formatHz(buf, sizeof buf, p.frequencyHz, true);
  strcat(buf, " Hz");
  text(buf, kScreenW / 2 + 20, kFreqY + 42, &FreeSansBold18pt7b, kInk, 1);
  for (int i = 0; i < 6; ++i) button(stepBox(i), kStepLabels[i], false, kInk);

  const Box& b = kLevelBar;
  snprintf(buf, sizeof buf, "LEVEL %d%%", p.levelPct);
  text(buf, b.x, b.y - 8, &FreeSans9pt7b, kMuted);
  canvas->fillRoundRect(b.x, b.y + 8, b.w, b.h - 16, 12, kPanel);
  canvas->drawRoundRect(b.x, b.y + 8, b.w, b.h - 16, 12, kLine);
  const int fillW = (b.w - 4) * p.levelPct / 100;
  if (fillW > 0) canvas->fillRoundRect(b.x + 2, b.y + 10, fillW, b.h - 20, 10, kTosca);

  const Harmonics harms[3] = {Harmonics::Odd, Harmonics::Even, Harmonics::Both};
  for (int i = 0; i < 3; ++i) button(waveBox(i), harmLabel(harms[i]), p.harmonics == harms[i], kInk);

  button(kTest, v.testing ? "BUNYI..." : "TAHAN TEST", v.testing, kTosca, &FreeSansBold12pt7b);
  snprintf(buf, sizeof buf, "SIMPAN PAD %d", v.pad + 1);
  button(kSave, buf, false, kPink, &FreeSansBold12pt7b);
}
}  // namespace

void screenBegin() {
  lcdBegin();
  canvas = new GFXcanvas16(kScreenW, kScreenH);
  canvas->fillScreen(kBg);
  lcdPushRows(canvas->getBuffer(), 0, kScreenH);
  lcdBrightness(kBright);
}

HitResult screenHit(int x, int y, bool service) {
  HitResult r;
  if (!service) {
    if (y < 74 && x < 240) { r.hit = Hit::Title; return r; }
    for (int i = 0; i < kPadCount; ++i) {
      int px, py; padRect(i, &px, &py);
      if (x >= px - kGap / 2 && x < px + kPadW + kGap / 2 && y >= py - kGap / 2 && y < py + kPadH + kGap / 2) {
        r.hit = Hit::Tab; r.index = i; return r;  // RUN: Tab = pad
      }
    }
    return r;
  }
  if (kDone.has(x, y)) { r.hit = Hit::Done; return r; }
  for (int i = 0; i < kPadCount; ++i) if (tabBox(i).has(x, y)) { r.hit = Hit::Tab; r.index = i; return r; }
  for (int i = 0; i < 6; ++i) if (stepBox(i).has(x, y)) { r.hit = Hit::Freq; r.index = i; r.value = kSteps[i]; return r; }
  if (kLevelBar.has(x, y)) {
    r.hit = Hit::LevelBar;
    r.value = constrain(roundf(100.f * (x - kLevelBar.x - 2) / (kLevelBar.w - 4)), 0, 100);
    return r;
  }
  for (int i = 0; i < 3; ++i) if (waveBox(i).has(x, y)) { r.hit = Hit::Harmonics; r.index = i; return r; }
  if (kTest.has(x, y)) { r.hit = Hit::Test; return r; }
  if (kSave.has(x, y)) { r.hit = Hit::Save; return r; }
  return r;
}

void screenUpdate(const ControlSnapshot& snap, const LocalView& v, uint32_t now, bool touched) {
  const State& s = snap.state;
  const auto& e = s.parameters;
  char sig[sizeof shown];
  int n = snprintf(sig, sizeof sig, "%d%d%d%d%d%d%d%d%p|%d%d%.1f%.1f|%.1f%d%d%d", v.service, v.pad, v.testing, v.pressed,
                   int(s.mode), s.muted, s.activePad, int(snap.playback), v.toast, snap.owner != 0, snap.owner == kLocalOwner,
                   s.setup.minHz, s.setup.maxHz, e.frequencyHz, e.levelPct, int(e.wave), int(e.harmonics));
  for (int i = 0; i < kPadCount && n < int(sizeof sig) - 24; ++i) {
    const auto& p = s.presets[i].parameters;
    n += snprintf(sig + n, sizeof sig - n, "|%d%.1f%d%d%d", s.presets[i].used, p.frequencyHz, int(p.wave), int(p.harmonics), p.levelPct);
  }
  const bool changed = strcmp(sig, shown) != 0;
  if (changed || touched) lastActivity = now;
  if (changed) {
    strcpy(shown, sig);
    canvas->fillScreen(kBg);
    if (v.service) drawService(snap, v); else drawRun(snap, v);
    lcdPushRows(canvas->getBuffer(), 0, kScreenH);
  }
  const bool wantDim = now - lastActivity > kDimAfterMs;
  if (wantDim != dimmed) { dimmed = wantDim; lcdBrightness(dimmed ? kDim : kBright); }
}
