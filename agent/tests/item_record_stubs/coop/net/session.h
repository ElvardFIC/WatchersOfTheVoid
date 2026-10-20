#pragma once
#include "coop/net/protocol.h"
#include <cstring>
#include <vector>
#include <functional>
namespace coop::net {
inline constexpr uint8_t kMaxPeers=4;
enum class Role : uint8_t { Host, Client };
class Session {
public:
    struct ReliableMessage;
    bool linked=true,sendOK=true;
    bool ready=true;
    int blockedSlot=-1;
    Role mode=Role::Client;
    std::vector<PropDropIntentPayload> drops;
    std::vector<std::pair<int,PropPlacementResultPayload>> results;
    std::function<void()> duringSend;
    std::function<void(const PropDropIntentPayload&)> onDrop;
    bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,size_t size) {
        if(duringSend){auto callback=std::move(duringSend);duringSend={};callback();}
        if(!sendOK || kind!=ReliableKind::PropPlacementResult || size!=sizeof(PropPlacementResultPayload))return false;
        PropPlacementResultPayload p{};std::memcpy(&p,data,size);results.push_back({slot,p});return true;
    }
    bool connected() const { return linked; }
    Role role() const { return mode; }
    bool AnyWorldReadyPeer() const { return ready; }
    bool IsSlotWorldReady(int slot) const { return ready && slot!=blockedSlot; }
    bool SendReliable(ReliableKind, const void* data, size_t size) {
        if (!sendOK || size!=sizeof(PropDropIntentPayload)) return false;
        PropDropIntentPayload p{};std::memcpy(&p,data,size);drops.push_back(p);if(onDrop)onDrop(p);return true;
    }
};
}
