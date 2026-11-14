#pragma once
#include "coop/net/protocol.h"
#include <array>
namespace coop::net {
inline constexpr int kMaxPeers = kPeerCapacity;
enum class Role { Host, Client };
class Session {
public:
    Role mode = Role::Host;
    bool linked = true;
    std::array<bool, kMaxPeers> peers{}, ready{}, allowed{};
    std::array<uint32_t, kMaxPeers> generations{};
    Session() {
        allowed.fill(true);
        peers[1] = peers[2] = ready[1] = ready[2] = true;
        generations[1] = 1; generations[2] = 2;
    }
    bool connected() const { return linked; }
    Role role() const { return mode; }
    bool IsSlotConnected(int slot) const { return peers.at(slot); }
    bool IsSlotWorldReady(int slot) const { return ready.at(slot); }
    uint32_t peerGenerationForSlot(int slot) const { return generations.at(slot); }
};
}
