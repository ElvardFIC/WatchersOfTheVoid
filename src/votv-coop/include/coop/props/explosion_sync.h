#pragma once
#include <cstdint>
namespace coop::net {class Session;struct ExplosionPayload;}
namespace coop::explosion_sync {
void Install(net::Session*);
void Tick();
void Receive(const net::ExplosionPayload&,uint8_t sender);
void OnPeerLeft(int);
void OnDisconnect();
// Called at the dying actor's native destroy seam (after K2_DestroyActor).
bool ConsumesClientDestroy(void* actor);
bool IsReplica(void* actor);
bool CapturingGascan();
}
