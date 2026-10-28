#pragma once
#include <cstdint>
#include "coop/player/remote_player.h"
#ifndef TEST_STATE_MAX_PEERS
#define TEST_STATE_MAX_PEERS 4
#endif
namespace coop::players {
inline constexpr uint8_t kMaxPeers=TEST_STATE_MAX_PEERS;
class Registry {
public:
    uint8_t peer=1;
    void* local=nullptr;
    coop::RemotePlayer puppet;
    void* ragdollBody=nullptr;
    uint8_t puppetSlot=0;
    static Registry& Get() { static Registry r; return r; }
    void* Local() { return local ? local : this; }
    bool IsLocal(void* a) { return a==Local(); }
    bool IsPuppet(void* a) { return a && a==puppet.actor; }
    bool IsPuppetBody(void* a) { return a && (a==puppet.actor || a==ragdollBody); }
    uint8_t PeerIdOfActor(void* a) { return IsPuppet(a)?puppetSlot:255; }
    uint32_t LocalPlayerElementId()const{return 1;}
    uint8_t LocalPeerId() const { return peer; }
    coop::RemotePlayer* Puppet(int) { return &puppet; }
};
}
