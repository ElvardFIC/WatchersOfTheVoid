// Host observations and client native replay. Scheduler suppression, per-row
// policy and recipient retry queues live in their respective event modules.

#include "coop/world/event_fire_sync.h"
#include "coop/world/event_location_sync.h"
#include "coop/world/event_schedule_authority.h"

#include "event_fire_policy.h"  // ReplayVerdict (co-located private header)
#include "event_fire_delivery.h"

#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/players_registry.h"
#include "coop/player/puppet_drive.h"
#include "coop/player/remote_player.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/world/world_singleton.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace coop::event_fire_sync {
namespace {

namespace R  = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;
namespace sg = ue_wrap::script_gate;

std::atomic<coop::net::Session*> g_session{nullptr};

// Resolution, on demand, game thread: the three classes come from the object index, which answers at
// once whether or not they are loaded; their members resolve once all three are, on the first pass or
// never (a renamed symbol on a future game version), so a failed pass latches loudly.
int32_t g_offSaveSlot = -1;           // mainGamemode.saveSlot (UsaveSlot_C*)
int32_t g_offEventer = -1;            // mainGamemode.eventer  (Atrigger_eventer_C*)
int32_t g_offPassEvents = -1;         // saveSlot.passEvents (TArray<FName>)
void* g_runEventFn = nullptr;         // runEvent(FName event, FName special)
void* g_runSpecialEventFn = nullptr;  // runSpecialEvent(FName eventName1) -> bool
void* g_settimeFn = nullptr;          // saveSlot.settime, the scheduler's walk
int32_t g_offEventParam = -1;         // runEvent's `event` in its parameter frame
int32_t g_offSpecialParam = -1;       // runSpecialEvent's `eventName1` in its parameter frame
bool g_resolved = false;
bool g_resolveFailed = false;
ue_wrap::CachedObjRef g_gmClass, g_slotClass, g_eventerClass;

// The watches are the only host broadcast source. Story rows dedupe by name;
// specials replay once per reliable occurrence, including repeated same names.
constexpr int kTagEventFire = 0x45564652;  // 'EVFR'
UE_FACT_FN(kRowRunEvent, "event_fire_sync", nullptr, L"runEvent", AnyClass);
UE_FACT_FN(kRowRunSpecialEvent, "event_fire_sync", nullptr, L"runSpecialEvent", AnyClass);
bool g_watchInstalled = false;
bool g_watchLive = false;
bool g_watchDead = false;          // settled without going live: said once, no longer polled
bool g_specialWatchInstalled = false;
bool g_specialWatchLive = false;
bool g_specialWatchDead = false;

// Client suppression and replay state, game thread.
struct PendingFire {
    FireKind kind;
    std::string name;
    // The active override: the host registry says this row is in flight, so bypass the
    // passEvents dedupe (a mid-event joiner's blob carries the row as completed history while
    // the event is still running).
    bool activeOverride = false;
    bool attempted = false;    // one replay try ran (paces the next)
    std::chrono::steady_clock::time_point lastAttempt{};
    uint64_t id = 0;
};
std::deque<PendingFire> g_pending;    // replays waiting for the eventer (join window)
// EventFire is pre-world-sendable, so a joiner can queue fires for its whole load window. The
// cap is the table plus specials plus margin; duplicates are skipped at queue time, so it is
// effectively unreachable.
constexpr size_t kMaxPending = 128;
// A row whose world or eventer is not up yet is retried on a real-time cadence: the per-tick
// install pump drives the drain, so a waiting row never waits for the next fire.
constexpr auto kReplayRetrySpacing = std::chrono::seconds(1);
std::unordered_set<std::string> g_replayed;  // rows replayed this session (dedupe)
uint32_t g_boundGen = 0;
uint64_t g_replayEpoch = 0, g_replayId = 0;
uint32_t g_replaySerial = 0;
bool g_drainingReplay = false;
std::atomic<unsigned> g_replays{0};          // fires replayed natively (ReplayCount)
std::unordered_map<std::string, uint64_t> g_gatherMs;  // last time this location row gathered people

// A scare that stays on the player who walked into it. The vent break is not one of these:
// it changes the room, so every machine runs it, and the others are brought to the control room.
bool PersonalOnly(const std::string& name) {
    return name == "toeStab" || name == "mann" || name == "fakeGrays" || name == "susArir";
}

uint64_t GatherNowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}
bool GatherRecently(const std::string& name) {
    const auto it = g_gatherMs.find(name);
    return it != g_gatherMs.end() && GatherNowMs() - it->second < 15000;
}
void MarkGather(const std::string& name) { g_gatherMs[name] = GatherNowMs(); }

bool SendGather(coop::net::Session* s, const std::string& name) {
    void* local = coop::players::Registry::Get().Local();
    ue_wrap::FVector loc{};
    ue_wrap::FRotator rot{};
    if (!local || !ue_wrap::engine::TryGetActorLocation(local, loc) ||
        !ue_wrap::engine::TryGetActorRotation(local, rot)) return false;
    coop::net::EventGatherPayload p{};
    const size_t n = name.size() < sizeof(p.name) - 1 ? name.size() : sizeof(p.name) - 1;
    std::memcpy(p.name, name.c_str(), n);
    p.x = loc.X; p.y = loc.Y; p.z = loc.Z; p.yaw = rot.Yaw;
    if (!s->SendReliable(coop::net::ReliableKind::EventGather, &p, sizeof(p))) return false;
    UE_LOGI("event_fire: location event '%s' -- asking the host to bring the others here", name.c_str());
    return true;
}

// The host gathers to where this peer's body actually is. A claimed point near that body is
// newer than the puppet; a point far from it is refused, so a packet cannot move the session.
bool GatherPoint(uint8_t slot, float claimX, float claimY, float claimZ, float claimYaw,
                 float& x, float& y, float& z, float& yaw) {
    coop::RemotePlayer& pup = coop::puppet_drive::Puppet(static_cast<int>(slot));
    void* actor = pup.GetActor();
    ue_wrap::FVector loc{};
    ue_wrap::FRotator rot{};
    if (!actor || !pup.valid() || !ue_wrap::engine::TryGetActorLocation(actor, loc) ||
        !ue_wrap::engine::TryGetActorRotation(actor, rot))
        return false;
    const float dx = claimX - loc.X, dy = claimY - loc.Y, dz = claimZ - loc.Z;
    const bool near = std::isfinite(claimX) && std::isfinite(claimY) && std::isfinite(claimZ) &&
                      std::isfinite(claimYaw) && (dx * dx + dy * dy + dz * dz) < 3000.f * 3000.f;
    x = near ? claimX : loc.X;
    y = near ? claimY : loc.Y;
    z = near ? claimZ : loc.Z;
    yaw = near ? claimYaw : rot.Yaw;
    if (!near)
        UE_LOGW("event_fire: slot %u claimed a gather (%.0f, %.0f, %.0f) far from their body -- "
                "using the body", static_cast<unsigned>(slot), claimX, claimY, claimZ);
    return true;
}

// The UE4 array header; 8-byte FName elements for the two arrays touched.
struct RawArray {
    R::FName* Data;
    int32_t Num;
    int32_t Max;
};

bool MembersMissing() {
    return g_offSaveSlot < 0 || g_offEventer < 0 || g_offPassEvents < 0 ||
           !g_runEventFn || !g_runSpecialEventFn || !g_settimeFn || g_offEventParam < 0 || g_offSpecialParam < 0;
}

bool ResolvePass() {
    const bool current = g_gmClass.Alive() && g_slotClass.Alive() && g_eventerClass.Alive();
    if (g_resolved && current) return true;
    if (!current) g_resolved = g_resolveFailed = false;
    if (g_resolveFailed) return false;
    namespace OI = ue_wrap::object_index;
    void* gmCls = OI::ClassByName(L"mainGamemode_C");
    void* ssCls = OI::ClassByName(L"saveSlot_C");
    void* evCls = OI::ClassByName(L"trigger_eventer_C");
    if (!gmCls || !ssCls || !evCls) return false;  // the world has not loaded them yet
    g_gmClass.Set(gmCls); g_slotClass.Set(ssCls); g_eventerClass.Set(evCls);
    g_offSaveSlot = R::FindPropertyOffset(gmCls, L"saveSlot");
    g_offEventer = R::FindPropertyOffset(gmCls, L"eventer");
    g_offPassEvents = R::FindPropertyOffset(ssCls, L"passEvents");
    g_runEventFn = R::FindFunction(evCls, L"runEvent");
    g_runSpecialEventFn = R::FindFunction(evCls, L"runSpecialEvent");
    g_settimeFn = R::FindFunction(ssCls, L"settime");
    g_offEventParam = g_runEventFn ? R::FindParamOffset(g_runEventFn, L"event") : -1;
    g_offSpecialParam = g_runSpecialEventFn ? R::FindParamOffset(g_runSpecialEventFn, L"eventName1") : -1;
    if (MembersMissing()) {
        g_resolveFailed = true;
        UE_LOGW("event_fire: resolution INCOMPLETE on loaded classes (saveSlot=0x%X eventer=0x%X "
                "passEvents=0x%X runEvent=%s(event=0x%X) runSpecialEvent=%s settime=%s) -- "
                "latched OFF; game version mismatch?",
                g_offSaveSlot, g_offEventer, g_offPassEvents, g_runEventFn ? "yes" : "NO",
                g_offEventParam, g_runSpecialEventFn ? "yes" : "NO", g_settimeFn ? "yes" : "NO");
        return false;
    }
    g_resolved = true;
    UE_LOGI("event_fire: resolved (saveSlot=0x%X passEvents=0x%X eventer=0x%X "
            "runEvent=yes runSpecialEvent=yes settime=yes)",
            g_offSaveSlot, g_offPassEvents, g_offEventer);
    return true;
}

void* SaveSlotOf(void* gm) {
    if (!gm || g_offSaveSlot < 0) return nullptr;
    void* ss = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(gm) + g_offSaveSlot);
    return (ss && R::IsLive(ss)) ? ss : nullptr;
}

void* EventerOf(void* gm) {
    if (!gm || g_offEventer < 0) return nullptr;
    void* ev = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(gm) + g_offEventer);
    return (ev && R::IsLive(ev)) ? ev : nullptr;
}

RawArray* ArrayAt(void* obj, int32_t off) {
    if (!obj || off < 0) return nullptr;
    return reinterpret_cast<RawArray*>(reinterpret_cast<uint8_t*>(obj) + off);
}

std::string NarrowName(const R::FName& n) {
    const std::wstring w = R::ToString(n);
    std::string s;
    s.reserve(w.size());
    for (wchar_t c : w) s.push_back((c > 0 && c < 128) ? static_cast<char>(c) : '?');
    return s;
}

// Whether the host's watch on this verb is live in an enabled gate: a connected host reaches the
// wire only through it.
bool WatchLive(FireKind kind) {
    const ue_wrap::facts::Function& row = kind == FireKind::SpecialEvent ? kRowRunSpecialEvent : kRowRunEvent;
    return sg::NameWatchLive(row, kTagEventFire) && sg::IsEnabled();
}

bool IsClientSession() {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->running() && s->role() == coop::net::Role::Client;
}

// A client runs no eventer verb of its own: the game's event menu and its cheat menu reach
// runEvent's and runSpecialEvent's bodies directly, and every spawn inside them is bytecode-internal,
// so a fire there mints creatures and props no mirror covers. The one eventer call a client runs is
// this module's replay, admitted by NativeFire's own scope: the eventer and the verb it dispatches,
// once. An eventer call nested inside it is refused -- the prank roll's own runSpecialEvent among
// them, whose chosen case the host sends as a fire of its own.
struct Admission {
    void* object = nullptr;
    void* function = nullptr;
    bool spent = false;
};
Admission* g_admission = nullptr;   // game thread; set for exactly the length of NativeFire's Call
uint64_t g_clientRefused = 0;

sg::Verdict OnEventerVerbPre(const sg::Call& call) {
    if (!IsClientSession()) return sg::Verdict::Run;
    Admission* a = g_admission;
    if (a && !a->spent && call.object == a->object && call.function == a->function) {
        a->spent = true;
        return sg::Verdict::Run;
    }
    const uint64_t n = ++g_clientRefused;
    if ((n & (n - 1)) == 0) {
        const bool special = call.function == g_runSpecialEventFn;
        const int32_t off = special ? g_offSpecialParam : g_offEventParam;
        const std::string ev = (call.locals && off >= 0)
            ? NarrowName(*reinterpret_cast<const R::FName*>(call.locals + off)) : std::string("?");
        UE_LOGI("event_fire: refused a client's own %s('%s') (#%llu) -- the host fires every event",
                special ? "runSpecialEvent" : "runEvent", ev.c_str(), static_cast<unsigned long long>(n));
    }
    return sg::Verdict::Cancel;
}

// The native fire, game thread. A dispatch that faults is said and not re-run.
bool NativeFire(FireKind kind, const std::wstring& eventName, const std::wstring& specialName,
                bool* attempted = nullptr) {
    if (attempted) *attempted = false;
    auto* session = g_session.load(std::memory_order_acquire);
    const auto role = session ? session->role() : net::Role::Host;
    const bool running = session && session->running(), connected = session && session->connected();
    const auto epoch = g_replayEpoch;
    const auto world = ue_wrap::world_identity::Generation();
    const auto serial = net::session_serial::Current();
    void* eventer = EventerOf(ue_wrap::world_singleton::Gamemode());
    if (!eventer) {
        UE_LOGW("event_fire: no live trigger_eventer -- native fire dropped ('%ls')", eventName.c_str());
        return false;
    }
    ue_wrap::CachedObjRef identity; identity.Set(eventer);
    const auto current = [&] {
        return epoch == g_replayEpoch && g_session.load(std::memory_order_acquire) == session &&
            world == ue_wrap::world_identity::Generation() && serial == net::session_serial::Current() &&
            identity.Get() == eventer && (!session || (session->role() == role &&
                session->running() == running && (!connected || session->connected())));
    };
    const char* verb = kind == FireKind::SpecialEvent ? "runSpecialEvent" : "runEvent";
    void* fn = kind == FireKind::SpecialEvent ? g_runSpecialEventFn : g_runEventFn;
    if (!fn) { UE_LOGW("event_fire: %s unresolved", verb); return false; }
    ue_wrap::ParamFrame f(fn);
    if (!f.valid()) return false;
    // A missed Set leaves a half-written frame the body reads garbage from: do not dispatch.
    const R::FName ev = ue_wrap::fname_utils::StringToFName(eventName);
    bool filled;
    if (kind == FireKind::SpecialEvent) {
        filled = f.Set<R::FName>(L"eventName1", ev);
    } else {
        filled = f.Set<R::FName>(L"event", ev) &&
                 f.Set<R::FName>(L"special", ue_wrap::fname_utils::StringToFName(specialName));
    }
    if (!filled || !current()) {
        UE_LOGW("event_fire: %s('%ls') param frame would not fill -- dispatch dropped",
                verb, eventName.c_str());
        return false;
    }
    Admission adm{eventer, fn};
    struct Scope {
        Admission* prev;
        explicit Scope(Admission* a) : prev(g_admission) { g_admission = a; }
        ~Scope() { g_admission = prev; }
    } scope(&adm);
    if (attempted) *attempted = true;
    if (!ue_wrap::Call(eventer, f)) {
        UE_LOGW("event_fire: %s('%ls') dispatch FAILED", verb, eventName.c_str());
        return false;
    }
    if (!current()) return false;
    // On a client the gate admits this call and nothing else; a live watch that did not admit it
    // refused it, so the body did not run.
    if (IsClientSession() && WatchLive(kind) && !adm.spent) {
        UE_LOGE("event_fire: %s('%ls') was refused at the gate -- the admission did not match the "
                "call; not retried", verb, eventName.c_str());
        return false;
    }
    if (kind == FireKind::SpecialEvent)
        UE_LOGI("event_fire: runSpecialEvent('%ls') dispatched", eventName.c_str());
    else
        UE_LOGI("event_fire: runEvent('%ls', special='%ls') dispatched",
                eventName.c_str(), specialName.c_str());
    return true;
}

void Broadcast(FireKind kind, const std::string& name) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Host) return;
    coop::net::EventFirePayload p{};  // zero-init -> name[] pre-NUL-bound
    p.dispatch = static_cast<uint8_t>(kind);
    const size_t n = name.size() < sizeof(p.name) - 1 ? name.size() : sizeof(p.name) - 1;
    std::memcpy(p.name, name.c_str(), n);
    delivery::Publish(p);
    UE_LOGI("event_fire: broadcast %s '%s'",
            kind == FireKind::SpecialEvent ? "runSpecialEvent" : "runEvent", name.c_str());
}

// Publish only after the native body commits. Client replay cannot echo.
void OnRunSpecialEventPost(const sg::Call& call) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || s->role() != coop::net::Role::Host || !call.locals ||
        !ResolvePass() || call.function != g_runSpecialEventFn || call.object != EventerOf(ue_wrap::world_singleton::Gamemode())) return;
    // Send the case the host actually chose, including nested prank rolls.
    // Clients never call summonArirPrank and therefore never reroll the case.
    const auto name = NarrowName(*reinterpret_cast<const R::FName*>(call.locals + g_offSpecialParam));
    Broadcast(FireKind::SpecialEvent, name);
}
void OnRunEventPost(const sg::Call& call) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return;
    if (!ResolvePass() || call.function != g_runEventFn || call.object != EventerOf(ue_wrap::world_singleton::Gamemode())) return;
    if (!call.locals || g_offEventParam < 0) return;
    const std::string name = NarrowName(*reinterpret_cast<const R::FName*>(call.locals + g_offEventParam));
    if (call.callerFunction == g_settimeFn) {
        if (s->role() != coop::net::Role::Host) return;
        UE_LOGI("event_fire: host OBSERVED scheduler fire '%s' (settime -> runEvent)", name.c_str());
        Broadcast(FireKind::RunEvent, name);
        return;
    }
    // A direct native fire belongs to this dispatch's admission scope. This
    // covers F1 HostFire and a host executing an accepted location request.
    if (s->role() == coop::net::Role::Host && g_admission &&
        g_admission->object == call.object && g_admission->function == call.function) {
        Broadcast(FireKind::RunEvent, name);
        return;
    }
    // A reflected clock update may contain a genuine host scheduler fire. Observe
    // that above, but do not echo our direct network/dev replay below.
    if (call.fromOurCode) return;
    // The host's broadcast already ran this row here; another overlap must not
    // start it a second time.
    if (s->role() != coop::net::Role::Host && g_replayed.count(name)) return;
    // A player walked into it. A personal scare stays on that machine. A row the replay
    // policy runs on every machine is a world change (the vent break). A location event still
    // uses its native activator when its creatures already ride another lane.
    const char* lane = nullptr;
    const bool replay = ReplayVerdict(name, &lane) == 1;
    if (PersonalOnly(name) || !replay) return;
    if (event_location_sync::IsLocationRow(name.c_str())) {
        if (s->role()==coop::net::Role::Host && replay)Broadcast(FireKind::RunEvent,name);
        return; // arm now; event_location_sync handles the real activation
    }
    if (GatherRecently(name)) {
        // Another overlap in the same volume must not repeat an accepted event.
        return;
    }
    if (s->role() == coop::net::Role::Host) {
        MarkGather(name);
        if (replay) {
            Broadcast(FireKind::RunEvent, name);
            UE_LOGI("event_fire: shared event '%s' on the host -- every machine runs it",
                        name.c_str());
        }
    } else if (SendGather(s, name)) {
        // This machine already runs the row. The host's copy of the fire must not run it again here.
        if (replay) g_replayed.insert(name);
        MarkGather(name);
    }
    return;
}

// True iff the client's own passEvents already contains the row (the transferred save
// carried this fire; its world effects are already in the loaded state).
bool InClientPassEvents(const std::string& name) {
    RawArray* pass = ArrayAt(SaveSlotOf(ue_wrap::world_singleton::Gamemode()), g_offPassEvents);
    if (!pass || !pass->Data || pass->Num <= 0 || pass->Num > 100000) return false;
    std::wstring w(name.begin(), name.end());
    const R::FName want = ue_wrap::fname_utils::StringToFName(w);
    for (int32_t i = 0; i < pass->Num; ++i) {
        const R::FName& e = pass->Data[i];
        if (e.ComparisonIndex == want.ComparisonIndex && e.Number == want.Number) return true;
    }
    return false;
}

// The replay step's result, game thread: Done pops the row, NotReady waits it (uncounted and paced),
// PermFailed drops it -- the event classes can never resolve on this build.
enum class ReplayStep { Done, NotReady, PermFailed };

// A terminal step drops the row outright and says why once, here.
bool DropIfTerminal(ReplayStep step, const PendingFire& pf) {
    if (step != ReplayStep::PermFailed) return false;
    UE_LOGE("event_fire: replay of '%s' DROPPED -- the event classes never resolved "
            "(permanent failure); the row could never dispatch", pf.name.c_str());
    return true;
}

// The client replay executor, game thread.
ReplayStep TryReplay(const PendingFire& pf) {
    if (!IsClientSession()) return ReplayStep::NotReady;
    auto* session = g_session.load(std::memory_order_acquire);
    const auto epoch = g_replayEpoch, serial = uint64_t(net::session_serial::Current());
    const auto world = ue_wrap::world_identity::Generation();
    const auto current = [&] {
        return epoch == g_replayEpoch && session == g_session.load(std::memory_order_acquire) &&
            world == ue_wrap::world_identity::Generation() && serial == net::session_serial::Current() && IsClientSession();
    };
    if (!ResolvePass() || !EventerOf(ue_wrap::world_singleton::Gamemode()))
        return g_resolveFailed ? ReplayStep::PermFailed : ReplayStep::NotReady;
    // A replay dispatches only into the CURRENT gameplay world: while it is Unknown or
    // another, the singleton lookup can still answer the eventer of the world being left
    // (the cached ref fails open on a null current world).
    if (ue_wrap::world_identity::CurrentWorldKind() != ue_wrap::world_identity::WorldKind::Gameplay)
        return ReplayStep::NotReady;
    if (g_boundGen == 0) g_boundGen = ue_wrap::world_identity::Generation();
    // Dedupe applies to one-shot scheduled rows only (the game's own passEvents semantics);
    // specials (graffiti, pranks the menu re-fires) are repeatable by design.
    if (pf.kind == FireKind::RunEvent) {
        if (g_replayed.count(pf.name)) {
            UE_LOGI("event_fire: '%s' already replayed this session -- skipping", pf.name.c_str());
            return ReplayStep::Done;
        }
        // A passEvents hit marks nothing: the skip must stay re-decidable, since marking here would
        // let a history-skipped fire permanently block a later in-flight override for the same row
        // (a fire landing between the joiner's connect and its blob capture rides both the wire and
        // the blob). A duplicate just rescans the array; passEvents never shrinks mid-session.
        if (!pf.activeOverride && InClientPassEvents(pf.name)) {
            UE_LOGI("event_fire: '%s' already in local passEvents (save carried it) -- skipping",
                    pf.name.c_str());
            return ReplayStep::Done;
        }
    }
    const std::wstring w(pf.name.begin(), pf.name.end());
    UE_LOGI("event_fire: client REPLAY %s '%s'%s",
            pf.kind == FireKind::SpecialEvent ? "runSpecialEvent" : "runEvent", pf.name.c_str(),
            pf.activeOverride ? " (in-flight active-override)" : "");
    // The special is always None: the only native special is the prank roll, whose chosen case
    // the host's runSpecialEvent watch sends on its own. Marked consumed only on a successful
    // preparation. A call that was actually attempted may have partial effects
    // after a fault, so its one-shot row is consumed even when completion is unknown.
    if (!current()) return ReplayStep::Done;
    bool attempted = false;
    const bool completed = NativeFire(pf.kind, w, L"None", &attempted);
    if (current() && attempted && pf.kind == FireKind::RunEvent) g_replayed.insert(pf.name);
    if (completed && current()) {
        g_replays.fetch_add(1, std::memory_order_relaxed);
    }
    // Preparation failure is retryable. Once native dispatch was attempted its
    // side effects may already exist, even if Call faulted; never repeat it.
    return attempted || !current() ? ReplayStep::Done : ReplayStep::NotReady;
}

// A queued row that could not run yet: stamp the pacing of its next try.
void NoteAttempt(PendingFire& pf) {
    pf.attempted = true;
    pf.lastAttempt = std::chrono::steady_clock::now();
}

// Before a drain and before a new fire is queued: rows and dedupe bound to a world since left
// describe it, not this one, so they close out here. An unbound queue (the join window) has no
// world to be stale in and is left alone, as is the queue through the travel itself -- the
// generation only moves once the new world is observed.
void DropStaleWorldQueue() {
    if (g_boundGen == 0) return;
    const uint32_t gen = ue_wrap::world_identity::Generation();
    if (gen == g_boundGen) return;
    UE_LOGI("event_fire: world changed (gen %u -> %u) -- dropped %zu queued fire(s) and cleared "
            "the replayed set (%zu entr(ies)); the new join queue stays unbound",
            g_boundGen, gen, g_pending.size(), g_replayed.size());
    g_pending.clear();
    ++g_replayEpoch;
    g_replayed.clear();
    g_boundGen = 0;
}

// CLIENT: replay the queued fires in arrival order, stopping at the first the eventer cannot take
// yet; a front row that already tried waits out its spacing first. Run before a new fire is
// handled, at the client's world-ready announce, and every install tick (the lasting retry).
void DrainPending() {
    if (g_drainingReplay || !IsClientSession()) return;
    g_drainingReplay = true;
    struct Drain { ~Drain() { g_drainingReplay = false; } } drain;
    DropStaleWorldQueue();
    const auto epoch = g_replayEpoch;
    const auto world = ue_wrap::world_identity::Generation();
    const auto serial = net::session_serial::Current();
    const auto now = std::chrono::steady_clock::now();
    for (int budget = 0; !g_pending.empty() && budget < 8; ++budget) {
        const PendingFire front = g_pending.front();
        // A latched resolve failure is permanent -- no dispatch is possible on this build, so
        // the row drops outright instead of wedging the FIFO behind a forever NotReady, with no
        // spacing to wait out first.
        if (front.attempted && !g_resolveFailed && now - front.lastAttempt < kReplayRetrySpacing)
            return;
        const ReplayStep step = TryReplay(front);
        if (epoch != g_replayEpoch || world != ue_wrap::world_identity::Generation() ||
            serial != net::session_serial::Current() || !IsClientSession()) return;
        if (g_pending.empty() || g_pending.front().id != front.id) return;
        if (step == ReplayStep::Done) {
            g_pending.pop_front();
            continue;
        }
        if (DropIfTerminal(step, front)) {
            g_pending.pop_front();
            continue;
        }
        NoteAttempt(g_pending.front());
        return;
    }
}

}  // namespace

void Install(coop::net::Session* session) {
    const auto serial = net::session_serial::Current();
    if (g_session.load(std::memory_order_acquire) != session || g_replaySerial != serial) {
        ++g_replayEpoch;
        g_pending.clear(); g_replayed.clear(); g_boundGen = 0;
    }
    g_replaySerial = serial;
    g_session.store(session, std::memory_order_release);
    delivery::Install(session);
    coop::event_schedule_authority::Install(session);
    // Called every pump tick; retry registration and name resolution as classes load.
    if (!g_watchInstalled)
        g_watchInstalled = sg::WatchName(kRowRunEvent, kTagEventFire, &OnEventerVerbPre, &OnRunEventPost);
    if (!g_specialWatchInstalled)
        g_specialWatchInstalled =
            sg::WatchName(kRowRunSpecialEvent, kTagEventFire, &OnEventerVerbPre, &OnRunSpecialEventPost);
    const bool runPending = g_watchInstalled && !g_watchLive && !g_watchDead;
    const bool specialPending = g_specialWatchInstalled && !g_specialWatchLive && !g_specialWatchDead;
    if (runPending || specialPending) sg::ResolvePendingNames();
    if (runPending) {
        if (sg::NameWatchLive(kRowRunEvent, kTagEventFire)) {
            g_watchLive = true;
            UE_LOGI("event_fire: the host's fires are seen at runEvent (a script-gate watch)");
        } else if (sg::NameWatchSettled(kRowRunEvent, kTagEventFire)) {
            g_watchDead = true;
            UE_LOGE("event_fire: the runEvent watch settled dead -- a host's fires do not cross, and "
                    "a client's own are not refused, this session");
        }
    }
    if (specialPending) {
        if (sg::NameWatchLive(kRowRunSpecialEvent, kTagEventFire)) g_specialWatchLive = true;
        else if (sg::NameWatchSettled(kRowRunSpecialEvent, kTagEventFire)) {
            g_specialWatchDead = true;
            UE_LOGE("event_fire: runSpecialEvent watch settled dead; special fires cannot cross");
        }
    }
    if (IsClientSession()) DrainPending();
}

bool HostFire(FireKind kind, const std::wstring& eventName, const std::wstring& specialName) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (s && s->running() && s->role() != coop::net::Role::Host) {
        UE_LOGW("event_fire: HostFire refused -- running as a client (host is authoritative)");
        return false;
    }
    // Session objects can be reused without a world change. Stamp the start as well as the pointer.
    const uint32_t gen = ue_wrap::world_identity::Generation();
    const uint32_t serial = coop::net::session_serial::Current();
    const bool wasRunning = s && s->running();
    const bool wasConnectedHost = s && s->connected() && s->role() == coop::net::Role::Host;
    const std::wstring ev = eventName;
    const std::wstring sp = specialName.empty() ? L"None" : specialName;
    GT::Post([kind, ev, sp, s, gen, serial, wasRunning, wasConnectedHost] {
        auto* cur = g_session.load(std::memory_order_acquire);
        if (cur && cur->running() && cur->role() != coop::net::Role::Host) {
            UE_LOGW("event_fire: HostFire('%ls') dropped -- the session is a running CLIENT now",
                    ev.c_str());
            return;
        }
        if (cur != s || ue_wrap::world_identity::Generation() != gen ||
            coop::net::session_serial::Current() != serial) {
            UE_LOGW("event_fire: HostFire('%ls') dropped -- the world or the session changed "
                    "between submit and dispatch", ev.c_str());
            return;
        }
        // A fire submitted by a starting or connected host dies with that session.
        if ((wasRunning && !(cur && cur->running())) ||
            (wasConnectedHost && !(cur && cur->connected()))) {
            UE_LOGW("event_fire: HostFire('%ls') dropped -- the submitting host session ended "
                    "before dispatch", ev.c_str());
            return;
        }
        // Resolve here, not before the post: a solo host's dev menu has no session. The native fire
        // warns loudly if the world or the eventer is not up.
        if (!ResolvePass()) {
            UE_LOGW("event_fire: HostFire('%ls') -- the event classes are not loaded", ev.c_str());
            return;
        }
        // A connected host reaches the wire only through the watches, so an unobserved fire
        // would run the event and tell nobody -- it is refused rather than waited out. Solo
        // keeps the unobserved dispatch path.
        if (cur && cur->connected() && !WatchLive(kind)) {
            UE_LOGW("event_fire: HostFire('%ls') dropped -- the %s watch is not live (a connected "
                    "host fires only through it)", ev.c_str(),
                    kind == FireKind::SpecialEvent ? "runSpecialEvent" : "runEvent");
            return;
        }
        // No send of its own: the runEvent/runSpecialEvent watches see this reflected call like
        // any other host fire and emit it once, after its body (a dev fire is the 'dev-call' origin).
        NativeFire(kind, ev, sp);
    });
    return true;
}

void OnReliable(const coop::net::EventFirePayload& payload) {
    if (!GT::IsGameThread()) { UE_LOGW("event_fire: OnReliable off-game-thread -- dropping"); return; }
    if (!IsClientSession()) return;
    // NUL-bound the name (the payload crosses the trust boundary; the dispatcher length-checked
    // it).
    char buf[sizeof(payload.name) + 1] = {};
    std::memcpy(buf, payload.name, sizeof(payload.name));
    const std::string name(buf);
    if (name.empty() || payload.dispatch > static_cast<uint8_t>(FireKind::SpecialEvent)) {
        UE_LOGW("event_fire: OnReliable malformed (dispatch=%u, name='%s') -- dropping",
                payload.dispatch, name.c_str());
        return;
    }
    const FireKind kind = static_cast<FireKind>(payload.dispatch);
    const char* lane = nullptr;
    const int verdict = ReplayVerdict(name, &lane);
    if (verdict == 0) {
        UE_LOGI("event_fire: '%s' NOT replayed (%s)", name.c_str(), lane);
        return;
    }
    if (verdict < 0) {
        UE_LOGW("event_fire: '%s' not in the replay policy -- default NO-replay (newer host? "
                "add the row's verdict)", name.c_str());
        return;
    }
    PendingFire pf{ kind, name }; pf.id = ++g_replayId;
    // One-shot rows dedupe at queue time too (a scheduler re-fire of a dev-fired row during the
    // same load window would otherwise queue twice; the replay would catch it later, but a
    // duplicate-free queue keeps the cap honest).
    if (kind == FireKind::RunEvent) {
        for (const auto& q : g_pending)
            if (q.kind == kind && q.name == name) return;
    }
    if (g_pending.size() >= kMaxPending) {
        UE_LOGW("event_fire: pending replay queue full (%zu) -- dropping '%s'",
                g_pending.size(), name.c_str());
        return;
    }
    UE_LOGI("event_fire: queued '%s' (%zu pending; the world or eventer is not up yet)",
            name.c_str(), g_pending.size() + 1);
    g_pending.push_back(std::move(pf));
    DrainPending();
}

void ReplayInFlightRow(const std::string& rowName) {
    if (!GT::IsGameThread()) { UE_LOGW("event_fire: ReplayInFlightRow off-game-thread -- dropping"); return; }
    if (!IsClientSession()) return;
    if (rowName.empty()) return;
    const char* lane = nullptr;
    const int verdict = ReplayVerdict(rowName, &lane);
    if (verdict == 0) {
        UE_LOGI("event_fire: in-flight '%s' NOT replayed (%s)", rowName.c_str(), lane);
        return;
    }
    if (verdict < 0) {
        UE_LOGW("event_fire: in-flight '%s' not in the replay policy -- default NO-replay "
                "(add the row's verdict)", rowName.c_str());
        return;
    }
    PendingFire pf{ FireKind::RunEvent, rowName, /*activeOverride=*/true }; pf.id = ++g_replayId;
    // The world or eventer is not up yet. If the row is already queued (a fire copy
    // landed in the pre-world window), upgrade it in place: two entries would double-dispatch,
    // and the plain copy alone could history-skip the in-flight replay.
    for (auto& q : g_pending) {
        if (q.kind == FireKind::RunEvent && q.name == rowName) {
            q.activeOverride = true;
            UE_LOGI("event_fire: in-flight '%s' already queued -- upgraded to active-override",
                    rowName.c_str());
            return;
        }
    }
    if (g_pending.size() >= kMaxPending) {
        UE_LOGW("event_fire: pending replay queue full (%zu) -- dropping in-flight '%s'",
                g_pending.size(), rowName.c_str());
        return;
    }
    UE_LOGI("event_fire: queued in-flight '%s' (%zu pending)",
            rowName.c_str(), g_pending.size() + 1);
    g_pending.push_back(std::move(pf));
    DrainPending();
}

void OnClientWorldReady() {
    if (g_pending.empty()) return;
    const size_t before = g_pending.size();
    DrainPending();
    if (g_pending.empty())
        UE_LOGI("event_fire: world ready -- the %zu queued fire(s) processed", before);
    else
        UE_LOGI("event_fire: world ready with %zu of %zu queued fire(s) still pending -- "
                "the install pump retries them each second", g_pending.size(), before);
}

void OnDisconnect() {
    ++g_replayEpoch;
    delivery::Clear();
    coop::event_schedule_authority::OnDisconnect();
    g_pending.clear();
    g_replayed.clear();
    g_gatherMs.clear();
    g_boundGen = 0;
    g_session.store(nullptr, std::memory_order_release);
}

void OnLocationGather(const coop::net::EventGatherPayload& payload, uint8_t senderSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Host) return;
    if (senderSlot == 0 || senderSlot >= coop::players::kMaxPeers) return;
    char buf[sizeof(payload.name)];
    std::memcpy(buf, payload.name, sizeof(buf));
    buf[sizeof(buf) - 1] = 0;
    const std::string name(buf);
    const char* lane = nullptr;
    const bool replay = ReplayVerdict(name, &lane) == 1;
    if (PersonalOnly(name) || event_location_sync::IsLocationRow(name.c_str()) || !replay || GatherRecently(name)) return;
    float x = 0, y = 0, z = 0, yaw = 0;
    if (!GatherPoint(senderSlot, payload.x, payload.y, payload.z, payload.yaw, x, y, z, yaw)) {
        UE_LOGW("event_fire: shared event '%s' from slot %u ignored -- their body is not here yet",
                name.c_str(), static_cast<unsigned>(senderSlot));
        return;
    }
    MarkGather(name);
    if (!replay) return;
    const std::wstring w(name.begin(), name.end());
    // The sender's machine already ran it. This machine and the other clients still have to.
    NativeFire(FireKind::RunEvent, w, L"None");
    UE_LOGI("event_fire: shared event '%s' from slot %u -- every machine runs it",
                name.c_str(), static_cast<unsigned>(senderSlot));
}

unsigned ReplayCount() { return g_replays.load(std::memory_order_relaxed); }

bool DevProbeClientRefusal() {
    if (!IsClientSession() || !WatchLive(FireKind::RunEvent)) return false;
    if (!ResolvePass()) return false;
    void* eventer = EventerOf(ue_wrap::world_singleton::Gamemode());
    ue_wrap::ParamFrame frame(g_runEventFn);
    if (!eventer || !frame.valid() || !frame.Set<R::FName>(L"event", ue_wrap::fname_utils::StringToFName(L"solar")) ||
        !frame.Set<R::FName>(L"special", ue_wrap::fname_utils::StringToFName(L"None"))) return false;
    const uint64_t before = g_clientRefused;
    ue_wrap::Call(eventer, frame); // deliberately outside NativeFire's admission scope
    return g_clientRefused > before;
}

}  // namespace coop::event_fire_sync
