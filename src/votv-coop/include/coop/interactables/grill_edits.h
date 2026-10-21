#pragma once
#include "coop/net/protocol.h"
#include "ue_wrap/core/script_gate.h"
namespace coop::net {class Session;}
namespace coop::grill_edits {
void Install(net::Session* session);
void Tick();
void BeforeChange(const ue_wrap::script_gate::Call& call,uint8_t operation);
void AfterChange(const ue_wrap::script_gate::Call& call);
void Receive(const net::GrillEditPayload& packet,uint8_t sender);
void OnPeerLeft(int slot);
void OnDisconnect();
}
