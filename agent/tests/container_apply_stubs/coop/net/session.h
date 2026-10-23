#pragma once
#include "coop/net/protocol.h"
#include "coop/net/peer_limits.h"
#include <array>
#include <functional>
namespace coop::net {
inline constexpr int kMaxPeers=kPeerCapacity;
enum class Role:uint8_t {Host,Client};
class Session {
public:
    Role mode=Role::Host;bool active=true,linked=true,sendOK=true;
    std::array<bool,kMaxPeers> peers{};
    std::array<uint32_t,kMaxPeers> generations{};
    std::function<void()> duringSend;
    bool running()const{return active;}bool connected()const{return linked;}
    Role role()const{return mode;}
    bool HasPeerConn(int slot)const{return peers[slot];}
    bool IsSlotConnected(int slot)const{return peers[slot];}
    bool IsSlotWorldReady(int slot)const{return peers[slot];}
    uint32_t peerGenerationForSlot(int slot)const{return generations[slot];}
};
}
