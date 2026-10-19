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
    bool peers[4]{false,true,true,true}, ready[4]{false,true,true,true};
    int blocked = -1, budget = -1;
    struct Sent { int slot; BlobChunkPayload chunk; };
    std::vector<Sent> sent;
    std::function<void()> callback;
    bool connected() const { return linked; }
    bool running() const { return started; }
    Role role() const { return mode; }
    bool HasPeerConn(int slot) const { return peers[slot]; }
    bool IsSlotWorldReady(int slot) const { return ready[slot]; }
    bool SendReliable(ReliableKind k, const void* p, size_t n) { return SendReliableToSlot(-1,k,p,n); }
    bool SendReliableToSlot(int slot, ReliableKind k, const void* p, size_t n) {
        if (k != ReliableKind::LaptopUI || n != sizeof(BlobChunkPayload) || blocked == slot || budget == 0) return false;
        if (budget > 0) --budget;
        Sent s{slot}; std::memcpy(&s.chunk,p,n); sent.push_back(s);
        auto cb = std::move(callback); callback = {}; if (cb) cb();
        return true;
    }
};
}
