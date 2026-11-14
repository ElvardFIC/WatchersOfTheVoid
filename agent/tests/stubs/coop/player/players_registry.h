#pragma once
#include <cstdint>
#include "coop/net/peer_limits.h"
#include "coop/player/remote_player.h"
namespace coop::players {
inline constexpr uint8_t kMaxPeers=coop::net::kPeerCapacity;
class Registry {
public:
    uint8_t peer=1;
    void* local=nullptr;
    coop::RemotePlayer puppet;
    static Registry& Get() { static Registry r; return r; }
    void* Local() { return local ? local : this; }
    bool IsLocal(void* a) { return a==Local(); }
    bool IsPuppet(void* a) { return a && a==puppet.actor; }
    uint8_t LocalPeerId() const { return peer; }
    coop::RemotePlayer* Puppet(int) { return &puppet; }
};
}
