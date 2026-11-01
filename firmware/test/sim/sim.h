#pragma once
// Simulated CP-100 board: virtual clock, GPIO lines, UART, I2S DAC capture,
// WS2812B chain, MAX7219 x4 chain and an MCP23017, all behind the fake
// Arduino layer in fake/. The real firmware sources are compiled against it.
//
// Threads: the firmware's loop() runs on the test's main thread; the real
// audioTask runs on a std::thread made by xTaskCreatePinnedToCore. Time only
// moves inside advance_us() (delay(), blocking UART/I2C/SPI/RMT/NVS calls),
// and advance_us() returns only once the audio thread is parked in
// i2s_write() with the 4-buffer DMA queue full (or waiting out its timeout
// while the I2S is halted), so every run is deterministic.
//
// Model assumptions (things this board cannot tell you about real hardware):
// - MAX7219: FC-16 style 4-in-1 module. DIN enters the RIGHTMOST module seen
//   from the front, digit register 1 = top row, segment bit D7 = leftmost
//   column. Other sellers' modules differ (display.h has flags for that).
//   Only logic is modelled: no 3V3->5V level, rise time or 10 MHz limits.
// - Contacts: pads, SAVE, MUTE and every MCP23017 input have the Rev08 10k
//   pull-ups; encoder pins rely on INPUT_PULLUP. PEC11R rests with A and B
//   open (HIGH) on a detent; "+1" is the direction the firmware calls
//   clockwise (A falls first), which on the real part depends on wiring.
// - Time: UART TX returns once the bytes fit in the 128-byte FIFO plus the
//   driver ring buffer (only if Serial.setTxBufferSize() ran before begin(),
//   as in core 2.0.17), RX bytes arrive at 115200 baud; I2C transfers cost
//   their bit time at 400 kHz (absent device = address NACK, no bus lock-up);
//   SPI 1 MHz; RMT the WS2812B frame time; an NVS write blocks for
//   nvs stall time (default 20 ms). Firmware computation itself takes zero
//   time, so CPU load of the synth on the ESP32-S3 (the firmware's "CPU %"
//   reads 0 here), task stack depth and I2S underruns are NOT verified here.
// - Audio: i2s_write() blocks per 128-frame buffer with 4 buffers in flight
//   and honours its tick timeout (1 tick = 1 ms); the DAC is ideal (no
//   PCM5102 filter, no amplifier). Host float maths (glibc sinf/fmodf, SSE)
//   stands in for newlib on the Xtensa FPU.
// - NVS write = flash write: the cache is off, the audio task is frozen and
//   the non-IRAM I2S ISR cannot run, so the DMA keeps looping its 4 buffers
//   for the whole stall. The capture contains that loop, and nvs_log() keeps
//   the peak of the 4 looping buffers for each write.
// - NVS: a text file; no partition size, wear or power-loss behaviour.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace sim {

constexpr int kFs = 48000;
constexpr int kDmaBuffers = 4;
constexpr int kDmaFrames = 128;

// ---------------------------------------------------------------- clock
uint64_t now_us();
void advance_us(uint64_t us);
void set_epoch_ms(uint32_t ms);  // millis() value at simulation start (default 0)

// ---------------------------------------------------------------- lines
// Line ids 0..48 are ESP32 GPIOs, 100+n is MCP23017 GPAn.
constexpr int kMcpLine = 100;
// level 0 = contact closed to GND, 1 = contact open (line pulled up).
void set_line(int line, int level);
int line_contact(int line);
void schedule_line(uint64_t at_us, int line, int level);
uint64_t schedule_last_us();  // time of the last scheduled event (0 if none)
int pin_mode(int pin);        // last pinMode() value, -1 if never set
int pin_output(int pin);      // last digitalWrite() value, -1 if never written

// ---------------------------------------------------------------- UART
void serial_send(const std::string& text);  // arrives at 115200 baud from now
const std::string& serial_log();
size_t serial_rx_pending();  // bytes sent by the test that the firmware has not read yet
uint64_t serial_tx_blocked_us();  // total time firmware spent blocked in Serial writes
bool loop_wdt_enabled();          // enableLoopWDT() was called
size_t serial_tx_buffer();        // driver TX ring buffer size in effect (0 = FIFO only)
uint64_t serial_tx_idle_at_us();  // when the UART will have sent everything queued so far

// ---------------------------------------------------------------- I2S
struct I2sInfo {
  bool installed = false;
  int port = -1;
  uint32_t sampleRate = 0;
  int bitsPerSample = 0, bitsPerChan = 0, channelFormat = -1, commFormat = -1, mode = 0;
  int dmaCount = 0, dmaLen = 0;
  bool autoClear = false, useApll = true;
  int bck = -2, ws = -2, dout = -2, din = -2, mck = -2;
  int badWrites = 0;  // wrong port/size/timeout
  int writes = 0;
  int timeouts = 0;              // i2s_write calls that returned without room (I2S halted)
  uint32_t minTicks = 0xFFFFFFFF, maxTicks = 0;  // ticks_to_wait seen
  std::string task;
  uint32_t taskStack = 0;
  unsigned taskPriority = 0;
  int taskCore = -1;
};
const I2sInfo& i2s();
uint64_t audio_t0_us();  // when the DMA started clocking frames
const std::vector<int32_t>& audio_left();
const std::vector<int32_t>& audio_right();
size_t frame_at(uint64_t t_us);  // index of the frame played at time t
uint64_t time_of_frame(size_t frame);
// Normalised (1.0 = int32 full scale) copy of the left channel.
std::vector<float> audio_segment(size_t from, size_t count);
// Stops (true) or restarts the I2S clock: the DMA stops taking buffers, so
// i2s_write() runs into its timeout. Audio timing (frame_at) is only valid
// before the first halt.
void i2s_set_halted(bool halted);

// ---------------------------------------------------------------- WS2812B
struct LedFrame {
  uint64_t t_us;
  int count;
  uint8_t rgb[8][3];
};
struct RmtInfo {
  int pin = -1;
  bool tx = false;
  int mem = 0;
  float tickNs = 0;
  int writes = 0;
  int timingErrors = 0;  // bit with high/low times outside the WS2812B datasheet window
  int levelErrors = 0;   // item not high-then-low
  int lengthErrors = 0;  // not a whole number of 24-bit LEDs
  int resetErrors = 0;   // < 280 us low between frames
  double minResetUs = 1e18;  // shortest low time between two frames
  double minT0H = 1e9, maxT0H = 0, minT1H = 1e9, maxT1H = 0, minT0L = 1e9, maxT0L = 0, minT1L = 1e9, maxT1L = 0;
};
const RmtInfo& rmt();
const std::vector<LedFrame>& led_frames();

// ---------------------------------------------------------------- MAX7219
struct Max7219 {
  uint8_t digit[8] = {0};
  uint8_t decode = 0, intensity = 0, scanLimit = 0, shutdownReg = 0, test = 0;
};
struct SpiInfo {
  bool begun = false;
  int sck = -2, miso = -2, mosi = -2, ss = -2;
  uint32_t clock = 0;
  int bitOrder = -1, mode = -1;
  int transfersOutsideTransaction = 0;
  int transfersWithCsHigh = 0;
  int badFrames = 0;  // CS rising edge after a bit count that is not 64
  int frames = 0;
};
const SpiInfo& spi();
std::array<Max7219, 4>& max_chain();  // [0] = module nearest DIN
void max_power_glitch(int module = -1);  // module (or all) back to power-on state (shutdown, blank)
// Times of CS latches that wrote the shutdown register (0x0C) of every module.
const std::vector<uint64_t>& max_control_writes();
// Rendered 32x8 picture, one byte per column, bit 0 = top row, x = 0 left,
// for an FC-16 style module: DIN enters the rightmost module seen from the
// front, digit register 1 = top row, segment bit D7 = leftmost column.
using Picture = std::array<uint8_t, 32>;
Picture framebuffer();
// What app.js drawMatrix() would paint for `text`: its 5x7 font (ref/font.h,
// generated from app.js), centred at floor((32 - width) / 2). Characters the
// app.js font lacks use the harness's own classic 5x7 glyphs when extras is
// true (B C G H J K Q X Z - :), otherwise they are drawn as a space like app.js.
// Lower case is upper-cased (the firmware's console "display" does that).
Picture render_text(const std::string& text, bool extras = true);
// Exact text whose render_text() equals the picture, or "?" followed by the
// best-effort reading ("?" alone if nothing decodes) when the picture is not
// a centred render of any text.
std::string decode_text(const Picture& fb);
std::string display_text();
bool display_is(const std::string& text);
std::string framebuffer_art(const Picture& fb);
int text_width(const std::string& text);  // columns, app.js rule: sum(width + 1) - 1

// ---------------------------------------------------------------- MCP23017
void mcp_attach(bool present);
bool mcp_attached();
void mcp_fail_next(int transactions);  // NACK the next n transactions to 0x20 (bus glitch)
uint8_t mcp_register(int reg);
void mcp_power_cycle();
int i2c_transactions();
struct I2cInfo {
  bool begun = false;
  int sda = -2, scl = -2;
  uint32_t freq = 0;
  uint16_t timeoutMs = 50;
};
const I2cInfo& i2c();

// ---------------------------------------------------------------- NVS
int nvs_writes();  // successful putBytes()
struct NvsWrite {
  uint64_t t_us;       // start of the flash write
  std::string key;
  bool ok;
  double ringPeak;     // peak |sample| (1.0 = FS) of the 4 DMA buffers looping during the write
  uint64_t stallUs;
};
const std::vector<NvsWrite>& nvs_log();
void nvs_set_fail(bool fail);       // putBytes() returns 0 (NVS full / flash error)
void nvs_set_stall_us(uint32_t us); // flash-write time (default 20000, env CP100_NVS_STALL_US)

// ---------------------------------------------------------------- misc
// calibrationFor() calls made from outside dsp.cpp (linked with --wrap).
int calibration_for_calls();
std::vector<std::string>& warnings();
[[noreturn]] void finish(int code);

}  // namespace sim
