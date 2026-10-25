#pragma once
#include "coop/net/protocol.h"
#include <array>
#include <functional>
#include <vector>
namespace coop::net {
inline constexpr int kMaxPeers=kPeerCapacity;
enum class Role { Host, Client };
class Session {
public:
    Role mode=Role::Host;
    bool started=true, linked=true;
    int blocked=-1;
    std::array<bool,kMaxPeers> peers{},ready{};
    std::array<uint32_t,kMaxPeers> generations{};
    struct Sent {int slot;PlayerBloodPayload packet;};
    std::vector<Sent> sent;
    std::vector<AtvStatePayload> atvStates;
    std::function<void()> callback;
    Session(){peers.fill(true);ready.fill(true);generations.fill(1);}
    bool running()const{return started;}
    bool connected()const{return linked;}
    Role role()const{return mode;}
    bool HasPeerConn(int s)const{return peers[s];}
    bool IsSlotWorldReady(int s)const{return ready[s];}
    uint32_t peerGenerationForSlot(int s)const{return generations[s];}
    bool SendReliableToSlot(int s,ReliableKind kind,const void* raw,size_t size){
        if(kind==ReliableKind::AtvSetComplete)return s!=blocked;
        if(kind==ReliableKind::AtvState && size==sizeof(AtvStatePayload)){
            if(s==blocked)return false;atvStates.push_back(*static_cast<const AtvStatePayload*>(raw));return true;
        }
        if(kind!=ReliableKind::PlayerBlood || size!=sizeof(PlayerBloodPayload) || s==blocked)return false;
        sent.push_back({s,*static_cast<const PlayerBloodPayload*>(raw)});
        auto fn=std::move(callback);callback={};if(fn)fn();return true;
    }
};
}
