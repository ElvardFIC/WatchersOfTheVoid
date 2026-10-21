#include "coop/props/prop_fire_sync.h"
#include "coop/props/drink_spill_sync.h"
#include "coop/element/intent_authority.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/explosion_sync.h"
#include "coop/props/explosion_events.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/prop_fire.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/world_identity.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>

namespace coop::prop_fire_sync {
namespace {
namespace F=ue_wrap::prop_fire;
namespace R=ue_wrap::reflection;
namespace sg=ue_wrap::script_gate;
using ue_wrap::CachedObjRef;
using net::PropFirePayload;
struct Row {
    std::wstring key;CachedObjRef actor;
    PropFirePayload state{};
    uint64_t received=0,sent=0;
    uint8_t seeds=0;
    bool have=false,applied=false,dirty=false;
};
struct Request {PropFirePayload p{};uint8_t sender=0;uint64_t at=0,occupancy=0;};
std::atomic<net::Session*> g_session{nullptr};
net::Role g_role=net::Role::Host;
std::array<uint16_t,net::kMaxPeers> g_people{};
std::array<Row,512> g_rows;
std::deque<Request> g_requests;
uint32_t g_last[net::kMaxPeers]{},g_sequence=0,g_revision=0,g_world=0;
uint64_t g_occupancy[net::kMaxPeers]{},g_rateAt[net::kMaxPeers]{},g_epoch=0,g_poll=0;
uint8_t g_rateCount[net::kMaxPeers]{},g_seeds=0;
bool g_ticking=false;
unsigned g_applying=0;
uint64_t Now(){using namespace std::chrono;return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();}
net::Session* Session(){auto* s=g_session.load();return s&&s->connected()?s:nullptr;}
bool Client(){auto* s=Session();return s&&s->role()==net::Role::Client;}
uint32_t Next(uint32_t& s){if(!++s)++s;return s;}
bool After(uint32_t a,uint32_t b){return static_cast<int32_t>(a-b)>0;}
bool KeyValid(const net::WireKey& k) {
    if(!k.len||k.len>sizeof(k.data))return false;
    for(unsigned i=0;i<k.len;++i)if(uint8_t(k.data[i])<33||uint8_t(k.data[i])>126)return false;
    return net::StringFromWireKey(k)!=L"None";
}
void Clear() {
    ++g_epoch;for(auto& o:g_occupancy)++o;
    std::fill(std::begin(g_last),std::end(g_last),0);
    std::fill(std::begin(g_rateAt),std::end(g_rateAt),0);
    std::fill(std::begin(g_rateCount),std::end(g_rateCount),0);
    g_requests.clear();g_rows={};g_people={};g_sequence=g_revision=0;g_poll=0;g_seeds=0;
    // Stack guards intentionally survive callbacks that clear the session.
}
void World(){auto w=ue_wrap::world_identity::Generation();auto* s=Session();if(w!=g_world || (s && s->role()!=g_role)){Clear();g_world=w;if(s)g_role=s->role();}}
bool Current(uint64_t epoch){auto* s=Session();return epoch==g_epoch&&g_world==ue_wrap::world_identity::Generation()&&s&&s->role()==g_role;}
void Reconcile(){for(int slot=0;slot<net::kMaxPeers;++slot){const auto person=roster_ledger::Get(slot).playerNo;
    if(g_people[slot]!=person){if(g_people[slot])OnPeerLeft(slot);g_people[slot]=person;}}}
bool Eligible(void* a) {
    return (F::IsProp(a)||F::IsGasoline(a))&&!hand_item::IsHandAxisActor(a)&&
        (!Client()||net_pump::IsInAnnouncedWorld(a));
}
std::wstring Key(void* a) {
    if(!Eligible(a))return {};
    auto key=F::IsGasoline(a)?drink_spill_sync::KeyFor(a):ue_wrap::prop::GetInteractableKeyString(a);
    if(key.empty()||key==L"None"||key.size()>31)return {};
    net::WireKey wire{};net::WireKeyFromString(key,wire);
    return KeyValid(wire)?key:std::wstring{};
}
void* Resolve(const std::wstring& key){
    return key.compare(0,3,L"ds_")==0?drink_spill_sync::FindByKey(key):prop_element_tracker::FindLiveActorByKey(key);
}
Row* Find(const std::wstring& key,bool create) {
    for(auto& r:g_rows)if(r.key==key)return &r;
    if(create)for(auto& r:g_rows)if(r.key.empty()){r.key=key;return &r;}
    return nullptr;
}
F::State Decode(const PropFirePayload& p){return {p.burning!=0,p.fuel,p.fuelSpeed};}
struct Applying {Applying(){++g_applying;}~Applying(){--g_applying;}};
void QueueLocal(PropFirePayload p,const std::wstring& key) {
    World();Reconcile();const auto slot=players::Registry::Get().LocalPeerId();
    if(!Client() || !slot || slot>=net::kMaxPeers)return;
    p.playerNo=roster_ledger::Get(slot).playerNo;if(!p.playerNo)return;p.sequence=Next(g_sequence);
    std::erase_if(g_requests,[&](const Request& r){return net::StringFromWireKey(r.p.key)==key;});
    if(g_requests.size()<128)g_requests.push_back({p,slot,Now(),g_occupancy[slot]});
    else UE_LOGW("prop_fire: request queue full");
}
void Retry(const Request& req,bool front=false) {
    // A native callback may already have queued a newer desired state for this
    // item. Never append the failed older ignite behind a newer extinguish.
    const auto key=net::StringFromWireKey(req.p.key);
    for(const auto& pending:g_requests)if(net::StringFromWireKey(pending.p.key)==key)return;
    if(front)g_requests.push_front(req);else g_requests.push_back(req);
}
sg::Verdict OnVerb(const sg::Call& c) {
    World();
    if((explosion_sync::IsReplica(c.callerObject)||explosion_events::IsReplica(c.callerObject))&&Eligible(c.object))return sg::Verdict::Cancel;
    if(!Client()||g_applying||!Eligible(c.object))return sg::Verdict::Run;
    const auto key=Key(c.object);if(key.empty())return F::IsGasoline(c.object)?sg::Verdict::Cancel:sg::Verdict::Run;
    // Spread between world items is rolled by the host. Each peer still runs the
    // native trace against its own player, who is not a prop and is not gated here.
    if(c.callerObject&&F::Owner(c.callerObject))return sg::Verdict::Cancel;
    PropFirePayload p{};net::WireKeyFromString(key,p.key);p.op=c.tag==0x46490001?0:1;
    if(p.op==0) {
        static R::ParamOffset fuel{L"fuel"};const int off=c.function?fuel.Of(c.function):-1;
        const int size=c.function?R::FunctionFrameSize(c.function):0;
        if(!c.locals||off<0||off>size||size-off<int(sizeof(float)))return sg::Verdict::Cancel;
        std::memcpy(&p.fuel,c.locals+off,sizeof(float));
        if(!std::isfinite(p.fuel)||p.fuel<=0)return sg::Verdict::Cancel;
        p.fuel=std::min(p.fuel,60.f);
    }
    // Coalesce unsent requests for one item; later extinguish must win over ignite.
    QueueLocal(p,key);
    return sg::Verdict::Cancel;
}
sg::Verdict OnDamage(const sg::Call& c) {
    // Host burns physical world items down. Client flame traces remain live so
    // touching/holding one still ignites that machine's possessed player.
    return Client()&&Eligible(c.object)&&(F::IsGasoline(c.object)||!Key(c.object).empty())?sg::Verdict::Cancel:sg::Verdict::Run;
}
sg::Verdict OnGasExtinguish(const sg::Call& c){
    if(!Client() || g_applying)return sg::Verdict::Run;
    void* owner=F::Owner(c.object);if(!owner || !F::IsGasoline(owner))return sg::Verdict::Run;
    // The host decides native expiry/underwater state. External extinguishing
    // still requests the canonical outcome, while local player traces stay live.
    const auto key=Key(owner);
    if(c.callerObject!=c.object && !key.empty()){
        PropFirePayload p{};net::WireKeyFromString(key,p.key);p.op=1;QueueLocal(p,key);
    }
    return sg::Verdict::Cancel;
}
sg::Verdict OnIgnited(const sg::Call& c) {
    return Client()&&g_applying&&Eligible(c.object)?sg::Verdict::Cancel:sg::Verdict::Run;
}
void Bind(Row& r,void* a) {
    if(r.actor.Is(a))return;
    r.actor.Set(a);r.applied=false;r.sent=0;
    if(!Client())r.have=false;
}
}
void Install(net::Session* s) {
    if(g_session.load()!=s || (s && s->role()!=g_role))Clear();g_session.store(s);if(s)g_role=s->role();World();
    sg::WatchName(L"ignite",0x46490001,&OnVerb,nullptr);
    sg::WatchName(L"extinguishFire",0x46490002,&OnVerb,nullptr);
    sg::WatchName(L"fireDamage",0x46490003,&OnDamage,nullptr);
    sg::WatchName(L"ignited",0x46490004,&OnIgnited,nullptr);
    sg::WatchClassName(L"comp_flammable_C",L"extinguish",0x46490005,&OnGasExtinguish,nullptr);
}
void OnHandReleased(void* a) {
    if(!Client())return;
    const auto key=Key(a);F::State state{};
    if(key.empty()||!F::Read(a,state)||!state.burning)return;
    PropFirePayload p{};net::WireKeyFromString(key,p.key);p.op=0;p.fuel=state.fuel;
    QueueLocal(p,key); // Existing reach/identity checks and missing-actor retry apply.
}
void QueueConnect(int slot){if(slot>0&&slot<net::kMaxPeers){World();g_seeds|=uint8_t(1u<<slot);g_poll=0;}}
void OnPeerLeft(int slot) {
    if(slot<0||slot>=net::kMaxPeers)return;
    g_people[slot]=0;
    ++g_occupancy[slot];g_last[slot]=0;g_rateAt[slot]=0;g_rateCount[slot]=0;
    g_seeds&=uint8_t(~(1u<<slot));for(auto& r:g_rows)r.seeds&=uint8_t(~(1u<<slot));
    std::erase_if(g_requests,[slot](const Request& r){return r.sender==slot;});
}
void Receive(const PropFirePayload& p,uint8_t sender) {
    auto* s=Session();if(!s||!KeyValid(p.key)||!p.sequence)return;
    World();Reconcile();const auto now=Now();
    if(s->role()==net::Role::Host) {
        if(sender==0||sender>=net::kMaxPeers||p.op>1||!p.playerNo||p.playerNo!=roster_ledger::Get(sender).playerNo||!s->IsSlotWorldReady(sender)||!std::isfinite(p.fuel)||p.fuel<0||p.fuel>60||
            (p.op==0&&p.fuel<=0)||(g_last[sender]&&!After(p.sequence,g_last[sender])))return;
        if(now-g_rateAt[sender]>=1000){g_rateAt[sender]=now;g_rateCount[sender]=0;}
        if(g_rateCount[sender]>=32||g_requests.size()>=128)return;
        ++g_rateCount[sender];g_last[sender]=p.sequence;
        const auto key=net::StringFromWireKey(p.key);
        std::erase_if(g_requests,[&](const Request& r){return net::StringFromWireKey(r.p.key)==key;});
        g_requests.push_back({p,sender,now,g_occupancy[sender]});
    } else {
        if(sender!=0||p.op!=2||p.playerNo||p.burning>1||!F::Valid(Decode(p)))return;
        Row* r=Find(net::StringFromWireKey(p.key),true);if(!r)return;
        if(r->have&&!After(p.sequence,r->state.sequence))return;
        r->state=p;r->have=true;r->applied=false;r->received=now;
    }
}
void Tick() {
    auto* s=Session();if(!s||g_ticking)return;
    struct Guard{Guard(){g_ticking=true;}~Guard(){g_ticking=false;}}guard;
    World();Reconcile();const auto epoch=g_epoch,now=Now();sg::ResolvePendingNames();if(!Current(epoch))return;
    if(!sg::NameWatchLive(L"ignite",0x46490001)||!sg::NameWatchLive(L"extinguishFire",0x46490002)||
        !sg::NameWatchLive(L"fireDamage",0x46490003)||!sg::NameWatchLive(L"ignited",0x46490004)||
        !sg::ClassNameWatchLive(L"comp_flammable_C",L"extinguish",0x46490005))return;
    const size_t count=g_requests.size();
    for(size_t i=0;i<count&&!g_requests.empty();++i) {
        if(!Current(epoch) || Session()!=s)return;
        Request req=g_requests.front();g_requests.pop_front();if(now>=req.at&&now-req.at>5000)continue;
        if(s->role()==net::Role::Client) {
            if(req.sender!=players::Registry::Get().LocalPeerId() || req.p.playerNo!=roster_ledger::Get(req.sender).playerNo || req.occupancy!=g_occupancy[req.sender])continue;
            if(!s->IsSlotWorldReady(0)){g_requests.push_back(req);continue;}
            const bool sent=s->SendReliable(net::ReliableKind::PropFire,&req.p,sizeof(req.p));
            if(!Current(epoch) || Session()!=s)return;
            if(req.p.playerNo!=roster_ledger::Get(req.sender).playerNo || req.occupancy!=g_occupancy[req.sender])continue;
            if(!sent){Retry(req,true);break;}
            continue;
        }
        if(req.occupancy!=g_occupancy[req.sender] || req.p.playerNo!=roster_ledger::Get(req.sender).playerNo || !s->IsSlotWorldReady(req.sender))continue;
        const auto key=net::StringFromWireKey(req.p.key);
        const auto sourceCurrent=[&]{return Current(epoch) && Session()==s && req.occupancy==g_occupancy[req.sender] &&
            req.p.playerNo==roster_ledger::Get(req.sender).playerNo && s->IsSlotWorldReady(req.sender);};
        void* a=Resolve(key);
        if(!Current(epoch) || Session()!=s)return;if(!sourceCurrent())continue;
        if(!a){Retry(req);continue;}
        CachedObjRef actor;actor.Set(a);
        const auto valid=[&]{return sourceCurrent() && actor.Is(a) && Key(a)==key && sourceCurrent() && actor.Is(a) &&
            std::none_of(g_requests.begin(),g_requests.end(),[&](const Request& newer){return net::StringFromWireKey(newer.p.key)==key;});};
        if(!valid())continue;
        auto target=element::IntentTarget::ForClientIntent(*s,req.sender,1200.f);
        bool body=target.HasBody();if(!valid())return;
        if(!body){Retry(req);continue;}
        bool allowed=bool(target.Authorize(a));if(!valid())return;
        if(!allowed)continue;
        Row* r=Find(key,true);if(!r){UE_LOGW("prop_fire: state table full");continue;}
        Bind(*r,a);
        bool ok=false,attempted=false;{Applying apply;ok=req.p.op==0?F::Ignite(a,req.p.fuel,&attempted,valid):F::Extinguish(a,&attempted,valid);}
        if(!Current(epoch) || Session()!=s)return;
        if(!valid())continue;
        // Re-dispatching a failed ignite can add fuel a second time after the
        // first native call committed only part of its work.
        if(ok || attempted){r->sent=0;g_poll=0;}else Retry(req);
    }
    if(s->role()==net::Role::Client) {
        for(auto& r:g_rows)if(r.have) {
            const auto key=r.key;const auto seq=r.state.sequence;
            const auto same=[&]{return Current(epoch) && Session()==s && r.key==key && r.have && r.state.sequence==seq;};
            void* a=r.actor.Get();
            if(!a||Key(a)!=key){if(!same())return;a=Resolve(key);if(!same())return;
                if(!a)continue;CachedObjRef resolved;resolved.Set(a);const auto actual=Key(a);
                if(!same())return;if(!resolved.Get() || actual!=key)continue;Bind(r,a);}
            if(!same())return;
            if(r.applied)continue;
            const CachedObjRef identity=r.actor;
            auto value=Decode(r.state);
            value.fuel=std::max(0.f,value.fuel-value.fuelSpeed*float(now>=r.received?now-r.received:0)/1000.f);
            if(value.fuel<=0)value.burning=false;
            const auto valid=[&]{return same() && identity.Is(a) && r.actor.Is(a) && Key(a)==key && same() && identity.Is(a);};
            bool ok=false;{Applying apply;ok=F::Apply(a,value,valid);}
            if(!Current(epoch))return;
            if(ok&&valid())r.applied=true;
        }
        return;
    }
    if(now<g_poll)return;g_poll=now+250;
    std::vector<CachedObjRef> actors;F::CollectBurning(actors);
    if(!Current(epoch))return;
    for(auto& ref:actors)if(void* a=ref.Get()) {
        const auto key=Key(a);if(key.empty())continue;
        Row* r=Find(key,true);if(r)Bind(*r,a);else UE_LOGW("prop_fire: state table full");
    }
    for(auto& r:g_rows)if(!r.key.empty())r.seeds|=g_seeds;
    g_seeds=0;
    for(auto& r:g_rows)if(!r.key.empty()) {
        if(!Current(epoch) || Session()!=s)return;
        const auto key=r.key;const auto identity=r.actor;
        void* a=r.actor.Get();F::State state{};
        if(a&&Key(a)==key) {if(!F::Read(a,state)){if(!Current(epoch) || Session()!=s)return;continue;}}
        else if(!r.have)continue;
        if(!Current(epoch) || Session()!=s)return;
        if(r.key!=key || (a && !identity.Is(a)))continue;
        const bool changed=!r.have||state.burning!=(r.state.burning!=0)||
            std::fabs(state.fuelSpeed-r.state.fuelSpeed)>.01f||
            state.fuel>r.state.fuel+1.f;
        if(changed||r.dirty||r.seeds||(state.burning&&now-r.sent>=2000)||!r.sent) {
            PropFirePayload p{};net::WireKeyFromString(r.key,p.key);p.op=2;
            p.burning=state.burning;p.fuel=state.fuel;p.fuelSpeed=state.fuelSpeed;
            p.sequence=Next(g_revision);r.state=p;r.have=true;r.dirty=true;
            bool delivered=true;
            for(int slot=1;slot<net::kMaxPeers;++slot){
                const auto occupancy=g_occupancy[slot];const auto person=roster_ledger::Get(slot).playerNo;
                if(!person || !s->IsSlotWorldReady(slot))continue;
                const bool seeded=s->SendReliableToSlot(slot,net::ReliableKind::PropFire,&p,sizeof(p));
                if(!Current(epoch) || Session()!=s)return;
                if(r.key!=key || r.state.sequence!=p.sequence){delivered=false;break;}
                if(seeded && occupancy==g_occupancy[slot] && person==roster_ledger::Get(slot).playerNo)r.seeds&=uint8_t(~(1u<<slot));
                else delivered=false;
            }
            // Broadcast success may mean only one recipient accepted its queue.
            // Retain final off/retirement state until every ready peer accepts.
            r.dirty=!delivered;if(delivered)r.sent=now;
        }
    }
}
void OnDisconnect(){g_session.store(nullptr);Clear();g_world=0;}
}
