#pragma once
#include <cstdint>
namespace coop::net { class Session; struct PlayerBloodPayload; }
namespace coop::blood_sync {
void Install(net::Session* session);
void Tick();
void Receive(const net::PlayerBloodPayload& packet, uint8_t sender);
void OnPeerLeft(int slot);
void OnDisconnect();
}
