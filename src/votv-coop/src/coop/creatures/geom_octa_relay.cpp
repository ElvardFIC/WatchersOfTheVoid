// coop/creatures/geom_octa_relay.cpp -- see coop/creatures/geom_octa_relay.h.

#include "coop/creatures/geom_octa_relay.h"

#include "coop/creatures/npc_mirror_brain.h"
#include "coop/creatures/npc_sync.h"
#include "coop/element/element.h"
#include "coop/element/registry.h"
#include "coop/net/net_clock.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"

#include "ue_wrap/actors/geom_octa.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/facts/function_row.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace coop::geom_octa_relay {
namespace {

namespace R  = ue_wrap::reflection;
namespace sg = ue_wrap::script_gate;
namespace UH = ue_wrap::ufunction_hook;
namespace GO = ue_wrap::geom_octa;
namespace EL = coop::element;
namespace B  = coop::npc_mirror_brain;
namespace N  = coop::net;

// The "fire" section's finish notify, the body the mirror's brain refuses and this lane replays; and the one native it
// makes that carries the answer: whether minSphere still overlaps the target.
UE_FACT_FN(kRowFireFinish, "geom_octa_relay", L"geomOcta_C", L"OnNotifyBegin_1DFC28904024A8FB71C3ACA9DBD440F9",
           Declared);
UE_FACT_FN(kRowIsOverlappingActor, "geom_octa_relay", L"PrimitiveComponent", L"IsOverlappingActor", Declared);

constexpr int kTagFinish = 0x4F435437;   // 'OCT7'

// The entries of the Octa's event graph a replay names (cooked byte offsets): minSphere's begin-overlap and `aggro`
// for an Aggro, "fire"'s finish notify for a verdict.
constexpr int32_t kEntryOverlap = 3766;
constexpr int32_t kEntryAggro = 3592;
constexpr int32_t kEntryFireFinish = 1092;
// End of minSphere.IsOverlappingActor(targetActor) in the 0.9.0n graph.
constexpr int32_t kFireOverlapEnd = 1227;

// Divergence from MTA, which resends a syncer's ped state every pulse so a lost packet is replaced by the next
// (reference/mtasa-blue/Server/mods/deathmatch/logic/CPedSync.cpp:220, :305): these decisions are one-shot and paired by
// count, so a missing one costs the look of one attack, bounded by the waits below, and never an outcome.
// The "close" section is 2.1667 s, the longest an Octa stays busy after the host's own close finished: an Aggro that
// arrives first waits for the client's close, at most this long plus a margin. A verdict waits that long plus 2 s.
constexpr uint64_t kAggroHoldMs = 2167 + 500;
constexpr uint64_t kVerdictWaitMs = 2167 + 2000;
constexpr uint64_t kResolveMs = 2000;   // a real target that does not resolve (an element still materialising)

constexpr size_t kMaxOctas = 8;
constexpr size_t kMaxKept = 4;      // verdicts kept per Octa
constexpr size_t kMaxWaiting = 16;  // refused notifies awaiting a verdict, per Octa

// Why an event is refused at the boundary. One counter and one line per reason.
enum class Reject : uint8_t {
    None, BadOp, BadTargetKind, BadRefire, VerdictWithTarget, TargetWithoutKind, SlotRange, BadOctaEid, BadTargetEid,
    Count
};
constexpr const char* kReasonName[static_cast<int>(Reject::Count)] = {
    "none", "op outside {0, 1}", "targetKind outside {0, 1, 2}", "refire outside {0, 1} or set on an Aggro",
    "a FireVerdict that names a target", "a target on targetKind 0", "a slot at or past kMaxPeers",
    "an Octa id that is not host-allocated", "an element target that is not host-allocated"};

uint32_t g_rejected[static_cast<int>(Reject::Count)] = {};

// The first reason an honest host never produces, else None. Pure.
Reject Check(const N::GeomOctaEventPayload& p) {
    if (p.op != N::kGeomOctaOpAggro && p.op != N::kGeomOctaOpVerdict) return Reject::BadOp;
    if (p.targetKind > N::kGeomOctaTargetElement) return Reject::BadTargetKind;
    if (p.refire > 1 || (p.op == N::kGeomOctaOpAggro && p.refire != 0)) return Reject::BadRefire;
    if (p.op == N::kGeomOctaOpVerdict && (p.targetKind != N::kGeomOctaTargetNone || p.target != 0))
        return Reject::VerdictWithTarget;
    if (p.targetKind == N::kGeomOctaTargetNone && p.target != 0) return Reject::TargetWithoutKind;
    if (!EL::Registry::IsAllowedHostAllocatedEid(p.octaEid)) return Reject::BadOctaEid;
    if (p.targetKind == N::kGeomOctaTargetSlot && p.target >= static_cast<uint32_t>(N::kMaxPeers))
        return Reject::SlotRange;
    if (p.targetKind == N::kGeomOctaTargetElement && !EL::Registry::IsAllowedHostAllocatedEid(p.target))
        return Reject::BadTargetEid;
    return Reject::None;
}

// ---- per-Octa state, only for a live, marked geomOcta_C mirror ----

struct Kept {
    uint32_t n;
    uint8_t  refire;
};

struct OctaState {
    uint32_t eid = 0;   // 0: free
    ue_wrap::CachedObjRef actor;
    uint64_t revision = 0;

    // An Aggro waiting to be replayed (the newest; an older one unreplayed is superseded).
    bool     aggroPending = false;
    uint8_t  targetKind = 0;
    uint32_t target = 0;
    uint32_t aggroBase = 0;
    uint64_t heldAt = 0;         // when it reached the lane
    uint64_t unresolvedAt = 0;   // when its target first failed to resolve
    bool     heldSaid = false;

    // The pairing since the last Aggro, latest wins: the pending refused fire-finishes take the NEWEST min(P, K) kept
    // verdicts numbered past `done` (relative to `base`, modular), oldest first; older kept ones retire as superseded. A number
    // d = n - base; `done` is the highest d replayed or timed out.
    bool     haveBase = false;
    uint32_t base = 0;
    uint32_t done = 0;
    uint64_t waitAt[kMaxWaiting] = {};      // the refused notifies awaiting a verdict, oldest first (a ring)
    bool     pendingSaid[kMaxWaiting] = {};
    size_t   waitHead = 0, waitCount = 0;
    bool     supersededSaid = false;        // `verdict superseded` is said once per attack
    Kept     kept[kMaxKept] = {};
    size_t   keptCount = 0;
};
OctaState g_octas[kMaxOctas];
Counters  g_counters{};

bool g_watched = false;
ue_wrap::CachedObjRef g_overlapFn;
uint64_t g_hookRetryAt = 0, g_epoch = 0, g_revision = 0;
uint32_t g_world = 0;
uint32_t g_serial=0;
uint64_t g_stops=0;
N::Role g_role=N::Role::Client;
N::Session* g_session = nullptr;
bool g_ticking = false;

bool Current() {
    return g_session && g_session == coop::npc_sync::GetSession() && g_session->running() &&
        g_session->connected() && g_role==N::Role::Client && g_session->role() == N::Role::Client &&
        g_world == ue_wrap::world_identity::Generation() && g_serial==N::session_serial::Current() && g_stops==g_session->StopCount();
}

bool EnterSession() {
    auto* session = coop::npc_sync::GetSession();
    const auto world = ue_wrap::world_identity::Generation();
    const auto serial=N::session_serial::Current();
    if (session != g_session || world != g_world || g_serial!=serial ||
        (session&&(g_stops!=session->StopCount() || g_role!=session->role()))) {
        OnDisconnect();
        g_session = session;
        g_world = world;
        g_serial=serial;g_stops=session?session->StopCount():0;
        g_role=session?session->role():N::Role::Client;
    }
    return Current();
}

unsigned long long Now() { return static_cast<unsigned long long>(N::NowMs()); }

// A drop is counted every time and said once per (line, reason): a host that is wrong would otherwise fill the log. The
// reasons are literals, compared by pointer.
bool SaidOnce(const char* line, const char* why) {
    struct Said {
        const char* line;
        const char* why;
    };
    static Said said[16];
    static size_t count = 0;
    for (size_t i = 0; i < count; ++i)
        if (said[i].line == line && said[i].why == why) return true;
    if (count < sizeof(said) / sizeof(said[0])) said[count++] = Said{line, why};
    return false;
}

void LogDroppedVerdict(uint32_t eid, uint32_t n, const char* why) {
    ++g_counters.verdictDropped;
    if (SaidOnce("verdict", why)) return;
    UE_LOGI("[OCTA] verdict dropped eid=%u n=%u why=%s (counted; said once per reason) t=%llu", static_cast<unsigned>(eid),
            static_cast<unsigned>(n), why, Now());
}

OctaState* Find(uint32_t eid) {
    for (OctaState& o : g_octas)
        if (o.eid == eid) return &o;
    return nullptr;
}

void* OctaMirrorOf(uint32_t eid);

bool Live(const OctaState& st) {
    return st.eid && st.actor.Get() && OctaMirrorOf(st.eid) == st.actor.Raw();
}

OctaState* Take(uint32_t eid, void* actor) {
    for (auto& o : g_octas) if (o.eid && !Live(o)) o = {};
    if (OctaState* o = Find(eid)) return o;
    for (OctaState& o : g_octas) {
        if (o.eid == 0) {
            o = OctaState{};
            o.eid = eid;
            o.actor.Set(actor);
            o.revision = ++g_revision;
            return &o;
        }
    }
    return nullptr;
}

// Native calls may disconnect, replace an actor or admit another attack. A
// receipt belongs only to the binding and attack captured before that call.
struct Receipt {
    uint32_t eid;
    uint64_t revision, epoch;
    ue_wrap::CachedObjRef actor;
    explicit Receipt(const OctaState& st) : eid(st.eid), revision(st.revision), epoch(g_epoch), actor(st.actor) {}
    bool Valid() const {
        if (epoch != g_epoch || !Current() || !actor.Get()) return false;
        const auto* st = Find(eid);
        return st && st->revision == revision && st->actor.Get() == actor.Raw() && Live(*st);
    }
};

void TargetText(uint8_t kind, uint32_t v, char (&out)[40]) {
    if (kind == N::kGeomOctaTargetSlot) std::snprintf(out, sizeof(out), "slot:%u", static_cast<unsigned>(v));
    else if (kind == N::kGeomOctaTargetElement) std::snprintf(out, sizeof(out), "element:%u", static_cast<unsigned>(v));
    else std::snprintf(out, sizeof(out), "none");
}

// Is `actor` a live, marked mirror of the Octa class, by the class chain's names?
bool IsOctaMirror(void* actor) {
    static const B::EntryTable* const table = B::TableNamed(ue_wrap::profile::name::NpcClass_GeomOcta);
    return actor && table && B::IsMarkedMirror(actor) && B::ChainHasTable(R::ClassOf(actor), *table);
}

// The Octa mirror of host element `eid`, or null.
void* OctaMirrorOf(uint32_t eid) {
    void* actor = EL::LiveActorOfType(static_cast<EL::ElementId>(eid), EL::ElementType::Npc);
    return IsOctaMirror(actor) ? actor : nullptr;
}

// The host's target as this client has it: a player's counterpart is the body this client keeps for that slot (its own
// player for its own slot, else a puppet, the host's included), an element's is its mirror or twin. False for a target
// that is not here (yet).
bool ResolveTarget(uint8_t kind, uint32_t v, void*& out) {
    out = nullptr;
    if (kind == N::kGeomOctaTargetNone) return true;
    void* actor = nullptr;
    if (kind == N::kGeomOctaTargetSlot) {
        auto& reg = coop::players::Registry::Get();
        if (static_cast<uint8_t>(v) == reg.LocalPeerId()) {
            actor = reg.Local();
        } else {
            coop::RemotePlayer* rp = reg.Puppet(static_cast<uint8_t>(v));
            actor = rp ? rp->GetActor() : nullptr;
        }
    } else {
        const EL::Element* e = EL::Registry::Get().Get(static_cast<EL::ElementId>(v));
        actor = e ? e->LiveActor() : nullptr;
    }
    if (!actor || !R::IsLive(actor)) return false;
    out = actor;
    return true;
}

// ---- the overwrite of the one overlap read a verdict replay makes ----

struct Overwrite {
    bool    armed = false;
    ue_wrap::CachedObjRef actor, minSphere, graph, function;
    uint64_t epoch = 0, revision = 0;
    uint32_t eid = 0;
    uint8_t refire = 0;
    uint32_t writes = 0;   // the answers written during the armed replay
};
Overwrite g_ow;

// After a native IsOverlappingActor: writes the host's answer into the result, one byte (the native's bool), only for
// the call the replayed notify makes on this Octa's minSphere from its own graph in the frame the pass ran. Any other
// call passes untouched, and so does any call off the game thread: natives are reached from other threads.
void OnOverlapPost(void* context, void* source, void* /*result*/) {
    if (!ue_wrap::game_thread::IsGameThread()) return;
    if (!g_ow.armed || g_ow.epoch != g_epoch || !Current() ||
        !g_ow.actor.Get() || source != g_ow.actor.Raw() ||
        !g_ow.minSphere.Get() || context != g_ow.minSphere.Raw() ||
        !g_ow.graph.Get() || !g_ow.function.Get() || UH::CurrentNativeFunction() != g_ow.function.Raw()) return;
    const auto* st = Find(g_ow.eid);
    if (!st || st->revision != g_ow.revision || !Live(*st) ||
        GO::MinSphere(source) != context || GO::GraphFunction(source) != g_ow.graph.Raw()) return;
    const UH::CallerFrame frame = UH::CurrentCallerFrame();
    if (frame.function != g_ow.graph.Raw() || !frame.locals || frame.locals != B::ScopedEntryPass::Frame() ||
        frame.codeOffset != kFireOverlapEnd || g_ow.writes != 0) return;
    if (void* r = UH::CurrentResult()) {
        *static_cast<uint8_t*>(r) = g_ow.refire;
        ++g_ow.writes;
        ++g_counters.overwriteWrites;
    }
}

// Never replace an outer replay's overwrite. The captured hook is disarmed even
// if a disconnect clears the lane while the native call is running.
class ArmScope {
public:
    ArmScope(const OctaState& st, uint8_t refire) {
        if (g_ow.armed || !Current() || !Live(st) || !g_overlapFn.Get()) return;
        Overwrite next{};
        next.actor = st.actor;
        next.minSphere.Set(GO::MinSphere(st.actor.Get()));
        next.graph.Set(GO::GraphFunction(st.actor.Get()));
        next.function = g_overlapFn;
        if (!next.minSphere.Get() || !next.graph.Get()) return;
        next.epoch = g_epoch; next.revision = st.revision; next.eid = st.eid;
        next.refire = refire; next.armed = true;
        if (!UH::SetArmed(next.function.Get(), &OnOverlapPost, true)) return;
        function_ = next.function;
        owns_ = true;
        g_ow = next;
    }
    ~ArmScope() {
        if (!owns_) return;
        if (function_.Get()) UH::SetArmed(function_.Get(), &OnOverlapPost, false);
        g_ow = {};
    }
    bool Ready() const { return owns_; }
    ArmScope(const ArmScope&) = delete;
    ArmScope& operator=(const ArmScope&) = delete;
private:
    bool owns_ = false;
    ue_wrap::CachedObjRef function_;
};

// Retry unavailable metadata/hooks at most once per second. No verdict may run
// against the client's local overlap answer while this hook is unavailable.
bool EnsureHook() {
    if (g_overlapFn.Get()) return true;
    const auto now = N::NowMs();
    if (now < g_hookRetryAt) return false;
    g_hookRetryAt = now + 1000;
    const ue_wrap::facts::Resolved fn = ue_wrap::facts::Resolve(kRowIsOverlappingActor);
    if (fn && UH::InstallPostHook(fn, &OnOverlapPost, false)) {
        g_overlapFn.Set(fn.Fn());
        return g_overlapFn.Get() != nullptr;
    }
    if (!SaidOnce("hook", "unavailable")) UE_LOGW("geom_octa_relay: overlap hook unavailable; verdicts wait for retry");
    return false;
}

// ---- the refused notify, counted ----

// Before the mirror's own "fire" finish notify: the body is refused by the brain, so the notify is queued, and Tick
// serves it with one of the host's kept verdicts (latest wins) or times it out. The lane's own replay of the same function is not counted, or a re-fire's
// replay would shift every later pairing by one.
sg::Verdict OnFinishPre(const sg::Call& call) {
    if (!Current() || call.fromOurCode || !GO::IsFinishNotify(call) || !B::IsMarkedMirror(call.object)) return sg::Verdict::Run;
    const EL::ElementId eid = EL::Registry::Get().EidForActor(call.object);
    OctaState* st = eid == EL::kInvalidId ? nullptr : Find(static_cast<uint32_t>(eid));
    if (!st || !st->haveBase || !Live(*st) || st->actor.Get() != call.object) return sg::Verdict::Run;
    if (st->waitCount == kMaxWaiting) {   // sixteen unanswered notifies: unreachable at one a 1.8 s with a 4.2 s wait
        LogDroppedVerdict(st->eid, 0, "too many refused notifies awaiting a verdict");
        return sg::Verdict::Run;
    }
    const size_t i = (st->waitHead + st->waitCount++) % kMaxWaiting;
    st->waitAt[i] = N::NowMs();
    st->pendingSaid[i] = false;
    return sg::Verdict::Run;
}

// ---- the replays ----

void SetBase(OctaState& st, uint32_t base) {
    st.haveBase = true;
    st.base = base;
    st.done = 0;
    st.waitHead = st.waitCount = 0;
    for (bool& s : st.pendingSaid) s = false;
    st.supersededSaid = false;
    // Kept verdicts numbered at or below the new base belong to an attack already over.
    size_t w = 0;
    for (size_t i = 0; i < st.keptCount; ++i)
        if (static_cast<int32_t>(st.kept[i].n - base) > 0) st.kept[w++] = st.kept[i];
    st.keptCount = w;
}

// Kept verdicts retired because a newer one exists (latest wins), whether it was applied or only arrived: counted always,
// said once per attack. It is not a fault, so RelayDirty does not read it.
void NoteSuperseded(OctaState& st, uint32_t upto, uint32_t count) {
    g_counters.verdictSuperseded += count;
    if (st.supersededSaid) return;
    st.supersededSaid = true;
    UE_LOGI("[OCTA] verdict superseded eid=%u upto=%u (counted; said once per attack) t=%llu",
            static_cast<unsigned>(st.eid), static_cast<unsigned>(upto), Now());
}

void LogAggro(const char* what, const OctaState& st, const char* why) {
    char text[40];
    TargetText(st.targetKind, st.target, text);
    UE_LOGI("[OCTA] aggro %s eid=%u target=%s base=%u%s%s t=%llu", what, static_cast<unsigned>(st.eid), text,
            static_cast<unsigned>(st.aggroBase), why ? " why=" : "", why ? why : "", Now());
}

// An Aggro whose look is not replayed still sets the base and zeroes the count: its attack's outcomes arrive, and its
// verdicts are kept and then dropped by number at the next Aggro.
void SkipAggro(OctaState& st, const char* why) {
    SetBase(st, st.aggroBase);
    st.aggroPending = false;
    ++g_counters.aggroSkipped;
    LogAggro("skipped", st, why);
}

void ServeAggro(OctaState& st, void* octa, uint64_t now) {
    const Receipt receipt(st);
    if (!receipt.Valid()) return;
    if (!st.aggroPending) return;
    if (GO::IsAnim(octa)) {   // busy: the previous attack's close has not finished here yet
        if (!st.heldSaid) {
            st.heldSaid = true;
            LogAggro("held", st, nullptr);
        }
        if (now - st.heldAt > kAggroHoldMs) SkipAggro(st, "the Octa stayed busy");
        return;
    }
    void* other = nullptr;
    if (!ResolveTarget(st.targetKind, st.target, other)) {
        if (!st.unresolvedAt) st.unresolvedAt = now;
        if (now - st.unresolvedAt > kResolveMs) SkipAggro(st, "the target did not resolve");
        return;
    }
    SetBase(st, st.aggroBase);
    st.aggroPending = false;
    bool ran = false;
    {
        B::ScopedEntryPass pass(octa, {kEntryOverlap, kEntryAggro});
        ran = GO::CallMinSphereBeginOverlap(octa, other);
    }
    if (!receipt.Valid()) return;
    if (ran) {
        ++g_counters.aggroReplayed;
        LogAggro("replayed", st, nullptr);
    } else {
        ++g_counters.aggroSkipped;
        LogAggro("skipped", st, "the overlap handler did not run");
    }
}

// Replays the refused notify with the host's answer in its overlap read: whether the call ran, and how many answers the
// hook wrote into the reads it made (one, when the overwrite works).
struct Replay {
    bool     ran = false;
    uint32_t wrote = 0;
};
Replay ReplayFinish(const OctaState& st, const ArmScope& arm) {
    const Receipt receipt(st);
    if (!receipt.Valid()) return {};
    Replay r;
    if (!arm.Ready()) return r;
    {
        void* octa = receipt.actor.Get();
        B::ScopedEntryPass pass(octa, {kEntryFireFinish});
        r.ran = GO::CallFireFinishNotify(octa);
    }
    if (receipt.Valid()) r.wrote = g_ow.writes;
    return r;
}

void ServeVerdicts(OctaState& st, void* octa, uint64_t now) {
    const Receipt receipt(st);
    if (!receipt.Valid()) return;
    if (!st.haveBase) return;
    size_t budget = kMaxWaiting;
    while (st.waitCount > 0 && budget--) {
        const bool ready = EnsureHook() && GO::MinSphere(octa) && GO::GraphFunction(octa);
        if (!receipt.Valid()) return;
        if (!ready) {
            if (now - st.waitAt[st.waitHead] <= kVerdictWaitMs) return;
            ++st.done;
            st.waitHead = (st.waitHead + 1) % kMaxWaiting;
            --st.waitCount;
            LogDroppedVerdict(st.eid, st.base + st.done, "host-answer hook or replay binding unavailable at timeout");
            continue;
        }
        // The kept verdicts numbered past `done` (relative to the base, modular), ascending by that number.
        size_t order[kMaxKept];
        size_t k = 0;
        for (size_t i = 0; i < st.keptCount; ++i) {
            const uint32_t d = st.kept[i].n - st.base;
            if (static_cast<int32_t>(d) <= 0 || d <= st.done) continue;
            size_t j = k++;
            for (; j > 0 && st.kept[order[j - 1]].n - st.base > d; --j) order[j] = order[j - 1];
            order[j] = i;
        }
        const uint32_t want = st.base + st.done + 1;   // what is waited for when none is kept
        const size_t slot = st.waitHead;
        if (k > 0) {
            // Latest wins: the P pending notifies are served oldest first with the NEWEST min(P, K) kept verdicts in
            // ascending order, and the kept verdicts older than those retire as superseded, never as dropped. A
            // joiner's chain reaches its first notify long after the host began to re-fire (P = 1, K > 1: the newest).
            const size_t served = k < st.waitCount ? k : st.waitCount;
            const size_t retire = k - served;
            bool gone[kMaxKept] = {};
            for (size_t i = 0; i < retire; ++i) gone[order[i]] = true;
            const size_t at = order[retire];
            gone[at] = true;
            const Kept v = st.kept[at];
            ArmScope arm(st, v.refire);
            if (!arm.Ready() && now - st.waitAt[slot] <= kVerdictWaitMs) return;
            size_t w = 0;
            for (size_t i = 0; i < st.keptCount; ++i)
                if (!gone[i]) st.kept[w++] = st.kept[i];
            st.keptCount = w;
            if (retire) NoteSuperseded(st, v.n, static_cast<uint32_t>(retire));
            st.done = v.n - st.base;
            st.waitHead = (st.waitHead + 1) % kMaxWaiting;
            --st.waitCount;
            const Replay r = ReplayFinish(st, arm);
            if (!receipt.Valid()) return;
            if (r.ran && r.wrote == 1) {
                ++g_counters.verdictApplied;
                UE_LOGI("[OCTA] verdict applied eid=%u n=%u refire=%u wrote=%u t=%llu", static_cast<unsigned>(st.eid),
                        static_cast<unsigned>(v.n), static_cast<unsigned>(v.refire), static_cast<unsigned>(r.wrote),
                        Now());
            } else {
                LogDroppedVerdict(st.eid, v.n, "the notify did not confirm one host answer");
            }
            continue;
        }
        if (!st.pendingSaid[slot]) {
            st.pendingSaid[slot] = true;
            UE_LOGI("[OCTA] verdict pending eid=%u n=%u t=%llu", static_cast<unsigned>(st.eid),
                    static_cast<unsigned>(want), Now());
        }
        if (now - st.waitAt[slot] <= kVerdictWaitMs) return;
        // No verdict in time: the look ends with the Octa idle (refire 0), and the next Aggro starts clean.
        ArmScope arm(st, 0);
        ++st.done;
        st.waitHead = (st.waitHead + 1) % kMaxWaiting;
        --st.waitCount;
        const Replay replay = ReplayFinish(st, arm);
        if (!receipt.Valid()) return;
        if (replay.ran && replay.wrote == 1) {
            ++g_counters.verdictTimeout;
            UE_LOGI("[OCTA] verdict timeout eid=%u n=%u t=%llu", static_cast<unsigned>(st.eid),
                    static_cast<unsigned>(want), Now());
        } else {
            LogDroppedVerdict(st.eid, want, "the timeout notify did not confirm one host answer");
        }
    }
}

void LogDroppedEvent(const N::GeomOctaEventPayload& p, const char* why) {
    ++g_counters.eventDropped;
    if (SaidOnce("event", why)) return;
    UE_LOGI("[OCTA] event dropped eid=%u op=%u why=%s (counted; said once per reason) t=%llu",
            static_cast<unsigned>(p.octaEid), static_cast<unsigned>(p.op), why, Now());
}

// The watch on the refused notify, once.
void EnsureWatched() {
    if (!g_watched) g_watched = sg::WatchName(kRowFireFinish, kTagFinish, &OnFinishPre, nullptr);
}

}  // namespace

void OnEvent(const N::GeomOctaEventPayload& p) {
    if (!EnterSession()) return;
    const Reject why = Check(p);
    if (why != Reject::None) {
        const uint32_t n = ++g_rejected[static_cast<int>(why)];
        if (n == 1)
            UE_LOGW("geom_octa_relay: a GeomOctaEvent was refused: %s (counted; said once per reason)",
                    kReasonName[static_cast<int>(why)]);
        return;
    }
    // The host sends none before the Octa's EntitySpawn, which rides the same lane: an Octa that is no live, marked
    // mirror here is destroyed, or the host is wrong. Nothing is reflected on anything else.
    void* actor = OctaMirrorOf(p.octaEid);
    if (!actor) {
        LogDroppedEvent(p, "no live Octa mirror");
        return;
    }
    OctaState* st = Take(p.octaEid, actor);
    if (!st) {
        LogDroppedEvent(p, "more Octas than tracked");
        return;
    }
    const uint64_t now = N::NowMs();
    if (p.op == N::kGeomOctaOpAggro) {
        ++g_counters.aggroReceived;
        g_counters.lastAggroBase = p.verdictNo;
    } else {
        ++g_counters.verdictReceived;
    }
    if (p.op == N::kGeomOctaOpAggro) {
        if (st->aggroPending) SkipAggro(*st, "superseded by a newer Aggro");
        st->revision = ++g_revision;
        st->aggroPending = true;
        st->targetKind = p.targetKind;
        st->target = p.target;
        st->aggroBase = p.verdictNo;
        st->heldAt = now;
        st->unresolvedAt = 0;
        st->heldSaid = false;
        return;
    }
    // A FireVerdict: kept until a refused notify is served (latest wins: Tick shows the newest, retires the older).
    // One numbered at or below the base, or already shown, is over.
    const uint32_t d = p.verdictNo - st->base;
    if (st->haveBase && (static_cast<int32_t>(d) <= 0 || d <= st->done)) {
        LogDroppedVerdict(p.octaEid, p.verdictNo, "at or below the base");
        return;
    }
    for (size_t i = 0; i < st->keptCount; ++i) {
        if (st->kept[i].n == p.verdictNo) {
            LogDroppedVerdict(p.octaEid, p.verdictNo, "already kept");
            return;
        }
    }
    if (st->keptCount == kMaxKept) {
        // The kept verdict nearest the base goes (the Default lane is ordered, so it is also the oldest: this is defensive).
        size_t ev = 0;
        for (size_t i = 1; i < st->keptCount; ++i)
            if (st->kept[i].n - st->base < st->kept[ev].n - st->base) ev = i;
        NoteSuperseded(*st, p.verdictNo, 1);
        for (size_t i = ev + 1; i < st->keptCount; ++i) st->kept[i - 1] = st->kept[i];
        --st->keptCount;
    }
    st->kept[st->keptCount++] = Kept{p.verdictNo, p.refire};
}

void Tick() {
    if (g_ticking || !EnterSession()) return;
    struct Guard { Guard() { g_ticking = true; } ~Guard() { g_ticking = false; } } guard;
    const auto epoch = g_epoch;
    EnsureWatched();
    uint64_t now = 0;   // read once, and only when an Octa is live
    for (OctaState& st : g_octas) {
        if (st.eid == 0) continue;
        void* octa = OctaMirrorOf(st.eid);
        if (!octa || !Live(st)) {
            st = OctaState{};
            continue;
        }
        if (!now) now = N::NowMs();
        const Receipt receipt(st);
        ServeAggro(st, octa, now);
        if (epoch != g_epoch || !Current()) return;
        if (!receipt.Valid()) continue;
        ServeVerdicts(st, octa, now);
        if (epoch != g_epoch || !Current()) return;
    }
}

Counters GetCounters() { return g_counters; }

void OnDisconnect() {
    ++g_epoch;
    g_session = nullptr;
    g_world = 0;
    g_hookRetryAt = 0;
    for (OctaState& o : g_octas) o = OctaState{};
    g_counters = Counters{};
}

}  // namespace coop::geom_octa_relay
