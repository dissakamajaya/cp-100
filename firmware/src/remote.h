#pragma once
#include "controller.h"
struct RemoteRequest { Command command; uint32_t id, deadline; };
void remoteBegin(const ControlSnapshot& initial);
void remotePublish(const ControlSnapshot& state);
bool remoteReceive(RemoteRequest* request);
void remoteReply(uint32_t id, const ControlResult& result, bool persisted);
void remotePrintWifi();
