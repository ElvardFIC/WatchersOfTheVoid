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
    bool linked=true;
    std::array<bool,kMaxPeers> peers{},ready{},allowed{};
    std::array<uint32_t,kMaxPeers> generations{};
    struct Sent {int slot;ServerStatePayload state;};
    std::vector<Sent> sent;
    std::function<void()> callback;
    Session(){allowed.fill(true);peers[1]=peers[2]=ready[1]=ready[2]=true;generations[1]=1;generations[2]=2;}
    bool connected()const{return linked;}
    Role role()const{return mode;}
    bool HasPeerConn(int slot)const{return peers[slot];}
    bool IsSlotWorldReady(int slot)const{return ready[slot];}
    uint32_t peerGenerationForSlot(int slot)const{return generations[slot];}
    bool SendReliableToSlot(int slot,ReliableKind kind,const void* data,int len){
        if(!linked||!peers[slot]||!ready[slot]||!allowed[slot])return false;
        if(kind==ReliableKind::ServerState&&len==sizeof(ServerStatePayload))sent.push_back({slot,*static_cast<const ServerStatePayload*>(data)});
        auto fn=std::move(callback);callback={};if(fn)fn();return true;
    }
    bool SendReliable(ReliableKind kind,const void* data,int len){
        bool any=false;for(int slot=1;slot<kMaxPeers;++slot)any=SendReliableToSlot(slot,kind,data,len)||any;return any;
    }
};
}
