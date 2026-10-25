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
    struct ReliableMessage {};
    Role mode=Role::Host;bool linked=true,active=true,sendOk=true,ready=true;
    std::array<uint32_t,kMaxPeers> generations;
    std::function<bool(int,ReliableKind,const void*,int)> allowSend;
    struct Sent {int slot;ReliableKind kind;std::vector<uint8_t> data;};std::vector<Sent> sent;
    Session(){generations.fill(1);}
    uint32_t peerGenerationForSlot(int slot)const{return generations.at(slot);}
    bool connected()const{return linked;}bool running()const{return active && linked;}Role role()const{return mode;}
    bool IsSlotWorldReady(int)const{return ready;}
    bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,int bytes){
        if(!sendOk || (allowSend && !allowSend(slot,kind,data,bytes)))return false;
        Sent row{slot,kind,{}};row.data.resize(bytes);std::memcpy(row.data.data(),data,bytes);sent.push_back(row);return true;
    }
};
}
