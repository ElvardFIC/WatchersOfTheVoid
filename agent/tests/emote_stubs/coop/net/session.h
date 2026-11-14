#pragma once
#include "coop/net/protocol.h"
#include <array>
#include <cstring>
#include <functional>
#include <vector>
namespace coop::net {
inline constexpr uint8_t kMaxPeers=kPeerCapacity;
enum class Role {Host,Client};
struct Session {
    struct Sent {int slot;EmoteStatePayload packet;};
    Role mode=Role::Host;bool linked=true,alive=true,sendOK=true;uint64_t stops=0;
    std::array<bool,kMaxPeers> ready{},online{};
    std::array<uint32_t,kMaxPeers> generation{};
    std::vector<Sent> sent;std::function<void()> callback;int attempts=0;
    Session(){generation.fill(1);online.fill(true);}
    Role role()const{return mode;}bool running()const{return alive;}bool connected()const{return linked;}
    uint64_t StopCount()const{return stops;}
    uint32_t peerGenerationForSlot(int i)const{return generation.at(i);}
    bool IsSlotWorldReady(int i)const{return ready.at(i);}
    bool IsSlotConnected(int i)const{return online.at(i);}
    int rttMsForSlot(int)const{return 100;}
    bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,int size){
        ++attempts;auto once=std::move(callback);callback={};if(once)once();
        if(!sendOK)return false;
        if(kind!=ReliableKind::EmoteState || size!=sizeof(EmoteStatePayload))std::abort();
        Sent entry{slot,{}};std::memcpy(&entry.packet,data,size);sent.push_back(entry);return true;
    }
};
}
