#pragma once
#include "coop/net/protocol.h"
#include <vector>
#include <cstring>
#include <functional>
namespace coop::net {
inline constexpr int kMaxPeers=4;enum class Role{Host,Client};
class Session {public:
 Role mode=Role::Host;bool linked=true,ready[4]{true,true,false,false},sendOk[4]{true,true,true,true};
 struct Sent{int slot;ReliableKind kind;std::vector<uint8_t> bytes;};std::vector<Sent> sent;
 std::function<void()> onSend;
 bool connected()const{return linked;}Role role()const{return mode;}
 uint32_t peerGenerationForSlot(int)const{return 1;}
 bool IsSlotWorldReady(int i)const{return ready[i];}bool HasPeerConn(int i)const{return ready[i];}
 bool AnyWorldReadyPeer()const{return ready[1]||ready[2]||ready[3];}
 bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,int size) {
  if(!sendOk[slot])return false;Sent p{slot,kind,{}};p.bytes.resize(size);std::memcpy(p.bytes.data(),data,size);sent.push_back(p);
  auto cb=std::move(onSend);onSend={};if(cb)cb();return true;
 }
 bool SendReliable(ReliableKind kind,const void* data,int size){return SendReliableToSlot(1,kind,data,size);}
};}
