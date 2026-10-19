#pragma once
#include <cstdint>
namespace coop::players {
inline constexpr uint8_t kMaxPeers = 4;
class Registry {
public:
    void* local = nullptr;
    static Registry& Get() { static Registry r; return r; }
    void* Local() { return local; }
};
}
