#pragma once
#include "coop/net/protocol.h"
#include <array>
#include <functional>
#include <vector>
#include <cstring>
namespace coop::net {
inline constexpr int kMaxPeers=4;
enum class Role {Host,Client};
class Session {
public:
    struct ReliableMessage {};
    struct Sent {int slot;KeyedScalarPayload payload;};
    Role mode=Role::Host;bool linked=true;
    std::array<bool,kMaxPeers> ready{true,true,true,true};
    std::vector<Sent> sent;
    std::function<bool(int,const KeyedScalarPayload&)> allow;
    struct SpillSent {int slot;DrinkSpillPayload payload;};std::vector<SpillSent> spills;
    std::function<bool(int,const DrinkSpillPayload&)> allowSpill;
    struct LaunchSent {int slot;DrinkLaunchPayload payload;};std::vector<LaunchSent> launches;
    std::function<bool(int,const DrinkLaunchPayload&)> allowLaunch;
    struct PourSent {int slot;DrinkPourPayload payload;};std::vector<PourSent> pours;
    std::function<bool(int,const DrinkPourPayload&)> allowPour;
    bool connected()const{return linked;}Role role()const{return mode;}
    bool IsSlotWorldReady(int slot)const{return ready.at(slot);}
    bool AnyWorldReadyPeer()const{for(int slot=1;slot<kMaxPeers;++slot)if(ready[slot])return true;return false;}
    bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,size_t bytes){
        if(kind==ReliableKind::DrinkPour && bytes==sizeof(DrinkPourPayload)){
            DrinkPourPayload p{};std::memcpy(&p,data,bytes);const bool accepted=!allowPour || allowPour(slot,p);
            if(linked && accepted)pours.push_back({slot,p});return linked && accepted;}
        if(kind==ReliableKind::DrinkLaunch && bytes==sizeof(DrinkLaunchPayload)){
            DrinkLaunchPayload p{};std::memcpy(&p,data,bytes);const bool accepted=!allowLaunch || allowLaunch(slot,p);
            if(linked && accepted)launches.push_back({slot,p});return linked && accepted;}
        if(kind==ReliableKind::DrinkSpill && bytes==sizeof(DrinkSpillPayload)){
            DrinkSpillPayload p{};std::memcpy(&p,data,bytes);const bool accepted=!allowSpill || allowSpill(slot,p);
            if(linked && accepted)spills.push_back({slot,p});return linked && accepted;}
        if(!linked || (kind!=ReliableKind::GrimeState && kind!=ReliableKind::WindowCleanState) || bytes!=sizeof(KeyedScalarPayload))return false;
        KeyedScalarPayload p{};std::memcpy(&p,data,bytes);
        const bool accepted=!allow || allow(slot,p);if(accepted)sent.push_back({slot,p});return accepted;
    }
    bool SendReliable(ReliableKind kind,const void* data,size_t bytes){return SendReliableToSlot(-1,kind,data,bytes);}
};
}
