#pragma once
#include <cstdint>
namespace coop::players {
inline constexpr uint8_t kMaxPeers=4;
class Registry {
public:
    uint8_t peer=1;
    static Registry& Get(){static Registry registry;return registry;}
    uint8_t LocalPeerId()const{return peer;}
};
}
