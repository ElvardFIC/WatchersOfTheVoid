#pragma once
#include "coop/net/protocol.h"
#include <vector>
namespace coop::net {
enum class Role {Host,Client};
class Session {
public:
    Role mode=Role::Host;bool active=true,linked=true,ready=true,sendOk=true;
    std::vector<EntitySpawnPayload> spawns;
    bool running()const{return active;}bool connected()const{return linked;}Role role()const{return mode;}
    bool AnyWorldReadyPeer(int=-1)const{return ready;}
    bool SendEntitySpawn(const EntitySpawnPayload& p,int){if(!sendOk)return false;spawns.push_back(p);return true;}
};
}
