#pragma once
#include "coop/net/peer_limits.h"
#include <array>
namespace coop { class RemotePlayer; }
namespace coop::players {
inline constexpr int kMaxPeers = coop::net::kPeerCapacity;
inline constexpr unsigned char kPeerIdUnknown = 255;
enum class PuppetLife { None, Dead, Alive };
class Registry {
public:
    void* local = nullptr;
    std::array<coop::RemotePlayer*, kMaxPeers> bodies{};
    std::array<PuppetLife, kMaxPeers> lives{};
    static Registry& Get() { static Registry registry; return registry; }
    void* Local() const { return local; }
    coop::RemotePlayer* Puppet(int slot) const { return bodies.at(slot); }
    PuppetLife LifeOf(int slot) const { return lives.at(slot); }
};
}
