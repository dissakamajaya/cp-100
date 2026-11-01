#pragma once
// Hardware-independent control engine. Only the control loop mutates it.
#include "model.h"

constexpr uint32_t kSessionLeaseMs = 2500;
constexpr uint32_t kRemotePadLeaseMs = 1200;
constexpr int kGatherMaxSteps = 16;
struct GatherStep { float frequencyHz, levelPct, glideS, holdS; };
struct GatherRoutine { int count = 0; GatherStep steps[kGatherMaxSteps] = {}; };
bool validGather(const GatherRoutine& routine);
GatherRoutine exampleGather();
float gatherDuration(const GatherRoutine& routine);

enum class Action : uint8_t {
  Claim, Heartbeat, Release, Mode, Parameters, Test, Mute, PadDown, PadUp,
  Save, Setup, Import, Factory, Sequence, GatherConfigure, GatherStart, Stop
};
struct Command {
  Action action = Action::Mute;
  uint32_t owner = 0;
  int index = -1;
  bool flag = false;
  Parameters parameters = kDefaultParameters;
  Setup setup = kDefaultSetup;
  Preset presets[kPadCount] = {};
  GatherRoutine gather;
};
struct ControlResult {
  bool ok = true;
  const char* message = "ok";
  bool presets = false, setup = false, gather = false;
};
enum class Playback : uint8_t { Idle, Ordered, Random, Gather };
struct ControlSnapshot {
  State state;
  GatherRoutine gather;
  uint32_t owner = 0;
  Playback playback = Playback::Idle;
  int step = -1;
  float elapsedS = 0;
  uint32_t uptimeMs = 0;
};
class Controller {
 public:
  void begin(const Preset presets[kPadCount], const Setup& setup, const GatherRoutine& gather);
  ControlResult apply(const Command& command, uint32_t now);
  void tick(uint32_t now);
  void physicalDown(int index, uint32_t now);
  void physicalUp(int index);
  void emergencyMute();
  ControlSnapshot snapshot(uint32_t now) const;
  const State& state() const { return state_; }
  const GatherRoutine& gather() const { return gather_; }
 private:
  State state_{};
  GatherRoutine gather_;
  uint32_t owner_ = 0, lastHeartbeat_ = 0, lastPad_ = 0;
  uint32_t started_ = 0, nextStep_ = 0, rng_ = 1;
  int held_ = -1, step_ = -1, cursor_ = kPadCount;
  int order_[kPadCount] = {};
  Playback playback_ = Playback::Idle;
  void stopPlayback();
  void stopAll();
  void advanceSequence(uint32_t now);
  uint32_t random();
};
