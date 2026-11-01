#pragma once
// Service panel switches behind the MCP23017 (Rev08 wiring page 12):
// MODE on GPA0, WAVE on GPA1/GPA2, HARMONICS on GPA3/GPA6, FOLD encoder on GPA4/GPA5.
// All inputs have 10k pull-ups to 3V3 and switch to GND. Poll from loop().
// Without the chip the panel reports absent and the serial console stands in.

#include <stdint.h>

#include "model.h"

struct PanelState {
  bool present = false;
  Mode mode = Mode::Run;
  Wave wave = Wave::Sine;
  Harmonics harmonics = Harmonics::Odd;
};

struct PanelEvents {
  bool connected = false;     // chip found (at boot or later)
  bool disconnected = false;  // chip stopped answering
  bool modeChanged = false;
  bool waveChanged = false;
  bool harmonicsChanged = false;
  int foldSteps = 0;
};

void panelBegin();
PanelEvents panelPoll();
const PanelState& panelState();
