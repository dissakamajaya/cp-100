#include "panel.h"

#include <Arduino.h>
#include <Wire.h>

#include "encoder.h"
#include "pins.h"

namespace {
// MCP23017 registers, IOCON.BANK = 0 (power-on default)
constexpr uint8_t IODIRA = 0x00, IODIRB = 0x01, GPPUA = 0x0C, GPIOA = 0x12, OLATA = 0x14, OLATB = 0x15;
constexpr uint8_t BIT_MODE = 1 << 0, BIT_SINE = 1 << 1, BIT_TRIANGLE = 1 << 2, BIT_ODD = 1 << 3,
                  BIT_FOLD_A = 1 << 4, BIT_FOLD_B = 1 << 5, BIT_BOTH = 1 << 6;
constexpr uint32_t kDebounceMs = 20, kProbeIntervalMs = 1000;
// A glitch on the bus must not look like a missing panel: lost = reads failing for 200 ms.
constexpr int kFailuresBeforeLost = 3;
constexpr uint32_t kLostAfterMs = 200;

PanelState state;
Encoder fold;
uint8_t lastRaw = 0xFF, stableRaw = 0;
uint32_t rawChangedAt = 0, lastProbe = 0;
bool haveStable = false;  // a debounced reading exists since the last (re)connect
bool everStable = false;  // a debounced reading exists since boot
int failures = 0;
uint32_t firstFailureAt = 0;

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MCP23017_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readGpioA(uint8_t* value) {
  Wire.beginTransmission(MCP23017_ADDR);
  Wire.write(GPIOA);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(uint8_t(MCP23017_ADDR), uint8_t(1)) != 1) return false;
  *value = Wire.read();
  return true;
}

bool configure() {
  // GPA0-6 inputs, GPA7 and GPB0-7 unused outputs driven LOW (Rev08).
  return writeRegister(IODIRA, 0x7F) && writeRegister(GPPUA, 0x7F) && writeRegister(OLATA, 0x00) &&
         writeRegister(IODIRB, 0x00) && writeRegister(OLATB, 0x00);
}

// Three-position slide: left input LOW, right input LOW, both HIGH = middle.
// Both LOW is not a real position (wiring fault or bounce): keep the last valid choice.
template <typename T>
T decodeSlide(uint8_t raw, uint8_t leftBit, uint8_t rightBit, T left, T middle, T right, T last) {
  const bool l = !(raw & leftBit), r = !(raw & rightBit);
  if (l && r) return last;
  return l ? left : r ? right : middle;
}
}  // namespace

void panelBegin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  Wire.setTimeOut(10);
  lastProbe = millis() - kProbeIntervalMs;  // probe on the first poll
}

PanelEvents panelPoll() {
  PanelEvents e;
  const uint32_t now = millis();
  if (!state.present) {
    if (now - lastProbe < kProbeIntervalMs) return e;
    lastProbe = now;
    if (!configure()) return e;
    state.present = true;
    haveStable = false;
    failures = 0;
    fold = Encoder();  // its last contact state is stale
    lastRaw = 0xFF;  // never a masked reading, so the first read restarts the debounce
    e.connected = true;
  }

  uint8_t raw;
  if (!readGpioA(&raw)) {
    if (failures++ == 0) firstFailureAt = now;
    if (failures < kFailuresBeforeLost || now - firstFailureAt < kLostAfterMs) return e;
    state.present = false;
    everStable = false;  // after a reconnect the switches are news again: re-sync to them
    lastProbe = now;
    e.disconnected = true;
    return e;
  }
  failures = 0;

  e.foldSteps = fold.update(raw & BIT_FOLD_A, raw & BIT_FOLD_B);

  const uint8_t switches = raw & (BIT_MODE | BIT_SINE | BIT_TRIANGLE | BIT_ODD | BIT_BOTH);
  if (switches != lastRaw) lastRaw = switches, rawChangedAt = now;
  if (now - rawChangedAt < kDebounceMs) return e;     // still settling
  if (haveStable && switches == stableRaw) return e;  // nothing new

  stableRaw = switches;
  haveStable = true;
  // Only the first reading since boot is news by itself. After a reconnect,
  // report only switches that really differ from what was last applied.
  const bool first = !everStable;
  everStable = true;
  const Mode mode = (switches & BIT_MODE) ? Mode::Run : Mode::Service;  // GPA0 LOW = SERVICE
  const Wave wave = decodeSlide(switches, BIT_SINE, BIT_TRIANGLE, Wave::Sine, Wave::Saw, Wave::Triangle, state.wave);
  const Harmonics harmonics = decodeSlide(switches, BIT_ODD, BIT_BOTH, Harmonics::Odd, Harmonics::Even,
                                          Harmonics::Both, state.harmonics);
  e.modeChanged = first || mode != state.mode;
  e.waveChanged = first || wave != state.wave;
  e.harmonicsChanged = first || harmonics != state.harmonics;
  state.mode = mode;
  state.wave = wave;
  state.harmonics = harmonics;
  return e;
}

const PanelState& panelState() { return state; }
