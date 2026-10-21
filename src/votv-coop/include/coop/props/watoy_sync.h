#pragma once
#include <cstdint>
namespace coop::net {class Session;struct WatoyPayload;}
namespace coop::watoy_sync {
void Install(net::Session* session);
void Tick();
void Receive(const net::WatoyPayload& state,uint8_t sender);
void QueueConnect(int slot);
void OnPeerLeft(int slot);
void OnDisconnect();
}
