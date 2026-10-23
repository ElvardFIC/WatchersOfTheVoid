#pragma once
#include "coop/net/peer_limits.h"
namespace coop::players {
inline constexpr uint8_t kMaxPeers=coop::net::kPeerCapacity;
class Registry {
public:
    uint8_t slot=0;
    void* local=nullptr;
    void* Local(){return local;}
    static Registry& Get(){static Registry value;return value;}
    uint8_t LocalPeerId()const{return slot;}
};
}
