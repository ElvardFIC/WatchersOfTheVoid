#include "coop/props/explosion_sync.h"
#include "coop/props/explosion_prop_motion.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/element/intent_authority.h"
#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/session/net_pump.h"
#include "coop/session/world_load_episode.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/prop_explosion.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/script_gate.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <vector>

namespace coop::explosion_sync {
namespace {
namespace X=ue_wrap::prop_explosion;
namespace R=ue_wrap::reflection;
namespace sg=ue_wrap::script_gate;
using ue_wrap::CachedObjRef;
using net::ExplosionPayload;
struct Pending {ExplosionPayload p{};uint64_t at=0,occupancy=0;uint8_t sender=0;uint32_t mask=0;};
struct Source {CachedObjRef actor,function;std::wstring key;uint64_t epoch=0;int depth=0;};
struct DestroyMark {CachedObjRef actor;uint64_t at=0;int32_t serial=0;};
std::atomic<net::Session*> g_session{nullptr};
std::deque<Pending> g_requests,g_blasts;
std::vector<Source> g_sources;
std::array<CachedObjRef,256> g_managed{},g_consumed{};
std::array<DestroyMark,128> g_destroy{};
uint64_t g_epoch=0,g_replayAt=0,g_occupancy[net::kMaxPeers]{},g_rateAt[net::kMaxPeers]{};
uint32_t g_world=0,g_sequence=0,g_revision=0,g_lastBlast=0,g_last[net::kMaxPeers]{};
uint8_t g_rate[net::kMaxPeers]{};
bool g_ticking=false;
uint64_t Now(){using namespace std::chrono;return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();}
uint64_t Age(uint64_t now,uint64_t then){return now>=then?now-then:0;}
net::Session* Session(){auto* s=g_session.load();return s&&s->connected()?s:nullptr;}
bool Client(){auto* s=Session();return s&&s->role()==net::Role::Client;}
uint32_t Next(uint32_t& n){if(!++n)++n;return n;}
bool After(uint32_t a,uint32_t b){return static_cast<int32_t>(a-b)>0;}
void Clear() {
    ++g_epoch;for(auto& n:g_occupancy)++n;
    g_requests.clear();g_blasts.clear();g_managed={};g_consumed={};g_destroy={};
    std::fill(std::begin(g_last),std::end(g_last),0);std::fill(std::begin(g_rate),std::end(g_rate),0);
    std::fill(std::begin(g_rateAt),std::end(g_rateAt),0);
    g_sequence=g_revision=g_lastBlast=0;g_replayAt=0;
    // Native break frames and the ticking guard are stack-owned. Epoch makes an
    // old frame inert without letting its POST pop a new session's frame.
}
void World(){auto w=ue_wrap::world_identity::Generation();if(w!=g_world){Clear();g_world=w;}}
bool Current(uint64_t epoch){return epoch==g_epoch&&Session()&&g_world==ue_wrap::world_identity::Generation();}
bool HooksLive() {
    return sg::ClassNameWatchLive(L"prop_gascan_C",L"broken",0x45580001)&&
        sg::ClassNameWatchLive(L"prop_gascan_C",L"broken_fire",0x45580002)&&
        sg::ClassNameWatchLive(L"explosion_C",L"ReceiveBeginPlay",0x45580003)&&
        sg::NameWatchLive(L"addDamage",0x45580004)&&sg::NameWatchLive(L"exploded",0x45580005)&&
        sg::NameWatchLive(L"reachedByExplosion",0x45580006)&&sg::NameWatchLive(L"ignite",0x45580007)&&
        sg::ClassNameWatchLive(L"explosion_C",L"runeCheck",0x45580008);
}
bool ValidKey(const net::WireKey& k) {
    if(!k.len||k.len>31)return false;
    for(unsigned i=0;i<k.len;++i)if(uint8_t(k.data[i])<33||uint8_t(k.data[i])>126)return false;
    return net::StringFromWireKey(k)!=L"None";
}
std::wstring Key(void* a) {
    if(!X::IsGascan(a)||hand_item::IsHandAxisActor(a))return {};
    if(Client()&&(!net_pump::IsInAnnouncedWorld(a)||world_load_episode::InEpisode()||world_load_episode::InReconcileWindow()))return {};
    auto key=ue_wrap::prop::GetInteractableKeyString(a);net::WireKey k{};
    if(key.size()>31)return {};net::WireKeyFromString(key,k);return ValidKey(k)?key:std::wstring{};
}
bool Has(const std::array<CachedObjRef,256>& rows,void* a){for(auto& r:rows)if(r.Is(a))return true;return false;}
bool Mark(std::array<CachedObjRef,256>& rows,void* a) {
    if(Has(rows,a))return true;
    for(auto& r:rows)if(!r.Get()){r.Set(a);return true;}
    return false;
}
X::State Decode(const ExplosionPayload& p) {
    return {{p.x,p.y,p.z},p.scale,p.radius,p.damage,p.fireStrength,p.fireFuel,p.innerShake,p.outerShake,p.ignoreWalls!=0,p.skipSetting!=0};
}
ExplosionPayload Encode(const X::State& s,const std::wstring& key) {
    ExplosionPayload p{};net::WireKeyFromString(key,p.key);p.sequence=Next(g_revision);p.op=1;
    p.x=s.position.X;p.y=s.position.Y;p.z=s.position.Z;p.scale=s.scale;p.radius=s.radius;p.damage=s.damage;
    p.fireStrength=s.fireStrength;p.fireFuel=s.fireFuel;p.innerShake=s.innerShake;p.outerShake=s.outerShake;
    p.ignoreWalls=s.ignoreWalls;p.skipSetting=s.skipSetting;return p;
}
sg::Verdict OnBreak(const sg::Call& c) {
    auto* s=Session();if(!s)return sg::Verdict::Run;
    World();
    auto key=Key(c.object);if(key.empty())return sg::Verdict::Run;
    if(s->role()==net::Role::Host) {
        // The host needs the complete capture/effect watch set before it can
        // associate the native BeginPlay blast with this break. Clients do
        // not: this callback itself is the authority boundary and their
        // request can wait in the queue until the remaining watches resolve.
        if(!HooksLive())return sg::Verdict::Run;
        // A cancellation by another watch or an absorbed native fault skips POST.
        // Stale captures never authorize a later unrelated explosion.
        std::erase_if(g_sources,[&](const Source& row){return row.depth>=c.depth||
            !Current(row.epoch)||!sg::IsBodyActive(row.function.Get());});
        if(Has(g_consumed,c.object))return sg::Verdict::Cancel;
        if(g_sources.size()>=64)return sg::Verdict::Run;
        if(!Mark(g_consumed,c.object))return sg::Verdict::Run;
        Source source;source.actor.Set(c.object);source.function.Set(c.function);
        source.key=key;source.epoch=g_epoch;source.depth=c.depth;
        g_sources.push_back(source);return sg::Verdict::Run;
    }
    const auto now=Now();
    for(auto& m:g_destroy)if(m.actor.Is(c.object))return sg::Verdict::Cancel;
    if(g_requests.size()>=128){UE_LOGW("explosion: break request queue full; refusing local-only gascan break");return sg::Verdict::Cancel;}
    DestroyMark* mark=nullptr;
    for(auto& m:g_destroy)if(!m.actor.Raw()||Age(now,m.at)>15000){mark=&m;break;}
    if(!mark){UE_LOGW("explosion: destroy dedupe table full; refusing local-only gascan break");return sg::Verdict::Cancel;}
    mark->actor.Set(c.object);mark->at=now;mark->serial=R::SlotSerial(mark->actor.Idx());
    ExplosionPayload p{};net::WireKeyFromString(key,p.key);p.sequence=Next(g_sequence);p.fire=c.tag==0x45580002;
    g_requests.push_back({p,now,0,0,0});
    return sg::Verdict::Cancel;
}
void AfterBreak(const sg::Call& c) {
    if(!g_sources.empty()&&g_sources.back().actor.Raw()==c.object&&g_sources.back().depth==c.depth)g_sources.pop_back();
}
sg::Verdict OnBegin(const sg::Call& c) {
    auto* s=Session();if(!s||!X::IsExplosion(c.object)||!HooksLive())return sg::Verdict::Run;
    if(s->role()!=net::Role::Host||g_sources.empty())return sg::Verdict::Run;
    const auto source=g_sources.back();
    if(!Current(source.epoch)||!source.actor.Get()||!sg::IsBodyActive(source.function.Get()))return sg::Verdict::Run;
    if(Has(g_managed,c.object))return sg::Verdict::Run;
    X::State state{};
    if(!X::Read(c.object,state)||!Current(source.epoch)){
        UE_LOGW("explosion: native gascan blast could not be captured");return sg::Verdict::Run;
    }
    if(!Mark(g_managed,c.object))return sg::Verdict::Run;
    explosion_prop_motion::ObserveBlast(c.object);
    if(g_blasts.size()>=256){UE_LOGW("explosion: blast queue full");return sg::Verdict::Run;}
    uint32_t mask=0;for(int slot=1;slot<net::kMaxPeers;++slot)if(s->IsSlotReady(slot))mask|=uint32_t(1u<<slot);
    g_blasts.push_back({Encode(state,source.key),Now(),0,0,mask});return sg::Verdict::Run;
}
sg::Verdict OnEffect(const sg::Call& c) {
    if(!Session()||!Has(g_managed,c.callerObject))return sg::Verdict::Run;
    if(!Client()&&c.tag==0x45580006)explosion_prop_motion::Observe(c.object,c.callerObject);
    auto& players=coop::players::Registry::Get();
    if(players.IsLocal(c.object))return sg::Verdict::Run;
    // A mainPlayer other than the possessed one must never touch this machine's
    // shared save health, even if it has not yet entered the puppet registry.
    if(c.object&&R::IsLive(c.object)&&R::ClassNameOf(c.object)==L"mainPlayer_C")return sg::Verdict::Cancel;
    if(!Client())return sg::Verdict::Run;
    return c.object==hand_item::LocalHandActor()?sg::Verdict::Run:sg::Verdict::Cancel;
}
sg::Verdict OnRune(const sg::Call& c) {
    return IsReplica(c.object)?sg::Verdict::Cancel:sg::Verdict::Run;
}
}
bool CapturingGascan(){return !g_sources.empty()&&Current(g_sources.back().epoch)&&
    sg::IsBodyActive(g_sources.back().function.Get());}
bool IsReplica(void* a){return Client()&&Has(g_managed,a);}
bool ConsumesClientDestroy(void* a) {
    if(!Client()||!a)return false;World();
    const auto now=Now();
    for(auto& m:g_destroy)if(m.actor.Raw()==a&&Age(now,m.at)<=15000&&
        R::ObjectAt(m.actor.Idx())==a&&R::SlotSerial(m.actor.Idx())==m.serial) {
        // The destroy already set kill flags. CachedObjRef::Is would reject it;
        // the array slot must still be the captured object (including serial).
        return true;
    }
    return false;
}
void Install(net::Session* s) {
    if(g_session.load()!=s)Clear();g_session.store(s);World();
    sg::WatchClassName(L"prop_gascan_C",L"broken",0x45580001,&OnBreak,&AfterBreak);
    sg::WatchClassName(L"prop_gascan_C",L"broken_fire",0x45580002,&OnBreak,&AfterBreak);
    sg::WatchClassName(L"explosion_C",L"ReceiveBeginPlay",0x45580003,&OnBegin,nullptr);
    sg::WatchName(L"addDamage",0x45580004,&OnEffect,nullptr);
    sg::WatchName(L"exploded",0x45580005,&OnEffect,nullptr);
    sg::WatchName(L"reachedByExplosion",0x45580006,&OnEffect,nullptr);
    sg::WatchName(L"ignite",0x45580007,&OnEffect,nullptr);
    sg::WatchClassName(L"explosion_C",L"runeCheck",0x45580008,&OnRune,nullptr);
}
void OnDisconnect(){g_session.store(nullptr);Clear();g_world=0;}
void OnPeerLeft(int slot) {
    if(slot<=0||slot>=net::kMaxPeers)return;
    ++g_occupancy[slot];g_last[slot]=0;g_rate[slot]=0;g_rateAt[slot]=0;
    std::erase_if(g_requests,[slot](const Pending& p){return p.sender==slot;});
    for(auto& p:g_blasts)p.mask&=uint32_t(~(1u<<slot));
}
void Receive(const ExplosionPayload& p,uint8_t sender) {
    auto* s=Session();if(!s||!p.sequence||!ValidKey(p.key)||p.fire>1||p.ignoreWalls>1||p.skipSetting>1)return;
    World();auto now=Now();
    if(s->role()==net::Role::Host) {
        if(!sender||sender>=net::kMaxPeers||p.op!=0||(g_last[sender]&&!After(p.sequence,g_last[sender])))return;
        if(Age(now,g_rateAt[sender])>=1000){g_rateAt[sender]=now;g_rate[sender]=0;}
        if(g_requests.size()>=128||g_rate[sender]>=16)return;
        ++g_rate[sender];g_last[sender]=p.sequence;
        g_requests.push_back({p,now,g_occupancy[sender],sender,0});
    } else {
        if(sender!=0||p.op!=1||!X::Valid(Decode(p))||(g_lastBlast&&!After(p.sequence,g_lastBlast))||g_blasts.size()>=256)return;
        g_lastBlast=p.sequence;g_blasts.push_back({p,now,0,0,0});
    }
}
void Tick() {
    auto* s=Session();if(!s||g_ticking)return;
    struct Guard{Guard(){g_ticking=true;}~Guard(){g_ticking=false;}}guard;
    World();const auto epoch=g_epoch,now=Now();sg::ResolvePendingNames();if(!Current(epoch)||!HooksLive())return;
    const auto count=g_requests.size();
    for(size_t i=0;i<count&&!g_requests.empty();++i) {
        auto req=g_requests.front();g_requests.pop_front();
        if(Age(now,req.at)>10000){UE_LOGW("explosion: break request expired");continue;}
        if(Client()) {
            if(!s->SendReliableToSlot(0,net::ReliableKind::Explosion,&req.p,sizeof(req.p))){g_requests.push_front(req);break;}
            continue;
        }
        if(req.occupancy!=g_occupancy[req.sender])continue;
        const auto key=net::StringFromWireKey(req.p.key);void* a=prop_element_tracker::FindLiveActorByKey(key);
        if(!a){g_requests.push_back(req);continue;}
        if(Key(a)!=key||Has(g_consumed,a))continue;
        CachedObjRef identity;identity.Set(a);
        const auto valid=[&]{return Current(epoch)&&req.occupancy==g_occupancy[req.sender]&&identity.Get()&&Key(a)==key;};
        auto target=element::IntentTarget::ForClientIntent(*s,req.sender,2500.f);
        const bool body=target.HasBody();if(!valid())return;
        if(!body){g_requests.push_back(req);continue;}
        const bool allowed=bool(target.Authorize(a));if(!valid())return;if(!allowed)continue;
        const bool ran=X::BreakGascan(a,req.p.fire!=0);
        if(!Current(epoch))return;
        if(!valid())continue;
        if(ran)ue_wrap::engine::DestroyActor(a);else g_requests.push_back(req);
        if(!Current(epoch))return;
    }
    if(Client()) {
        if(world_load_episode::InEpisode()||world_load_episode::InReconcileWindow()){
            g_blasts.clear();return;
        }
        if(g_replayAt&&Age(now,g_replayAt)<250)return;
        // Preserve event order across a missing native class/spawn failure. Mark
        // the replica before Finish so even reentrant BeginPlay effects are gated.
        while(!g_blasts.empty()) {
            const auto event=g_blasts.front();
            if(Age(now,event.at)>10000){g_blasts.pop_front();continue;}
            const auto valid=[&]{return Current(epoch)&&!g_blasts.empty()&&g_blasts.front().p.sequence==event.p.sequence;};
            const bool ok=X::Replay(Decode(event.p),[&](void* a){return valid()&&Mark(g_managed,a);},valid);
            if(!Current(epoch))return;
            if(!ok){g_replayAt=now;break;}
            g_replayAt=0;
            g_blasts.pop_front();
        }
    } else {
        uint32_t blocked=0;
        for(auto& blast:g_blasts) {
            if(Age(now,blast.at)>10000){blast.mask=0;continue;}
            for(int slot=1;slot<net::kMaxPeers;++slot)if((blast.mask&(1u<<slot))&&!(blocked&(1u<<slot))) {
                if(s->SendReliableToSlot(slot,net::ReliableKind::Explosion,&blast.p,sizeof(blast.p)))blast.mask&=uint32_t(~(1u<<slot));
                else blocked|=uint32_t(1u<<slot);
            }
        }
        std::erase_if(g_blasts,[](const Pending& p){return !p.mask;});
    }
}
}
