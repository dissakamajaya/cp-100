#include "dsp.h"

#include <math.h>

namespace {
constexpr float kTwoPi = 6.28318530718f;
constexpr double kPi = 3.14159265358979323846;

#include "calibration_table.h"

// floor() without a libm call: the FPU's truncating convert plus a fix-up.
inline float fastFloor(float x) {
  const float t = float(int(x));
  return t > x ? t - 1 : t;
}

// Same as reflect() for the range the folder can reach (|x| < 2^23).
inline float fastReflect(float x) {
  float w = x + 1;
  w -= 4 * fastFloor(w * .25f);
  return w <= 2 ? w - 1 : 3 - w;
}

// sin(2 pi t) for t in [0, 1): odd Taylor series to z^11 after folding the
// phase into [-1/4, 1/4] (|error| < 6e-8).
inline float fastSin(float t) {
  const float x = t < .25f ? t : t < .75f ? .5f - t : t - 1;
  const float z = kTwoPi * x, z2 = z * z;
  return z * (1 + z2 * (-1.f / 6 + z2 * (1.f / 120 + z2 * (-1.f / 5040 + z2 * (1.f / 362880 + z2 * (-1.f / 39916800))))));
}

// PolyBLEP residual for a unit step at phase 0; dt = phase increment.
inline float polyBlep(float t, float dt, float invDt) {
  if (t < dt) {
    t *= invDt;
    return t + t - t * t - 1;
  }
  if (t > 1 - dt) {
    t = (t - 1) * invDt;
    return t * t + t + t + 1;
  }
  return 0;
}

double besselI0(double x) {
  double sum = 1, term = 1;
  for (int k = 1; k < 30; k++) {
    term *= (x / (2 * k)) * (x / (2 * k));
    sum += term;
  }
  return sum;
}
}  // namespace

float oscillatorSample(float phase, Wave wave) {
  float t = fmodf(phase, 1.0f);
  if (t < 0) t += 1.0f;
  if (wave == Wave::Saw) return 2 * t - 1;
  if (wave == Wave::Triangle) return 1 - 4 * fabsf(t - 0.5f);
  return sinf(kTwoPi * t);
}

float reflect(float x) {
  float wrapped = fmodf(x + 1, 4.0f);
  if (wrapped < 0) wrapped += 4.0f;
  return wrapped <= 2 ? wrapped - 1 : 3 - wrapped;
}

float foldSample(float x, float foldPct, Harmonics harmonics) {
  if (foldPct <= 0) return x;
  const float amount = foldPct / 100, driven = x * (1 + 8 * amount), bias = .55f * amount;
  const float odd = reflect(driven);
  const float even = (reflect(driven + bias) + reflect(-driven + bias)) * .5f;
  return harmonics == Harmonics::Even ? x + even : harmonics == Harmonics::Both ? odd + even : odd;
}

Calibration calibrationFor(Wave wave, float foldPct, Harmonics harmonics) {
  constexpr int count = 4096;
  double sum = 0, squared = 0;
  for (int i = 0; i < count; i++) {
    const double x = foldSample(oscillatorSample(float(i) / count, wave), foldPct, harmonics);
    sum += x;
    squared += x * x;
  }
  const double mean = sum / count;
  double variance = squared / count - mean * mean;
  if (variance < 1e-12) variance = 1e-12;
  return {float(mean), float(.36 / sqrt(variance))};
}

Calibration calibrationLookup(Wave wave, float foldPct, Harmonics harmonics) {
  const int w = uint8_t(wave) <= 2 ? int(wave) : 0, h = uint8_t(harmonics) <= 2 ? int(harmonics) : 0;
  const int f = !isfinite(foldPct) || foldPct < 0 ? 0 : foldPct > 100 ? 1000 : int(roundf(foldPct * 10));
  return kCalibrationTable[w][h][f];
}

Synth::Synth(float sampleRate) : fs_(sampleRate) {
  hpR_ = 1 - kTwoPi * 5 / fs_;  // 5 Hz DC blocker, as in the simulator
  waveGain_[0].value = 1;
  drive_.value = 1;
  oddMix_.value = 1;
  // Time constants match AudioPreview.update() in dsp.js.
  cFreq_ = coeff(.015f), cWave_ = coeff(.012f), cParam_ = coeff(.02f), cMute_ = coeff(.01f);

  // Kaiser-windowed sinc low-pass at the inner rate: cutoff 22 kHz, beta 7
  // (about -70 dB stop band from ~28 kHz, -0.1 dB at 10 kHz).
  const double fsInner = double(fs_) * kOversample, cutoff = 22000 / fsInner, beta = 7;
  double sum = 0, taps[kTaps];
  for (int i = 0; i < kTaps; i++) {
    const double m = i - (kTaps - 1) / 2.0, r = 2 * m / (kTaps - 1);
    const double sinc = m == 0 ? 2 * cutoff : sin(2 * kPi * cutoff * m) / (kPi * m);
    taps[i] = sinc * besselI0(beta * sqrt(1 - r * r)) / besselI0(beta);
    sum += taps[i];
  }
  for (int i = 0; i < kTaps; i++) fir_[i] = float(taps[i] / sum);
}

float Synth::coeff(float seconds) const { return 1 - expf(-1 / (seconds * fs_)); }

void Synth::render(float* out, int frames, const SynthTarget& t) {
  const float amount = t.foldPct / 100;
  const bool even = t.harmonics == Harmonics::Even, odd = t.harmonics == Harmonics::Odd;
  const float innerRate = fs_ * kOversample;
  float peak = 0;

  for (int n = 0; n < frames; n++) {
    if (level_.value < 1e-4f) {
      // Silent (-80 dB, ~75-90 ms after a release): jump straight to the new
      // sound so a pad starts on its own pitch and wave. A pad pressed sooner
      // glides from the previous pitch, as the simulator always does.
      freq_.value = t.frequencyHz;
      for (int w = 0; w < 3; w++) waveGain_[w].value = int(t.wave) == w ? 1.f : 0.f;
      drive_.value = 1 + 8 * amount;
      bias_.value = .55f * amount;
      oddMix_.value = even ? 0.f : 1.f;
      dryMix_.value = even ? 1.f : 0.f;
      evenMix_.value = odd ? 0.f : .5f;
      mean_.value = t.mean;
    }
    freq_.step(t.frequencyHz, cFreq_);
    if (fabsf(t.frequencyHz - freq_.value) < 1e-3f) freq_.value = t.frequencyHz;  // float glide can stall
    for (int w = 0; w < 3; w++) waveGain_[w].step(int(t.wave) == w ? 1.f : 0.f, cWave_);
    drive_.step(1 + 8 * amount, cParam_);
    bias_.step(.55f * amount, cParam_);
    oddMix_.step(even ? 0.f : 1.f, cParam_);
    dryMix_.step(even ? 1.f : 0.f, cParam_);
    evenMix_.step(odd ? 0.f : .5f, cParam_);
    mean_.step(t.mean, cParam_);
    level_.step(t.level, t.fastRelease ? cMute_ : cParam_);

    const float dt = freq_.value / innerRate, invDt = 1 / dt;
    const float gSine = waveGain_[0].value, gSaw = waveGain_[1].value, gTri = waveGain_[2].value;
    const float drive = drive_.value, bias = bias_.value;
    const float oddMix = oddMix_.value, dryMix = dryMix_.value, evenMix = evenMix_.value;
    for (int k = 0; k < kOversample; k++) {
      phase_ += dt;
      if (phase_ >= 1) phase_ -= 1;
      float s = 0;
      if (gSine > 1e-4f) s += gSine * fastSin(phase_);
      if (gSaw > 1e-4f) s += gSaw * (2 * phase_ - 1 - polyBlep(phase_, dt, invDt));
      if (gTri > 1e-4f) s += gTri * (1 - 4 * fabsf(phase_ - .5f));
      const float d = drive * s;
      float y = dryMix * s;
      if (oddMix > 1e-4f) y += oddMix * fastReflect(d);
      if (evenMix > 1e-4f) y += evenMix * (fastReflect(d + bias) + fastReflect(-d + bias));
      history_[historyPos_] = history_[historyPos_ + kTaps] = y;
      if (++historyPos_ == kTaps) historyPos_ = 0;
    }
    // history_[historyPos_ .. +kTaps) holds the last kTaps inner samples, oldest first.
    const float* h = history_ + historyPos_;
    float acc = 0;
    for (int i = 0; i < kTaps; i++) acc += fir_[i] * h[i];

    const float x = acc - mean_.value;
    hpY_ = x - hpX_ + hpR_ * hpY_;
    hpX_ = x;
    float v = hpY_ * level_.value;
    if (v > 1) v = 1;
    if (v < -1) v = -1;
    out[n] = v;
    if (fabsf(v) > peak) peak = fabsf(v);
  }
  peak_ = peak;
}
