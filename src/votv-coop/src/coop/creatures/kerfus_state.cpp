// Kerfus state publication, recipient debt and guarded mirror refresh.
#include "coop/creatures/kerfus_state.h"
#include "coop/element/bind_wait.h"
#include "coop/element/registry.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/props/active_drive.h"
#include "coop/props/prop_drive_host.h"
#include "ue_wrap/actors/kerfus.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/facts/function_row.h"
#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

namespace coop::kerfus_state {
namespace {
namespace EL=coop::element;namespace R=ue_wrap::reflection;namespace sg=ue_wrap::script_gate;
namespace KF=coop::net::kerfus_state_flags;namespace UK=ue_wrap::kerfus;namespace N=coop::net;
using Packet=N::KerfusStatePayload;
UE_FACT_FN(kRowReceiveTick, "kerfus_state", L"p_kerfus_C", L"ReceiveTick", Declared);
constexpr int kTagTick=0x4B465401;
constexpr float kEnergyStep=.5f;constexpr uint64_t kEnergyPeriodMs=1000,kRetryMs=250;
constexpr uint16_t kStateKind=static_cast<uint16_t>(N::ReliableKind::KerfusState);
constexpr size_t kAttempts=16,kScans=64,kClientCapacity=512;
std::atomic<N::Session*> g_session{nullptr};
bool g_watched=false,g_saidLive=false,g_sampling=false;
uint64_t g_hostEpoch=1,g_hostRevision=0,g_publication=0;
struct Scope {
    N::Session* session=nullptr;uint32_t world=0,serial=0;void* worldObject=nullptr;
    uint64_t stops=0,epoch=0;N::Role role=N::Role::Host;bool linked=false;
    bool Current()const {
        return epoch==g_hostEpoch&&session&&session==g_session.load(std::memory_order_acquire)&&
            world==ue_wrap::world_identity::Generation()&&serial==N::session_serial::Current()&&
            worldObject&&worldObject==ue_wrap::world_identity::CurrentWorld()&&session->StopCount()==stops&&
            session->running()&&session->role()==role&&session->connected()==linked;
    }
};
std::optional<Scope> g_bound;
struct HostRec {
    int32_t idx=-1;EL::ElementId eid=EL::kInvalidId;
    bool sent=false;uint8_t flags=0;float energy=0;uint64_t sentMs=0,revision=0,publication=0;
    ue_wrap::CachedObjRef actor,cls;coop::prop_drive_host::ClaimLease lease;
    EL::Registry::IdentitySnapshot identity;
};
struct Delivery {
    Scope scope;HostRec owner;int slot=0;uint32_t generation=0;uint64_t ready=0;
    bool attempted=false;uint64_t at=0;
};
struct ApplyRec {
    Scope scope;Packet packet{};EL::Registry::IdentitySnapshot identity;
    ue_wrap::CachedObjRef actor,cls;
    bool checked=false,read=false,wroteActive=false,wroteCharging=false,wroteEnergy=false;
    bool needsRefresh=false,updDispatched=false,done=false,attempted=false;uint64_t at=0;
};
std::unordered_map<void*,HostRec> g_host;
std::map<uint64_t,std::shared_ptr<Delivery>> g_send;
std::map<uint32_t,std::shared_ptr<ApplyRec>> g_apply;
std::set<uint32_t> g_wrongClassSaid;
std::array<uint32_t,N::kMaxPeers> g_peer{};
std::array<uint64_t,N::kMaxPeers> g_ready{};
std::array<bool,N::kMaxPeers> g_wasReady{};
uint64_t g_sendCursor=0;uint32_t g_applyCursor=0;
struct Busy {Busy(){g_sampling=true;}~Busy(){g_sampling=false;}};
bool Id(uint32_t id){return id>0&&id<EL::kHostRangeSize;}
bool Valid(const Packet& p){return Id(p.elementId)&&std::isfinite(p.energy)&&!(p.flags&~(KF::kActive|KF::kCharging));}
bool Same(const Packet& a,const Packet& b){return a.elementId==b.elementId&&a.flags==b.flags&&a.energy==b.energy;}
bool Paced(bool attempted,uint64_t at,uint64_t now){return attempted&&now>=at&&now-at<kRetryMs;}
void Clear(){++g_hostEpoch;g_host.clear();g_send.clear();g_apply.clear();g_wrongClassSaid.clear();g_bound.reset();g_peer.fill(0);g_ready.fill(0);g_wasReady.fill(false);g_sendCursor=0;g_applyCursor=0;}
void Bind(){
    auto* s=g_session.load(std::memory_order_acquire);
    const auto world=ue_wrap::world_identity::Generation(),serial=N::session_serial::Current();
    void* object=ue_wrap::world_identity::CurrentWorld();
    if(g_bound&&(!s||g_bound->session!=s||g_bound->world!=world||g_bound->serial!=serial||g_bound->worldObject!=object||
        g_bound->stops!=s->StopCount()||g_bound->role!=s->role()||g_bound->linked!=s->connected()||!s->running()))Clear();
    if(!g_bound&&s&&s->running()&&object)g_bound=Scope{s,world,serial,object,s->StopCount(),g_hostEpoch,s->role(),s->connected()};
}
bool Identity(const EL::Registry::IdentitySnapshot& was,bool mirror,EL::Registry::IdentitySnapshot& now){
    return EL::Registry::Get().ReadIdentity(was.id,now)&&now.incarnation==was.incarnation&&
        now.type==EL::ElementType::Prop&&now.mirror==mirror&&!now.deleting&&now.typeName==was.typeName;
}
bool Actor(const ue_wrap::CachedObjRef& actor,const ue_wrap::CachedObjRef& cls){return actor.Get()&&cls.Get()&&R::ClassOf(actor.Raw())==cls.Raw();}
bool Bound(const HostRec& row){
    EL::Registry::IdentitySnapshot now;
    return Id(row.eid)&&Identity(row.identity,false,now)&&now.actor==row.actor.Raw()&&now.internalIdx==row.idx&&
        Actor(row.actor,row.cls)&&EL::Registry::Get().EidForActor(now.actor)==row.eid;
}
bool Snapshot(void* k,EL::ElementId eid,Packet& packet,const std::function<bool()>& current={}){
    bool active=false,charging=false;float energy=0;
    if((current&&!current())||!UK::ReadActiveWhile(k,active,current)||(current&&!current())||
        !UK::ReadChargingWhile(k,charging,current)||(current&&!current())||
        !UK::ReadEnergyWhile(k,energy,current)||(current&&!current())||!std::isfinite(energy))return false;
    packet={};packet.elementId=eid;packet.flags=(active?KF::kActive:0)|(charging?KF::kCharging:0);packet.energy=energy;return true;
}
uint64_t SendKey(int slot,uint32_t id){return (uint64_t(slot)<<32)|id;}
bool Owner(const std::shared_ptr<Delivery>& send){
    if(!send->scope.Current()||!Bound(send->owner))return false;
    const auto it=g_host.find(send->owner.actor.Raw());
    return it!=g_host.end()&&it->second.publication==send->owner.publication&&
        it->second.identity.incarnation==send->owner.identity.incarnation;
}
bool Recipient(const std::shared_ptr<Delivery>& send,bool ready){
    auto* s=send->scope.session;
    return send->scope.Current()&&send->scope.linked&&send->generation&&s->IsSlotConnected(send->slot)&&
        s->peerGenerationForSlot(send->slot)==send->generation&&s->SlotWorldReadyRevision(send->slot)==send->ready&&
        (!ready||s->IsSlotWorldReady(send->slot));
}
bool Installed(const std::shared_ptr<Delivery>& send){
    auto it=g_send.find(SendKey(send->slot,send->owner.eid));return it!=g_send.end()&&it->second==send&&Owner(send);
}
void Queue(const HostRec& row,int slot,const Scope& scope){
    if(slot<=0||slot>=N::kMaxPeers||!scope.Current()||scope.role!=N::Role::Host||!scope.linked||!Bound(row))return;
    auto* s=scope.session;const auto gen=s->peerGenerationForSlot(slot);
    if(!gen||!s->IsSlotConnected(slot))return;
    auto send=std::make_shared<Delivery>();send->scope=scope;send->owner=row;send->slot=slot;send->generation=gen;
    send->ready=s->SlotWorldReadyRevision(slot);g_send[SendKey(slot,row.eid)]=std::move(send);
}
void QueueSlot(int slot,const Scope& scope){
    std::vector<HostRec> rows;for(const auto& [actor,row]:g_host)rows.push_back(row);
    for(const auto& row:rows){if(!scope.Current())return;Queue(row,slot,scope);}
}
void SyncPeers(const Scope& scope){
    if(!scope.linked)return;
    for(int slot=1;slot<N::kMaxPeers;++slot){
        auto* s=scope.session;const auto gen=s->IsSlotConnected(slot)?s->peerGenerationForSlot(slot):0;
        const auto ready=s->SlotWorldReadyRevision(slot);const bool up=s->IsSlotWorldReady(slot);
        const bool changed=gen!=g_peer[slot]||ready!=g_ready[slot]||(up&&!g_wasReady[slot]);
        g_peer[slot]=gen;g_ready[slot]=ready;g_wasReady[slot]=up;
        if(gen&&changed)QueueSlot(slot,scope);
        if(!scope.Current())return;
    }
}
void OnTickPost(const sg::Call& call){
    if(g_sampling)return;Bind();if(!g_bound||g_bound->role!=N::Role::Host||!g_bound->Current()||!call.object)return;
    Busy busy;const Scope scope=*g_bound;void* k=call.object;HostRec r;
    if(auto it=g_host.find(k);it!=g_host.end())r=it->second;
    if(!r.actor.Is(k)||!r.cls.Get()||R::ClassOf(k)!=r.cls.Raw()){
        r={};r.actor.Set(k);if(!r.actor.Get())return;r.idx=r.actor.Idx();r.cls.Set(R::ClassOf(k));
    }
    if(!scope.Current()||!Actor(r.actor,r.cls))return;
    r.revision=++g_hostRevision;
    const auto eid=EL::Registry::Get().EidForActor(k);EL::Registry::IdentitySnapshot identity;
    if(Id(eid)&&EL::Registry::Get().ReadIdentity(eid,identity)&&identity.type==EL::ElementType::Prop&&!identity.mirror&&!identity.deleting){
        if(r.eid!=eid||r.identity.incarnation!=identity.incarnation){r.sent=false;r.publication=0;}
        r.eid=eid;r.identity=identity;
    }else{r.eid=EL::kInvalidId;r.identity={};}
    g_host[k]=r;
    const auto current=[&]{
        if(!scope.Current()||!Actor(r.actor,r.cls)||(Id(r.eid)&&!Bound(r)))return false;
        const auto it=g_host.find(k);return it!=g_host.end()&&it->second.revision==r.revision;
    };
    Packet now{};if(!Snapshot(k,r.eid,now,current))return;
    if(Id(r.eid)){
        if(now.flags&KF::kActive){
            auto lease=coop::prop_drive_host::ClaimOwned(k,coop::prop_drive_host::ClaimSource::Kerfus,"kerfus",current);
            if(!current())return;if(lease)r.lease=lease;
        }else if(r.lease){
            const bool released=coop::prop_drive_host::ReleaseOwned(r.lease,current);if(!current())return;
            if(released||!coop::prop_drive_host::OwnsClaim(r.lease))r.lease={};
        }
    }
    if(!current())return;g_host[k]=r;
    const auto ms=coop::active_drive::NowMs();if(!current())return;
    const bool edge=!r.sent||now.flags!=r.flags;
    const bool step=std::fabs(now.energy-r.energy)>=kEnergyStep&&(ms<r.sentMs||ms-r.sentMs>=kEnergyPeriodMs);
    if((!edge&&!step)||!Valid(now))return;
    r.sent=true;r.flags=now.flags;r.energy=now.energy;r.sentMs=ms;r.publication=++g_publication;g_host[k]=r;
    for(int slot=1;slot<N::kMaxPeers;++slot){if(!current())return;Queue(r,slot,scope);}
}
void PumpSends(const Scope& scope){
    SyncPeers(scope);if(!scope.Current()||g_send.empty())return;
    std::vector<std::shared_ptr<Delivery>> rows;rows.reserve(kScans);
    {
    auto it=g_send.upper_bound(g_sendCursor);if(it==g_send.end())it=g_send.begin();
    const auto count=std::min(kScans,g_send.size());
    for(size_t i=0;i<count;++i){rows.push_back(it->second);if(++it==g_send.end())it=g_send.begin();}
    }
    size_t attempts=0;
    for(const auto& send:rows){
        if(!scope.Current())return;g_sendCursor=SendKey(send->slot,send->owner.eid);
        if(!Installed(send)||!Recipient(send,false)){
            const auto key=SendKey(send->slot,send->owner.eid);auto old=g_send.find(key);if(old!=g_send.end()&&old->second==send)g_send.erase(old);continue;
        }
        const auto now=coop::active_drive::NowMs();if(!scope.Current())return;
        if(!Recipient(send,true)||Paced(send->attempted,send->at,now))continue;
        if(attempts++>=kAttempts)break;send->attempted=true;send->at=now;
        const auto current=[&]{return Installed(send)&&Recipient(send,true);};Packet packet{};
        if(!Snapshot(send->owner.actor.Raw(),send->owner.eid,packet,current)||!current()||!Valid(packet))continue;
        const bool accepted=scope.session->SendReliableToSlot(send->slot,N::ReliableKind::KerfusState,&packet,sizeof packet);
        if(!current()||!accepted)continue;
        g_send.erase(SendKey(send->slot,send->owner.eid));
    }
}
bool Installed(const std::shared_ptr<ApplyRec>& row){auto it=g_apply.find(row->packet.elementId);return it!=g_apply.end()&&it->second==row&&row->scope.Current();}
bool Bound(const std::shared_ptr<ApplyRec>& row){
    if(!Installed(row))return false;EL::Registry::IdentitySnapshot now;
    return Identity(row->identity,true,now)&&now.actor==row->actor.Raw()&&Actor(row->actor,row->cls)&&
        now.internalIdx==row->actor.Idx()&&EL::Registry::Get().EidForActor(now.actor)==row->packet.elementId;
}
void Finish(const std::shared_ptr<ApplyRec>& row){if(Installed(row))row->done=true;}
void Attempt(const std::shared_ptr<ApplyRec>& row){
    if(!Installed(row)||row->done)return;EL::Registry::IdentitySnapshot identity;
    if(!Identity(row->identity,true,identity)){Finish(row);return;}
    if(!row->actor.Raw()){
        if(!identity.actor)return;
        if(!R::IsLiveByIndex(identity.actor,identity.internalIdx)){Finish(row);return;}
        row->actor.Set(identity.actor);row->cls.Set(R::ClassOf(identity.actor));
    }
    if(!Bound(row)){Finish(row);return;}
    const auto current=[&]{return Bound(row)&&!row->done;};void* actor=row->actor.Raw();
    if(!row->checked){const bool kind=UK::IsKerfusWhile(actor,current);if(!current())return;
        if(!kind){g_wrongClassSaid.insert(row->packet.elementId);return;}row->checked=true;}
    if(!row->read){bool active=false;if(!UK::ReadActiveWhile(actor,active,current)||!current())return;
        row->needsRefresh=row->needsRefresh||(active!=bool(row->packet.flags&KF::kActive));row->read=true;}
    if(!row->wroteActive){const bool okay=UK::WriteActiveWhile(actor,(row->packet.flags&KF::kActive)!=0,current,&row->wroteActive);if(!current()||!okay)return;}
    if(!row->wroteCharging){const bool okay=UK::WriteChargingWhile(actor,(row->packet.flags&KF::kCharging)!=0,current);if(!current()||!okay)return;row->wroteCharging=true;}
    if(!row->wroteEnergy){const bool okay=UK::WriteEnergyWhile(actor,row->packet.energy,current);if(!current()||!okay)return;row->wroteEnergy=true;}
    if(row->needsRefresh&&!row->updDispatched){
        (void)UK::RunUpdWhile(actor,false,current,&row->updDispatched);
        if(!current()||!row->updDispatched)return;
    }
    Finish(row);
}
void PumpApply(const Scope& scope){
    if(g_apply.empty())return;
    std::vector<std::shared_ptr<ApplyRec>> rows;rows.reserve(kScans);
    {
    auto it=g_apply.upper_bound(g_applyCursor);if(it==g_apply.end())it=g_apply.begin();
    const auto count=std::min(kScans,g_apply.size());
    for(size_t i=0;i<count;++i){rows.push_back(it->second);if(++it==g_apply.end())it=g_apply.begin();}
    }
    size_t attempts=0;
    for(const auto& row:rows){
        if(!scope.Current())return;g_applyCursor=row->packet.elementId;
        if(!Installed(row))continue;
        EL::Registry::IdentitySnapshot identity;
        if(!Identity(row->identity,true,identity)){auto found=g_apply.find(row->packet.elementId);if(found!=g_apply.end()&&found->second==row)g_apply.erase(found);continue;}
        if(row->done)continue;
        const auto now=coop::active_drive::NowMs();if(!scope.Current())return;
        if(Paced(row->attempted,row->at,now))continue;
        if(attempts++>=kAttempts)break;row->attempted=true;row->at=now;Attempt(row);
    }
}
void ReplayState(const uint8_t* bytes,size_t n,int){Packet p{};if(n==sizeof p){std::memcpy(&p,bytes,sizeof p);OnState(p);}}
} // namespace

void Install(N::Session* session){g_session.store(session,std::memory_order_release);if(!g_watched)g_watched=sg::WatchName(kRowReceiveTick,kTagTick,nullptr,&OnTickPost);}
void Tick(){
    if(g_sampling)return;Bind();if(!g_bound||!g_bound->Current())return;
    Busy busy;const auto scope=*g_bound;
    if(!g_saidLive&&g_watched){sg::ResolvePendingNames();if(!scope.Current())return;if(sg::NameWatchLive(kRowReceiveTick,kTagTick))g_saidLive=true;if(!scope.Current())return;}
    if(scope.role==N::Role::Host)PumpSends(scope);else if(scope.linked)PumpApply(scope);
}
void OnPeerWorldReady(int slot){if(slot<=0||slot>=N::kMaxPeers)return;Bind();if(g_bound&&g_bound->Current()&&g_bound->role==N::Role::Host)QueueSlot(slot,*g_bound);}
void OnState(const Packet& payload){
    Bind();if(!g_bound||!g_bound->Current()||g_bound->role!=N::Role::Client||!g_bound->linked||!Valid(payload))return;
    const auto scope=*g_bound;Packet packet{};packet.elementId=payload.elementId;packet.flags=payload.flags;packet.energy=payload.energy;
    EL::Registry::IdentitySnapshot identity;
    if(!EL::Registry::Get().ReadIdentity(packet.elementId,identity)){
        if(!EL::bind_wait::Hold(packet.elementId,kStateKind,0,&ReplayState,&packet,sizeof packet,true)&&scope.Current())EL::bind_wait::NoteNeverBound(kStateKind,packet.elementId);
        return;
    }
    if(identity.type!=EL::ElementType::Prop||!identity.mirror||identity.deleting)return;
    std::shared_ptr<ApplyRec> old;if(auto it=g_apply.find(packet.elementId);it!=g_apply.end())old=it->second;
    if(old&&!old->done&&old->identity.incarnation==identity.incarnation&&Same(old->packet,packet)&&
        (!old->actor.Raw()||Bound(old)))return;
    if(!old&&g_apply.size()>=kClientCapacity){
        auto evict=std::find_if(g_apply.begin(),g_apply.end(),[](const auto& item){return item.second->done;});
        if(evict==g_apply.end())return;g_apply.erase(evict);
    }
    auto row=std::make_shared<ApplyRec>();row->scope=scope;row->packet=packet;row->identity=identity;
    if(old&&old->identity.incarnation==identity.incarnation&&Bound(old))
        row->needsRefresh=old->needsRefresh&&old->wroteActive&&!old->updDispatched;
    g_apply[packet.elementId]=std::move(row);
}
bool HostKnows(uint32_t eid){
    if(!g_bound||!g_bound->Current()||g_bound->role!=N::Role::Host)return false;
    std::vector<HostRec> rows;for(const auto& [actor,row]:g_host)rows.push_back(row);
    for(const auto& row:rows)if(row.eid==eid&&Bound(row))return true;
    return false;
}
void OnDisconnect(){Clear();}
} // namespace coop::kerfus_state
