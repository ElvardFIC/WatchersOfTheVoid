#pragma once
#include "coop/net/protocol.h"
#include <vector>
#include <array>
#include <functional>
#include <utility>
namespace coop::net {
struct Session {
    Role mode = Role::Client;
    bool linked = true;
    bool active=true,sendOk=true;
    std::array<uint32_t,kMaxPeers> generations=[] {std::array<uint32_t,kMaxPeers> r{};r.fill(1);return r;}();
    std::function<void()> duringSend;
    std::function<bool(int,const SleepStatePayload&)> allowSend;
    bool ready[kMaxPeers]{};
    bool joining[kMaxPeers]{};
    std::vector<SleepStatePayload> sent;
    std::vector<int> destinations;
    int attempts=0;
    Role role() const { return mode; }
    bool running() const { return active; }
    bool connected() const { return linked; }
    bool IsSlotWorldReady(int slot) const { return ready[slot]; }
    bool IsSlotConnected(int slot) const { return ready[slot] || joining[slot]; }
    uint32_t peerGenerationForSlot(int slot) const {return generations[slot];}
    bool SendReliable(ReliableKind kind, const void* data, size_t) {
        if (kind != ReliableKind::SleepState) throw "Unexpected dream broadcast";
        return SendReliableToSlot(-1,kind,data,sizeof(SleepStatePayload));
    }
    bool SendReliableToSlot(int slot, ReliableKind kind, const void* data, size_t size) {
        ++attempts;
        if(kind!=ReliableKind::SleepState || size!=sizeof(SleepStatePayload))throw "Unexpected sleep packet";
        const auto p=*static_cast<const SleepStatePayload*>(data);
        auto callback=std::move(duringSend);duringSend={};if(callback)callback();
        if(!sendOk || (allowSend && !allowSend(slot,p)))return false;
        sent.push_back(p);destinations.push_back(slot);return true;
    }
};
}
