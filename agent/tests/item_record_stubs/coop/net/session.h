#pragma once
#include "coop/net/protocol.h"
#include <cstring>
#include <vector>
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
    bool connected() const { return linked; }
    Role role() const { return mode; }
    bool AnyWorldReadyPeer() const { return ready; }
    bool IsSlotWorldReady(int slot) const { return ready && slot!=blockedSlot; }
    bool SendReliable(ReliableKind, const void* data, size_t size) {
        if (!sendOK || size!=sizeof(PropDropIntentPayload)) return false;
        PropDropIntentPayload p{};std::memcpy(&p,data,size);drops.push_back(p);return true;
    }
};
}
