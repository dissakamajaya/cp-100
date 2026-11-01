#include "leds.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "pins.h"

namespace {
constexpr int kBitsPerLed = 24;
constexpr uint32_t kPulseFrameMs = 20;   // pulse animation frame interval
constexpr uint32_t kRefreshMs = 1000;    // resend an unchanged frame: heals LEDs that glitched or powered up late
constexpr uint32_t kLatchUs = 300;       // WS2812B-V5 needs > 280 us low between frames

rmt_obj_t* rmt = nullptr;
rmt_data_t bits[kPadCount * kBitsPerLed];
uint8_t shown[kPadCount][3];
bool shownValid = false;
uint32_t lastWriteMs = 0, lastEndUs = 0, lastUpdateMs = 0;
float pulsePhase = 0;  // cycles, 0..1; accumulated so it stays smooth after weeks of uptime
int pulsingPad = -1;

struct Rgb {
  uint8_t r, g, b;
};
// Colours from the simulator's pad styles.
constexpr Rgb kWhite = {255, 255, 255}, kPink = {242, 140, 189}, kTosca = {100, 218, 207};

void encode(int led, uint8_t r, uint8_t g, uint8_t b) {
  const uint8_t grb[3] = {g, r, b};  // WS2812B wants green first
  rmt_data_t* out = bits + led * kBitsPerLed;
  for (int c = 0; c < 3; c++)
    for (int bit = 7; bit >= 0; bit--, out++) {
      const bool one = grb[c] & (1 << bit);
      out->level0 = 1;
      out->duration0 = one ? 8 : 4;  // 100 ns ticks: T1H 0.8 us, T0H 0.4 us
      out->level1 = 0;
      out->duration1 = one ? 4 : 8;
    }
}
}  // namespace

void ledsBegin() {
  rmt = rmtInit(PIN_RGB, RMT_TX_MODE, RMT_MEM_64);
  if (!rmt) {
    Serial.println("RGB: RMT init failed, pad LEDs disabled");
    return;
  }
  rmtSetTick(rmt, 100);
}

void ledsUpdate(const State& state, uint32_t now) {
  if (!rmt) return;
  const uint32_t dt = now - lastUpdateMs;  // unsigned: safe across the millis() wrap
  lastUpdateMs = now;

  uint8_t frame[kPadCount][3];
  bool pulsing = false;
  bool urgent = !shownValid;  // something other than the pulse animation changed
  for (int i = 0; i < kPadCount; i++) {
    const LedState led = ledState(state, i);
    const Rgb c = led.color == LedState::Pink ? kPink : led.color == LedState::Tosca ? kTosca : kWhite;
    float scale = kLedBrightness;
    if (led.pulseHz > 0) {
      if (pulsingPad != i) pulsingPad = i, pulsePhase = 0, urgent = true;  // each press starts at full brightness
      pulsePhase += (dt > 100 ? 100 : dt) * .001f * led.pulseHz;
      pulsePhase -= floorf(pulsePhase);
      // Dip to 82% halfway through each cycle, like the simulator's pad-pulse.
      scale *= 1 - .18f * (.5f - .5f * cosf(6.2831853f * pulsePhase));
      pulsing = true;
    }
    frame[i][0] = uint8_t(c.r * scale + .5f);
    frame[i][1] = uint8_t(c.g * scale + .5f);
    frame[i][2] = uint8_t(c.b * scale + .5f);
    if (led.pulseHz <= 0 && shownValid && memcmp(frame[i], shown[i], 3)) urgent = true;
  }
  if (!pulsing) pulsingPad = -1;

  const uint32_t sinceWrite = now - lastWriteMs;
  const bool due = urgent || (pulsing && sinceWrite >= kPulseFrameMs) || sinceWrite >= kRefreshMs;
  if (!due) return;
  if (micros() - lastEndUs < kLatchUs) return;  // previous frame not latched yet; next loop pass

  for (int i = 0; i < kPadCount; i++) encode(i, frame[i][0], frame[i][1], frame[i][2]);
  rmtWriteBlocking(rmt, bits, kPadCount * kBitsPerLed);
  lastEndUs = micros();
  memcpy(shown, frame, sizeof(frame));
  shownValid = true;
  lastWriteMs = now;
}
