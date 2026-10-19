#pragma once
#include <cstdint>
#include <vector>
namespace coop::net { class Session; }
namespace coop::garden_actions {
void Install(net::Session* session);
void Tick();
bool Receive(const std::vector<uint8_t>& blob, uint8_t slot);
void OnPeerLeftSlot(int slot);
void OnDisconnect();
}
