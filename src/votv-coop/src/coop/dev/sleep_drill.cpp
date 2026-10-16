// coop/dev/sleep_drill.cpp -- see coop/dev/sleep_drill.h.

#include "coop/dev/sleep_drill.h"

#include "coop/config/config.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/player/sleep_sync.h"
#include "coop/props/prop_snapshot.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"

#include "ue_wrap/actors/sleep.h"
#include "ue_wrap/actors/vitals.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/world/active_events.h"
#include "ue_wrap/world/daynightcycle.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace coop::dev::sleep_drill {
namespace {

namespace SLP = ue_wrap::sleep;
namespace V   = ue_wrap::vitals;
namespace DNC = ue_wrap::daynightcycle;
namespace AE  = ue_wrap::active_events;
namespace R   = ue_wrap::reflection;
namespace OI  = ue_wrap::object_index;
using Clock = std::chrono::steady_clock;

enum class Arm { Off, HostAsleep, ClientRest };

// The host: the join, a quiet world, its bed and its sleep, then (client_rest) the client's rest. The client:
// the join, the host's sleep and then (host_asleep) ten seconds of its clock, or (client_rest) the host's wake,
// ten seconds of awake food and its own rest.
enum class Step {
    WaitJoin, HostQuiet, HostAsleep, HostWaitClientRest, HostWaitClientWake,
    ClientWaitHost, ClientSample, ClientWaitHostWake, ClientAwake, ClientQuiet, ClientResting, Done
};

constexpr float kNeed          = 30.f;   // the need both peers sleep from; the wake loop ends a sleep at 100
constexpr float kNativeFactor  = 20.f;   // the game's sleep dilation (mainGamemode.cpp:5428)
constexpr float kHostFactorTol = 0.5f;
constexpr float kWorldTol      = 0.01f;  // a client's world must read 1
constexpr float kRestedNeed    = 99.f;   // a rest that reached it ended on the game's own wake, not on an interrupt
constexpr float kClockShare    = 0.9f;   // of the native rate the client's clock must show while the host sleeps
constexpr float kFoodShare     = 0.25f;  // the food rate asleep may differ from awake by this much
constexpr auto  kWindow        = std::chrono::seconds(10);
constexpr auto  kSample        = std::chrono::seconds(1);
constexpr auto  kJoinBound     = std::chrono::seconds(300);
constexpr auto  kQuietBound    = std::chrono::seconds(120);
constexpr auto  kSleepBound    = std::chrono::seconds(240);
constexpr auto  kPeerBound     = std::chrono::seconds(120);

std::atomic<coop::net::Session*> g_session{nullptr};
Step        g_step = Step::WaitJoin;
const char* g_waitingFor = "the join";
Clock::time_point g_deadline{};
bool        g_saidArm = false;
int         g_slot = -1;
bool        g_dreamHeld = false;  // the host's nightmare chance is held at 0

// The host's sleep.
float             g_hostDilation = 0.f;
float             g_hostNeed0 = 0.f;
Clock::time_point g_hostT0{};

// The client's reads of its world's dilation across a window, and of its clock.
float             g_dilMin = 0.f, g_dilMax = 0.f;
int               g_dilReads = 0;
Clock::time_point g_nextRead{};
Clock::time_point g_t0{};
float             g_total0 = 0.f;
float             g_perGameSecond = 0.f;  // clock units a game second adds at the native timescale of 1
// client_rest: the awake food window, then the rest.
float             g_awakeFood0 = 0.f;
float             g_awakeRate = 0.f;
float             g_restFood0 = 0.f;
float             g_restNeed0 = 0.f;

Arm ArmOf() {
    static const Arm a = [] {
        const std::string v = coop::config::ResolveEnum(::coop::config_registry::rows::sleep_drill);
        return v == "host_asleep" ? Arm::HostAsleep : v == "client_rest" ? Arm::ClientRest : Arm::Off;
    }();
    return a;
}

const char* ArmName() { return ArmOf() == Arm::HostAsleep ? "host_asleep" : ArmOf() == Arm::ClientRest ? "client_rest" : "off"; }

bool IsHost(coop::net::Session* s) { return s->role() == coop::net::Role::Host; }
const char* RoleName(bool host) { return host ? "host" : "client"; }

void Enter(Step step, const char* waitingFor, std::chrono::seconds bound) {
    g_step = step;
    g_waitingFor = waitingFor;
    g_deadline = Clock::now() + bound;
}

void Say(bool host, bool pass, const char* fmt, ...) {
    char buf[384];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (pass) UE_LOGI("[SLEEP-DRILL] %s %s PASS %s", ArmName(), RoleName(host), buf);
    else UE_LOGW("[SLEEP-DRILL] %s %s FAIL %s", ArmName(), RoleName(host), buf);
}

void RestoreDream() {
    if (!g_dreamHeld) return;
    SLP::SetDreamProbability(-1.f);
    g_dreamHeld = false;
}

// The arm is over for this peer. The host's last line is its DONE.
void Finish(bool host) {
    if (host) {
        RestoreDream();
        UE_LOGI("[SLEEP-DRILL] %s host DONE", ArmName());
    }
    g_step = Step::Done;
}

void Fail(bool host, const char* fmt, ...) {
    char buf[320];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Say(host, false, "%s", buf);
    Finish(host);
}

float ReadField(V::Field f) {
    float v = -1.f;
    V::Read(f, &v);
    return v;
}

// What the sleep entry refuses on, for a refusal's line.
void InputsInto(char* buf, size_t n) {
    int32_t events = -1;
    AE::ReadCount(events);
    std::snprintf(buf, n, "food %.1f need %.1f activeEvents %d isSleep %d", ReadField(V::Field::Food),
                  ReadField(V::Field::Sleep), events, SLP::IsSleeping() ? 1 : 0);
}

// ---- the nearest bed -------------------------------------------------------------------------

struct BedPick {
    void* base = nullptr;
    std::vector<void*> classes;
    ue_wrap::FVector from;
    void* best = nullptr;
    float bestD2 = 0.f;
};

void OnBedClass(void* ctx, void* cls, void*) {
    auto* p = static_cast<BedPick*>(ctx);
    if (R::IsDescendantOfAny(cls, &p->base, 1)) p->classes.push_back(cls);
}

void OnBedInstance(void* ctx, void* obj, int32_t) {
    auto* p = static_cast<BedPick*>(ctx);
    if (R::NameStartsWith(R::NameOf(obj), L"Default__")) return;
    ue_wrap::FVector at;
    if (!ue_wrap::engine::TryGetActorLocation(obj, at)) return;
    const float dx = at.X - p->from.X, dy = at.Y - p->from.Y, dz = at.Z - p->from.Z;
    const float d2 = dx * dx + dy * dy + dz * dz;
    if (!p->best || d2 < p->bestD2) {
        p->best = obj;
        p->bestD2 = d2;
    }
}

// The bed (bed_C or a descendant) nearest this peer's body, or null.
void* NearestBed() {
    BedPick pick;
    pick.base = OI::ClassByName(L"bed_C");
    void* body = coop::players::Registry::Get().Local();
    if (!pick.base || !body || !ue_wrap::engine::TryGetActorLocation(body, pick.from)) return nullptr;
    OI::ForEachClass(&OnBedClass, &pick);
    for (void* cls : pick.classes) OI::ForEachInstance(cls, &OnBedInstance, &pick);
    return pick.best;
}

// ---- the host ---------------------------------------------------------------------------------

// Awake, a quiet world, then the bed: the entry refuses with an event active, food at 10, no floor.
void HostGoToBed() {
    int32_t events = -1;
    if (!AE::ReadCount(events) || events > 0) return;  // the bound names the wait
    V::Write(V::Field::Sleep, kNeed);
    if (SLP::SetDreamProbability(0.f)) g_dreamHeld = true;
    void* bed = SLP::FindBed();
    if (!bed) return Fail(true, "no bed_C in the world");
    const bool called = SLP::CallSleep(bed);
    char in[128];
    InputsInto(in, sizeof(in));
    if (!called) return Fail(true, "the sleep call did not dispatch | %s", in);
    if (!SLP::IsSleeping()) return Fail(true, "the sleep call was refused | %s", in);
    g_hostDilation = SLP::GetGlobalTimeDilation();
    g_hostNeed0 = ReadField(V::Field::Sleep);
    g_hostT0 = Clock::now();
    UE_LOGI("sleep_drill: [host] asleep -- world dilation %.2f (the game's is %.0f), need %.1f | %s",
            g_hostDilation, kNativeFactor, g_hostNeed0, in);
    Enter(Step::HostAsleep, "the host to wake", kSleepBound);
}

void HostWatchWake() {
    if (SLP::IsSleeping()) return;
    const float secs = std::chrono::duration<float>(Clock::now() - g_hostT0).count();
    const float need = ReadField(V::Field::Sleep);
    const bool pass = std::fabs(g_hostDilation - kNativeFactor) <= kHostFactorTol && need >= kRestedNeed;
    Say(true, pass, "T_host=%.1f s, world dilation %.2f asleep (the game's is %.0f), need %.1f -> %.1f%s", secs,
        g_hostDilation, kNativeFactor, g_hostNeed0, need, need >= kRestedNeed ? "" : " (woken before it was rested)");
    if (!pass || ArmOf() == Arm::HostAsleep) return Finish(true);
    Enter(Step::HostWaitClientRest, "the client to rest", kPeerBound + kWindow);
}

void TickHost(coop::net::Session* s) {
    namespace SS = coop::sleep_sync;
    switch (g_step) {
    case Step::WaitJoin:
        for (int slot = 1; slot < static_cast<int>(coop::players::kMaxPeers) && g_slot < 0; ++slot)
            if (s->IsSlotWorldReady(slot) && coop::prop_snapshot::IsBracketClosed(slot)) g_slot = slot;
        if (g_slot < 0) return;
        UE_LOGI("sleep_drill: [host] slot %d's join is over; waiting for no event, then to bed", g_slot);
        Enter(Step::HostQuiet, "no active event", kQuietBound);
        return;
    case Step::HostQuiet:     HostGoToBed(); return;
    case Step::HostAsleep:    HostWatchWake(); return;
    case Step::HostWaitClientRest:
        if (SS::PeerState(g_slot) == coop::net::SleepPeerState::Awake) return;
        Enter(Step::HostWaitClientWake, "the client's rest to end", kSleepBound);
        return;
    case Step::HostWaitClientWake:
        if (SS::PeerState(g_slot) != coop::net::SleepPeerState::Awake) return;
        Finish(true);
        return;
    default: return;
    }
}

// ---- the client -------------------------------------------------------------------------------

// One read of this client's world dilation into the window's min and max; false when it did not read.
bool ReadWorld(float* out) {
    const float d = SLP::GetGlobalTimeDilation();
    if (d <= 0.f) return false;
    g_dilMin = g_dilReads == 0 ? d : std::fmin(g_dilMin, d);
    g_dilMax = g_dilReads == 0 ? d : std::fmax(g_dilMax, d);
    ++g_dilReads;
    if (out) *out = d;
    return true;
}

bool WorldHeldAtOne() { return g_dilMin >= 1.f - kWorldTol && g_dilMax <= 1.f + kWorldTol; }

// The host's flag has risen: the clock and the world's dilation are read for the window.
void ClientBeginSample() {
    float total = 0.f, day = 0.f, scale = 0.f;
    DNC::Rates rt{};
    if (!DNC::ReadClock(total, day, scale) || !DNC::ReadRates(rt)) return;  // the bound names the wait
    g_perGameSecond = rt.diffMult * rt.settingMultiplayer * rt.sleepingTimeDilation;
    if (g_perGameSecond <= 0.f) return Fail(false, "the clock's rates read %.3f: no native rate to compare with", g_perGameSecond);
    g_dilReads = 0;
    if (!ReadWorld(nullptr)) return Fail(false, "the world's dilation did not read");
    g_total0 = total;
    g_t0 = Clock::now();
    g_nextRead = g_t0 + kSample;
    Enter(Step::ClientSample, "the clock window to run", kPeerBound);
}

void ClientSample() {
    const auto now = Clock::now();
    if (now < g_nextRead) return;
    g_nextRead = now + kSample;
    if (!ReadWorld(nullptr)) return Fail(false, "the world's dilation did not read");
    if (!coop::sleep_sync::HostAsleepNow())
        return Fail(false, "the host woke after %.1f s, before the window ended",
                    std::chrono::duration<float>(now - g_t0).count());
    if (now - g_t0 < kWindow) return;
    float total = 0.f, day = 0.f, scale = 0.f;
    if (!DNC::ReadClock(total, day, scale)) return;
    const float real = std::chrono::duration<float>(now - g_t0).count();
    const float advance = total - g_total0;
    const float factor = advance / (g_perGameSecond * real);
    const bool pass = WorldHeldAtOne() && factor >= kClockShare * kNativeFactor;
    Say(false, pass, "world dilation %.2f..%.2f over %d reads, clock +%.1f in %.1f s = x%.1f the native rate (needs x%.1f)",
        g_dilMin, g_dilMax, g_dilReads, advance, real, factor, kClockShare * kNativeFactor);
    Finish(false);
}

void ClientGoToBed() {
    int32_t events = -1;
    if (!AE::ReadCount(events) || events > 0) return;  // the bound names the wait
    V::Write(V::Field::Sleep, kNeed);
    g_restNeed0 = ReadField(V::Field::Sleep);
    g_restFood0 = ReadField(V::Field::Food);
    void* bed = NearestBed();
    if (!bed) return Fail(false, "no bed_C in the world, or no local body to measure from");
    const bool called = SLP::CallSleep(bed);
    char in[128];
    InputsInto(in, sizeof(in));
    if (!called) return Fail(false, "the sleep call did not dispatch | %s", in);
    if (!SLP::IsSleeping()) return Fail(false, "the sleep call was refused | %s", in);
    g_dilReads = 0;
    float dilation = 0.f;
    if (!ReadWorld(&dilation)) return Fail(false, "the world's dilation did not read");
    g_t0 = Clock::now();
    g_nextRead = g_t0 + kSample;
    UE_LOGI("sleep_drill: [client] in bed -- world dilation %.2f right after the call (a native sleep sets %.0f), "
            "need %.1f food %.2f | %s", dilation, kNativeFactor, g_restNeed0, g_restFood0, in);
    Enter(Step::ClientResting, "the client's rest to end", kSleepBound);
}

void ClientWatchRest() {
    const auto now = Clock::now();
    if (now >= g_nextRead) {
        g_nextRead = now + kSample;
        if (!ReadWorld(nullptr)) return Fail(false, "the world's dilation did not read");
    }
    if (SLP::IsSleeping()) return;
    const float secs = std::chrono::duration<float>(now - g_t0).count();
    const float need = ReadField(V::Field::Sleep);
    const float food = ReadField(V::Field::Food);
    const float rate = secs > 0.f ? (g_restFood0 - food) / secs : 0.f;
    const float diff = g_awakeRate > 0.f ? (rate - g_awakeRate) / g_awakeRate : 0.f;
    const bool pass = WorldHeldAtOne() && need >= kRestedNeed && std::fabs(diff) <= kFoodShare;
    Say(false, pass, "T_client=%.1f s, world dilation %.2f..%.2f over %d reads, need %.1f -> %.1f, food %.2f -> %.2f = "
        "%.4f/s against %.4f/s awake (%+.0f%%)", secs, g_dilMin, g_dilMax, g_dilReads, g_restNeed0, need, g_restFood0,
        food, rate, g_awakeRate, diff * 100.f);
    Finish(false);
}

void TickClient() {
    namespace SS = coop::sleep_sync;
    switch (g_step) {
    case Step::WaitJoin:
        if (!coop::net_pump::HasAnnouncedWorldReady() ||
            coop::join_progress::CurrentPhase() != coop::join_progress::Phase::Idle)
            return;
        UE_LOGI("sleep_drill: [client] joined; waiting for the host's sleep");
        Enter(Step::ClientWaitHost, "the host to sleep", kJoinBound);
        return;
    case Step::ClientWaitHost:
        if (!SS::HostAsleepNow()) return;
        if (ArmOf() == Arm::HostAsleep) ClientBeginSample();
        else Enter(Step::ClientWaitHostWake, "the host to wake", kSleepBound);
        return;
    case Step::ClientSample: ClientSample(); return;
    case Step::ClientWaitHostWake:
        if (SS::HostAsleepNow()) return;
        g_awakeFood0 = ReadField(V::Field::Food);
        g_t0 = Clock::now();
        Enter(Step::ClientAwake, "the awake food window", kPeerBound);
        return;
    case Step::ClientAwake: {
        const auto now = Clock::now();
        if (now - g_t0 < kWindow) return;
        const float secs = std::chrono::duration<float>(now - g_t0).count();
        const float food = ReadField(V::Field::Food);
        g_awakeRate = (g_awakeFood0 - food) / secs;
        if (g_awakeRate <= 0.f)
            return Fail(false, "the awake food rate is not measurable (%.3f -> %.3f in %.1f s)", g_awakeFood0, food, secs);
        UE_LOGI("sleep_drill: [client] awake food rate %.4f/s over %.1f s; going to bed once no event is active",
                g_awakeRate, secs);
        Enter(Step::ClientQuiet, "no active event", kQuietBound);
        return;
    }
    case Step::ClientQuiet:   ClientGoToBed(); return;
    case Step::ClientResting: ClientWatchRest(); return;
    default: return;
    }
}

const char* ArmPlan(bool host) {
    if (ArmOf() == Arm::HostAsleep)
        return host ? "to bed once a client's join is over, then its own native sleep"
                    : "reading this client's world and clock while the host sleeps";
    return host ? "its own native sleep, then waiting for the client's rest"
                : "after the host wakes: awake food, then this client's own rest in the nearest bed";
}

}  // namespace

bool IsEnabled() { return ArmOf() != Arm::Off; }

void Install(coop::net::Session* session) {
    if (!IsEnabled()) return;
    g_session.store(session, std::memory_order_release);
}

void Tick() {
    if (!IsEnabled()) return;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->running() || !SLP::EnsureResolved() || g_step == Step::Done) return;
    AE::EnsureResolved();
    const bool host = IsHost(s);
    if (!g_saidArm) {
        g_saidArm = true;
        Enter(Step::WaitJoin, "the join", kJoinBound);
        UE_LOGI("[SLEEP-DRILL] %s %s armed -- %s", ArmName(), RoleName(host), ArmPlan(host));
    }
    if (Clock::now() > g_deadline)
        return Fail(host, "timed out waiting for %s", g_waitingFor);
    if (host) TickHost(s);
    else TickClient();
}

void OnDisconnect() {
    RestoreDream();
    g_step = Step::WaitJoin;
    g_saidArm = false;
    g_slot = -1;
    g_dilReads = 0;
    g_awakeRate = 0.f;
}

}  // namespace coop::dev::sleep_drill
