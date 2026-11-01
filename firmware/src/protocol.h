#pragma once
#include <stddef.h>
#include <string>
#include "controller.h"
bool decodeCommand(const char* json, size_t size, Command* command, const char** error);
std::string encodeState(const ControlSnapshot& state, float peak, float loadPct, uint32_t stalls);
std::string encodePresets(const Preset presets[kPadCount]);
