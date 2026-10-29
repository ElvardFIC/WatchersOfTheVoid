#pragma once
#include "coop/net/protocol.h"
#include <cstring>
#include <vector>
#include <functional>
namespace coop::net {
enum class Role : uint8_t { Host, Client };
class Session {
public:
    struct ReliableMessage {};
    Role mode = Role::Host;
    bool linked = true;
    bool started = true;
    bool worldReady=true;
    std::function<void()> onSend;
    int sendBudget = -1;
    std::vector<BlobChunkPayload> sent;
    std::vector<int> recipients;
    bool connected() const { return linked; }
    bool running() const { return started; }
    Role role() const { return mode; }
    bool IsSlotWorldReady(int)const{return worldReady;}
    bool SendReliable(ReliableKind kind, const void* data, size_t size) {
        if (kind != ReliableKind::PlacedSave || size != sizeof(BlobChunkPayload) || sendBudget == 0) return false;
        if (sendBudget > 0) --sendBudget;
        BlobChunkPayload p{}; std::memcpy(&p, data, size); sent.push_back(p); recipients.push_back(-1);
        auto callback=std::move(onSend);onSend={};if(callback)callback();return true;
    }
    bool SendReliableToSlot(int slot, ReliableKind k, const void* p, size_t n) {
        if (!SendReliable(k, p, n)) return false;
        recipients.back()=slot;return true;
    }

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
