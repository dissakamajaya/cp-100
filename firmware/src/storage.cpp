#include "storage.h"
#include <Preferences.h>
#include <string.h>

namespace {
constexpr uint8_t kLayout = 2;
constexpr const char* kNamespace = "cp100";
constexpr size_t kMaxBlob = 512;
// Explicit byte flags avoid interpreting corrupt raw NVS bytes as C++ bools.
struct StoredPreset { uint8_t used, wave, harmonics, reserved; float hz, fold; int32_t level; };
struct StoredSetup { float minHz, maxHz; uint8_t calibrated, color, pulse, reserved; };
struct LegacyParameters { float hz; int32_t level; uint8_t wave, pad1[3]; int32_t fold; uint8_t harmonics, pad2[3]; };
struct LegacyPreset { uint8_t used, pad[3]; LegacyParameters p; };
static_assert(sizeof(StoredPreset) == 16 && sizeof(StoredSetup) == 12 && sizeof(LegacyPreset) == 24, "NVS layouts");

bool loadBlob(Preferences& prefs, const char* key, void* out, size_t size, uint8_t layout) {
  uint8_t buffer[1 + kMaxBlob];
  if (size > kMaxBlob || prefs.getBytesLength(key) != size + 1 || prefs.getBytes(key, buffer, size + 1) != size + 1 || buffer[0] != layout) return false;
  memcpy(out, buffer + 1, size); return true;
}
bool saveBlob(const char* key, const void* data, size_t size) {
  if (size > kMaxBlob) return false;
  uint8_t buffer[1 + kMaxBlob]; buffer[0] = kLayout;
  memcpy(buffer + 1, data, size);
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  const bool ok = prefs.putBytes(key, buffer, size + 1) == size + 1;
  prefs.end(); return ok;
}
}
void storageLoad(Preset presets[kPadCount], Setup* setup) {
  examplePresets(presets); *setup = kDefaultSetup;
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return;
  StoredPreset disk[kPadCount];
  LegacyPreset legacy[8];
  if (loadBlob(prefs, "presets", disk, sizeof(disk), kLayout)) {
    for (int i = 0; i < kPadCount; ++i) {
      const auto& d = disk[i];
      Parameters p{d.hz, d.level, Wave(d.wave), d.fold, Harmonics(d.harmonics)};
      presets[i] = d.used == 1 && validParameters(p) ? Preset{true, cleanParameters(p)} : Preset{false, {}};
    }
  } else if (loadBlob(prefs, "presets", legacy, sizeof(legacy), 1)) {
    // Retain the first four slots; the old blob is not overwritten until an explicit save.
    for (int i = 0; i < kPadCount; ++i) {
      const auto& d = legacy[i]; const auto& l = d.p;
      Parameters p{l.hz, l.level, Wave(l.wave), float(l.fold), Harmonics(l.harmonics)};
      presets[i] = d.used == 1 && validParameters(p) ? Preset{true, cleanParameters(p)} : Preset{false, {}};
    }
  }
  StoredSetup diskSetup;
  if (loadBlob(prefs, "setup", &diskSetup, sizeof(diskSetup), kLayout) || loadBlob(prefs, "setup", &diskSetup, sizeof(diskSetup), 1)) {
    Setup s{diskSetup.minHz, diskSetup.maxHz, diskSetup.calibrated == 1, LedColor(diskSetup.color), diskSetup.pulse == 1};
    if (diskSetup.calibrated <= 1 && diskSetup.pulse <= 1 && validSetup(s)) *setup = s;
  }
  prefs.end();
}
bool storageSavePresets(const Preset presets[kPadCount]) {
  StoredPreset disk[kPadCount] = {};
  for (int i = 0; i < kPadCount; ++i) {
    const auto& p = presets[i].parameters;
    if (presets[i].used && !validParameters(p)) return false;
    if (presets[i].used) disk[i] = {1, uint8_t(p.wave), uint8_t(p.harmonics), 0, p.frequencyHz, p.foldPct, p.levelPct};
  }
  return saveBlob("presets", disk, sizeof(disk));
}
bool storageSaveSetup(const Setup& s) {
  if (!validSetup(s)) return false;
  StoredSetup d{s.minHz, s.maxHz, uint8_t(s.calibrated), uint8_t(s.ledColor), uint8_t(s.ledPulse), 0};
  return saveBlob("setup", &d, sizeof(d));
}
GatherRoutine storageLoadGather() {
  GatherRoutine r;
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return exampleGather();
  const bool loaded = loadBlob(prefs, "gather", &r, sizeof(r), kLayout);
  prefs.end(); return loaded && validGather(r) ? r : exampleGather();
}
bool storageSaveGather(const GatherRoutine& r) { return validGather(r) && saveBlob("gather", &r, sizeof(r)); }
bool storageLoadWifi(char password[17]) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return false;
  const bool ok = prefs.getBytesLength("wifi") == 17 && prefs.getBytes("wifi", password, 17) == 17;
  prefs.end();
  if (!ok || password[16] != 0) return false;
  for (int i = 0; i < 16; ++i)
    if (!((password[i] >= '0' && password[i] <= '9') || (password[i] >= 'a' && password[i] <= 'f'))) return false;
  return true;
}
bool storageSaveWifi(const char password[17]) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  const bool ok = prefs.putBytes("wifi", password, 17) == 17;
  prefs.end(); return ok;
}
