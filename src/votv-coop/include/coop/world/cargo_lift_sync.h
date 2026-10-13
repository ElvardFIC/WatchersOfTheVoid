#pragma once
#include <cstdint>
namespace coop::net { class Session; struct CargoLiftPayload; }
namespace coop::cargo_lift_sync {
void Install(net::Session* session);
void Tick();
void Receive(const net::CargoLiftPayload& p, uint8_t sender);
void QueueConnect(int slot);
void OnPeerLeft(int slot);
void OnDisconnect();
}
