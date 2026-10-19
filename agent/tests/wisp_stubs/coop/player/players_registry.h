#pragma once
#include "coop/element/element.h"
namespace coop {class RemotePlayer;}
namespace coop::players {
inline constexpr uint8_t kMaxPeers=4;
inline constexpr uint8_t kPeerIdUnknown=0xff;
struct Registry {
    void* local=nullptr;
    RemotePlayer* puppets[kMaxPeers]{};
    static Registry& Get(){static Registry r;return r;}
    uint8_t LocalPeerId()const{return 1;}
    element::ElementId LocalPlayerElementId()const{return 17;}
    void* Local(){return local;}
    RemotePlayer* Puppet(uint8_t slot){return slot<kMaxPeers?puppets[slot]:nullptr;}
};
}
