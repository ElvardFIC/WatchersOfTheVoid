#pragma once
#include "coop/net/protocol.h"
#include <vector>
#include <cstring>
#include <functional>
#include <array>
#ifndef TEST_STATE_MAX_PEERS
#define TEST_STATE_MAX_PEERS 4
#endif
namespace coop::net {
inline constexpr int kMaxPeers=TEST_STATE_MAX_PEERS;
enum class Role {Host,Client};
class Session {
public:
 struct ReliableMessage {};
 Role mode=Role::Host;bool linked=true,active=true,sendOk=true,ready=true;
 std::function<bool(int,ReliableKind,const void*,int)> allowSend;
 struct Sent{int slot;ReliableKind kind;std::vector<uint8_t> data;uint8_t origin=0;};std::vector<Sent> sent;
 bool poseActive=false; PropPoseSnapshot pose{};
 uint32_t peerEpoch=1;
 bool perSlot=false;
 std::array<uint32_t,kMaxPeers> generations{};
 std::array<bool,kMaxPeers> links{},lanes{},worlds{};
 int testRtt=-1;
 int rttMsForSlot(int)const{return testRtt;}
 uint32_t peerGenerationForSlot(int slot)const{return perSlot?generations.at(slot):peerEpoch;}
 void SetLocalPropPose(bool active,const PropPoseSnapshot& p){poseActive=active;pose=p;}
 bool IsSlotReady(int slot)const{return perSlot?lanes.at(slot):ready;}
 bool IsSlotConnected(int slot)const{return perSlot?links.at(slot):ready;}
 bool connected()const{return linked;}bool running()const{return linked&&active;}Role role()const{return mode;}
 bool AnyWorldReadyPeer()const{return ready;}bool IsSlotWorldReady(int slot)const{return perSlot?worlds.at(slot):ready;}
 bool SendReliableToSlot(int slot,ReliableKind k,const void* data,int n,uint8_t origin=0){
  if(!sendOk || (allowSend && !allowSend(slot,k,data,n)))return false;Sent s{slot,k,{},origin};s.data.resize(n);std::memcpy(s.data.data(),data,n);sent.push_back(s);return true;
 }
 bool SendReliable(ReliableKind k,const void* data,int n,uint8_t origin=0){return SendReliableToSlot(-1,k,data,n);}

    bool SendReliableBatchToSlot(int slot, ReliableKind kind, const void* raw, int bytes, int count) {
        if (!raw || bytes < 0 || count <= 0) return false;
        std::vector<uint8_t> owned(static_cast<const uint8_t*>(raw),static_cast<const uint8_t*>(raw)+static_cast<size_t>(bytes)*count);
        for(int i=0;i<count;++i) if(!SendReliableToSlot(slot,kind,owned.data()+static_cast<size_t>(i)*bytes,bytes)) return false;
        return true;
    }
    bool SendReliableBatch(ReliableKind kind, const void* raw, int bytes, int count) {
        if (!raw || bytes < 0 || count <= 0) return false;
        std::vector<uint8_t> owned(static_cast<const uint8_t*>(raw),static_cast<const uint8_t*>(raw)+static_cast<size_t>(bytes)*count);
        for(int i=0;i<count;++i) if(!SendReliable(kind,owned.data()+static_cast<size_t>(i)*bytes,bytes)) return false;
        return true;
    }
};
}
