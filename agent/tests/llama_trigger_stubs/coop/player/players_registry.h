#pragma once
#include "coop/player/remote_player.h"
#include <array>
namespace coop::players {
inline constexpr uint8_t kMaxPeers=32;
enum class PuppetLife {None,Dead,Alive};
class Registry {
public:
    std::array<RemotePlayer*,32> peers{};std::array<PuppetLife,32> lives{};void* local=nullptr;
    static Registry& Get(){static Registry registry;return registry;}
    void* Local(){return local;}
    RemotePlayer* Puppet(int slot){return peers.at(slot);}
    PuppetLife LifeOf(int slot){return lives.at(slot);}
    uint8_t PeerIdOfActor(void* actor){for(int slot=0;slot<32;++slot)if(peers[slot]&&peers[slot]->actor==actor)return uint8_t(slot);return 255;}
};
}
