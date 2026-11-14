#pragma once
#include <array>
#include <cstdint>
namespace coop::players {
inline constexpr uint8_t kMaxPeers=32;
class Registry {
public:
    void* local=nullptr;std::array<void*,kMaxPeers> puppets{};
    static Registry& Get(){static Registry value;return value;}
    bool IsLocal(void* actor){return actor && actor==local;}
    uint8_t PeerIdOfActor(void* actor){for(uint8_t slot=0;slot<kMaxPeers;++slot)if(actor && puppets[slot]==actor)return slot;return 255;}
};
}
