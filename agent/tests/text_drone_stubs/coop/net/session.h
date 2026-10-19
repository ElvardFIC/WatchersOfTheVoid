#pragma once
#include "coop/net/protocol.h"
#include <vector>
#include <cstring>
namespace coop::net {
inline constexpr int kMaxPeers=4;
enum class Role {Host,Client};
class Session {
public:
 struct ReliableMessage {};
 Role mode=Role::Host;bool linked=true,sendOk=true,ready=true;
 struct Sent{int slot;ReliableKind kind;std::vector<uint8_t> data;};std::vector<Sent> sent;
 bool connected()const{return linked;}bool running()const{return linked;}Role role()const{return mode;}
 bool AnyWorldReadyPeer()const{return ready;}bool IsSlotWorldReady(int)const{return ready;}
 bool SendReliableToSlot(int slot,ReliableKind k,const void* data,int n){
  if(!sendOk)return false;Sent s{slot,k,{}};s.data.resize(n);std::memcpy(s.data.data(),data,n);sent.push_back(s);return true;
 }
 bool SendReliable(ReliableKind k,const void* data,int n){return SendReliableToSlot(-1,k,data,n);}
};
}
