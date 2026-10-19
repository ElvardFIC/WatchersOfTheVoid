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
 Role mode=Role::Host;bool linked=true;bool sendOk[4]{true,true,true,true},ready[4]{true,true,true,true};
 struct Sent{int slot;ExplosionPayload p;};std::vector<Sent> sent;
 bool connected()const{return linked;}Role role()const{return mode;}
 bool IsSlotReady(int s)const{return ready[s];}
 bool SendReliableToSlot(int slot,ReliableKind k,const void* data,int n){
  if(k!=ReliableKind::Explosion||n!=sizeof(ExplosionPayload))std::abort();
  if(!sendOk[slot])return false;Sent s{slot,{}};std::memcpy(&s.p,data,n);sent.push_back(s);return true;
 }
};
}
