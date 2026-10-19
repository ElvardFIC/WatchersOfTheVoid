#pragma once
#include "coop/net/protocol.h"
#include <cstring>
#include <functional>
#include <vector>
namespace coop::net {
enum class Role { Host, Client };
class Session {
public:
    Role mode = Role::Host;
    bool linked = true, started = true;
    bool ready[4]{false, true, true, true}, peers[4]{false, true, true, true};
    int blockedSlot = -1;
    struct Sent { int slot; EventCuePayload payload; };
    std::vector<Sent> sent;
    std::function<void()> callback;
    bool connected() const { return linked; }
    bool running() const { return started; }
    Role role() const { return mode; }
    bool HasPeerConn(int slot) const { return peers[slot]; }
    bool IsSlotWorldReady(int slot) const { return ready[slot]; }
    bool SendReliableToSlot(int slot, ReliableKind kind, const void* data, size_t size) {
        if (blockedSlot == slot || kind != ReliableKind::EventCue || size != sizeof(EventCuePayload)) return false;
        Sent s{slot}; std::memcpy(&s.payload, data, size); sent.push_back(s);
        auto cb = std::move(callback); callback = {}; if (cb) cb();
        return true;
    }
};
}
