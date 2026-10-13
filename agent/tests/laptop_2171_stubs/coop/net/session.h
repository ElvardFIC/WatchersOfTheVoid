#pragma once

#include "coop/net/protocol.h"

#include <cstddef>
#include <array>
#include <vector>

namespace coop::net {

inline constexpr uint8_t kMaxPeers = 4;

enum class Role : uint8_t { Host, Client };

class Session {
public:
    bool isRunning = true;
    bool isConnected = true;
    Role currentRole = Role::Client;
    int laptopStateSends = 0;
    std::array<bool, kMaxPeers> ready{};
    std::array<bool, kMaxPeers> sendOk{true, true, true, true};
    struct Sent { int slot; LaptopStatePayload payload; uint8_t origin; };
    std::vector<Sent> sent;

    bool running() const { return isRunning; }
    bool connected() const { return isConnected; }
    Role role() const { return currentRole; }
    bool IsSlotReady(int slot) const { return ready.at(slot); }
    bool SendReliableToSlot(int slot, ReliableKind kind, const void* data, size_t, uint8_t origin = 0) {
        if (!sendOk.at(slot)) return false;
        if (kind == ReliableKind::LaptopState) {
            ++laptopStateSends;
            sent.push_back({slot, *static_cast<const LaptopStatePayload*>(data), origin});
        }
        return true;
    }
};

}  // namespace coop::net
