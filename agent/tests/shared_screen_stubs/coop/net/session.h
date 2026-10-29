#pragma once
#include "coop/net/protocol.h"
#include <vector>
#include <utility>
#include <cassert>
#include <functional>
namespace coop::net {
inline constexpr uint8_t kMaxPeers = 4;
enum class Role : uint8_t { Host, Client };
struct Session {
    Role mode = Role::Host;
    bool linked = true, started = true, refuse = false, ready[kMaxPeers]{};
    bool peers[kMaxPeers]{false, true, true, true};
    int budget = -1, blocked = -1;
    std::function<void()> callback;
    int attempts = 0;
    std::vector<std::pair<int,BlobChunkPayload>> sent;
    Role role()const{return mode;}
    bool connected()const{return linked;}
    bool running()const{return started;}
    bool HasPeerConn(int slot)const{return peers[slot];}
    bool IsSlotWorldReady(int slot)const{return ready[slot];}
    bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,size_t size){
        assert(kind==ReliableKind::SharedScreen&&size==sizeof(BlobChunkPayload));
        ++attempts;if(refuse || budget == 0 || blocked == slot)return false;
        if (budget > 0) --budget;
        sent.emplace_back(slot,*static_cast<const BlobChunkPayload*>(data));
        auto cb = std::move(callback); callback = {}; if (cb) cb();
        return true;
    }
    bool SendReliable(ReliableKind kind,const void* data,size_t size){return SendReliableToSlot(-1,kind,data,size);}

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
