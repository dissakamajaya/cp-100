#include <math.h>
#include <stdio.h>
#include <string.h>
#include <set>
#include <string>
#include <vector>
#include "controller.h"
#include "protocol.h"
#include "storage.h"
#include <Preferences.h>
static int checks = 0, failures = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; printf("FAIL line %d: %s\n", __LINE__, #x); } } while(0)
Controller fresh(const Setup& setup = kDefaultSetup) {
  Preset presets[kPadCount]; examplePresets(presets); Controller c; c.begin(presets, setup, exampleGather()); return c;
}
ControlResult apply(Controller& c, Action action, uint32_t now, int index = -1, bool flag = false) {
  Command cmd; cmd.owner = 42; cmd.action = action; cmd.index = index; cmd.flag = flag; return c.apply(cmd, now);
}
void beat(Controller& c, uint32_t now, int index = -1) { CHECK(apply(c, Action::Heartbeat, now, index).ok); }
void advance(Controller& c, uint32_t from, uint32_t to) {
  for (uint32_t t = from; t < to; t += 500) { beat(c, t); c.tick(t); }
  beat(c, to); c.tick(to);
}
void leasesAndPads() {
  auto c = fresh(); CHECK(kPadCount == 4); CHECK(c.state().muted); CHECK(c.state().mode == Mode::Run);
  c.physicalDown(0, 0); CHECK(!c.state().muted && c.state().activePad == 0);
  c.physicalDown(1, 1); c.physicalUp(1); CHECK(c.state().activePad == 0 && !c.state().muted);
  c.physicalUp(0); CHECK(c.state().muted);
  CHECK(apply(c, Action::Claim, 10).ok); CHECK(c.state().muted && c.state().mode == Mode::Service);
  CHECK(!apply(c, Action::Claim, 11).ok);
  Command cmd; cmd.owner = 42; cmd.action = Action::Parameters; cmd.parameters = {4000, 50, Wave::Triangle, 18.1f, Harmonics::Both};
  CHECK(c.apply(cmd, 20).ok); CHECK(c.state().parameters.frequencyHz == 4000 && fabs(c.state().parameters.foldPct - 18.1f) < 1e-5f);
  CHECK(apply(c, Action::Test, 30).ok && !c.state().muted);
  beat(c, 100); CHECK(!c.state().muted); // heartbeat with no held pad does not stop TEST
  cmd.parameters.frequencyHz = 1000; CHECK(c.apply(cmd, 110).ok && !c.state().muted);
  c.tick(2599); CHECK(!c.state().muted); c.tick(2600); CHECK(c.state().muted && c.state().mode == Mode::Run);
  CHECK(!apply(c, Action::Heartbeat, 2601).ok); CHECK(!apply(c, Action::Test, 2602).ok);
  CHECK(apply(c, Action::Claim, 2700).ok); CHECK(c.state().muted);
  CHECK(apply(c, Action::PadDown, 2710, 0).ok); CHECK(!c.state().muted);
  beat(c, 3000, 0); c.tick(4199); CHECK(!c.state().muted); c.tick(4200); CHECK(c.state().muted);
  beat(c, 4300); CHECK(apply(c, Action::PadDown, 4310, 1).ok);
  beat(c, 4400, -1); CHECK(c.state().muted); // lost pointer-up is healed by heartbeat
  CHECK(apply(c, Action::PadDown, 4410, 2).ok); c.physicalUp(2); CHECK(!c.state().muted);
  CHECK(apply(c, Action::PadUp, 4420, 1).ok && !c.state().muted);
  CHECK(apply(c, Action::PadUp, 4430, 2).ok && c.state().muted);
  c.physicalDown(1, 4440); CHECK(!apply(c, Action::PadDown, 4441, 0).ok);
  CHECK(!apply(c, Action::Test, 4442).ok); CHECK(!apply(c, Action::Sequence, 4443).ok); CHECK(!apply(c, Action::GatherStart, 4444).ok);
  cmd.parameters.frequencyHz = 123; CHECK(c.apply(cmd, 4450).ok); c.physicalUp(1); CHECK(c.state().muted);
  CHECK(apply(c, Action::Release, 4460).ok); CHECK(c.state().muted && c.state().mode == Mode::Run);
}
void momentaryLimits() {
  const float expected[] = {1096, 1360, 2249, 2386};
  for (int i=0;i<kPadCount;++i) {
    auto c=fresh();
    CHECK(c.state().presets[i].used && c.state().presets[i].parameters.frequencyHz==expected[i]);
    c.physicalDown(i,100); CHECK(!c.state().muted && c.state().parameters.frequencyHz==expected[i]);
    const auto press=c.padPressId(); CHECK(press!=0);
    c.tick(5099); CHECK(!c.state().muted);
    c.physicalDown(i,5099); CHECK(c.padPressId()==press); // duplicate does not extend
    c.tick(5100); CHECK(c.state().muted && c.state().activePad==-1);
    c.tick(20000);c.physicalDown(i,20001);CHECK(c.state().muted); // still physically held
    c.physicalDown((i+1)%4,20002);CHECK(c.state().muted); // first held retains ownership
    c.physicalUp(i);c.physicalDown(i,20003);CHECK(!c.state().muted && c.padPressId()!=press);
    c.physicalUp(i);CHECK(c.state().muted); // release before five seconds
    c.tick(30000);CHECK(c.state().muted); // no automatic repeat
  }
  // SERVICE and parameter edits cannot renew a physical press's deadline.
  auto service=fresh();apply(service,Action::Claim,0);service.physicalDown(2,10);
  for(uint32_t t=400;t<5000;t+=400)beat(service,t);
  Command edit;edit.owner=42;edit.action=Action::Parameters;edit.parameters=service.state().parameters;
  CHECK(service.apply(edit,5000).ok);service.tick(5009);CHECK(!service.state().muted);
  service.tick(5010);CHECK(service.state().muted);
  CHECK(!apply(service,Action::Test,5011).ok);service.physicalUp(2);
  CHECK(apply(service,Action::Test,5012).ok);service.emergencyMute();
  // Remote heartbeat/hold lease is separate from the absolute 5-second limit.
  auto remote=fresh();apply(remote,Action::Claim,0);CHECK(apply(remote,Action::PadDown,10,1).ok);
  for(uint32_t t=400;t<5000;t+=400)beat(remote,t,1);
  remote.tick(5009);CHECK(!remote.state().muted);remote.tick(5010);CHECK(remote.state().muted);
  beat(remote,5100,1);CHECK(!apply(remote,Action::PadDown,5200,1).ok && remote.state().muted);
  CHECK(apply(remote,Action::PadUp,5300,1).ok);CHECK(apply(remote,Action::PadDown,5301,1).ok && !remote.state().muted);
  CHECK(apply(remote,Action::PadUp,5302,1).ok && remote.state().muted);
  // Unsigned millis wrap preserves the exact boundary and release/re-arm semantics.
  auto wrapped=fresh();const uint32_t start=0xfffffff0u;wrapped.physicalDown(3,start);
  wrapped.tick(start+4999);CHECK(!wrapped.state().muted);wrapped.tick(start+5000);CHECK(wrapped.state().muted);
  wrapped.physicalUp(3);wrapped.physicalDown(3,start+5001);CHECK(!wrapped.state().muted);
  // A custom persisted slot survives, and factory restore uses the four new defaults.
  Preferences::data.clear();Preset custom[kPadCount],loaded[kPadCount];Setup setup;examplePresets(custom);
  custom[0].parameters.frequencyHz=777;CHECK(storageSavePresets(custom));storageLoad(loaded,&setup);
  CHECK(loaded[0].parameters.frequencyHz==777);Controller restored;restored.begin(loaded,setup,exampleGather());
  apply(restored,Action::Claim,0);CHECK(apply(restored,Action::Factory,1).presets);
  for(int i=0;i<kPadCount;++i)CHECK(restored.state().presets[i].parameters.frequencyHz==expected[i]);
}
void savesAndValidation() {
  auto c = fresh(); apply(c, Action::Claim, 0);
  Command cmd; cmd.owner = 42; cmd.action = Action::Parameters; cmd.parameters = {4000, 35, Wave::Saw, 99.9f, Harmonics::Even};
  CHECK(c.apply(cmd, 1).ok); CHECK(apply(c, Action::Save, 2, 3).presets);
  CHECK(c.state().presets[3].parameters.frequencyHz == 4000); CHECK(c.state().muted);
  cmd.parameters.foldPct = NAN; CHECK(!c.apply(cmd, 3).ok); CHECK(c.state().parameters.foldPct == 99.9f);
  cmd.action = Action::Setup; cmd.setup = {200, 2000, true, LedColor::Pink, true}; CHECK(c.apply(cmd, 4).setup);
  CHECK(c.state().parameters.frequencyHz == 2000); CHECK(!apply(c, Action::PadDown, 5, 3).ok);
  cmd.setup.minHz = 2000; CHECK(!c.apply(cmd, 6).ok && c.state().setup.minHz == 200);
  cmd.action = Action::Import; examplePresets(cmd.presets); cmd.presets[2].parameters.frequencyHz = INFINITY;
  CHECK(!c.apply(cmd, 7).ok); CHECK(c.state().presets[3].parameters.frequencyHz == 4000);
  examplePresets(cmd.presets); CHECK(c.apply(cmd, 8).presets); CHECK(c.state().muted);
  CHECK(apply(c, Action::Mode, 9, -1, false).ok); CHECK(!apply(c, Action::Save, 10, 0).ok);
}
void sequences() {
  auto c = fresh(); apply(c, Action::Claim, 0); CHECK(apply(c, Action::Sequence, 1).ok);
  CHECK(c.snapshot(1).step == 0);
  for (int step = 1; step <= 8; ++step) { advance(c, (step-1)*3000+2, step*3000+1); CHECK(c.snapshot(step*3000+1).step == step % 4); }
  c.physicalDown(2, 24010); CHECK(c.snapshot(24010).playback == Playback::Idle && c.state().activePad == 2);
  c.physicalUp(2); CHECK(c.state().muted);
  apply(c, Action::Sequence, 24020, -1, true);
  int previous = -1;
  for (int cycle = 0; cycle < 4; ++cycle) {
    std::set<int> seen;
    for (int i = 0; i < 4; ++i) {
      uint32_t t = 24020 + (cycle * 4 + i) * 3000;
      if (cycle || i) advance(c, t - 2999, t);
      int step = c.snapshot(t).step;
      if (i == 0) CHECK(step != previous);
      seen.insert(step); previous = step;
    }
    CHECK(seen.size() == 4);
  }
  apply(c, Action::Stop, 69030); CHECK(c.state().muted && c.snapshot(69030).playback == Playback::Idle);
  // Empty or excluded steps never keep the previous slot's sound.
  auto d = fresh(); apply(d, Action::Claim, 0);
  Command import; import.owner = 42; import.action = Action::Import; examplePresets(import.presets); import.presets[1].used = false;
  d.apply(import, 1); apply(d, Action::Sequence, 2); CHECK(!d.state().muted);
  advance(d, 3, 3002); CHECK(d.state().muted && d.snapshot(3002).step == 1);
  d.tick(5502); CHECK(d.state().muted && d.state().mode == Mode::Run && d.snapshot(5502).playback == Playback::Idle);
}
void gathers() {
  auto c = fresh(); apply(c, Action::Claim, 0);
  Command config; config.owner = 42; config.action = Action::GatherConfigure; config.gather.count = 2;
  config.gather.steps[0] = {100, 40, 1, 1}; config.gather.steps[1] = {400, 20, 1, 1};
  CHECK(c.apply(config, 1).gather); CHECK(apply(c, Action::GatherStart, 10).ok);
  CHECK(c.state().parameters.levelPct == 0 && c.state().parameters.wave == Wave::Sine);
  advance(c, 11, 510); CHECK(c.state().parameters.levelPct == 20 && c.state().parameters.frequencyHz == 100);
  advance(c, 511, 2510); CHECK(fabs(c.state().parameters.frequencyHz-200) < .01 && c.state().parameters.levelPct == 30);
  advance(c, 2511, 5010); CHECK(c.state().parameters.levelPct == 10 && !c.state().muted);
  advance(c, 5011, 6010); CHECK(c.state().muted && c.snapshot(6010).playback == Playback::Idle);
  CHECK(c.state().presets[0].parameters.frequencyHz == 1096);
  config.gather.steps[0].glideS = NAN; CHECK(!c.apply(config, 6020).ok && c.gather().steps[0].glideS == 1);
  apply(c, Action::GatherStart, 6030); c.physicalDown(1, 6040); CHECK(c.snapshot(6040).playback == Playback::Idle && c.state().activePad == 1);
  c.physicalUp(1); apply(c, Action::GatherStart, 6050); c.emergencyMute(); c.tick(6060); CHECK(c.state().muted && c.snapshot(6060).playback == Playback::Idle);
  // Zero-length steps finish without division by zero, and a lease loss cancels GATHER.
  config.gather.steps[0] = {100,30,0,0};config.gather.steps[1] = {200,20,0,0};CHECK(c.apply(config,6070).ok);
  apply(c,Action::GatherStart,6080);CHECK(isfinite(c.state().parameters.frequencyHz));
  c.tick(10000); CHECK(c.state().muted && c.state().mode == Mode::Run);
  auto wrapped = fresh(); const uint32_t start = 0xffffff00u;
  CHECK(apply(wrapped,Action::Claim,start).ok);CHECK(apply(wrapped,Action::Test,start+1).ok);
  beat(wrapped, start+1000);wrapped.tick(start+3499);CHECK(!wrapped.state().muted);wrapped.tick(start+3500);CHECK(wrapped.state().muted);
}
bool decode(const std::string& json, Command* c = nullptr) { Command tmp; const char* error; return decodeCommand(json.data(), json.size(), c ? c : &tmp, &error); }
void protocol() {
  Command c;
  const std::string valid = R"({"action":"parameters","parameters":{"frequencyHz":4000,"levelPct":35,"foldPct":18.1,"wave":"sine","harmonics":"odd"}})";
  CHECK(decode(valid,&c)); CHECK(c.parameters.frequencyHz==4000 && fabs(c.parameters.foldPct-18.1)<.001);
  for (const auto& json : {"null","{}","[]",R"({"action":"padDown","index":4})",R"({"action":"padDown","index":0.5})",R"({"action":"padDown","index":true})",R"({"action":"mode","mode":"invalid"})",R"({"action":"heartbeat"})",R"({"action":"parameters","parameters":{}})"}) CHECK(!decode(json));
  std::string invalid=valid;invalid.replace(invalid.find("4000"),4,"4001");CHECK(!decode(invalid));
  invalid=valid;invalid.replace(invalid.find("18.1"),4,"\"18.1\"");CHECK(!decode(invalid));
  CHECK(!decode(std::string(4097,'x')));
  Preset p[kPadCount]; examplePresets(p);p[0].parameters.foldPct=18.1f;p[0].parameters.frequencyHz=4000;
  CHECK(decode("{\"action\":\"import\",\"file\":"+encodePresets(p)+"}",&c));CHECK(c.presets[0].parameters.foldPct==18.1f);
  CHECK(!decode(R"({"action":"import","file":{"schema":"chladni-08","version":4,"folding":"parity-fold-v2","presets":[null,null,null,null,null]}})"));
  CHECK(decode(R"({"action":"import","file":{"schema":"chladni-08","version":4,"folding":"parity-fold-v2","presets":[null,null,null,null,null,null,null,null]}})"));
  CHECK(!decode(R"({"action":"import","file":{"schema":"chladni-08","version":4,"folding":"parity-fold-v2","presets":[null,null,null,null,null,null,null,{}]}})"));
  CHECK(decode(R"({"action":"setup","setup":{"minHz":20,"maxHz":4000}})"));
  CHECK(encodeState(fresh().snapshot(0),0,0,0).find("password") == std::string::npos);
  CHECK(encodeState(fresh().snapshot(0),0,0,0).find("\"padMaxHoldMs\":5000") != std::string::npos);
  CHECK(encodeState(fresh().snapshot(0),0,0,0).find("ledColor") == std::string::npos);
  CHECK(encodeState(fresh().snapshot(0),0,0,0).find("\"display\":false") != std::string::npos);
  CHECK(!decode(R"({"action":"setup","setup":{"minHz":300,"maxHz":200,"ledColor":"pink","ledPulse":false}})"));
  CHECK(!decode(R"({"action":"gatherConfigure","steps":[{"frequencyHz":100,"levelPct":30,"glideS":61,"holdS":4}]})"));
}
void storage() {
  Preferences::data.clear(); Preset p[kPadCount], loaded[kPadCount];Setup setup;
  storageLoad(p,&setup);CHECK(p[0].used && setup.maxHz==4000);
  p[0].parameters={4000,45,Wave::Saw,18.1f,Harmonics::Both};CHECK(storageSavePresets(p));CHECK(storageSaveSetup({40,2000,true,LedColor::Pink,true}));
  storageLoad(loaded,&setup);CHECK(loaded[0].parameters.frequencyHz==4000 && loaded[0].parameters.foldPct==18.1f);CHECK(setup.maxHz==2000 && setup.calibrated && setup.ledPulse);
  auto g=exampleGather();g.steps[0].frequencyHz=100;CHECK(storageSaveGather(g));CHECK(storageLoadGather().steps[0].frequencyHz==100);
  Preferences::failWrites=true;CHECK(!storageSavePresets(p));CHECK(!storageSaveSetup(setup));Preferences::failWrites=false;
  Preferences::shortRead=true;storageLoad(loaded,&setup);CHECK(loaded[0].parameters.frequencyHz==1096 && setup.maxHz==4000);Preferences::shortRead=false;
  // Construct the exact previous 8-slot raw layout with distinct integer fold values.
  struct LP { float hz;int32_t level;uint8_t wave,pad1[3];int32_t fold;uint8_t harm,pad2[3]; };
  struct L {uint8_t used,pad[3];LP p;};L old[8]={};static_assert(sizeof(old)==192,"legacy layout");
  for(int i=0;i<8;++i){old[i].used=1;old[i].p.hz=100+i;old[i].p.level=30;old[i].p.fold=18+i;old[i].p.wave=1;old[i].p.harm=2;}
  auto& blob=Preferences::data["cp100/presets"];blob.resize(193);blob[0]=1;memcpy(blob.data()+1,old,sizeof(old));
  std::vector<uint8_t> before=blob;
  struct LS{float min,max;uint8_t cal,color,pulse,pad;};LS legacySetup{50,2000,1,2,0,0};
  auto& sb=Preferences::data["cp100/setup"];sb.resize(13);sb[0]=1;memcpy(sb.data()+1,&legacySetup,sizeof(legacySetup));
  storageLoad(loaded,&setup);CHECK(loaded[0].parameters.foldPct==18 && loaded[3].parameters.frequencyHz==103);CHECK(setup.minHz==50 && setup.maxHz==2000);CHECK(blob==before);
  CHECK(storageSavePresets(loaded));CHECK(Preferences::data["cp100/presets"][0]==2);
  storageLoad(loaded,&setup);CHECK(loaded[3].parameters.foldPct==21);
  Preferences::data["cp100/presets"][0]=99;storageLoad(loaded,&setup);CHECK(loaded[0].parameters.frequencyHz==1096);
}
int main(){leasesAndPads();momentaryLimits();savesAndValidation();sequences();gathers();protocol();storage();printf("%d checks, %d failures\n",checks,failures);return failures?1:0;}
