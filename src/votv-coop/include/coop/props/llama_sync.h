#pragma once
#include "ue_wrap/core/script_gate.h"
namespace coop::net {class Session;struct LlamaStatePayload;}
namespace coop::llama_sync {
void Install(net::Session* session);
void Tick();
void Receive(const net::LlamaStatePayload& packet,uint8_t sender);
void QueueConnect(int slot);
void OnPeerLeft(int slot);
void OnDisconnect();
ue_wrap::script_gate::Verdict BeforeGraph(const ue_wrap::script_gate::Call& call);
void AfterGraph(const ue_wrap::script_gate::Call& call);
}
