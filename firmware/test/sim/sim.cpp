// Simulated board behind the fake Arduino layer. See sim.h.

#include "sim.h"

#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <thread>

#include "dsp.h"
#include "pins.h"
#include "ref/font.h"

namespace sim {
namespace {

// ------------------------------------------------------------ clock + audio sync
std::mutex mtx;  // guards now, audio queue and capture
std::condition_variable cv;
uint64_t nowUs = 0;
uint32_t epochMs = 0;
int calibrationCalls = 0;

I2sInfo i2sInfo;
bool audioRunning = false;
bool audioBlocked = false;
uint64_t audioDeadline = UINT64_MAX;  // when the parked i2s_write() call times out
bool i2sHalted = false;
uint64_t haltedConsumed = 0;
bool flashBusy = false;  // NVS write in progress: audio task frozen, DMA loops its buffers
uint64_t t0Us = 0;
uint64_t buffersWritten = 0;
std::vector<int32_t> capL, capR;

uint64_t consumedLocked() {
  if (!i2sInfo.installed) return 0;
  if (i2sHalted) return haltedConsumed;
  const uint64_t frames = (nowUs - t0Us) * kFs / 1000000;
  return frames / kDmaFrames;
}
bool queueFullLocked() { return buffersWritten >= consumedLocked() + kDmaBuffers; }
// The audio thread sits in i2s_write() and has nothing to do until time moves on.
bool audioParkedLocked() { return audioBlocked && queueFullLocked() && nowUs < audioDeadline; }
uint64_t underruns = 0;

[[noreturn]] void harnessFatal(const char* what) {
  fprintf(stdout, "CHECK FAIL harness/fatal: %s\n", what);
  finish(3);
}

// ------------------------------------------------------------ lines
constexpr int kPins = 49;
int contact[kPins + 8 + kMcpLine];  // 1 = open
int modes[kPins];
int outputs[kPins];
std::set<int> floatWarned;
struct Event {
  uint64_t at;
  uint64_t seq;
  int line, level;
  bool operator<(const Event& o) const { return at != o.at ? at < o.at : seq < o.seq; }
};
std::multiset<Event> schedule;
uint64_t scheduleSeq = 0;
std::vector<std::string> warningList;

void warn(const std::string& w) {
  for (const auto& x : warningList)
    if (x == w) return;
  warningList.push_back(w);
}

// Rev08: tactiles have 10k pull-ups to 3V3; GPIO41 (matrix CS) 10k pull-up.
bool externalPullup(int pin) {
  for (uint8_t p : PIN_PADS)
    if (p == pin) return true;
  return pin == PIN_SAVE || pin == PIN_MUTE || pin == PIN_MATRIX_CS;
}

void maxLatch();
void maxCsFalling();

void applyEventsUpTo(uint64_t t) {
  while (!schedule.empty() && schedule.begin()->at <= t) {
    const Event e = *schedule.begin();
    schedule.erase(schedule.begin());
    contact[e.line] = e.level;
  }
}

// ------------------------------------------------------------ UART
constexpr double kUartByteUs = 1e6 * 10 / 115200;  // 8N1
constexpr int kUartFifo = 128;                     // hardware FIFO
size_t txBufferSize = 0;                           // driver ring buffer: 0 unless setTxBufferSize() before begin()
bool serialBegun = false;
bool loopWdtEnabled = false;
constexpr int kUartRxBuffer = 256;
std::string txLog;
double txDoneAt = 0;
uint64_t txBlockedUs = 0;
std::deque<std::pair<double, char>> rx;
double rxLast = 0;
bool echo = false;

void uartWrite(const char* data, size_t n) {
  txLog.append(data, n);
  if (echo) fwrite(data, 1, n, stderr);
  const double start = std::max(double(nowUs), txDoneAt);
  txDoneAt = start + n * kUartByteUs;
  const double returnAt = txDoneAt - double(kUartFifo + txBufferSize) * kUartByteUs;
  if (returnAt > double(nowUs)) {
    const uint64_t wait = uint64_t(returnAt - double(nowUs) + 0.999);
    txBlockedUs += wait;
    advance_us(wait);
  }
}

// ------------------------------------------------------------ RMT / WS2812B
RmtInfo rmtInfo;
std::vector<LedFrame> leds;
uint64_t lastLedEndUs = 0;
bool ledWritten = false;

// ------------------------------------------------------------ SPI / MAX7219
SpiInfo spiInfo;
bool inTransaction = false;
std::array<Max7219, 4> chain;
uint64_t shiftReg = 0;
int bitsSinceFall = 0;
std::vector<uint64_t> ctrlWrites;

void maxCsFalling() { bitsSinceFall = 0; }

void maxLatch() {
  spiInfo.frames++;
  if (bitsSinceFall != 64) spiInfo.badFrames++;
  int shutdownWrites = 0;
  for (int k = 0; k < 4; k++) {
    const uint16_t word = uint16_t(shiftReg >> (16 * k));
    const int addr = (word >> 8) & 0x0F;
    const uint8_t data = word & 0xFF;
    Max7219& m = chain[k];
    if (addr >= 1 && addr <= 8) m.digit[addr - 1] = data;
    else if (addr == 0x9) m.decode = data;
    else if (addr == 0xA) m.intensity = data & 0x0F;
    else if (addr == 0xB) m.scanLimit = data & 0x07;
    else if (addr == 0xC) m.shutdownReg = data & 1, shutdownWrites++;
    else if (addr == 0xF) m.test = data & 1;
  }
  if (bitsSinceFall == 64 && shutdownWrites == 4) ctrlWrites.push_back(nowUs);
  bitsSinceFall = 0;
}

// ------------------------------------------------------------ app.js font
struct Glyph {
  char c;
  int width;
  uint8_t cols[5];
};
// The harness's own classic 5x7 glyphs for characters the app.js font does
// not have (bit 0 = top row). Not taken from display.cpp.
const Glyph kExtraGlyphs[] = {
    {'B', 5, {0x7F, 0x49, 0x49, 0x49, 0x36}}, {'C', 5, {0x3E, 0x41, 0x41, 0x41, 0x22}},
    {'G', 5, {0x3E, 0x41, 0x49, 0x49, 0x7A}}, {'H', 5, {0x7F, 0x08, 0x08, 0x08, 0x7F}},
    {'J', 5, {0x20, 0x40, 0x41, 0x3F, 0x01}}, {'K', 5, {0x7F, 0x08, 0x14, 0x22, 0x41}},
    {'Q', 5, {0x3E, 0x41, 0x51, 0x21, 0x5E}}, {'X', 5, {0x63, 0x14, 0x08, 0x14, 0x63}},
    {'Z', 5, {0x61, 0x51, 0x49, 0x45, 0x43}}, {'-', 3, {0x08, 0x08, 0x08}},
    {':', 1, {0x24}},
};

const Glyph* appGlyph(char c) {
  static std::vector<Glyph> font = [] {
    std::vector<Glyph> f;
    for (const AppGlyph& g : kAppFont) f.push_back({g.c, g.width, {g.cols[0], g.cols[1], g.cols[2], g.cols[3], g.cols[4]}});
    return f;
  }();
  for (const Glyph& g : font)
    if (g.c == c) return &g;
  return nullptr;
}
const Glyph* extraGlyph(char c) {
  for (const Glyph& g : kExtraGlyphs)
    if (g.c == c) return &g;
  return nullptr;
}
const Glyph& glyphFor(char c, bool extras) {
  if (c >= 'a' && c <= 'z') c = char(c - 32);
  if (const Glyph* g = appGlyph(c)) return *g;
  if (extras)
    if (const Glyph* g = extraGlyph(c)) return *g;
  return *appGlyph(' ');  // app.js: fonts[char] || fonts[' ']
}

// ------------------------------------------------------------ I2C / MCP23017
I2cInfo i2cInfo;
bool mcpPresent = false;
int mcpFailNext = 0;
uint8_t mcpRegs[0x16];
uint8_t mcpPointer = 0;
int i2cCount = 0;

// Address phase: ACK only from a present MCP23017 that is not glitching.
bool mcpAck(uint16_t address) {
  if (!mcpPresent || address != MCP23017_ADDR) return false;
  if (mcpFailNext > 0) {
    mcpFailNext--;
    return false;
  }
  return true;
}

void mcpReset() {
  memset(mcpRegs, 0, sizeof(mcpRegs));
  mcpRegs[0x00] = 0xFF;  // IODIRA
  mcpRegs[0x01] = 0xFF;  // IODIRB
  mcpPointer = 0;
}

uint8_t mcpPinLevels() {
  uint8_t v = 0;
  for (int b = 0; b < 8; b++)
    if (contact[kMcpLine + b]) v |= 1 << b;  // Rev08: every GPA input has a 10k pull-up
  return v;
}

uint8_t mcpRead(uint8_t reg) {
  if (reg == 0x12) {  // GPIOA
    const uint8_t dir = mcpRegs[0x00];
    const uint8_t in = mcpPinLevels() ^ mcpRegs[0x02];
    return (in & dir) | (mcpRegs[0x14] & ~dir);
  }
  if (reg == 0x13) return (0xFF & mcpRegs[0x01]) | (mcpRegs[0x15] & ~mcpRegs[0x01]);
  return reg < sizeof(mcpRegs) ? mcpRegs[reg] : 0;
}

void mcpWrite(uint8_t reg, uint8_t v) {
  if (reg >= sizeof(mcpRegs)) return;
  if (reg == 0x12) reg = 0x14;  // writing GPIO writes OLAT
  if (reg == 0x13) reg = 0x15;
  if (reg == 0x0A || reg == 0x0B) {
    mcpRegs[0x0A] = mcpRegs[0x0B] = v;
    if (v & 0x80) warn("MCP23017: firmware set IOCON.BANK=1 (model only supports BANK=0)");
    return;
  }
  if (reg == 0x0E || reg == 0x0F || reg == 0x10 || reg == 0x11) return;  // read-only
  mcpRegs[reg] = v;
}

void mcpAdvancePointer() {
  if (mcpRegs[0x0A] & 0x20) return;  // SEQOP=1: no increment (BANK=0 toggles A/B, not modelled)
  mcpPointer = uint8_t((mcpPointer + 1) % 0x16);
}

void i2cSpend(int bits) { advance_us(uint64_t(bits * 1e6 / (i2cInfo.freq ? i2cInfo.freq : 100000) + 0.5)); }

// ------------------------------------------------------------ NVS
std::map<std::string, std::vector<uint8_t>> nvs;
int nvsWriteCount = 0;
std::vector<NvsWrite> nvsLog;
bool nvsFail = false;
uint32_t nvsStallUs = 20000;
const char* nvsPath() { return getenv("CP100_NVS"); }

void nvsLoad() {
  nvs.clear();
  const char* path = nvsPath();
  if (!path) return;
  std::ifstream f(path);
  std::string ns, key, hex;
  while (f >> ns >> key >> hex) {
    std::vector<uint8_t> v;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) v.push_back(uint8_t(strtoul(hex.substr(i, 2).c_str(), nullptr, 16)));
    nvs[ns + "/" + key] = v;
  }
}

void nvsSave() {
  const char* path = nvsPath();
  if (!path) return;
  std::ofstream f(path, std::ios::trunc);
  for (const auto& kv : nvs) {
    const size_t slash = kv.first.find('/');
    f << kv.first.substr(0, slash) << ' ' << kv.first.substr(slash + 1) << ' ';
    char b[3];
    for (uint8_t x : kv.second) snprintf(b, sizeof(b), "%02x", x), f << b;
    f << '\n';
  }
}

// A flash write: the cache is disabled, the audio task (core 0) is frozen and
// the I2S ISR (not in IRAM) cannot run, so the DMA descriptors, a closed ring,
// keep replaying the 4 buffers they hold. Records their peak, appends the
// replay to the capture and blocks the caller for the write time.
NvsWrite flashWrite(const char* key) {
  NvsWrite w{nowUs, key, false, 0, nvsStallUs};
  {
    std::lock_guard<std::mutex> lk(mtx);
    const size_t ring = size_t(kDmaBuffers) * kDmaFrames, n = capL.size();
    for (size_t i = n >= ring ? n - ring : 0; i < n; i++)
      w.ringPeak = std::max(w.ringPeak, fabs(capL[i] / 2147483648.0));
    if (audioRunning && !i2sHalted && n >= ring && nvsStallUs) {
      const uint64_t endFrames = (nowUs + nvsStallUs - t0Us) * kFs / 1000000;
      // The DMA plays on through the stall; afterwards the queue is "full" of stale buffers again.
      while (buffersWritten < endFrames / kDmaFrames + kDmaBuffers) {
        for (int i = 0; i < kDmaFrames; i++) {
          const size_t at = capL.size() - ring;
          capL.push_back(capL[at]);
          capR.push_back(capR[at]);
        }
        buffersWritten++;
      }
    }
    flashBusy = true;
  }
  advance_us(nvsStallUs);
  std::lock_guard<std::mutex> lk(mtx);
  flashBusy = false;
  return w;
}

struct Init {
  Init() {
    for (auto& c : contact) c = 1;
    for (auto& m : modes) m = -1;
    for (auto& o : outputs) o = -1;
    mcpReset();
    echo = getenv("CP100_ECHO") != nullptr;
    if (const char* c = getenv("CP100_NVS_STALL_US")) nvsStallUs = uint32_t(atoi(c));
    capL.reserve(48000 * 30);
    capR.reserve(48000 * 30);
  }
} init;

}  // namespace

// ================================================================= public API
uint64_t now_us() { return nowUs; }
void set_epoch_ms(uint32_t ms) { epochMs = ms; }

void advance_us(uint64_t us) {
  if (us == 0) return;
  std::unique_lock<std::mutex> lk(mtx);
  const uint64_t target = nowUs + us;
  // Step at most one DMA buffer (or to the audio task's i2s_write timeout) at a
  // time: on the chip the audio task (priority 20) preempts whatever loop() is
  // blocked in, so it never falls behind.
  while (nowUs < target) {
    uint64_t step = target;
    if (audioRunning && !flashBusy) {
      uint64_t next = audioDeadline;
      if (!i2sHalted)
        next = std::min<uint64_t>(next, t0Us + ((consumedLocked() + 1) * kDmaFrames * 1000000ull + kFs - 1) / kFs);
      step = std::min(target, std::max(next, nowUs + 1));
    }
    nowUs = step;
    applyEventsUpTo(nowUs);
    if (!audioRunning || flashBusy) continue;
    if (!i2sHalted && buffersWritten < consumedLocked()) underruns++;
    if (audioParkedLocked()) continue;  // the DMA did not free a buffer yet
    cv.notify_all();
    if (!cv.wait_for(lk, std::chrono::seconds(20), [] { return audioParkedLocked(); }))
      harnessFatal("audio task never blocked again in i2s_write (hang or busy loop)");
  }
}

void set_line(int line, int level) { contact[line] = level ? 1 : 0; }
int line_contact(int line) { return contact[line]; }
void schedule_line(uint64_t at, int line, int level) { schedule.insert({at, scheduleSeq++, line, level ? 1 : 0}); }
uint64_t schedule_last_us() { return schedule.empty() ? 0 : schedule.rbegin()->at; }
int pin_mode(int pin) { return modes[pin]; }
int pin_output(int pin) { return outputs[pin]; }

void serial_send(const std::string& text) {
  double t = std::max(double(nowUs), rxLast);
  for (char c : text) {
    t += kUartByteUs;
    rx.push_back({t, c});
  }
  rxLast = t;
}
const std::string& serial_log() { return txLog; }
size_t serial_rx_pending() { return rx.size(); }
uint64_t serial_tx_blocked_us() { return txBlockedUs; }
size_t serial_tx_buffer() { return txBufferSize; }
bool loop_wdt_enabled() { return loopWdtEnabled; }
uint64_t serial_tx_idle_at_us() { return uint64_t(std::max(double(nowUs), txDoneAt)); }

const I2sInfo& i2s() { return i2sInfo; }
uint64_t audio_t0_us() { return t0Us; }
const std::vector<int32_t>& audio_left() { return capL; }
const std::vector<int32_t>& audio_right() { return capR; }
size_t frame_at(uint64_t t) { return t <= t0Us ? 0 : size_t((t - t0Us) * kFs / 1000000); }
uint64_t time_of_frame(size_t f) { return t0Us + uint64_t(f) * 1000000 / kFs; }
std::vector<float> audio_segment(size_t from, size_t count) {
  std::vector<float> out;
  out.reserve(count);
  for (size_t i = from; i < from + count && i < capL.size(); i++) out.push_back(float(capL[i] / 2147483648.0));
  return out;
}
void i2s_set_halted(bool halted) {
  std::unique_lock<std::mutex> lk(mtx);
  if (halted == i2sHalted) return;
  if (halted) {
    haltedConsumed = consumedLocked();
    i2sHalted = true;
  } else {
    // The DMA resumes with the buffer it stopped at.
    i2sHalted = false;
    t0Us = nowUs - (haltedConsumed * kDmaFrames * 1000000ull) / kFs;
    cv.notify_all();
    if (!cv.wait_for(lk, std::chrono::seconds(20), [] { return audioParkedLocked(); }))
      harnessFatal("audio task did not block again after the I2S restarted");
  }
}

const RmtInfo& rmt() { return rmtInfo; }
const std::vector<LedFrame>& led_frames() { return leds; }

const SpiInfo& spi() { return spiInfo; }
std::array<Max7219, 4>& max_chain() { return chain; }
void max_power_glitch(int module) {
  for (int k = 0; k < 4; k++)
    if (module < 0 || module == k) chain[k] = Max7219();
  shiftReg = 0;
}
const std::vector<uint64_t>& max_control_writes() { return ctrlWrites; }

Picture framebuffer() {
  Picture fb{};
  for (int block = 0; block < 4; block++) {
    const Max7219& m = chain[3 - block];  // module nearest DIN = rightmost block
    if (!(m.shutdownReg & 1)) continue;
    if (m.decode) warn("MAX7219: BCD decode enabled, matrix shows decoded segments");
    for (int c = 0; c < 8; c++) {
      uint8_t col = 0;
      for (int r = 0; r < 8; r++) {
        const bool on = m.test || (r <= m.scanLimit && (m.digit[r] & (0x80 >> c)));
        if (on) col |= 1 << r;
      }
      fb[block * 8 + c] = col;
    }
  }
  return fb;
}

int text_width(const std::string& text) {
  int width = -1;  // app.js: glyphs.reduce((n, g) => n + g[0].length + 1, -1)
  for (char c : text) width += glyphFor(c, true).width + 1;
  return width;
}

Picture render_text(const std::string& text, bool extras) {
  Picture fb{};
  int width = -1;
  for (char c : text) width += glyphFor(c, extras).width + 1;
  const int spare = 32 - width;
  int column = spare >= 0 ? spare / 2 : -((1 - spare) / 2);  // Math.floor(spare / 2)
  for (char c : text) {
    const Glyph& g = glyphFor(c, extras);
    for (int x = 0; x < g.width; x++)
      if (column + x >= 0 && column + x < 32) fb[column + x] |= g.cols[x];
    column += g.width + 1;
  }
  return fb;
}

namespace {
uint8_t colAt(const Picture& fb, int x) { return x >= 0 && x < 32 ? fb[x] : 0; }
bool blankFrom(const Picture& fb, int x) {
  for (; x < 32; x++)
    if (colAt(fb, x)) return false;
  return true;
}
// Every reading of the columns from x on as glyphs separated by one blank column.
void parseFrom(const Picture& fb, int x, const std::string& sofar, std::vector<std::string>* out) {
  if (out->size() >= 64) return;
  if (blankFrom(fb, x)) {
    out->push_back(sofar);
    return;
  }
  auto tryGlyph = [&](const Glyph& g) {
    for (int i = 0; i < g.width; i++)
      if (colAt(fb, x + i) != g.cols[i]) return;
    if (colAt(fb, x + g.width)) return;  // separator
    parseFrom(fb, x + g.width + 1, sofar + g.c, out);
  };
  for (const AppGlyph& a : kAppFont) tryGlyph(glyphFor(a.c, false));
  for (const Glyph& g : kExtraGlyphs) tryGlyph(g);
}
}  // namespace

std::string decode_text(const Picture& fb) {
  int first = 0;
  while (first < 32 && !fb[first]) first++;
  if (first == 32) return "";
  std::vector<std::string> readings;
  for (int x = first - 2; x <= first; x++) parseFrom(fb, x, "", &readings);
  for (std::string r : readings) {
    while (!r.empty() && r.back() == ' ') r.pop_back();
    size_t lead = 0;
    while (lead < r.size() && r[lead] == ' ') lead++;
    r = r.substr(lead);
    if (render_text(r) == fb) return r;
  }
  return readings.empty() ? "?" : "?" + readings.front();
}
std::string display_text() { return decode_text(framebuffer()); }
bool display_is(const std::string& text) { return framebuffer() == render_text(text); }

std::string framebuffer_art(const Picture& fb) {
  std::string s;
  for (int r = 0; r < 8; r++) {
    for (int x = 0; x < 32; x++) s += (fb[x] & (1 << r)) ? '#' : '.';
    s += '\n';
  }
  return s;
}

void mcp_attach(bool present) { mcpPresent = present; }
bool mcp_attached() { return mcpPresent; }
void mcp_fail_next(int n) { mcpFailNext = n; }
uint8_t mcp_register(int reg) { return mcpRegs[reg]; }
void mcp_power_cycle() { mcpReset(); }
int i2c_transactions() { return i2cCount; }
const I2cInfo& i2c() { return i2cInfo; }

int nvs_writes() { return nvsWriteCount; }
const std::vector<NvsWrite>& nvs_log() { return nvsLog; }
void nvs_set_fail(bool fail) { nvsFail = fail; }
void nvs_set_stall_us(uint32_t us) { nvsStallUs = us; }
int calibration_for_calls() { return calibrationCalls; }
std::vector<std::string>& warnings() { return warningList; }

void finish(int code) {
  fflush(stdout);
  fflush(stderr);
  _exit(code);  // the audio thread is parked forever; skip static destructors
}

}  // namespace sim

// ================================================================= Arduino API
using namespace sim;
struct rmt_obj_s {
  int dummy;
};
static rmt_obj_s rmtObject;

uint32_t millis() { return uint32_t(nowUs / 1000) + epochMs; }
uint32_t micros() { return uint32_t(nowUs + uint64_t(epochMs) * 1000); }
void delay(uint32_t ms) { advance_us(uint64_t(ms) * 1000); }
void delayMicroseconds(uint32_t us) { advance_us(us); }

void pinMode(uint8_t pin, uint8_t mode) {
  if (pin >= kPins) harnessFatal("pinMode on a pin the ESP32-S3 does not have");
  modes[pin] = mode;
}

void digitalWrite(uint8_t pin, uint8_t val) {
  if (pin >= kPins) harnessFatal("digitalWrite on a pin the ESP32-S3 does not have");
  if (modes[pin] != OUTPUT) warn("digitalWrite on GPIO" + std::to_string(pin) + " before pinMode(OUTPUT)");
  const int before = outputs[pin];
  outputs[pin] = val ? 1 : 0;
  if (pin == PIN_MATRIX_CS) {
    if (before != 0 && val == LOW) maxCsFalling();
    if (before == 0 && val == HIGH) maxLatch();
  }
}

int digitalRead(uint8_t pin) {
  if (pin >= kPins) harnessFatal("digitalRead on a pin the ESP32-S3 does not have");
  if (modes[pin] == OUTPUT) return outputs[pin] > 0;
  if (contact[pin] == 0) return LOW;
  if (modes[pin] == INPUT_PULLUP || externalPullup(pin)) return HIGH;
  if (!floatWarned.count(pin)) {
    floatWarned.insert(pin);
    warn("digitalRead of floating GPIO" + std::to_string(pin) + " (no pull-up configured)");
  }
  return HIGH;
}

// ---------------------------------------------------------------- Serial
HardwareSerial Serial;
void HardwareSerial::begin(unsigned long baud, uint32_t, int8_t, int8_t, bool, unsigned long, uint8_t) {
  if (baud != 115200) warn("Serial.begin with baud " + std::to_string(baud));
  serialBegun = true;
}
size_t HardwareSerial::setTxBufferSize(size_t n) {
  if (serialBegun) {
    warn("Serial.setTxBufferSize after begin() is ignored by the 2.0.17 core");
    return 0;
  }
  if (n <= size_t(kUartFifo)) {
    warn("Serial.setTxBufferSize must exceed the 128-byte FIFO");
    txBufferSize = 0;
    return kUartFifo;
  }
  txBufferSize = n;
  return n;
}

// Virtual time stands still while the audio task renders, so the cycle counter
// (and the firmware's audio CPU figure) stays deterministic and reads 0 % here.
EspClass ESP;
uint32_t EspClass::getCycleCount() { return uint32_t(now_us() * 240u); }
uint32_t getCpuFrequencyMhz() { return 240; }
void enableLoopWDT() { loopWdtEnabled = true; }
int HardwareSerial::available() {
  int n = 0;
  for (const auto& b : rx) {
    if (b.first > double(nowUs)) break;
    n++;
  }
  if (n > kUartRxBuffer) warn("UART RX buffer (256 B) would have overflowed");
  return n;
}
int HardwareSerial::read() {
  if (rx.empty() || rx.front().first > double(nowUs)) return -1;
  const char c = rx.front().second;
  rx.pop_front();
  return uint8_t(c);
}
size_t HardwareSerial::write(uint8_t c) {
  const char ch = char(c);
  uartWrite(&ch, 1);
  return 1;
}
size_t HardwareSerial::write(const uint8_t* b, size_t n) {
  uartWrite(reinterpret_cast<const char*>(b), n);
  return n;
}
size_t HardwareSerial::print(const char* s) {
  const size_t n = strlen(s);
  uartWrite(s, n);
  return n;
}
size_t HardwareSerial::print(char c) {
  uartWrite(&c, 1);
  return 1;
}
size_t HardwareSerial::print(int v) { return printf("%d", v); }
size_t HardwareSerial::print(unsigned v) { return printf("%u", v); }
size_t HardwareSerial::print(long v) { return printf("%ld", v); }
size_t HardwareSerial::print(unsigned long v) { return printf("%lu", v); }
size_t HardwareSerial::print(double v, int digits) { return printf("%.*f", digits, v); }
size_t HardwareSerial::println() { return print("\r\n"); }
size_t HardwareSerial::printf(const char* format, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, format);
  const int n = vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);
  if (n < 0) return 0;
  const size_t len = std::min<size_t>(size_t(n), sizeof(buf) - 1);
  uartWrite(buf, len);
  return len;
}

// ---------------------------------------------------------------- FreeRTOS / esp_err
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t code, const char* name, const uint32_t stackDepth, void* params,
                                   UBaseType_t priority, TaskHandle_t*, const BaseType_t core) {
  i2sInfo.task = name;
  i2sInfo.taskStack = stackDepth;
  i2sInfo.taskPriority = priority;
  i2sInfo.taskCore = core;
  {
    std::lock_guard<std::mutex> lk(mtx);
    audioRunning = true;
  }
  std::thread(code, params).detach();
  // A higher-priority task preempts loop() at once: wait until it parks.
  std::unique_lock<std::mutex> lk(mtx);
  if (!cv.wait_for(lk, std::chrono::seconds(20), [] { return audioParkedLocked(); }))
    harnessFatal("audio task did not fill the DMA queue after start");
  return pdPASS;
}

void sim_esp_error_check_failed(esp_err_t rc, const char* file, int line, const char* expr) {
  fprintf(stdout, "CHECK FAIL harness/esp_error_check: %s:%d %s returned %d (the chip would abort and reboot)\n", file,
          line, expr, rc);
  finish(4);
}

// ---------------------------------------------------------------- I2S
esp_err_t i2s_driver_install(i2s_port_t port, const i2s_config_t* c, int, void*) {
  if (i2sInfo.installed || port != I2S_NUM_0) return ESP_ERR_INVALID_ARG;
  if (c->dma_buf_count < 2 || c->dma_buf_count > 128 || c->dma_buf_len < 8 || c->dma_buf_len > 1024)
    return ESP_ERR_INVALID_ARG;
  const int chanBits = c->bits_per_chan ? int(c->bits_per_chan) : int(c->bits_per_sample);
  if (c->dma_buf_len * 2 * chanBits / 8 > 4092) return ESP_ERR_INVALID_ARG;
  i2sInfo.installed = true;
  i2sInfo.port = port;
  i2sInfo.sampleRate = c->sample_rate;
  i2sInfo.bitsPerSample = c->bits_per_sample;
  i2sInfo.bitsPerChan = chanBits;
  i2sInfo.channelFormat = c->channel_format;
  i2sInfo.commFormat = c->communication_format;
  i2sInfo.mode = c->mode;
  i2sInfo.dmaCount = c->dma_buf_count;
  i2sInfo.dmaLen = c->dma_buf_len;
  i2sInfo.autoClear = c->tx_desc_auto_clear;
  i2sInfo.useApll = c->use_apll;
  std::lock_guard<std::mutex> lk(mtx);
  t0Us = nowUs;
  return ESP_OK;
}

esp_err_t i2s_set_pin(i2s_port_t, const i2s_pin_config_t* p) {
  i2sInfo.bck = p->bck_io_num;
  i2sInfo.ws = p->ws_io_num;
  i2sInfo.dout = p->data_out_num;
  i2sInfo.din = p->data_in_num;
  i2sInfo.mck = p->mck_io_num;
  return ESP_OK;
}

esp_err_t i2s_zero_dma_buffer(i2s_port_t) { return ESP_OK; }

// IDF 4.4 legacy driver: waits up to ticks_to_wait for a free DMA buffer,
// returns ESP_OK with *bytes_written short of size when that runs out.
esp_err_t i2s_write(i2s_port_t port, const void* src, size_t size, size_t* written, TickType_t ticks) {
  std::unique_lock<std::mutex> lk(mtx);
  const size_t bufferBytes = size_t(kDmaFrames) * 2 * 4;
  // A finite timeout is fine as long as it outlasts one DMA buffer (2.7 ms) with margin.
  if (port != I2S_NUM_0 || size != bufferBytes || ticks < 20) i2sInfo.badWrites++;
  i2sInfo.minTicks = std::min<uint32_t>(i2sInfo.minTicks, ticks);
  i2sInfo.maxTicks = std::max<uint32_t>(i2sInfo.maxTicks, ticks);
  audioDeadline = ticks == portMAX_DELAY ? UINT64_MAX : nowUs + uint64_t(ticks) * 1000;
  while (queueFullLocked() && nowUs < audioDeadline) {
    audioBlocked = true;
    cv.notify_all();
    cv.wait(lk);
  }
  audioBlocked = false;
  if (queueFullLocked()) {  // timed out: nothing written
    audioDeadline = UINT64_MAX;
    i2sInfo.timeouts++;
    if (written) *written = 0;
    return ESP_OK;
  }
  audioDeadline = UINT64_MAX;
  const int32_t* s = static_cast<const int32_t*>(src);
  const size_t frames = size / 8;
  for (size_t i = 0; i < frames; i++) {
    capL.push_back(s[2 * i]);
    capR.push_back(s[2 * i + 1]);
  }
  // Keep the capture aligned with the DMA timeline even if a write is not one buffer.
  buffersWritten += (frames + kDmaFrames - 1) / kDmaFrames;
  i2sInfo.writes++;
  if (written) *written = size;
  return ESP_OK;
}

// ---------------------------------------------------------------- RMT
rmt_obj_t* rmtInit(int pin, bool tx, rmt_reserve_memsize_t mem) {
  rmtInfo.pin = pin;
  rmtInfo.tx = tx;
  rmtInfo.mem = mem;
  return &rmtObject;
}

float rmtSetTick(rmt_obj_t*, float tick) {
  // 80 MHz APB / integer divider (1..255): 12.5 ns steps.
  int div = int(tick / 12.5f + 0.5f);
  if (div < 1) div = 1;
  if (div > 255) div = 255;
  rmtInfo.tickNs = div * 12.5f;
  return rmtInfo.tickNs;
}

bool rmtWriteBlocking(rmt_obj_t*, rmt_data_t* data, size_t size) {
  rmtInfo.writes++;
  if (rmtInfo.tickNs <= 0) warn("rmtWriteBlocking before rmtSetTick");
  if (size % 24) rmtInfo.lengthErrors++;
  if (ledWritten) {
    rmtInfo.minResetUs = std::min(rmtInfo.minResetUs, double(nowUs - lastLedEndUs));
    if (nowUs - lastLedEndUs < 280) rmtInfo.resetErrors++;
  }
  LedFrame f{};
  f.t_us = nowUs;
  f.count = int(size / 24);
  double totalNs = 0;
  uint8_t bytes[8 * 3] = {0};
  for (size_t i = 0; i < size; i++) {
    const rmt_data_t& d = data[i];
    if (d.level0 != 1 || d.level1 != 0) rmtInfo.levelErrors++;
    const double hi = d.duration0 * rmtInfo.tickNs, lo = d.duration1 * rmtInfo.tickNs;
    totalNs += hi + lo;
    const bool one = hi > 625;  // midpoint of T0H 400 / T1H 800
    // WS2812B (Worldsemi): T0H 0.4us, T1H 0.8us, T0L 0.85us, T1L 0.45us, all +/-150 ns.
    if (one) {
      rmtInfo.minT1H = std::min(rmtInfo.minT1H, hi), rmtInfo.maxT1H = std::max(rmtInfo.maxT1H, hi);
      rmtInfo.minT1L = std::min(rmtInfo.minT1L, lo), rmtInfo.maxT1L = std::max(rmtInfo.maxT1L, lo);
      if (hi < 650 || hi > 950 || lo < 300 || lo > 600) rmtInfo.timingErrors++;
    } else {
      rmtInfo.minT0H = std::min(rmtInfo.minT0H, hi), rmtInfo.maxT0H = std::max(rmtInfo.maxT0H, hi);
      rmtInfo.minT0L = std::min(rmtInfo.minT0L, lo), rmtInfo.maxT0L = std::max(rmtInfo.maxT0L, lo);
      if (hi < 250 || hi > 550 || lo < 700 || lo > 1000) rmtInfo.timingErrors++;
    }
    if (i < 8 * 24 && one) bytes[i / 8] |= 0x80 >> (i % 8);
  }
  for (int led = 0; led < 8 && led < f.count; led++) {
    f.rgb[led][0] = bytes[led * 3 + 1];  // GRB on the wire
    f.rgb[led][1] = bytes[led * 3 + 0];
    f.rgb[led][2] = bytes[led * 3 + 2];
  }
  leds.push_back(f);
  advance_us(uint64_t(totalNs / 1000 + 0.5));
  lastLedEndUs = nowUs;
  ledWritten = true;
  return true;
}

// ---------------------------------------------------------------- SPI
void SPIClass::begin(int8_t sck, int8_t miso, int8_t mosi, int8_t ss) {
  spiInfo.begun = true;
  spiInfo.sck = sck, spiInfo.miso = miso, spiInfo.mosi = mosi, spiInfo.ss = ss;
}
void SPIClass::beginTransaction(SPISettings s) {
  inTransaction = true;
  spiInfo.clock = s._clock;
  spiInfo.bitOrder = s._bitOrder;
  spiInfo.mode = s._dataMode;
}
void SPIClass::endTransaction() { inTransaction = false; }
uint8_t SPIClass::transfer(uint8_t d) {
  if (!spiInfo.begun) warn("SPI transfer before begin()");
  if (!inTransaction) spiInfo.transfersOutsideTransaction++;
  if (outputs[PIN_MATRIX_CS] != 0) spiInfo.transfersWithCsHigh++;
  for (int i = 0; i < 8; i++) {
    const int bit = spiInfo.bitOrder == LSBFIRST ? (d >> i) & 1 : (d >> (7 - i)) & 1;
    shiftReg = (shiftReg << 1) | uint64_t(bit);  // MAX7219 samples DIN on CLK rising edge (mode 0)
    bitsSinceFall++;
  }
  advance_us(uint64_t(8e6 / (spiInfo.clock ? spiInfo.clock : 1000000) + 1));
  return 0;
}
void SPIClass::transfer(void* data, uint32_t size) {
  uint8_t* p = static_cast<uint8_t*>(data);
  for (uint32_t i = 0; i < size; i++) p[i] = transfer(p[i]);
}
uint16_t SPIClass::transfer16(uint16_t d) {
  transfer(uint8_t(d >> 8));
  transfer(uint8_t(d));
  return 0;
}

// ---------------------------------------------------------------- Wire
TwoWire Wire;
bool TwoWire::begin(int sda, int scl, uint32_t frequency) {
  i2cInfo.begun = true;
  i2cInfo.sda = sda, i2cInfo.scl = scl, i2cInfo.freq = frequency ? frequency : 100000;
  return true;
}
bool TwoWire::begin(uint8_t, int, int, uint32_t) {
  warn("Wire.begin in slave mode");
  return false;
}
void TwoWire::setTimeOut(uint16_t ms) { i2cInfo.timeoutMs = ms; }
uint16_t TwoWire::getTimeOut() { return i2cInfo.timeoutMs; }
void TwoWire::beginTransmission(uint16_t address) {
  if (nonStop) warn("Wire: beginTransmission while a repeated-start transaction is pending");
  nonStop = false;
  txAddress = address;
  txLength = 0;
}
void TwoWire::beginTransmission(uint8_t a) { beginTransmission(uint16_t(a)); }
void TwoWire::beginTransmission(int a) { beginTransmission(uint16_t(a)); }
size_t TwoWire::write(uint8_t b) {
  if (txLength >= sizeof(txBuffer)) return 0;
  txBuffer[txLength++] = b;
  return 1;
}
size_t TwoWire::write(const uint8_t* b, size_t n) {
  for (size_t i = 0; i < n; i++)
    if (!write(b[i])) return i;
  return n;
}
uint8_t TwoWire::endTransmission(bool sendStop) {
  if (!i2cInfo.begun) warn("Wire used before begin()");
  if (!sendStop) {
    nonStop = true;  // the real core defers everything to requestFrom()
    return 0;
  }
  i2cCount++;
  const bool ack = mcpAck(txAddress);
  i2cSpend(ack ? 2 + 9 * int(1 + txLength) : 2 + 9);
  if (!ack) return 2;
  if (txLength > 0) {
    mcpPointer = txBuffer[0];
    for (size_t i = 1; i < txLength; i++) {
      mcpWrite(mcpPointer, txBuffer[i]);
      mcpAdvancePointer();
    }
  }
  return 0;
}
uint8_t TwoWire::endTransmission() { return endTransmission(true); }
size_t TwoWire::requestFrom(uint16_t address, size_t size, bool) {
  rxIndex = rxLength = 0;
  if (size > sizeof(rxBuffer)) size = sizeof(rxBuffer);
  i2cCount++;
  const bool ack = mcpAck(address);
  if (nonStop) {
    nonStop = false;
    if (address != txAddress) return 0;
    if (!ack) {
      i2cSpend(2 + 9);
      return 0;
    }
    i2cSpend(3 + 9 * int(1 + txLength) + 9 * int(1 + size));
    if (txLength > 0) {
      mcpPointer = txBuffer[0];
      for (size_t i = 1; i < txLength; i++) {
        mcpWrite(mcpPointer, txBuffer[i]);
        mcpAdvancePointer();
      }
    }
  } else {
    if (!ack) {
      i2cSpend(2 + 9);
      return 0;
    }
    i2cSpend(2 + 9 * int(1 + size));
  }
  for (size_t i = 0; i < size; i++) {
    rxBuffer[i] = mcpRead(mcpPointer);
    mcpAdvancePointer();
  }
  rxLength = size;
  return rxLength;
}
uint8_t TwoWire::requestFrom(uint16_t a, uint8_t s, bool st) { return uint8_t(requestFrom(a, size_t(s), st)); }
uint8_t TwoWire::requestFrom(uint16_t a, uint8_t s, uint8_t st) { return uint8_t(requestFrom(a, size_t(s), bool(st))); }
size_t TwoWire::requestFrom(uint8_t a, size_t s, bool st) { return requestFrom(uint16_t(a), s, st); }
uint8_t TwoWire::requestFrom(uint16_t a, uint8_t s) { return uint8_t(requestFrom(a, size_t(s), true)); }
uint8_t TwoWire::requestFrom(uint8_t a, uint8_t s, uint8_t st) { return uint8_t(requestFrom(uint16_t(a), size_t(s), bool(st))); }
uint8_t TwoWire::requestFrom(uint8_t a, uint8_t s) { return uint8_t(requestFrom(uint16_t(a), size_t(s), true)); }
uint8_t TwoWire::requestFrom(int a, int s, int st) { return uint8_t(requestFrom(uint16_t(a), size_t(s), bool(st))); }
uint8_t TwoWire::requestFrom(int a, int s) { return uint8_t(requestFrom(uint16_t(a), size_t(s), true)); }
int TwoWire::available() { return int(rxLength - rxIndex); }
int TwoWire::read() { return rxIndex < rxLength ? rxBuffer[rxIndex++] : -1; }

// ---------------------------------------------------------------- Preferences
bool Preferences::begin(const char* name, bool readOnly, const char*) {
  if (started_) return false;
  if (strlen(name) > 15) {
    warn("Preferences namespace longer than 15 chars");
    return false;
  }
  nvsLoad();
  bool exists = false;
  for (const auto& kv : nvs)
    if (kv.first.rfind(std::string(name) + "/", 0) == 0) exists = true;
  if (readOnly && !exists) return false;  // NVS_READONLY on a missing namespace fails
  snprintf(ns_, sizeof(ns_), "%s", name);
  readOnly_ = readOnly;
  started_ = true;
  return true;
}
void Preferences::end() { started_ = false; }
bool Preferences::clear() { return false; }
bool Preferences::remove(const char*) { return false; }
size_t Preferences::putBytes(const char* key, const void* value, size_t len) {
  if (!started_ || readOnly_ || !key || !value || !len) return 0;
  if (strlen(key) > 15) {
    warn("Preferences key longer than 15 chars");
    return 0;
  }
  NvsWrite w = flashWrite(key);
  w.ok = !nvsFail;
  nvsLog.push_back(w);
  if (nvsFail) return 0;
  const uint8_t* p = static_cast<const uint8_t*>(value);
  nvs[std::string(ns_) + "/" + key] = std::vector<uint8_t>(p, p + len);
  nvsWriteCount++;
  nvsSave();
  return len;
}
size_t Preferences::getBytesLength(const char* key) {
  if (!started_ || !key) return 0;
  auto it = nvs.find(std::string(ns_) + "/" + key);
  return it == nvs.end() ? 0 : it->second.size();
}
size_t Preferences::getBytes(const char* key, void* buf, size_t maxLen) {
  const size_t len = getBytesLength(key);
  if (!len || !buf || !maxLen) return 0;
  if (len > maxLen) return 0;  // the real core logs "not enough space" and returns 0
  memcpy(buf, nvs[std::string(ns_) + "/" + key].data(), len);
  return len;
}
bool Preferences::isKey(const char* key) { return getBytesLength(key) > 0; }

// ---------------------------------------------------------------- calibrationFor() watch
// dsp.h says calibrationFor() is too slow for the control loop; the firmware
// must use calibrationLookup(). Linked with -Wl,--wrap so every call from
// another translation unit (main.cpp, audio.cpp, ...) is counted.
extern "C" Calibration __real__Z14calibrationFor4Wavei9Harmonics(Wave, int, Harmonics);
extern "C" Calibration __wrap__Z14calibrationFor4Wavei9Harmonics(Wave w, int fold, Harmonics h) {
  calibrationCalls++;
  return __real__Z14calibrationFor4Wavei9Harmonics(w, fold, h);
}
