#pragma once
#include "coop/net/protocol.h"
#include <vector>
#include <cstring>
#include <cstdlib>
namespace coop::net {
inline constexpr int kMaxPeers=4;enum class Role{Host,Client};
class Session{public:struct ReliableMessage{};Role mode=Role::Host;bool linked=true,ready[4]{true,true,true,true},sendOk[4]{true,true,true,true};
 struct Sent{int slot;ExplosionEventPayload p;};std::vector<Sent> sent;
 bool connected()const{return linked;}Role role()const{return mode;}bool IsSlotReady(int i)const{return ready[i];}
 bool SendReliableToSlot(int slot,ReliableKind k,const void* p,int n){if(k!=ReliableKind::ExplosionEvent||n!=sizeof(ExplosionEventPayload))std::abort();
  if(!sendOk[slot])return false;Sent s{slot,{}};std::memcpy(&s.p,p,n);sent.push_back(s);return true;}
};}
