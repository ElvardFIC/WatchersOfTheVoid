// coop/creatures/piramid_sync.cpp -- see coop/creatures/piramid_sync.h. The pyramid-event
// choreography lane: client-mirror brain suppression and the host-to-client gather relay.
// Everything else about the pyramid rides the generic rails: the world-actor pose mirror, the
// NPC lane for the wisps, the event replay verdict, the native event registry parity.

#include "coop/creatures/piramid_sync.h"

#include "coop/element/element.h"
#include "coop/element/mirror_managers.h"  // WaMirrors() / NpcMirrors()
#include "coop/element/npc.h"
#include "coop/element/registry.h"
#include "coop/element/world_actor.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/facts/function_row.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace coop::piramid_sync {
namespace {

namespace R  = ue_wrap::reflection;
namespace E  = ue_wrap::engine;
namespace GT = ue_wrap::game_thread;

constexpr const wchar_t* kPyramidClassName = L"piramid2_C";
UE_FACT_FN(kRowSeeWisps, "piramid_sync", L"piramid2_C", L"seeWisps", Declared);
UE_FACT_FN(kRowCheckIfReached, "piramid_sync", L"piramid2_C", L"checkIfReached", Declared);
UE_FACT_FN(kRowRandLoc, "piramid_sync", L"piramid2_C", L"randLoc", Declared);
UE_FACT_FN(kRowChangeLook, "piramid_sync", L"piramid2_C", L"changeLook", Declared);
constexpr const char*    kPyramidTypeName  = "piramid2_C";

// Match the native planar-distance threshold and float arithmetic before
// admitting the one-shot native gather call.
constexpr float kReplayAttemptRadius = 10000.0f;
// A gather relay that cannot converge (the mirror missing, the interpolation never closing) is
// dropped loudly after this window; the wisp's death still arrives through the NPC lane's
// destroy.
constexpr long long kReplayDeadlineMs = 5000;

std::atomic<coop::net::Session*> g_session{nullptr};
inline coop::net::Session* LoadSession() { return g_session.load(std::memory_order_acquire); }
uint64_t g_epoch=0,g_pendingRevision=0;
uint32_t g_world=0;
bool g_ticking=false;
bool Current(coop::net::Session* session) {
    return session&&session==LoadSession()&&session->running()&&session->connected()&&
        g_world==ue_wrap::world_identity::Generation();
}

// Lazy hook arming, one-shot and latched. The pyramid class loads only when the event chain
// spawns it, so resolution is gated on a pyramid world-actor Element existing (host: allocated
// at the deferred spawn; client: materialised from the wire). A member miss on the then-loaded
// class can never heal (name drift on a future game version), so one attempt, latched loudly.
bool g_armed = false;
bool g_armFailedLatched = false;
std::atomic<bool> g_armedAtomic{false};

void* g_fnSeeWisps = nullptr;
void* g_fnCheckIfReached = nullptr;
void* g_fnRandLoc = nullptr;
void* g_fnChangeLook = nullptr;  // the 1 Hz random head-wander re-roll, suppressed on mirrors
int32_t g_offWispTarget = -1;
int32_t g_isWalkingOff = -1;
uint8_t g_isWalkingMask = 0;
int32_t g_gatheringOff = -1;
uint8_t g_gatheringMask = 0;
// The facing axis: the pyramid's visible heading lives in the world rotation of its two arrow
// components (the actor root never yaws; the animation orients the body off the component).
// The host's turning step interpolates both components; the mirror's brain is suppressed, so
// nothing turns them and it would face its spawn heading forever.
int32_t g_offMovementVector = -1;
int32_t g_offArrow = -1;
int32_t g_offRelLook = -1;  // the head axis: the look-at's idle target vector

// The thread-local allow slot for the client gather replay: our own re-dispatch of the
// reached-check must pass the brain-suppress interceptor; thread-local because the whole
// stage-and-call sequence is one inline game-thread block.
struct AllowCheck {
    ue_wrap::CachedObjRef actor,function;
    void* params=nullptr;
    uint64_t epoch=0;
    bool used=false;
};
thread_local AllowCheck t_allowCheck;

// The host gather edge detector: pyramid actor to its last-seen gathering flag (game thread
// only; the reached-check dispatch is timer-fired). Swept at 1 Hz against the live world-actor
// set, so a recycled heap address cannot inherit a stale true and eat a fresh pyramid's first
// relay.
struct GatherEdge {ue_wrap::CachedObjRef actor;bool gathering=false;};
std::unordered_map<void*, GatherEdge> g_lastGathering;

// The client's pending gather, a single slot: the host serialises gathers behind its own
// gathering latch, so a newer relay legitimately supersedes an unconverged older one.
struct PendingGather {
    uint32_t pyramidEid = 0;
    uint32_t wispEid = 0;
    long long deadlineMs = 0;
    bool active = false;
    float lastDist = -1.0f;  // last measured mirror 2D dist (deadline diagnostics)
    int attempts = 0;        // dispatch attempts (branch-not-taken retries)
    ue_wrap::CachedObjRef pyramid,wisp;
    uint64_t revision=0;
};
PendingGather g_pending;
PendingGather g_wispTick;

// Client: pyramid mirror eids whose actor tick we restored. The world-actor sync parks generic
// mirrors tick-off, but the pyramid's tick carries the beam params, the look-at and the hover
// smoothing.
std::unordered_map<uint32_t,ue_wrap::CachedObjRef> g_tickRestored;

// The facing axis streams as the pose snapshot's auxiliary yaw, the host's true component
// heading at pose cadence. Deriving it from position deltas was refuted live: the native
// heading keeps easing toward the walk target for up to 10 s after motion stops, which no
// motion delta can observe.

std::atomic<int> g_relayCount{0};

long long g_lastProbeMs = 0;   // 250 ms pre-arm probe / client restore-scan throttle
long long g_lastSweepMs = 0;   // 1 s host edge-map sweep throttle

long long NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

bool ReadBoolAt(void* obj, int32_t off, uint8_t mask) {
    return (*(reinterpret_cast<uint8_t*>(obj) + off) & mask) != 0;
}
void WriteBoolAt(void* obj, int32_t off, uint8_t mask, bool v) {
    uint8_t& b = *(reinterpret_cast<uint8_t*>(obj) + off);
    if (v) b |= mask; else b &= static_cast<uint8_t>(~mask);
}

// Brain suppression on client mirrors: a PRE interceptor on the state-writing timer handlers
// (every walk-to caller). Any client-side pyramid is a mirror, since the client scheduler is
// dormant and the world-actor sync suppresses local allowlisted spawns, so a per-role gate is
// the per-self gate for this class.
bool BrainSuppress_Interceptor(void* /*self*/, void* /*params*/) {
    auto* s = LoadSession();
    if (!GT::IsGameThread() || !Current(s)) return false;
    if (s->role() != coop::net::Role::Client) return false;  // host brain is authoritative
    return true;  // cancel the mirror's brain step
}

bool CheckSuppress_Interceptor(void* self,void* params) {
    if(!BrainSuppress_Interceptor(self,params))return false;
    if(!t_allowCheck.used&&t_allowCheck.epoch==g_epoch&&t_allowCheck.actor.Get()==self&&
        t_allowCheck.function.Get()==g_fnCheckIfReached&&t_allowCheck.params==params) {
        t_allowCheck.used=true;return false;
    }
    return true;
}

void* PyramidOf(uint32_t eid,bool mirror) {
    if(!coop::element::Registry::IsAllowedHostAllocatedEid(eid))return nullptr;
    auto* el=coop::element::WaMirrors().Get(eid);
    return el&&el->IsMirror()==mirror&&el->GetTypeName()==kPyramidTypeName?el->LiveActor():nullptr;
}
void* WispOf(uint32_t eid,bool mirror) {
    if(!coop::element::Registry::IsAllowedHostAllocatedEid(eid))return nullptr;
    auto* el=coop::element::NpcMirrors().Get(eid);
    return el&&el->IsMirror()==mirror&&el->GetTypeName()=="killerwisp_C"?el->LiveActor():nullptr;
}

// Host actor to lane identity: linear snapshots over the live elements, called only on a
// gather commit edge, once per consumed wisp.
uint32_t FindWaEidForActor(void* actor) {
    if(!actor)return 0;
    std::vector<coop::element::WorldActor*> snap;
    coop::element::WaMirrors().Snapshot(snap);
    for (auto* el : snap)
        if (el && !el->IsMirror() && el->GetTypeName()==kPyramidTypeName && el->LiveActor() == actor)
            return static_cast<uint32_t>(el->GetId());
    return 0;
}
uint32_t FindNpcEidForActor(void* actor) {
    if(!actor)return 0;
    std::vector<coop::element::Npc*> snap;
    coop::element::NpcMirrors().Snapshot(snap);
    for (auto* el : snap)
        if (el && !el->IsMirror() && el->GetTypeName()=="killerwisp_C" && el->LiveActor() == actor &&
            coop::element::Registry::IsAllowedHostAllocatedEid(el->GetId()))return static_cast<uint32_t>(el->GetId());
    return 0;
}

// The host gather detect: a POST observer on the reached-check (a 1 Hz timer dispatch, game
// thread). The gather commit is inside the function body (walking cleared, the wisp's gather,
// gathering set, the montage), so the POST reads the just-written flag and edge-detects its
// rise. The montage-completed path resets it off this seam; the next dispatch's POST records
// the falling edge (no relay) and re-arms the detector for the next wisp.
void CheckIfReached_POST(void* self, void* /*function*/, void* /*params*/) {
    if (!self) return;
    auto* s = LoadSession();
    if (!GT::IsGameThread() || !Current(s) || s->role() != coop::net::Role::Host || !R::IsLive(self)) return;
    if (g_gatheringOff < 0 || g_offWispTarget < 0) return;
    const bool gathering = ReadBoolAt(self, g_gatheringOff, g_gatheringMask);
    auto& edge = g_lastGathering[self];
    if(!edge.actor.Is(self)){edge={};edge.actor.Set(self);}
    if (gathering == edge.gathering) return;
    edge.gathering = gathering;
    if (!gathering) return;  // falling edge: gather finished -- nothing to relay
    void* wisp = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(self) + g_offWispTarget);
    if (!wisp) {
        UE_LOGW("piramid-gather[host]: gathering rose with null wispTarget -- not relaying");
        return;
    }
    const uint32_t pyramidEid = FindWaEidForActor(self);
    const uint32_t wispEid = FindNpcEidForActor(wisp);
    if (pyramidEid == 0 || wispEid == 0) {
        UE_LOGW("piramid-gather[host]: identity unresolved (pyramidEid=%u wispEid=%u) -- not "
                "relaying (unmirrored wisp? WA element raced?)", pyramidEid, wispEid);
        return;
    }
    coop::net::PyramidGatherPayload p{};
    p.pyramidEid = pyramidEid;
    p.wispEid = wispEid;
    const auto epoch=g_epoch;
    const bool sent=s->SendReliable(coop::net::ReliableKind::PyramidGather, &p, sizeof(p));
    if(epoch!=g_epoch||!Current(s)||s->role()!=coop::net::Role::Host)return;
    if (sent) {
        g_relayCount.fetch_add(1, std::memory_order_relaxed);
        UE_LOGI("piramid-gather[host]: relayed gather pyramidEid=%u wispEid=%u", pyramidEid, wispEid);
    } else {
        UE_LOGW("piramid-gather[host]: SendReliable(PyramidGather) FAILED (pyramidEid=%u wispEid=%u)",
                pyramidEid, wispEid);
    }
}

// Arming.
bool AnyPyramidElementExists() {
    std::vector<coop::element::WorldActor*> snap;
    coop::element::WaMirrors().Snapshot(snap);
    for (auto* el : snap)
        if (el && el->GetTypeName() == kPyramidTypeName) return true;
    return false;
}

void TryArmHooks() {
    if (g_armed || g_armFailedLatched) return;
    if (!AnyPyramidElementExists()) return;  // class not needed yet -- no GUObjectArray walk
    // A class not yet in the index, or still being loaded, is asked for again next time, at the cost of
    // one lookup; only a loaded class that lacks a member below, or a hook table with no room, disables
    // the lane.
    void* cls = ue_wrap::object_index::ClassByName(kPyramidClassName);
    if (!cls) return;
    const ue_wrap::facts::Resolved seeWisps = ue_wrap::facts::ResolveOn(kRowSeeWisps, cls);
    const ue_wrap::facts::Resolved checkIfReached = ue_wrap::facts::ResolveOn(kRowCheckIfReached, cls);
    const ue_wrap::facts::Resolved randLoc = ue_wrap::facts::ResolveOn(kRowRandLoc, cls);
    const ue_wrap::facts::Resolved changeLook = ue_wrap::facts::ResolveOn(kRowChangeLook, cls);
    g_fnSeeWisps = seeWisps.Fn();
    g_fnCheckIfReached = checkIfReached.Fn();
    g_fnRandLoc = randLoc.Fn();
    g_fnChangeLook = changeLook.Fn();
    g_offWispTarget = R::FindPropertyOffset(cls, L"wispTarget");
    // The facing axis, auxiliary: a miss disables only the heading drive, not the lane. The two
    // heading arrow components the host's turning step rotates.
    g_offMovementVector = R::FindPropertyOffset(cls, L"movementVector");
    g_offArrow = R::FindPropertyOffset(cls, L"Arrow");
    if (g_offMovementVector < 0)
        UE_LOGW("piramid-brain: movementVector offset unresolved -- client mirror facing will stay "
                "at spawn heading (name drift?)");
    // The head axis, auxiliary like the facing pair: a miss disables only the head stream.
    g_offRelLook = R::FindPropertyOffset(cls, L"relLook");
    if (g_offRelLook < 0)
        UE_LOGW("piramid-brain: relLook offset unresolved -- the mirror head keeps its own "
                "wander (name drift?)");
    const bool boolsOk =
        R::FindBoolProperty(cls, L"isWalking", g_isWalkingOff, g_isWalkingMask) &&
        R::FindBoolProperty(cls, L"gathering", g_gatheringOff, g_gatheringMask);
    if (!g_fnSeeWisps || !g_fnCheckIfReached || !g_fnRandLoc || !g_fnChangeLook ||
        g_offWispTarget < 0 || !boolsOk) {
        g_armFailedLatched = true;
        UE_LOGE("piramid-brain: member resolve FAILED (seeWisps=%p checkIfReached=%p randLoc=%p "
                "changeLook=%p wispTarget=%d bools=%d) -- lane DISABLED for process (name drift?)",
                g_fnSeeWisps, g_fnCheckIfReached, g_fnRandLoc, g_fnChangeLook, g_offWispTarget,
                boolsOk ? 1 : 0);
        return;
    }
    // Register all or disable: a partial set would half-suppress the mirror brain, worse than
    // none; seeWisps alive with the reached-check dead latches walking forever.
    const bool i1 = GT::RegisterInterceptor(seeWisps, &BrainSuppress_Interceptor);
    const bool i2 = i1 && GT::RegisterInterceptor(checkIfReached, &CheckSuppress_Interceptor);
    const bool i3 = i2 && GT::RegisterInterceptor(randLoc, &BrainSuppress_Interceptor);
    // changeLook joins the suppressed set: its 1 Hz random look re-roll is the head divergence,
    // and the streamed host vector is the one writer on a mirror.
    const bool i4 = i3 && GT::RegisterInterceptor(changeLook, &BrainSuppress_Interceptor);
    const bool o1 = i4 && GT::RegisterPostObserver(checkIfReached, &CheckIfReached_POST);
    if (!o1) {
        if (i1) GT::UnregisterInterceptor(g_fnSeeWisps, &BrainSuppress_Interceptor);
        if (i2) GT::UnregisterInterceptor(g_fnCheckIfReached, &CheckSuppress_Interceptor);
        if (i3) GT::UnregisterInterceptor(g_fnRandLoc, &BrainSuppress_Interceptor);
        if (i4) GT::UnregisterInterceptor(g_fnChangeLook, &BrainSuppress_Interceptor);
        g_armFailedLatched = true;
        UE_LOGE("piramid-brain: hook table FULL (interceptors %d/%d/%d/%d, observer %d) -- lane "
                "DISABLED for process + rolled back", i1, i2, i3, i4, o1);
        return;
    }
    g_armed = true;
    g_armedAtomic.store(true, std::memory_order_release);
    UE_LOGI("piramid-brain: armed -- 4 brain interceptors (seeWisps/checkIfReached/randLoc/"
            "changeLook) + checkIfReached POST (gather edge); wispTarget@%d isWalking@%d/%02x "
            "gathering@%d/%02x relLook@%d",
            g_offWispTarget, g_isWalkingOff, g_isWalkingMask, g_gatheringOff, g_gatheringMask,
            g_offRelLook);
}

// Client mirror upkeep. A new pyramid mirror: undo anything its brain wrote in the pre-arm
// window (up to 250 ms, in which a seeWisps could have latched walking and a target before the
// interceptors existed), then restore its actor tick (beams, look-at, hover; the march stays
// zero with walking cleared).
void RestoreNewClientMirrors() {
    auto* session=LoadSession();const auto epoch=g_epoch;
    std::vector<coop::element::Registry::ActorIdPair> snap;
    coop::element::Registry::Get().SnapshotActorsByType(coop::element::ElementType::WorldActor,snap);
    for(const auto pair:snap) {
        void* actor=PyramidOf(pair.id,true);if(!actor||actor!=pair.actor)continue;
        auto found=g_tickRestored.find(pair.id);
        if(found!=g_tickRestored.end()&&found->second.Get()==actor)continue;
        ue_wrap::CachedObjRef ref;ref.Set(actor);
        if(!ReadBoolAt(actor,g_gatheringOff,g_gatheringMask)) {
            WriteBoolAt(actor,g_isWalkingOff,g_isWalkingMask,false);
            *reinterpret_cast<void**>(static_cast<uint8_t*>(actor)+g_offWispTarget)=nullptr;
        }
        const bool restored=E::SetActorTickEnabled(actor,true);
        if(epoch!=g_epoch||!Current(session)||session->role()!=coop::net::Role::Client)return;
        if(restored&&ref.Get()&&PyramidOf(pair.id,true)==actor)g_tickRestored[pair.id]=ref;
    }
    std::erase_if(g_tickRestored,[](const auto& row){return !row.second.Get()||PyramidOf(row.first,true)!=row.second.Raw();});
}

// Attempt the queued gather replay; retried every tick until converged or the deadline.
void TryWispTick() {
    if(!g_wispTick.active)return;
    const auto row=g_wispTick;const auto epoch=g_epoch;auto* session=LoadSession();
    void* wisp=row.wisp.Get();
    if(!Current(session)||session->role()!=coop::net::Role::Client||!row.pyramid.Get()||!wisp||
        PyramidOf(row.pyramidEid,true)!=row.pyramid.Raw()||WispOf(row.wispEid,true)!=wisp) {
        g_wispTick.active=false;return;
    }
    int32_t off=-1;uint8_t mask=0;
    if(!R::FindBoolProperty(R::ClassOf(wisp),L"gathered",off,mask)||off<0||!mask)return;
    if(!ReadBoolAt(wisp,off,mask)){g_wispTick.active=false;return;}
    const bool applied=E::SetActorTickEnabled(wisp,true);
    if(epoch==g_epoch&&Current(session)&&g_wispTick.revision==row.revision&&applied)g_wispTick.active=false;
}

void TryReplayPendingGather() {
    const auto entryEpoch=g_epoch;
    TryWispTick();
    if(entryEpoch!=g_epoch)return;
    if(!g_pending.active)return;
    if(NowMs()>g_pending.deadlineMs){g_pending.active=false;return;}
    auto row=g_pending;auto* session=LoadSession();const auto epoch=g_epoch;
    void* pyr=PyramidOf(row.pyramidEid,true);void* wisp=WispOf(row.wispEid,true);
    if((row.pyramid.Raw()&&row.pyramid.Get()!=pyr)||(row.wisp.Raw()&&row.wisp.Get()!=wisp)) {
        g_pending.active=false;return;
    }
    if(!pyr||!wisp)return;
    if(!row.pyramid.Raw()){g_pending.pyramid.Set(pyr);row.pyramid=g_pending.pyramid;}
    if(!row.wisp.Raw()){g_pending.wisp.Set(wisp);row.wisp=g_pending.wisp;}
    auto bound=[&] {
        return epoch==g_epoch&&Current(session)&&session->role()==coop::net::Role::Client&&
            row.pyramid.Get()&&row.wisp.Get()&&PyramidOf(row.pyramidEid,true)==pyr&&WispOf(row.wispEid,true)==wisp;
    };
    auto current=[&]{return bound()&&g_pending.revision==row.revision&&g_pending.active;};
    if(ReadBoolAt(pyr,g_gatheringOff,g_gatheringMask)){g_pending.active=false;return;}
    ue_wrap::FVector pl{},wl{};
    if(!E::TryGetActorLocation(pyr,pl)||!current())return;
    if(!E::TryGetActorLocation(wisp,wl)||!current())return;
    auto finite=[](const ue_wrap::FVector& v){return std::isfinite(v.X)&&std::isfinite(v.Y)&&std::isfinite(v.Z);};
    if(!finite(pl)||!finite(wl))return;
    const float dx=pl.X-wl.X,dy=pl.Y-wl.Y;
    const float distance=std::sqrt(dx*dx+dy*dy);
    if(!std::isfinite(distance)||distance>kReplayAttemptRadius)return;
    g_pending.lastDist=static_cast<float>(distance);
    ue_wrap::CachedObjRef function;function.Set(g_fnCheckIfReached);
    ue_wrap::ParamFrame frame(function.Get());if(!frame.valid()||!current())return;
    void** target=reinterpret_cast<void**>(static_cast<uint8_t*>(pyr)+g_offWispTarget);
    void* oldTarget=*target;const bool oldWalking=ReadBoolAt(pyr,g_isWalkingOff,g_isWalkingMask);
    ue_wrap::CachedObjRef previousTarget;
    if(oldTarget&&R::IsLive(oldTarget))previousTarget.Set(oldTarget);
    *target=wisp;WriteBoolAt(pyr,g_isWalkingOff,g_isWalkingMask,true);
    auto cleanup=[&] {
        if(epoch!=g_epoch||!Current(session)||!row.pyramid.Get()||PyramidOf(row.pyramidEid,true)!=pyr)return;
        if(!ReadBoolAt(pyr,g_gatheringOff,g_gatheringMask)&&*target==wisp) {
            *target=previousTarget.Get();WriteBoolAt(pyr,g_isWalkingOff,g_isWalkingMask,oldWalking);
        }
    };
    struct Cleanup {decltype(cleanup)& run;~Cleanup(){run();}} cleanupGuard{cleanup};
    struct AllowScope {
        AllowCheck previous=t_allowCheck;
        AllowScope(void* actor,void* function,void* params) {
            t_allowCheck={};t_allowCheck.actor.Set(actor);t_allowCheck.function.Set(function);
            t_allowCheck.params=params;t_allowCheck.epoch=g_epoch;
        }
        ~AllowScope(){t_allowCheck=previous;}
    };
    bool admitted=false;
    g_pending.active=false;
    {
        AllowScope allow(pyr,function.Get(),frame.data());
        ue_wrap::Call(pyr,frame);
        admitted=t_allowCheck.used;
    }
    if(bound()&&g_pending.revision==row.revision) {
        ++g_pending.attempts;
        g_pending.active=!admitted;
    }
    if(!bound()||!function.Get())return;
    const bool gathering=ReadBoolAt(pyr,g_gatheringOff,g_gatheringMask);
    if(!gathering)return;
    if(!admitted)return;
    int32_t off=-1;uint8_t mask=0;
    if(!R::FindBoolProperty(R::ClassOf(wisp),L"gathered",off,mask)||off<0||!mask||!bound()||
        !ReadBoolAt(wisp,off,mask))return;
    g_wispTick=row;g_wispTick.active=true;
    TryWispTick();
}

// Host, 1 Hz: drop edge-map entries whose actor is no longer a tracked pyramid (the event
// ended); a recycled address must not inherit a stale gathering flag.
void SweepStaleEdgeEntries() {
    if (g_lastGathering.empty()) return;
    std::vector<coop::element::WorldActor*> snap;
    coop::element::WaMirrors().Snapshot(snap);
    for (auto it = g_lastGathering.begin(); it != g_lastGathering.end();) {
        bool live = false;
        for (auto* el : snap)
            if (el && el->LiveActor() == it->first && it->second.actor.Get()==it->first) { live = true; break; }
        if (live) ++it; else it = g_lastGathering.erase(it);
    }
}

}  // namespace

void Install(coop::net::Session* session) {
    const auto world=ue_wrap::world_identity::Generation();
    if(session!=LoadSession()||g_world!=world){OnDisconnect();g_world=world;}
    g_session.store(session, std::memory_order_release);
}

void Tick() {
    if(!GT::IsGameThread()||g_ticking)return;
    auto* s = LoadSession();
    if(s&&g_world!=ue_wrap::world_identity::Generation())Install(s);
    if (!Current(s)) return;
    struct Guard {Guard(){g_ticking=true;}~Guard(){g_ticking=false;}} guard;
    const auto epoch=g_epoch;
    const long long now = NowMs();

    if (!g_armed) {
        if (g_armFailedLatched) return;
        if (now - g_lastProbeMs < 250) return;  // 250 ms: client must arm well inside the
        g_lastProbeMs = now;                    // BeginPlay+1s first-timer window
        TryArmHooks();
        return;
    }

    if (s->role() == coop::net::Role::Host) {
        if (now - g_lastSweepMs >= 1000) {
            g_lastSweepMs = now;
            SweepStaleEdgeEntries();
        }
        return;
    }

    // Client: the restore scan at 250 ms; the pending replay every tick (cheap lookups, rare).
    if (now - g_lastProbeMs >= 250) {
        g_lastProbeMs = now;
        RestoreNewClientMirrors();
    }
    if(epoch!=g_epoch||!Current(s)||s->role()!=coop::net::Role::Client)return;
    TryReplayPendingGather();
}

bool ReadHostHeadingYaw(void* actor,float& outYaw) {
    if(!actor||!R::IsLive(actor)||!g_armedAtomic.load()||g_offMovementVector<0)return false;
    void* component=*reinterpret_cast<void**>(static_cast<uint8_t*>(actor)+g_offMovementVector);
    if(!component||!R::IsLive(component))return false;
    ue_wrap::CachedObjRef owner,axis;owner.Set(actor);axis.Set(component);
    const auto epoch=g_epoch;const auto world=ue_wrap::world_identity::Generation();
    const float value=E::GetComponentWorldRotation(component).Yaw;
    if(epoch!=g_epoch||world!=ue_wrap::world_identity::Generation()||!owner.Get()||!axis.Get()||
        *reinterpret_cast<void**>(static_cast<uint8_t*>(actor)+g_offMovementVector)!=component||!std::isfinite(value))return false;
    outYaw=value;return true;
}

void ApplyMirrorHeadingYaw(void* actor,float yaw) {
    if(!actor||!R::IsLive(actor)||!std::isfinite(yaw)||!g_armedAtomic.load()||g_offMovementVector<0)return;
    ue_wrap::CachedObjRef owner;owner.Set(actor);
    const auto epoch=g_epoch;const auto world=ue_wrap::world_identity::Generation();
    const ue_wrap::FRotator rot{0.f,yaw,0.f};
    void* mv=*reinterpret_cast<void**>(static_cast<uint8_t*>(actor)+g_offMovementVector);
    if(mv&&R::IsLive(mv)) {
        ue_wrap::CachedObjRef component;component.Set(mv);
        const bool applied=E::SetComponentWorldRotation(mv,rot);
        if(epoch!=g_epoch||world!=ue_wrap::world_identity::Generation()||!owner.Get()||!component.Get()||
            *reinterpret_cast<void**>(static_cast<uint8_t*>(actor)+g_offMovementVector)!=mv||!applied)return;
    }
    if(g_offArrow>=0) {
        void* arrow=*reinterpret_cast<void**>(static_cast<uint8_t*>(actor)+g_offArrow);
        if(arrow&&R::IsLive(arrow))E::SetComponentWorldRotation(arrow,rot);
    }
}

bool ReadHostRelLook(void* actor, float& outX, float& outY, float& outZ) {
    if (!actor || !R::IsLive(actor) || !g_armedAtomic.load(std::memory_order_acquire) || g_offRelLook < 0)
        return false;
    const float* v = reinterpret_cast<const float*>(
        reinterpret_cast<const uint8_t*>(actor) + g_offRelLook);
    if(!std::isfinite(v[0])||!std::isfinite(v[1])||!std::isfinite(v[2]))return false;
    outX = v[0]; outY = v[1]; outZ = v[2];
    return true;
}

void ApplyMirrorRelLook(void* actor, float x, float y, float z) {
    if (!actor || !R::IsLive(actor) || !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||
        !g_armedAtomic.load(std::memory_order_acquire) || g_offRelLook < 0)
        return;
    // All zero means the host stream had no value (pre-arm, or an offset miss on the host side):
    // keep the mirror's current target rather than yanking the head to the origin.
    if (x == 0.f && y == 0.f && z == 0.f) return;
    float* v = reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(actor) + g_offRelLook);
    v[0] = x; v[1] = y; v[2] = z;
}

namespace {
// The wisp-target eid cache: the resolve walks the NPC element table (a heap snapshot), fine
// on the rare gather edge but not at the pose stream's rate. The target changes on the 1 Hz
// seeWisps, gather and delete cadence, so a single pointer-to-eid memo collapses the per-tick
// cost to one pointer compare. Game thread only. One entry suffices: the event spawns one
// pyramid (two alternating pyramids would degrade to a per-tick resolve, not break).
void*    g_lastHostWisp = nullptr;
ue_wrap::CachedObjRef g_lastHostWispRef;
uint32_t g_lastHostWispEid = 0;
}  // namespace

uint32_t ReadHostWispTargetEid(void* actor) {
    // During the walk-to-wisp phase the host's head eases toward the target's world location (the
    // tick's chase branch), while the mirror, its target nulled by design until the gather relay,
    // idled on a look wander the host itself was ignoring. So the target identity is streamed and
    // the mirror runs the same native branch. 0 means no target (pre-arm, a miss, or genuinely
    // idle).
    if (!actor || !R::IsLive(actor) || !g_armedAtomic.load(std::memory_order_acquire) || g_offWispTarget < 0)
        return 0;
    void* wisp = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(actor) + g_offWispTarget);
    if (!wisp) { g_lastHostWisp = nullptr; g_lastHostWispEid = 0; return 0; }
    if (wisp == g_lastHostWisp && g_lastHostWispRef.Get()==wisp && g_lastHostWispEid != 0 &&
        WispOf(g_lastHostWispEid,false)==wisp)return g_lastHostWispEid;
    const uint32_t eid = FindNpcEidForActor(wisp);  // once per target CHANGE (1 Hz seeWisps cadence)
    g_lastHostWisp = wisp;
    g_lastHostWispRef.Set(eid?wisp:nullptr);
    g_lastHostWispEid = eid;
    return eid;
}

void ApplyMirrorWispTarget(void* actor, uint32_t wispEid) {
    if (!actor || !R::IsLive(actor) || !g_armedAtomic.load(std::memory_order_acquire) ||
        g_offWispTarget < 0 || g_gatheringOff < 0)
        return;
    // While the mirror is mid-gather the field belongs to the gather choreography (the replay
    // staged it; the montage's notify consumes and clears it natively). The host clears its own
    // target on its own montage clock, and pushing that clear here mid-suck would strand the
    // mirror's delete path. Hands off until the mirror's gathering falls.
    if (ReadBoolAt(actor, g_gatheringOff, g_gatheringMask)) return;
    void** slot = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(actor) + g_offWispTarget);
    if (wispEid == 0) {
        if (*slot) {
            *slot = nullptr;
            UE_LOGI("piramid-brain[client]: mirror wispTarget CLEARED (host idle)");
        }
        return;
    }
    void* wisp=WispOf(wispEid,true);
    if(!wisp)return;
    if (*slot != wisp) {
        *slot = wisp;
        UE_LOGI("piramid-brain[client]: mirror wispTarget -> npc eid=%u (native chase branch "
                "steers head/searchlight during the walk)", wispEid);
    }
}

void QueueConnectBroadcastForSlot(int slot) {
    auto* session=LoadSession();
    if(!GT::IsGameThread()||!Current(session)||session->role()!=coop::net::Role::Host||
        slot<=0||slot>=coop::net::kMaxPeers||!session->IsSlotWorldReady(slot)||!g_armed)return;
    const auto generation=session->peerGenerationForSlot(slot);
    const auto epoch=g_epoch;
    if(!generation)return;
    std::vector<coop::element::Registry::ActorIdPair> snap;
    coop::element::Registry::Get().SnapshotActorsByType(coop::element::ElementType::WorldActor,snap);
    for(const auto pair:snap) {
        if(epoch!=g_epoch||!Current(session)||session->role()!=coop::net::Role::Host||
            session->peerGenerationForSlot(slot)!=generation||!session->IsSlotWorldReady(slot))return;
        void* pyr=PyramidOf(pair.id,false);
        if(!pyr||pyr!=pair.actor||!ReadBoolAt(pyr,g_gatheringOff,g_gatheringMask))continue;
        void* wisp=*reinterpret_cast<void**>(static_cast<uint8_t*>(pyr)+g_offWispTarget);
        const auto eid=FindNpcEidForActor(wisp);if(!eid)continue;
        coop::net::PyramidGatherPayload payload{};payload.pyramidEid=pair.id;payload.wispEid=eid;
        session->SendReliableToSlot(slot,coop::net::ReliableKind::PyramidGather,&payload,sizeof(payload));
    }
}

void OnDisconnect() {
    ++g_epoch;
    g_session.store(nullptr,std::memory_order_release);
    g_world=0;
    g_lastProbeMs=g_lastSweepMs=0;
    g_pending = PendingGather{};
    g_wispTick = PendingGather{};
    g_lastGathering.clear();
    g_tickRestored.clear();
    // The host target memo: a recycled actor address in a new session must not alias the old
    // session's eid.
    g_lastHostWisp = nullptr;
    g_lastHostWispRef.Reset();
    g_lastHostWispEid = 0;
    // The probe counters are per session: a forced re-run in the same process after a reconnect
    // must not see the previous session's relays.
    g_relayCount.store(0, std::memory_order_relaxed);
    // The hooks stay latched, role- and session-gated inside, the NPC and world-actor observer
    // shape.
}

void OnPyramidGather(const coop::net::PyramidGatherPayload& payload) {
    auto* session=LoadSession();
    if(!GT::IsGameThread()||!Current(session)||session->role()!=coop::net::Role::Client)return;
    if (!coop::element::Registry::IsAllowedHostAllocatedEid(payload.pyramidEid) ||
        !coop::element::Registry::IsAllowedHostAllocatedEid(payload.wispEid)) {
        UE_LOGW("piramid-gather[client]: eid out of host range (pyramid=%u wisp=%u) -- dropping",
                payload.pyramidEid, payload.wispEid);
        return;
    }
    if(g_pending.pyramidEid==payload.pyramidEid&&g_pending.wispEid==payload.wispEid&&
        (!g_pending.pyramid.Raw()||g_pending.pyramid.Get()==PyramidOf(payload.pyramidEid,true))&&
        (!g_pending.wisp.Raw()||g_pending.wisp.Get()==WispOf(payload.wispEid,true)))return;
    g_pending={};
    g_pending.pyramidEid = payload.pyramidEid;
    g_pending.wispEid = payload.wispEid;
    g_pending.deadlineMs = NowMs() + kReplayDeadlineMs;
    g_pending.lastDist = -1.0f;
    g_pending.attempts = 0;
    g_pending.active = true;
    g_pending.revision=++g_pendingRevision;
    g_pending.pyramid.Set(PyramidOf(payload.pyramidEid,true));
    g_pending.wisp.Set(WispOf(payload.wispEid,true));
    UE_LOGI("piramid-gather[client]: queued replay pyramidEid=%u wispEid=%u (deadline %lld ms)",
            payload.pyramidEid, payload.wispEid, kReplayDeadlineMs);
}

bool DebugHooksArmed() { return g_armedAtomic.load(std::memory_order_acquire); }
int  DebugHostRelayCount() { return g_relayCount.load(std::memory_order_relaxed); }

}  // namespace coop::piramid_sync
