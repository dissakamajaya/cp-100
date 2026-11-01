#pragma once
// Quadrature decoder for the Bourns PEC11R-4215F-S0024 (24 detents, 24 pulses:
// one full A/B cycle per detent, both contacts open = HIGH at rest).
// A step is counted when the shaft settles back on a detent, so contact
// bounce and half-turned positions never produce stray or doubled steps.

#include <stdint.h>

class Encoder {
 public:
  // a, b: true = HIGH (open). Returns +1 (clockwise), -1, or 0.
  int update(bool a, bool b) {
    static const int8_t kTable[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    const uint8_t now = (a ? 2 : 0) | (b ? 1 : 0);
    if (now == prev_) return 0;
    quarters_ += kTable[(prev_ << 2) | now];
    prev_ = now;
    if (now != 3) return 0;
    const int step = quarters_ >= 2 ? 1 : quarters_ <= -2 ? -1 : 0;
    quarters_ = 0;
    return step;
  }

 private:
  uint8_t prev_ = 3;
  int8_t quarters_ = 0;
};
