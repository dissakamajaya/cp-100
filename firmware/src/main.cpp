// Four-pad controller with an offline Wi-Fi service panel.
#include <Arduino.h>
#include <string.h>
#include "audio.h"
#include "controller.h"
#include "inputs.h"
#include "protocol.h"
#include "remote.h"
#include "storage.h"

namespace {
Controller controller;
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
  delay(12); // let all four DMA buffers drain before a flash-cache stall
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
}
void setup() {
  Serial.setTxBufferSize(4096); Serial.begin(115200);
  Preset presets[kPadCount]; Setup setup;
  storageLoad(presets, &setup);
  controller.begin(presets, setup, storageLoadGather());
  audioBegin(); audioSetTarget(targetFor(controller.state()));
  inputsBegin();
  remoteBegin(controller.snapshot(millis()));
  Serial.println("Four pads / Wi-Fi service. Commands: status | wifi | export | mute");
  enableLoopWDT();
}
void loop() {
  controller.tick(millis());
  const auto inputs = inputsPoll();
  for (int i = 0; i < kPadCount; ++i) if (inputs.padUp & (1 << i)) controller.physicalUp(i);
  for (int i = 0; i < kPadCount; ++i) if (inputs.padDown & (1 << i)) controller.physicalDown(i, millis());
  serviceRequest();
  // A physical emergency MUTE in this pass always wins over a remote command.
  if (inputs.mutePressed) controller.emergencyMute();
  pollConsole();
  controller.tick(millis());
  audioSetTarget(targetFor(controller.state()));
  remotePublish(controller.snapshot(millis()));
  delay(1);
}
