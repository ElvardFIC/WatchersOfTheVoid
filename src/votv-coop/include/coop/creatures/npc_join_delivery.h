#pragma once
#include <cstdint>
#include <functional>
#include <vector>
namespace coop::net { class Session; }
namespace coop::npc_join_delivery {
// Game-thread queue/pump. Admission and retries do not replay native NPC creation.
void Queue(int slot);
void Tick();
void OnDisconnect();
bool HasPending(int slot);
using CompletionFn=std::function<void(coop::net::Session*,int)>;
uint64_t BeginSnapshot(int slot,coop::net::Session* expected = nullptr);
bool SnapshotCurrent(int slot,uint64_t token);
bool CompleteSnapshot(int slot,uint64_t token,const std::vector<uint8_t>& payload,CompletionFn sent,
                      std::function<bool()> valid = {},CompletionFn invalid = {});
void CancelCompletion(int slot);
bool HasCompletion(int slot);
}
