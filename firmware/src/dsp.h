#pragma once
// Port of simulator/dist/dsp.js. The simulator is the reference: the same
// oscillator, parity folding and RMS calibration, so a preset made in the
// browser should sound the same on the device.

#include <stdint.h>

enum class Wave : uint8_t { Sine, Saw, Triangle };
enum class Harmonics : uint8_t { Odd, Even, Both };

// Reference versions of the simulator's functions (exact semantics, slow).
float oscillatorSample(float phase, Wave wave);
float reflect(float x);
// EVEN keeps the source and adds only the even part of the biased transfer.
// ODD uses symmetric folding; BOTH sums that with the even part.
float foldSample(float x, float foldPct, Harmonics harmonics);

struct Calibration {
  float mean;
  float gain;  // scales the folded wave to 0.36 RMS
};
// 4096-point measurement, as dsp.js calibrationFor(). Too slow for the control
// loop on the ESP32-S3; the firmware uses calibrationLookup() instead.
Calibration calibrationFor(Wave wave, float foldPct, Harmonics harmonics);
// Precomputed from the simulator (calibration_table.h).
Calibration calibrationLookup(Wave wave, float foldPct, Harmonics harmonics);

// What the control side asks the audio side to play. Copied under a lock once
// per audio block.
struct SynthTarget {
  float frequencyHz = 128.5f;
  float level = 0;  // 0..1 including calibration gain; 0 = muted
  float foldPct = 0;
  float mean = 0;
  Wave wave = Wave::Sine;
  Harmonics harmonics = Harmonics::Odd;
  bool fastRelease = false;  // mute uses the 10 ms release, like the simulator
};

// Free-running synth. The oscillator never restarts: every parameter change,
// including mute, is a smoothed glide toward the new target.
// Internally it runs at 4x the output rate, like the simulator's WaveShaper
// oversample='4x': band-limited (PolyBLEP) saw, folding at 192 kHz, then a
// 64-tap low-pass decimator back to 48 kHz.
class Synth {
 public:
  explicit Synth(float sampleRate);
  void render(float* out, int frames, const SynthTarget& target);
  float peak() const { return peak_; }

  static constexpr int kOversample = 4;
  static constexpr int kTaps = 64;

 private:
  struct Smooth {
    float value = 0;
    void step(float target, float coeff) { value += (target - value) * coeff; }
  };
  float coeff(float seconds) const;

  float fs_;
  float cFreq_, cWave_, cParam_, cMute_;
  float phase_ = 0;
  Smooth freq_, waveGain_[3], drive_, bias_, oddMix_, dryMix_, evenMix_, mean_, level_;
  float fir_[kTaps];
  float history_[2 * kTaps] = {};  // each sample stored twice so the dot product never wraps
  int historyPos_ = 0;
  float hpX_ = 0, hpY_ = 0, hpR_;
  float peak_ = 0;
};
