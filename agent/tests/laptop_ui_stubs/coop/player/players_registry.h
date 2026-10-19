#pragma once
#include <cstdint>
namespace coop::players {
inline constexpr uint8_t kMaxPeers=4;
class Registry {
public:
    uint8_t local=0;
    static Registry& Get() { static Registry r; return r; }
    uint8_t LocalPeerId() { return local; }
};
}
