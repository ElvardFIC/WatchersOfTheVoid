#pragma once
#include "coop/net/protocol.h"
#include <vector>
#include <cstring>
#include <cstdlib>
namespace coop::net {
inline constexpr int kMaxPeers=4;
enum class Role {Host,Client};
class Session {
public:
 struct ReliableMessage {};
 Role mode=Role::Host;bool linked=true,sendOk=true;
 struct Sent{int slot;PropFirePayload state;};std::vector<Sent> sent;
 bool connected()const{return linked;}Role role()const{return mode;}
 void SetLocalPropPose(bool,const PropPoseSnapshot&){}
 bool SendReliableToSlot(int slot,ReliableKind k,const void* data,int n){
  if(k!=ReliableKind::PropFire||n!=sizeof(PropFirePayload))std::abort();
  if(!sendOk)return false;Sent s{slot,{}};std::memcpy(&s.state,data,n);sent.push_back(s);return true;
 }
 bool SendReliable(ReliableKind k,const void* data,int n){return SendReliableToSlot(-1,k,data,n);}
};
}
