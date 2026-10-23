#pragma once
#include "coop/net/protocol.h"
#include <array>
#include <cstring>
#include <functional>
#include <vector>

// Only transport and peer presence are doubled. The console controller, wire codecs,
// chunk sender and assembler in sat_console_delivery_test.cpp are production code.
namespace coop::net {
inline constexpr int kMaxPeers = kPeerCapacity;
enum class Role { Host, Client };
class Session {
public:
    struct ReliableMessage {};
    struct Sent { int slot; ReliableKind kind; std::vector<uint8_t> data; };
    Role mode = Role::Host;
    bool linked = true, active = true, sendOk = true;
    std::array<bool, kMaxPeers> connectedPeers{}, readyPeers{};
    std::array<uint32_t, kMaxPeers> generations{};
    std::vector<Sent> sent, attempts;
    std::function<bool(const Sent&)> onSend;
    Session() { generations.fill(1); }
    bool connected() const { return linked; }
    bool running() const { return active; }
    Role role() const { return mode; }
    bool IsSlotConnected(int slot) const { return slot >= 0 && slot < kMaxPeers && connectedPeers[slot]; }
    bool IsSlotWorldReady(int slot) const { return IsSlotConnected(slot) && readyPeers[slot]; }
    uint32_t peerGenerationForSlot(int slot) const { return generations.at(slot); }
    bool SendReliableToSlot(int slot, ReliableKind kind, const void* raw, int n, uint8_t = 0) {
        Sent packet{slot, kind, std::vector<uint8_t>(n)};
        std::memcpy(packet.data.data(), raw, n);
        attempts.push_back(packet);
        // Take an owned callable: the callback may replace this session's callback.
        const auto callback = onSend;
        const bool accepted = sendOk && (!callback || callback(packet));
        if (accepted) sent.push_back(std::move(packet));
        return accepted;
    }
    bool SendReliable(ReliableKind kind, const void* raw, int n, uint8_t = 0) {
        return SendReliableToSlot(-1, kind, raw, n);
    }
};
}
