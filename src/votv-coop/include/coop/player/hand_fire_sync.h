#pragma once
#include <cstdint>
namespace coop::net { class Session; struct HandFirePayload; }
namespace coop::hand_fire_sync {
void Install(net::Session*);
void Tick();
void Receive(const net::HandFirePayload&, uint8_t sender);
void QueueConnect(int slot);
void OnPeerLeft(int slot);
void OnDisconnect();
}
