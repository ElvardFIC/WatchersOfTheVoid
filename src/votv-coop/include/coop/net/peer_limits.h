#pragma once

#include <cstdint>

namespace coop::net {

inline constexpr uint8_t kPeerCapacity = 32;
inline constexpr uint8_t kMinPlayers = 2;
inline constexpr uint8_t kDefaultPlayers = 4;
using PeerMask = uint32_t;
static_assert(kPeerCapacity == sizeof(PeerMask) * 8);
inline constexpr PeerMask kAllClientPeers = ~PeerMask{1};

constexpr uint8_t ClampPlayerLimit(int players) {
    return static_cast<uint8_t>(players < kMinPlayers ? kMinPlayers
        : players > kPeerCapacity ? kPeerCapacity : players);
}

template<class IsFree>
int FindAvailableClientSeat(int players, IsFree isFree) {
    const int limit = ClampPlayerLimit(players);
    for (int slot = 1; slot < limit; ++slot)
        if (isFree(slot)) return slot;
    return -1;
}

}  // namespace coop::net
