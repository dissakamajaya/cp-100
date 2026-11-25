// CP-100 on the Waveshare ESP32-S3-Touch-AMOLED-2.16: the four pads are a
// touch grid, sound goes to the onboard ES8311 speaker, and the phone service
// panel over Wi-Fi is unchanged from the main firmware. Holding the CP-100
// title for 3 s opens a simplified on-screen SERVICE editor.
#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include "../audio.h"
#include "../controller.h"
#include "../protocol.h"
#include "../remote.h"
#include "../storage.h"
#include "board.h"
#include "screen.h"
#include "touch.h"

namespace {
Controller controller;
LocalView view;

SynthTarget targetFor(const State& s) {
  const auto& p = s.parameters;
  const auto cal = calibrationLookup(p.wave, p.foldPct, p.harmonics);
  SynthTarget t;
  t.frequencyHz = p.frequencyHz; t.foldPct = p.foldPct;
  t.wave = p.wave; t.harmonics = p.harmonics; t.mean = cal.mean;
  t.level = s.muted ? 0 : p.levelPct / 100.f * cal.gain;
  t.padPressId = controller.padPressId();
  t.padMaxFrames = kPadMaxHoldMs * (kSampleRate / 1000);
  t.fastRelease = s.muted; return t;
}
void quiesceAudio() {
  auto target = targetFor(controller.state()); target.level = 0; target.fastRelease = true;
  audioSetTarget(target);
  const uint32_t start = millis();
  while (audioStats().peak > 1e-4f && millis() - start < 150) delay(2);
  delay(12);
}
// Applies a command and persists what it changed, for the phone and the screen alike.
ControlResult applyAndPersist(const Command& command, bool* persisted) {
  *persisted = true;
  const ControlResult result = controller.apply(command, millis());
  if (result.ok && (result.presets || result.setup || result.gather)) {
    quiesceAudio();
    if (result.presets) *persisted = storageSavePresets(controller.state().presets) && *persisted;
    if (result.setup) *persisted = storageSaveSetup(controller.state().setup) && *persisted;
    if (result.gather) *persisted = storageSaveGather(controller.gather()) && *persisted;
    if (!*persisted) Serial.println("NVS write failed; changes remain in RAM only.");
  }
  return result;
}
void serviceRequest() {
  RemoteRequest request;
  if (!remoteReceive(&request)) return;
  ControlResult result;
  bool persisted = true;
  if (int32_t(millis() - request.deadline) > 0) {
    result.ok = false; result.message = "Perintah kedaluwarsa. Coba lagi.";
  } else {
    result = applyAndPersist(request.command, &persisted);
  }
  controller.tick(millis());
  audioSetTarget(targetFor(controller.state()));
  remotePublish(controller.snapshot(millis()));
  remoteReply(request.id, result, persisted);
}

// ---- On-screen SERVICE editor: a local control session like the phone's ----
constexpr uint32_t kEnterHoldMs = 3000, kRepeatDelayMs = 400, kRepeatMs = 80, kToastMs = 1500;
char toastText[32];
uint32_t toastAt = 0, testAt = 0, heartbeatAt = 0;

ControlResult local(Action action, int index = -1, const Parameters* p = nullptr) {
  Command c; c.action = action; c.owner = kLocalOwner; c.index = index;
  if (p) c.parameters = *p;
  bool persisted;
  const ControlResult r = applyAndPersist(c, &persisted);
  if (action == Action::Save && r.ok) {
    snprintf(toastText, sizeof toastText, persisted ? "Tersimpan ke PAD %d" : "PAD %d: RAM saja", index + 1);
    view.toast = toastText; toastAt = millis();
  }
  return r;
}
void loadPad(int i) {
  view.pad = i;
  const Preset& preset = controller.state().presets[i];
  if (!preset.used) return;
  Parameters p = preset.parameters;
  p.wave = Wave::Sine;  // the on-screen editor is sine-only
  local(Action::Parameters, -1, &p);
}
void enterService() {
  if (!local(Action::Claim).ok) return;  // phone holds the session
  view.service = true; view.testing = false; heartbeatAt = millis();
  loadPad(0);
}
void leaveService() {
  local(Action::Release);
  view = LocalView{};
}
void stopTest() {
  if (!view.testing) return;
  view.testing = false; local(Action::Mute);
}
void editParameters(const HitResult& h) {
  Parameters p = controller.state().parameters;
  const Setup& s = controller.state().setup;
  if (h.hit == Hit::Freq) p.frequencyHz = constrain(roundf((p.frequencyHz + h.value) * 10) / 10, s.minHz, s.maxHz);
  else if (h.hit == Hit::LevelBar) p.levelPct = int(h.value);
  else if (h.hit == Hit::Harmonics) p.harmonics = h.index == 1 ? Harmonics::Even : h.index == 2 ? Harmonics::Both : Harmonics::Odd;
  p.wave = Wave::Sine;
  local(Action::Parameters, -1, &p);
}
void servicePress(const HitResult& h) {
  switch (h.hit) {
    case Hit::Done: leaveService(); break;
    case Hit::Tab: stopTest(); loadPad(h.index); break;
    case Hit::Freq: case Hit::LevelBar: case Hit::Harmonics: editParameters(h); break;
    case Hit::Test: if (local(Action::Test).ok) { view.testing = true; testAt = millis(); } break;
    case Hit::Save: stopTest(); local(Action::Save, view.pad); break;
    default: break;
  }
}

// RUN: touch behaves like the physical pads (press = down, lift or slide off = up).
// SERVICE: buttons act on press; FREQ steps repeat while held; LEVEL follows the finger.
bool pollTouch() {
  static uint32_t lastPoll = 0, downAt = 0, repeatAt = 0;
  static bool wasDown = false;
  static HitResult pressed;
  const uint32_t now = millis();
  if (now - lastPoll < 10) return wasDown;
  lastPoll = now;
  const TouchPoint t = touchRead();
  const bool edge = t.down && !wasDown;
  if (edge) downAt = now;

  if (!view.service) {
    const HitResult h = t.down ? screenHit(t.x, t.y, false) : HitResult{};
    const int pad = h.hit == Hit::Tab ? h.index : -1;
    if (pad != view.pressed) {
      if (view.pressed >= 0) controller.physicalUp(view.pressed);
      view.pressed = pad;
      if (pad >= 0) controller.physicalDown(pad, now);
    }
    if (edge) pressed = h;
    if (t.down && pressed.hit == Hit::Title && h.hit == Hit::Title && now - downAt >= kEnterHoldMs) {
      pressed = {};
      enterService();
    }
  } else if (edge) {
    pressed = screenHit(t.x, t.y, true);
    repeatAt = now + kRepeatDelayMs;
    servicePress(pressed);
  } else if (t.down && view.service) {
    if (pressed.hit == Hit::Freq && int32_t(now - repeatAt) >= 0) { repeatAt = now + kRepeatMs; editParameters(pressed); }
    if (pressed.hit == Hit::LevelBar) {
      const HitResult h = screenHit(t.x, t.y, true);
      if (h.hit == Hit::LevelBar && int(h.value) != controller.state().parameters.levelPct) editParameters(h);
    }
  } else if (!t.down && wasDown) {
    if (pressed.hit == Hit::Test) stopTest();
    pressed = {};
  }
  wasDown = t.down;
  return t.down;
}
void serviceTick() {
  const uint32_t now = millis();
  if (view.toast && now - toastAt > kToastMs) view.toast = nullptr;
  if (!view.service) return;
  if (controller.snapshot(now).owner != kLocalOwner) { view = LocalView{}; return; }
  if (view.testing && now - testAt >= kPadMaxHoldMs) stopTest();  // same 5 s ceiling as a pad
  if (now - heartbeatAt >= 500) { heartbeatAt = now; local(Action::Heartbeat); }
}
void pollConsole() {
  static char line[48]; static size_t size = 0; static bool overflow = false;
  for (int budget = 0; budget < 32 && Serial.available(); ++budget) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      line[size] = 0;
      if (overflow) Serial.println("Command too long.");
      else if (!strcmp(line, "mute")) controller.emergencyMute();
      else if (!strcmp(line, "wifi")) remotePrintWifi();
      else if (!strcmp(line, "status")) {
        const auto a = audioStats();
        Serial.println(encodeState(controller.snapshot(millis()), a.peak, a.loadPct, a.stalls).c_str());
      } else if (!strcmp(line, "export")) Serial.println(encodePresets(controller.state().presets).c_str());
      else if (size) Serial.println("Commands: status | wifi | export | mute. Service controls: http://192.168.4.1");
      size = 0; overflow = false; break;
    } else if (size < sizeof(line) - 1) line[size++] = c;
    else overflow = true;
  }
}
}  // namespace

void setup() {
  Serial.setTxBufferSize(4096); Serial.begin(115200);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  screenBegin();
  touchBegin();
  Preset presets[kPadCount]; Setup setup;
  storageLoad(presets, &setup);
  controller.begin(presets, setup, storageLoadGather());
  audioBegin(); audioSetTarget(targetFor(controller.state()));
  remoteBegin(controller.snapshot(millis()));
  Serial.println("CP-100 AMOLED: touch pads / Wi-Fi service. Commands: status | wifi | export | mute");
  enableLoopWDT();
}

void loop() {
  controller.tick(millis());
  const bool touched = pollTouch();
  serviceTick();
  serviceRequest();
  pollConsole();
  controller.tick(millis());
  audioSetTarget(targetFor(controller.state()));
  const auto snap = controller.snapshot(millis());
  remotePublish(snap);
  screenUpdate(snap, view, millis(), touched);
  delay(1);
}
