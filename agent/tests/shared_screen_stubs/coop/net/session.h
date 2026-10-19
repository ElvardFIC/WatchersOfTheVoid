#pragma once
#include "coop/net/protocol.h"
#include <vector>
#include <utility>
#include <cassert>
namespace coop::net {
inline constexpr uint8_t kMaxPeers = 4;
enum class Role : uint8_t { Host, Client };
struct Session {
    Role mode = Role::Host;
    bool linked = true, refuse = false, ready[kMaxPeers]{};
    int attempts = 0;
    std::vector<std::pair<int,BlobChunkPayload>> sent;
    Role role()const{return mode;}
    bool connected()const{return linked;}
    bool IsSlotWorldReady(int slot)const{return ready[slot];}
    bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,size_t size){
        assert(kind==ReliableKind::SharedScreen&&size==sizeof(BlobChunkPayload));
        ++attempts;if(refuse)return false;
        sent.emplace_back(slot,*static_cast<const BlobChunkPayload*>(data));return true;
    }
    bool SendReliable(ReliableKind kind,const void* data,size_t size){return SendReliableToSlot(-1,kind,data,size);}
};
}
