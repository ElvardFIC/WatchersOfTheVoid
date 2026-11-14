#pragma once
#include "coop/net/protocol.h"
#include <array>
#include <functional>
#include <vector>
namespace coop::net {
inline constexpr int kMaxPeers=32;
enum class Role {Host,Client};
class Session {
public:
    bool linked=true,active=true;
    Role mode=Role::Host;
    std::array<bool,kMaxPeers> ready{};
    std::vector<WindowStrokePayload> sent;
    std::function<bool(const WindowStrokePayload&)> allow;
    bool connected()const{return linked;}
    bool running()const{return active;}
    Role role()const{return mode;}
    bool IsSlotWorldReady(int slot)const{return ready.at(slot);}
    bool SendReliable(ReliableKind kind,const void* data,size_t bytes){
        if(kind!=ReliableKind::WindowStroke || bytes!=sizeof(WindowStrokePayload))return false;
        WindowStrokePayload owned{};std::memcpy(&owned,data,bytes);
        const bool accepted=!allow || allow(owned);
        if(accepted)sent.push_back(owned);
        return accepted;
    }
};
}
