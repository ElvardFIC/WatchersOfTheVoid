#include "coop/props/explosion_events.h"
#include "coop/props/explosion_sync.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_echo_suppress.h"
#include "coop/element/intent_authority.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/session/net_pump.h"
#include "coop/session/world_load_episode.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/prop_explosion.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/engine/engine.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <map>
namespace coop::explosion_events {
namespace {
namespace X=ue_wrap::prop_explosion;namespace R=ue_wrap::reflection;namespace E=ue_wrap::engine;
namespace sg=ue_wrap::script_gate;using ue_wrap::CachedObjRef;using P=net::ExplosionEventPayload;
struct Pending{P p{};uint64_t at=0,occupancy=0;uint8_t sender=0,mask=0;};
struct Birth{CachedObjRef actor,source;P identity{};uint64_t at=0;};
struct Occurrence{P p{};CachedObjRef actor;uint64_t at=0;uint32_t ordinals[net::kMaxPeers]{};};
struct Source{uint32_t nativeOrdinal=0,last[net::kMaxPeers]{};std::deque<Occurrence> events;};
struct Death{CachedObjRef actor;int32_t serial=0;P p{};uint64_t at=0;bool sent=false;};
std::atomic<net::Session*> g_session{nullptr};
std::array<Birth,512> g_births{};
std::array<CachedObjRef,512> g_managed{},g_cleanup{};
std::array<Death,256> g_deaths{};
std::map<std::wstring,Source> g_sources;
std::deque<Pending> g_requests,g_broadcast,g_replays;
CachedObjRef g_begin;
uint64_t g_epoch=0,g_retry=0,g_hookRetry=0,g_occupancy[net::kMaxPeers]{},g_rateAt[net::kMaxPeers]{};
uint32_t g_world=0,g_sequence=0,g_revision=0,g_lastBlast=0,g_last[net::kMaxPeers]{};
uint16_t g_rates[net::kMaxPeers]{};bool g_ticking=false;unsigned g_spawning=0;
uint64_t Now(){using namespace std::chrono;return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();}
uint64_t Age(uint64_t a,uint64_t b){return a>=b?a-b:0;}
net::Session* Session(){auto* s=g_session.load();return s&&s->connected()?s:nullptr;}
bool Client(){auto* s=Session();return s&&s->role()==net::Role::Client;}
bool Loading(){return world_load_episode::InEpisode()||world_load_episode::InReconcileWindow();}
bool After(uint32_t a,uint32_t b){return int32_t(a-b)>0;}
uint32_t Next(uint32_t& n){if(!++n)++n;return n;}
void Clear(){++g_epoch;for(auto& n:g_occupancy)++n;g_births={};g_managed={};g_cleanup={};g_deaths={};g_sources.clear();
    g_requests.clear();g_broadcast.clear();g_replays.clear();g_retry=0;g_sequence=g_revision=g_lastBlast=0;
    std::fill(std::begin(g_last),std::end(g_last),0);std::fill(std::begin(g_rates),std::end(g_rates),0);std::fill(std::begin(g_rateAt),std::end(g_rateAt),0);}
void World(){auto w=ue_wrap::world_identity::Generation();if(w!=g_world){Clear();g_world=w;}}
bool Current(uint64_t e){return e==g_epoch&&Session()&&g_world==ue_wrap::world_identity::Generation();}
bool Has(const std::array<CachedObjRef,512>& rows,void* a){for(const auto& r:rows)if(r.Is(a))return true;return false;}
bool Mark(std::array<CachedObjRef,512>& rows,void* a){if(Has(rows,a))return true;for(auto& r:rows)if(!r.Get()){r.Set(a);return true;}return false;}
std::wstring Class(const P& p){return std::wstring(p.sourceClass.data,p.sourceClass.data+p.sourceClass.len);}
bool Text(const char* data,unsigned len,unsigned cap){if(!len||len>cap)return false;for(unsigned i=0;i<len;++i)if(uint8_t(data[i])<33||uint8_t(data[i])>126)return false;return true;}
std::wstring Token(const P& p){return net::StringFromWireKey(p.key)+L"|"+Class(p);}
Source* Row(const P& p){auto key=Token(p);auto it=g_sources.find(key);if(it!=g_sources.end())return &it->second;
    if(g_sources.size()>=8192)return nullptr;return &g_sources[key];}
// Ordinals belong to a peer, never to a shared clock: a late joiner starts at one.
// Match short-lived native echoes only once per peer, retaining separate occurrences
// for a recurring source. A permanently increasing shared ordinal loses later blasts.
bool SameBlast(const P& a,const P& b){const float dx=a.x-b.x,dy=a.y-b.y,dz=a.z-b.z;
    return a.variant==b.variant&&a.radius==b.radius&&a.damage==b.damage&&a.fireStrength==b.fireStrength&&
        a.fireFuel==b.fireFuel&&a.sx==b.sx&&a.sy==b.sy&&a.sz==b.sz&&dx*dx+dy*dy+dz*dz<=500.f*500.f;}
bool Echo(Source& row,const P& p,uint8_t peer){for(auto& e:row.events)
    if(!e.ordinals[peer]&&Age(Now(),e.at)<=2500&&SameBlast(e.p,p)){e.ordinals[peer]=p.ordinal;row.last[peer]=p.ordinal;return true;}
    return false;}
void Remember(Source& row,const P& p,const CachedObjRef& actor,uint8_t peer){
    while(!row.events.empty()&&(row.events.size()>=32||Age(Now(),row.events.front().at)>15000))row.events.pop_front();
    Occurrence e;e.p=p;e.actor=actor;e.at=Now();e.ordinals[peer]=p.ordinal;row.last[peer]=p.ordinal;row.events.push_back(e);}
bool SetClass(P& p,void* a){const auto c=R::ClassNameOf(a);if(c.empty()||c.size()>63)return false;
    p.sourceClass.len=uint8_t(c.size());for(size_t i=0;i<c.size();++i){if(c[i]<33||c[i]>126)return false;p.sourceClass.data[i]=char(c[i]);}return true;}
bool LocalAction(const std::wstring& c){return c==L"mainPlayer_C"||c==L"UI_oven_C"||c==L"ui_serverMinigame_C"||c==L"tool_explode_C";}
X::Blast Decode(const P& p){X::Blast b;b.transform={p.qx,p.qy,p.qz,p.qw,p.x,p.y,p.z,0,p.sx,p.sy,p.sz,0};
    b.radius=p.radius;b.damage=p.damage;b.fireStrength=p.fireStrength;b.fireFuel=p.fireFuel;b.innerShake=p.innerShake;b.outerShake=p.outerShake;
    b.force=p.force;b.debrisForce=p.debrisForce;b.impact={p.ix,p.iy,p.iz};b.debris=p.debris;b.variant=p.variant;b.shake=p.shake!=0;b.ignoreWalls=p.ignoreWalls!=0;b.skipSetting=p.skipSetting!=0;return b;}
void Encode(const X::Blast& b,P& p){const auto& t=b.transform;p.qx=t.RotX;p.qy=t.RotY;p.qz=t.RotZ;p.qw=t.RotW;p.x=t.TX;p.y=t.TY;p.z=t.TZ;p.sx=t.SX;p.sy=t.SY;p.sz=t.SZ;
    p.radius=b.radius;p.damage=b.damage;p.fireStrength=b.fireStrength;p.fireFuel=b.fireFuel;p.innerShake=b.innerShake;p.outerShake=b.outerShake;
    p.force=b.force;p.debrisForce=b.debrisForce;p.ix=b.impact.X;p.iy=b.impact.Y;p.iz=b.impact.Z;p.debris=b.debris;p.variant=b.variant;p.shake=b.shake;p.ignoreWalls=b.ignoreWalls;p.skipSetting=b.skipSetting;}
bool Hooks(){return g_begin.Get()&&sg::ClassNameWatchLive(L"explosion_C",L"ReceiveBeginPlay",0x45590001)&&
    sg::NameWatchLive(L"addDamage",0x45590002)&&sg::NameWatchLive(L"exploded",0x45590003)&&sg::NameWatchLive(L"reachedByExplosion",0x45590004)&&
    sg::NameWatchLive(L"ignite",0x45590005)&&sg::ClassNameWatchLive(L"explosion_C",L"runeCheck",0x45590006);}
void Capture(void*,void* source,void* actor){
    if(!Session()||g_spawning||!X::IsGameplayBlast(actor))return;World();
    Birth b;b.actor.Set(actor);b.at=Now();b.identity.scope=3;
    if(source&&R::IsLive(source)){
        if(hand_item::IsMirrorActor(source)||(R::ClassNameOf(source)==L"mainPlayer_C"&&!players::Registry::Get().IsLocal(source)))return;
        b.source.Set(source);if(!SetClass(b.identity,source))return;
        if(source==hand_item::LocalHandActor()){
            b.identity.scope=1;b.identity.handGeneration=hand_item::Generation(players::Registry::Get().LocalPeerId());
        }else{
            auto key=ue_wrap::prop::GetInteractableKeyString(source);
            if(key!=L"None"&&!key.empty()&&key.size()<=31&&(!Client()||net_pump::IsInAnnouncedWorld(source))){
                net::WireKeyFromString(key,b.identity.key);b.identity.scope=0;
            }else if(LocalAction(Class(b.identity))||(ue_wrap::prop::IsDescendantOfProp(source)&&!prop_echo_suppress::IsMirrorSpawn(source)))b.identity.scope=2;
        }
    }else{const char unknown[]="World";b.identity.sourceClass.len=5;std::memcpy(b.identity.sourceClass.data,unknown,5);}
    for(auto& row:g_births)if(!row.actor.Get()||Age(b.at,row.at)>15000){row=b;return;}
    UE_LOGW("explosion_events: native birth table full");
}
void Resolve(){if(g_begin.Get()||Age(Now(),g_hookRetry)<1000)return;g_hookRetry=Now();
    void* gs=R::FindClassDefaultObject(L"GameplayStatics");void* fn=gs?R::FindDispatchFunctionCached(R::ClassOf(gs),L"BeginDeferredActorSpawnFromClass"):nullptr;
    if(fn&&ue_wrap::ufunction_hook::InstallPostHook(fn,&Capture))g_begin.Set(fn);}
uint8_t Destinations(){auto* s=Session();uint8_t mask=0;if(s)for(int i=1;i<net::kMaxPeers;++i)if(s->IsSlotReady(i))mask|=uint8_t(1u<<i);return mask;}
void Broadcast(P p){p.op=1;p.sequence=Next(g_revision);g_broadcast.push_back({p,Now(),0,0,Destinations()});}
sg::Verdict Cancel(void* a){Mark(g_cleanup,a);E::SetActorHiddenInGame(a,true);return sg::Verdict::Cancel;}
sg::Verdict OnBegin(const sg::Call& c){
    if(!Session()||!X::IsGameplayBlast(c.object)||!Hooks())return sg::Verdict::Run;World();
    if(Has(g_managed,c.object)||explosion_sync::IsReplica(c.object)||explosion_sync::CapturingGascan())return sg::Verdict::Run;
    Birth birth;bool found=false;for(auto& row:g_births)if(row.actor.Is(c.object)){birth=row;row={};found=true;break;}
    if(!found){if(Client())return Cancel(c.object);birth.actor.Set(c.object);birth.identity.scope=3;birth.identity.sourceClass.len=5;std::memcpy(birth.identity.sourceClass.data,"World",5);}
    if(Client()&&(Loading()||birth.identity.scope==3))return Cancel(c.object);
    const auto epoch=g_epoch;X::Blast blast;if(!X::ReadBlast(c.object,blast)||!Current(epoch)){UE_LOGW("explosion_events: cannot read native blast");return sg::Verdict::Run;}
    auto p=birth.identity;Encode(blast,p);p.origin=players::Registry::Get().LocalPeerId();
    if(p.origin>=net::kMaxPeers)return sg::Verdict::Run;
    if(p.scope==0){auto* row=Row(p);if(!row)return sg::Verdict::Run;p.ordinal=Next(row->nativeOrdinal);
        if(!Client()&&Echo(*row,p,0))return Cancel(c.object);
    }else p.ordinal=1;
    if(Client()){
        if(g_requests.size()>=512){UE_LOGW("explosion_events: request queue full; refusing local-only blast");return Cancel(c.object);}
        Death* death=nullptr;if(p.scope==0&&birth.source.Get()){
            for(auto& d:g_deaths)if(!d.actor.Raw()||Age(Now(),d.at)>15000){death=&d;break;}
            if(!death){UE_LOGW("explosion_events: source-destroy table full; refusing local-only blast");return Cancel(c.object);}
        }
        p.sequence=Next(g_sequence);p.op=0;g_requests.push_back({p,Now()});
        if(death){death->actor=birth.source;death->serial=R::SlotSerial(birth.source.Idx());death->p=p;death->at=Now();death->sent=false;}
        return Cancel(c.object);
    }
    if(g_broadcast.size()>=512||!Mark(g_managed,c.object))return sg::Verdict::Run;
    if(p.scope==0)Remember(*Row(p),p,birth.source,0);
    Broadcast(p);return sg::Verdict::Run;
}
sg::Verdict Effect(const sg::Call& c){if(!Session()||!Has(g_managed,c.callerObject))return sg::Verdict::Run;
    if(players::Registry::Get().IsLocal(c.object))return sg::Verdict::Run;
    if(c.object&&R::IsLive(c.object)&&R::ClassNameOf(c.object)==L"mainPlayer_C")return sg::Verdict::Cancel;
    return !Client()||c.object==hand_item::LocalHandActor()?sg::Verdict::Run:sg::Verdict::Cancel;
}
sg::Verdict Rune(const sg::Call& c){return IsReplica(c.object)?sg::Verdict::Cancel:sg::Verdict::Run;}
bool Play(const P& p,bool authority,uint64_t epoch){struct Spawn{Spawn(){++g_spawning;}~Spawn(){--g_spawning;}}spawn;
    return X::ReplayBlast(Decode(p),authority,[&](void* a){return Current(epoch)&&Mark(g_managed,a);},[&]{return Current(epoch);});}
}
bool IsReplica(void* a){return Client()&&Has(g_managed,a);}
bool ConsumesClientDestroy(void* a){if(!Client()||!a)return false;World();
    for(auto& d:g_deaths)if(d.actor.Raw()==a&&Age(Now(),d.at)<=15000&&R::ObjectAt(d.actor.Idx())==a&&R::SlotSerial(d.actor.Idx())==d.serial){
        if(d.sent)return true;if(g_requests.size()>=512)return false;
        auto p=d.p;p.op=2;p.sequence=Next(g_sequence);g_requests.push_back({p,Now()});d.sent=true;return true;
    }return false;
}
void Install(net::Session* s){if(g_session.load()!=s)Clear();g_session.store(s);World();
    sg::WatchClassName(L"explosion_C",L"ReceiveBeginPlay",0x45590001,&OnBegin,nullptr);
    sg::WatchName(L"addDamage",0x45590002,&Effect,nullptr);sg::WatchName(L"exploded",0x45590003,&Effect,nullptr);
    sg::WatchName(L"reachedByExplosion",0x45590004,&Effect,nullptr);sg::WatchName(L"ignite",0x45590005,&Effect,nullptr);
    sg::WatchClassName(L"explosion_C",L"runeCheck",0x45590006,&Rune,nullptr);
}
void OnDisconnect(){g_session.store(nullptr);Clear();g_world=0;}
void OnPeerLeft(int slot){if(slot<=0||slot>=net::kMaxPeers)return;++g_occupancy[slot];g_last[slot]=0;g_rates[slot]=0;g_rateAt[slot]=0;
    for(auto& [key,row]:g_sources){row.last[slot]=0;for(auto& e:row.events)e.ordinals[slot]=UINT32_MAX;}
    std::erase_if(g_requests,[&](const Pending& p){return p.sender==slot;});for(auto& p:g_broadcast)p.mask&=uint8_t(~(1u<<slot));}
void Receive(const P& p,uint8_t sender){auto* s=Session();if(!s||!p.sequence||!p.ordinal||p.reserved||p.scope>3||p.shake>1||p.ignoreWalls>1||p.skipSetting>1||p.origin>=net::kMaxPeers||
    !Text(p.sourceClass.data,p.sourceClass.len,63)||(p.scope==0&&(!Text(p.key.data,p.key.len,31)||net::StringFromWireKey(p.key)==L"None"))||!X::Valid(Decode(p)))return;
    World();const auto now=Now();if(!Client()){
        if(!sender||sender>=net::kMaxPeers||p.origin!=sender||p.scope==3||(p.op!=0&&p.op!=2)||(p.op==2&&p.scope!=0)||
           (g_last[sender]&&!After(p.sequence,g_last[sender]))||g_requests.size()>=512)return;
        if(Age(now,g_rateAt[sender])>=1000){g_rateAt[sender]=now;g_rates[sender]=0;}if(g_rates[sender]>=128)return;
        ++g_rates[sender];g_last[sender]=p.sequence;g_requests.push_back({p,now,g_occupancy[sender],sender});
    }else{if(sender!=0||p.op!=1||(g_lastBlast&&!After(p.sequence,g_lastBlast))||g_replays.size()>=512)return;
        g_lastBlast=p.sequence;g_replays.push_back({p,now});}
}
void Tick(){if(!Session()||g_ticking)return;struct Guard{Guard(){g_ticking=true;}~Guard(){g_ticking=false;}}guard;
    World();const auto epoch=g_epoch,now=Now();auto* s=Session();Resolve();sg::ResolvePendingNames();if(!Current(epoch)||!Hooks())return;
    for(auto& r:g_cleanup){auto saved=r;r.Reset();if(void* a=saved.Get())E::DestroyActor(a);if(!Current(epoch))return;}
    if(Client()){
        if(Loading()){g_requests.clear();g_replays.clear();return;}
        while(!g_requests.empty()){auto p=g_requests.front();if(Age(now,p.at)>10000){g_requests.pop_front();continue;}
            if(!s->SendReliableToSlot(0,net::ReliableKind::ExplosionEvent,&p.p,sizeof(P)))break;g_requests.pop_front();}
        if(g_retry&&Age(now,g_retry)<250)return;
        while(!g_replays.empty()){auto event=g_replays.front();if(Age(now,event.at)>10000){g_replays.pop_front();continue;}
            bool ok=Play(event.p,false,epoch);if(!Current(epoch))return;if(!ok){g_retry=now;break;}g_retry=0;g_replays.pop_front();}
        return;
    }
    const auto count=g_retry&&Age(now,g_retry)<250?size_t(0):std::min<size_t>(32,g_requests.size());
    for(size_t i=0;i<count&&!g_requests.empty();++i){auto req=g_requests.front();g_requests.pop_front();auto p=req.p;
        if(Age(now,req.at)>10000||req.occupancy!=g_occupancy[req.sender])continue;
        auto current=[&]{return Current(epoch)&&req.occupancy==g_occupancy[req.sender];};
        Source* row=nullptr;void* a=nullptr;CachedObjRef identity;
        if(p.scope==0){row=Row(p);if(!row)continue;
            if(p.op==2){for(auto& e:row->events)if(e.ordinals[req.sender]==p.ordinal){
                    if(void* live=e.actor.Get();live&&R::ClassNameOf(live)==Class(p)&&ue_wrap::prop::GetInteractableKeyString(live)==net::StringFromWireKey(p.key)){
                        E::DestroyActor(live);if(!current())return;}break;}
                continue;}
            if(row->last[req.sender]&&!After(p.ordinal,row->last[req.sender]))continue;
            if(Echo(*row,p,req.sender))continue;
            a=prop_element_tracker::FindLiveActorByKey(net::StringFromWireKey(p.key));
            if(!a){g_requests.push_front(req);break;}if(R::ClassNameOf(a)!=Class(p)||hand_item::IsHandAxisActor(a))continue;identity.Set(a);
        }
        auto target=element::IntentTarget::ForClientIntent(*s,req.sender,p.scope==1?2500.f:100000.f);
        const bool body=target.HasBody();if(!current())return;if(!body){g_requests.push_front(req);break;}
        const ue_wrap::FVector pos{p.x,p.y,p.z};const bool reach=bool(target.AuthorizeSegment(pos,pos));if(!current())return;if(!reach)continue;
        if(p.scope==0){if(!identity.Get())continue;ue_wrap::FVector source{};bool located=E::TryGetActorLocation(a,source);if(!current())return;
            const float dx=source.X-p.x,dy=source.Y-p.y,dz=source.Z-p.z;
            if(!located||!identity.Get()||dx*dx+dy*dy+dz*dz>10000.f*10000.f)continue;
        }else if(p.scope==1){
            const auto generation=hand_item::Generation(req.sender);
            if(!p.handGeneration)continue;
            if(!generation||After(p.handGeneration,generation)){g_requests.push_front(req);break;}
            auto next=p.handGeneration;if(!++next)++next;
            const bool held=generation==p.handGeneration&&hand_item::HeldClassIs(req.sender,Class(p).c_str());
            // Empty-hand announce can precede the explosion after the held source
            // destroys itself. Accept that immediate transition, never an old item.
            const bool released=generation==next&&hand_item::LastHeldClassIs(req.sender,Class(p).c_str());
            if(!held&&!released)continue;
        }else if(p.scope==2){if(Class(p).rfind(L"prop_",0)!=0&&!LocalAction(Class(p)))continue;}
        if(g_broadcast.size()>=512){g_requests.push_front(req);break;}
        // Commit before callbacks so a native echo cannot emit a second copy.
        const auto previous=row?row->last[req.sender]:0;if(row)Remember(*row,p,identity,req.sender);
        const bool ok=Play(p,true,epoch);if(!Current(epoch))return;
        if(!ok){if(row){row->last[req.sender]=previous;row->events.pop_back();}
            if(current())g_requests.push_front(req);g_retry=now;break;}
        g_retry=0;
        Broadcast(p);
    }
    uint8_t blocked=0;for(auto& b:g_broadcast){if(Age(now,b.at)>10000){b.mask=0;continue;}
        for(int slot=1;slot<net::kMaxPeers;++slot)if((b.mask&(1u<<slot))&&!(blocked&(1u<<slot))){
            if(s->SendReliableToSlot(slot,net::ReliableKind::ExplosionEvent,&b.p,sizeof(P)))b.mask&=uint8_t(~(1u<<slot));else blocked|=uint8_t(1u<<slot);}}
    std::erase_if(g_broadcast,[](const Pending& p){return !p.mask;});
}
}
