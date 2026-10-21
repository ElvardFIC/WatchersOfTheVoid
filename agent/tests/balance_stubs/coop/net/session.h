#pragma once
#include "coop/net/protocol.h"
#include <array>
#include <functional>
#include <vector>
#include <cassert>
namespace coop::net {
inline constexpr uint8_t kMaxPeers = 4;
enum class Role { Host, Client };
struct Session {
    Role mode = Role::Host;
    bool linked = true;
    std::array<bool,4> ready{false,true,true,false}, allowed{true,true,true,true};
    std::array<uint32_t,4> generation{0,1,2,0};
    struct Sent { int slot; int32_t value; };
    std::vector<Sent> sent;
    std::function<void()> callback;
    bool connected() const { return linked; }
    Role role() const { return mode; }
    bool IsSlotReady(int slot) const { return ready[slot]; }
    uint32_t peerGenerationForSlot(int slot) const { return generation[slot]; }
    bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,size_t size) {
        assert(kind==ReliableKind::BalanceSync && size==sizeof(BalancePayload));
        if(!allowed[slot]) return false;
        sent.push_back({slot,static_cast<const BalancePayload*>(data)->value});
        auto cb=std::move(callback);callback={};if(cb)cb();return true;
    }
};
}
