// Local QA transport: real controller/protocol/storage, no radio, DAC or GPIO.
#include <chrono>
#include <iostream>
#include <string>
#include <sys/select.h>
#include <unistd.h>
#include "controller.h"
#include "protocol.h"
#include "storage.h"
uint32_t nowMs() {
  static const auto start = std::chrono::steady_clock::now();
  return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count());
}
int main() {
  Preset presets[kPadCount];Setup setup;storageLoad(presets,&setup);
  Controller controller;controller.begin(presets,setup,storageLoadGather());
  std::string line;
  for (;;) {
    controller.tick(nowMs());
    fd_set set;FD_ZERO(&set);FD_SET(STDIN_FILENO,&set);timeval timeout{0,10000};
    if (select(STDIN_FILENO+1,&set,nullptr,nullptr,&timeout)<=0) continue;
    if (!std::getline(std::cin,line)) return 0;
    ControlResult result;bool persisted=true;
    if(line=="!state") {}
    else {
      Command c;c.owner=42;
      if(line=="!claim") c.action=Action::Claim;
      else {const char* error;if(!decodeCommand(line.data(),line.size(),&c,&error)){std::cout<<"{\"ok\":false,\"message\":\"Invalid command\"}"<<std::endl;continue;}c.owner=42;}
      result=controller.apply(c,nowMs());
      if(result.presets)persisted=storageSavePresets(controller.state().presets)&&persisted;
      if(result.setup)persisted=storageSaveSetup(controller.state().setup)&&persisted;
      if(result.gather)persisted=storageSaveGather(controller.gather())&&persisted;
    }
    std::cout<<"{\"ok\":"<<(result.ok?"true":"false")<<",\"persisted\":"<<(persisted?"true":"false")<<",\"state\":"<<encodeState(controller.snapshot(nowMs()),0,0,0)<<"}"<<std::endl;
  }
}
