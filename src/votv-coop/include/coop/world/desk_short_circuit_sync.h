#pragma once
#include <cstdint>
namespace coop::net { class Session; struct DeskShortCircuitPayload; }
namespace coop::desk_short_circuit_sync {
void Install(net::Session* session);
void Tick();
void Receive(const net::DeskShortCircuitPayload& p, uint8_t sender);
void OnPeerLeft(int slot);
void OnDisconnect();
}
