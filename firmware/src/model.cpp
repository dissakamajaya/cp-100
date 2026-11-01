#include "model.h"

#include <math.h>

namespace {
float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
}  // namespace

bool validParameters(const Parameters& p) {
  return isfinite(p.frequencyHz) && p.frequencyHz >= kMinHz && p.frequencyHz <= kMaxHz &&
         isfinite(p.foldPct) && p.levelPct >= 0 && p.levelPct <= 100 && p.foldPct >= 0 && p.foldPct <= 100 &&
         uint8_t(p.wave) <= uint8_t(Wave::Triangle) && uint8_t(p.harmonics) <= uint8_t(Harmonics::Both);
}

Parameters cleanParameters(const Parameters& p) {
  Parameters c = p;
  c.frequencyHz = roundf(p.frequencyHz * 10) / 10;
  c.foldPct = roundf(p.foldPct * 10) / 10;
  return c;
}

bool validSetup(const Setup& s) {
  return isfinite(s.minHz) && isfinite(s.maxHz) && s.minHz >= kMinHz && s.maxHz <= kMaxHz &&
         s.minHz < s.maxHz && uint8_t(s.ledColor) <= uint8_t(LedColor::Alternate);
}

bool inRange(const Parameters& p, const Setup& s) {
  return p.frequencyHz >= s.minHz && p.frequencyHz <= s.maxHz;
}

void examplePresets(Preset out[kPadCount]) {
  const Parameters examples[4] = {
      {128.5f, 35, Wave::Sine, 0, Harmonics::Odd},
      {196, 35, Wave::Triangle, 18, Harmonics::Odd},
      {256, 30, Wave::Saw, 32, Harmonics::Both},
      {384, 40, Wave::Sine, 58, Harmonics::Both},
  };
  for (int i = 0; i < kPadCount; i++) out[i] = i < 4 ? Preset{true, examples[i]} : Preset{false, {}};
}

State initialState(const Preset presets[kPadCount], const Setup& setup) {
  State s{};
  s.mode = Mode::Run;
  s.muted = true;
  s.armed = false;
  s.activePad = -1;
  s.setup = validSetup(setup) ? setup : kDefaultSetup;
  s.parameters = kDefaultParameters;
  s.parameters.frequencyHz = clampf(s.parameters.frequencyHz, s.setup.minHz, s.setup.maxHz);
  for (int i = 0; i < kPadCount; i++) {
    const bool ok = presets[i].used && validParameters(presets[i].parameters);
    s.presets[i] = ok ? Preset{true, cleanParameters(presets[i].parameters)} : Preset{false, {}};
  }
  return s;
}

bool setMode(State& s, Mode mode) {
  s.mode = mode;
  s.muted = true;
  s.armed = false;
  s.activePad = -1;
  return true;
}

bool applySetup(State& s, const Setup& setup) {
  if (s.mode != Mode::Service || !validSetup(setup)) return false;
  s.setup = setup;
  s.parameters.frequencyHz = clampf(s.parameters.frequencyHz, setup.minHz, setup.maxHz);
  s.muted = true;
  s.armed = false;
  s.activePad = -1;
  return true;
}

bool applyParameters(State& s, const Parameters& p) {
  if (s.mode != Mode::Service || !validParameters(p)) return false;
  Parameters c = p;
  c.frequencyHz = clampf(c.frequencyHz, s.setup.minHz, s.setup.maxHz);
  s.parameters = cleanParameters(c);
  s.activePad = -1;
  return true;
}

bool toggleArm(State& s) {
  if (s.mode != Mode::Service) return false;
  s.armed = !s.armed;
  s.muted = true;
  s.activePad = -1;
  return true;
}

bool setMuted(State& s, bool muted) {
  if (!muted && s.mode == Mode::Run) return false;
  s.muted = muted;
  s.armed = false;
  s.activePad = -1;
  return true;
}

bool padDown(State& s, int i, bool* presetSaved) {
  if (presetSaved) *presetSaved = false;
  // First pad held wins; later pads are ignored until it is released.
  if (i < 0 || i >= kPadCount || s.activePad >= 0) return false;
  if (s.mode == Mode::Service && s.armed) {
    s.presets[i] = {true, s.parameters};
    s.armed = false;
    s.muted = true;
    if (presetSaved) *presetSaved = true;
    return true;
  }
  if (!s.presets[i].used || !inRange(s.presets[i].parameters, s.setup)) return false;
  s.parameters = s.presets[i].parameters;
  s.activePad = i;
  s.muted = false;
  return true;
}

bool padUp(State& s, int i) {
  if (s.activePad != i) return false;
  s.activePad = -1;
  s.muted = true;
  return true;
}

bool releaseAll(State& s) {
  const bool changed = s.activePad >= 0 || !s.muted;
  s.activePad = -1;
  s.muted = true;
  return changed;
}

LedState ledState(const State& s, int i) {
  const bool active = s.activePad == i && !s.muted;
  LedState::Color color = LedState::White;
  if (active) {
    if (s.setup.ledColor == LedColor::Alternate) color = i % 2 ? LedState::Tosca : LedState::Pink;
    else color = s.setup.ledColor == LedColor::Tosca ? LedState::Tosca : LedState::Pink;
  }
  const float fraction = (s.parameters.frequencyHz - s.setup.minHz) / (s.setup.maxHz - s.setup.minHz);
  return {active, color, active && s.setup.ledPulse ? .5f + clampf(fraction, 0, 1) * 1.5f : 0};
}

const char* waveName(Wave w) {
  return w == Wave::Saw ? "sawtooth" : w == Wave::Triangle ? "triangle" : "sine";
}

const char* harmonicsName(Harmonics h) {
  return h == Harmonics::Even ? "even" : h == Harmonics::Both ? "both" : "odd";
}
