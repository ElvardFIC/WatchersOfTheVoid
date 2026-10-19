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
 struct ReliableMessage { const uint8_t* payload=nullptr; int payloadLen=0; int senderPeerSlot=0; };
 struct Sent { int slot; ReliableKind kind; std::vector<uint8_t> data; };
 Role mode=Role::Host; bool linked=true,sendOk=true; bool ready[4]{true,true,true,true}; std::vector<Sent> sent;
 bool connected()const{return linked;} Role role()const{return mode;}
 bool IsSlotReady(int s)const{return s>=0&&s<4&&ready[s];}
 bool SendReliableToSlot(int s,ReliableKind k,const void* data,int n){
  if(n>kMaxReliablePayload)std::abort();if(!sendOk)return false;
  const auto* p=static_cast<const uint8_t*>(data);sent.push_back({s,k,{p,p+n}});return true;
 }
 void SetLocalHandPose(bool,const HandPoseSnapshot&){}
 bool TryGetRemoteHandPose(uint8_t,HandPoseSnapshot&,bool*){return false;}
};
}
