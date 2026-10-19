#pragma once
#include <cstdint>
namespace coop::net {class Session;struct ExplosionEventPayload;}
namespace coop::explosion_events {
void Install(net::Session*);
void Tick();
void Receive(const net::ExplosionEventPayload&,uint8_t sender);
void OnPeerLeft(int);
void OnDisconnect();
bool IsReplica(void*);
bool ConsumesClientDestroy(void*);
}
