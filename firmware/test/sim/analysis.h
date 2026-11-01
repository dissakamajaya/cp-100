#pragma once
// Measurement helpers for the captured I2S stream (floats, 1.0 = full scale).

#include <math.h>

#include <algorithm>
#include <vector>

namespace ana {

constexpr double kPi = 3.14159265358979323846;

inline double rms(const std::vector<float>& x) {
  double s = 0;
  for (float v : x) s += double(v) * v;
  return x.empty() ? 0 : sqrt(s / x.size());
}

inline double peak(const std::vector<float>& x) {
  double p = 0;
  for (float v : x) p = std::max(p, double(fabsf(v)));
  return p;
}

inline double db(double x) { return 20 * log10(std::max(x, 1e-12)); }

// Amplitude of the component at f (Hann window), exact for a stationary sinusoid.
inline double amplitudeAt(const std::vector<float>& x, double f, double fs) {
  const size_t n = x.size();
  if (n < 2) return 0;
  double re = 0, im = 0, wsum = 0;
  const double w0 = 2 * kPi * f / fs;
  // recursive phasor to keep it fast
  double c = 1, s = 0;
  const double cw = cos(w0), sw = sin(w0);
  for (size_t i = 0; i < n; i++) {
    const double w = 0.5 - 0.5 * cos(2 * kPi * i / (n - 1));
    re += w * x[i] * c;
    im -= w * x[i] * s;
    wsum += w;
    const double nc = c * cw - s * sw;
    s = s * cw + c * sw;
    c = nc;
    if ((i & 1023) == 1023) {  // renormalise the phasor
      const double m = sqrt(c * c + s * s);
      c /= m, s /= m;
    }
  }
  return 2 * sqrt(re * re + im * im) / wsum;
}

// Fundamental frequency: normalised autocorrelation for the period, then a
// golden-section search of the Hann-windowed DTFT peak around it.
inline double fundamental(const std::vector<float>& x, double fs, double fmin = 15, double fmax = 2200) {
  const size_t n = x.size();
  const int lagMin = int(fs / fmax), lagMax = int(fs / fmin);
  const size_t len = std::min<size_t>(n > size_t(lagMax) + 2 ? n - lagMax - 2 : 0, 8192);
  if (len < 256) return 0;
  std::vector<double> r(lagMax + 2, 0);
  double e0 = 0;
  for (size_t i = 0; i < len; i++) e0 += double(x[i]) * x[i];
  if (e0 <= 0) return 0;
  double best = -1;
  for (int lag = lagMin; lag <= lagMax + 1; lag++) {
    double s = 0, e1 = 0;
    for (size_t i = 0; i < len; i++) {
      s += double(x[i]) * x[i + lag];
      e1 += double(x[i + lag]) * x[i + lag];
    }
    r[lag] = s / sqrt(e0 * e1 + 1e-30);
    best = std::max(best, r[lag]);
  }
  int lag = -1;
  for (int k = lagMin + 1; k <= lagMax; k++)
    if (r[k] >= r[k - 1] && r[k] >= r[k + 1] && r[k] > 0.9 * best && r[k] > 0.5) {
      lag = k;
      break;
    }
  if (lag < 0) return 0;
  const double a = r[lag - 1], b = r[lag], c = r[lag + 1];
  const double den = a - 2 * b + c;
  const double frac = den != 0 ? 0.5 * (a - c) / den : 0;
  double f = fs / (lag + frac);
  // refine
  double lo = f * 0.985, hi = f * 1.015;
  const double g = (sqrt(5.0) - 1) / 2;
  double x1 = hi - g * (hi - lo), x2 = lo + g * (hi - lo);
  double f1 = amplitudeAt(x, x1, fs), f2 = amplitudeAt(x, x2, fs);
  for (int it = 0; it < 40; it++) {
    if (f1 > f2) {
      hi = x2, x2 = x1, f2 = f1, x1 = hi - g * (hi - lo), f1 = amplitudeAt(x, x1, fs);
    } else {
      lo = x1, x1 = x2, f1 = f2, x2 = lo + g * (hi - lo), f2 = amplitudeAt(x, x2, fs);
    }
  }
  return (lo + hi) / 2;
}

// Harmonic amplitudes 1..k (Hann) at multiples of f0.
inline std::vector<double> harmonics(const std::vector<float>& x, double f0, double fs, int k) {
  std::vector<double> out;
  for (int h = 1; h <= k; h++) out.push_back(amplitudeAt(x, f0 * h, fs));
  return out;
}

inline double maxStep(const std::vector<float>& x) {
  double m = 0;
  for (size_t i = 1; i < x.size(); i++) m = std::max(m, double(fabsf(x[i] - x[i - 1])));
  return m;
}

// Signal with its fundamental and harmonics removed is hard; instead the
// "low-frequency" content: one-pole low-pass at fc, peak absolute value.
inline double lowFrequencyPeak(const std::vector<float>& x, double fc, double fs) {
  const double a = 1 - exp(-2 * kPi * fc / fs);
  double y = 0, p = 0;
  for (float v : x) {
    y += a * (v - y);
    y += 0;  // second pole
    p = std::max(p, fabs(y));
  }
  return p;
}

}  // namespace ana
