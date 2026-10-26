#pragma once
#include "ue_wrap/core/script_gate.h"
#include <functional>
namespace coop::net {class Session;}
namespace coop::llama_events {
void Install(net::Session* session);
void Tick();
bool Prepare(void* actor,const std::function<bool()>& current);
void OnDisconnect();
ue_wrap::script_gate::Verdict BeforeSetEvent(const ue_wrap::script_gate::Call& call);
}
