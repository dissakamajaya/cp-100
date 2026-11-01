// Host-side checks that the firmware's DSP and state rules match the simulator.
// Run from firmware/:  g++ -std=c++17 -Isrc test/host_test.cpp src/dsp.cpp src/model.cpp -o /tmp/cp100-test && /tmp/cp100-test

#include <math.h>
#include <stdio.h>
#include <initializer_list>

#include "dsp.h"
#include "model.h"

struct FoldCase {
  int wave, harmonics, foldPct;
  float phase, expected;
};
struct CalCase {
  int wave, harmonics, foldPct;
  float mean, gain;
};
#include "simulator_reference.h"

static int failures = 0;
#define CHECK(cond, ...)                    \
  do {                                      \
    if (!(cond)) {                          \
      failures++;                           \
      printf("FAIL %s:%d ", __FILE__, __LINE__); \
      printf(__VA_ARGS__);                  \
      printf("\n");                         \
    }                                       \
  } while (0)

static void testFoldMatchesSimulator() {
  for (const auto& c : kFoldCases) {
    const float got = foldSample(oscillatorSample(c.phase, Wave(c.wave)), c.foldPct, Harmonics(c.harmonics));
    CHECK(fabsf(got - c.expected) < 1e-4f, "fold w%d h%d f%d p%.2f: %f vs %f", c.wave, c.harmonics, c.foldPct,
          c.phase, got, c.expected);
  }
}

static void testCalibrationMatchesSimulator() {
  for (const auto& c : kCalCases) {
    const Calibration got = calibrationFor(Wave(c.wave), c.foldPct, Harmonics(c.harmonics));
    CHECK(fabsf(got.mean - c.mean) < 1e-4f && fabsf(got.gain - c.gain) / c.gain < 1e-3f,
          "calibration w%d h%d f%d: %f/%f vs %f/%f", c.wave, c.harmonics, c.foldPct, got.mean, got.gain, c.mean,
          c.gain);
  }
}

// The firmware plays from the generated table; it must equal the simulator's
// calibrationFor() (the C++ port and the sampled simulator values) everywhere.
static void testCalibrationTableMatchesSimulator() {
  for (const auto& c : kCalCases) {
    const Calibration got = calibrationLookup(Wave(c.wave), c.foldPct, Harmonics(c.harmonics));
    CHECK(fabsf(got.mean - c.mean) < 1e-5f && fabsf(got.gain - c.gain) / c.gain < 1e-5f,
          "table w%d h%d f%d: %f/%f vs simulator %f/%f", c.wave, c.harmonics, c.foldPct, got.mean, got.gain, c.mean,
          c.gain);
  }
  int worst = 0;
  float worstErr = 0;
  for (int w = 0; w < 3; w++)
    for (int h = 0; h < 3; h++)
      for (int f = 0; f <= 1000; f++) {
        const Calibration t = calibrationLookup(Wave(w), f / 10.f, Harmonics(h));
        const Calibration r = calibrationFor(Wave(w), f / 10.f, Harmonics(h));
        const float err = fabsf(t.gain - r.gain) / r.gain + fabsf(t.mean - r.mean);
        if (err > worstErr) worstErr = err, worst = w * 1000 + h * 100 + f;
        CHECK(err < 1e-3f, "table w%d h%d f%d: %f/%f vs calibrationFor %f/%f", w, h, f, t.mean, t.gain, r.mean, r.gain);
        CHECK(t.gain > 0 && t.gain < 10 && isfinite(t.mean), "table w%d h%d f%d not sane", w, h, f);
      }
  (void)worst;
  const Calibration lo = calibrationLookup(Wave::Sine, -7, Harmonics::Odd);
  const Calibration hi = calibrationLookup(Wave::Sine, 140, Harmonics::Odd);
  CHECK(lo.gain == calibrationLookup(Wave::Sine, 0, Harmonics::Odd).gain &&
            hi.gain == calibrationLookup(Wave::Sine, 100, Harmonics::Odd).gain,
        "fold outside 0..100 must clamp");
}

static float rms(const float* x, int n) {
  double s = 0;
  for (int i = 0; i < n; i++) s += double(x[i]) * x[i];
  return sqrtf(float(s / n));
}

static void testSynthLevelMuteAndContinuity() {
  constexpr int fs = 48000;
  static float buf[fs];
  Synth synth(fs);
  SynthTarget t;
  t.frequencyHz = 200;
  t.foldPct = 58;
  t.wave = Wave::Saw;
  t.harmonics = Harmonics::Both;
  const Calibration cal = calibrationFor(t.wave, t.foldPct, t.harmonics);
  t.mean = cal.mean;
  t.level = cal.gain;  // level 100%
  synth.render(buf, fs, t);
  const float r = rms(buf + fs / 2, fs / 2);
  CHECK(fabsf(r - .36f) < .03f, "steady RMS at level 100 should be ~0.36, got %f", r);

  // Gate release: the 10 ms mute leaves < 1% after 50 ms.
  t.level = 0;
  t.fastRelease = true;
  synth.render(buf, fs / 20, t);
  const float tail = rms(buf + fs / 20 - 240, 240);
  CHECK(tail < .0036f, "mute tail should be < 1%% of full level, got %f", tail);

  // A frequency jump glides; no single-sample step larger than a clean saw edge would make.
  t.level = cal.gain * .35f;
  t.fastRelease = false;
  t.frequencyHz = 120;
  t.foldPct = 0;
  t.wave = Wave::Sine;
  t.harmonics = Harmonics::Odd;
  const Calibration sine = calibrationFor(t.wave, 0, t.harmonics);
  t.mean = sine.mean;
  t.level = sine.gain * .35f;
  synth.render(buf, fs / 2, t);
  t.frequencyHz = 900;
  synth.render(buf, fs / 4, t);
  float maxStep = 0;
  for (int i = 1; i < fs / 4; i++) maxStep = fmaxf(maxStep, fabsf(buf[i] - buf[i - 1]));
  CHECK(maxStep < .1f, "frequency glide should be click-free, max step %f", maxStep);
}

static void testStateRules() {
  Preset presets[kPadCount];
  examplePresets(presets);
  State s = initialState(presets, kDefaultSetup);
  CHECK(s.muted && s.mode == Mode::Run && s.activePad == -1, "boot must be RUN and muted");

  CHECK(padDown(s, 1), "pad 2 has a preset");
  CHECK(!s.muted && s.activePad == 1 && s.parameters.frequencyHz == 1360, "pad 2 plays 1360 Hz");
  CHECK(!padDown(s, 2), "second pad is ignored while the first is held");
  CHECK(!padUp(s, 2) && !s.muted, "releasing the ignored pad changes nothing");
  CHECK(padUp(s, 1) && s.muted && s.activePad == -1, "release mutes");
  CHECK(!padDown(s, kPadCount), "empty pad does nothing");

  CHECK(!setMuted(s, false), "TEST/unmute is not allowed in RUN");
  CHECK(!applyParameters(s, s.parameters), "knobs are ignored in RUN");

  setMode(s, Mode::Service);
  Parameters p = s.parameters;
  p.frequencyHz = 432.04f;
  p.levelPct = 50;
  CHECK(applyParameters(s, p) && s.parameters.frequencyHz == 432.0f, "frequency rounds to 0.1 Hz");
  CHECK(toggleArm(s) && s.armed, "SAVE arms");
  bool saved = false;
  CHECK(padDown(s, 3, &saved) && saved && s.presets[3].used && s.presets[3].parameters.levelPct == 50,
        "armed pad press stores the preset");
  CHECK(!s.armed && s.muted && s.activePad == -1, "saving disarms and stays muted");

  Setup narrow = s.setup;
  narrow.minHz = 1200;
  narrow.maxHz = 1500;
  CHECK(applySetup(s, narrow), "sweep limits apply in SERVICE");
  setMode(s, Mode::Run);
  CHECK(!padDown(s, 0), "1096 Hz preset is outside 1200-1500 and blocked");
  CHECK(padDown(s, 1), "1360 Hz preset is inside and plays");

  const LedState led = ledState(s, 1);
  CHECK(led.active && led.color == LedState::Tosca, "odd index pad lights tosca when alternating");
  CHECK(ledState(s, 0).color == LedState::White, "idle pads are white");
}

static void testPadAudioLimit() {
  // Deliberately keep sending a sounding target for >5 seconds: the audio side
  // must stop even if the controller loop stalls, or Wi-Fi heartbeats continue.
  Synth synth(48000);float buffer[128];SynthTarget t;
  t.frequencyHz=1096;t.level=.35f;t.padPressId=1;t.padMaxFrames=240000;
  bool sounded=false,tailSilent=true;
  for(int block=0;block<2000;++block) {
    if(block==1000)t.frequencyHz=1360; // edits must not reset the sample counter
    synth.render(buffer,128,t);
    for(int n=0;n<128;++n) {
      const int frame=block*128+n;
      if(frame<239000 && fabsf(buffer[n])>.001f)sounded=true;
      if(frame>=240000 && buffer[n]!=0)tailSilent=false;
    }
  }
  CHECK(sounded,"limited pad should produce audio before cutoff");
  CHECK(tailSilent,"all samples after five seconds must be exactly zero without control-loop help");
  synth.render(buffer,128,t);CHECK(rms(buffer,128)==0,"same held press cannot restart audio");
  t.padPressId=2;synth.render(buffer,128,t);CHECK(rms(buffer,128)>.001f,"a new press gets a new five-second budget");
  // Release just before the deadline: the fade tail also stays inside 5 s.
  Synth released(48000);t.padPressId=1;t.padMaxFrames=240000;
  for(int block=0;block<1874;++block)released.render(buffer,128,t); // 239872 frames
  t.padPressId=0;t.padMaxFrames=0;t.level=0;t.fastRelease=true;
  released.render(buffer,128,t);released.render(buffer,128,t);
  CHECK(rms(buffer,128)==0,"release tail cannot exceed the five-second pad budget");
  t.level=.35f;t.fastRelease=false;
  t.padPressId=0;t.padMaxFrames=0; // TEST/sequence/GATHER use their own existing rules
  for(int block=0;block<1900;++block)synth.render(buffer,128,t);
  CHECK(rms(buffer,128)>.001f,"manual pad ceiling does not impose a global five-second playback limit");
}

static void testExtendedRange() {
  float buffer[4096];
  for (float hz : {20.f, 4000.f}) for (int w = 0; w < 3; ++w) for (int h = 0; h < 3; ++h)
    for (float fold : {0.f, .1f, 18.1f, 100.f}) {
      Synth synth(48000);
      const auto cal = calibrationLookup(Wave(w), fold, Harmonics(h));
      SynthTarget target;target.frequencyHz=hz;target.wave=Wave(w);target.harmonics=Harmonics(h);target.foldPct=fold;target.mean=cal.mean;target.level=.35f*cal.gain;
      bool finite = true;
      for(int block=0;block<6;++block){synth.render(buffer,4096,target);for(float x:buffer)finite=finite&&isfinite(x)&&fabsf(x)<=1;}
      CHECK(finite && rms(buffer,4096)>.002f,"extended DSP Hz %.1f wave %d harmonics %d fold %.1f",hz,w,h,fold);
      target.level=0;target.fastRelease=true;
      for(int block=0;block<2;++block)synth.render(buffer,4096,target);
      CHECK(rms(buffer+3072,1024)<1e-5f,"extended range release must mute");
    }
}

int main() {
  testFoldMatchesSimulator();
  testCalibrationMatchesSimulator();
  testCalibrationTableMatchesSimulator();
  testSynthLevelMuteAndContinuity();
  testStateRules();
  testExtendedRange();
  testPadAudioLimit();
  printf(failures ? "%d check(s) failed\n" : "all checks passed\n", failures);
  return failures ? 1 : 0;
}
