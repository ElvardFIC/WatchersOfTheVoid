#pragma once
#include "coop/net/protocol.h"
#include <array>
#include <functional>
namespace coop::net {
struct Config;
inline constexpr int kMaxPeers = 32;
enum class Role {Host, Client};
class Session {
public:
    Role mode = Role::Host;
    bool live = true, linked = true;
    uint64_t stops = 0;
    std::array<uint32_t,32> generations{};
    std::array<bool,32> online{}, ready{};
    int sends = 0;
    std::function<void()> onSend;
    bool running() const {return live;}
    bool connected() const {return linked;}
    Role role() const {return mode;}
    uint64_t StopCount() const {return stops;}
    uint32_t peerGenerationForSlot(int i) const {return generations.at(i);}
    bool IsSlotConnected(int i) const {return online.at(i);}
    bool IsSlotWorldReady(int i) const {return ready.at(i);}
    bool SendReliable(ReliableKind,const void*,size_t){++sends;auto cb=onSend;if(cb)cb();return linked;}
};
}
