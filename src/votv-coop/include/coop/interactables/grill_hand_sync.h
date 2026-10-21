#pragma once
#include <cstdint>
namespace coop::net {class Session;struct GrillHandPayload;}
namespace coop::grill_hand_sync {
void Install(net::Session* session);
void Tick();
void Receive(const net::GrillHandPayload& packet,uint8_t sender);
void QueueConnect(int slot);
void OnPeerLeft(int slot);
void OnDisconnect();
}
