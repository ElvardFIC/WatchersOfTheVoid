#include "coop/interactables/kitchen_sync.h"
#include "coop/interactables/grill_hand_sync.h"
#include "coop/interactables/grill_edits.h"
#include "coop/interactables/kitchen_wire.h"
#include "coop/element/intent_authority.h"
#include "coop/element/portable_identity.h"
#include "coop/element/object_scan_hub.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/player/roster_ledger.h"
#include "coop/player/hand_item.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_save_data.h"
#include "coop/props/remote_prop.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/devices/kitchen_machine.h"
#include "ue_wrap/devices/cremator.h"
#include "ue_wrap/engine/world_identity.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <set>
#include <map>

namespace coop::kitchen_sync {
namespace {
namespace K=ue_wrap::kitchen_machine;
namespace R=ue_wrap::reflection;
namespace sg=ue_wrap::script_gate;
std::atomic<net::Session*> g_session{nullptr};
bool g_installed=false;
bool g_ticking=false;
uint32_t g_world=0;
uint64_t g_epoch=0,g_revision=0;
uint64_t g_next=0, g_animationAt=0;
uint64_t g_recoveryAt=0;
std::vector<ue_wrap::CachedObjRef> g_actors, g_scratch;
struct Last { ue_wrap::CachedObjRef actor; net::KitchenStatePayload state{}; uint64_t at=0,revision=0; };
std::unordered_map<std::wstring,Last> g_sent;
struct Pending {net::KitchenStatePayload state{};uint64_t revision=0;};
std::unordered_map<std::wstring,Pending> g_pending;
struct Applied { ue_wrap::CachedObjRef actor;uint64_t revision=0; };
std::unordered_map<std::wstring,Applied> g_applied;
std::unordered_map<std::wstring,uint64_t> g_retryAt;
roster_ledger::PerSlotState<uint64_t> g_intentAt;
struct Motion { ue_wrap::CachedObjRef actor; float x=0,y=0; };
std::unordered_map<std::wstring,Motion> g_motion;
struct Owner { ue_wrap::CachedObjRef actor; uint16_t playerNo=0; uint64_t until=0; };
std::unordered_map<std::wstring,Owner> g_doorOwners;
struct GrillOp {ue_wrap::CachedObjRef actor;net::KitchenIntentPayload p{};uint64_t until=0,retryAt=0,serial=0;uint8_t slot=0;uint16_t playerNo=0;};
std::unordered_map<std::wstring,GrillOp> g_grillOps;
std::unordered_map<std::wstring,GrillOp> g_grillLast;
struct GrillSave {ue_wrap::CachedObjRef actor;uint64_t retryAt=0,serial=0;};
std::unordered_map<std::wstring,GrillSave> g_grillSaves;
uint64_t g_grillSerial=0;
std::set<std::wstring> g_painting;
std::map<int32_t,uint64_t> g_sampling;
struct Join {ue_wrap::CachedObjRef actor;uint32_t pending=0;};
std::map<std::wstring,Join> g_joins;
std::wstring g_joinCursor;
uint16_t g_joinPeople[net::kMaxPeers]{};
struct LidScope {void* object=nullptr;ue_wrap::CachedObjRef actor,player;void* stack=nullptr;int depth=0;uint64_t epoch=0;};
std::vector<LidScope> g_lidScopes;
uint64_t Now() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
bool Client() {
    auto* s=g_session.load();
    return s && s->connected() && s->role()==net::Role::Client;
}
bool Current(net::Session* session,uint64_t epoch) {
    return g_session.load()==session && session && session->connected() && g_epoch==epoch &&
        g_world==ue_wrap::world_identity::Generation();
}
bool IsMachine(void* a) { return K::Type(a)!=K::Kind::None; }
bool SharedMachine(void* a) {
    return IsMachine(a) && !hand_item::IsHandAxisActor(a) && ue_wrap::world_identity::CurrentWorld() &&
        ue_wrap::world_identity::WorldOf(a)==ue_wrap::world_identity::CurrentWorld();
}
void ResetWorld() {
    const uint32_t gen=ue_wrap::world_identity::Generation();
    if (gen==g_world) return;
    ++g_epoch;g_world=gen; g_actors.clear(); g_scratch.clear(); g_sent.clear(); g_pending.clear();g_lidScopes.clear();
    g_applied.clear(); g_retryAt.clear();
    g_next=0; g_animationAt=0; g_recoveryAt=0;
    g_motion.clear(); g_doorOwners.clear();
    g_joins.clear();g_joinCursor.clear();
    for(auto& person:g_joinPeople)person=0;
    g_grillOps.clear();g_grillLast.clear();g_grillSaves.clear();++g_grillSerial;
    for (int slot=0;slot<net::kMaxPeers;++slot) g_intentAt[slot]=0;
}
void Begin(void*,bool full) {
    ResetWorld(); g_scratch.clear();
    if (!full) for (auto& r:g_actors) if (r.Alive()) g_scratch.push_back(r);
}
void Match(void*,void* a) {
    if(!SharedMachine(a))return;
    if (R::NameStartsWith(R::NameOf(a),L"Default__")) return;
    for (auto& r:g_scratch) if (r.Get()==a) return;
    ue_wrap::CachedObjRef r; r.Set(a); g_scratch.push_back(r);
}
size_t Complete(void*,bool,uint32_t gen) {
    g_world=gen; g_actors.swap(g_scratch); return g_actors.size();
}
bool Ready() { return true; }
// Type uses live class names; class-set version makes the hub reconsider previously missing classes.
uint64_t ClassCount() { return ue_wrap::object_index::ClassSetVersion(); }
void Identity(void* a,net::WireKey& key,uint32_t& eid) {
    key={}; eid=0;
    if (K::Type(a)==K::Kind::Cremator) {
        net::WireKeyFromString(element::PortableWireKey(a),key);
        return;
    }
    const auto k=ue_wrap::prop::GetInteractableKeyString(a);
    if (!k.empty() && k!=L"None" && k.size()<=sizeof(key.data)) net::WireKeyFromString(k,key);
    const auto id=prop_element_tracker::GetPropElementIdForActor(a);
    if (id!=element::kInvalidId) eid=id;
    if (!eid) {
        const auto mirror=remote_prop::ResolveMirrorEidByActor(a);
        if (mirror!=element::kInvalidId) eid=mirror;
    }
}
std::wstring Token(const net::WireKey& key,uint32_t eid) {
    return key.len ? net::StringFromWireKey(key) : (L"#"+std::to_wstring(eid));
}
void* Resolve(const net::WireKey& key,uint32_t eid) {
    void* a=nullptr;
    if (key.len) {
        const auto identity=net::StringFromWireKey(key);
        for (void* machine : ue_wrap::cremator::Instances())
            if (element::PortableWireKey(machine)==identity) { a=machine; break; }
    }
    if (!a && key.len) a=prop_element_tracker::FindLiveActorByKey(net::StringFromWireKey(key));
    if (!a && eid) a=remote_prop::ResolveLiveActorByEid(eid);
    if (!a || !SharedMachine(a)) return nullptr;
    return a;
}
bool Valid(const net::KitchenStatePayload& p) {
    if(p.cooking>1 || !std::isfinite(p.cookVolume) || p.cookVolume<0 || p.cookVolume>1 ||
       (p.kind!=5 && (p.cooking || p.cookVolume || (p.flags&K::HasLid))))return false;
    if (p.kind==6 && ((p.flags&~K::Opened)||!p.hasDoorPose)) return false;
    if(p.kind==5 && ((p.flags&~(K::Active|K::HasLid))||p.coals<0||p.coals>16||
        !std::isfinite(p.burn)||p.burn<0||p.burn>=1.01f))return false;
    return kitchen_wire::ValidPose(p) && p.key.len<=sizeof(p.key.data) && (p.key.len || p.elementId) && p.kind>=1 && p.kind<=6 &&
        p.mode>=0 && p.mode<=4 && std::isfinite(p.remaining) &&
        std::isfinite(p.timer) && std::isfinite(p.power) && p.remaining>=0 && p.remaining<=60 &&
        p.timer>=0 && p.timer<=60 && p.power>=0 && p.power<=1.01f;
}
K::State Decode(const net::KitchenStatePayload& p) {
    K::State s{}; s.kind=static_cast<K::Kind>(p.kind); s.flags=p.flags; s.mode=p.mode;
    s.remaining=p.remaining; s.timer=p.timer; s.power=p.power;
    s.coals=p.coals;s.burn=p.burn;s.cooking=p.cooking!=0;s.cookVolume=p.cookVolume;
    s.hasDoorPose=p.hasDoorPose!=0; if (s.hasDoorPose) s.doorPose=kitchen_wire::DecodePose(p);
    return s;
}
bool PaintAndRemember(const std::wstring& id,bool immediate=false) {
    if(g_painting.count(id))return false;
    auto pending=g_pending.find(id);if(pending==g_pending.end())return false;
    const auto row=pending->second;
    auto* session=g_session.load();const auto epoch=g_epoch;
    const auto current=[&]{const auto it=g_pending.find(id);return Current(session,epoch) &&
        it!=g_pending.end() && it->second.revision==row.revision;};
    const uint64_t now=Now();
    auto retry=g_retryAt.find(id);
    if (!immediate && retry!=g_retryAt.end() && now<retry->second) return false;
    struct Guard {std::wstring id;Guard(const std::wstring& value):id(value){g_painting.insert(id);}~Guard(){g_painting.erase(id);}} guard(id);
    void* actor=Resolve(row.state.key,row.state.elementId);
    ue_wrap::CachedObjRef live;live.Set(actor);
    const bool ok=actor && current() && K::Apply(actor,Decode(row.state),current);
    if(!current())return false;
    if (!ok || !live.Get()) {
        g_applied.erase(id);
        g_retryAt[id]=now+250;
        return false;
    }
    g_retryAt.erase(id);
    g_applied[id]={live,row.revision};
    return true;
}
bool Publish(void* a,int slot) {
    auto* s=g_session.load();
    const auto epoch=g_epoch;ue_wrap::CachedObjRef live;live.Set(a);
    if(!s || !SharedMachine(a))return false;
    struct Sample {
        int32_t index;uint64_t ticket;bool entered;
        explicit Sample(int32_t idx):index(idx),ticket(++g_revision) {
            auto [it,added]=g_sampling.emplace(index,ticket);entered=added;
            if(!added)it->second=ticket;
        }
        ~Sample(){if(entered)g_sampling.erase(index);}
        bool Current()const {const auto it=g_sampling.find(index);return entered && it!=g_sampling.end() && it->second==ticket;}
    } sample(live.Idx());
    if(!sample.entered)return false;
    K::State state{};
    if (!K::Read(a,state) || !Current(s,epoch) || !live.Get() || !sample.Current()) return false;
    net::KitchenStatePayload p{};
    Identity(a,p.key,p.elementId);
    if (!p.key.len && !p.elementId) return false;
    p.kind=static_cast<uint8_t>(state.kind); p.flags=state.flags; p.mode=state.mode;
    p.remaining=state.remaining; p.timer=state.timer; p.power=state.power;
    p.coals=state.coals;p.burn=state.burn;p.cooking=uint8_t(state.cooking);p.cookVolume=state.cookVolume;
    kitchen_wire::EncodePose(state,p);
    if (!Valid(p) || !Current(s,epoch) || !live.Get()) return false;
    const auto id=Token(p.key,p.elementId);
    const auto last=g_sent[id];
    const uint64_t now=Now();
    // A heartbeat also repairs join timing and local cord callbacks. A replacement actor on
    // the same save key always gets a fresh baseline, held by slot+serial rather than address.
    if (slot<0 && last.actor.Get()==a && now-last.at<2000 &&
        p.kind==last.state.kind && p.flags==last.state.flags && p.mode==last.state.mode &&
        p.cooking==last.state.cooking && std::fabs(p.cookVolume-last.state.cookVolume)<.002f &&
        p.coals==last.state.coals && std::fabs(p.burn-last.state.burn)<.02f &&
        p.timer==last.state.timer && std::fabs(p.remaining-last.state.remaining)<0.2f &&
        std::fabs(p.power-last.state.power)<0.01f && kitchen_wire::SamePose(p,last.state)) return true;
    const auto revision=++g_revision;g_sent[id].revision=revision;
    const bool ok=slot<0 ? s->SendReliable(net::ReliableKind::KitchenState,&p,sizeof(p))
        : s->SendReliableToSlot(slot,net::ReliableKind::KitchenState,&p,sizeof(p));
    if(!Current(s,epoch) || !live.Get())return false;
    auto found=g_sent.find(id);
    if (ok && slot<0 && found!=g_sent.end() && found->second.revision==revision) {
        if (last.actor.Get()!=a || last.state.flags!=p.flags || last.state.mode!=p.mode)
            UE_LOGI("kitchen: state key='%ls' eid=%u kind=%u flags=%u mode=%d",id.c_str(),p.elementId,p.kind,p.flags,p.mode);
        found->second={live,p,now,revision};
    }
    return ok;
}
void ReconcileRecipients() {
    for(int slot=1;slot<net::kMaxPeers;++slot) {
        const auto person=roster_ledger::Get(slot).playerNo;
        if(person==g_joinPeople[slot])continue;
        for(auto& [id,row]:g_joins)row.pending&=uint32_t(~(1u<<slot));
        g_joinPeople[slot]=person;
    }
}
void RetryJoins() {
    auto* s=g_session.load();const auto epoch=g_epoch;
    const auto count=(std::min)(size_t(8),g_joins.size());
    for(size_t i=0;i<count && !g_joins.empty();++i) {
        auto it=g_joins.upper_bound(g_joinCursor);if(it==g_joins.end())it=g_joins.begin();
        const auto id=it->first;g_joinCursor=id;const auto row=it->second;
        void* actor=row.actor.Get();if(!actor || !SharedMachine(actor)){g_joins.erase(it);continue;}
        uint32_t done=0;
        for(int slot=1;slot<net::kMaxPeers;++slot)if(row.pending&(1u<<slot)) {
            const auto person=roster_ledger::Get(slot).playerNo;
            if(!s->IsSlotWorldReady(slot) || (Publish(actor,slot) && roster_ledger::Get(slot).playerNo==person))done|=uint32_t(1u<<slot);
            if(!Current(s,epoch))return;
        }
        it=g_joins.find(id);
        if(it!=g_joins.end() && it->second.actor.Is(actor)) {
            it->second.pending&=uint32_t(~done);if(!it->second.pending)g_joins.erase(it);
        }
    }
}
bool Grill(void* a) {return K::Type(a)==K::Kind::Grill && !hand_item::IsHandAxisActor(a);}
void QueueGrill(void* actor,uint8_t op,uint8_t slot=0) {
    ResetWorld();net::KitchenIntentPayload p{};Identity(actor,p.key,p.elementId);p.op=op;
    if(!p.key.len&&!p.elementId)return;
    const auto id=Token(p.key,p.elementId);
    if(g_grillOps.size()>=64&&!g_grillOps.count(id))return;
    const auto now=Now();
    auto pending=g_grillOps.find(id);
    if(pending!=g_grillOps.end()&&pending->second.actor.Is(actor)&&pending->second.p.op==op&&pending->second.slot==slot)return;
    if(!slot) {
        auto last=g_grillLast.find(id);
        if(last!=g_grillLast.end()&&last->second.actor.Is(actor)&&last->second.p.op==op&&now<last->second.until)return;
        if(g_grillLast.size()>=64) {
            std::erase_if(g_grillLast,[now](const auto& row){return row.second.until<=now||!row.second.actor.Alive();});
            if(g_grillLast.size()>=64)return;
        }
        auto& lastRow=g_grillLast[id];lastRow.actor.Set(actor);lastRow.p=p;lastRow.until=now+500;
    }
    auto& row=g_grillOps[id];row={};row.actor.Set(actor);row.p=p;row.until=now+5000;
    row.serial=++g_grillSerial;row.slot=slot;row.playerNo=slot?roster_ledger::Get(slot).playerNo:0;
}
void DrainGrills() {
    auto* s=g_session.load();const auto epoch=g_epoch;const auto now=Now();
    std::vector<std::wstring> saves;for(const auto& row:g_grillSaves)saves.push_back(row.first);
    for(const auto& key:saves) {
        auto it=g_grillSaves.find(key);if(it==g_grillSaves.end())continue;
        auto row=it->second;void* a=row.actor.Get();
        if(!a||!Grill(a)||ue_wrap::prop::GetKeyString(a)!=key){g_grillSaves.erase(it);continue;}
        if(now<row.retryAt)continue;it->second.retryAt=now+500;
        const bool sent=prop_save_data::Publish(s,a,key);
        if(!Current(s,epoch))return;
        it=g_grillSaves.find(key);
        if(sent&&it!=g_grillSaves.end()&&it->second.serial==row.serial)g_grillSaves.erase(it);
    }
    std::vector<std::wstring> ids;for(const auto& row:g_grillOps)ids.push_back(row.first);
    for(const auto& id:ids) {
        auto it=g_grillOps.find(id);if(it==g_grillOps.end())continue;
        auto row=it->second;void* a=row.actor.Get();
        if(now>=row.until||!a||!Grill(a)||Resolve(row.p.key,row.p.elementId)!=a) {g_grillOps.erase(it);continue;}
        if(now<row.retryAt)continue;it->second.retryAt=now+200;
        bool done=false;
        if(Client())done=s->SendReliable(net::ReliableKind::KitchenIntent,&row.p,sizeof(row.p));
        else if(!row.slot||!row.playerNo||roster_ledger::Get(row.slot).playerNo!=row.playerNo||
            !element::IntentTarget::ForClientIntent(*s,row.slot,500.f).Authorize(a))done=true;
        else {
            if(!Current(s,epoch) || !row.actor.Get())return;
            // A client's coal edit can arrive after its ignition intent.
            // Wait briefly for the native fuel to land.
            K::State state{};
            if(K::Read(a,state) && (row.p.op==6||state.coals>0)) {
                if(!Current(s,epoch) || !row.actor.Get() || roster_ledger::Get(row.slot).playerNo!=row.playerNo)return;
                done=K::SetGrillFire(a,row.p.op==5);
                if(done&&row.actor.Get()&&Current(s,epoch))Publish(a,-1);
            }
        }
        if(!Current(s,epoch))return;
        it=g_grillOps.find(id);
        if(done&&it!=g_grillOps.end()&&it->second.serial==row.serial)g_grillOps.erase(it);
    }
}
sg::Verdict OnGrillIgnite(const sg::Call& c) {
    auto* session=g_session.load();
    if(!c.fromOurCode && session && session->connected() && K::Type(c.object)==K::Kind::Grill &&
        hand_item::IsMirrorActor(c.object))return sg::Verdict::Cancel;
    if(!Client()||c.fromOurCode||!Grill(c.object)||!net_pump::IsInAnnouncedWorld(c.object))return sg::Verdict::Run;
    K::State state{};if(K::Read(c.object,state)&&(state.flags&K::Active))return sg::Verdict::Cancel;
    QueueGrill(c.object,5);return sg::Verdict::Cancel;
}
sg::Verdict OnGrillCoal(const sg::Call& c) {
    auto* session=g_session.load();
    if(c.fromOurCode || !session || !session->connected())return sg::Verdict::Run;
    if(!hand_item::IsMirrorActor(c.object)){grill_edits::BeforeChange(c,1);return sg::Verdict::Run;}
    const int param=c.function?R::FindParamOffset(c.function,L"added"):-1;
    const int size=c.function?R::FunctionFrameSize(c.function):0;int offset=-1;uint8_t mask=0;
    if(c.locals && param>=0 && R::FindBoolProperty(c.function,L"added",offset,mask) && mask && offset>=param && offset<size) {
        c.locals[offset]&=uint8_t(~mask);if(auto* out=sg::OutParamPtr(c,param))out[offset-param]&=uint8_t(~mask);
    }
    return sg::Verdict::Cancel;
}
sg::Verdict OnGrillLid(const sg::Call& c) {
    auto* session=g_session.load();
    if(!c.fromOurCode && session && session->connected() && hand_item::IsMirrorActor(c.object))return sg::Verdict::Cancel;
    grill_edits::BeforeChange(c,2);return sg::Verdict::Run;
}
sg::Verdict OnGrillAction(const sg::Call& c) {
    auto* session=g_session.load();
    if(!c.fromOurCode && session && session->connected() && K::Type(c.object)==K::Kind::Grill &&
       hand_item::IsMirrorActor(c.object))return sg::Verdict::Cancel;
    if(!Client() || c.fromOurCode || !Grill(c.object) || !net_pump::IsInAnnouncedWorld(c.object) ||
       !c.function || !c.locals || g_lidScopes.size()>=16)return sg::Verdict::Run;
    const int action=R::FindParamOffset(c.function,L"action"),player=R::FindParamOffset(c.function,L"player");
    const int size=R::FunctionFrameSize(c.function);
    if(action<0 || action>=size || c.locals[action]!=7 || player<0 || player>size || sizeof(void*)>size-player)return sg::Verdict::Run;
    void* user=nullptr;std::memcpy(&user,c.locals+player,sizeof(user));
    if(!user || user!=players::Registry::Get().Local())return sg::Verdict::Run;
    ResetWorld();LidScope scope{};scope.object=c.object;scope.actor.Set(c.object);scope.player.Set(user);
    scope.stack=c.stack;scope.depth=c.depth;scope.epoch=g_epoch;g_lidScopes.push_back(scope);
    return sg::Verdict::Run;
}
void AfterGrillChange(const sg::Call& c) {
    grill_edits::AfterChange(c);
    auto* s=g_session.load();if(!s||!s->connected()||!Grill(c.object))return;
    ue_wrap::CachedObjRef live;live.Set(c.object);
    ResetWorld();
    if(!Client()&&!c.fromOurCode && c.tag==0x4B54000A) {
        net::WireKey key{};uint32_t eid=0;Identity(c.object,key,eid);g_grillOps.erase(Token(key,eid));
    }
    if(!Client()&&!c.fromOurCode && c.tag==0x4B54000C) {
        const int off=c.function?R::FindParamOffset(c.function,L"action"):-1;
        if(c.locals&&off>=0&&off<R::FunctionFrameSize(c.function)&&c.locals[off]==4) {
            net::WireKey key{};uint32_t eid=0;Identity(c.object,key,eid);g_grillOps.erase(Token(key,eid));
        }
    }
    if(!Client() && !c.fromOurCode && (c.tag==0x4B54000B || c.tag==0x4B54000D) && net_pump::IsInAnnouncedWorld(c.object)) {
        // The host publishes its local edits. Client edits use the ordered
        // field channel, without sending an unrelated old full record.
        const auto key=ue_wrap::prop::GetKeyString(c.object);
        if(!key.empty()&&key!=L"None"&&key.size()<=31&& (g_grillSaves.size()<64||g_grillSaves.count(key))) {
            // Capture now, while the completed addCoal state is still intact:
            // an older incoming host heartbeat must not overwrite it first.
            auto& row=g_grillSaves[key];row.actor.Set(c.object);row.retryAt=Now()+500;row.serial=++g_grillSerial;
            const auto serial=row.serial;const auto world=g_world;
            const bool sent=prop_save_data::Publish(s,c.object,key);
            if(g_session.load()!=s||world!=ue_wrap::world_identity::Generation()||!live.Get())return;
            auto it=g_grillSaves.find(key);
            if(sent&&it!=g_grillSaves.end()&&it->second.serial==serial)g_grillSaves.erase(it);
        }
    }
    if(!Client())Publish(c.object,-1);
}
void AfterGrillAction(const sg::Call& c) {
    auto it=std::find_if(g_lidScopes.rbegin(),g_lidScopes.rend(),[&](const LidScope& scope){
        return scope.object==c.object && scope.stack==c.stack && scope.depth==c.depth;});
    if(it!=g_lidScopes.rend())g_lidScopes.erase(std::next(it).base());
    AfterGrillChange(c);
}
sg::Verdict OnAction(const sg::Call& c) {
    if (!Client() || c.fromOurCode || !IsMachine(c.object)) return sg::Verdict::Run;
    // The dedicated coffee lane validates and queues these presses once.
    if (K::Type(c.object)==K::Kind::CoffeeMaker || K::Type(c.object)==K::Kind::CoffeeVending)
        return sg::Verdict::Run;
    if (K::Type(c.object)==K::Kind::Cremator) return sg::Verdict::Run;
    if(hand_item::IsHandAxisActor(c.object))return hand_item::IsMirrorActor(c.object)?sg::Verdict::Cancel:sg::Verdict::Run;
    if (!net_pump::IsInAnnouncedWorld(c.object)) return sg::Verdict::Run;
    static R::ParamOffset actionOff{L"action"};
    const int32_t off=actionOff.Of(c.function);
    if (!c.locals || off<0 || off>=R::FunctionFrameSize(c.function)) return sg::Verdict::Run;
    const uint8_t action=c.locals[off];
    if(Grill(c.object)&&action==4){QueueGrill(c.object,6);return sg::Verdict::Cancel;}
    if (action!=4 && !(K::Type(c.object)==K::Kind::Microwave && action==8)) return sg::Verdict::Run;
    net::KitchenIntentPayload p{};
    Identity(c.object,p.key,p.elementId);
    p.op=1; p.action=action; p.control=K::ReadControl(c.object);
    if (p.key.len || p.elementId) {
        auto* s=g_session.load();
        if (!s->SendReliable(net::ReliableKind::KitchenIntent,&p,sizeof(p)))
            UE_LOGW("kitchen: button intent could not be queued");
    }
    return sg::Verdict::Cancel;
}
sg::Verdict OnProducer(const sg::Call& c) {
    auto* s=g_session.load();
    if(c.fromOurCode || !s || !s->connected())return sg::Verdict::Run;
    if(K::Type(c.object)==K::Kind::Grill && hand_item::IsMirrorActor(c.object))return sg::Verdict::Cancel;
    if(hand_item::IsHandAxisActor(c.object))return sg::Verdict::Run;
    return Client() && IsMachine(c.object) && net_pump::IsInAnnouncedWorld(c.object)
        ? sg::Verdict::Cancel : sg::Verdict::Run;
}
sg::Verdict OnDoor(const sg::Call& c) {
    if (!Client() || c.fromOurCode) return sg::Verdict::Run;
    void* parent=K::DoorParent(c.object);
    if (!parent || !K::OwnsDoor(parent) || !net_pump::IsInAnnouncedWorld(parent)) return sg::Verdict::Run;
    // A receiver's hinge can touch its stop between snapshots. Only the host
    // decides that automatic close; forwarding it would undo a peer's drag.
    if (c.tag==0x4B540007 && K::DoorUser(parent)!=players::Registry::Get().Local()) return sg::Verdict::Cancel;
    net::KitchenIntentPayload p{};
    Identity(parent,p.key,p.elementId); p.op=c.tag==0x4B540006 ? 2 : 3;
    if (p.key.len || p.elementId) g_session.load()->SendReliable(net::ReliableKind::KitchenIntent,&p,sizeof(p));
    return sg::Verdict::Cancel;
}
sg::Verdict OnMouse(const sg::Call& c) {
    if (!Client() || c.fromOurCode) return sg::Verdict::Run;
    void* parent=K::DoorParent(c.object);
    if (!parent || !net_pump::IsInAnnouncedWorld(parent) ||
        K::DoorUser(parent)!=players::Registry::Get().Local()) return sg::Verdict::Run;
    static R::ParamOffset mouse{L"mouseXY"};
    const int off=mouse.Of(c.function);
    if (!c.locals || off<0) return sg::Verdict::Cancel;
    ue_wrap::FVector2D delta{}; std::memcpy(&delta,c.locals+off,sizeof(delta));
    if (!std::isfinite(delta.X) || !std::isfinite(delta.Y)) return sg::Verdict::Cancel;
    ResetWorld();
    net::WireKey key{}; uint32_t eid=0; Identity(parent,key,eid);
    const auto id=Token(key,eid);
    if ((!key.len && !eid) || (g_motion.size()>=256 && !g_motion.count(id))) return sg::Verdict::Cancel;
    auto& motion=g_motion[id]; motion.actor.Set(parent);
    motion.x=std::clamp(motion.x+delta.X,-200.f,200.f);
    motion.y=std::clamp(motion.y+delta.Y,-200.f,200.f);
    return sg::Verdict::Cancel;
}
}

void Install(net::Session* session) {
    if(g_session.load()!=session)OnDisconnect();
    g_session.store(session);
    grill_hand_sync::Install(session);
    grill_edits::Install(session);
    if (g_installed) return;
    g_installed=true;
    element::scan_hub::Register({"kitchen",nullptr,&Ready,&IsMachine,&Begin,&Match,&Complete,2,&ClassCount});
    sg::WatchName(L"actionOptionIndex",0x4B540001,&OnAction,nullptr);
    sg::WatchClassName(L"prop_minioven_C",L"ReceiveTick",0x4B540002,&OnProducer,nullptr);
    sg::WatchClassName(L"prop_microwave_C",L"wave",0x4B540003,&OnProducer,nullptr);
    sg::WatchClassName(L"prop_coffeeMaker_C",
        L"BndEvt__StaticMesh_K2Node_ComponentBoundEvent_0_ComponentHitSignature__DelegateSignature",
        0x4B540004,&OnProducer,nullptr);
    sg::WatchName(L"Open",0x4B540006,&OnDoor,nullptr);
    sg::WatchName(L"Close",0x4B540007,&OnDoor,nullptr);
    sg::WatchClassName(L"prop_swinger_C",L"mouseAssign",0x4B540008,&OnMouse,nullptr);
    sg::WatchClassName(L"prop_bbq_C",L"ReceiveTick",0x4B540009,&OnProducer,nullptr);
    sg::WatchClassName(L"prop_bbq_C",L"attemptIgnite",0x4B54000A,&OnGrillIgnite,&AfterGrillChange);
    sg::WatchClassName(L"prop_bbq_C",L"addCoal",0x4B54000B,&OnGrillCoal,&AfterGrillChange);
    sg::WatchClassName(L"prop_bbq_C",L"actionOptionIndex",0x4B54000C,&OnGrillAction,&AfterGrillAction);
    sg::WatchClassName(L"prop_bbq_C",L"setLid",0x4B54000D,&OnGrillLid,&AfterGrillChange);
    for(const auto* verb:{L"playSound",L"Timeline_0__UpdateFunc",L"Timeline_0__FinishedFunc"})
        sg::WatchClassName(L"prop_bbq_C",verb,0x4B54000E,&OnProducer,nullptr);
}
void Tick() {
    auto* s=g_session.load();
    if (!s || !s->connected() || g_ticking) return;
    struct Guard {Guard(){g_ticking=true;}~Guard(){g_ticking=false;}} guard;
    grill_hand_sync::Tick();
    if(g_session.load()!=s || !s->connected())return;
    ResetWorld();const auto epoch=g_epoch;sg::ResolvePendingNames();
    if(!Current(s,epoch))return;
    grill_edits::Tick();
    if(!Current(s,epoch))return;
    ReconcileRecipients();
    if(!Client())RetryJoins();
    if(!Current(s,epoch))return;
    DrainGrills();
    if(!Current(s,epoch))return;
    const auto actors=g_actors;
    const uint64_t now=Now();
    if (Client()) {
        const float seconds=g_animationAt ? static_cast<float>(now-g_animationAt)/1000.f : 0.f;
        g_animationAt=now;
        for (const auto& r:actors) if (void* a=r.Get()) {
            K::Animate(a,seconds);if(!Current(s,epoch))return;
        }
        if (now<g_next) return;
        g_next=now+50;
        std::vector<std::wstring> motions;for(const auto& [id,row]:g_motion)motions.push_back(id);
        for (const auto& id:motions) {
            auto it=g_motion.find(id);if(it==g_motion.end())continue;
            const auto motion=it->second;
            void* a=motion.actor.Get();
            if (!a) { g_motion.erase(it); continue; }
            net::KitchenIntentPayload p{}; Identity(a,p.key,p.elementId);
            if(!Current(s,epoch))return;
            p.op=4; p.mouseX=motion.x; p.mouseY=motion.y;
            g_motion.erase(it);
            if (p.mouseX==0 && p.mouseY==0)continue;
            const bool sent=s->SendReliable(net::ReliableKind::KitchenIntent,&p,sizeof(p));
            if(!Current(s,epoch))return;
            if(!sent && motion.actor.Get()) {
                auto found=g_motion.find(id);
                if(found!=g_motion.end() && !found->second.actor.Is(a))continue;
                auto& retry=g_motion[id];retry.actor=motion.actor;
                retry.x=std::clamp(retry.x+p.mouseX,-200.f,200.f);
                retry.y=std::clamp(retry.y+p.mouseY,-200.f,200.f);
            }
        }
        // Keep the latest row until its actor arrives. Once applied to that actor, leave the
        // 50-ms retry loop: Apply reads and composes several component transforms. Later canonical
        // host snapshots still apply immediately in Apply(), including same-state heartbeats that
        // repair physics drift.
        std::vector<std::wstring> pending;for(const auto& row:g_pending)pending.push_back(row.first);
        for (const auto& id:pending) {
            auto row=g_pending.find(id);if(row==g_pending.end())continue;
            auto done=g_applied.find(id);
            void* actor=done==g_applied.end()?nullptr:done->second.actor.Get();
            if (actor && done->second.revision==row->second.revision &&
                ue_wrap::world_identity::WorldOf(actor)==ue_wrap::world_identity::CurrentWorld()) continue;
            PaintAndRemember(id);
            if(!Current(s,epoch))return;
        }
    } else {
        if (now<g_next) return;
        // State-changing actions publish immediately. This pass is a recovery heartbeat,
        // so avoid polling every appliance and door several times a second.
        g_next=now+50;
        for (auto it=g_sent.begin();it!=g_sent.end();) {
            if (!it->second.actor.Alive()) it=g_sent.erase(it); else ++it;
        }
        for (auto it=g_doorOwners.begin();it!=g_doorOwners.end();) {
            if (!it->second.actor.Alive() || now>=it->second.until) it=g_doorOwners.erase(it); else ++it;
        }
        const bool recovery=now>=g_recoveryAt;
        if (recovery) g_recoveryAt=now+1000;
        for (const auto& r:actors) if (void* a=r.Get()) {
            // Cremator hinges move continuously; other appliances keep the
            // previous recovery polling cadence.
            if (K::Type(a)==K::Kind::Cremator || K::Type(a)==K::Kind::Grill || recovery) Publish(a,-1);
            if(!Current(s,epoch))return;
        }
    }
}
void Apply(const net::KitchenStatePayload& p) {
    if (!Client() || !Valid(p)) return;
    ResetWorld();
    const auto id=Token(p.key,p.elementId);
    if (g_pending.size()>=256 && !g_pending.count(id)) return;
    g_pending[id]={p,++g_revision}; PaintAndRemember(id,true);
}
void OnIntent(const net::KitchenIntentPayload& p,uint8_t senderSlot) {
    auto* s=g_session.load();
    if (!s || s->role()!=net::Role::Host || senderSlot<1 || senderSlot>=net::kMaxPeers ||
        p.key.len>sizeof(p.key.data) || p.op<1 || p.op>6 ||
        !std::isfinite(p.mouseX) || !std::isfinite(p.mouseY) || std::fabs(p.mouseX)>200.f || std::fabs(p.mouseY)>200.f) return;
    ResetWorld();
    void* a=Resolve(p.key,p.elementId);
    if (!a) return;
    if (K::Type(a)==K::Kind::Cremator && p.op==1) return; // lever uses DoorVerbIntent
    auto target=element::IntentTarget::ForClientIntent(*s,senderSlot,500.f);
    if (!target.Authorize(a)) return;
    if(p.op>=5) {
        if(!Grill(a)||p.action||p.control||p.mouseX!=0||p.mouseY!=0)return;
        QueueGrill(a,p.op,senderSlot);return;
    }
    const uint64_t now=Now();
    if (p.op==1) {
        if (g_intentAt[senderSlot] && now-g_intentAt[senderSlot]<150) return;
        g_intentAt[senderSlot]=now;
    } else {
        if (!K::OwnsDoor(a) || K::DoorUser(a)==players::Registry::Get().Local()) return;
        const auto playerNo=roster_ledger::Get(senderSlot).playerNo;
        if (!playerNo) return;
        net::WireKey key{}; uint32_t eid=0; Identity(a,key,eid);
        auto& owner=g_doorOwners[Token(key,eid)];
        if (owner.actor.Get()==a && now<owner.until && owner.playerNo!=playerNo) return;
        owner.actor.Set(a); owner.playerNo=playerNo; owner.until=now+400;
    }
    bool ok=false;
    if (p.op==1) {
        auto* puppet=players::Registry::Get().Puppet(senderSlot);
        void* player=puppet ? puppet->GetActor() : nullptr;
        ok=K::Press(a,player,p.action,p.control);
    } else if (p.op==4) {
        auto* puppet=players::Registry::Get().Puppet(senderSlot);
        ok=K::MoveDoor(a,puppet ? puppet->GetActor() : nullptr,{p.mouseX,p.mouseY});
    } else ok=K::SetDoor(a,p.op==2);
    if (p.op!=4) {
        UE_LOGI("kitchen: peer %u op=%u action=%u control=%u applied=%d",senderSlot,p.op,p.action,p.control,ok);
        Publish(a,-1); Publish(a,senderSlot);
    }
}
void SendAllToSlot(int slot) {
    grill_hand_sync::QueueConnect(slot);
    auto* s=g_session.load();
    if (!s || s->role()!=net::Role::Host || slot<1 || slot>=net::kMaxPeers) return;
    ResetWorld();
    ReconcileRecipients();
    const auto epoch=g_epoch;const auto actors=g_actors;
    for(const auto& ref:actors)if(void* actor=ref.Get()) {
        if(!SharedMachine(actor))continue;
        net::WireKey key{};uint32_t eid=0;Identity(actor,key,eid);
        if(!key.len && !eid)continue;
        const auto id=Token(key,eid);
        if(g_joins.size()>=256 && !g_joins.count(id))continue;
        auto& row=g_joins[id];row.actor=ref;row.pending|=uint32_t(1u<<slot);
    }
    if(Current(s,epoch))RetryJoins();
}
void OnPeerLeft(int slot) {
    if(slot<0 || slot>=net::kMaxPeers)return;
    grill_hand_sync::OnPeerLeft(slot);grill_edits::OnPeerLeft(slot);g_joinPeople[slot]=0;g_intentAt[slot]=0;
    for(auto& [key,row]:g_joins)row.pending&=uint32_t(~(1u<<slot));
    std::erase_if(g_grillOps,[slot](const auto& row){return row.second.slot==slot;});
}
void OnDisconnect() {
    grill_hand_sync::OnDisconnect();
    grill_edits::OnDisconnect();
    ++g_epoch;g_lidScopes.clear();
    g_session.store(nullptr); g_actors.clear(); g_scratch.clear(); g_sent.clear(); g_pending.clear();
    g_applied.clear(); g_retryAt.clear();
    g_next=g_animationAt=g_recoveryAt=0; g_world=0;
    g_motion.clear(); g_doorOwners.clear();
    g_joins.clear();g_joinCursor.clear();
    for(auto& person:g_joinPeople)person=0;
    g_grillOps.clear();g_grillLast.clear();g_grillSaves.clear();++g_grillSerial;
    for (int slot=0;slot<net::kMaxPeers;++slot) g_intentAt[slot]=0;
}
bool IsLocalLidOutput(void* source) {
    if(!Client())return false;
    for(auto it=g_lidScopes.rbegin();it!=g_lidScopes.rend();++it)
        if(it->object==source && it->actor.Get() && Current(g_session.load(),it->epoch) &&
           it->player.Get() && it->player.Get()==players::Registry::Get().Local())return true;
    return false;
}
bool PublishCurrent(void* actor) {
    auto* session=g_session.load();
    return session && session->connected() && session->role()==net::Role::Host && Grill(actor) && Publish(actor,-1);
}
}
