// End-to-end scenarios for the CP-100 firmware running on the simulated board.
// Usage: cp100sim <scenario>   (see kScenarios at the bottom; "list" prints them)
// Each check prints one line:  CHECK PASS|FAIL <scenario>/<check>: <numbers>
// Every scenario ends with an "invariants" check (bus protocols, NVS-write
// silence, no calibrationFor() at runtime, no harness warnings).

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <functional>
#include <map>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <Arduino.h>
#include <Preferences.h>
#include <driver/i2s.h>

#include "analysis.h"
#include "audio.h"
#include "dsp.h"
#include "leds.h"
#include "model.h"
#include "pins.h"
#include "ref/font.h"
#include "ref/reference.h"
#include "sim.h"

void setup();
void loop();

namespace {

using sim::now_us;

const char* gScenario = "";
int gFailures = 0;
bool gI2sHaltExpected = false;      // scenario halts the I2S on purpose
int gHarnessCalibrationCalls = 0;   // calibrationFor() calls made by the harness itself

void check(bool ok, const char* name, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
void check(bool ok, const char* name, const char* fmt, ...) {
  char buf[4096];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  for (char* p = buf; *p; p++)
    if (*p == '\n' || *p == '\r') *p = '|';
  printf("CHECK %s %s/%s: %s\n", ok ? "PASS" : "FAIL", gScenario, name, buf);
  fflush(stdout);
  if (!ok) gFailures++;
}

void info(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void info(const char* fmt, ...) {
  char buf[4096];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  printf("INFO %s: %s\n", gScenario, buf);
}

// ------------------------------------------------------------------ time, loop() and display watch
double nowMs() { return now_us() / 1000.0; }

uint64_t gLoopMaxUs = 0;
double gLoopMaxAtMs = 0;
void resetLoopMax() { gLoopMaxUs = 0, gLoopMaxAtMs = 0; }

struct Shown {
  double t;
  std::string text;
};
std::vector<Shown>* gWatch = nullptr;
sim::Picture gWatchLast{};

void loopOnce() {
  const uint64_t t = now_us();
  loop();
  const uint64_t d = now_us() - t;
  if (d > gLoopMaxUs) gLoopMaxUs = d, gLoopMaxAtMs = t / 1000.0;
  if (gWatch) {
    const sim::Picture fb = sim::framebuffer();
    if (fb != gWatchLast) gWatch->push_back({nowMs(), sim::decode_text(fb)}), gWatchLast = fb;
  }
}

void runMs(double ms) {
  const uint64_t end = now_us() + uint64_t(ms * 1000);
  while (now_us() < end) loopOnce();
}

// Runs loop() until pred() holds; returns elapsed ms or -1 after maxMs.
double runUntil(const std::function<bool()>& pred, double maxMs) {
  const uint64_t start = now_us(), end = start + uint64_t(maxMs * 1000);
  while (now_us() < end) {
    if (pred()) return (now_us() - start) / 1000.0;
    loopOnce();
  }
  return pred() ? (now_us() - start) / 1000.0 : -1;
}

// Records every picture change (decoded) while alive.
struct DisplayWatch {
  std::vector<Shown> ev;
  DisplayWatch() {
    gWatchLast = sim::framebuffer();
    ev.push_back({nowMs(), sim::decode_text(gWatchLast)});
    gWatch = &ev;
  }
  ~DisplayWatch() { gWatch = nullptr; }
  // First time at or after `after` that `text` appears; -1 if never.
  double firstAt(const std::string& text, double after = -1e18) const {
    for (const Shown& s : ev)
      if (s.t >= after && s.text == text) return s.t;
    return -1;
  }
  // How long `text` stayed after its first appearance at or after `after`.
  double shownFor(const std::string& text, double after = -1e18) const {
    for (size_t i = 0; i < ev.size(); i++)
      if (ev[i].t >= after && ev[i].text == text) return (i + 1 < ev.size() ? ev[i + 1].t : nowMs()) - ev[i].t;
    return -1;
  }
  std::string trace(double t0) const {
    std::string s;
    char b[96];
    for (const Shown& x : ev) {
      snprintf(b, sizeof(b), " '%s'@%+.0f", x.text.c_str(), x.t - t0);
      s += b;
    }
    return s;
  }
};

void boot() {
  setup();
  loopOnce();
}

// ------------------------------------------------------------------ console
std::string cmd(const std::string& line, double settleMs = 3) {
  const size_t mark = sim::serial_log().size();
  sim::serial_send(line + "\n");
  runUntil([] { return sim::serial_rx_pending() == 0; }, 2000);
  runMs(settleMs);
  return sim::serial_log().substr(mark);
}
std::string firstLine(const std::string& s) { return s.substr(0, s.find_first_of("\r\n")); }
// The command's own reply is its last line; log lines (e.g. "preset saved to pad 6") may come first.
std::string lastLine(const std::string& s) {
  size_t end = s.find_last_not_of("\r\n");
  if (end == std::string::npos) return "";
  const size_t start = s.find_last_of("\r\n", end);
  return s.substr(start == std::string::npos ? 0 : start + 1, end - (start == std::string::npos ? 0 : start + 1) + 1);
}
bool contains(const std::string& s, const std::string& what) { return s.find(what) != std::string::npos; }
std::string logSince(size_t mark) { return sim::serial_log().substr(mark); }

struct PadInfo {
  bool used = false;
  double hz = 0;
  int level = 0, fold = 0;
  std::string wave, harm;
};
struct Status {
  bool ok = false;
  std::string mode;
  bool muted = false, armed = false;
  int pad = -1;
  double hz = 0;
  int level = 0, fold = 0;
  std::string wave, harm;
  double sweepMin = 0, sweepMax = 0;
  bool sweepSet = false;
  std::string led;
  bool pulse = false;
  std::string step;
  bool audioOk = false;
  double peak = -1, cpu = -1;
  long stalls = -1;
  bool panel = false;
  std::string panelMode, panelWave, panelHarm;
  PadInfo pads[8];
  std::string raw;
};

bool parseParams(const std::string& s, double* hz, int* level, std::string* wave, int* fold, std::string* harm) {
  static const std::regex re(R"(([0-9.]+) Hz, level (-?\d+)%, (\w+), fold (-?\d+)%, (\w+))");
  std::smatch m;
  if (!std::regex_search(s, m, re)) return false;
  *hz = atof(m[1].str().c_str());
  *level = atoi(m[2].str().c_str());
  *wave = m[3];
  *fold = atoi(m[4].str().c_str());
  *harm = m[5];
  return true;
}

Status status() {
  Status st;
  st.raw = cmd("status", 2);
  std::smatch m;
  static const std::regex head(R"(mode (RUN|SERVICE)(, muted)?(, SAVE armed)?, pad (\d+), ([^\r\n]*))");
  if (!std::regex_search(st.raw, m, head)) return st;
  st.mode = m[1];
  st.muted = m[2].matched;
  st.armed = m[3].matched;
  st.pad = atoi(m[4].str().c_str());
  if (!parseParams(m[5], &st.hz, &st.level, &st.wave, &st.fold, &st.harm)) return st;
  static const std::regex sw(R"(sweep ([0-9.]+)-([0-9.]+) Hz( \(set\))?, LED (\w+)( \+ pulse)?, FREQ step ([0-9.]+) Hz)");
  if (!std::regex_search(st.raw, m, sw)) return st;
  st.sweepMin = atof(m[1].str().c_str());
  st.sweepMax = atof(m[2].str().c_str());
  st.sweepSet = m[3].matched;
  st.led = m[4];
  st.pulse = m[5].matched;
  st.step = m[6];
  static const std::regex au(R"(audio: peak ([0-9.]+), CPU ([0-9]+)% of real time, I2S stalls ([0-9]+))");
  if (!std::regex_search(st.raw, m, au)) return st;
  st.audioOk = true;
  st.peak = atof(m[1].str().c_str());
  st.cpu = atof(m[2].str().c_str());
  st.stalls = atol(m[3].str().c_str());
  static const std::regex pa(R"(panel: MODE (RUN|SERVICE), WAVE (\w+), HARMONICS (\w+))");
  if (std::regex_search(st.raw, m, pa)) st.panel = true, st.panelMode = m[1], st.panelWave = m[2], st.panelHarm = m[3];
  else if (!contains(st.raw, "panel: MCP23017 not found")) return st;
  for (int i = 0; i < 8; i++) {
    const std::string key = "  pad " + std::to_string(i + 1) + ": ";
    const size_t at = st.raw.find(key);
    if (at == std::string::npos) return st;
    const std::string line = st.raw.substr(at + key.size(), st.raw.find('\n', at) - at - key.size());
    PadInfo& p = st.pads[i];
    p.used = line.rfind("empty", 0) != 0;
    if (p.used && !parseParams(line, &p.hz, &p.level, &p.wave, &p.fold, &p.harm)) return st;
  }
  st.ok = true;
  return st;
}

// ------------------------------------------------------------------ contacts
int padLine(int pad) { return PIN_PADS[pad - 1]; }
constexpr int MCP_MODE = sim::kMcpLine + 0, MCP_SINE = sim::kMcpLine + 1, MCP_TRI = sim::kMcpLine + 2,
              MCP_ODD = sim::kMcpLine + 3, MCP_FOLD_A = sim::kMcpLine + 4, MCP_FOLD_B = sim::kMcpLine + 5,
              MCP_BOTH = sim::kMcpLine + 6;

// Closes (level 0) or opens (1) a contact, optionally with ~1.5 ms of bounce.
void setContact(int line, int level, bool bounce = false, uint64_t delayUs = 50) {
  const uint64_t t = now_us() + delayUs;
  if (bounce) {
    sim::schedule_line(t, line, level);
    sim::schedule_line(t + 200, line, !level);
    sim::schedule_line(t + 450, line, level);
    sim::schedule_line(t + 900, line, !level);
    sim::schedule_line(t + 1500, line, level);
  } else {
    sim::schedule_line(t, line, level);
  }
}
void press(int line, bool bounce = false) { setContact(line, 0, bounce); }
void release(int line, bool bounce = false) { setContact(line, 1, bounce); }
void tapButton(int line, double holdMs = 80, bool bounce = true) {
  press(line, bounce);
  runMs(holdMs);
  release(line, bounce);
  runMs(30);
}

// Schedules an encoder turn: one full quadrature cycle per detent. detents > 0
// is the direction the firmware counts as +1 (A falls first: 11 -> 01 -> 00 -> 10 -> 11).
uint64_t scheduleTurn(int lineA, int lineB, int detents, double msPerDetent, bool bounce = false, uint64_t startUs = 0) {
  static const int cw[4][2] = {{0, 1}, {0, 0}, {1, 0}, {1, 1}};
  static const int ccw[4][2] = {{1, 0}, {0, 0}, {0, 1}, {1, 1}};
  const int(*seq)[2] = detents > 0 ? cw : ccw;
  const uint64_t t = (startUs ? startUs : now_us()) + 100;
  int a = 1;
  uint64_t last = t;
  for (int d = 0; d < abs(detents); d++) {
    for (int q = 0; q < 4; q++) {
      const uint64_t at = t + uint64_t((d * 4 + q) * msPerDetent * 250);
      const int na = seq[q][0], nb = seq[q][1];
      const int line = na != a ? lineA : lineB, level = na != a ? na : nb;
      sim::schedule_line(at, line, level);
      if (bounce) {
        sim::schedule_line(at + 120, line, !level);
        sim::schedule_line(at + 260, line, level);
      }
      a = na;
      last = std::max(last, at + (bounce ? 260 : 0));
    }
  }
  return last;
}
void turn(int lineA, int lineB, int detents, double msPerDetent, bool bounce = false) {
  const uint64_t last = scheduleTurn(lineA, lineB, detents, msPerDetent, bounce);
  runMs((last - now_us()) / 1000.0 + 5);
}

// ------------------------------------------------------------------ audio
std::vector<float> audioMs(double fromMs, double durMs) {
  const size_t a = sim::frame_at(uint64_t(fromMs * 1000)), b = sim::frame_at(uint64_t((fromMs + durMs) * 1000));
  return sim::audio_segment(a, b - a);
}
double freqOf(const std::vector<float>& x, double fmin = 15, double fmax = 2200) {
  return ana::fundamental(x, sim::kFs, fmin, fmax);
}
// First time (ms, absolute) at or after fromMs when |x| exceeds thr.
double onsetMs(double fromMs, double thr, double maxMs = 500) {
  const auto x = audioMs(fromMs, maxMs);
  for (size_t i = 0; i < x.size(); i++)
    if (fabsf(x[i]) > thr) return fromMs + i * 1000.0 / sim::kFs;
  return -1;
}
// Last time in [fromMs, fromMs+spanMs) when |x| exceeds thr (fromMs if never).
double lastAboveMs(double fromMs, double spanMs, double thr) {
  const auto x = audioMs(fromMs, spanMs);
  for (size_t i = x.size(); i-- > 0;)
    if (fabsf(x[i]) > thr) return fromMs + (i + 1) * 1000.0 / sim::kFs;
  return fromMs;
}
size_t nonZero(double fromMs, double durMs) {
  if (fromMs < 0) durMs += fromMs, fromMs = 0;  // windows that start before boot
  if (durMs <= 0) return 0;
  const size_t a = sim::frame_at(uint64_t(fromMs * 1000)), b = sim::frame_at(uint64_t((fromMs + durMs) * 1000));
  size_t n = 0;
  for (size_t i = a; i < b && i < sim::audio_left().size(); i++) n += sim::audio_left()[i] != 0 || sim::audio_right()[i] != 0;
  return n;
}
bool leftEqualsRight() {
  const auto &l = sim::audio_left(), &r = sim::audio_right();
  for (size_t i = 0; i < l.size(); i++)
    if (l[i] != r[i]) return false;
  return true;
}
double maxStepIn(double fromMs, double durMs) { return ana::maxStep(audioMs(fromMs, durMs)); }
double rmsLast(double ms) { return ana::rms(audioMs(nowMs() - ms, ms)); }
double freqLast(double ms) { return freqOf(audioMs(nowMs() - ms, ms)); }
bool playing(double hz, double ms = 200) { return rmsLast(ms) > 0.01 && fabs(freqLast(ms) - hz) < 0.1; }
// Releases audible within maxMs of t (absolute ms): nothing above -60 dBFS after t + maxMs.
double silentAfter(double t, double spanMs = 300) { return lastAboveMs(t, spanMs, 1e-3) - t; }

constexpr double kDbfs60 = 1e-3;
const double kExpectedRms035 = 0.36 * 0.35 * kOutputTrim;
// quiesceAudio() waits for a block peak < 1e-3 (before kOutputTrim) and then
// for the 4 queued buffers to drain: every looping DMA buffer must be below that.
const double kNvsRingMax = 1e-4;  // -80 dBFS at the DAC

// ------------------------------------------------------------------ LEDs
struct Rgb {
  int r, g, b;
};
Rgb scaled(int r, int g, int b, float s = kLedBrightness) {
  return {int(uint8_t(r * s + .5f)), int(uint8_t(g * s + .5f)), int(uint8_t(b * s + .5f))};
}
const Rgb kWhite = scaled(255, 255, 255), kPink = scaled(242, 140, 189), kTosca = scaled(100, 218, 207);
bool same(const uint8_t* c, Rgb e) { return c[0] == e.r && c[1] == e.g && c[2] == e.b; }
std::string rgbStr(const uint8_t* c) {
  char b[32];
  snprintf(b, sizeof(b), "(%d,%d,%d)", c[0], c[1], c[2]);
  return b;
}
const sim::LedFrame* lastLeds() { return sim::led_frames().empty() ? nullptr : &sim::led_frames().back(); }
// LED pattern check: led `active` (0-based, -1 none) has colour `on`, the rest white.
bool ledsAre(int active, Rgb on, std::string* detail) {
  const sim::LedFrame* f = lastLeds();
  if (!f) {
    *detail = "no LED frame written";
    return false;
  }
  bool ok = f->count == 8;
  std::string d;
  for (int i = 0; i < 8; i++) {
    const bool good = same(f->rgb[i], i == active ? on : kWhite);
    ok = ok && good;
    d += rgbStr(f->rgb[i]);
  }
  *detail = d;
  return ok;
}

// ------------------------------------------------------------------ display
double waitDisplay(const std::string& text, double maxMs) {
  return runUntil([&] { return sim::display_is(text); }, maxMs);
}
std::string shown() { return sim::display_text(); }
const AppRender* appRender(const std::string& text, bool frequency) {
  for (const AppRender& r : kAppRenders)
    if (r.text == text && r.frequency == frequency) return &r;
  return nullptr;
}
// The picture app.js drawMatrix() paints (when the text only uses app.js
// glyphs), otherwise the harness renderer with its extra glyphs.
sim::Picture expectedPicture(const std::string& text, bool frequency = false) {
  if (const AppRender* r = appRender(text, frequency))
    if (r->allGlyphs) {
      sim::Picture p{};
      for (int i = 0; i < 32; i++) p[i] = r->cols[i];
      return p;
    }
  return sim::render_text(text);
}
std::string freqText(double hz) {
  char b[16];
  snprintf(b, sizeof(b), "%.1f", hz);
  return b;
}

// ------------------------------------------------------------------ panel
void slideWave(const char* pos) {  // "sine" "saw" "triangle" "fault"
  const bool s = !strcmp(pos, "sine") || !strcmp(pos, "fault"), t = !strcmp(pos, "triangle") || !strcmp(pos, "fault");
  setContact(MCP_SINE, s ? 0 : 1, true);
  setContact(MCP_TRI, t ? 0 : 1, true);
  runMs(60);
}
void slideHarm(const char* pos) {  // "odd" "even" "both" "fault"
  const bool o = !strcmp(pos, "odd") || !strcmp(pos, "fault"), b = !strcmp(pos, "both") || !strcmp(pos, "fault");
  setContact(MCP_ODD, o ? 0 : 1, true);
  setContact(MCP_BOTH, b ? 0 : 1, true);
  runMs(60);
}
void slideMode(bool service) {
  setContact(MCP_MODE, service ? 0 : 1, true);
  runMs(80);
}
void attachPanel(bool service) {
  sim::mcp_attach(true);
  sim::set_line(MCP_MODE, service ? 0 : 1);
  sim::set_line(MCP_SINE, 0);  // SINE
  sim::set_line(MCP_ODD, 0);   // ODD
}

// Holds FREQ push until the sweep editor opens, then lets go.
void openSweepEditor() {
  press(PIN_FREQ_PUSH, true);
  runUntil([] { return sim::display_is("MIN"); }, 2600);
  release(PIN_FREQ_PUSH, true);
  runMs(40);
}

// ================================================================== scenarios

void scenarioBoot() {
  boot();
  const std::string log = sim::serial_log();
  check(contains(log, "CP-100 console") && contains(log, "factory") && contains(log, "sweep MIN MAX"), "help_banner",
        "%zu bytes of help on Serial at boot", log.size());
  check(sim::loop_wdt_enabled(), "loop_watchdog", "loopTask added to the 5 s task watchdog: %d", sim::loop_wdt_enabled());
  check(sim::serial_tx_buffer() == 1024 && sim::serial_tx_blocked_us() == 0, "serial_tx_buffer",
        "UART TX ring buffer %zu bytes (set before Serial.begin), loop blocked %.1f ms printing the help",
        sim::serial_tx_buffer(), sim::serial_tx_blocked_us() / 1000.0);

  // pins
  bool pullups = true;
  std::string bad;
  for (int p : PIN_PADS)
    if (sim::pin_mode(p) != INPUT_PULLUP) pullups = false, bad += " " + std::to_string(p);
  for (int p : {PIN_SAVE, PIN_MUTE, PIN_FREQ_PUSH, PIN_FREQ_A, PIN_FREQ_B, PIN_LEVEL_A, PIN_LEVEL_B})
    if (sim::pin_mode(p) != INPUT_PULLUP) pullups = false, bad += " " + std::to_string(p);
  check(pullups, "input_pullups", "pads, SAVE, MUTE, FREQ A/B/push, LEVEL A/B all INPUT_PULLUP%s", bad.c_str());
  check(sim::pin_mode(PIN_MATRIX_CS) == OUTPUT && sim::pin_output(PIN_MATRIX_CS) == 1, "matrix_cs_idle_high",
        "GPIO41 mode %d level %d", sim::pin_mode(PIN_MATRIX_CS), sim::pin_output(PIN_MATRIX_CS));

  // I2S + task
  const sim::I2sInfo& i = sim::i2s();
  const bool i2sOk = i.installed && i.port == 0 && i.sampleRate == 48000 && i.bitsPerSample == 32 && i.bitsPerChan == 32 &&
                     i.channelFormat == I2S_CHANNEL_FMT_RIGHT_LEFT && i.commFormat == I2S_COMM_FORMAT_STAND_I2S &&
                     i.mode == (I2S_MODE_MASTER | I2S_MODE_TX) && i.dmaCount == 4 && i.dmaLen == 128 && i.autoClear &&
                     i.bck == PIN_I2S_BCK && i.ws == PIN_I2S_LRCK && i.dout == PIN_I2S_DOUT && i.mck == -1 && i.din == -1;
  check(i2sOk, "i2s_config",
        "port %d, %u Hz, %d/%d bit, fmt %d, comm %d (Philips, PCM5102 FMT=GND), DMA %dx%d, auto-clear %d, BCK %d WS %d DOUT %d MCK %d",
        i.port, i.sampleRate, i.bitsPerSample, i.bitsPerChan, i.channelFormat, i.commFormat, i.dmaCount, i.dmaLen,
        i.autoClear, i.bck, i.ws, i.dout, i.mck);
  check(i.task == "audio" && i.taskCore == 0 && i.taskPriority == configMAX_PRIORITIES - 5 && i.taskStack >= 4096,
        "audio_task", "task '%s' core %d (expected 0) priority %u (expected configMAX_PRIORITIES-5 = %d) stack %u bytes (>= 4096)",
        i.task.c_str(), i.taskCore, i.taskPriority, configMAX_PRIORITIES - 5, i.taskStack);

  // SPI + MAX7219 configuration
  const sim::SpiInfo& s = sim::spi();
  check(s.begun && s.sck == PIN_MATRIX_CLK && s.mosi == PIN_MATRIX_DIN && s.clock <= 10000000 && s.mode == 0 &&
            s.bitOrder == MSBFIRST && s.badFrames == 0 && s.transfersWithCsHigh == 0 && s.transfersOutsideTransaction == 0,
        "spi_max7219_bus", "SCK %d MOSI %d, %u Hz mode %d, %d frames, %d bad, %d with CS high", s.sck, s.mosi, s.clock,
        s.mode, s.frames, s.badFrames, s.transfersWithCsHigh);
  bool cfg = true;
  for (const auto& m : sim::max_chain())
    cfg = cfg && m.decode == 0 && m.scanLimit == 7 && m.shutdownReg == 1 && m.test == 0 && m.intensity == 4;
  check(cfg, "max7219_registers", "all 4 modules: decode 0, scan limit 7, shutdown off, test off, intensity 4");

  const sim::I2cInfo& w = sim::i2c();
  check(w.begun && w.sda == PIN_I2C_SDA && w.scl == PIN_I2C_SCL && w.freq == 400000, "i2c_bus", "SDA %d SCL %d %u Hz timeout %u ms",
        w.sda, w.scl, w.freq, w.timeoutMs);

  // display: CP100 (app.js font + the extra C) for 1500 ms, then 128.5
  const bool splash = sim::framebuffer() == expectedPicture("CP100");
  const double t0 = nowMs();
  DisplayWatch dw;
  runMs(2200);
  const double tFreq = dw.firstAt("128.5");
  check(splash && dw.ev.size() == 2 && tFreq - t0 >= 1495 && tFreq - t0 <= 1760 &&
            sim::framebuffer() == expectedPicture("128.5", true),
        "display_splash", "CP100 centred like app.js: %d; then 128.5 (app.js render) after %.0f ms (1500 + up to 250 ms refresh);%s",
        splash, tFreq - t0, dw.trace(t0).c_str());

  // audio silent
  const size_t frames = sim::audio_left().size();
  check(frames > 48000 && nonZero(0, nowMs()) == 0, "audio_silent", "%zu frames captured, %zu non-zero", frames,
        nonZero(0, nowMs()));
  check(i.minTicks == 100 && i.maxTicks == 100 && i.timeouts == 0 && i.badWrites == 0, "i2s_write_timeout",
        "%d i2s_write calls, ticks_to_wait %u..%u (expected pdMS_TO_TICKS(100) = 100), %d timed out", i.writes, i.minTicks,
        i.maxTicks, i.timeouts);
  const Status st = status();
  check(st.ok && st.audioOk && st.peak == 0 && st.cpu == 0 && st.stalls == 0, "status_audio_line",
        "'%s' (CPU reads 0%% in the simulation by design)",
        firstLine(st.raw.substr(std::min(st.raw.size(), st.raw.find("audio:")))).c_str());

  // LEDs
  std::string d;
  const bool white = ledsAre(-1, kWhite, &d);
  const sim::RmtInfo& r = sim::rmt();
  check(white, "leds_white_35pct", "expected (%d,%d,%d) x8, got %s", kWhite.r, kWhite.g, kWhite.b, d.c_str());
  check(r.pin == PIN_RGB && r.tx && r.tickNs == 100 && r.timingErrors == 0 && r.levelErrors == 0 && r.lengthErrors == 0 &&
            r.resetErrors == 0,
        "ws2812_timing", "pin %d tick %.1f ns, T0H %.0f T0L %.0f T1H %.0f T1L %.0f ns, %d writes, %d timing/%d level/%d reset errors",
        r.pin, r.tickNs, r.minT0H, r.minT0L, r.minT1H, r.minT1L, r.writes, r.timingErrors, r.levelErrors, r.resetErrors);
}

// Hold pad 1 and release it.
void scenarioPad() {
  boot();
  runMs(1800);
  const double tPress = nowMs();
  press(padLine(1), true);
  runMs(700);
  const double on = onsetMs(tPress, kDbfs60);
  check(on > 0 && on - tPress <= 20, "onset_latency",
        "first sample above -60 dBFS %.1f ms after the contact closed (5 ms debounce + up to 13.3 ms DMA queue)",
        on - tPress);
  const auto steady = audioMs(tPress + 300, 400);
  const double f = freqOf(steady), rms = ana::rms(steady);
  check(fabs(f - 128.5) < 0.05, "frequency", "%.3f Hz (expected 128.5)", f);
  check(fabs(rms / kExpectedRms035 - 1) < 0.02, "rms", "%.5f FS = %.2f dBFS (expected 0.36*0.35*%.2f = %.5f)", rms,
        ana::db(rms), kOutputTrim, kExpectedRms035);
  check(leftEqualsRight(), "left_equals_right", "%zu frames compared", sim::audio_left().size());
  std::string d;
  { const bool ledOk = ledsAre(0, kPink, &d); check(ledOk, "led1_pink", "%s", d.c_str()); }
  check(sim::display_is("128.5"), "display_while_playing", "'%s'", shown().c_str());

  const double tRel = nowMs();
  release(padLine(1), true);
  runMs(500);
  const double gone = lastAboveMs(tRel, 400, kDbfs60);
  // decay start: first frame after release whose envelope begins to fall
  const double decayStart = lastAboveMs(tRel, 400, 0.9 * ana::peak(steady));
  check(gone - tRel <= 70, "release_below_-60dBFS",
        "%.1f ms after the contact opened, %.1f ms after the fade started (10 ms mute ramp needs ~45 ms for 39 dB)",
        gone - tRel, gone - decayStart);
  const size_t nz = nonZero(tRel + 250, 250);
  check(nz == 0, "release_exact_zero", "%zu non-zero frames 250-500 ms after release", nz);
  { const bool ledOk = ledsAre(-1, kWhite, &d); check(ledOk, "led_white_after_release", "%s", d.c_str()); }
}

void scenarioPriority() {
  boot();
  runMs(1700);
  const double t0 = nowMs();
  press(padLine(2), true);
  runMs(100);
  press(padLine(3), true);
  runMs(500);
  auto seg = audioMs(t0 + 300, 300);
  double f = freqOf(seg);
  const double a196 = ana::amplitudeAt(seg, 196, 48000), a256 = ana::amplitudeAt(seg, 256, 48000);
  check(fabs(f - 196) < 0.05 && ana::db(a256 / a196) < -60, "first_pad_wins",
        "%.3f Hz, 256 Hz component %.1f dB below 196 Hz", f, ana::db(a196 / std::max(a256, 1e-12)));
  std::string d;
  { const bool ledOk = ledsAre(1, kTosca, &d); check(ledOk, "led2_tosca", "%s", d.c_str()); }
  const double rmsBefore = ana::rms(seg);
  release(padLine(3), true);
  runMs(300);
  seg = audioMs(nowMs() - 250, 250);
  f = freqOf(seg);
  check(fabs(f - 196) < 0.05 && fabs(ana::rms(seg) / rmsBefore - 1) < 0.01, "release_second_pad_no_change",
        "%.3f Hz, rms %.5f vs %.5f", f, ana::rms(seg), rmsBefore);
  press(padLine(3), true);  // held again: still ignored
  runMs(100);
  const double tRel = nowMs();
  release(padLine(2), true);
  runMs(400);
  const double gone = lastAboveMs(tRel, 400, kDbfs60);
  check(gone - tRel < 80 && nonZero(tRel + 300, 100) == 0, "release_first_pad_mutes",
        "silent %.1f ms after releasing pad 2 although pad 3 is still held", gone - tRel);
  release(padLine(3), true);
  runMs(100);

  // empty pads
  bool silent = true;
  for (int p = 5; p <= 8; p++) {
    const double t = nowMs();
    press(padLine(p), true);
    runMs(150);
    silent = silent && nonZero(t, 150) == 0 && ledsAre(-1, kWhite, &d);
    release(padLine(p), true);
    runMs(50);
  }
  check(silent, "empty_pads_5_8", "no audio, LEDs stay white%s", silent ? "" : (" " + d).c_str());

  // MUTE while holding
  press(padLine(1), true);
  runMs(300);
  const double tMute = nowMs();
  DisplayWatch dw;
  tapButton(PIN_MUTE, 60);
  const double goneMute = lastAboveMs(tMute, 300, kDbfs60);
  const std::string disp = shown();
  runMs(200);
  const double tail = ana::peak(audioMs(nowMs() - 100, 100));
  check(goneMute - tMute < 90 && tail < 1e-4 && disp == "MUTE", "mute_button_while_holding",
        "below -60 dBFS %.1f ms after MUTE pressed (5 ms debounce + DMA + 10 ms ramp), peak %.1f dBFS 190-290 ms after, display '%s'",
        goneMute - tMute, ana::db(tail), disp.c_str());
  const double tTest = nowMs();
  tapButton(PIN_MUTE, 60);
  runMs(200);
  check(nonZero(tTest, 300) == 0 && dw.firstAt("TEST") < 0, "mute_button_test_refused_in_run",
        "%zu non-zero frames, display%s", nonZero(tTest, 300), dw.trace(tMute).c_str());
  runMs(1000);
  check(sim::display_is("128.5") && dw.shownFor("MUTE") >= 895 && dw.shownFor("MUTE") <= 1160, "mute_flash_900ms",
        "MUTE shown %.0f ms, then '%s'", dw.shownFor("MUTE"), shown().c_str());
  release(padLine(1), true);
  runMs(50);
  const std::string reply = cmd("test");
  check(contains(reply, "not allowed") && nonZero(nowMs() - 3, 200) == 0, "console_test_refused_in_run", "reply '%s'",
        firstLine(reply).c_str());
}

// Pad ownership (app.js heldSource): the first source to hold a pad owns it.
void scenarioOwner() {
  boot();
  runMs(1700);
  // --- a physical hold cannot be taken or released by the console
  press(padLine(1), true);
  runMs(300);
  const std::string rDown = lastLine(cmd("down 2"));
  const std::string rUp = lastLine(cmd("up 1"));
  const std::string rTap = lastLine(cmd("tap 2 300"));
  runMs(600);
  const bool still = playing(128.5) && fabs(rmsLast(200) / kExpectedRms035 - 1) < 0.02;
  check(rDown == "ignored: another pad is held" && rTap == "ignored: another pad is held" && still,
        "console_cannot_steal_physical_hold", "down 2 -> '%s', tap 2 300 -> '%s', 600 ms later pad 1 still plays 128.5 Hz: %d",
        rDown.c_str(), rTap.c_str(), still);
  check(rUp == "ignored: the console is not holding that pad" && still, "console_up_cannot_release_physical",
        "up 1 while pad 1 is held by hand -> '%s', still playing %d", rUp.c_str(), still);
  double t = nowMs();
  release(padLine(1), true);
  runMs(300);
  check(silentAfter(t) < 80, "physical_release_mutes", "silent %.1f ms after pad 1 opened", silentAfter(t));

  // --- a console hold ignores physical pads
  const std::string rPlay = lastLine(cmd("down 1"));
  runMs(200);
  press(padLine(2), true);
  runMs(250);
  const bool p1 = playing(128.5);
  release(padLine(2), true);
  runMs(250);
  const bool p2 = playing(128.5);
  const std::string rUp2 = lastLine(cmd("up 2"));
  runMs(100);
  const bool p3 = playing(128.5);
  t = nowMs();
  const std::string rUp1 = lastLine(cmd("up 1"));
  runMs(300);
  check(rPlay == "playing" && p1 && p2 && p3 && rUp2 == "ignored: the console is not holding that pad" && rUp1 == "ok" &&
            silentAfter(t) < 80,
        "console_hold_ignores_physical",
        "down 1 -> '%s'; pad 2 pressed %d / released %d keeps 128.5 Hz; up 2 -> '%s' (%d); up 1 -> '%s', silent after %.1f ms",
        rPlay.c_str(), p1, p2, rUp2.c_str(), p3, rUp1.c_str(), silentAfter(t));
  const std::string rEmpty = lastLine(cmd("down 5"));
  check(rEmpty == "ignored: pad is empty" && nonZero(nowMs() - 3, 3) == 0, "console_empty_pad", "down 5 -> '%s'",
        rEmpty.c_str());

  // --- tap: timed console hold, cannot be stolen either
  const std::string rTap1 = lastLine(cmd("tap 1 400"));
  const double tt = nowMs();
  runMs(150);
  press(padLine(3), true);
  runMs(600);
  release(padLine(3), true);
  runMs(100);
  const double end = lastAboveMs(tt, 800, kDbfs60) - tt;
  const auto mid = audioMs(tt + 200, 150);
  check(rTap1 == "playing" && fabs(freqOf(mid) - 128.5) < 0.1 && end > 390 && end < 470, "tap_holds_then_releases",
        "tap 1 400 -> '%s', %.3f Hz while pad 3 pressed, sound ends %.0f ms after the reply", rTap1.c_str(), freqOf(mid), end);

  // --- SERVICE: a knob change clears activePad, releasing the owner still mutes
  cmd("mode service");
  runMs(1000);
  press(padLine(1), true);
  runMs(300);
  const double r35 = rmsLast(200);
  turn(PIN_LEVEL_A, PIN_LEVEL_B, 5, 60);
  runMs(300);
  const double r40 = rmsLast(200);
  const Status sk = status();
  t = nowMs();
  release(padLine(1), true);
  runMs(300);
  check(fabs(r40 / r35 - 40.0 / 35) < 0.02 && sk.ok && sk.pad == 0 && !sk.muted && silentAfter(t) < 80,
        "knob_change_then_release_mutes",
        "holding pad 1, LEVEL +5: rms x%.3f (expected x%.3f), status pad %d muted %d; release -> silent after %.1f ms",
        r40 / r35, 40.0 / 35, sk.pad, sk.muted, silentAfter(t));
  const std::string rd2 = lastLine(cmd("down 2"));
  runMs(200);
  const std::string rl = lastLine(cmd("level 30"));
  runMs(200);
  const bool still2 = playing(196);
  t = nowMs();
  const std::string ru2 = lastLine(cmd("up 2"));
  runMs(300);
  check(rd2 == "playing" && rl == "ok" && still2 && ru2 == "ok" && silentAfter(t) < 80, "console_knob_change_then_up_mutes",
        "down 2 -> '%s', level 30 -> '%s' (still sounding %d), up 2 -> '%s', silent after %.1f ms", rd2.c_str(), rl.c_str(),
        still2, ru2.c_str(), silentAfter(t));

  // --- armed SAVE: a console press stores the sound
  tapButton(PIN_SAVE, 60);
  const std::string rs = lastLine(cmd("down 7"));
  const std::string disp = shown();
  const std::string ru7 = lastLine(cmd("up 7"));
  const Status s7 = status();
  check(rs == "saved" && disp == "SAVED" && ru7 == "ok" && s7.ok && s7.pads[6].used && s7.pads[6].hz == 196 &&
            s7.pads[6].level == 30,
        "console_down_saves_when_armed", "down 7 -> '%s', display '%s', up 7 -> '%s', pad 7 %.1f Hz %d%%", rs.c_str(),
        disp.c_str(), ru7.c_str(), s7.pads[6].hz, s7.pads[6].level);

  // --- sweep limits and the sweep editor
  cmd("sweep 150 300");
  cmd("mode run");
  runMs(1000);
  const std::string rOut = lastLine(cmd("down 1"));
  const std::string rIn = lastLine(cmd("down 2"));
  runMs(200);
  const bool in2 = playing(196);
  cmd("up 2");
  runMs(200);
  check(rOut == "ignored: preset outside the sweep limits" && rIn == "playing" && in2, "console_sweep_limits",
        "limits 150-300: down 1 (128.5 Hz) -> '%s', down 2 (196 Hz) -> '%s' (plays %d)", rOut.c_str(), rIn.c_str(), in2);
  cmd("mode service");
  runMs(1000);
  openSweepEditor();
  const std::string rEd = lastLine(cmd("down 2"));
  t = nowMs();
  press(padLine(2), true);
  runMs(300);
  release(padLine(2), true);
  runMs(100);
  const size_t nz = nonZero(t, 400);
  tapButton(PIN_MUTE, 60);
  const std::string batal = shown();
  check(rEd == "ignored: sweep editor is open" && nz == 0 && batal == "BATAL", "pads_ignored_in_sweep_editor",
        "down 2 -> '%s', physical pad 2: %zu non-zero frames, MUTE -> '%s'", rEd.c_str(), nz, batal.c_str());
}

const HarmonicRef* refByName(const char* name) {
  for (const auto& r : kHarmonicRefs)
    if (!strcmp(r.name, name)) return &r;
  return nullptr;
}

// Compares the steady state of `seg` with the ideal simulator period.
void compareHarmonics(const char* checkName, const std::vector<float>& seg, const HarmonicRef& ref) {
  const double f0 = freqOf(seg, ref.hz * 0.8, ref.hz * 1.25);
  const auto h = ana::harmonics(seg, f0, 48000, 8);
  double refMax = 0;
  for (double a : ref.amp) refMax = std::max(refMax, a);
  double worst = 0;
  std::string detail;
  for (int k = 0; k < 8; k++) {
    char b[64];
    if (ana::db(ref.amp[k] / refMax) < -40) {
      snprintf(b, sizeof(b), " h%d:(ref %.0f dB)", k + 1, ana::db(ref.amp[k]));
    } else {
      const double dev = ana::db(h[k]) - ana::db(ref.amp[k]);
      worst = std::max(worst, fabs(dev));
      snprintf(b, sizeof(b), " h%d:%+.2f", k + 1, dev);
    }
    detail += b;
  }
  const double rms = ana::rms(seg);
  check(fabs(f0 - ref.hz) < 0.05 && worst < 0.5 && fabs(ana::db(rms / ref.rms)) < 0.2, checkName,
        "%s %d%% %s fold %d %s: f0 %.3f Hz, rms %+.2f dB vs ideal, max |dev| %.2f dB, per harmonic dB:%s", ref.wave,
        ref.level, ref.harmonics, ref.fold, ref.name, f0, ana::db(rms / ref.rms), worst, detail.c_str());
}

void scenarioHarmonics() {
  boot();
  runMs(1700);
  for (int pad = 1; pad <= 4; pad++) {
    const HarmonicRef& ref = kHarmonicRefs[pad - 1];
    const double t = nowMs();
    press(padLine(pad), true);
    runMs(900);
    const auto seg = audioMs(t + 400, 500);
    char name[32];
    snprintf(name, sizeof(name), "pad%d_spectrum", pad);
    compareHarmonics(name, seg, ref);
    release(padLine(pad), true);
    runMs(300);
  }
}

void scenarioConsole() {
  boot();
  runMs(1700);
  std::string r = cmd("mode service");
  check(contains(r, "ok") && shown() == "SERV", "mode_service", "display '%s'", shown().c_str());
  // each change flashes what changed (app.js setParameter), a frequency change shows the frequency
  struct Step {
    const char* line;
    const char* display;
    bool frequency;
  } steps[] = {{"level 30", "L 030", false}, {"f 220", "220.0", true}, {"wave triangle", "TRI", false},
               {"harm both", "O+E", false},  {"fold 40", "F 040", false}};
  bool ok = true, disp = true;
  std::string trace;
  for (const Step& s : steps) {
    ok = ok && lastLine(cmd(s.line)) == "ok";
    const bool good = sim::framebuffer() == expectedPicture(s.display, s.frequency);
    disp = disp && good;
    trace += std::string(" [") + s.line + "]->'" + shown() + "'" + (good ? "" : "!");
  }
  Status st = status();
  check(ok && st.ok && st.mode == "SERVICE" && st.hz == 220 && st.level == 30 && st.wave == "triangle" && st.harm == "both" &&
            st.fold == 40,
        "parameters", "%s %.1f Hz level %d %s fold %d %s", st.mode.c_str(), st.hz, st.level, st.wave.c_str(), st.fold,
        st.harm.c_str());
  check(disp, "parameter_flash_texts", "app.js renders expected:%s", trace.c_str());
  check(nonZero(nowMs() - 100, 100) == 0, "silent_until_test", "%zu non-zero frames", nonZero(nowMs() - 100, 100));
  const double t = nowMs();
  r = cmd("test");
  runMs(800);
  compareHarmonics("test_sound_220Hz", audioMs(t + 300, 500), *refByName("console"));
  const double tm = nowMs();
  cmd("mute");
  runMs(300);
  check(lastAboveMs(tm, 300, kDbfs60) - tm < 70, "mute", "below -60 dBFS %.1f ms after the command",
        lastAboveMs(tm, 300, kDbfs60) - tm);
  const int nvs0 = sim::nvs_writes();
  r = lastLine(cmd("save 6"));
  const std::string dsave = shown();
  st = status();
  const PadInfo& p6 = st.pads[5];
  check(r == "saved" && dsave == "SAVED" && p6.used && p6.hz == 220 && p6.level == 30 && p6.wave == "triangle" &&
            p6.fold == 40 && p6.harm == "both" && sim::nvs_writes() == nvs0 + 1 && !st.armed,
        "save_6", "reply '%s', display '%s', pad 6: %.1f Hz %d%% %s %d %s, NVS writes +%d, armed %d", r.c_str(), dsave.c_str(),
        p6.hz, p6.level, p6.wave.c_str(), p6.fold, p6.harm.c_str(), sim::nvs_writes() - nvs0, st.armed);
  cmd("mode run");
  runMs(1100);
  const std::string rRun = lastLine(cmd("save 3"));
  check(contains(rRun, "not allowed") && sim::nvs_writes() == nvs0 + 1, "save_refused_in_run", "save 3 in RUN -> '%s'",
        rRun.c_str());
  const double tp = nowMs();
  press(padLine(6), true);
  runMs(900);
  compareHarmonics("pad6_plays_saved_sound", audioMs(tp + 300, 500), *refByName("console"));
  std::string d;
  { const bool ledOk = ledsAre(5, kTosca, &d); check(ledOk, "led6_tosca", "%s", d.c_str()); }
  release(padLine(6), true);
  runMs(200);
  // export -> file for the simulator importer (run.sh)
  std::string json = cmd("export");
  json = firstLine(json);
  if (const char* path = getenv("CP100_EXPORT")) std::ofstream(path) << json << "\n";
  check(json.rfind("{\"schema\":\"chladni-08\"", 0) == 0 && contains(json, "\"frequencyHz\":220.0"), "export_json",
        "%s", json.c_str());
  // factory: SERVICE only (app.js IMPORT), releases and mutes
  const std::string rf = lastLine(cmd("factory"));
  const Status sf = status();
  check(contains(rf, "not allowed") && sf.pads[5].used && sim::nvs_writes() == nvs0 + 1, "factory_refused_in_run",
        "factory in RUN -> '%s', pad 6 kept %d", rf.c_str(), sf.pads[5].used);
  cmd("mode service");
  cmd("test");
  runMs(400);
  const bool before = rmsLast(200) > 0.01;
  const double tf = nowMs();
  const std::string rf2 = lastLine(cmd("factory"));
  runMs(300);
  const Status st2 = status();
  check(rf2 == "ok" && before && silentAfter(tf) < 120 && st2.muted && !st2.pads[5].used && st2.pads[0].hz == 128.5 &&
            st2.pads[3].hz == 384 && sim::nvs_writes() == nvs0 + 2,
        "factory", "TEST playing %d -> factory '%s', silent after %.0f ms, muted %d, pad 6 used %d, pad 1 %.1f Hz, NVS +%d",
        before, rf2.c_str(), silentAfter(tf), st2.muted, st2.pads[5].used, st2.pads[0].hz, sim::nvs_writes() - nvs0);
}

// Physical service panel through the emulated MCP23017.
void scenarioPanel() {
  attachPanel(false);
  boot();
  runMs(100);
  const bool splash100 = sim::display_is("CP100");
  runMs(1300);
  const bool splash1400 = sim::display_is("CP100");
  runMs(400);
  check(contains(sim::serial_log(), "panel: MCP23017 connected") && sim::mcp_register(0x00) == 0x7F &&
            sim::mcp_register(0x0C) == 0x7F && sim::mcp_register(0x01) == 0x00 && sim::mcp_register(0x14) == 0 &&
            sim::mcp_register(0x15) == 0,
        "mcp_configured", "IODIRA %02X GPPUA %02X IODIRB %02X OLATA %02X OLATB %02X", sim::mcp_register(0x00),
        sim::mcp_register(0x0C), sim::mcp_register(0x01), sim::mcp_register(0x14), sim::mcp_register(0x15));
  check(splash100 && splash1400, "boot_splash_with_panel",
        "CP100 at 100 ms %d and 1400 ms %d (MODE slide = RUN = boot mode: no RUN flash)", splash100, splash1400);

  slideMode(true);
  const std::string disp = shown();
  Status st = status();
  check(disp == "SERV" && st.mode == "SERVICE" && st.wave == "sine" && st.harm == "odd", "mode_slide_service",
        "display '%s', status mode %s wave %s harm %s", disp.c_str(), st.mode.c_str(), st.wave.c_str(), st.harm.c_str());

  struct Step {
    const char* slide;
    const char* pos;
    const char* expect;
    const char* flash;  // nullptr: no change, no flash expected
  } steps[] = {{"wave", "saw", "sawtooth", "SAW"},   {"wave", "triangle", "triangle", "TRI"}, {"wave", "fault", "triangle", nullptr},
               {"wave", "sine", "sine", "SINE"},     {"wave", "fault", "sine", nullptr},      {"wave", "saw", "sawtooth", "SAW"},
               {"harm", "even", "even", "EVEN"},     {"harm", "both", "both", "O+E"},         {"harm", "fault", "both", nullptr},
               {"harm", "odd", "odd", "ODD"},        {"harm", "fault", "odd", nullptr},       {"harm", "even", "even", "EVEN"}};
  bool allOk = true, flashOk = true;
  std::string trace;
  for (const Step& s : steps) {
    const size_t mark = sim::serial_log().size();
    DisplayWatch dw;
    if (!strcmp(s.slide, "wave")) slideWave(s.pos);
    else slideHarm(s.pos);
    const bool flashed = s.flash ? dw.firstAt(s.flash) >= 0 : dw.ev.size() == 1;
    flashOk = flashOk && flashed;
    (void)mark;
    st = status();
    const std::string got = !strcmp(s.slide, "wave") ? st.wave : st.harm;
    allOk = allOk && got == s.expect;
    trace += std::string(" ") + s.slide + ":" + s.pos + "->" + got + "/'" + dw.ev.back().text + "'" + (flashed ? "" : "!");
  }
  check(allOk, "slides_incl_middle_and_both_low", "%s", trace.c_str());
  check(flashOk, "slide_flash_texts", "each real change flashes SAW/TRI/SINE, EVEN/O+E/ODD, a both-LOW fault changes nothing:%s",
        trace.c_str());
  runMs(1000);

  // FOLD encoder on GPA4/GPA5
  st = status();
  int fold0 = st.fold;
  turn(MCP_FOLD_A, MCP_FOLD_B, 5, 60, true);
  const std::string foldDisp = shown();
  st = status();
  const int slowUp = st.fold - fold0;
  turn(MCP_FOLD_A, MCP_FOLD_B, -2, 60, true);
  const int slowDown = status().fold - st.fold;
  fold0 = status().fold;
  runMs(100);
  turn(MCP_FOLD_A, MCP_FOLD_B, 4, 20, true);
  const int fast = status().fold - fold0;
  char fexp[16];
  snprintf(fexp, sizeof(fexp), "F %03d", st.fold);
  check(slowUp == 5 && slowDown == -2 && foldDisp == fexp, "fold_encoder_slow_bounce",
        "+5 detents -> %+d (display '%s', expected '%s'), -2 detents -> %+d", slowUp, foldDisp.c_str(), fexp, slowDown);
  check(fast == 1 + 3 * 4, "fold_encoder_fast_x4", "4 detents 20 ms apart -> %+d (expected 1 + 3x4 = 13)", fast);

  // FREQ encoder (direct GPIO)
  runMs(1000);
  double hz0 = status().hz;
  turn(PIN_FREQ_A, PIN_FREQ_B, 3, 60, true);
  const std::string hzDisp = shown();
  double hz1 = status().hz;
  runMs(100);
  turn(PIN_FREQ_A, PIN_FREQ_B, 5, 20, true);
  double hz2 = status().hz;
  check(fabs(hz1 - hz0 - 3) < 1e-3 && fabs(hz2 - hz1 - 41) < 1e-3 && hzDisp == freqText(hz1), "freq_encoder_slow_fast",
        "slow 3 detents %.1f -> %.1f (+%.1f, display '%s'), fast 5 detents -> %.1f (+%.1f, expected 1 + 4x10 = 41)", hz0,
        hz1, hz1 - hz0, hzDisp.c_str(), hz2, hz2 - hz1);
  tapButton(PIN_FREQ_PUSH, 120);
  const std::string fineDisp = shown();
  st = status();
  turn(PIN_FREQ_A, PIN_FREQ_B, 3, 60, true);
  runMs(100);
  turn(PIN_FREQ_A, PIN_FREQ_B, 3, 20, true);
  const double hz3 = status().hz;
  tapButton(PIN_FREQ_PUSH, 120);
  const std::string coarseDisp = shown();
  const Status st2 = status();
  // README: "putaran cepat x10" also in 0.1 Hz mode: 3 slow (0.3) + 1 + 2 fast x10 (0.1 + 2.0)
  check(fineDisp == "0.1" && st.step == "0.1" && fabs(hz3 - hz2 - 2.4) < 1e-3 && coarseDisp == "1" && st2.step == "1",
        "freq_push_fine_step",
        "push -> '%s' step %s; 3 slow + 3 fast detents %.1f -> %.1f (+%.1f, expected 0.3 + 0.1 + 2x1.0); push -> '%s' step %s",
        fineDisp.c_str(), st.step.c_str(), hz2, hz3, hz3 - hz2, coarseDisp.c_str(), st2.step.c_str());

  // LEVEL encoder
  const int lv0 = status().level;
  turn(PIN_LEVEL_A, PIN_LEVEL_B, 5, 60, true);
  const std::string lvDisp = shown();
  const int lv1 = status().level;
  runMs(100);
  turn(PIN_LEVEL_A, PIN_LEVEL_B, 3, 20, true);
  const int lv2 = status().level;
  turn(PIN_LEVEL_A, PIN_LEVEL_B, -40, 20, false);
  const int lv3 = status().level;
  char lexp[16];
  snprintf(lexp, sizeof(lexp), "L %03d", lv1);
  check(lv1 - lv0 == 5 && lv2 - lv1 == 9 && lv3 == 0 && lvDisp == lexp, "level_encoder",
        "%d -> %d (slow +5, display '%s') -> %d (fast 3 = 1+2x4) -> %d (fast -40, clamps at 0)", lv0, lv1, lvDisp.c_str(),
        lv2, lv3);
  turn(PIN_LEVEL_A, PIN_LEVEL_B, 30, 60, false);

  // TEST via MUTE button in SERVICE plays the panel sound
  const Status cur = status();
  const double tt = nowMs();
  tapButton(PIN_MUTE, 60);
  const std::string testDisp = shown();
  runMs(700);
  const double ft = freqOf(audioMs(tt + 300, 400));
  check(testDisp == "TEST" && fabs(ft - cur.hz) < 0.05, "test_button_plays", "display '%s', %.3f Hz (panel %.1f Hz, %s, fold %d, %s)",
        testDisp.c_str(), ft, cur.hz, cur.wave.c_str(), cur.fold, cur.harm.c_str());
  tapButton(PIN_MUTE, 60);
  const std::string muteDisp = shown();
  runMs(1200);
  check(muteDisp == "MUTE" && rmsLast(200) == 0, "mute_button_mutes", "display '%s', rms %.5f", muteDisp.c_str(), rmsLast(200));

  // SAVE -> pad 7
  const int nvs0 = sim::nvs_writes();
  tapButton(PIN_SAVE, 60);
  const std::string saveDisp = shown();
  runMs(1200);
  const std::string saveDisp2 = shown();
  tapButton(padLine(7), 60);
  const std::string okDisp = shown();
  st = status();
  const PadInfo& p7 = st.pads[6];
  check(saveDisp == "SAVE" && saveDisp2 == freqText(cur.hz) && okDisp == "SAVED" && p7.used && fabs(p7.hz - cur.hz) < 1e-3 &&
            p7.level == cur.level && p7.wave == cur.wave && p7.fold == cur.fold && p7.harm == cur.harm &&
            sim::nvs_writes() == nvs0 + 1,
        "save_button_pad7", "'%s' (after 1.2 s '%s') -> pad 7 -> '%s'; pad 7 = %.1f Hz %d%% %s fold %d %s; NVS +%d",
        saveDisp.c_str(), saveDisp2.c_str(), okDisp.c_str(), p7.hz, p7.level, p7.wave.c_str(), p7.fold, p7.harm.c_str(),
        sim::nvs_writes() - nvs0);
  runMs(1100);
  tapButton(PIN_SAVE, 60);
  const std::string a1 = shown();
  runMs(200);
  tapButton(PIN_SAVE, 60);
  const std::string a2 = shown();
  st = status();
  check(a1 == "SAVE" && a2 == freqText(cur.hz) && !st.armed, "save_twice_disarms",
        "'%s' then '%s' (the frequency, as app.js), armed %d", a1.c_str(), a2.c_str(), st.armed);

  // RUN: encoders ignored
  slideMode(false);
  const Status before = status();
  turn(PIN_FREQ_A, PIN_FREQ_B, 5, 60, true);
  turn(PIN_LEVEL_A, PIN_LEVEL_B, 5, 60, true);
  turn(MCP_FOLD_A, MCP_FOLD_B, 5, 60, true);
  slideWave("triangle");
  const Status after = status();
  check(after.mode == "RUN" && after.hz == before.hz && after.level == before.level && after.fold == before.fold &&
            after.wave == before.wave,
        "encoders_ignored_in_run", "%.1f Hz %d%% fold %d %s before, %.1f Hz %d%% fold %d %s after", before.hz, before.level,
        before.fold, before.wave.c_str(), after.hz, after.level, after.fold, after.wave.c_str());
  const double tp = nowMs();
  press(padLine(7), true);
  runMs(700);
  const double fp = freqOf(audioMs(tp + 300, 400));
  check(fabs(fp - p7.hz) < 0.05, "pad7_plays_in_run", "%.3f Hz (saved %.1f Hz)", fp, p7.hz);
  release(padLine(7), true);
  runMs(200);
  // entering SERVICE again picks up the slides (WAVE moved to TRIANGLE while in RUN)
  slideMode(true);
  st = status();
  check(st.wave == "triangle", "service_entry_reads_slides", "wave %s after re-entering SERVICE", st.wave.c_str());

  // a slide moved while the sweep editor is open: the panel and the sound must agree afterwards
  openSweepEditor();
  slideWave("sine");
  runMs(100);
  tapButton(PIN_SAVE, 60);  // stores the (unchanged) limits and closes the editor
  runMs(300);
  st = status();
  check(st.panelWave == "sine" && st.wave == "sine", "slide_moved_during_sweep_edit",
        "after the editor closes the panel reports WAVE %s, the sound uses %s", st.panelWave.c_str(), st.wave.c_str());

  // the MODE slide switches the mode only when it differs (app.js mode buttons)
  runMs(1000);
  cmd("mode run");  // console RUN while the slide still says SERVICE
  runMs(1000);
  press(padLine(1), true);
  runMs(300);
  DisplayWatch dw;
  slideMode(false);  // slide now agrees with the mode: no enterMode
  runMs(300);
  const bool keeps = playing(128.5);
  release(padLine(1), true);
  runMs(200);
  check(keeps && dw.firstAt("RUN") < 0, "mode_slide_equal_mode_no_reentry",
        "console RUN, pad 1 held, slide moved to RUN: still playing %d, display%s", keeps, dw.trace(dw.ev[0].t).c_str());
}

// Matrix messages, their durations and the armed-SAVE cycle (physical panel).
void scenarioFlash() {
  attachPanel(false);
  boot();
  runMs(1800);
  double t0 = nowMs();
  {
    DisplayWatch dw;
    slideMode(true);
    runMs(1300);
    const double serv = dw.shownFor("SERV");
    check(dw.firstAt("SERV") >= 0 && serv >= 895 && serv <= 1160 && sim::display_is("128.5"), "mode_serv_flash",
          "SERV for %.0f ms (900 + up to 250 ms refresh), then '%s';%s", serv, shown().c_str(), dw.trace(t0).c_str());
  }
  // FREQ push: step 0.1 / 1
  tapButton(PIN_FREQ_PUSH, 100);
  const std::string f01 = shown();
  runMs(1200);
  const std::string back = shown();
  tapButton(PIN_FREQ_PUSH, 100);
  const std::string f1 = shown();
  runMs(1200);
  check(f01 == "0.1" && back == "128.5" && f1 == "1", "freq_push_flash", "'%s' -> '%s' -> '%s'", f01.c_str(), back.c_str(),
        f1.c_str());

  // LEVEL: "L 036" for 900 ms, exact app.js picture
  t0 = nowMs();
  {
    DisplayWatch dw;
    turn(PIN_LEVEL_A, PIN_LEVEL_B, 1, 60);
    const bool exact = sim::framebuffer() == expectedPicture("L 036");
    runMs(1300);
    const double lms = dw.shownFor("L 036");
    check(exact && lms >= 895 && lms <= 1160 && sim::display_is("128.5"), "level_flash_900ms",
          "'L 036' app.js picture %d, shown %.0f ms, then '%s'", exact, lms, shown().c_str());
  }
  // a frequency change clears the message at once
  turn(PIN_LEVEL_A, PIN_LEVEL_B, -1, 60);
  const std::string l35 = shown();
  runMs(100);
  t0 = nowMs();
  turn(PIN_FREQ_A, PIN_FREQ_B, 1, 60);
  const std::string after = shown();
  check(l35 == "L 035" && after == "129.5", "freq_change_clears_message", "'%s', FREQ +1 100 ms later -> '%s' at once",
        l35.c_str(), after.c_str());
  turn(PIN_FREQ_A, PIN_FREQ_B, -1, 60);
  // FOLD
  turn(MCP_FOLD_A, MCP_FOLD_B, 1, 60);
  const std::string fold = shown();
  const bool foldExact = sim::framebuffer() == expectedPicture("F 001");
  turn(MCP_FOLD_A, MCP_FOLD_B, -1, 60);
  check(fold == "F 001" && foldExact && shown() == "F 000", "fold_flash", "'%s' (app.js picture %d) then '%s'", fold.c_str(),
        foldExact, shown().c_str());
  // slides
  std::string trace;
  bool slides = true;
  for (const auto& s : std::vector<std::pair<std::string, std::string>>{
           {"saw", "SAW"}, {"triangle", "TRI"}, {"sine", "SINE"}, {"even", "EVEN"}, {"both", "O+E"}, {"odd", "ODD"}}) {
    if (s.first == "even" || s.first == "both" || s.first == "odd") slideHarm(s.first.c_str());
    else slideWave(s.first.c_str());
    const bool good = sim::framebuffer() == expectedPicture(s.second);
    slides = slides && good;
    trace += " " + s.first + "->'" + shown() + "'" + (good ? "" : "!");
  }
  check(slides, "slide_flashes", "app.js pictures:%s", trace.c_str());
  runMs(1200);
  // MUTE: TEST / MUTE
  tapButton(PIN_MUTE, 60);
  const std::string test = shown();
  tapButton(PIN_MUTE, 60);
  const std::string mute = shown();
  check(test == "TEST" && mute == "MUTE", "mute_button_flashes", "'%s' then '%s'", test.c_str(), mute.c_str());
  runMs(1200);

  // SAVE armed: SAVE, frequency, SAVE again 900 ms of every 2 s
  {
    DisplayWatch dw;
    press(PIN_SAVE, true);
    runMs(60);
    release(PIN_SAVE, true);
    runUntil([] { return sim::display_is("SAVE"); }, 100);
    const double ta = dw.firstAt("SAVE");  // the moment SAVE appeared, not when the poll saw it
    runMs(6500);
    // expected: SAVE [0,900) [2000,2900) [4000,4900) [6000..; transitions late by up to one 250 ms refresh
    std::vector<Shown> ev;
    for (const Shown& s : dw.ev)
      if (s.t >= ta - 1) ev.push_back(s);
    bool cycle = ev.size() == 7;
    for (size_t i = 0; cycle && i < ev.size(); i++) {
      const bool save = i % 2 == 0;
      const double nominal = (i / 2) * 2000.0 + (save ? 0 : 900);
      const double late = ev[i].t - ta - nominal;
      cycle = ev[i].text == (save ? "SAVE" : "128.5") && late >= -2 && late <= 260;
    }
    check(cycle, "armed_save_cycle", "SAVE 900 ms of every 2 s while armed (each change at most 250 ms late):%s",
          dw.trace(ta).c_str());
    tapButton(PIN_SAVE, 60);
    const std::string dis = shown();
    DisplayWatch dw2;
    runMs(2500);
    check(dis == "128.5" && dw2.ev.size() == 1 && !status().armed, "disarm_shows_frequency",
          "SAVE again -> '%s', no SAVE in the next 2.5 s:%s", dis.c_str(), dw2.trace(dw2.ev[0].t).c_str());
  }

  // SAVE -> pad: SAVED for 900 ms; with a failing NVS write: RAM
  {
    tapButton(PIN_SAVE, 60);
    DisplayWatch dw;
    const double tp = nowMs();
    tapButton(padLine(7), 60);
    runMs(1300);
    const double ms = dw.shownFor("SAVED");
    check(ms >= 895 && ms <= 1160 && sim::display_is("128.5") && sim::nvs_writes() == 1, "saved_flash",
          "SAVED shown %.0f ms, then '%s', NVS writes %d;%s", ms, shown().c_str(), sim::nvs_writes(), dw.trace(tp).c_str());
  }
  {
    sim::nvs_set_fail(true);
    const size_t mark = sim::serial_log().size();
    tapButton(PIN_SAVE, 60);
    tapButton(padLine(8), 60);
    const std::string ram = shown();
    sim::nvs_set_fail(false);
    runMs(1200);
    const Status st = status();
    check(ram == "RAM" && contains(logSince(mark), "NVS write failed") && contains(logSince(mark), "(RAM only)") &&
              st.pads[7].used,
          "ram_flash_when_nvs_fails", "display '%s', serial: %s, pad 8 kept in RAM %d", ram.c_str(),
          firstLine(logSince(mark)).c_str(), st.pads[7].used);
  }
  // MODE back to RUN
  slideMode(false);
  const std::string run = shown();
  check(run == "RUN", "mode_run_flash", "'%s'", run.c_str());
  // console display TEXT: 3 s
  {
    DisplayWatch dw;
    t0 = nowMs();
    const std::string r = lastLine(cmd("display P100"));
    runMs(3400);
    const double ms = dw.shownFor("P100");
    check(r == "ok" && ms >= 2995 && ms <= 3260 && sim::display_is("128.5"), "console_display_3s",
          "display P100 -> '%s', shown %.0f ms, then '%s'", r.c_str(), ms, shown().c_str());
  }
}

void scenarioSweep() {
  boot();
  runMs(1700);
  cmd("mode service");
  cmd("test");
  runMs(1100);
  const int nvs0 = sim::nvs_writes();
  DisplayWatch dw;
  const double tPress = nowMs();
  press(PIN_FREQ_PUSH, true);
  const double tMin = runUntil([] { return sim::display_is("MIN"); }, 3000);
  release(PIN_FREQ_PUSH, true);
  const double tShow = waitDisplay("20.0", 1500);
  const double minMs = dw.shownFor("MIN");
  check(tMin > 1990 && tMin < 2020 && minMs >= 795 && minMs <= 1060 && tShow > 0, "hold_2s_opens_min",
        "'MIN' after %.0f ms of holding FREQ, shown %.0f ms (800 + refresh), then '20.0'", tMin, minMs);
  check(silentAfter(tPress + tMin, 300) < 80, "editor_mutes", "TEST sound silent %.1f ms after the editor opened",
        silentAfter(tPress + tMin, 300));
  turn(PIN_FREQ_A, PIN_FREQ_B, 13, 20);
  runMs(100);
  turn(PIN_FREQ_A, PIN_FREQ_B, 9, 60);
  const std::string minDisp = shown();
  tapButton(PIN_FREQ_PUSH, 100);
  const std::string maxLabel = shown();
  const double tLab = nowMs();
  const double tMax = waitDisplay("2000.0", 1500);
  const double maxMs = dw.shownFor("MAX", tLab - 200);
  turn(PIN_FREQ_A, PIN_FREQ_B, -170, 20);
  runMs(100);
  turn(PIN_FREQ_A, PIN_FREQ_B, -9, 60);
  const std::string maxDisp = shown();
  check(minDisp == "150.0" && maxLabel == "MAX" && tMax >= 0 && maxMs >= 795 && maxMs <= 1060 && maxDisp == "300.0",
        "turn_min_max",
        "MIN set to '%s' (13 fast + 9 slow, 1 Hz steps x10 fast), push -> '%s' for %.0f ms -> 2000.0, MAX set to '%s' (170 fast + 9 slow down)",
        minDisp.c_str(), maxLabel.c_str(), maxMs, maxDisp.c_str());
  // pads, console pad/save/factory are refused while editing
  double t = nowMs();
  press(padLine(2), true);
  runMs(200);
  release(padLine(2), true);
  runMs(100);
  const std::string rd = lastLine(cmd("down 3"));
  const std::string rs = lastLine(cmd("save 3"));
  const std::string rf = lastLine(cmd("factory"));
  check(nonZero(t, 300) == 0 && shown() == "300.0" && rd == "ignored: sweep editor is open" && rs == "ignored: sweep editor is open" &&
            contains(rf, "not allowed") && sim::nvs_writes() == nvs0,
        "pads_ignored_while_editing", "pad 2: %zu non-zero frames, display '%s'; down 3 -> '%s', save 3 -> '%s', factory -> '%s'",
        nonZero(t, 300), shown().c_str(), rd.c_str(), rs.c_str(), rf.c_str());
  // the limits keep a 1 Hz gap
  tapButton(PIN_FREQ_PUSH, 100);  // MIN
  turn(PIN_FREQ_A, PIN_FREQ_B, 30, 20);
  const std::string minTop = shown();
  tapButton(PIN_FREQ_PUSH, 100);  // MAX
  turn(PIN_FREQ_A, PIN_FREQ_B, -5, 60);
  const std::string maxBottom = shown();
  tapButton(PIN_FREQ_PUSH, 100);  // MIN
  turn(PIN_FREQ_A, PIN_FREQ_B, -15, 20);
  runMs(100);
  turn(PIN_FREQ_A, PIN_FREQ_B, -8, 60);
  const std::string minBack = shown();
  check(minTop == "299.0" && maxBottom == "300.0" && minBack == "150.0", "min_gap_1hz",
        "MIN spun up past MAX stops at '%s', MAX turned down stays at '%s', MIN back to '%s'", minTop.c_str(),
        maxBottom.c_str(), minBack.c_str());
  const size_t mark = sim::serial_log().size();
  tapButton(PIN_SAVE, 60);
  const std::string okDisp = shown();
  Status st = status();
  check(okDisp == "SAVED" && contains(logSince(mark), "sweep 150.0-300.0 Hz") && st.sweepMin == 150 && st.sweepMax == 300 &&
            st.sweepSet && sim::nvs_writes() == nvs0 + 1,
        "save_stores_limits", "display '%s', status sweep %.1f-%.1f set=%d, NVS +%d", okDisp.c_str(), st.sweepMin, st.sweepMax,
        st.sweepSet, sim::nvs_writes() - nvs0);

  cmd("mode run");
  runMs(1100);
  struct Want {
    int pad;
    bool plays;
  } wants[] = {{1, false}, {2, true}, {3, true}, {4, false}};
  std::string trace;
  bool ok = true;
  for (const Want& w : wants) {
    const double tw = nowMs();
    press(padLine(w.pad), true);
    runMs(500);
    const auto seg = audioMs(tw + 250, 250);
    const bool plays = ana::rms(seg) > 0.01;
    const double f = plays ? freqOf(seg) : 0;
    ok = ok && plays == w.plays;
    char b[64];
    snprintf(b, sizeof(b), " pad%d:%s%.1f", w.pad, plays ? "plays " : "silent ", f);
    trace += b;
    release(padLine(w.pad), true);
    runMs(300);
  }
  check(ok, "run_respects_limits", "limits 150-300 Hz:%s", trace.c_str());

  cmd("mode service");
  runMs(1100);
  openSweepEditor();
  runMs(900);
  const std::string min2 = shown();
  turn(PIN_FREQ_A, PIN_FREQ_B, 5, 60);
  const std::string edited = shown();
  tapButton(PIN_MUTE, 60);
  const std::string cancel = shown();
  runMs(1200);
  const std::string afterCancel = shown();
  st = status();
  check(min2 == "150.0" && edited == "155.0" && cancel == "BATAL" && afterCancel == freqText(st.hz) && st.sweepMin == 150 &&
            st.sweepMax == 300 && sim::nvs_writes() == nvs0 + 1 && st.muted,
        "mute_cancels_edit", "editor shows '%s', edited to '%s', MUTE -> '%s' -> '%s', limits still %.1f-%.1f, NVS +%d, muted %d",
        min2.c_str(), edited.c_str(), cancel.c_str(), afterCancel.c_str(), st.sweepMin, st.sweepMax,
        sim::nvs_writes() - nvs0, st.muted);

  // console sweep: rounded to 0.1, at least 1 Hz apart; the editor starts from them
  const std::string r1 = lastLine(cmd("sweep 100 100.5"));
  const std::string r2 = lastLine(cmd("sweep 30.04 1500.06"));
  st = status();
  check(contains(r1, "not allowed") && r2 == "ok" && fabs(st.sweepMin - 30.0) < 1e-6 && fabs(st.sweepMax - 1500.1) < 1e-6,
        "console_sweep_rounding", "sweep 100 100.5 -> '%s'; sweep 30.04 1500.06 -> '%s', status %.1f-%.1f", r1.c_str(),
        r2.c_str(), st.sweepMin, st.sweepMax);
  openSweepEditor();
  runMs(900);
  const std::string emin = shown();
  turn(PIN_FREQ_A, PIN_FREQ_B, 1, 60);
  const std::string emin1 = shown();
  tapButton(PIN_FREQ_PUSH, 100);
  runMs(900);
  const std::string emax = shown();
  turn(PIN_FREQ_A, PIN_FREQ_B, -1, 60);
  const std::string emax1 = shown();
  // a failing NVS write keeps the limits in RAM
  sim::nvs_set_fail(true);
  const size_t mark2 = sim::serial_log().size();
  tapButton(PIN_SAVE, 60);
  const std::string ram = shown();
  sim::nvs_set_fail(false);
  st = status();
  check(emin == "30.0" && emin1 == "31.0" && emax == "1500.1" && emax1 == "1499.1" && ram == "RAM" &&
            contains(logSince(mark2), "(RAM only)") && fabs(st.sweepMin - 31) < 1e-6 && fabs(st.sweepMax - 1499.1) < 1e-4,
        "editor_steps_and_ram", "MIN '%s' +1 -> '%s', MAX '%s' -1 -> '%s', SAVE with NVS failing -> '%s', status %.1f-%.1f",
        emin.c_str(), emin1.c_str(), emax.c_str(), emax1.c_str(), ram.c_str(), st.sweepMin, st.sweepMax);
}

// NVS writes: the DMA buffers that loop during the flash write must be silent,
// and the fade before it must be click-free, for every write path.
void scenarioNvs() {
  boot();
  runMs(1700);
  cmd("mode service");
  cmd("f 200");
  auto testOn = [] {
    cmd("test");
    runMs(400);
  };
  struct Result {
    std::string name;
    bool ok;
  };
  auto path = [&](const char* name, bool soundBefore, const std::function<void()>& act) {
    const size_t n0 = sim::nvs_log().size();
    const double steady = maxStepIn(nowMs() - 150, 150);
    const double rmsBefore = rmsLast(150);
    const double t0 = nowMs();
    act();
    runMs(150);
    const auto& log = sim::nvs_log();
    double ring = 0, loopPeak = 0, fade = 0, waitMs = 0;
    for (size_t i = n0; i < log.size(); i++) {
      const double tw = log[i].t_us / 1000.0;
      ring = std::max(ring, log[i].ringPeak);
      loopPeak = std::max(loopPeak, ana::peak(audioMs(tw, log[i].stallUs / 1000.0 + 11)));
      if (i == n0) fade = maxStepIn(t0, tw - t0), waitMs = tw - t0;
    }
    const size_t writes = log.size() - n0;
    const bool ok = writes > 0 && ring <= kNvsRingMax && loopPeak <= kNvsRingMax && fade <= steady * 1.01 + 1e-9 &&
                    (soundBefore ? rmsBefore > 0.02 : rmsBefore == 0);
    char chk[64];
    snprintf(chk, sizeof(chk), "silent_%s", name);
    check(ok, chk,
          "%s: %zu NVS write(s) %.0f ms after the action, looping DMA buffers peak %.1f dBFS (limit %.1f), DAC during the stall "
          "%.1f dBFS, fade max step %.5f vs steady %.5f, sound before %.4f rms",
          name, writes, waitMs, ana::db(ring), ana::db(kNvsRingMax), ana::db(loopPeak), fade, steady, rmsBefore);
  };

  testOn();
  path("console_save", true, [] { cmd("save 6"); });
  testOn();
  path("save_button_then_pad", true, [] {
    // SAVE and pad 7 almost together: the arm fade has only just begun when the save starts
    setContact(PIN_SAVE, 0, false, 50);
    setContact(padLine(7), 0, false, 2550);
    runMs(30);
    setContact(PIN_SAVE, 1);
    setContact(padLine(7), 1);
  });
  check(status().pads[6].used, "pad7_saved", "the SAVE + pad path stored pad 7");
  runMs(1000);
  path("sweep_editor_save", false, [] {
    openSweepEditor();
    turn(PIN_FREQ_A, PIN_FREQ_B, 3, 60);
    tapButton(PIN_SAVE, 60);
  });
  testOn();
  path("console_sweep", true, [] { cmd("sweep 25 1900"); });
  testOn();
  path("console_led", true, [] { cmd("led pink"); });
  testOn();
  path("console_pulse", true, [] { cmd("pulse on"); });
  testOn();
  path("console_factory", true, [] { cmd("factory"); });
  testOn();
  sim::nvs_set_fail(true);
  std::string reply;
  path("console_save_nvs_failing", true, [&] { reply = lastLine(cmd("save 3")); });
  const std::string disp = shown();
  sim::nvs_set_fail(false);
  check(reply == "saved in RAM only (NVS write failed)" && disp == "RAM", "console_save_ram_reply", "reply '%s', display '%s'",
        reply.c_str(), disp.c_str());
  // all writes left the sound muted
  const Status st = status();
  check(st.ok && st.muted && rmsLast(100) < 1e-5, "muted_after_writes", "status ok %d, muted %d, rms %.3g", st.ok, st.muted, rmsLast(100));
}

// Long console replies must not hold up loop(): pad response while printing.
void scenarioTxbuf() {
  boot();
  runMs(1700);
  for (const char* c : {"export", "status", "help"}) {
    runUntil([] { return now_us() >= sim::serial_tx_idle_at_us(); }, 1000);
    runMs(50);
    const size_t mark = sim::serial_log().size();
    const uint64_t blocked0 = sim::serial_tx_blocked_us();
    resetLoopMax();
    sim::serial_send(std::string(c) + "\n");
    // the pad closes as the command's last byte arrives
    const double tPress = nowMs() + (strlen(c) + 1) * 1e3 * 10 / 115200 + 0.1;
    sim::schedule_line(uint64_t(tPress * 1000), padLine(1), 0);
    runMs(300);
    const double on = onsetMs(tPress, kDbfs60);
    const size_t bytes = sim::serial_log().size() - mark;
    char name[32];
    snprintf(name, sizeof(name), "%s_does_not_stall", c);
    check(on > 0 && on - tPress <= 20 && gLoopMaxUs < 5000 && sim::serial_tx_blocked_us() == blocked0, name,
          "%zu-byte reply (%.0f ms on the wire): pad 1 audible %.1f ms after it closed, longest loop() %.2f ms, UART blocked %.1f ms",
          bytes, bytes * 10e3 / 115200, on - tPress, gLoopMaxUs / 1000.0, (sim::serial_tx_blocked_us() - blocked0) / 1000.0);
    sim::schedule_line(now_us() + 100, padLine(1), 1);
    runMs(300);
  }
}

// FOLD/LEVEL/FREQ spun fast: loop() never blocks and calibrationFor() is never called.
void scenarioLoopgap() {
  attachPanel(true);
  boot();
  runMs(1800);
  cmd("test");
  runMs(300);
  struct Spin {
    const char* name;
    int a, b, detents;
    double msPerDetent;
  } spins[] = {{"fold", MCP_FOLD_A, MCP_FOLD_B, 24, 8}, {"level", PIN_LEVEL_A, PIN_LEVEL_B, -24, 8},
               {"freq", PIN_FREQ_A, PIN_FREQ_B, 48, 8}};
  for (const Spin& s : spins) {
    const Status before = status();
    runMs(100);
    resetLoopMax();
    turn(s.a, s.b, s.detents, s.msPerDetent);
    const uint64_t worst = gLoopMaxUs;
    const Status after = status();
    const int n = abs(s.detents);
    double got = 0, want = 0;
    if (!strcmp(s.name, "fold")) got = after.fold - before.fold, want = std::min(100, before.fold + 1 + (n - 1) * 4) - before.fold;
    if (!strcmp(s.name, "level")) got = after.level - before.level, want = std::max(0, before.level - 1 - (n - 1) * 4) - before.level;
    if (!strcmp(s.name, "freq")) got = after.hz - before.hz, want = 1 + (n - 1) * 10;
    char name[32];
    snprintf(name, sizeof(name), "%s_spin_no_block", s.name);
    check(worst < 5000 && fabs(got - want) < 1e-3, name,
          "%d detents at %.0f ms: changed %+.1f (expected %+.1f), longest loop() %.2f ms at t=%.0f ms", n, s.msPerDetent, got,
          want, worst / 1000.0, gLoopMaxAtMs);
  }
  // all three at once
  cmd("fold 50");
  cmd("level 50");
  runMs(1100);
  resetLoopMax();
  const uint64_t start = now_us() + 1000;
  uint64_t last = scheduleTurn(MCP_FOLD_A, MCP_FOLD_B, -20, 8, false, start);
  last = std::max(last, scheduleTurn(PIN_LEVEL_A, PIN_LEVEL_B, 20, 9, false, start + 300));
  last = std::max(last, scheduleTurn(PIN_FREQ_A, PIN_FREQ_B, -20, 7, false, start + 600));
  runMs((last - now_us()) / 1000.0 + 5);
  check(gLoopMaxUs < 5000 && sim::calibration_for_calls() == 0, "all_spins_no_block",
        "FOLD, LEVEL and FREQ together: longest loop() %.2f ms; calibrationFor() calls %d (the table lookup is used)",
        gLoopMaxUs / 1000.0, sim::calibration_for_calls());
}

// The I2S stops: i2s_write() gives up after 100 ms, the stall counter shows it.
void scenarioI2sStall() {
  gI2sHaltExpected = true;
  boot();
  runMs(1700);
  cmd("mode service");
  cmd("test");
  runMs(500);
  const Status s0 = status();
  sim::i2s_set_halted(true);
  runMs(450);
  const int timeouts = sim::i2s().timeouts;
  const Status s1 = status();  // loop() and the console keep running
  sim::i2s_set_halted(false);
  runMs(300);
  const Status s2 = status();
  check(s0.audioOk && s0.stalls == 0 && s1.audioOk && s1.stalls == timeouts && timeouts >= 4 && timeouts <= 5 &&
            s2.stalls == s1.stalls,
        "stall_counter", "before %ld; I2S halted 450 ms: %d i2s_write timeouts of 100 ms, status 'I2S stalls %ld'; resumed: %ld",
        s0.stalls, timeouts, s1.stalls, s2.stalls);
  check(s0.peak > 0.1 && s1.ok, "console_alive_during_stall", "audio peak %.3f before; status answered while halted: %d",
        s0.peak, s1.ok);
}

// Run 1 of the persistence pair: store things in NVS.
void scenarioPersist1() {
  boot();
  runMs(1700);
  std::string trace;
  bool ok = true;
  for (const auto& c : std::vector<std::pair<const char*, const char*>>{{"mode service", "ok"},
                                                                         {"f 333.3", "ok"},
                                                                         {"level 44", "ok"},
                                                                         {"wave saw", "ok"},
                                                                         {"harm even", "ok"},
                                                                         {"fold 12", "ok"},
                                                                         {"save 5", "saved"},
                                                                         {"sweep 30 1500", "ok"},
                                                                         {"led tosca", "ok"},
                                                                         {"pulse on", "ok"}}) {
    const std::string r = lastLine(cmd(c.first));
    ok = ok && r == c.second;
    trace += std::string(" [") + c.first + "]->" + r;
  }
  check(ok && sim::nvs_writes() == 4, "nvs_writes", "%d NVS writes for 1 preset + 3 setup changes;%s", sim::nvs_writes(),
        trace.c_str());
}

void scenarioPersist2() {
  boot();
  runMs(1700);
  const Status st = status();
  const PadInfo& p = st.pads[4];
  check(st.ok && p.used && p.hz == 333.3 && p.level == 44 && p.wave == "sawtooth" && p.fold == 12 && p.harm == "even" &&
            st.pads[0].used && st.pads[0].hz == 128.5,
        "preset_survives_reboot", "pad 5: %.1f Hz %d%% %s fold %d %s; pad 1 %.1f Hz", p.hz, p.level, p.wave.c_str(), p.fold,
        p.harm.c_str(), st.pads[0].hz);
  check(st.sweepMin == 30 && st.sweepMax == 1500 && st.sweepSet && st.led == "tosca" && st.pulse, "setup_survives_reboot",
        "sweep %.1f-%.1f set=%d, LED %s pulse %d", st.sweepMin, st.sweepMax, st.sweepSet, st.led.c_str(), st.pulse);
  const double t = nowMs();
  press(padLine(5), true);
  runMs(700);
  const double f = freqOf(audioMs(t + 300, 400));
  compareHarmonics("pad5_even_spectrum", audioMs(t + 300, 400), *refByName("persist"));
  const sim::LedFrame* lf = lastLeds();
  const bool toscaHue = lf && lf->rgb[4][1] > lf->rgb[4][0] * 2 && lf->rgb[4][1] <= kTosca.g && lf->rgb[4][1] >= kTosca.g * 0.8;
  check(fabs(f - 333.3) < 0.05 && toscaHue, "pad5_plays_after_reboot", "%.3f Hz, LED 5 %s (tosca, pulsing)", f,
        lf ? rgbStr(lf->rgb[4]).c_str() : "-");
  release(padLine(5), true);
  runMs(100);
}

void scenarioPersist3() {  // NVS blob from another layout version -> examples
  boot();
  runMs(200);
  const Status st = status();
  check(st.ok && st.pads[0].hz == 128.5 && st.pads[3].hz == 384 && !st.pads[4].used && st.sweepMin == 20 &&
            st.sweepMax == 2000 && !st.sweepSet,
        "bad_blob_falls_back", "pad 1 %.1f, pad 4 %.1f, pad 5 used %d, sweep %.0f-%.0f", st.pads[0].hz, st.pads[3].hz,
        st.pads[4].used, st.sweepMin, st.sweepMax);
}

void scenarioRobust() {
  attachPanel(false);
  boot();
  runMs(1700);

  // --- console abuse
  std::mt19937 rng(1234);
  std::string garbage;
  for (int i = 0; i < 600; i++) {
    char c = char(rng() & 0xFF);
    if (c == 'f' || c == 't' || c == 's' || c == 'd' || c == 'l' || c == 'm') c = '#';  // avoid real commands
    garbage += c;
  }
  sim::serial_send(garbage + "\n");
  runMs(120);
  const std::string longLine(200, 'x');
  std::string r = cmd(longLine);
  check(contains(r, "line too long"), "overlong_line", "200-char line -> '%s'", firstLine(r).c_str());
  struct Bad {
    const char* line;
    const char* expect;
  };
  // numbers must be finite and |x| < 1e6, pads 1-8, tap 1..60000 ms; value commands are SERVICE-only
  const Bad runBads[] = {{"level 50.5", "unknown"},  {"tap 1 0", "unknown"},     {"f nan", "unknown"},
                         {"f inf", "unknown"},       {"f -inf", "unknown"},      {"f 1e9", "unknown"},
                         {"f 1e6", "unknown"},       {"f -1e6", "unknown"},      {"f 999999", "not allowed"},
                         {"tap 9", "unknown"},       {"tap 0", "unknown"},       {"save 0", "unknown"},
                         {"down 1.5", "unknown"},    {"f 220x", "unknown"},      {"sweep 2000 20", "not allowed"},
                         {"level 3e9", "unknown"},   {"tap 1 60001", "unknown"}, {"wave square", "unknown"},
                         {"factory", "not allowed"}, {"save 3", "not allowed"},  {"f 220", "not allowed"}};
  std::string trace;
  bool ok = true;
  for (const Bad& b : runBads) {
    r = cmd(b.line);
    const bool good = contains(firstLine(r), b.expect);
    ok = ok && good;
    trace += std::string(" [") + b.line + "]->" + (good ? b.expect : firstLine(r));
  }
  const Status st0 = status();
  check(ok && st0.ok && nonZero(nowMs() - 2000, 2000) == 0 && sim::nvs_writes() == 0, "bad_commands_rejected",
        "%s; still answers status, audio silent", trace.c_str());
  r = cmd("mode service");
  ok = true;
  trace.clear();
  for (const Bad& b : {Bad{"f 1e9", "unknown"}, Bad{"f 19.9", "not allowed"}, Bad{"f 2000.1", "not allowed"},
                       Bad{"level 101", "not allowed"}, Bad{"fold -1", "not allowed"}, Bad{"level -0", "ok"},
                       Bad{"sweep 20 20", "not allowed"}, Bad{"sweep 100 100.9", "not allowed"},
                       Bad{"sweep 19.9 2000", "not allowed"}, Bad{"sweep 20 2000.1", "not allowed"},
                       Bad{"sweep 1e6 2e6", "unknown"}}) {
    r = cmd(b.line);
    const bool good = contains(firstLine(r), b.expect);
    ok = ok && good;
    trace += std::string(" [") + b.line + "]->" + (good ? b.expect : firstLine(r));
  }
  const Status st1 = status();
  check(ok && st1.hz == 128.5 && st1.level == 0 && st1.sweepMin == 20 && st1.sweepMax == 2000 && !st1.sweepSet,
        "bad_service_values_rejected", "%s; params %.1f Hz %d%%, sweep %.1f-%.1f", trace.c_str(), st1.hz, st1.level,
        st1.sweepMin, st1.sweepMax);
  cmd("level 35");
  // save is refused while a pad is held
  press(padLine(1), true);
  runMs(200);
  const std::string rh = lastLine(cmd("save 4"));
  const std::string rc = lastLine(cmd("down 2"));
  release(padLine(1), true);
  runMs(200);
  cmd("down 2");
  const std::string rc2 = lastLine(cmd("save 4"));
  cmd("up 2");
  runMs(100);
  const Status sh = status();
  check(rh == "ignored: another pad is held" && rc2 == "ignored: another pad is held" && sh.ok && sh.pads[3].hz == 384 && sh.pads[3].level == 40,
        "save_refused_while_held", "pad 1 held by hand: save 4 -> '%s'; console holding pad 2: save 4 -> '%s' (%s)", rh.c_str(),
        rc2.c_str(), rc.c_str());
  slideMode(true);   // physical switch to SERVICE so that the next change is real
  slideMode(false);  // back to RUN

  // --- MCP23017 lost and found
  size_t mark = sim::serial_log().size();
  const double tLost = nowMs();
  sim::mcp_attach(false);
  const double lostAfter = runUntil([&] { return contains(logSince(mark), "MCP23017 lost"); }, 400);
  runMs(400);
  sim::mcp_attach(true);
  const double backAfter = runUntil([&] { return contains(logSince(mark), "MCP23017 connected"); }, 2000);
  // lost = reads failing for 200 ms (a bus glitch is not a missing panel)
  check(lostAfter >= 199 && lostAfter < 215 && backAfter >= 0 && backAfter <= 1000, "mcp_detach_reattach",
        "loss reported %.1f ms after unplug, reconnected %.0f ms after replug (%.0f ms after loss)", lostAfter, backAfter,
        nowMs() - tLost);
  runMs(100);

  // --- reads failing for ~150 ms: not lost; for ~400 ms: lost
  mark = sim::serial_log().size();
  sim::mcp_fail_next(150);
  runMs(300);
  const bool notLost = !contains(logSince(mark), "lost");
  sim::mcp_fail_next(400);
  runMs(450);
  const bool lost = contains(logSince(mark), "lost");
  runUntil([&] { return contains(logSince(mark), "MCP23017 connected"); }, 1500);
  check(notLost && lost, "lost_after_200ms_of_failures", "150 failed reads -> lost %d, 400 failed reads -> lost %d", !notLost,
        lost);
  runMs(100);

  // --- one failed I2C read while a visitor holds a pad (RUN)
  const double tHold = nowMs();
  press(padLine(1), true);
  runMs(500);
  sim::mcp_fail_next(1);
  runMs(1500);
  const auto tail = audioMs(nowMs() - 300, 300);
  const double gap = [&] {
    // longest stretch below -60 dBFS while the pad was held
    const auto x = audioMs(tHold + 300, nowMs() - tHold - 300);
    size_t run = 0, best = 0;
    for (float v : x) run = fabsf(v) < kDbfs60 ? run + 1 : 0, best = std::max(best, run);
    return best * 1000.0 / 48000;
  }();
  check(ana::rms(tail) > 0.5 * kExpectedRms035 && gap < 1, "i2c_glitch_keeps_playing",
        "one failed MCP read while pad 1 held: rms %.4f in the last 300 ms, longest silence %.0f ms, display '%s'",
        ana::rms(tail), gap, shown().c_str());
  release(padLine(1), true);
  runMs(300);

  // --- SERVICE: glitches must not re-enter the mode, cut TEST, disarm SAVE or close the editor
  slideMode(true);
  runMs(1000);
  cmd("test");
  runMs(300);
  {
    DisplayWatch dw;
    mark = sim::serial_log().size();
    sim::mcp_fail_next(2);
    runMs(300);
    const bool keeps = playing(128.5);
    check(keeps && dw.ev.size() == 1 && !contains(logSince(mark), "lost"), "glitch_keeps_test_sound",
          "2 failed reads in SERVICE with TEST playing: still playing %d, display%s", keeps, dw.trace(dw.ev[0].t).c_str());
  }
  tapButton(PIN_SAVE, 60);
  sim::mcp_fail_next(2);
  runMs(100);
  const bool armed = status().armed;
  tapButton(PIN_SAVE, 60);
  check(armed, "glitch_keeps_save_armed", "armed after 2 failed reads: %d", armed);
  openSweepEditor();
  runMs(900);
  sim::mcp_fail_next(2);
  runMs(100);
  const std::string ed = shown();
  tapButton(PIN_MUTE, 60);
  const std::string bat = shown();
  check(ed == "20.0" && bat == "BATAL", "glitch_keeps_sweep_editor", "editor shows '%s' after 2 failed reads, MUTE -> '%s'",
        ed.c_str(), bat.c_str());
  runMs(1000);
  // a real loss in SERVICE: the MODE switch cannot be read, so the device drops to RUN
  // (pads usable, nothing armed); on reconnect it follows the switch back to SERVICE, muted
  cmd("test");
  runMs(300);
  {
    DisplayWatch dw;
    mark = sim::serial_log().size();
    sim::mcp_attach(false);
    runUntil([&] { return contains(logSince(mark), "lost"); }, 500);
    runMs(100);
    const Status lostSt = status();
    const double lostRms = rmsLast(50);
    const bool silentLost = lostRms < kDbfs60;
    sim::mcp_attach(true);
    runUntil([&] { return contains(logSince(mark), "MCP23017 connected"); }, 1500);
    runMs(300);
    const Status back = status();
    check(lostSt.ok && lostSt.mode == "RUN" && silentLost && back.ok && back.mode == "SERVICE" && back.muted &&
              dw.firstAt("RUN") >= 0 && dw.firstAt("SERV") >= 0,
          "loss_in_service_drops_to_run", "lost: mode %s, muted %d, silent %d (rms %.3g); reconnected: mode %s, muted %d; display%s",
          lostSt.mode.c_str(), lostSt.muted, silentLost, lostRms, back.mode.c_str(), back.muted, dw.trace(dw.ev[0].t).c_str());
  }
  cmd("mute");
  slideMode(false);
  runMs(1000);

  // --- pad chatter
  std::string d;
  double t = nowMs();
  for (int i = 0; i < 100; i++) sim::schedule_line(now_us() + 100 + i * 2000, padLine(1), i % 2 == 0 ? 0 : 1);
  sim::schedule_line(now_us() + 100 + 100 * 2000, padLine(1), 1);
  runMs(260);
  check(nonZero(t, 260) == 0, "chatter_2ms_ignored", "pad 1 toggling every 2 ms for 200 ms: %zu non-zero frames", nonZero(t, 260));
  t = nowMs();
  std::mt19937 r2(99);
  uint64_t at = now_us() + 100;
  for (int i = 0; i < 400; i++) {
    at += 1000 + r2() % 9000;
    sim::schedule_line(at, padLine(1 + r2() % 8), int(r2() & 1));
  }
  for (int p = 1; p <= 8; p++) sim::schedule_line(at + 1000, padLine(p), 1);
  runMs((at - now_us()) / 1000.0 + 300);
  const Status st2 = status();
  const bool chatterLeds = ledsAre(-1, kWhite, &d);
  check(st2.ok && st2.pad == 0 && st2.muted && ana::peak(audioMs(nowMs() - 50, 50)) < 1e-4 && chatterLeds &&
            sim::nvs_writes() == 0,
        "random_chatter_8_pads", "%.1f s of random contacts on all pads, then all open: pad %d muted %d, silent, LEDs white, NVS writes %d",
        (nowMs() - t) / 1000, st2.pad, st2.muted, sim::nvs_writes());

  // --- clicks while changing the sound
  slideMode(true);
  slideWave("sine");
  slideHarm("odd");
  cmd("fold 0");  // the last chattered pad may have left a folded preset in the panel
  cmd("f 200");
  cmd("level 35");
  cmd("test");
  runMs(400);
  const double refStep = maxStepIn(nowMs() - 200, 200);
  const double tSweep = nowMs();
  turn(PIN_FREQ_A, PIN_FREQ_B, 60, 20);
  runMs(300);
  const double f2 = freqOf(audioMs(nowMs() - 200, 200));
  const double sweepStep = maxStepIn(tSweep, nowMs() - tSweep);
  const double peakAmp = ana::peak(audioMs(nowMs() - 100, 100));
  const double bound = peakAmp * 2 * ana::kPi * f2 / 48000;
  check(sweepStep <= bound * 1.05, "no_clicks_freq_sweep",
        "200 -> %.1f Hz by encoder: max sample step %.5f, sine at the top frequency allows %.5f (at 200 Hz: %.5f)", f2,
        sweepStep, bound, refStep);
  // wave, fold, harmonics and mute while sounding
  bool clickFree = true;
  std::string ctrace;
  for (const char* c : {"wave triangle", "fold 60", "harm both", "fold 0", "wave sine"}) {
    const double before = maxStepIn(nowMs() - 150, 150);
    const double tc = nowMs();
    cmd(c);
    cmd("test");
    runMs(300);
    const double during = maxStepIn(tc, 120);
    const double afterS = maxStepIn(nowMs() - 150, 150);
    const bool good = during <= 1.1 * std::max(before, afterS);
    clickFree = clickFree && good;
    char b[128];
    snprintf(b, sizeof(b), " [%s] %.4f vs %.4f/%.4f", c, during, before, afterS);
    ctrace += b;
  }
  check(clickFree, "no_clicks_param_changes", "max step during change vs steady before/after:%s", ctrace.c_str());
  const double sBefore = maxStepIn(nowMs() - 150, 150);
  const double tm = nowMs();
  cmd("mute");
  runMs(200);
  const double sMute = maxStepIn(tm, 200);
  const double muteTail = ana::peak(audioMs(nowMs() - 20, 20));
  check(sMute <= sBefore * 1.01 && muteTail < 1e-4, "no_click_mute",
        "max step %.5f during the fade, %.5f steady, %.0f dBFS 180 ms later", sMute, sBefore, ana::db(muteTail));
}

sim::Picture testPattern() {
  // digits 1..4 (app.js glyphs) at column m*8+1, plus the top-left pixel
  sim::Picture p{};
  for (int m = 0; m < 4; m++) {
    // the glyph's own columns from the app.js font (render_text would centre it)
    for (const AppGlyph& g : kAppFont)
      if (g.c == char('1' + m))
        for (int i = 0; i < g.width; i++) p[m * 8 + 1 + i] = g.cols[i];
  }
  p[0] |= 0x01;
  return p;
}

void scenarioDisplay() {
  boot();
  runMs(1700);
  DisplayWatch dw;
  const double t0 = nowMs();
  const std::string r = lastLine(cmd("test display"));
  const auto fb = sim::framebuffer();
  const sim::Picture want = testPattern();
  check(r == "ok" && fb == want, "test_display_1234", "digits 1-4 at columns 1/9/17/25 + top-left pixel:|%s|expected:|%s",
        sim::framebuffer_art(fb).c_str(), sim::framebuffer_art(want).c_str());
  runMs(3400);
  double patternMs = 0;
  for (size_t i = 0; i + 1 < dw.ev.size(); i++)
    if (dw.ev[i].t >= t0 && dw.ev[i + 1].text == "128.5") patternMs = dw.ev[i + 1].t - t0;
  check(sim::display_is("128.5") && patternMs >= 2995 && patternMs <= 3260, "test_display_3s",
        "pattern shown %.0f ms, then '%s'", patternMs, shown().c_str());

  // every literal that main.cpp sends to the matrix is a known message and renders like app.js
  const std::set<std::string> known = {"CP100", "SAVED", "RAM", "ERR",  "SAVE", "SINE", "SAW", "TRI", "ODD",  "EVEN",
                                       "O+E",   "RUN",   "SERV", "MUTE", "TEST", "0.1",  "1",   "MIN", "MAX", "BATAL"};
  std::set<std::string> found;
  {
    std::ifstream f(CP100_SRC_DIR "/main.cpp");
    std::string line;
    static const std::regex lit("\"([^\"]*)\"");
    while (std::getline(f, line)) {
      if (!contains(line, "flash(") && !contains(line, "displayText(") && !contains(line, "cancelSweep(") &&
          !contains(line, "Label("))
        continue;
      for (std::sregex_iterator it(line.begin(), line.end(), lit), end; it != end; ++it)
        if ((*it)[1].str().find('%') == std::string::npos) found.insert((*it)[1]);
    }
  }
  std::string unknown, bad, list;
  for (const std::string& m : found)
    if (!known.count(m)) unknown += " '" + m + "'";
  for (const std::string& m : known) {
    runMs(50);
    cmd("display " + m);
    const bool good = sim::framebuffer() == expectedPicture(m);
    const AppRender* a = appRender(m, false);
    list += " " + m + (a && a->allGlyphs ? "(app.js)" : "(+extra glyphs)");
    if (!good) bad += " '" + m + "'->'" + shown() + "'";
  }
  check(unknown.empty() && found.size() >= 15, "messages_known", "%zu literals sent to the matrix by main.cpp%s", found.size(),
        unknown.empty() ? ", all expected" : (", unexpected:" + unknown).c_str());
  check(bad.empty(), "messages_match_app_js", "pixel-exact against app.js drawMatrix():%s%s", list.c_str(),
        bad.empty() ? "" : ("; wrong:" + bad).c_str());
  // frequencies through the normal display path
  runMs(3200);
  cmd("mode service");
  runMs(1000);
  bad.clear();
  list.clear();
  for (double hz : {20.0, 128.5, 333.3, 1999.9, 2000.0}) {
    char c[32];
    snprintf(c, sizeof(c), "f %.1f", hz);
    cmd(c);
    const std::string text = freqText(hz);
    const bool good = sim::framebuffer() == expectedPicture(text, true) && appRender(text, true);
    list += " " + text;
    if (!good) bad += " '" + text + "'->'" + shown() + "'";
  }
  check(bad.empty(), "frequencies_match_app_js", "toFixed(1) pictures:%s%s", list.c_str(), bad.empty() ? "" : ("; wrong:" + bad).c_str());
  cmd("f 128.5");
  cmd("mode run");
  runMs(1200);

  // keep-alive: control registers rewritten every second
  const size_t k0 = sim::max_control_writes().size();
  runMs(5000);
  const auto& cw = sim::max_control_writes();
  double minGap = 1e9, maxGap = 0;
  for (size_t i = k0 + 1; i < cw.size(); i++) {
    const double g = (cw[i] - cw[i - 1]) / 1000.0;
    minGap = std::min(minGap, g), maxGap = std::max(maxGap, g);
  }
  check(cw.size() - k0 >= 4 && minGap >= 999 && maxGap <= 1010, "keepalive_every_1s",
        "%zu control-register refreshes in 5 s, every %.0f..%.0f ms", cw.size() - k0, minGap, maxGap);

  // fault injection: MAX7219 loses power / resets (5 V dip from the amplifier, ESD)
  sim::max_power_glitch();
  runMs(1100);
  const std::string after = shown();
  check(after == "128.5", "recovers_after_max7219_reset", "display 1.1 s after all modules reset: '%s'%s", after.c_str(),
        after.empty() ? " (blank: control registers only written in displayBegin())" : "");
  // partial corruption: one module reset, one in display-test, one with garbage control registers
  runMs(500);
  auto& chain = sim::max_chain();
  sim::max_power_glitch(1);
  chain[2].test = 1;
  chain[3].decode = 0xFF, chain[3].scanLimit = 0, chain[3].intensity = 15;
  const std::string broken = shown();
  runMs(1100);
  bool regs = true;
  for (const auto& m : chain) regs = regs && m.decode == 0 && m.scanLimit == 7 && m.shutdownReg == 1 && m.test == 0 && m.intensity == 4;
  check(broken != "128.5" && regs && sim::display_is("128.5"), "recovers_partial_corruption",
        "corrupted picture '%s' -> 1.1 s later registers restored %d, display '%s'", broken.c_str(), regs, shown().c_str());
  sim::warnings().erase(std::remove_if(sim::warnings().begin(), sim::warnings().end(),
                                       [](const std::string& w) { return contains(w, "BCD decode"); }),
                        sim::warnings().end());
}

// Brightness of one LED channel over time (1 ms grid, held between writes).
std::vector<float> ledSeries(int led, int channel, double fromMs, double toMs, double full) {
  std::vector<float> out;
  const auto& frames = sim::led_frames();
  size_t k = 0;
  double value = 0;
  for (double t = fromMs; t < toMs; t += 1) {
    while (k < frames.size() && frames[k].t_us / 1000.0 <= t) value = frames[k].rgb[led][channel], k++;
    out.push_back(float(value / full));
  }
  return out;
}

struct PulseResult {
  double hz, minRatio, maxRatio, maxJumpLsb;
  int writes;
  double minGapMs, maxGapMs;
};
PulseResult measurePulse(int pad, int channel, double base, double ms) {
  const double t0 = nowMs();
  press(padLine(pad), true);
  runMs(ms);
  const double t1 = nowMs();
  release(padLine(pad), true);
  runMs(100);
  const auto s = ledSeries(pad - 1, channel, t0 + 200, t1, base);
  PulseResult r{};
  r.minRatio = *std::min_element(s.begin(), s.end());
  r.maxRatio = *std::max_element(s.begin(), s.end());
  std::vector<float> ac(s.begin(), s.end());
  double mean = 0;
  for (float v : ac) mean += v;
  mean /= ac.size();
  for (float& v : ac) v -= float(mean);
  double best = 0;
  for (double f = 0.3; f <= 3.0; f += 0.002) {
    const double a = ana::amplitudeAt(ac, f, 1000);
    if (a > best) best = a, r.hz = f;
  }
  const auto& frames = sim::led_frames();
  r.minGapMs = 1e9;
  double prevT = -1;
  int prevV = -1;
  for (const auto& f : frames) {
    const double t = f.t_us / 1000.0;
    if (t < t0 + 200 || t > t1) continue;
    r.writes++;
    if (prevT >= 0) r.minGapMs = std::min(r.minGapMs, t - prevT), r.maxGapMs = std::max(r.maxGapMs, t - prevT);
    if (prevV >= 0) r.maxJumpLsb = std::max(r.maxJumpLsb, double(abs(f.rgb[pad - 1][channel] - prevV)));
    prevT = t, prevV = f.rgb[pad - 1][channel];
  }
  return r;
}

// First LED frame at or after tMs in which `led` is not white: its channel value.
int firstColouredValue(int led, int channel, double tMs) {
  for (const auto& f : sim::led_frames())
    if (f.t_us / 1000.0 >= tMs && !same(f.rgb[led], kWhite)) return f.rgb[led][channel];
  return -1;
}

void scenarioPulse() {
  boot();
  runMs(1700);
  // idle: an unchanged frame is resent every second
  {
    const double t0 = nowMs();
    runMs(5000);
    double minGap = 1e9, maxGap = 0, prev = -1;
    int n = 0;
    for (const auto& f : sim::led_frames()) {
      const double t = f.t_us / 1000.0;
      if (t < t0) continue;
      if (prev >= 0) minGap = std::min(minGap, t - prev), maxGap = std::max(maxGap, t - prev);
      prev = t, n++;
    }
    check(n >= 4 && minGap >= 999 && maxGap <= 1005, "idle_refresh_1s", "%d frames in 5 s idle, every %.0f..%.0f ms", n, minGap,
          maxGap);
  }
  cmd("mode service");
  const std::string r = cmd("pulse on");
  cmd("mode run");
  runMs(1100);
  // pad 2 = 196 Hz, tosca: G channel 218 * 0.35
  const double expect2 = 0.5 + (196.0 - 20) / (2000 - 20) * 1.5;
  PulseResult p = measurePulse(2, 1, 218 * kLedBrightness, 8000);
  check(contains(r, "ok") && fabs(p.hz - expect2) < 0.01 && p.minRatio > 0.80 && p.minRatio < 0.84 && p.maxRatio > 0.98 &&
            p.maxRatio < 1.01 && p.minGapMs >= 19 && p.maxGapMs <= 25,
        "pad2_pulse", "%.3f Hz (expected %.3f), brightness %.1f%%..%.1f%% of the 35%% level, %d writes, gaps %.1f..%.1f ms (20 ms frames)",
        p.hz, expect2, p.minRatio * 100, p.maxRatio * 100, p.writes, p.minGapMs, p.maxGapMs);
  std::string d;
  runMs(100);
  { const bool ledOk = ledsAre(-1, kWhite, &d); check(ledOk, "pulse_stops_on_release", "%s", d.c_str()); }
  const double expect4 = 0.5 + (384.0 - 20) / (2000 - 20) * 1.5;
  // pad 4 (index 3, odd) is tosca in the alternate scheme
  p = measurePulse(4, 1, 218 * kLedBrightness, 8000);
  check(fabs(p.hz - expect4) < 0.01 && p.minRatio > 0.80 && p.minRatio < 0.84, "pad4_pulse",
        "%.3f Hz (expected %.3f), brightness %.1f%%..%.1f%%", p.hz, expect4, p.minRatio * 100, p.maxRatio * 100);
  check(p.maxJumpLsb <= 2, "pulse_smooth", "largest step between frames %.0f LSB", p.maxJumpLsb);
  // the phase restarts for each newly pulsing pad: hold pad 2 half a cycle (darkest), then pad 4 starts at full
  runMs(500);
  press(padLine(2), true);
  runMs(1.0 / expect2 / 2 * 1000);
  const int dim = lastLeds()->rgb[1][1];
  release(padLine(2), true);
  runMs(50);
  double t = nowMs();
  press(padLine(4), true);
  runMs(100);
  const int start4 = firstColouredValue(3, 1, t);
  release(padLine(4), true);
  runMs(50);
  t = nowMs();
  press(padLine(2), true);
  runMs(100);
  const int start2 = firstColouredValue(1, 1, t);
  release(padLine(2), true);
  runMs(100);
  const int full = int(218 * kLedBrightness + .5f);
  check(dim < full * 0.86 && start4 >= full - 1 && start2 >= full - 1, "pulse_phase_restarts",
        "pad 2 released at %d/%d (mid-cycle), next press pad 4 starts at %d, pad 2 again at %d", dim, full, start4, start2);
}

// WS2812B latch: >= 280 us low between frames even when updates are urgent.
void scenarioLedLatch() {
  boot();
  runMs(1700);
  Preset presets[kPadCount];
  examplePresets(presets);
  State s = initialState(presets, kDefaultSetup);
  const int w0 = sim::rmt().writes;
  const double t0 = nowMs();
  // every call wants a new frame (a different pad lights each time); 50 us apart
  for (int i = 0; i < 400; i++) {
    s.activePad = int8_t(i % 4);
    s.muted = false;
    ledsUpdate(s, millis());
    sim::advance_us(50);
  }
  const int frames = sim::rmt().writes - w0;
  const double span = nowMs() - t0;
  const double frameUs = 8 * 24 * 1.2;
  // frames are back to back but each waits for the latch: one per (frame + latch) at most
  check(sim::rmt().resetErrors == 0 && sim::rmt().minResetUs >= 280 && frames >= int(span * 1000 / (frameUs + 400)),
        "latch_between_frames", "%d urgent frames in %.1f ms, shortest low time between frames %.0f us (WS2812B needs > 280)",
        frames, span, sim::rmt().minResetUs);
  // the last request is shown once the latch time has passed
  s.activePad = 6;
  ledsUpdate(s, millis());
  sim::advance_us(400);
  ledsUpdate(s, millis());
  std::string d;
  const bool shownLast = ledsAre(6, kPink, &d);
  check(shownLast, "latest_frame_after_latch", "%s", d.c_str());
}

// Long uptime: millis() near 2^32 (49.7 days) and the LED pulse after weeks.
void scenarioUptime() {
  const uint32_t epoch = 0xFFFFFFFFu - 4000;  // wraps 4 s after boot
  sim::set_epoch_ms(epoch);
  boot();
  runMs(1700);
  check(sim::display_is("128.5"), "splash_ends_before_wrap", "'%s'", shown().c_str());
  runMs(1000);  // t = 2.7 s
  const double t = nowMs();
  press(padLine(1), true);
  runMs(3000);  // wrap at t=4.0 s
  const auto seg = audioMs(t + 300, 2600);
  size_t quiet = 0;  // 5 ms windows with RMS below -60 dBFS
  for (size_t i = 0; i + 240 <= seg.size(); i += 240)
    quiet += ana::rms(std::vector<float>(seg.begin() + i, seg.begin() + i + 240)) < kDbfs60;
  const double tr = nowMs();
  release(padLine(1), true);
  runMs(300);
  check(quiet == 0 && lastAboveMs(tr, 300, kDbfs60) - tr < 80, "pad_across_millis_wrap",
        "pad held across the 2^32 ms wrap: %zu silent 5 ms windows, silent %.0f ms after release", quiet,
        lastAboveMs(tr, 300, kDbfs60) - tr);
  const std::string reply = lastLine(cmd("tap 2 500"));
  const double tt = nowMs();
  runMs(800);
  const double end = lastAboveMs(tt, 800, kDbfs60);
  check(reply == "playing" && end - tt > 450 && end - tt < 600, "console_tap_after_wrap", "tap 2 500 -> '%s', sounded for %.0f ms",
        reply.c_str(), end - tt);
  check(sim::display_is("196.0"), "display_after_wrap", "'%s'", shown().c_str());
  // a flash started after the wrap ends on time (millis() - flashUntil arithmetic)
  DisplayWatch dw;
  const double tc = nowMs();
  cmd("mute");
  runMs(1500);
  const double muteMs = dw.shownFor("MUTE", tc);
  check(muteMs >= 795 && muteMs <= 1060 && sim::display_is("196.0"), "flash_after_wrap", "MUTE shown %.0f ms:%s", muteMs,
        dw.trace(tc).c_str());
}

void scenarioPulseUptime() {
  sim::set_epoch_ms(3000000000u);  // 34.7 days of uptime
  boot();
  runMs(1700);
  cmd("mode service");
  cmd("pulse on");
  cmd("mode run");
  runMs(1100);
  const double expect = 0.5 + (196.0 - 20) / (2000 - 20) * 1.5;
  PulseResult p = measurePulse(2, 1, 218 * kLedBrightness, 8000);
  check(p.maxJumpLsb <= 2 && fabs(p.hz - expect) < 0.02, "pulse_smooth_after_34_days",
        "millis() = 3.0e9: %.3f Hz (expected %.3f), largest step between frames %.0f LSB (smooth pulse: <= 1), %d writes in %.0f s",
        p.hz, expect, p.maxJumpLsb, p.writes, 7.8);
}

// Energy not at multiples of the fundamental, relative to the total (dB).
double inharmonicDb(const std::vector<float>& x, double f0) {
  double total = 0, wsum = 0;  // Hann-weighted mean power: no partial-period error
  for (size_t i = 0; i < x.size(); i++) {
    const double w = 0.5 - 0.5 * cos(2 * ana::kPi * i / (x.size() - 1));
    total += w * double(x[i]) * x[i];
    wsum += w;
  }
  total /= wsum;
  double harmonic = 0;
  for (int h = 1; h * f0 < 23900; h++) {
    const double a = ana::amplitudeAt(x, h * f0, 48000);
    harmonic += a * a / 2;
  }
  return 10 * log10(std::max(total - harmonic, total * 1e-12) / total);
}

// Negative controls: the measurement code must catch the faults it is meant to catch.
// Fixes from the second review round, each pinned by a check.
void scenarioRound2() {
  // --- console: a refused 'down' must not cancel a running tap
  boot();
  runMs(1700);
  const std::string rt = lastLine(cmd("tap 1 1000"));
  runMs(300);
  const std::string rd = lastLine(cmd("down 2"));
  const double t = nowMs();
  runMs(1500);
  const double end = lastAboveMs(t - 300, 1800, kDbfs60) - (t - 300);
  press(padLine(3), true);
  runMs(300);
  const bool pad3 = playing(256);
  release(padLine(3), true);
  runMs(300);
  check(rt == "playing" && rd == "ignored: another pad is held" && end > 950 && end < 1100 && pad3, "tap_survives_refused_down",
        "tap 1 1000 -> '%s', down 2 -> '%s', tap sounded %.0f ms, physical pad 3 afterwards plays %d", rt.c_str(), rd.c_str(),
        end, pad3);

  // --- console mute/test flash like the TEST-MUTE button
  cmd("mode service");
  runMs(1100);
  cmd("test");
  const std::string ft = shown();
  runMs(300);
  cmd("mute");
  const std::string fm = shown();
  runMs(1000);
  check(ft == "TEST" && fm == "MUTE", "console_mute_test_flash", "test -> '%s', mute -> '%s'", ft.c_str(), fm.c_str());

  // --- sweep editor: console test/sweep refused, status shows the editor
  openSweepEditor();
  const std::string rtest = lastLine(cmd("test"));
  const std::string rsweep = lastLine(cmd("sweep 100 500"));
  runMs(300);
  const bool quiet = rmsLast(250) < 1e-5;
  Status st = status();
  const bool editorLine = contains(st.raw, "editor open: MIN 20.0 Hz, MAX 2000.0 Hz, editing MIN");
  tapButton(PIN_MUTE, 60);
  runMs(1000);
  st = status();
  check(rtest == "ignored: sweep editor is open" && rsweep == "ignored: sweep editor is open" && quiet && editorLine &&
            st.ok && st.muted && st.sweepMin == 20 && st.sweepMax == 2000 && !contains(st.raw, "editor open"),
        "console_respects_sweep_editor", "test -> '%s', sweep -> '%s', silent %d, status editor line %d; after BATAL muted %d, limits %.1f-%.1f",
        rtest.c_str(), rsweep.c_str(), quiet, editorLine, st.muted, st.sweepMin, st.sweepMax);

  // --- FREQ push held through the editor's SAVE must not reopen it
  openSweepEditor();
  turn(PIN_FREQ_A, PIN_FREQ_B, 3, 80);
  press(PIN_FREQ_PUSH, true);
  runMs(2300);
  tapButton(PIN_SAVE, 60);
  runMs(400);
  const std::string afterSave = shown();
  runMs(2500);
  const std::string later = shown();
  release(PIN_FREQ_PUSH, true);
  runMs(300);
  st = status();
  check(afterSave == "SAVED" && later != "MIN" && st.ok && st.step == "1" && !contains(st.raw, "editor open") && st.sweepMin == 23,
        "held_push_does_not_reopen_editor", "after SAVE '%s', 2.5 s later '%s', step %s, limits %.1f-%.1f", afterSave.c_str(),
        later.c_str(), st.step.c_str(), st.sweepMin, st.sweepMax);

  // --- FREQ held from RUN across a mode change does not open the editor
  cmd("mode run");
  runMs(1100);
  press(PIN_FREQ_PUSH, true);
  runMs(2500);
  cmd("mode service");
  runMs(500);
  const std::string modeShown = shown();
  release(PIN_FREQ_PUSH, true);
  runMs(300);
  st = status();
  check(modeShown == "SERV" && st.ok && !contains(st.raw, "editor open") && st.step == "1", "held_push_across_mode_change",
        "display '%s', editor open %d, step %s", modeShown.c_str(), contains(st.raw, "editor open"), st.step.c_str());
  cmd("mode run");
  runMs(1100);

  // --- a roll from pad 3 to pad 2 inside one loop pass plays pad 2 (releases first)
  press(padLine(3));
  runMs(300);
  release(padLine(3));
  press(padLine(2));
  runMs(300);
  const bool rolled = playing(196);
  release(padLine(2));
  runMs(300);
  check(rolled, "same_pass_roll_plays_new_pad", "pad 3 released and pad 2 pressed at the same instant: pad 2 plays %d", rolled);
}

// SERVICE pad press with the panel: a preset whose WAVE/HARMONICS differ from the
// slides shows them; the panel lost in SERVICE leaves the pads usable.
void scenarioRound2Panel() {
  attachPanel(true);  // slides SINE / ODD
  boot();
  runMs(2000);
  {
    DisplayWatch dw;
    press(padLine(3), true);  // sawtooth / both
    runMs(200);
    const std::string hint = shown();
    release(padLine(3), true);
    runMs(1200);
    press(padLine(2), true);  // triangle / odd
    runMs(200);
    const std::string hint2 = shown();
    release(padLine(2), true);
    runMs(1200);
    press(padLine(1), true);  // sine / odd = slides
    runMs(200);
    const std::string same = shown();
    release(padLine(1), true);
    runMs(1200);
    check(hint == "SAW" && hint2 == "TRI" && same == "128.5", "service_pad_shows_preset_wave",
          "slides SINE/ODD: pad 3 -> '%s', pad 2 -> '%s', pad 1 -> '%s'", hint.c_str(), hint2.c_str(), same.c_str());
  }
  // armed SAVE + panel lost + MODE slid to RUN: a visitor press must not overwrite a preset
  tapButton(PIN_SAVE, 60);
  const int nvs0 = sim::nvs_writes();
  sim::mcp_attach(false);
  runMs(400);
  setContact(MCP_MODE, 1);
  runMs(100);
  press(padLine(2), true);
  runMs(300);
  const bool plays = playing(196);
  release(padLine(2), true);
  runMs(300);
  const Status st = status();
  check(st.ok && st.mode == "RUN" && !st.armed && plays && sim::nvs_writes() == nvs0 && st.pads[1].hz == 196 &&
            st.pads[1].wave == "triangle",
        "loss_while_armed_keeps_presets", "mode %s, armed %d, pad 2 plays 196 Hz %d, NVS +%d, pad 2 %.1f Hz %s", st.mode.c_str(),
        st.armed, plays, sim::nvs_writes() - nvs0, st.pads[1].hz, st.pads[1].wave.c_str());
}

void scenarioSelftest() {
  // frequency estimator and harmonic analysis on synthetic signals
  std::vector<float> x(24000), saw(24000), jump(24000);
  for (size_t i = 0; i < x.size(); i++) {
    const double t = i / 48000.0;
    x[i] = float(0.1 * sin(2 * ana::kPi * 173.1 * t) + 0.03 * sin(2 * ana::kPi * 3 * 173.1 * t));
    saw[i] = float(0.1 * (2 * fmod(256.0 * t, 1.0) - 1));
    jump[i] = float(0.1 * sin(2 * ana::kPi * 200 * t + (i > 12000 ? 1.0 : 0.0)));
  }
  const double f1 = freqOf(x), f2 = freqOf(saw);
  const auto h = ana::harmonics(x, f1, 48000, 3);
  check(fabs(f1 - 173.1) < 0.005 && fabs(f2 - 256) < 0.01 && fabs(h[0] - 0.1) < 1e-4 && fabs(h[2] - 0.03) < 1e-4 && h[1] < 1e-5,
        "estimators", "sine+3rd: %.4f Hz, h1 %.5f h2 %.1e h3 %.5f; saw 256: %.4f Hz", f1, h[0], h[1], h[2], f2);
  const double bound = 0.1 * 2 * ana::kPi * 200 / 48000;
  check(ana::maxStep(jump) > 3 * bound, "click_detector", "1 rad phase jump: max step %.4f vs smooth bound %.4f",
        ana::maxStep(jump), bound);

  // inharmonic-energy meter: band-limited saw (clean) and with a tone 30 dB down
  std::vector<float> bl(12000), bl2(12000);
  const double fb = 1013.7;
  for (size_t i = 0; i < bl.size(); i++) {
    double v = 0;
    for (int k = 1; k * fb < 23900; k++) v += 0.1 / k * sin(2 * ana::kPi * k * fb * i / 48000);
    bl[i] = float(v);
    bl2[i] = float(v + 0.1 * 1.2825 * 0.0316 * sin(2 * ana::kPi * 1517.3 * i / 48000));
  }
  const double clean = inharmonicDb(bl, freqOf(bl)), dirty = inharmonicDb(bl2, freqOf(bl2));
  check(clean < -60 && fabs(dirty + 30) < 1.5, "inharmonic_meter", "band-limited saw %.1f dB, plus a -30 dB tone %.1f dB", clean,
        dirty);

  // the harness renderer reproduces app.js drawMatrix() (ref/font.h) for every text
  int same = 0, total = 0;
  std::string diff;
  for (const AppRender& r : kAppRenders) {
    sim::Picture want{};
    for (int i = 0; i < 32; i++) want[i] = r.cols[i];
    total++;
    const bool good = sim::render_text(r.text, false) == want && (!r.allGlyphs || sim::render_text(r.text, true) == want);
    if (good) same++;
    else diff += std::string(" '") + r.text + "'";
  }
  check(same == total && total > 60, "renderer_matches_app_js", "%d/%d app.js drawMatrix() pictures reproduced%s", same, total,
        diff.c_str());
  // decoder: exact for centred text, '?' for anything else
  bool round = true;
  std::string rtrace;
  for (const char* m : {"CP100", "SAVED", "BATAL", "MAX", "F 018", "L 100", "O+E", "0.1", "1", "128.5", "2000.0", "1 2 3 4"}) {
    const std::string got = sim::decode_text(sim::render_text(m));
    if (got != m) round = false, rtrace += std::string(" '") + m + "'->'" + got + "'";
  }
  sim::Picture shifted{};
  const sim::Picture save = sim::render_text("SAVE");
  for (int i = 0; i < 31; i++) shifted[i + 1] = save[i];
  const std::string off = sim::decode_text(shifted);
  sim::Picture oldFont = sim::render_text("SAVE");
  oldFont[sim::text_width("SAVE") / 2] ^= 0x10;  // one wrong pixel
  const std::string wrong = sim::decode_text(oldFont);
  check(round && off[0] == '?' && wrong[0] == '?', "decoder", "round trips%s; off-centre SAVE -> '%s'; one pixel wrong -> '%s'",
        round ? " ok" : rtrace.c_str(), off.c_str(), wrong.c_str());

  // display decoder rejects a wrong module order / mirrored module
  boot();
  runMs(1700);
  auto& chain = sim::max_chain();
  const auto saved = chain;
  std::swap(chain[0], chain[3]);
  std::swap(chain[1], chain[2]);
  const std::string reversed = shown();
  chain = saved;
  for (auto& m : chain)
    for (auto& row : m.digit) row = uint8_t(((row * 0x0802LU & 0x22110LU) | (row * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16);
  const std::string mirrored = shown();
  chain = saved;
  check(reversed != "128.5" && mirrored != "128.5" && shown() == "128.5", "display_decoder",
        "modules reversed -> '%s', columns mirrored -> '%s', original -> '%s'", reversed.c_str(), mirrored.c_str(),
        shown().c_str());

  // DMA backpressure: the audio thread is always exactly 4 buffers ahead of the DAC
  const size_t ahead = sim::audio_left().size() - sim::frame_at(now_us());
  check(ahead > 3 * 128 && ahead <= 4 * 128, "dma_backpressure", "%zu frames queued ahead of the DAC (4 x 128 max)", ahead);

  // NVS-write monitor: a write while sound plays (no fade) is caught
  cmd("mode service");
  cmd("test");
  runMs(400);
  {
    Preferences p;
    p.begin("selftest", false);
    const uint8_t v = 1;
    p.putBytes("x", &v, 1);
    p.end();
  }
  const sim::NvsWrite& w = sim::nvs_log().back();
  const double loopPeak = ana::peak(audioMs(w.t_us / 1000.0, w.stallUs / 1000.0));
  check(w.ringPeak > 0.05 && loopPeak > 0.05, "nvs_write_monitor",
        "unfaded write while TEST plays: looping DMA buffers peak %.1f dBFS, DAC during the %u ms stall %.1f dBFS",
        ana::db(w.ringPeak), unsigned(w.stallUs / 1000), ana::db(loopPeak));
  cmd("mute");
  runMs(200);

  // WS2812B timing validator flags a T0H of 1 us
  rmt_data_t bad[24];
  for (auto& b : bad) b.level0 = 1, b.duration0 = 10, b.level1 = 0, b.duration1 = 3;
  const int before = sim::rmt().timingErrors;
  runMs(2);
  rmtWriteBlocking(nullptr, bad, 24);
  check(sim::rmt().timingErrors > before, "ws2812_validator", "%d timing errors for 1.0/0.3 us bits", sim::rmt().timingErrors - before);
  // the invariants below must not count these deliberate faults
  sim::nvs_set_fail(false);
}

// Fingerprint of everything the board emitted; run twice to prove determinism.
void scenarioFingerprint() {
  sim::mcp_attach(true);
  sim::set_line(MCP_MODE, 1);
  boot();
  runMs(1700);
  press(padLine(3), true);
  runMs(400);
  turn(PIN_FREQ_A, PIN_FREQ_B, 3, 20, true);
  release(padLine(3), true);
  cmd("status");
  runMs(300);
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
  for (int32_t v : sim::audio_left()) mix(uint32_t(v));
  for (const auto& f : sim::led_frames()) mix(f.t_us), mix(f.rgb[2][1]);
  for (char c : sim::serial_log()) mix(uint8_t(c));
  printf("FINGERPRINT %016llx %zu frames\n", (unsigned long long)h, sim::audio_left().size());
}

// Headroom and aliasing of the synth over the whole parameter space.
void scenarioDsp() {
  double worst = 0;
  std::string at;
  for (int w = 0; w < 3; w++)
    for (int hm = 0; hm < 3; hm++)
      for (int fold = 0; fold <= 100; fold++) {
        const Calibration c = calibrationFor(Wave(w), fold, Harmonics(hm));
        const Calibration l = calibrationLookup(Wave(w), fold, Harmonics(hm));
        if (fabs(c.gain - l.gain) > 1e-5 * c.gain || fabs(c.mean - l.mean) > 1e-6) at = "table differs from calibrationFor";
        for (int i = 0; i < 4096; i++) {
          const double v = fabs((foldSample(oscillatorSample(i / 4096.f, Wave(w)), fold, Harmonics(hm)) - c.mean) * c.gain);
          if (v > worst) {
            char b[96];
            snprintf(b, sizeof(b), "%s/%s/fold %d", waveName(Wave(w)), harmonicsName(Harmonics(hm)), fold);
            worst = v, at = b;
          }
        }
      }
  gHarnessCalibrationCalls = sim::calibration_for_calls();
  check(worst < 1.0, "level100_headroom", "largest peak at level 100%%: %.3f of the +/-1 clamp (%s); clamp never clips", worst,
        at.c_str());

  // inharmonic (aliased) energy of the 4x oversampled folder, relative to the signal
  boot();
  runMs(1700);
  cmd("mode service");
  struct Case {
    double hz;
    int fold;
    const char* wave;
    const char* harm;
  } cases[] = {{196, 18, "triangle", "odd"}, {256, 32, "saw", "both"},  {383, 58, "sine", "both"},
               {517, 0, "saw", "odd"},       {1013, 0, "saw", "odd"}, {1000, 58, "sine", "both"}, {2000, 100, "sine", "both"}};
  std::string trace;
  for (const Case& k : cases) {
    char c[64];
    snprintf(c, sizeof(c), "f %.1f", k.hz);
    cmd(c);
    snprintf(c, sizeof(c), "fold %d", k.fold);
    cmd(c);
    cmd(std::string("wave ") + k.wave);
    cmd(std::string("harm ") + k.harm);
    cmd("level 100");
    cmd("test");
    runMs(500);
    const auto x = audioMs(nowMs() - 250, 250);
    const double f0 = freqOf(x, k.hz * 0.9, k.hz * 1.1);
    char b[128];
    snprintf(b, sizeof(b), " [%s %.0f Hz fold %d %s] %.1f dB", k.wave, k.hz, k.fold, k.harm, inharmonicDb(x, f0));
    trace += b;
    cmd("mute");
    runMs(100);
  }
  info("inharmonic (alias) energy relative to the total:%s", trace.c_str());
  check(true, "alias_report", "inharmonic energy:%s", trace.c_str());
}

// Checked at the end of every scenario.
void invariants() {
  std::string why;
  const sim::RmtInfo& r = sim::rmt();
  if (r.resetErrors || r.levelErrors || r.lengthErrors || (r.timingErrors && strcmp(gScenario, "selftest")))
    why += " WS2812B errors (reset " + std::to_string(r.resetErrors) + ", timing " + std::to_string(r.timingErrors) + ");";
  const sim::SpiInfo& s = sim::spi();
  if (s.badFrames || s.transfersWithCsHigh || s.transfersOutsideTransaction) why += " MAX7219 SPI framing errors;";
  const sim::I2sInfo& i = sim::i2s();
  if (i.badWrites) why += " " + std::to_string(i.badWrites) + " bad i2s_write calls;";
  if (i.timeouts && !gI2sHaltExpected) why += " " + std::to_string(i.timeouts) + " i2s_write timeouts;";
  if (sim::calibration_for_calls() != gHarnessCalibrationCalls)
    why += " calibrationFor() called " + std::to_string(sim::calibration_for_calls() - gHarnessCalibrationCalls) +
           " times at runtime;";
  double ring = 0;
  int loud = 0;
  for (size_t k = 0; k < sim::nvs_log().size(); k++) {
    const sim::NvsWrite& w = sim::nvs_log()[k];
    if (!strcmp(gScenario, "selftest") && k + 1 == sim::nvs_log().size()) continue;  // the deliberate one
    ring = std::max(ring, w.ringPeak);
    loud += w.ringPeak > kNvsRingMax;
  }
  if (loud) why += " " + std::to_string(loud) + " NVS write(s) with sound in the DMA buffers;";
  for (const auto& w : sim::warnings()) why += " warning: " + w + ";";
  check(why.empty(), "invariants",
        "%s%zu LED frames, %d SPI frames, %d i2s_write, %zu NVS writes (loudest DMA loop %.0f dBFS), %d calibrationFor() at runtime",
        why.empty() ? "" : (why + " | ").c_str(), sim::led_frames().size(), s.frames, i.writes, sim::nvs_log().size(),
        ana::db(ring), sim::calibration_for_calls() - gHarnessCalibrationCalls);
}

struct Scenario {
  const char* name;
  void (*fn)();
  const char* what;
};
const Scenario kScenarios[] = {
    {"boot", scenarioBoot, "1 boot: banner, splash, silence, LEDs, bus setup, audio task"},
    {"pad", scenarioPad, "2 hold/release pad 1"},
    {"priority", scenarioPriority, "3 first pad wins, empty pads, MUTE, TEST refused"},
    {"owner", scenarioOwner, "3b pad ownership (heldSource) across pads and console"},
    {"harmonics", scenarioHarmonics, "4 example presets vs simulator period()"},
    {"console", scenarioConsole, "5 console SERVICE flow"},
    {"panel", scenarioPanel, "6 physical SERVICE flow through the MCP23017"},
    {"flash", scenarioFlash, "6b matrix messages, durations, armed SAVE cycle"},
    {"sweep", scenarioSweep, "7 sweep editor"},
    {"nvs", scenarioNvs, "7b NVS writes: DMA silence and click-free fade on every path"},
    {"persist1", scenarioPersist1, "8a store presets and setup"},
    {"persist2", scenarioPersist2, "8b read them back in a new process"},
    {"persist3", scenarioPersist3, "8c NVS blob with another layout"},
    {"robust", scenarioRobust, "9 robustness: console abuse, I2C glitches, chatter, clicks"},
    {"display", scenarioDisplay, "10 display: test pattern, app.js pictures, keep-alive"},
    {"pulse", scenarioPulse, "11 LED pulse, refresh, phase restart"},
    {"ledlatch", scenarioLedLatch, "11b WS2812B latch time between frames"},
    {"uptime", scenarioUptime, "12 millis() wrap"},
    {"pulse_uptime", scenarioPulseUptime, "12b LED pulse after 34.7 days"},
    {"loopgap", scenarioLoopgap, "13 fast encoder spins never block loop()"},
    {"txbuf", scenarioTxbuf, "14 long console replies do not stall loop()"},
    {"i2s_stall", scenarioI2sStall, "15 I2S stall counter"},
    {"selftest", scenarioSelftest, "harness negative controls"},
    {"dsp", scenarioDsp, "headroom at level 100 and alias energy"},
    {"round2", scenarioRound2, "second review round: console timer, editor rules, held FREQ push, pad roll"},
    {"round2_panel", scenarioRound2Panel, "second review round: SERVICE preset hint, panel loss while armed"},
    {"fingerprint", scenarioFingerprint, "hash of all outputs (determinism)"},
};

}  // namespace

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IOLBF, 0);
  if (argc < 2 || !strcmp(argv[1], "list")) {
    for (const auto& s : kScenarios) printf("%-13s %s\n", s.name, s.what);
    sim::finish(argc < 2 ? 2 : 0);
  }
  for (const auto& s : kScenarios) {
    if (strcmp(s.name, argv[1])) continue;
    gScenario = s.name;
    s.fn();
    if (strcmp(s.name, "fingerprint")) invariants();
    sim::finish(gFailures ? 1 : 0);
  }
  fprintf(stderr, "unknown scenario %s\n", argv[1]);
  sim::finish(2);
}
