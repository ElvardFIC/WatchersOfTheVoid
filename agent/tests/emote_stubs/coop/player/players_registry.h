#pragma once
#include "coop/player/remote_player.h"
#include "coop/net/peer_limits.h"
#include <array>
namespace coop::players {
enum class PuppetLife {None,Dead,Alive};
inline constexpr uint8_t kMaxPeers=net::kPeerCapacity;
struct Registry {
    uint8_t peer=0;void* local=nullptr;std::array<RemotePlayer,kMaxPeers> puppets{};
    static Registry& Get(){static Registry r;return r;}
    uint8_t LocalPeerId()const{return peer;}void* Local(){return local;}
    uint8_t PeerIdOfActor(void* actor){for(uint8_t i=0;i<kMaxPeers;++i)if(puppets[i].body==actor)return i;return 255;}
    RemotePlayer* Puppet(uint8_t i){return &puppets.at(i);}
    PuppetLife LifeOf(uint8_t i){return puppets.at(i).body?PuppetLife::Alive:PuppetLife::None;}
};
}
