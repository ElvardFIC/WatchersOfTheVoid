#pragma once
#include "coop/net/protocol.h"
namespace coop::net {
inline constexpr int kMaxPeers=4;
enum class Role {Host,Client};
struct Session {
    Role mode=Role::Host;
    bool active=true;
    bool linked=true;
    uint32_t readyMask=2;bool sendOk=true;
    int sends=0;GrayboarActionPayload sent{};
    Role role()const{return mode;}
    bool running()const{return active;}
    bool connected()const{return linked;}
    bool IsSlotWorldReady(int slot)const{return (readyMask&(1u<<slot))!=0;}
    bool SendReliableToSlot(int,ReliableKind kind,const void* data,int size) {
        if(kind!=ReliableKind::GrayboarAction || size!=sizeof(sent))return false;
        ++sends;sent=*static_cast<const GrayboarActionPayload*>(data);return sendOk;
    }
};
}
