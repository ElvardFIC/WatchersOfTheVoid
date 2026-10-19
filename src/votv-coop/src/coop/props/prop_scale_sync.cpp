#include "coop/props/prop_scale_sync.h"
#include "coop/props/prop_scale_converge.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/remote_prop.h"
#include "coop/player/local_prop_hold.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "ue_wrap/actors/prop.h"
#include <array>
#include <chrono>
#include <cstring>
#include <map>

namespace coop::prop_scale_sync {
namespace {
using Packet = net::PropScaleStatePayload;
uint64_t Now() { return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count(); }
bool Valid(const Packet& p) {
    return p.key.len && p.key.len <= sizeof(p.key.data) && p.holdGen &&
        !(p.key.len==4 && std::memcmp(p.key.data,"None",4)==0) &&
        std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
        p.x >= .001f && p.y >= .001f && p.z >= .001f && p.x <= 100 && p.y <= 100 && p.z <= 100;
}
struct Pending { Packet packet{}; uint64_t until=0; bool apply=true; uint64_t sequence=0; uint32_t peer=0; };
uint64_t epoch=0, sequence=0;
std::array<Pending,net::kMaxPeers> requests;
std::map<std::wstring,Pending> canonicals;
ue_wrap::CachedObjRef observed;
Packet last{};
uint64_t lastSend=0;
uint32_t world=0;
net::Session* session=nullptr;
void Ensure(net::Session& s) {
    const auto w=ue_wrap::world_identity::Generation();
    if(session!=&s || world!=w) { Reset();session=&s;world=w; }
}
void* Resolve(const Packet& p) {
    void* actor=prop_element_tracker::FindLiveActorByKey(net::StringFromWireKey(p.key));
    return actor && ue_wrap::prop::IsDescendantOfProp(actor) ? actor : nullptr;
}
bool Send(net::Session& s,const Packet& p) {
    return s.role()==net::Role::Host ? s.SendReliable(net::ReliableKind::PropScaleState,&p,sizeof(p))
        : s.SendReliableToSlot(0,net::ReliableKind::PropScaleState,&p,sizeof(p));
}
void Observe(net::Session& s,void* actor,uint16_t generation,bool flush) {
    if(!actor || !ue_wrap::prop::IsDescendantOfProp(actor))return;
    ue_wrap::CachedObjRef identity;identity.Set(actor);
    const auto scale=ue_wrap::engine::GetActorScale3D(actor);
    if(!identity.Is(actor))return;
    Packet p{};p.holdGen=generation;p.elementId=prop_element_tracker::GetPropElementIdForActor(actor);
    net::WireKeyFromString(ue_wrap::prop::GetInteractableKeyString(actor),p.key);
    if(!identity.Is(actor))return;
    p.x=scale.X;p.y=scale.Y;p.z=scale.Z;
    if(!Valid(p))return;
    const bool changed=!observed.Is(actor) || std::memcmp(&p,&last,sizeof(p))!=0;
    const auto now=Now();
    if(!changed && (flush || now-lastSend<1000))return;
    if(!flush && observed.Is(actor) && now-lastSend<50)return;
    const auto before=epoch;
    if(Send(s,p) && epoch==before && world==ue_wrap::world_identity::Generation() &&
        s.connected() && identity.Is(actor)){last=p;lastSend=now;observed=identity;}
}
}
void Reset() {++epoch;requests={};canonicals.clear();observed.Reset();last={};lastSend=0;world=0;session=nullptr;}
void Receive(net::Session& s,const Packet& p,int sender) {
    if(!s.connected() || !Valid(p))return;
    Ensure(s);
    if(s.role()==net::Role::Host) {
        if(sender<=0 || sender>=net::kMaxPeers)return;
        // One latest request per sender; only its currently accepted hold may apply it.
        requests[sender]={p,Now()+2000,true,++sequence,s.peerGenerationForSlot(sender)};
    } else if(sender==0) {
        const auto key=net::StringFromWireKey(p.key);
        if(canonicals.size()>=64 && !canonicals.count(key))return;
        canonicals[key]={p,Now()+30000,true,++sequence,0};
    }
}
void Tick(net::Session& s,void* held,uint16_t generation) {
    Ensure(s);
    if(!s.connected()) {Reset();return;}
    const auto now=Now();
    const auto before=epoch;
    if(s.role()==net::Role::Host) {
        for(int slot=1;slot<net::kMaxPeers;++slot) {
            const auto request=requests[slot];if(!request.until)continue;
            if(now>=request.until || request.peer!=s.peerGenerationForSlot(slot)) {requests[slot]={};continue;}
            void* actor=Resolve(request.packet);
            if(!actor)continue;
            if(request.apply) {
                if(remote_prop::DrivenActor(slot)!=actor ||
                    remote_prop::DrivenHoldGeneration(slot)!=request.packet.holdGen)continue;
                if(local_prop_hold::IsHeld(actor)){requests[slot]={};continue;}
                if(!props::ConvergePropScale(actor,{request.packet.x,request.packet.y,request.packet.z}))continue;
                if(epoch!=before || world!=ue_wrap::world_identity::Generation() || !s.connected())return;
                if(requests[slot].sequence!=request.sequence)continue;
                requests[slot].apply=false;
            }
            const bool sent=Send(s,request.packet);
            if(epoch!=before || world!=ue_wrap::world_identity::Generation() || !s.connected())return;
            if(sent && requests[slot].sequence==request.sequence)requests[slot]={};
        }
    } else {
        const auto pending=canonicals;
        for(const auto& [key,row]:pending) {
            bool done=now>=row.until;
            if(!done) {
                void* actor=Resolve(row.packet);
                if(!actor)continue;
                const auto p=row.packet;
                done=local_prop_hold::IsHeld(actor) || props::ConvergePropScale(actor,{p.x,p.y,p.z});
            }
            if(epoch!=before || world!=ue_wrap::world_identity::Generation() || !s.connected())return;
            const auto current=canonicals.find(key);
            if(done && current!=canonicals.end() && current->second.sequence==row.sequence)canonicals.erase(current);
        }
    }
    if(observed.Get() && observed.Get()!=held) {
        Observe(s,observed.Get(),last.holdGen,true); // last scale goes before reliable release
        if(epoch!=before || world!=ue_wrap::world_identity::Generation() || !s.connected())return;
        observed.Reset();last={};
    }
    Observe(s,held,generation,false);
}
}
