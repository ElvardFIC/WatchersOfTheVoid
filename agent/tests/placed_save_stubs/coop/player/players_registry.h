#pragma once
#include <cstdint>
#include "coop/player/remote_player.h"
namespace coop::players {
inline constexpr uint8_t kMaxPeers=4;
class Registry {
public:
    coop::RemotePlayer remote;
    void* local=nullptr;
    static Registry& Get(){static Registry r;return r;}
    coop::RemotePlayer* Puppet(int){return &remote;}
    void* Local(){return local;}
};
}
