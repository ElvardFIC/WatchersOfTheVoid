#pragma once
#include "coop/net/protocol.h"
#include <vector>
#include <functional>
#include <cstring>
namespace coop::net {
inline constexpr int kMaxPeers = 4;
enum class Role { Host, Client };
class Session { public:
    Role mode = Role::Host;
    bool active = true, linked = true;
    bool peers[4]{false, true, true, false}, ok[4]{true, true, true, true};
    bool ready[4]{false, true, true, false};
    uint32_t generation[4]{1, 1, 1, 1};
    struct Sent { int slot; ReliableKind kind; std::vector<uint8_t> bytes; };
    std::vector<Sent> sent;
    std::function<void()> onSend;
    bool running() const { return active; }
    bool connected() const { return linked; }
    Role role() const { return mode; }
    bool HasPeerConn(int slot) const { return peers[slot]; }
    bool IsSlotWorldReady(int slot) const { return ready[slot]; }
    uint32_t peerGenerationForSlot(int slot) const { return generation[slot]; }
    bool SendReliableToSlot(int slot, ReliableKind kind, const void* data, int len) {
        if (!ok[slot]) return false;
        Sent item{slot, kind, std::vector<uint8_t>(len)};
        std::memcpy(item.bytes.data(), data, len); sent.push_back(item);
        auto cb = std::move(onSend); onSend = {}; if (cb) cb(); return true;
    }
    bool SendReliable(ReliableKind kind, const void* data, int len) { return SendReliableToSlot(1, kind, data, len); }
};
}
