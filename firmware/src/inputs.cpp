#include "inputs.h"

#include <Arduino.h>

#include "pins.h"

namespace {
constexpr uint32_t kDebounceMs = 5;

struct Button {
  uint8_t pin;
  bool stable = false;  // true = pressed
  bool last = false;
  uint32_t changedAt = 0;

  explicit Button(uint8_t p) : pin(p) {}

  // Returns +1 on press, -1 on release, 0 otherwise.
  int poll(uint32_t now) {
    const bool raw = digitalRead(pin) == LOW;
    if (raw != last) last = raw, changedAt = now;
    if (raw == stable || now - changedAt < kDebounceMs) return 0;
    stable = raw;
    return raw ? 1 : -1;
  }
};

Button pads[4] = {Button(PIN_PADS[0]), Button(PIN_PADS[1]), Button(PIN_PADS[2]), Button(PIN_PADS[3])};
Button mute(PIN_MUTE);
}  // namespace

void inputsBegin() {
  // External 10k pull-ups remain recommended; internal pull-ups keep loose inputs quiet.
  for (auto& b : pads) pinMode(b.pin, INPUT_PULLUP);
  for (uint8_t pin : {PIN_MUTE})
    pinMode(pin, INPUT_PULLUP);
}

InputEvents inputsPoll() {
  const uint32_t now = millis();
  InputEvents e;
  for (int i = 0; i < 4; i++) {
    const int edge = pads[i].poll(now);
    if (edge > 0) e.padDown |= 1 << i;
    if (edge < 0) e.padUp |= 1 << i;
  }
  e.mutePressed = mute.poll(now) > 0;
  return e;
}
