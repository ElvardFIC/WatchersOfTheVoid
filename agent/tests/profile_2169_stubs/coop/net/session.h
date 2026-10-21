#pragma once
#include "coop/net/protocol.h"
#include <array>
#include <vector>
#include <cstring>
#include <functional>
namespace coop::net {
inline constexpr int kMaxPeers=4;
enum class Role {Host,Client};
struct Config;
class Session {
public:
 struct ReliableMessage {};
 Role mode=Role::Host; bool linked=true,started=true,sendOk=true;
 std::array<bool,kMaxPeers> slots{true,true,true,true};
 std::vector<BlobChunkPayload> sent;
 std::function<void()> onSend;
 bool running()const{return started;} bool connected()const{return linked;} Role role()const{return mode;}
 bool IsSlotConnected(int slot)const{return slots.at(slot);}
 bool SendReliableToSlot(int,ReliableKind,const void* data,size_t n){
  if(!sendOk)return false; BlobChunkPayload p{}; if(n!=sizeof(p))return false;
  std::memcpy(&p,data,n);sent.push_back(p);auto callback=std::move(onSend);onSend={};if(callback)callback();return true;
 }
 bool SendReliable(ReliableKind k,const void* data,size_t n){return SendReliableToSlot(0,k,data,n);}
};
}
