#pragma once
#include "coop/player/remote_player.h"
#include <cstdint>
namespace coop::players {
inline constexpr int kMaxPeers=4;
class Registry {public:
 void* local=nullptr;uint8_t slot=0;RemotePlayer puppets[4];
 static Registry& Get(){static Registry r;return r;}void* Local(){return local;}
 uint8_t LocalPeerId()const{return slot;}RemotePlayer* Puppet(uint8_t i){return &puppets[i];}
 uint8_t PeerIdOfActor(void* a){if(!a)return 255;if(a==local)return slot;for(uint8_t i=0;i<4;++i)if(puppets[i].actor==a)return i;return 255;}
};}
