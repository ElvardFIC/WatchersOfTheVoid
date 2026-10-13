#pragma once
#include "coop/net/protocol.h"
#include <cstdlib>
namespace coop::net {
enum class Role : uint8_t { Host, Client };
class Session {
public:
    struct ReliableMessage;
    bool linked=true;
    Role mode=Role::Host;
    bool connected() const { return linked; }
    Role role() const { return mode; }
    bool SendPropSpawn(const PropSpawnPayload&) { std::abort(); }
    bool SendPropDestroy(const PropDestroyPayload&) { std::abort(); }
};
}
