#pragma once
#include <cstdint>
#include "ue_wrap/core/script_gate.h"
namespace coop::net {class Session;struct WallCreativePayload;}
namespace coop::wall_creative_sync {
void Install(net::Session* session);
ue_wrap::script_gate::Verdict BeforeBuild(const ue_wrap::script_gate::Call& call);
void Tick();
void Receive(const net::WallCreativePayload& packet,uint8_t sender);
void OnDisconnect();
bool NativeCurrent(void* wall);
}
