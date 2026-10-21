#pragma once
#include "coop/net/protocol.h"
namespace coop::net {class Session;}
namespace coop::water_pour_sync {
void Install(net::Session* session);
void Tick();
// Called by the client FinishSpawningActor seam. True consumes only this exact
// native bucket output, excluding it from generic item placement.
bool ObserveBirth(void* source,void* actor);
void ObserveHostBirth(void* source,void* actor);
void Receive(const net::WaterPourPayload& packet,uint8_t sender);
void ReceiveReply(const net::WaterPourReplyPayload& packet,uint8_t sender);
void ReceiveSplash(const net::WaterSplashPayload& packet,uint8_t sender);
void ReceiveLaunch(const net::WaterLaunchPayload& packet,uint8_t sender);
void OnPeerLeft(int slot);
void OnDisconnect();
}
