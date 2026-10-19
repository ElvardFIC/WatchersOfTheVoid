#include "coop/interactables/kitchen_sync.h"
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

namespace coop::kitchen_sync {
namespace {
namespace K=ue_wrap::kitchen_machine;
namespace R=ue_wrap::reflection;
namespace sg=ue_wrap::script_gate;
std::atomic<net::Session*> g_session{nullptr};
bool g_installed=false;
uint32_t g_world=0;
uint64_t g_next=0, g_animationAt=0;
uint64_t g_recoveryAt=0;
std::vector<ue_wrap::CachedObjRef> g_actors, g_scratch;
struct Last { ue_wrap::CachedObjRef actor; net::KitchenStatePayload state{}; uint64_t at=0; };
std::unordered_map<std::wstring,Last> g_sent;
std::unordered_map<std::wstring,net::KitchenStatePayload> g_pending;
struct Applied { ue_wrap::CachedObjRef actor; };
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
uint64_t Now() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
bool Client() {
    auto* s=g_session.load();
    return s && s->connected() && s->role()==net::Role::Client;
}
bool IsMachine(void* a) { return K::Type(a)!=K::Kind::None; }
void ResetWorld() {
    const uint32_t gen=ue_wrap::world_identity::Generation();
    if (gen==g_world) return;
    g_world=gen; g_actors.clear(); g_scratch.clear(); g_sent.clear(); g_pending.clear();
    g_applied.clear(); g_retryAt.clear();
    g_next=0; g_animationAt=0; g_recoveryAt=0;
    g_motion.clear(); g_doorOwners.clear();
    g_grillOps.clear();g_grillLast.clear();g_grillSaves.clear();++g_grillSerial;
    for (int slot=0;slot<net::kMaxPeers;++slot) g_intentAt[slot]=0;
}
void Begin(void*,bool full) {
    ResetWorld(); g_scratch.clear();
    if (!full) for (auto& r:g_actors) if (r.Alive()) g_scratch.push_back(r);
}
void Match(void*,void* a) {
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
    if (!a || !IsMachine(a) || ue_wrap::world_identity::WorldOf(a)!=ue_wrap::world_identity::CurrentWorld()) return nullptr;
    return a;
}
bool Valid(const net::KitchenStatePayload& p) {
    if (p.kind==6 && ((p.flags&~K::Opened)||!p.hasDoorPose)) return false;
    if(p.kind==5 && ((p.flags&~K::Active)||p.coals<0||p.coals>16||
        !std::isfinite(p.burn)||p.burn<0||p.burn>=1.01f))return false;
    return kitchen_wire::ValidPose(p) && p.key.len<=sizeof(p.key.data) && (p.key.len || p.elementId) && p.kind>=1 && p.kind<=6 &&
        !(p.flags&128) && p.mode>=0 && p.mode<=4 && std::isfinite(p.remaining) &&
        std::isfinite(p.timer) && std::isfinite(p.power) && p.remaining>=0 && p.remaining<=60 &&
        p.timer>=0 && p.timer<=60 && p.power>=0 && p.power<=1.01f;
}
K::State Decode(const net::KitchenStatePayload& p) {
    K::State s{}; s.kind=static_cast<K::Kind>(p.kind); s.flags=p.flags; s.mode=p.mode;
    s.remaining=p.remaining; s.timer=p.timer; s.power=p.power;
    s.coals=p.coals;s.burn=p.burn;
    s.hasDoorPose=p.hasDoorPose!=0; if (s.hasDoorPose) s.doorPose=kitchen_wire::DecodePose(p);
    return s;
}
bool PaintAndRemember(const std::wstring& id,const net::KitchenStatePayload& p,bool immediate=false) {
    const uint64_t now=Now();
    auto retry=g_retryAt.find(id);
    if (!immediate && retry!=g_retryAt.end() && now<retry->second) return false;
    void* actor=Resolve(p.key,p.elementId);
    if (!actor || !K::Apply(actor,Decode(p))) {
        g_applied.erase(id);
        g_retryAt[id]=now+250;
        return false;
    }
    g_retryAt.erase(id);
    g_applied[id].actor.Set(actor);
    return true;
}
void Publish(void* a,int slot) {
    auto* s=g_session.load();
    K::State state{};
    if (!s || !K::Read(a,state)) return;
    net::KitchenStatePayload p{};
    Identity(a,p.key,p.elementId);
    if (!p.key.len && !p.elementId) return;
    p.kind=static_cast<uint8_t>(state.kind); p.flags=state.flags; p.mode=state.mode;
    p.remaining=state.remaining; p.timer=state.timer; p.power=state.power;
    p.coals=state.coals;p.burn=state.burn;
    kitchen_wire::EncodePose(state,p);
    if (!Valid(p)) return;
    const auto id=Token(p.key,p.elementId);
    auto& last=g_sent[id];
    const uint64_t now=Now();
    // A heartbeat also repairs join timing and local cord callbacks. A replacement actor on
    // the same save key always gets a fresh baseline, held by slot+serial rather than address.
    if (slot<0 && last.actor.Get()==a && now-last.at<2000 &&
        p.kind==last.state.kind && p.flags==last.state.flags && p.mode==last.state.mode &&
        p.coals==last.state.coals && std::fabs(p.burn-last.state.burn)<.02f &&
        p.timer==last.state.timer && std::fabs(p.remaining-last.state.remaining)<0.2f &&
        std::fabs(p.power-last.state.power)<0.01f && kitchen_wire::SamePose(p,last.state)) return;
    const bool ok=slot<0 ? s->SendReliable(net::ReliableKind::KitchenState,&p,sizeof(p))
        : s->SendReliableToSlot(slot,net::ReliableKind::KitchenState,&p,sizeof(p));
    if (ok && slot<0) {
        if (last.actor.Get()!=a || last.state.flags!=p.flags || last.state.mode!=p.mode)
            UE_LOGI("kitchen: state key='%ls' eid=%u kind=%u flags=%u mode=%d",id.c_str(),p.elementId,p.kind,p.flags,p.mode);
        last.actor.Set(a); last.state=p; last.at=now;
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
    auto* s=g_session.load();const auto world=g_world;const auto now=Now();
    std::vector<std::wstring> saves;for(const auto& row:g_grillSaves)saves.push_back(row.first);
    for(const auto& key:saves) {
        auto it=g_grillSaves.find(key);if(it==g_grillSaves.end())continue;
        auto row=it->second;void* a=row.actor.Get();
        if(!a||!Grill(a)||ue_wrap::prop::GetKeyString(a)!=key){g_grillSaves.erase(it);continue;}
        if(now<row.retryAt)continue;it->second.retryAt=now+500;
        const bool sent=prop_save_data::Publish(s,a,key);
        if(g_session.load()!=s||world!=ue_wrap::world_identity::Generation())return;
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
            // A freshly added client's coal record can arrive after its ignite
            // intent on another lane. Wait briefly for the native fuel to land.
            K::State state{};
            if(K::Read(a,state) && (row.p.op==6||state.coals>0)) {
                done=K::SetGrillFire(a,row.p.op==5);
                if(done&&row.actor.Get()&&g_session.load()==s&&world==ue_wrap::world_identity::Generation())Publish(a,-1);
            }
        }
        if(g_session.load()!=s||world!=ue_wrap::world_identity::Generation())return;
        it=g_grillOps.find(id);
        if(done&&it!=g_grillOps.end()&&it->second.serial==row.serial)g_grillOps.erase(it);
    }
}
sg::Verdict OnGrillIgnite(const sg::Call& c) {
    if(!Client()||c.fromOurCode||!Grill(c.object)||!net_pump::IsInAnnouncedWorld(c.object))return sg::Verdict::Run;
    K::State state{};if(K::Read(c.object,state)&&(state.flags&K::Active))return sg::Verdict::Cancel;
    QueueGrill(c.object,5);return sg::Verdict::Cancel;
}
void AfterGrillChange(const sg::Call& c) {
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
    if(c.tag==0x4B54000B && net_pump::IsInAnnouncedWorld(c.object)) {
        // addCoal has already consumed one local coal item. Publish its native
        // contents through the existing record lane; no second addCoal replay.
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
sg::Verdict OnAction(const sg::Call& c) {
    if (!Client() || c.fromOurCode || !IsMachine(c.object)) return sg::Verdict::Run;
    if (K::Type(c.object)==K::Kind::Cremator) return sg::Verdict::Run;
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
    if ((!key.len && !eid) || g_motion.size()>=256) return sg::Verdict::Cancel;
    auto& motion=g_motion[Token(key,eid)]; motion.actor.Set(parent);
    motion.x=std::clamp(motion.x+delta.X,-200.f,200.f);
    motion.y=std::clamp(motion.y+delta.Y,-200.f,200.f);
    return sg::Verdict::Cancel;
}
}

void Install(net::Session* session) {
    if(g_session.load()!=session)OnDisconnect();
    g_session.store(session);
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
    sg::WatchClassName(L"prop_bbq_C",L"addCoal",0x4B54000B,nullptr,&AfterGrillChange);
    sg::WatchClassName(L"prop_bbq_C",L"actionOptionIndex",0x4B54000C,nullptr,&AfterGrillChange);
}
void Tick() {
    auto* s=g_session.load();
    if (!s || !s->connected()) return;
    ResetWorld(); sg::ResolvePendingNames();
    DrainGrills();
    if(g_session.load()!=s||g_world!=ue_wrap::world_identity::Generation())return;
    const uint64_t now=Now();
    if (Client()) {
        const float seconds=g_animationAt ? static_cast<float>(now-g_animationAt)/1000.f : 0.f;
        g_animationAt=now;
        for (auto& r:g_actors) if (void* a=r.Get()) K::Animate(a,seconds);
        if (now<g_next) return;
        g_next=now+50;
        for (auto it=g_motion.begin();it!=g_motion.end();) {
            void* a=it->second.actor.Get();
            if (!a) { it=g_motion.erase(it); continue; }
            net::KitchenIntentPayload p{}; Identity(a,p.key,p.elementId);
            p.op=4; p.mouseX=it->second.x; p.mouseY=it->second.y;
            if (p.mouseX==0 && p.mouseY==0) { it=g_motion.erase(it); continue; }
            if (s->SendReliable(net::ReliableKind::KitchenIntent,&p,sizeof(p))) it=g_motion.erase(it);
            else ++it;
        }
        // Keep the latest row until its actor arrives. Once applied to that actor, leave the
        // 50-ms retry loop: Apply reads and composes several component transforms. Later canonical
        // host snapshots still apply immediately in Apply(), including same-state heartbeats that
        // repair physics drift.
        for (auto& row:g_pending) {
            auto done=g_applied.find(row.first);
            void* actor=done==g_applied.end()?nullptr:done->second.actor.Get();
            if (actor && ue_wrap::world_identity::WorldOf(actor)==ue_wrap::world_identity::CurrentWorld()) continue;
            PaintAndRemember(row.first,row.second);
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
        for (auto& r:g_actors) if (void* a=r.Get()) {
            // Cremator hinges move continuously; other appliances keep the
            // previous recovery polling cadence.
            if (K::Type(a)==K::Kind::Cremator || recovery) Publish(a,-1);
        }
    }
}
void Apply(const net::KitchenStatePayload& p) {
    if (!Client() || !Valid(p)) return;
    ResetWorld();
    const auto id=Token(p.key,p.elementId);
    if (g_pending.size()>=256 && !g_pending.count(id)) return;
    g_pending[id]=p; PaintAndRemember(id,p,true);
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
    auto* s=g_session.load();
    if (!s || s->role()!=net::Role::Host || slot<1 || slot>=net::kMaxPeers) return;
    ResetWorld();
    for (auto& r:g_actors) if (void* a=r.Get()) Publish(a,slot);
}
void OnDisconnect() {
    g_session.store(nullptr); g_actors.clear(); g_scratch.clear(); g_sent.clear(); g_pending.clear();
    g_applied.clear(); g_retryAt.clear();
    g_next=g_animationAt=g_recoveryAt=0; g_world=0;
    g_motion.clear(); g_doorOwners.clear();
    g_grillOps.clear();g_grillLast.clear();g_grillSaves.clear();++g_grillSerial;
    for (int slot=0;slot<net::kMaxPeers;++slot) g_intentAt[slot]=0;
}
}
