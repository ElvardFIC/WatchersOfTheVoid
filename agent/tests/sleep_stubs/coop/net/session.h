#pragma once
#include "coop/net/protocol.h"
#include <vector>
namespace coop::net {
struct Session {
    Role mode = Role::Client;
    bool linked = true;
    bool ready[kMaxPeers]{};
    std::vector<SleepStatePayload> sent;
    Role role() const { return mode; }
    bool running() const { return true; }
    bool connected() const { return linked; }
    bool IsSlotWorldReady(int slot) const { return ready[slot]; }
    bool SendReliable(ReliableKind kind, const void* data, size_t) {
        if (kind != ReliableKind::SleepState) throw "Unexpected dream broadcast";
        sent.push_back(*static_cast<const SleepStatePayload*>(data));
        return true;
    }
    bool SendReliableToSlot(int, ReliableKind kind, const void* data, size_t size) {
        return SendReliable(kind, data, size);
    }
};
}
