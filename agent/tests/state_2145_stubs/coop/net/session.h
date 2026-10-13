#pragma once
#include "coop/net/protocol.h"
#include <vector>
#include <cstring>
#include <functional>
namespace coop::net {
inline constexpr int kMaxPeers=4;
enum class Role {Host,Client};
class Session {
public:
 struct ReliableMessage {};
 Role mode=Role::Host;bool linked=true,sendOk=true,ready=true;
 std::function<bool(int,ReliableKind,const void*,int)> allowSend;
 struct Sent{int slot;ReliableKind kind;std::vector<uint8_t> data;};std::vector<Sent> sent;
 bool poseActive=false; PropPoseSnapshot pose{};
 void SetLocalPropPose(bool active,const PropPoseSnapshot& p){poseActive=active;pose=p;}
 bool IsSlotReady(int)const{return ready;}
 bool connected()const{return linked;}bool running()const{return linked;}Role role()const{return mode;}
 bool AnyWorldReadyPeer()const{return ready;}bool IsSlotWorldReady(int)const{return ready;}
 bool SendReliableToSlot(int slot,ReliableKind k,const void* data,int n,uint8_t origin=0){
  if(!sendOk || (allowSend && !allowSend(slot,k,data,n)))return false;Sent s{slot,k,{}};s.data.resize(n);std::memcpy(s.data.data(),data,n);sent.push_back(s);return true;
 }
 bool SendReliable(ReliableKind k,const void* data,int n,uint8_t origin=0){return SendReliableToSlot(-1,k,data,n);}
};
}
