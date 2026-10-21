#pragma once
#include "coop/net/protocol.h"
#include <vector>
#include <cstring>
#include <cstdlib>
#include <array>
#include <functional>
namespace coop::net {
inline constexpr int kMaxPeers=4;
enum class Role {Host,Client};
class Session {
public:
 struct ReliableMessage {};
 Role mode=Role::Host;bool linked=true,sendOk=true;
 std::array<bool,4> ready{true,true,true,true};std::function<void()> onSend;
 std::function<bool(int)> allowSend;
 struct Sent{int slot;PropFirePayload state;};std::vector<Sent> sent;
 bool poseActive=false;PropPoseSnapshot pose{};
 void SetLocalPropPose(bool active,const PropPoseSnapshot& snapshot){poseActive=active;pose=snapshot;}
 bool connected()const{return linked;}Role role()const{return mode;}
 bool IsSlotWorldReady(int slot)const{return ready.at(slot);}
 bool SendReliableToSlot(int slot,ReliableKind k,const void* data,int n){
  if(k!=ReliableKind::PropFire||n!=sizeof(PropFirePayload))std::abort();
  const bool accepted=sendOk && (!allowSend || allowSend(slot));auto callback=std::move(onSend);onSend={};if(callback)callback();
  if(!accepted)return false;Sent s{slot,{}};std::memcpy(&s.state,data,n);sent.push_back(s);return true;
 }
 bool SendReliable(ReliableKind k,const void* data,int n){return SendReliableToSlot(-1,k,data,n);}
};
}
