#pragma once
#include "coop/player/remote_player.h"
#include <array>
namespace coop::players {
inline constexpr uint8_t kMaxPeers=32;
class Registry {public:
 uint8_t peer=0;void* local=nullptr;std::array<coop::RemotePlayer,kMaxPeers> puppets;
 static Registry& Get(){static Registry r;return r;}
 void* Local(){return local;}uint8_t LocalPeerId()const{return peer;}
 coop::RemotePlayer* Puppet(int slot){return &puppets.at(slot);}
};}
