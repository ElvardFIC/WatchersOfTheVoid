#pragma once
#include <cstdint>
namespace coop::players {
inline constexpr uint8_t kMaxPeers=4;
struct Registry {
    void* local=nullptr;
    void* puppet=nullptr;
    uint8_t slot=2;
    static Registry& Get(){static Registry r;return r;}
    void* Local(){return local;}
    uint8_t PeerIdOfActor(void* actor){return actor==puppet?slot:255;}
};
}
