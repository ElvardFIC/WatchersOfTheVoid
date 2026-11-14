#pragma once
#include "coop/net/protocol.h"
#include <vector>
#include <cstring>
#include <functional>
namespace coop::net {
inline constexpr int kMaxPeers=4;
enum class Role {Host,Client};
class Session {
public:
 struct ReliableMessage {};
 Role mode=Role::Host;bool linked=true,sendOk=true,ready=true,active=true;
 std::function<void()> beforeSend;
 struct Sent{int slot;ReliableKind kind;std::vector<uint8_t> data;};std::vector<Sent> sent;
 bool connected()const{return linked;}bool running()const{return active && linked;}Role role()const{return mode;}
 bool AnyWorldReadyPeer()const{return ready;}bool IsSlotWorldReady(int)const{return ready;}
 bool SendReliableToSlot(int slot,ReliableKind k,const void* data,int n){
  auto callback=std::move(beforeSend);beforeSend={};if(callback)callback();
  if(!sendOk)return false;Sent s{slot,k,{}};s.data.resize(n);std::memcpy(s.data.data(),data,n);sent.push_back(s);return true;
 }
 bool SendReliable(ReliableKind k,const void* data,int n){return SendReliableToSlot(-1,k,data,n);}

    bool SendReliableBatchToSlot(int slot, ReliableKind kind, const void* raw, int bytes, int count) {
        if (!raw || bytes < 0 || count <= 0) return false;
        std::vector<uint8_t> owned(static_cast<const uint8_t*>(raw),static_cast<const uint8_t*>(raw)+static_cast<size_t>(bytes)*count);
        for(int i=0;i<count;++i) if(!SendReliableToSlot(slot,kind,owned.data()+static_cast<size_t>(i)*bytes,bytes)) return false;
        return true;
    }
    bool SendReliableBatch(ReliableKind kind, const void* raw, int bytes, int count) {
        if (!raw || bytes < 0 || count <= 0) return false;
        std::vector<uint8_t> owned(static_cast<const uint8_t*>(raw),static_cast<const uint8_t*>(raw)+static_cast<size_t>(bytes)*count);
        for(int i=0;i<count;++i) if(!SendReliable(kind,owned.data()+static_cast<size_t>(i)*bytes,bytes)) return false;
        return true;
    }
};
}
