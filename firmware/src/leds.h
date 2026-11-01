#pragma once
// WS2812B under each pad (4 in a chain) on GPIO38 through the AHCT buffer.
// Standby white, active pink or tosca, optional pulse (model.cpp ledState).

#include <stdint.h>

#include "model.h"

// Four pad LEDs; brightness remains at the commissioned bench default.
constexpr float kLedBrightness = 0.35f;

void ledsBegin();
void ledsUpdate(const State& state, uint32_t nowMs);  // call every loop; writes only when needed
