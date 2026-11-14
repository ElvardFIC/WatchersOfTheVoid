#pragma once
#include <cstdint>
#include "coop/player/remote_player.h"
namespace coop::element { class Player; } namespace coop::players {
inline constexpr uint8_t kMaxPeers=4;
inline constexpr uint8_t kPeerIdHost=0;
class Registry {
public:
    uint8_t peer=1;
    void* local=nullptr;
    coop::RemotePlayer puppet;
    static Registry& Get() { static Registry r; return r; }
    void* Local() { return local; }
    bool IsLocal(void* a) { return a==Local(); }
    bool IsPuppet(void* a) { return a && a==puppet.actor; }
    int PeerIdOfActor(void* a) { return a && a==local ? peer : IsPuppet(a) ? 2 : -1; }
    uint32_t LocalPlayerElementId() const { return 0; }
    coop::element::Player* GetPlayerElement(uint8_t) { return nullptr; } uint8_t LocalPeerId() const { return peer; }
    coop::RemotePlayer* Puppet(int) { return &puppet; }
};
}
