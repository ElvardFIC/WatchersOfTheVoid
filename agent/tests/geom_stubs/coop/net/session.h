#pragma once
#include "coop/net/protocol.h"
#include <vector>
#include <array>
#include <functional>
namespace coop::net {
inline constexpr int kMaxPeers=32;
enum class Role { Host, Client };
class Session {
public:
    Role mode=Role::Host;
    bool active=true, linked=true, ready=true, sendOk=true;
    bool birthOk=true;
    uint64_t stops=0;
    std::array<uint64_t,32> worldRevisions{};
    void MarkSlotWorldReady(int slot,bool value){ready=value;if(!value)++worldRevisions.at(slot);}
    uint64_t SlotWorldReadyRevision(int slot)const{return worldRevisions.at(slot);}
    std::array<uint32_t,32> generations=[] {std::array<uint32_t,32> value{};value.fill(1);return value;}();
    std::array<bool,32> online=[] {std::array<bool,32> value{};value[0]=value[31]=true;return value;}();
    std::vector<int> slots;
    std::vector<GeomOctaEventPayload> sent;
    std::vector<EntitySpawnPayload> births;
    std::vector<int> birthSlots,wireSlots;
    std::vector<ReliableKind> wireKinds;
    std::function<void()> sendHook;
    std::function<bool(int)> allowSlot;
    Role role()const{return mode;}
    bool running()const{return active;}
    bool connected()const{return linked;}
    uint64_t StopCount()const{return stops;}
    uint32_t peerGenerationForSlot(int slot)const{return generations.at(slot);}
    bool IsSlotConnected(int slot)const{return online.at(slot);}
    bool IsSlotWorldReady(int slot)const{return ready&&online.at(slot);}
    bool AnyWorldReadyPeer()const{return ready;}
    bool SendReliable(ReliableKind kind,const void* data,size_t){
        const bool accepted=sendOk&&(kind!=ReliableKind::EntitySpawn||birthOk);
        if(accepted){if(kind==ReliableKind::EntitySpawn)births.push_back(*static_cast<const EntitySpawnPayload*>(data));
            else sent.push_back(*static_cast<const GeomOctaEventPayload*>(data));}
        if(sendHook)sendHook();
        return accepted;
    }
    bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,size_t n){
        if(allowSlot&&!allowSlot(slot))return false;
        if(sendOk&&(kind!=ReliableKind::EntitySpawn||birthOk)){
            if(kind==ReliableKind::EntitySpawn)birthSlots.push_back(slot);else slots.push_back(slot);
            wireSlots.push_back(slot);wireKinds.push_back(kind);
        }
        return SendReliable(kind,data,n);
    }
};
}
