#pragma once
#include "coop/net/protocol.h"
#include <cstring>
#include <vector>
#include <functional>
namespace coop::net {
enum class Role : uint8_t { Host, Client };
class Session {
public:
    struct ReliableMessage;
    bool linked=true,sendOK=true;
    bool ready=true;
    int blockedSlot=-1;
    Role mode=Role::Client;
    std::vector<PropDropIntentPayload> drops;
    std::vector<PropPlacementResultPayload> replies;
    std::function<void()> duringSend;
    std::function<void(const PropDropIntentPayload&)> onDrop;
    bool connected() const { return linked; }
    Role role() const { return mode; }
    bool HasPeerConn(int slot) const { return linked && slot > 0 && slot < 4 && slot != blockedSlot; }
    bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,size_t size) {
        if (!sendOK || !HasPeerConn(slot) || kind != ReliableKind::PropPlacementResult ||
            size != sizeof(PropPlacementResultPayload)) return false;
        PropPlacementResultPayload p{};std::memcpy(&p,data,size);replies.push_back(p);
        auto cb=std::move(duringSend);duringSend={};if(cb)cb();
        return true;
    }
    bool AnyWorldReadyPeer() const { return ready; }
    bool IsSlotWorldReady(int slot) const { return ready && slot!=blockedSlot; }
    bool SendReliable(ReliableKind, const void* data, size_t size) {
        if (!sendOK || size!=sizeof(PropDropIntentPayload)) return false;
        PropDropIntentPayload p{};std::memcpy(&p,data,size);drops.push_back(p);if(onDrop)onDrop(p);return true;
    }
};
}
