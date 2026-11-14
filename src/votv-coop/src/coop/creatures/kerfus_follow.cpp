// Follow activators own service leases across active-state reads and teardown.
#include "coop/creatures/kerfus_follow.h"
#include "coop/creatures/served_player.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "ue_wrap/actors/kerfus.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include <atomic>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
namespace coop::kerfus_follow {
namespace {
namespace SP=coop::served_player;namespace N=coop::net;namespace R=ue_wrap::reflection;
std::atomic<N::Session*> g_session{nullptr};uint64_t g_epoch=1,g_attempt=0;bool g_ticking=false;
struct Scope {
    N::Session* session=nullptr;uint32_t world=0,serial=0;void* object=nullptr;uint64_t stops=0,epoch=0;bool linked=false;
    bool Current()const{return session&&session==g_session.load(std::memory_order_acquire)&&epoch==g_epoch&&
        world==ue_wrap::world_identity::Generation()&&object&&object==ue_wrap::world_identity::CurrentWorld()&&
        serial==N::session_serial::Current()&&session->StopCount()==stops&&session->running()&&session->role()==N::Role::Host&&session->connected()==linked;}
};
std::optional<Scope> g_bound;
struct Rec {Scope scope;ue_wrap::CachedObjRef actor,cls;SP::Lease lease;uint8_t slot=0;uint32_t generation=0;uint64_t ready=0,attempt=0;bool on=false;};
std::unordered_map<void*,std::shared_ptr<Rec>> g_activator;
std::vector<std::shared_ptr<Rec>> Snapshot(){std::vector<std::shared_ptr<Rec>> rows;for(const auto& [actor,row]:g_activator)rows.push_back(row);return rows;}
bool Current(const std::shared_ptr<Rec>& row){
    if(!row->scope.Current()||!row->actor.Get()||!row->cls.Get())return false;
    if(row->slot){auto* s=row->scope.session;if(!row->scope.linked||row->slot>=N::kMaxPeers||!row->generation||
        !s->IsSlotConnected(row->slot)||s->peerGenerationForSlot(row->slot)!=row->generation||
        s->SlotWorldReadyRevision(row->slot)!=row->ready||!s->IsSlotWorldReady(row->slot))return false;}
    const auto cls=R::ClassOf(row->actor.Raw());return row->scope.Current()&&row->actor.Get()&&row->cls.Get()&&cls==row->cls.Raw();
}
bool Installed(const std::shared_ptr<Rec>& row){
    bool exact=false;{auto it=g_activator.find(row->actor.Raw());exact=it!=g_activator.end()&&it->second==row;}
    if(!exact||!Current(row))return false;
    {auto it=g_activator.find(row->actor.Raw());return it!=g_activator.end()&&it->second==row;}
}
void Drop(const std::shared_ptr<Rec>& row){
    {auto it=g_activator.find(row->actor.Raw());if(it!=g_activator.end()&&it->second==row)g_activator.erase(it);}
    if(row->lease)SP::UnserveOwned(row->lease);
}
void Clear(){++g_epoch;const auto rows=Snapshot();g_activator.clear();g_bound.reset();for(const auto& row:rows)if(row->lease)SP::UnserveOwned(row->lease);}
void Bind(){
    auto* s=g_session.load(std::memory_order_acquire);const auto world=ue_wrap::world_identity::Generation(),serial=N::session_serial::Current();void* object=ue_wrap::world_identity::CurrentWorld();
    if(g_bound&&(!s||g_bound->session!=s||g_bound->world!=world||g_bound->serial!=serial||g_bound->object!=object||
        g_bound->stops!=s->StopCount()||!s->running()||s->role()!=N::Role::Host||g_bound->linked!=s->connected()))Clear();
    if(!g_bound&&s&&s->running()&&s->role()==N::Role::Host&&object)g_bound=Scope{s,world,serial,object,s->StopCount(),g_epoch,s->connected()};
}
}
void Install(N::Session* session){g_session.store(session,std::memory_order_release);Bind();}
void OnToggle(void* actor,uint8_t slot){
    Bind();if(!g_bound||!g_bound->Current()||!actor||slot>=N::kMaxPeers)return;
    auto row=std::make_shared<Rec>();row->scope=*g_bound;row->slot=slot;row->attempt=++g_attempt;
    row->actor.Set(actor);if(!row->actor.Get()||!row->scope.Current())return;row->cls.Set(R::ClassOf(actor));
    if(slot){row->generation=row->scope.session->peerGenerationForSlot(slot);row->ready=row->scope.session->SlotWorldReadyRevision(slot);}
    if(!Current(row))return;
    std::shared_ptr<Rec> previous;{auto it=g_activator.find(actor);if(it!=g_activator.end())previous=it->second;}
    g_activator[actor]=row;
    const auto current=[&]{return Installed(row);};bool active=false;
    if(!ue_wrap::kerfus::ReadActiveWhile(actor,active,current)||!current()){
        bool own=false;{auto it=g_activator.find(actor);own=it!=g_activator.end()&&it->second==row;}
        if(own){g_activator.erase(actor);if(previous&&Current(previous)&&!g_activator.contains(actor))g_activator.emplace(actor,previous);}return;
    }
    if(active){
        g_activator.erase(actor);if(previous&&Current(previous)&&!g_activator.contains(actor))g_activator.emplace(actor,previous);return;
    }
    if(!slot){g_activator.erase(actor);if(previous)Drop(previous);return;}
    auto lease=SP::ServeOwned(actor,slot,SP::kPlayerCharacter|SP::kPlayerPawn,current);
    if(!current()){
        if(lease)SP::UnserveOwned(lease);return;
    }
    if(!lease){g_activator.erase(actor);if(previous&&Current(previous)&&SP::Owns(previous->lease)&&!g_activator.contains(actor))g_activator.emplace(actor,previous);return;}
    row->lease=lease;
}
void Tick(){
    if(g_ticking)return;g_ticking=true;struct Busy{~Busy(){g_ticking=false;}} busy;
    Bind();if(!g_bound||!g_bound->Current())return;
    const auto scope=*g_bound;for(const auto& row:Snapshot()){
        if(!scope.Current())return;
        if(!Installed(row)||!SP::Owns(row->lease)){Drop(row);continue;}
        const auto current=[&]{return Installed(row)&&SP::Owns(row->lease);};bool active=false;
        if(!ue_wrap::kerfus::ReadActiveWhile(row->actor.Raw(),active,current)||!current())continue;
        if(!active)Drop(row);else row->on=true;
    }
}
void OnPeerLeft(uint8_t slot){if(!slot||slot>=N::kMaxPeers)return;for(const auto& row:Snapshot())if(row->slot==slot)Drop(row);}
void OnDisconnect(){g_session.store(nullptr,std::memory_order_release);Clear();}
}
