#pragma once
#include "coop/net/protocol.h"
namespace coop::net {
inline constexpr int kMaxPeers=4;
enum class Role {Host,Client};
class Session {
public:
    Role mode=Role::Client;
    bool linked=true;
    int births=0;
    bool connected()const{return linked;}
    Role role()const{return mode;}
    bool SendPropSpawn(const PropSpawnPayload&){++births;return true;}
};
}
