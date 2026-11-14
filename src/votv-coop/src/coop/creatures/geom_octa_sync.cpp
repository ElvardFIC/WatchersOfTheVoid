// coop/creatures/geom_octa_sync.cpp -- see coop/creatures/geom_octa_sync.h.

#include "coop/creatures/geom_octa_sync.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/creatures/geom_octa_relay.h"
#include "coop/creatures/npc_mirror_brain.h"
#include "coop/element/element.h"
#include "coop/element/registry.h"
#include "coop/net/net_clock.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"

#include "ue_wrap/actors/geom_octa.h"
#include "ue_wrap/actors/kerfur.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/facts/function_row.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <array>
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>
#include <functional>

namespace coop::geom_octa_sync {
namespace {

namespace R  = ue_wrap::reflection;
namespace sg = ue_wrap::script_gate;
namespace EL = coop::element;
namespace GO = ue_wrap::geom_octa;
namespace N  = coop::net;

// The rows: the bodies this lane watches, each declared on the class that owns it. The two notify functions are the
// class's export names for the "fire" and "close" montage sections' finish notify, one GUID each.
UE_FACT_FN(kRowAggro, "geom_octa_sync", L"geomOcta_C", L"aggro", Declared);
UE_FACT_FN(kRowFireFinish, "geom_octa_sync", L"geomOcta_C", L"OnNotifyBegin_1DFC28904024A8FB71C3ACA9DBD440F9",
           Declared);
UE_FACT_FN(kRowCloseFinish, "geom_octa_sync", L"geomOcta_C", L"OnNotifyBegin_9B2108B443F09BA08A3C20ADBECCC562",
           Declared);
UE_FACT_FN(kRowLaserBeginPlay, "geom_octa_sync", L"geomOctaLazer_C", L"ReceiveBeginPlay", Declared);
UE_FACT_FN(kRowOctaGraph, "geom_octa_sync", L"geomOcta_C", L"ExecuteUbergraph_geomOcta", Declared);
// The two begin-overlap handlers whose target choice a mirror's brain refuses; watched on the host only while the Octa
// drill's measure or attack arm is on, to see whether any overlap fires at all.
UE_FACT_FN(kRowMinSphereOverlap, "geom_octa_sync", L"geomOcta_C",
           L"BndEvt__geomOcta_minSphere_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);
UE_FACT_FN(kRowPropSphereOverlap, "geom_octa_sync", L"geomOcta_C",
           L"BndEvt__geomOcta_propAggroSphere_K2Node_ComponentBoundEvent_1_ComponentBeginOverlapSignature__DelegateSignature",
           Declared);

constexpr int kTagAggro = 0x4F435430;   // 'OCT0'
constexpr int kTagFire  = 0x4F435431;   // 'OCT1'
constexpr int kTagClose = 0x4F435432;   // 'OCT2'
constexpr int kTagLaser = 0x4F435433;   // 'OCT3'
constexpr int kTagGraph = 0x4F435434;   // 'OCT4'
constexpr int kTagMinSphere  = 0x4F435435;   // 'OCT5'
constexpr int kTagPropSphere = 0x4F435436;   // 'OCT6'

// The entry of the Octa's event graph that is the wander (`move`), a cooked byte offset: a mirror that runs it was not
// parked. The stub `move` function runs whether or not the graph's entry does, so the graph is what is watched.
constexpr int32_t kMoveEntry = 5538;

std::atomic<coop::net::Session*> g_session{nullptr};
uint64_t g_epoch = 0;
uint32_t g_world = 0;
uint32_t g_serial=0;
uint64_t g_stops=0;
N::Role g_role=N::Role::Host;
bool g_watched = false;

Counters g_counters{};
char     g_lastTarget[40] = {};
uint32_t g_windowWanted = 0;    // OpenVerdictWindow: the verdict to mark, 0 for no window
uint32_t g_windowCounted = 0;   // the verdicts of the window's Octa counted since it opened
int32_t  g_windowIndex = -1;    // the window's Octa, by object slot and the slot's serial
int32_t  g_windowSerial = 0;

// The host's count of "fire" finish verdicts per Octa, for its life, keyed by the Octa's object slot and the slot's
// serial (the shape of npc_mirror_brain's mark): an element id or an address recycled for another Octa never inherits a
// count. A fixed table; a new Octa takes an entry whose slot no longer holds its serial, else the next entry in
// round-robin order.
struct VerdictCount {
    int32_t  index = -1;
    int32_t  serial = 0;
    uint32_t n = 0;
};
constexpr size_t kMaxOctas = 16;
VerdictCount g_verdicts[kMaxOctas];
size_t       g_verdictNext = 0;

// `octa`'s verdict count, counted up by one; 0 when its slot gives no serial.
uint32_t CountVerdict(void* octa) {
    const int32_t index = R::InternalIndexOf(octa);
    if (index < 0) return 0;
    for (VerdictCount& v : g_verdicts)
        if (v.index == index && R::SlotSerial(index) == v.serial) return ++v.n;
    const int32_t serial = R::AllocateSlotSerial(index);
    if (serial == 0) return 0;
    VerdictCount* slot = nullptr;
    for (VerdictCount& v : g_verdicts) {
        if (v.index < 0 || R::SlotSerial(v.index) != v.serial) {
            slot = &v;
            break;
        }
    }
    if (!slot) {
        slot = &g_verdicts[g_verdictNext];
        g_verdictNext = (g_verdictNext + 1) % kMaxOctas;
    }
    *slot = VerdictCount{index, serial, 1};
    return 1;
}

// `octa`'s verdict count so far, without counting; 0 for an Octa with none.
uint32_t PeekVerdicts(void* octa) {
    const int32_t index = R::InternalIndexOf(octa);
    for (const VerdictCount& v : g_verdicts)
        if (index >= 0 && v.index == index && R::SlotSerial(index) == v.serial) return v.n;
    return 0;
}

// `aggro`'s entry as the PRE saw it: whether the Octa had a target, kept for the POST of the same call. The body's
// guard tests the old target, not the argument, so the POST alone cannot tell a passed guard from a refused one.
struct Invocation {
    ue_wrap::CachedObjRef octa, function;
    void* stack = nullptr;
    uint64_t invocation = 0, epoch = 0;
};
struct AggroEntry : Invocation {
    ue_wrap::CachedObjRef target;
    bool  hadTarget = false;
};
std::array<AggroEntry, 32> g_aggroEntries{};
std::array<Invocation, 32> g_fireEntries{};

R::ParamOffset g_aggroArg{L"actor"};
R::ParamOffset g_graphEntry{L"EntryPoint"};

bool IsHost() {
    coop::net::Session* s = g_session.load(std::memory_order_acquire);
    return s && s->running() && g_role==N::Role::Host && s->role() == coop::net::Role::Host &&
        g_world == ue_wrap::world_identity::Generation() && g_serial==N::session_serial::Current() && g_stops==s->StopCount();
}

// The element id of `actor` when the registry names it an Npc element, else the invalid id.
EL::ElementId NpcEid(void* actor) {
    if (!actor) return EL::kInvalidId;
    const EL::Registry& reg = EL::Registry::Get();
    const EL::ElementId eid = reg.EidForActor(actor);
    if (eid == EL::kInvalidId) return EL::kInvalidId;
    const EL::Element* el = reg.Get(eid);
    return el && el->GetType() == EL::ElementType::Npc ? eid : EL::kInvalidId;
}

void EidText(EL::ElementId eid, char (&out)[16]) {
    if (eid == EL::kInvalidId) std::snprintf(out, sizeof(out), "none");
    else std::snprintf(out, sizeof(out), "%u", static_cast<unsigned>(eid));
}

// A target as the wire carries it: a player by slot (the host's own is 0, a puppet its slot), else an element the
// registry names, else none. An ATV is no element, so an Octa that targets one sends None: the client's look of that
// attack idles, and its outcome (the blast) still arrives.
struct WireTarget {
    uint8_t  kind = N::kGeomOctaTargetNone;
    uint32_t v = 0;
};

WireTarget WireTargetOf(void* target) {
    if (!target) return WireTarget{};
    const uint8_t peer = coop::players::Registry::Get().PeerIdOfActor(target);
    if (peer != coop::players::kPeerIdUnknown) return WireTarget{N::kGeomOctaTargetSlot, peer};
    const EL::ElementId eid = EL::Registry::Get().EidForActor(target);
    if (eid != EL::kInvalidId) return WireTarget{N::kGeomOctaTargetElement, static_cast<uint32_t>(eid)};
    return WireTarget{};
}

void TargetText(const WireTarget& t, char (&out)[40]) {
    if (t.kind == N::kGeomOctaTargetSlot) std::snprintf(out, sizeof(out), "slot:%u", static_cast<unsigned>(t.v));
    else if (t.kind == N::kGeomOctaTargetElement) std::snprintf(out, sizeof(out), "element:%u", static_cast<unsigned>(t.v));
    else std::snprintf(out, sizeof(out), "none");
}

// ---- what the host knows of each Octa's EntitySpawn, and the one Aggro it keeps ----
//
// Host enrollment enables decisions; DrainDecisions separately proves each recipient's ordered birth admission.
// The event rides Default behind that recipient's idempotent birth. The Octa's first aggro runs inside its own spawn's finish, on both spawn routes,
// before the NPC lane's send (the dev route sends at the interceptor's PRE, the gamemode's at the drain), so an Aggro
// that finds no sent spawn is kept, one per Octa, and goes out from OnNpcSpawnSent.
// Divergence from MTA, whose server never holds a message for an element: an element exists from its creation
// (reference/mtasa-blue/Server/mods/deathmatch/logic/CElement.cpp:39), so what is said of it can be sent at once. Here the
// Octa's first decision is made inside the spawn, before the NPC lane has made it an element a peer knows of.
constexpr size_t kMaxSent = 16;
uint32_t g_sentEids[kMaxSent];
size_t   g_sentCount = 0;

bool SpawnSent(uint32_t eid) {
    for (size_t i = 0; i < g_sentCount; ++i)
        if (g_sentEids[i] == eid) return true;
    return false;
}

void ForgetSent(uint32_t eid) {
    for (size_t i = 0; i < g_sentCount; ++i) {
        if (g_sentEids[i] == eid) {
            g_sentEids[i] = g_sentEids[--g_sentCount];
            return;
        }
    }
}

void NoteSent(uint32_t eid) {
    if (SpawnSent(eid)) return;
    // An eid whose element is gone is the end of that Octa; its entry goes first.
    for (size_t i = 0; i < g_sentCount;) {
        if (!EL::Registry::Get().Get(static_cast<EL::ElementId>(g_sentEids[i]))) g_sentEids[i] = g_sentEids[--g_sentCount];
        else ++i;
    }
    if (g_sentCount < kMaxSent) g_sentEids[g_sentCount++] = eid;
    else UE_LOGW("geom_octa_sync: more than %zu Octas with a sent spawn -- the newest is not tracked", kMaxSent);
}

struct KeptAggro {
    int32_t    index = -1;   // the Octa, by object slot and the slot's serial
    int32_t    serial = 0;
    WireTarget target;
    uint32_t   base = 0;
};
constexpr size_t kMaxKept = 8;
KeptAggro g_kept[kMaxKept];

bool KeptLive(const KeptAggro& k) { return k.index >= 0 && R::SlotSerial(k.index) == k.serial; }

KeptAggro* FindKept(void* octa) {
    const int32_t index = R::InternalIndexOf(octa);
    for (KeptAggro& k : g_kept)
        if (k.index == index && KeptLive(k)) return &k;
    return nullptr;
}

// A newer aggro of the same Octa replaces its kept one; an entry whose Octa has ended is the first to be reused.
void Keep(void* octa, const WireTarget& target, uint32_t base) {
    const int32_t index = R::InternalIndexOf(octa);
    const int32_t serial = index >= 0 ? R::AllocateSlotSerial(index) : 0;
    // A failure is counted always and said once per reason.
    static bool noSerialSaid = false, fullSaid = false;
    if (serial == 0) {
        ++g_counters.keepFailed;
        if (!noSerialSaid) {
            noSerialSaid = true;
            UE_LOGW("geom_octa_sync: an Octa's slot gave no serial -- its Aggro is dropped (counted; said once)");
        }
        return;
    }
    KeptAggro* slot = FindKept(octa);
    for (KeptAggro& k : g_kept) {
        if (slot) break;
        if (k.index < 0 || !KeptLive(k)) slot = &k;
    }
    if (!slot) {
        ++g_counters.keepFailed;
        if (!fullSaid) {
            fullSaid = true;
            UE_LOGW("geom_octa_sync: more than %zu Octas hold a kept Aggro -- this one is dropped (counted; said once)",
                    kMaxKept);
        }
        return;
    }
    *slot = KeptAggro{index, serial, target, base};
}

unsigned long long Now() { return static_cast<unsigned long long>(coop::net::NowMs()); }

// The argument `aggro` was called with, when live; null for None.
bool AggroArgument(const sg::Call& call, void*& arg) {
    if (!call.function || !call.locals) return false;
    const int32_t off = g_aggroArg.Of(call.function);
    const int32_t size = R::FunctionFrameSize(call.function);
    if (off < 0 || off > size || sizeof(arg) > static_cast<size_t>(size - off)) return false;
    arg = nullptr;
    std::memcpy(&arg, call.locals + off, sizeof(arg));
    return !arg || R::IsLive(arg);
}

sg::Verdict OnAggroPre(const sg::Call& call) {
    if (call.depth < 1 || call.depth > static_cast<int>(g_aggroEntries.size())) return sg::Verdict::Run;
    // A cancelled body has no POST. The next PRE at its depth replaces it.
    auto& entry = g_aggroEntries[call.depth - 1];
    entry = {};
    void* target = nullptr;
    if (!IsHost() || !call.invocation || !R::IsLive(call.object) ||
        !AggroArgument(call, target) || coop::npc_mirror_brain::IsMarkedMirror(call.object)) return sg::Verdict::Run;
    entry.octa.Set(call.object);
    entry.function.Set(call.function);
    entry.target.Set(target);
    entry.stack = call.stack;
    entry.invocation = call.invocation;
    entry.epoch = g_epoch;
    entry.hadTarget = GO::TargetActor(call.object) != nullptr;
    return sg::Verdict::Run;
}

struct Decision {
    struct Peer {uint32_t generation=0;bool pending=false,attempted=false;uint64_t at=0;};
    N::Session* session=g_session.load();
    uint64_t epoch=g_epoch;
    N::GeomOctaEventPayload p{};
    std::string nativeClass;
    ue_wrap::CachedObjRef actor,cls,target,nativeTarget;
    std::array<Peer,N::kMaxPeers> peers{};
    void* Target()const{
        if(p.targetKind==N::kGeomOctaTargetSlot){
            if(!p.target)return coop::players::Registry::Get().Local();
            auto* puppet=coop::players::Registry::Get().Puppet(uint8_t(p.target));return puppet?puppet->GetActor():nullptr;
        }
        const auto* element=EL::Registry::Get().Get(p.target);return element?element->LiveActor():nullptr;
    }
    bool Current()const{
        return session==g_session.load() && epoch==g_epoch && IsHost() && session->connected() &&
            actor.Get() && cls.Get() && R::ClassOf(actor.Raw())==cls.Raw() && NpcEid(actor.Raw())==p.octaEid &&
            EL::Registry::Get().Get(p.octaEid)->GetTypeName()==nativeClass &&
            GO::TargetActor(actor.Raw())==nativeTarget.Raw() && (!nativeTarget.Raw()||nativeTarget.Get()) &&
            (p.targetKind==N::kGeomOctaTargetNone || (target.Get()&&Target()==target.Raw()));
    }
};
std::vector<std::shared_ptr<Decision>> g_decisions;
struct BirthReceipt {
    uint32_t eid=0;
    ue_wrap::CachedObjRef actor,cls;
    std::array<uint32_t,N::kMaxPeers> generation{};
    std::array<uint64_t,N::kMaxPeers> worldRevision{};
    bool Current()const{return actor.Get()&&cls.Get()&&R::ClassOf(actor.Raw())==cls.Raw()&&NpcEid(actor.Raw())==eid;}
};
std::vector<BirthReceipt> g_births;
bool BirthKnown(const Decision& decision,int slot,uint32_t generation,uint64_t worldRevision){
    for(const auto& birth:g_births)if(birth.eid==decision.p.octaEid && birth.actor.Raw()==decision.actor.Raw() &&
        birth.cls.Raw()==decision.cls.Raw() && birth.Current() && generation && birth.generation[slot]==generation &&
        birth.worldRevision[slot]==worldRevision)return true;
    return false;
}
void NoteBirth(const Decision& decision,int slot,uint32_t generation,uint64_t worldRevision){
    g_births.erase(std::remove_if(g_births.begin(),g_births.end(),[](const auto& birth){return !birth.Current();}),g_births.end());
    for(auto& birth:g_births)if(birth.eid==decision.p.octaEid&&birth.actor.Raw()==decision.actor.Raw()&&birth.cls.Raw()==decision.cls.Raw()){
        birth.generation[slot]=generation;birth.worldRevision[slot]=worldRevision;return;
    }
    if(g_births.size()>=kMaxSent)g_births.erase(g_births.begin());
    BirthReceipt birth;birth.eid=decision.p.octaEid;birth.actor=decision.actor;birth.cls=decision.cls;
    birth.generation[slot]=generation;birth.worldRevision[slot]=worldRevision;g_births.push_back(birth);
}
bool CaptureBirth(const Decision& decision,N::EntitySpawnPayload& packet,const std::function<bool()>& current){
    ue_wrap::FVector location{},scale{};ue_wrap::FRotator rotation{};
    if(!current() || !ue_wrap::engine::TryGetActorTransform(decision.actor.Raw(),location,rotation,scale,current) || !current())return false;
    if(std::fabs(location.X)>N::kMaxCoord||std::fabs(location.Y)>N::kMaxCoord||std::fabs(location.Z)>N::kMaxCoord)return false;
    if(decision.nativeClass.empty()||decision.nativeClass.size()>63)return false;
    for(unsigned char c:decision.nativeClass)if(c<33||c>126)return false;
    packet={};packet.className.len=static_cast<uint8_t>(decision.nativeClass.size());
    std::memcpy(packet.className.data,decision.nativeClass.data(),decision.nativeClass.size());
    packet.elementId=decision.p.octaEid;
    packet.locX=location.X;packet.locY=location.Y;packet.locZ=location.Z;
    packet.rotPitch=rotation.Pitch;packet.rotYaw=rotation.Yaw;packet.rotRoll=rotation.Roll;
    packet.scaleX=scale.X;packet.scaleY=scale.Y;packet.scaleZ=scale.Z;
    if(!current())return false;
    packet.savePersisted=ue_wrap::kerfur::HasSaveKey(decision.actor.Raw())?1:0;
    return current();
}
bool g_drainingDecisions=false;
void DrainDecisions(){
    if(g_drainingDecisions)return;
    struct Busy{Busy(){g_drainingDecisions=true;}~Busy(){g_drainingDecisions=false;}}busy;
    const auto pending=g_decisions;size_t budget=32;
    std::array<bool,N::kMaxPeers> blocked{};
    for(const auto& decision:pending){
        const auto installed=[&]{return std::find(g_decisions.begin(),g_decisions.end(),decision)!=g_decisions.end();};
        if(!installed())continue;
        if(!decision->Current()){
            g_decisions.erase(std::remove(g_decisions.begin(),g_decisions.end(),decision),g_decisions.end());continue;
        }
        auto* session=decision->session;
        for(int slot=1;slot<N::kMaxPeers;++slot){
            auto& peer=decision->peers[slot];if(!peer.pending)continue;
            if(!session->IsSlotConnected(slot)||peer.generation!=session->peerGenerationForSlot(slot)){peer.pending=false;continue;}
            const auto now=N::NowMs();
            if(blocked[slot] || !session->IsSlotWorldReady(slot) || (peer.attempted&&(now<peer.at||now-peer.at<250))){blocked[slot]=true;continue;}
            if(!budget)return;--budget;
            peer.attempted=true;peer.at=now;
            const uint64_t worldRevision=session->SlotWorldReadyRevision(slot);
            if(!BirthKnown(*decision,slot,peer.generation,worldRevision)){
                N::EntitySpawnPayload birth{};
                const auto continuing=[&]{return installed()&&decision->Current()&&peer.pending&&
                    peer.generation==session->peerGenerationForSlot(slot)&&session->IsSlotWorldReady(slot)&&
                    session->SlotWorldReadyRevision(slot)==worldRevision;};
                if(!CaptureBirth(*decision,birth,continuing)){blocked[slot]=true;continue;}
                if(!installed()||!decision->Current())return;
                if(peer.generation!=session->peerGenerationForSlot(slot)||!session->IsSlotConnected(slot)){peer.pending=false;continue;}
                if(!session->IsSlotWorldReady(slot)||session->SlotWorldReadyRevision(slot)!=worldRevision){blocked[slot]=true;continue;}
                const bool born=session->SendReliableToSlot(slot,N::ReliableKind::EntitySpawn,&birth,sizeof birth);
                if(!installed()||!decision->Current())return;
                if(!peer.pending)continue;
                if(peer.generation!=session->peerGenerationForSlot(slot)||!session->IsSlotConnected(slot)){peer.pending=false;continue;}
                if(!born){++g_counters.sendFailed;blocked[slot]=true;continue;}
                if(session->SlotWorldReadyRevision(slot)!=worldRevision){blocked[slot]=true;continue;}
                NoteBirth(*decision,slot,peer.generation,worldRevision);
                if(!session->IsSlotWorldReady(slot)){blocked[slot]=true;continue;}
                if(!budget)return;--budget;
            }
            if(session->SlotWorldReadyRevision(slot)!=worldRevision||!session->IsSlotWorldReady(slot)){blocked[slot]=true;continue;}
            const auto packet=decision->p;
            const bool accepted=session->SendReliableToSlot(slot,N::ReliableKind::GeomOctaEvent,&packet,sizeof packet);
            if(!installed()||!decision->Current())return;
            if(session->SlotWorldReadyRevision(slot)!=worldRevision){blocked[slot]=true;continue;}
            if(peer.generation!=session->peerGenerationForSlot(slot)||!session->IsSlotConnected(slot)){peer.pending=false;continue;}
            if(accepted)peer.pending=false;
            else {++g_counters.sendFailed;blocked[slot]=true;}
        }
        bool remains=false;for(const auto& peer:decision->peers)remains|=peer.pending;
        if(!remains)g_decisions.erase(std::remove(g_decisions.begin(),g_decisions.end(),decision),g_decisions.end());
    }
}
bool SendEvent(const N::GeomOctaEventPayload& input, int slot = -1) {
    if(!IsHost()||!g_session.load()->connected()||slot==0||slot>=N::kMaxPeers||slot < -1)return false;
    auto decision=std::make_shared<Decision>();decision->p=input;
    const auto* element=EL::Registry::Get().Get(input.octaEid);
    if(!element)return false;
    decision->nativeClass=element->GetTypeName();
    decision->actor.Set(element->LiveActor());if(decision->actor.Get())decision->cls.Set(R::ClassOf(decision->actor.Raw()));
    if(decision->actor.Get())decision->nativeTarget.Set(GO::TargetActor(decision->actor.Raw()));
    if(input.targetKind!=N::kGeomOctaTargetNone)decision->target.Set(decision->Target());
    if(!decision->Current())return false;
    if(input.op==N::kGeomOctaOpAggro){
        for(auto& old:g_decisions)if(old->p.octaEid==input.octaEid)
            for(int target=1;target<N::kMaxPeers;++target)if(slot<0||target==slot)old->peers[target].pending=false;
    }
    g_decisions.erase(std::remove_if(g_decisions.begin(),g_decisions.end(),[](const auto& row){
        if(!row->Current())return true;for(const auto& peer:row->peers)if(peer.pending)return false;return true;
    }),g_decisions.end());
    if(input.op==N::kGeomOctaOpVerdict){
        size_t kept=0;
        for(size_t i=g_decisions.size();i>0;--i){
            const auto& old=g_decisions[i-1];
            if(old->p.octaEid==input.octaEid && old->p.op==N::kGeomOctaOpVerdict && ++kept>=4)
                g_decisions.erase(g_decisions.begin()+i-1);
        }
    }
    if(g_decisions.size()>=kMaxSent*(N::kMaxPeers+4)){++g_counters.sendFailed;return false;}
    bool audience=false;
    for(int target=1;target<N::kMaxPeers;++target)if((slot<0||target==slot)&&decision->session->IsSlotConnected(target)){
        auto& peer=decision->peers[target];peer.generation=decision->session->peerGenerationForSlot(target);
        peer.pending=peer.generation!=0;audience|=peer.pending;
    }
    if(!audience)return false;
    g_decisions.push_back(decision);DrainDecisions();return decision->Current();
}

void SendAggro(uint32_t eid, const WireTarget& target, uint32_t base, int slot = -1) {
    N::GeomOctaEventPayload p{};
    p.op = N::kGeomOctaOpAggro;
    p.octaEid = eid;
    p.verdictNo = base;
    p.targetKind = target.kind;
    p.target = target.v;
    char text[40];
    TargetText(target, text);
    if (SendEvent(p, slot)) {
        char join[24] = "";
        if (slot >= 0) std::snprintf(join, sizeof(join), " join slot=%d", slot);   // a joiner's snapshot, not a live send
        UE_LOGI("[OCTA] aggro queued eid=%u target=%s base=%u%s t=%llu", static_cast<unsigned>(eid), text,
                static_cast<unsigned>(base), join, Now());
    }
}

void SendVerdict(uint32_t eid, uint32_t n, int refire) {
    // [dev] octa_red=no_verdict, the attack arm's red: the host counts its verdicts and sends none, so the client's
    // refused notify waits for a verdict that never comes and its look times out.
    static const bool noVerdict = coop::config::ResolveEnum(::coop::config_registry::rows::octa_red) == "no_verdict";
    if (noVerdict) return;
    N::GeomOctaEventPayload p{};
    p.op = N::kGeomOctaOpVerdict;
    p.octaEid = eid;
    p.verdictNo = n;
    p.refire = static_cast<uint8_t>(refire);
    if (SendEvent(p))
        UE_LOGI("[OCTA] verdict queued eid=%u n=%u refire=%d t=%llu", static_cast<unsigned>(eid),
                static_cast<unsigned>(n), refire, Now());
}

void OnAggroPost(const sg::Call& call) {
    if (call.depth < 1 || call.depth > static_cast<int>(g_aggroEntries.size())) return;
    auto& pending = g_aggroEntries[call.depth - 1];
    if (!call.invocation || pending.invocation != call.invocation || pending.stack != call.stack ||
        pending.octa.Raw() != call.object || pending.function.Raw() != call.function) return;
    const auto entry = pending;
    pending = {};
    if (!IsHost() || entry.epoch != g_epoch || entry.hadTarget || !entry.octa.Get() ||
        !entry.function.Get() || (entry.target.Raw() && !entry.target.Get()) ||
        coop::npc_mirror_brain::IsMarkedMirror(call.object)) return;
    const WireTarget target = WireTargetOf(entry.target.Get());
    char text[40];
    TargetText(target, text);
    std::snprintf(g_lastTarget, sizeof(g_lastTarget), "%s", text);
    ++g_counters.hostAggro;
    const uint32_t base = PeekVerdicts(call.object);
    const EL::ElementId eid = NpcEid(call.object);
    if (eid == EL::kInvalidId) {
        Keep(call.object, target, base);   // not enrolled yet: OnNpcSpawnSent sends it with the spawn
    } else if (SpawnSent(static_cast<uint32_t>(eid))) {
        SendAggro(static_cast<uint32_t>(eid), target, base);
    }
    // An enrolled Octa whose spawn was not sent had no world-ready peer when it spawned: nothing to send to, and a
    // peer that becomes ready later is served by the join, not by this event.
}

sg::Verdict OnFireFinishPre(const sg::Call& call) {
    if (call.depth < 1 || call.depth > static_cast<int>(g_fireEntries.size())) return sg::Verdict::Run;
    auto& entry = g_fireEntries[call.depth - 1];
    entry = {};
    if (!IsHost() || !call.invocation || !R::IsLive(call.object) || !GO::IsFinishNotify(call) ||
        coop::npc_mirror_brain::IsMarkedMirror(call.object)) return sg::Verdict::Run;
    entry.octa.Set(call.object); entry.function.Set(call.function);
    entry.stack = call.stack; entry.invocation = call.invocation; entry.epoch = g_epoch;
    return sg::Verdict::Run;
}

void OnFireFinishPost(const sg::Call& call) {
    if (call.depth < 1 || call.depth > static_cast<int>(g_fireEntries.size())) return;
    auto& pending = g_fireEntries[call.depth - 1];
    if (!call.invocation || pending.invocation != call.invocation || pending.stack != call.stack ||
        pending.octa.Raw() != call.object || pending.function.Raw() != call.function) return;
    const auto entry = pending;
    pending = {};
    if (!IsHost() || entry.epoch != g_epoch || !entry.octa.Get() || !entry.function.Get() ||
        coop::npc_mirror_brain::IsMarkedMirror(call.object)) return;
    const EL::ElementId id = NpcEid(call.object);
    const uint32_t n = CountVerdict(call.object);
    const int refire = GO::TargetActor(call.object) ? 1 : 0;
    ++g_counters.hostVerdict;
    const int32_t slot = R::InternalIndexOf(call.object);
    const bool windowOcta = slot >= 0 && slot == g_windowIndex && R::SlotSerial(slot) == g_windowSerial;
    if (windowOcta && g_windowWanted > 0 && !g_counters.windowReached && ++g_windowCounted == g_windowWanted) {
        g_counters.windowReached = true;
        g_counters.idleAtWindow = g_counters.idle;
    }
    // Counted always, since the count is the host's truth; sent only for an Octa whose spawn was sent (a verdict comes
    // seconds after the first aggro, so a spawn not yet sent by then is a rarity, and the client times that look out).
    if (id != EL::kInvalidId && SpawnSent(static_cast<uint32_t>(id))) SendVerdict(static_cast<uint32_t>(id), n, refire);
}

void OnCloseFinishPost(const sg::Call& call) {
    char eid[16];
    EidText(NpcEid(call.object), eid);
    ++g_counters.idle;
    UE_LOGI("[OCTA] idle eid=%s t=%llu", eid, Now());
}

void OnLaserPost(const sg::Call& call) {
    char eid[16];
    EidText(NpcEid(GO::LaserOwner(call.object)), eid);
    ++g_counters.laser;
    g_counters.lastLaserMs = N::NowMs();
    UE_LOGI("[OCTA] laser eid=%s t=%llu", eid, static_cast<unsigned long long>(g_counters.lastLaserMs));
}

// ---- [dev] the overlap probe ----
//
// While the Octa drill's measure or attack arm is on, the host's Octa reports each begin-overlap handler that fires: once per
// handler and class of the other actor, with the handler's running total in the counters. It only observes.

bool ProbeOn() {
    static const bool on = [] {
        const std::string arm = coop::config::ResolveEnum(::coop::config_registry::rows::octa_drill);
        return arm == "measure" || arm == "attack" || arm == "join";
    }();
    return on;
}

struct SeenOverlap {
    int   which;   // 0 minSphere, 1 propAggroSphere
    void* cls;     // compared, never read through
};
constexpr size_t kMaxSeen = 16;
SeenOverlap g_seen[kMaxSeen];
size_t      g_seenCount = 0;
R::ParamOffset g_overlapOther{L"OtherActor"};

sg::Verdict NoteOverlap(const sg::Call& call, int which) {
    if (!IsHost() || coop::npc_mirror_brain::IsMarkedMirror(call.object)) return sg::Verdict::Run;
    const uint32_t total = which == 0 ? ++g_counters.overlapMin : ++g_counters.overlapProp;
    const int32_t off = g_overlapOther.Of(call.function);
    void* other = nullptr;
    const int32_t size = R::FunctionFrameSize(call.function);
    if (off >= 0 && off <= size && sizeof(other) <= static_cast<size_t>(size - off) && call.locals)
        std::memcpy(&other, call.locals + off, sizeof(other));
    if (other && !R::IsLive(other)) other = nullptr;
    void* cls = other ? R::ClassOf(other) : nullptr;
    for (size_t i = 0; i < g_seenCount; ++i)
        if (g_seen[i].which == which && g_seen[i].cls == cls) return sg::Verdict::Run;
    if (g_seenCount < kMaxSeen) g_seen[g_seenCount++] = SeenOverlap{which, cls};
    UE_LOGI("[OCTA-PROBE] host overlap fn=%s other=%ls total=%u", which == 0 ? "minSphere" : "propAggroSphere",
            other ? R::ClassNameOf(other).c_str() : L"none", static_cast<unsigned>(total));
    return sg::Verdict::Run;
}
sg::Verdict OnMinSphereOverlapPre(const sg::Call& call) { return NoteOverlap(call, 0); }
sg::Verdict OnPropSphereOverlapPre(const sg::Call& call) { return NoteOverlap(call, 1); }

// The entry each watched graph call began with, by the call's depth. The graph's EntryPoint sits in the actor's
// persistent frame, which every nested entry writes: BeginPlay's entry calls `move()`, whose nested entry (refused on a
// mirror) still leaves 5538 there, so BeginPlay's own POST would read 5538. The PRE of each call keeps its own entry
// under its depth, and the POST reads it back; a depth past the table is not recorded and its POST is skipped.
constexpr int kMaxDepth = 32;
thread_local int32_t t_entryAtDepth[kMaxDepth];

int32_t EntryOf(const sg::Call& call) {
    if (!call.function || !call.locals) return -1;
    const int32_t off = g_graphEntry.Of(call.function);
    const int32_t size = R::FunctionFrameSize(call.function);
    int32_t entry = -1;
    if (off >= 0 && off <= size && sizeof(entry) <= static_cast<size_t>(size - off))
        std::memcpy(&entry, call.locals + off, sizeof(entry));
    return entry;
}

sg::Verdict OnGraphPre(const sg::Call& call) {
    if (call.depth >= 1 && call.depth <= kMaxDepth) t_entryAtDepth[call.depth - 1] = EntryOf(call);
    return sg::Verdict::Run;
}

// After an Octa's event graph ran an entry: a post fires only for a body that ran, so this sees the wander on a
// marked mirror exactly when npc_mirror_brain did not park it.
void OnGraphPost(const sg::Call& call) {
    if (call.depth < 1 || call.depth > kMaxDepth) return;
    const int32_t entry = t_entryAtDepth[call.depth - 1];
    if (entry != kMoveEntry || !coop::npc_mirror_brain::IsMarkedMirror(call.object)) return;
    char eid[16];
    EidText(NpcEid(call.object), eid);
    ++g_counters.mirrorMove;
    UE_LOGI("[OCTA] mirror ran move eid=%s", eid);
}

}  // namespace

void Install(coop::net::Session* session) {
    const auto world = ue_wrap::world_identity::Generation();
    const auto serial=N::session_serial::Current();
    if (g_session.load(std::memory_order_acquire) != session || g_world != world || g_serial!=serial ||
        (session && (g_stops!=session->StopCount() || g_role!=session->role()))) {
        OnDisconnect();
        g_world = world;
    }
    g_session.store(session, std::memory_order_release);
    g_serial=serial;g_stops=session?session->StopCount():0;
    g_role=session?session->role():N::Role::Host;
    DrainDecisions();
    if (g_watched) return;
    // Each watch registers once; one that is refused is said once and asked again on the next call, which the install
    // fanout makes every pump tick, until all five hold.
    struct Watch {
        const ue_wrap::facts::Function* row;
        int                             tag;
        sg::PreFn                       pre;
        sg::PostFn                      post;
        bool                            probeOnly = false;   // registered only while the drill's measure or attack arm is on
        bool                            registered = false;
        bool                            refusedSaid = false;
    };
    static Watch watches[] = {
        {&kRowAggro, kTagAggro, &OnAggroPre, &OnAggroPost},
        {&kRowFireFinish, kTagFire, &OnFireFinishPre, &OnFireFinishPost},
        {&kRowCloseFinish, kTagClose, nullptr, &OnCloseFinishPost},
        {&kRowLaserBeginPlay, kTagLaser, nullptr, &OnLaserPost},
        {&kRowOctaGraph, kTagGraph, &OnGraphPre, &OnGraphPost},
        {&kRowMinSphereOverlap, kTagMinSphere, &OnMinSphereOverlapPre, nullptr, true},
        {&kRowPropSphereOverlap, kTagPropSphere, &OnPropSphereOverlapPre, nullptr, true},
    };
    bool all = true;
    for (Watch& w : watches) {
        if (w.probeOnly && !ProbeOn()) continue;
        if (!w.registered) w.registered = sg::WatchName(*w.row, w.tag, w.pre, w.post);
        if (!w.registered && !w.refusedSaid) {
            w.refusedSaid = true;
            UE_LOGE("geom_octa_sync: the watch on %ls::%ls did not register -- retried; its [OCTA] line is not "
                    "printed until it does", w.row->Class(), w.row->Name());
        }
        all = all && w.registered;
    }
    if (!all) return;
    g_watched = true;
    UE_LOGI("geom_octa_sync: the watches are registered by class and name, each live once the class loads%s",
            ProbeOn() ? " (with the overlap probe)" : "");
}

Counters GetCounters() { return g_counters; }

void OnNpcSpawnSent(uint32_t eid, void* actor, const char* className, size_t classLen, bool sent, int slot) {
    // Every NPC spawn send passes here: the class is told by its bytes, with no string made.
    constexpr char kOctaName[] = "geomOcta_C";
    if (!className || classLen != sizeof(kOctaName) - 1 || std::memcmp(className, kOctaName, classLen) != 0 || !IsHost())
        return;
    KeptAggro* kept = actor ? FindKept(actor) : nullptr;
    if (!sent) {
        // No peer was world-ready, so the spawn went nowhere and neither does its Aggro: a peer that becomes ready later
        // is served by the join's re-send.
        ForgetSent(eid);
        if (kept) *kept = KeptAggro{};
        return;
    }
    NoteSent(eid);
    // A joiner's snapshot (slot >= 0) of an Octa MID-ATTACK on a target: one Aggro to that slot, the target as the wire
    // carries it now and the verdict count now as the base. The joiner's mirror starts the chain from its beginning, and
    // its first refused fire-finish pairs with base + 1, the host's next verdict, a fire in flight included. It replaces
    // a kept Aggro (one Aggro per Octa per slot). An Octa between attacks, or on a target the wire cannot name (None),
    // sends nothing here: the joiner's next Aggro is the host's next one, and a None attack in flight idles on the
    // joiner while its blast still arrives. The spawn was sent just before, on the same lane, so the event follows it.
    // MTA joins a ped by a phase snapshot (animation block, elapsed time):
    // reference/mtasa-blue/Server/mods/deathmatch/logic/CEntityAddPacket.cpp:954-975. A Blueprint timeline cannot be
    // seeked, so the decision is replayed here and the joiner's own graph runs it from its start.
    if (slot >= 0 && actor) {
        void* target = GO::TargetActor(actor);
        const WireTarget wt = WireTargetOf(target);
        if (target && wt.kind != N::kGeomOctaTargetNone) {
            ++g_counters.joinMid;
            if (kept) *kept = KeptAggro{};
            // [dev] octa_red=no_join, the join arm's red: the mid-attack branch sends nothing.
            static const bool noJoin = coop::config::ResolveEnum(::coop::config_registry::rows::octa_red) == "no_join";
            if (!noJoin) SendAggro(eid, wt, PeekVerdicts(actor), slot);
            return;
        }
        ++g_counters.joinBetween;
    }
    if (!kept) return;
    const KeptAggro k = *kept;
    *kept = KeptAggro{};
    // The attack may have ended while the spawn waited; then there is nothing to replay. A snapshot's send carries the
    // spawn to its slot alone, so the Aggro goes there alone: another joiner whose snapshot has not run would otherwise
    // get the event ahead of its spawn and drop it.
    if (GO::TargetActor(actor) != nullptr) SendAggro(eid, k.target, k.base, slot);
}

void OpenVerdictWindow(uint32_t n, void* octa) {
    const int32_t index = octa ? R::InternalIndexOf(octa) : -1;
    const int32_t serial = index >= 0 ? R::AllocateSlotSerial(index) : 0;
    g_windowWanted = serial != 0 ? n : 0;   // an Octa that cannot be told apart opens no window
    g_windowIndex = index;
    g_windowSerial = serial;
    g_windowCounted = 0;
    g_counters.windowReached = false;
    g_counters.idleAtWindow = 0;
}

const char* LastAggroTarget() { return g_lastTarget; }

void OnDisconnect() {
    ++g_epoch;
    g_session.store(nullptr, std::memory_order_release);
    g_world = 0;
    g_serial=0;g_stops=0;g_decisions.clear();g_births.clear();
    for (VerdictCount& v : g_verdicts) v = VerdictCount{};
    g_verdictNext = 0;
    for (KeptAggro& k : g_kept) k = KeptAggro{};
    g_sentCount = 0;
    g_counters = Counters{};
    g_windowWanted = g_windowCounted = 0;
    g_windowIndex = -1;
    g_windowSerial = 0;
    g_seenCount = 0;
    g_lastTarget[0] = '\0';
    g_aggroEntries.fill({});
    g_fireEntries.fill({});
    coop::geom_octa_relay::OnDisconnect();   // the client's half of the lane ends with the session too
}

}  // namespace coop::geom_octa_sync
