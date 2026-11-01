#pragma once
#include "controller.h"
void storageLoad(Preset presets[kPadCount], Setup* setup);
bool storageSavePresets(const Preset presets[kPadCount]);
bool storageSaveSetup(const Setup& setup);
GatherRoutine storageLoadGather();
bool storageSaveGather(const GatherRoutine& routine);
bool storageLoadWifi(char password[17]);
bool storageSaveWifi(const char password[17]);
