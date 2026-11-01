#include "protocol.h"
#include <ArduinoJson.h>
#include <string.h>
#include <math.h>

namespace {
bool number(JsonVariantConst v, float lo, float hi, float* out) {
  if ((!v.is<float>() && !v.is<int>()) || v.is<bool>()) return false;
  float x = v.as<float>();
  if (!isfinite(x) || x < lo || x > hi) return false;
  *out = x; return true;
}
bool choice(JsonVariantConst v, const char* a, const char* b, const char* c, uint8_t* out) {
  if (!v.is<const char*>()) return false;
  const char* s = v.as<const char*>();
  if (!strcmp(s, a)) *out = 0;
  else if (!strcmp(s, b)) *out = 1;
  else if (c && !strcmp(s, c)) *out = 2;
  else return false;
  return true;
}
bool parameters(JsonVariantConst v, Parameters* p) {
  float level; uint8_t wave, harmonic;
  if (!v.is<JsonObjectConst>() || !number(v["frequencyHz"], kMinHz, kMaxHz, &p->frequencyHz) ||
      !number(v["levelPct"], 0, 100, &level) || level != roundf(level) ||
      !number(v["foldPct"], 0, 100, &p->foldPct) ||
      !choice(v["wave"], "sine", "sawtooth", "triangle", &wave) ||
      !choice(v["harmonics"], "odd", "even", "both", &harmonic)) return false;
  p->levelPct = int(level); p->wave = Wave(wave); p->harmonics = Harmonics(harmonic);
  *p = cleanParameters(*p); return true;
}
void writeParameters(JsonObject o, const Parameters& p) {
  o["frequencyHz"] = round(double(p.frequencyHz) * 10) / 10; o["levelPct"] = p.levelPct; o["foldPct"] = round(double(p.foldPct) * 10) / 10;
  o["wave"] = waveName(p.wave); o["harmonics"] = harmonicsName(p.harmonics);
}
void writePresets(JsonArray a, const Preset presets[kPadCount]) {
  for (int i = 0; i < kPadCount; ++i) {
    if (presets[i].used) writeParameters(a.createNestedObject(), presets[i].parameters);
    else a.add(nullptr);
  }
}
}
bool decodeCommand(const char* text, size_t size, Command* out, const char** error) {
  *error = "Perintah atau angka tidak valid.";
  if (!size || size > 4096) return false;
  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, text, size) || !doc.is<JsonObject>() || !doc["action"].is<const char*>()) return false;
  const char* action = doc["action"];
  Command c;
  if (!strcmp(action, "heartbeat")) {
    c.action = Action::Heartbeat;
    float index;
    if (!number(doc["heldPad"], -1, kPadCount - 1, &index) || index != roundf(index)) return false;
    c.index = int(index);
  } else if (!strcmp(action, "release")) c.action = Action::Release;
  else if (!strcmp(action, "mode")) {
    uint8_t mode;
    if (!choice(doc["mode"], "run", "service", nullptr, &mode)) return false;
    c.action = Action::Mode; c.flag = mode == 1;
  } else if (!strcmp(action, "parameters")) {
    c.action = Action::Parameters;
    if (!parameters(doc["parameters"], &c.parameters)) return false;
  } else if (!strcmp(action, "test")) c.action = Action::Test;
  else if (!strcmp(action, "mute")) c.action = Action::Mute;
  else if (!strcmp(action, "stop")) c.action = Action::Stop;
  else if (!strcmp(action, "factory")) c.action = Action::Factory;
  else if (!strcmp(action, "padDown") || !strcmp(action, "padUp") || !strcmp(action, "save")) {
    float index;
    if (!number(doc["index"], 0, kPadCount - 1, &index) || index != roundf(index)) return false;
    c.index = int(index); c.action = !strcmp(action, "padDown") ? Action::PadDown : !strcmp(action, "padUp") ? Action::PadUp : Action::Save;
  } else if (!strcmp(action, "setup")) {
    JsonVariantConst v = doc["setup"];
    if (!number(v["minHz"], kMinHz, kMaxHz, &c.setup.minHz) || !number(v["maxHz"], kMinHz, kMaxHz, &c.setup.maxHz)) return false;
    c.setup.minHz = roundf(c.setup.minHz * 10) / 10; c.setup.maxHz = roundf(c.setup.maxHz * 10) / 10;
    c.setup.calibrated = true; // legacy LED bytes are retained only for NVS compatibility
    if (!validSetup(c.setup)) return false;
    c.action = Action::Setup;
  } else if (!strcmp(action, "import")) {
    JsonVariantConst v = doc["file"];
    if (!v["schema"].is<const char*>() || !v["folding"].is<const char*>() ||
        strcmp(v["schema"], "chladni-08") || strcmp(v["folding"], "parity-fold-v2") || v["version"] != 4 ||
        !v["presets"].is<JsonArrayConst>()) return false;
    JsonArrayConst a = v["presets"];
    if (a.size() != kPadCount && a.size() != 8) return false;
    // Validate every legacy slot before retaining the first four (atomic migration).
    for (size_t i = 0; i < a.size(); ++i) {
      if (a[i].isNull()) continue;
      Parameters p;
      if (!parameters(a[i], &p)) return false;
      if (i < kPadCount) c.presets[i] = {true, p};
    }
    c.action = Action::Import;
  } else if (!strcmp(action, "sequence")) {
    uint8_t mode;
    if (!choice(doc["mode"], "ordered", "random", nullptr, &mode)) return false;
    c.action = Action::Sequence; c.flag = mode == 1;
  } else if (!strcmp(action, "gatherStart")) c.action = Action::GatherStart;
  else if (!strcmp(action, "gatherConfigure")) {
    JsonArrayConst a = doc["steps"].as<JsonArrayConst>();
    if (a.size() < 1 || a.size() > kGatherMaxSteps) return false;
    c.gather.count = int(a.size());
    for (int i = 0; i < c.gather.count; ++i) {
      const auto v = a[i]; auto& s = c.gather.steps[i];
      if (!number(v["frequencyHz"], kMinHz, kMaxHz, &s.frequencyHz) || !number(v["levelPct"], 0, 100, &s.levelPct) ||
          !number(v["glideS"], 0, 60, &s.glideS) || !number(v["holdS"], 0, 60, &s.holdS)) return false;
    }
    if (!validGather(c.gather)) return false;
    c.action = Action::GatherConfigure;
  } else return false;
  *out = c; return true;
}
std::string encodePresets(const Preset presets[kPadCount]) {
  DynamicJsonDocument doc(2048);
  doc["schema"] = "chladni-08"; doc["version"] = 4; doc["folding"] = "parity-fold-v2";
  writePresets(doc.createNestedArray("presets"), presets);
  std::string out; serializeJson(doc, out); return out;
}
std::string encodeState(const ControlSnapshot& s, float peak, float loadPct, uint32_t stalls) {
  DynamicJsonDocument doc(6144);
  doc["uptimeMs"] = s.uptimeMs;
  doc["firmware"] = "cp100-remote-1.0.0-minimal";
  auto hardware = doc.createNestedObject("hardware"); hardware["display"] = false; hardware["leds"] = false; doc["ssid"] = "ATMOS-CP-300-EXP"; doc["address"] = "192.168.4.1";
  doc["mode"] = s.state.mode == Mode::Run ? "run" : "service";
  doc["muted"] = s.state.muted; doc["activePad"] = s.state.activePad; doc["sessionActive"] = s.owner != 0;
  writeParameters(doc.createNestedObject("parameters"), s.state.parameters);
  writePresets(doc.createNestedArray("presets"), s.state.presets);
  auto setup = doc.createNestedObject("setup");
  setup["minHz"] = round(double(s.state.setup.minHz) * 10) / 10; setup["maxHz"] = round(double(s.state.setup.maxHz) * 10) / 10; setup["calibrated"] = s.state.setup.calibrated;
  auto playback = doc.createNestedObject("playback");
  playback["kind"] = s.playback == Playback::Ordered ? "ordered" : s.playback == Playback::Random ? "random" : s.playback == Playback::Gather ? "gather" : "idle";
  playback["step"] = s.step; playback["elapsedS"] = s.elapsedS;
  auto gather = doc.createNestedArray("gather");
  for (int i = 0; i < s.gather.count; ++i) {
    auto step = gather.createNestedObject(); const auto& v = s.gather.steps[i];
    step["frequencyHz"] = round(double(v.frequencyHz) * 10) / 10; step["levelPct"] = v.levelPct; step["glideS"] = round(double(v.glideS) * 10) / 10; step["holdS"] = round(double(v.holdS) * 10) / 10;
  }
  auto audio = doc.createNestedObject("audio"); audio["peak"] = peak; audio["loadPct"] = loadPct; audio["stalls"] = stalls;
  std::string out; serializeJson(doc, out); return out;
}
