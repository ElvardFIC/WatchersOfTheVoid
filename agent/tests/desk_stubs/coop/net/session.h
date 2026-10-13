#pragma once
#include "coop/net/protocol.h"
#include <vector>
#include <cstring>
namespace coop::net {
inline constexpr int kMaxPeers=4;
enum class Role { Host,Client };
class Session {
public:
 Role mode=Role::Host; bool linked=true,sendOk=true;
 struct Sent {int slot; DeskVisualPayload state;};
 std::vector<Sent> sent;
 bool connected() const {return linked;}
 Role role() const {return mode;}
 bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,int len) {
  if(kind!=ReliableKind::DishAimState || len!=sizeof(DeskVisualPayload)) std::abort();
  if(!sendOk)return false;
  Sent s{slot,{}};std::memcpy(&s.state,data,len);sent.push_back(s);return true;
 }
 bool SendReliable(ReliableKind kind,const void* data,int len){return SendReliableToSlot(-1,kind,data,len);}
};
}
