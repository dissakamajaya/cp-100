#pragma once
// Port of simulator/dist/model.js: the controller state and its rules.
// Every mutator returns true when the state changed.

#include <stdint.h>

#include "dsp.h"

enum class Mode : uint8_t { Run, Service };
enum class LedColor : uint8_t { Pink, Tosca, Alternate };

struct Parameters {
  float frequencyHz;
  int levelPct;
  Wave wave;
  float foldPct;
  Harmonics harmonics;
};

struct Setup {
  float minHz;
  float maxHz;
  bool calibrated;
  LedColor ledColor;
  bool ledPulse;
};

struct Preset {
  bool used;
  Parameters parameters;
};

constexpr int kPadCount = 4;
constexpr float kMinHz = 20, kMaxHz = 4000;
constexpr Parameters kDefaultParameters = {1096, 35, Wave::Sine, 0, Harmonics::Odd};
constexpr Setup kDefaultSetup = {20, 4000, false, LedColor::Alternate, false};

struct State {
  Mode mode;
  bool muted;
  bool armed;
  int8_t activePad;  // -1 = none
  Setup setup;
  Parameters parameters;
  Preset presets[kPadCount];
};

bool validParameters(const Parameters& p);
Parameters cleanParameters(const Parameters& p);
bool validSetup(const Setup& s);
bool inRange(const Parameters& p, const Setup& s);
void examplePresets(Preset out[kPadCount]);
State initialState(const Preset presets[kPadCount], const Setup& setup);

bool setMode(State& s, Mode mode);
bool applySetup(State& s, const Setup& setup);         // SERVICE only
bool applyParameters(State& s, const Parameters& p);   // SERVICE only
bool toggleArm(State& s);                              // SERVICE only
bool setMuted(State& s, bool muted);                   // unmute (TEST) only in SERVICE
bool padDown(State& s, int index, bool* presetSaved = nullptr);
bool padUp(State& s, int index);
bool releaseAll(State& s);

struct LedState {
  bool active;
  enum Color : uint8_t { White, Pink, Tosca } color;
  float pulseHz;
};
LedState ledState(const State& s, int index);

const char* waveName(Wave w);
const char* harmonicsName(Harmonics h);
