#pragma once
#include "coop/player/remote_player.h"
#include <array>
#include <cstdint>
namespace coop::players {
inline constexpr uint8_t kPeerIdUnknown=255;
struct Registry {
    void* local=nullptr;
    uint8_t localId=0;
    std::array<RemotePlayer,32> peers{};
    static Registry& Get(){static Registry r;return r;}
    void* Local(){return local;}
    uint8_t LocalPeerId(){return localId;}
    RemotePlayer* Puppet(uint8_t id){return id<32?&peers[id]:nullptr;}
    uint8_t PeerIdOfActor(void* actor){
        if(actor==local)return localId;
        for(uint8_t i=0;i<32;++i)if(peers[i].actor==actor)return i;
        return kPeerIdUnknown;
    }
};
}
