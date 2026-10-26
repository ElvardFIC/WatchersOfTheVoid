#pragma once
#include <cstdint>
#include <functional>
namespace coop::net {class Session;}
namespace coop::llama_begin_restore {
bool Prepare(void* actor,bool originalSetting,uint64_t invocation,const std::function<bool()>& current,uint64_t& ticket);
void Commit(uint64_t ticket);
void Cancel(uint64_t ticket);
void Finished(uint64_t invocation);
// A source-owned native jump consumes the deferred one-shot before restoration.
void Consumed(void* actor);
void Install(net::Session* session);
void Tick();
void OnDisconnect();
}
