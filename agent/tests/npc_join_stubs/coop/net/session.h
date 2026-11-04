#pragma once
#include "coop/net/protocol.h"
#include <array>
#include <vector>
#include <functional>
#include <cstring>
namespace coop::net {
inline constexpr int kMaxPeers=32;
enum class Role {Host,Client};
struct Session {
 Role mode=Role::Host;bool active=true,linked=true,ready=true,sendOk=true;
 uint64_t stops=0;
 std::array<uint32_t,32> generations=[] {std::array<uint32_t,32> a{};a.fill(1);return a;}();
 std::array<uint64_t,32> worldRevisions{};
 std::array<bool,32> online=[] {std::array<bool,32> a{};a[31]=true;return a;}();
 struct Packet{int slot;ReliableKind kind;std::vector<uint8_t> data;};
 std::vector<Packet> packets;
 std::vector<Packet> attempts;
 std::vector<int> closed;
 void CloseJoinGate(int slot){closed.push_back(slot);}
 std::function<bool(int,ReliableKind,const void*)> allow;
 std::function<void()> onSend;
 std::vector<EntityPoseSnapshot> poseBatch;
 int poseWrites=0;
 void SetLocalNpcPoseBatch(const std::vector<EntityPoseSnapshot>& value){poseBatch=value;++poseWrites;}
 bool running()const{return active;}bool connected()const{return linked;}Role role()const{return mode;}
 uint64_t StopCount()const{return stops;}
 bool IsSlotConnected(int slot)const{return online.at(slot);}
 bool IsSlotWorldReady(int slot)const{return ready&&online.at(slot);}
 uint32_t peerGenerationForSlot(int slot)const{return generations.at(slot);}
 uint64_t SlotWorldReadyRevision(int slot)const{return worldRevisions.at(slot);}
 void ResetWorld(int slot){++worldRevisions.at(slot);}
 bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,int length){
  Packet p{slot,kind,std::vector<uint8_t>(length)};std::memcpy(p.data.data(),data,length);attempts.push_back(p);
  const bool accepted=sendOk&&(!allow||allow(slot,kind,data));
  if(accepted)packets.push_back(std::move(p));
  if(onSend)onSend();return accepted;
 }
};
}
