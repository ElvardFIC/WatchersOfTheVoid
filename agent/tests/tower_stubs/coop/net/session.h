#pragma once
#include "coop/net/protocol.h"
#include <array>
#include <functional>
#include <vector>
#include <cstring>
namespace coop::net {
inline constexpr int kMaxPeers=kPeerCapacity;
enum class Role {Host,Client};
class Session {
public:
    Role mode=Role::Host;
    bool live=true,linked=true,sendOk=true;
    uint64_t stops=0;
    std::array<uint32_t,32> generations=[] {std::array<uint32_t,32>a{};a.fill(1);return a;}();
    std::array<bool,32> online=[] {std::array<bool,32>a{};a.fill(true);return a;}(),ready=online;
    struct Sent {FuseTowerPayload packet;int slot;};
    std::vector<Sent> sent;
    std::function<void()> onSend;
    bool running()const{return live;}
    bool connected()const{return linked;}
    Role role()const{return mode;}
    uint64_t StopCount()const{return stops;}
    uint32_t peerGenerationForSlot(int i)const{return generations.at(i);}
    bool IsSlotConnected(int i)const{return online.at(i);}
    bool IsSlotWorldReady(int i)const{return ready.at(i);}
    bool Send(int slot,ReliableKind kind,const void* data,size_t size){
        auto cb=std::move(onSend);onSend={};if(cb)cb();if(!sendOk)return false;
        if(kind!=ReliableKind::FuseTowerState||size!=sizeof(FuseTowerPayload))return false;
        Sent row{};std::memcpy(&row.packet,data,size);row.slot=slot;sent.push_back(row);return true;
    }
    bool SendReliable(ReliableKind k,const void* p,size_t n){return Send(-1,k,p,n);}
    bool SendReliableToSlot(int i,ReliableKind k,const void* p,size_t n){return Send(i,k,p,n);}
};
}
