#pragma once
#include "coop/net/protocol.h"
#include <functional>
#include <vector>
namespace coop::net {
enum class Role : uint8_t {Host,Client};
class Session {
public:
    bool linked=true, sendOK=true;
    Role mode=Role::Host;
    std::vector<PropSpawnPayload> sent;
    std::function<void()> duringSend;
    bool connected() const {return linked;}
    Role role() const {return mode;}
    bool SendPropSpawn(const PropSpawnPayload& p) {
        const bool accepted=sendOK;
        if(accepted)sent.push_back(p);
        auto callback=std::move(duringSend);duringSend={};if(callback)callback();
        return accepted;
    }
};
}
