#pragma once
#include <cstdint>
#include "coop/player/remote_player.h"
namespace coop::players {
inline constexpr uint8_t kMaxPeers=4;
class Registry {public:
 uint8_t peer=1;void* local=nullptr;coop::RemotePlayer puppet;
 static Registry& Get(){static Registry r;return r;}
 void* Local(){return local?local:this;}uint8_t LocalPeerId()const{return peer;}
 coop::RemotePlayer* Puppet(int){return &puppet;}
};
}
