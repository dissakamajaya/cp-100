#include "controller.h"
#include <math.h>

namespace {
constexpr int kRemoteSource = 100;
ControlResult refused(const char* text) { return {false, text, false, false, false}; }
float bounded(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
bool inside(float x, float lo, float hi) { return isfinite(x) && x >= lo && x <= hi; }
}
float gatherDuration(const GatherRoutine& r) {
  float total = 2;
  for (int i = 0; i < r.count; ++i) total += r.steps[i].glideS + r.steps[i].holdS;
  return total;
}
bool validGather(const GatherRoutine& r) {
  if (r.count < 1 || r.count > kGatherMaxSteps) return false;
  for (int i = 0; i < r.count; ++i) {
    const auto& s = r.steps[i];
    if (!inside(s.frequencyHz, kMinHz, kMaxHz) || !inside(s.levelPct, 0, 100) ||
        !inside(s.glideS, 0, 60) || !inside(s.holdS, 0, 60)) return false;
  }
  return gatherDuration(r) <= 300;
}
GatherRoutine exampleGather() {
  GatherRoutine r;
  r.count = 5;
  r.steps[0] = {55.6f, 40, 2, 4}; r.steps[1] = {88.2f, 36, 4, 4};
  r.steps[2] = {140, 32, 4, 4}; r.steps[3] = {222.2f, 26, 4, 4}; r.steps[4] = {352.8f, 20, 4, 4};
  return r;
}
void Controller::begin(const Preset presets[kPadCount], const Setup& setup, const GatherRoutine& gather) {
  state_ = initialState(presets, setup);
  gather_ = validGather(gather) ? gather : exampleGather();
  owner_ = 0; held_ = -1; playback_ = Playback::Idle; step_ = -1;
}
void Controller::stopPlayback() {
  if (playback_ == Playback::Idle) return;
  playback_ = Playback::Idle; step_ = -1;
  held_ = -1;
  releaseAll(state_);
}
void Controller::stopAll() {
  stopPlayback(); held_ = -1;
  setMuted(state_, true);
}
void Controller::emergencyMute() { stopAll(); }
uint32_t Controller::random() {
  rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
  return rng_;
}
void Controller::advanceSequence(uint32_t now) {
  if (cursor_ >= kPadCount) {
    for (int i = 0; i < kPadCount; ++i) order_[i] = i;
    if (playback_ == Playback::Random) {
      for (int i = kPadCount - 1; i > 0; --i) {
        int j = random() % (i + 1); int t = order_[i]; order_[i] = order_[j]; order_[j] = t;
      }
      if (order_[0] == step_) { int t = order_[0]; order_[0] = order_[1]; order_[1] = t; }
    }
    cursor_ = 0;
  }
  step_ = order_[cursor_++];
  releaseAll(state_); state_.armed = false;
  padDown(state_, step_); // empty/excluded presets stay silent for the full slot
  nextStep_ = now + 3000;
}
ControlResult Controller::apply(const Command& c, uint32_t now) {
  tick(now); // An expired owner cannot revive its lease with a late packet.
  if (c.action == Action::Claim) {
    if (!c.owner || owner_) return refused("Panel sedang digunakan. Tunggu sesi berakhir.");
    owner_ = c.owner; lastHeartbeat_ = now; rng_ = c.owner;
    stopAll(); setMode(state_, Mode::Service);
    return {};
  }
  if (!owner_ || c.owner != owner_) return refused("Sesi berakhir. Hubungkan kembali.");
  if (c.action == Action::Heartbeat) {
    lastHeartbeat_ = now;
    if (held_ >= kRemoteSource) {
      if (c.index == held_ - kRemoteSource) lastPad_ = now;
      else { held_ = -1; releaseAll(state_); }
    }
    return {};
  }
  if (c.action == Action::Release) {
    stopAll(); setMode(state_, Mode::Run); owner_ = 0;
    return {};
  }
  if (c.action == Action::Mute || c.action == Action::Stop) { stopAll(); return {}; }
  if (c.action == Action::Mode) {
    stopAll(); setMode(state_, c.flag ? Mode::Service : Mode::Run); return {};
  }
  if (state_.mode != Mode::Service) return refused("Aktifkan SERVICE untuk mengubah pengaturan.");
  if (held_ >= 0 && held_ < kPadCount &&
      (c.action == Action::Test || c.action == Action::Sequence || c.action == Action::GatherStart))
    return refused("Lepaskan pad fisik terlebih dahulu.");
  switch (c.action) {
    case Action::Parameters:
      if (!validParameters(c.parameters)) return refused("Parameter tidak valid.");
      stopPlayback(); applyParameters(state_, c.parameters); return {};
    case Action::Test:
      stopAll(); setMuted(state_, false); return {};
    case Action::PadDown:
      if (c.index < 0 || c.index >= kPadCount) return refused("Pad tidak valid.");
      stopPlayback();
      if (held_ != -1) return refused("Pad lain sedang ditahan.");
      if (!padDown(state_, c.index)) return refused("Pad kosong atau di luar batas sweep.");
      held_ = kRemoteSource + c.index; lastPad_ = now; return {};
    case Action::PadUp:
      if (held_ == kRemoteSource + c.index) { held_ = -1; releaseAll(state_); }
      return {};
    case Action::Save:
      if (c.index < 0 || c.index >= kPadCount) return refused("Pad tidak valid.");
      stopAll(); state_.presets[c.index] = {true, state_.parameters};
      return {true, "Preset tersimpan.", true, false, false};
    case Action::Setup:
      if (!validSetup(c.setup)) return refused("Batas sweep tidak valid.");
      stopAll(); applySetup(state_, c.setup);
      return {true, "Setup tersimpan.", false, true, false};
    case Action::Import:
      for (int i = 0; i < kPadCount; ++i)
        if (c.presets[i].used && !validParameters(c.presets[i].parameters)) return refused("Preset tidak valid.");
      stopAll();
      for (int i = 0; i < kPadCount; ++i) {
        state_.presets[i] = c.presets[i];
        if (state_.presets[i].used) state_.presets[i].parameters = cleanParameters(c.presets[i].parameters);
      }
      return {true, "Preset diimpor.", true, false, false};
    case Action::Factory:
      stopAll(); examplePresets(state_.presets);
      return {true, "Empat preset contoh dipulihkan.", true, false, false};
    case Action::Sequence:
      stopAll(); playback_ = c.flag ? Playback::Random : Playback::Ordered;
      started_ = now; cursor_ = kPadCount; advanceSequence(now); return {};
    case Action::GatherConfigure:
      if (!validGather(c.gather)) return refused("GATHER perlu 1–16 langkah, maksimal 300 detik.");
      stopAll(); gather_ = c.gather;
      return {true, "GATHER tersimpan.", false, false, true};
    case Action::GatherStart:
      stopAll(); playback_ = Playback::Gather; started_ = now; tick(now); return {};
    default: return refused("Perintah tidak dikenal.");
  }
}
void Controller::tick(uint32_t now) {
  if (owner_ && now - lastHeartbeat_ >= kSessionLeaseMs) {
    stopAll(); setMode(state_, Mode::Run); owner_ = 0; return;
  }
  if (held_ >= kRemoteSource && now - lastPad_ >= kRemotePadLeaseMs) {
    held_ = -1; releaseAll(state_);
  }
  if (playback_ == Playback::Ordered || playback_ == Playback::Random) {
    if (int32_t(now - nextStep_) >= 0) advanceSequence(now);
  } else if (playback_ == Playback::Gather) {
    const float t = (now - started_) * .001f;
    float start = 0, frequency = 0, level = 0;
    bool found = false;
    for (int i = 0; i < gather_.count; ++i) {
      const auto& s = gather_.steps[i];
      if (t < start + s.glideS) {
        const float u = (t - start) / s.glideS;
        frequency = i ? gather_.steps[i - 1].frequencyHz * powf(s.frequencyHz / gather_.steps[i - 1].frequencyHz, u) : s.frequencyHz;
        level = i ? gather_.steps[i - 1].levelPct + (s.levelPct - gather_.steps[i - 1].levelPct) * u : s.levelPct * u;
        step_ = i; found = true; break;
      }
      start += s.glideS;
      if (t < start + s.holdS) { frequency = s.frequencyHz; level = s.levelPct; step_ = i; found = true; break; }
      start += s.holdS;
    }
    if (!found) {
      const auto& last = gather_.steps[gather_.count - 1];
      if (t >= start + 2) { stopAll(); return; }
      frequency = last.frequencyHz; level = last.levelPct * (1 - (t - start) / 2); step_ = gather_.count - 1;
    }
    // GATHER is sine-only, bounded by the commissioned setup, with no preset writes.
    state_.parameters = {bounded(frequency, state_.setup.minHz, state_.setup.maxHz), int(roundf(level)), Wave::Sine, 0, Harmonics::Odd};
    state_.activePad = -1; state_.muted = false; state_.armed = false;
  }
}
void Controller::physicalDown(int i, uint32_t now) {
  tick(now); stopPlayback();
  if (i < 0 || i >= kPadCount || held_ != -1) return;
  bool saved = false;
  // Saving to an explicit slot in the app replaces physical SAVE arming.
  if (padDown(state_, i, &saved)) held_ = i;
}
void Controller::physicalUp(int i) {
  if (held_ != i) return;
  held_ = -1; releaseAll(state_);
}
ControlSnapshot Controller::snapshot(uint32_t now) const {
  return {state_, gather_, owner_, playback_, step_, playback_ == Playback::Idle ? 0 : (now - started_) * .001f, now};
}
