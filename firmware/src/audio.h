#pragma once
// I2S output to the GY-PCM5102: 48 kHz, 32-bit slots (64 Fs), the same
// mono signal on L and R. Rendering runs in its own task on core 1; Wi-Fi/HTTP use core 0.

#include <stdint.h>

#include "dsp.h"

constexpr int kSampleRate = 48000;
// Hardware trim stays at 0.5 (about 0.18 full-scale digital RMS at LEVEL 100).
// The current browser preview has no 0.12 attenuation. Commission the physical
// amplifier/shaker separately; do not infer a safe output from browser loudness.
constexpr float kOutputTrim = 0.5f;

struct AudioStats {
  float peak;          // last block's peak, 0..1 before trim
  float loadPct;       // highest render time per block in the last second, % of real time
  uint32_t stalls;     // i2s_write calls that timed out (DMA not draining)
};

void audioBegin();
void audioSetTarget(const SynthTarget& target);
AudioStats audioStats();
