// CP-100 on the Waveshare ESP32-S3-Touch-AMOLED-2.16: the four pads are a
// touch grid, sound goes to the onboard ES8311 speaker, and the phone service
// panel over Wi-Fi is unchanged from the main firmware.
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
int touchedPad = -1;  // pad under the finger, -1 when lifted or between pads

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
void serviceRequest() {
  RemoteRequest request;
  if (!remoteReceive(&request)) return;
  ControlResult result;
  bool persisted = true;
  if (int32_t(millis() - request.deadline) > 0) {
    result.ok = false; result.message = "Perintah kedaluwarsa. Coba lagi.";
  } else {
    result = controller.apply(request.command, millis());
    if (result.ok && (result.presets || result.setup || result.gather)) {
      quiesceAudio();
      if (result.presets) persisted = storageSavePresets(controller.state().presets) && persisted;
      if (result.setup) persisted = storageSaveSetup(controller.state().setup) && persisted;
      if (result.gather) persisted = storageSaveGather(controller.gather()) && persisted;
      if (!persisted) Serial.println("NVS write failed; changes remain in RAM only.");
    }
  }
  controller.tick(millis());
  audioSetTarget(targetFor(controller.state()));
  remotePublish(controller.snapshot(millis()));
  remoteReply(request.id, result, persisted);
}
// Touch behaves like the physical pads: press = down, lift or slide off = up.
bool pollTouch() {
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll < 10) return touchedPad >= 0;
  lastPoll = millis();
  const TouchPoint t = touchRead();
  const int pad = t.down ? screenPadAt(t.x, t.y) : -1;
  if (pad != touchedPad) {
    if (touchedPad >= 0) controller.physicalUp(touchedPad);
    touchedPad = pad;
    if (pad >= 0) controller.physicalDown(pad, millis());
  }
  return t.down;
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
  serviceRequest();
  pollConsole();
  controller.tick(millis());
  audioSetTarget(targetFor(controller.state()));
  const auto snap = controller.snapshot(millis());
  remotePublish(snap);
  screenUpdate(snap, touchedPad, millis(), touched);
  delay(1);
}
