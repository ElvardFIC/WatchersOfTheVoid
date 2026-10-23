#pragma once
#include "coop/net/protocol.h"
#include "coop/net/peer_limits.h"
#include <array>
#include <functional>
#include <vector>
namespace coop::net {
inline constexpr int kMaxPeers=kPeerCapacity;
enum class Role:uint8_t {Host,Client};
class Session {
public:
    bool live=true,sendOK=true;Role mode=Role::Host;
    std::array<bool,kMaxPeers> peers{},ready{};
    std::array<uint32_t,kMaxPeers> generations{};
    struct Sent {int slot;AriralShipStatePayload state;};
    std::vector<Sent> sent;
    std::function<void()> duringSend;
    bool running()const{return live;}
    bool connected()const{return live;}
    Role role()const{return mode;}
    bool HasPeerConn(int slot)const{return peers[slot];}
    bool IsSlotWorldReady(int slot)const{return ready[slot];}
    uint32_t peerGenerationForSlot(int slot)const{return generations[slot];}
    bool SendReliableToSlot(int slot,ReliableKind kind,const void* p,uint16_t size) {
        if(kind!=ReliableKind::AriralShipState || size!=sizeof(AriralShipStatePayload))return false;
        const bool accepted=sendOK;
        if(accepted)sent.push_back({slot,*static_cast<const AriralShipStatePayload*>(p)});
        auto callback=std::move(duringSend);duringSend={};if(callback)callback();return accepted;
    }
};
}
