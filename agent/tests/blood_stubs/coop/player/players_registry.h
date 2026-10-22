#pragma once
#include "coop/net/peer_limits.h"
namespace coop::players {
class Registry {
public:
    uint8_t slot=0;
    static Registry& Get(){static Registry value;return value;}
    uint8_t LocalPeerId()const{return slot;}
};
}
