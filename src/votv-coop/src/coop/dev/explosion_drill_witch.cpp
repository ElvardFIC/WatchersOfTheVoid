// coop/dev/explosion_drill_witch.cpp -- see coop/dev/explosion_drill_internal.h.
//
// The witch car arm (request_witch). A client sitting in the witch car fires the rocket its left-mouse event spawns;
// the host must take that shot from the client's seat. The host spawns the car and a crate for the rocket to meet,
// the client walks to its mirror with the director, takes the seat through the game's own verb and presses the car's
// left mouse once. Each half judges its own lane counters as deltas against a baseline taken before the first send.
// The host reports its shot half (accepted, not refused as not_seated) apart from its blast half (the request
// accepted, the blast captured): a rocket that meets nothing expires, which leaves the blast half INCONCLUSIVE.
// The car destroys itself at begin-play in the story mode or outside Halloween, each peer on its own state, so both
// halves put the sandbox mode and isHalloween in force before the car exists on them and back at the end; the host
// spawns only after the client's chat line says its own state is in.

#include "coop/dev/explosion_drill_internal.h"

#include "explosion_drill_source.h"          // Guard, ReadBool, WriteBool, Count (co-located private header)
#include "atv_seat.h"                        // TrySit (co-located private header)
#include "hostile_drill_explosion_detail.h"  // Say, ChatHas, PollDue (co-located private header)

#include "coop/dev/director/director.h"
#include "coop/interactables/atv_sync.h"
#include "coop/player/players_registry.h"
#include "coop/props/join_membership_sweep.h"
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_component.h"
#include "ue_wrap/world/game_mode.h"
#include "ue_wrap/world/world_singleton.h"

#include <algorithm>
#include <memory>

namespace coop::dev::explosion_drill::detail {

namespace E  = ue_wrap::engine;
namespace R  = ue_wrap::reflection;
namespace ES = coop::explosion_sync;
namespace ER = coop::explosion_request;
namespace GM = ue_wrap::game_mode;
namespace HD = coop::dev::hostile_drill::explosion::detail;

namespace {

constexpr const char*    kName = "request_witch";
constexpr const wchar_t* kCarCls = L"car1_witch_C";
constexpr const wchar_t* kCarPath = L"/Game/objects/car1_witch.car1_witch_C";
// The car's left-mouse input event: the game's own fire, which spawns the rocket along the car's camera.
constexpr const wchar_t* kFireFn = L"InpActEvt_LeftMouseButton_K2Node_InputKeyEvent_0";
constexpr const char*    kReadyLine = "hostile-drill request_witch client-ready";
constexpr int            kSandbox = 4;             // enum_gamemode's sandbox (ue_wrap/world/game_mode.h)
constexpr float          kTargetAheadCm = 1000.f;  // the crate, along the car's forward vector
constexpr float          kReachCm = 400.f;         // the walk ends this near the car
constexpr int            kWalkDeadlineS = 90;

unsigned long long U(uint64_t v) { return static_cast<unsigned long long>(v); }

// An edge's time, taken the first time it is seen.
void Edge(Clock::time_point& t) {
    if (t == Clock::time_point{}) t = Clock::now();
}

// The game's mode and the gamemode's isHalloween as the arm found them, while its own are in force.
struct Env {
    bool held = false;
    int  mode = -1;
    bool halloween = false;
};

bool HoldEnv(Env& e) {
    if (e.held) return true;
    void* gi = ue_wrap::world_singleton::GameInstance();
    void* gm = ue_wrap::world_singleton::Gamemode();
    bool halloween = false;
    if (!gi || !gm || !R::IsLive(gm) || !ReadBool(gm, L"isHalloween", halloween)) return false;
    const int mode = GM::ReadFrom(gi);
    if (mode < 0) return false;
    GM::WriteTo(gi, kSandbox);
    WriteBool(gm, L"isHalloween", true);
    e = Env{true, mode, halloween};
    Info("witch mode %d and isHalloween %d put in force (sandbox, true)", mode, halloween ? 1 : 0);
    return true;
}

void PutEnv(Env& e) {
    if (!e.held) return;
    void* gi = ue_wrap::world_singleton::GameInstance();
    void* gm = ue_wrap::world_singleton::Gamemode();
    if (gi) GM::WriteTo(gi, e.mode);
    if (gm && R::IsLive(gm)) WriteBool(gm, L"isHalloween", e.halloween);
    Info("witch mode and isHalloween put back (%d, %d)", e.mode, e.halloween ? 1 : 0);
    e = Env{};
}

// ---- the host ----

enum class HP : uint8_t { Wait, Spawn, Rest, Judge, Done };

struct Host {
    HP                    phase = HP::Wait;
    Clock::time_point     at{};
    Env                   env;
    ES::Counters          sync0{};
    ER::Counters          req0{};
    ue_wrap::CachedObjRef car;
    Rest                  rest;
    bool                  shotSaid = false;
};
Host g_h;

void HostEnter(HP p) {
    g_h.phase = p;
    g_h.at = Clock::now();
}

// The car and the crate stay in the world: the run ends on the done line.
void HostEnd() {
    PutEnv(g_h.env);
    g_h.phase = HP::Done;
}

void HostStop(const char* why) {
    Warn("ABANDONED %s", why);
    HostEnd();
}

// True (and the arm abandoned) once the phase has waited its bound.
bool HostLate(const char* why) {
    if (!Past(g_h.at, kBound)) return false;
    HostStop(why);
    return true;
}

// The state in force on the host, then the car at the blast point.
void HostSpawn() {
    void* me = coop::players::Registry::Get().Local();
    Vec B{};
    const Fix f = HoldEnv(g_h.env) ? ComputeB(me, me, B) : Fix::Wait;
    if (f == Fix::Wait) {
        HostLate("the gamemode or the fixture never resolved");
        return;
    }
    void* cls = ResolveClass(kCarCls, kCarPath);
    if (f != Fix::Ok || !cls) return HostStop("the fixture is blocked or untraceable, or car1_witch_C is unloadable");
    void* car = E::SpawnActor(cls, {B.X, B.Y, B.Z + kLift});
    if (!car) {
        HostLate("the witch car could not be spawned");
        return;
    }
    g_h.car.Set(car);
    Info("host witch car spawned at (%.0f,%.0f,%.0f)", B.X, B.Y, B.Z + kLift);
    HostEnter(HP::Rest);
}

// The car's camera component height: the rocket flies from the car along that camera.
bool CameraZ(void* car, float& z) {
    static R::InstanceOffset sCamera{L"Camera"};
    const int32_t off = sCamera.Of(car);
    void* cam = off < 0 ? nullptr : *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(car) + off);
    if (!cam || !R::IsLive(cam)) return false;
    const Vec p = E::GetComponentLocation(cam);
    z = p.Z;
    return Len(p) > 0.f;
}

// The car at rest: the crate goes ahead of it at the camera's height, as the rocket's target.
void HostRest() {
    void* car = g_h.car.Get();
    if (!car) return HostStop("the witch car is gone before it rested (it destroys itself in story mode)");
    NotePhysics(g_h.rest, car);
    if (!AtRest(g_h.rest)) {
        HostLate("the witch car never came to rest");
        return;
    }
    Vec at{};
    float z = 0.f;
    void* cls = ResolveClass(kCrateCls, kCratePath);
    if (!cls || !E::TryGetActorLocation(car, at) || !CameraZ(car, z))
        return HostStop("the crate's class, or the car's place or camera, is unreadable");
    const Vec fwd = E::GetActorForwardVector(car);
    if (!E::SpawnActor(cls, {at.X + fwd.X * kTargetAheadCm, at.Y + fwd.Y * kTargetAheadCm, z})) {
        HostLate("the crate could not be spawned");
        return;
    }
    Info("host witch car at rest (%.0f,%.0f,%.0f), crate ahead at camera height %.0f", at.X, at.Y, at.Z, z);
    HostEnter(HP::Judge);
}

void HostJudge() {
    Host&              h = g_h;
    const ER::Counters c = ER::GetCounters();
    const uint64_t     shots = c.shotsAccepted - h.req0.shotsAccepted;
    const uint64_t     captured = ES::GetCounters().captured - h.sync0.captured;
    uint64_t           refused = 0, accepted = 0;
    Miss               m;
    Compare(m, "shotsAccepted", shots, 1);
    Compare(m, "captured", captured, 1);
    Compare(m, "evicted", c.evicted - h.req0.evicted, 0);
    for (int r = 0; r < static_cast<int>(ER::Refusal::Count); ++r) {
        const uint64_t d = c.refused[r] - h.req0.refused[r];
        refused += d;
        Compare(m, ER::RefusalName(static_cast<ER::Refusal>(r)), d, 0);
    }
    for (int k = 0; k < 3; ++k) {
        const uint64_t d = c.requestsAccepted[k] - h.req0.requestsAccepted[k];
        accepted += d;
        Compare(m, k == 0 ? "requestsAccepted[projectile]" : "requestsAccepted[hand or self]", d, k == 0 ? 1 : 0);
    }
    if (!h.shotSaid && shots == 1 && refused == 0) {
        h.shotSaid = true;
        Info("host shot half PASS arm=%s shotsAccepted=1 not_seated=0", kName);
    }
    if (!m.over && c.expired - h.req0.expired > 0) {
        Warn("ABANDONED INCONCLUSIVE arm=%s the rocket expired: its blast half is not shown", kName);
        return HostEnd();
    }
    if (!m.name) {
        Info("host DONE PASS arm=%s accepted=%llu captured=%llu", kName, U(accepted), U(captured));
        return HostEnd();
    }
    if (!m.over && !Past(h.at, kVerdictBound)) return;
    Warn("FAIL host arm=%s %s got=%llu want=%llu", kName, m.name, U(m.got), U(m.want));
    HostEnd();
}

// ---- the client ----

enum class CP : uint8_t { Wait, Walk, Sit, Judge, Done };

struct Client {
    CP                    phase = CP::Wait;
    Clock::time_point     at{}, dissAt{}, appliedAt{}, firedAt{};
    Env                   env;
    ES::Counters          sync0{};
    ER::Counters          req0{};
    ue_wrap::CachedObjRef car;
    std::shared_ptr<coop::director::BackgroundWalk> walk;
    atv_seat::SitTry      sit;
};
Client g_c;

void ClientEnter(CP p) {
    g_c.phase = p;
    g_c.at = Clock::now();
}

void ClientEnd() {
    if (g_c.walk && g_c.walk->state.load() == 0) coop::director::CancelBackgroundWalk(*g_c.walk);
    g_c.walk.reset();
    Unguard();
    PutEnv(g_c.env);
    g_c.phase = CP::Done;
}

void ClientAbandon(const char* why) {
    Warn("ABANDONED %s", why);
    ClientEnd();
}

// True (and the arm abandoned) once the phase has waited its bound.
bool ClientLate(std::chrono::seconds bound, const char* why) {
    if (!Past(g_c.at, bound)) return false;
    ClientAbandon(why);
    return true;
}

// The client acts once its own streams run, as a player in a settled world does; its state is then put in force,
// its car class resolved (so the mirror can spawn) and the host told.
void ClientWait() {
    g_src.host = false;
    const bool streams = coop::net_pump::HasAnnouncedWorldReady() &&
                         coop::join_progress::CurrentPhase() == coop::join_progress::Phase::Idle &&
                         coop::net_pump::IsWorldSettled() && coop::join_membership_sweep::HasLoadTailQuiesced();
    if (!streams || !HoldEnv(g_c.env) || Guard(false)) {
        ClientLate(kVerdictBound, "the client's streams, the gamemode or the guard never resolved");
        return;
    }
    if (!ResolveClass(kCarCls, kCarPath)) return ClientAbandon("car1_witch_C is neither loaded nor loadable by path");
    g_c.sync0 = ES::GetCounters();
    g_c.req0 = ER::GetCounters();
    SnapshotPre();  // the arm's rocket is the one born after this
    HD::Say(kReadyLine);
    Info("client witch state in force, the host told");
    ClientEnter(CP::Walk);
}

// The witch car's mirror here, once the host's announce has placed it.
void* FindMirror() {
    void* cls = ue_wrap::object_index::ClassByName(kCarCls);
    void* found = nullptr;
    if (cls)
        ue_wrap::object_index::ForEachInstance(
            cls,
            [](void* ctx, void* obj, int32_t) {
                if (!R::NameStartsWith(R::NameOf(obj), L"Default__")) *static_cast<void**>(ctx) = obj;
            },
            &found);
    return found;
}

// The mirror found, the walk to it started with the director and waited for.
void ClientWalk() {
    Vec at{};
    if (!g_c.walk) {
        void* car = FindMirror();
        if (!car || !E::TryGetActorLocation(car, at) || Len(at) < 1.f) {  // a mirror at the origin is not placed yet
            ClientLate(kVerdictBound, "the witch car's mirror never stood here");
            return;
        }
        g_c.car.Set(car);
        g_c.walk = coop::director::StartBackgroundWalk(at, kReachCm, kWalkDeadlineS);
        Info("client witch car at (%.0f,%.0f,%.0f), walking to it", at.X, at.Y, at.Z);
        return;
    }
    if (!g_c.car.Get()) return ClientAbandon("the witch car is gone before the walk ended");
    const int state = g_c.walk->state.load();
    if (state == 2) return ClientAbandon("the director's walk to the car did not arrive");
    if (state == 1) ClientEnter(CP::Sit);
}

// The seat through the game's own verb; seated, the car's left-mouse event runs once with an empty key. The seat claim
// and its first state leave in the tick the lane makes the claim, so the host has the seat before the shot.
void ClientSit() {
    void* car = g_c.car.Get();
    void* player = coop::players::Registry::Get().Local();
    if (!car) return ClientAbandon("the witch car is gone before the seat");
    if (!player || !atv_seat::TrySit(player, car, g_c.sit)) return;
    if (coop::atv_sync::ActorSeatedBy(coop::players::Registry::Get().LocalPeerId()) != car)
        return ClientAbandon("the seat was refused (see the [ATVP] sit lines)");
    void* fn = R::FindFunction(R::ClassOf(car), kFireFn);
    ue_wrap::ParamFrame f(fn);
    if (!fn || !f.valid() || !ue_wrap::Call(car, f)) return ClientAbandon("the car's left-mouse event did not run");
    g_c.firedAt = Clock::now();
    Info("client witch seated and fired");
    ClientEnter(CP::Judge);
}

void ClientJudge() {
    Client&            c = g_c;
    const ER::Counters rc = ER::GetCounters();
    const ES::Counters sc = ES::GetCounters();
    uint64_t           sent = 0, applied = 0;
    Miss               m;
    for (int k = 0; k < 3; ++k) {
        const uint64_t out = rc.requestsSent[k] - c.req0.requestsSent[k];
        sent += out;
        applied += sc.applied[k] - c.sync0.applied[k];
        Compare(m, k == 0 ? "requestsSent[projectile]" : "requestsSent[hand or self]", out, k == 0 ? 1 : 0);
    }
    const uint64_t dissolved = sc.dissolved - c.sync0.dissolved;
    if (dissolved >= 1) Edge(c.dissAt);
    if (applied >= 1) Edge(c.appliedAt);
    Compare(m, "shotsSent", rc.shotsSent - c.req0.shotsSent, 1);
    Compare(m, "dissolved", dissolved, 1);
    Compare(m, "applied", applied, 1);
    // The counters the arm does not use stay at 0.
    Compare(m, "pendingDropped", rc.pendingDropped - c.req0.pendingDropped, 0);
    Compare(m, "pendingDead", rc.pendingDead - c.req0.pendingDead, 0);
    Compare(m, "pendingOrphaned", rc.pendingOrphaned - c.req0.pendingOrphaned, 0);
    Compare(m, "clientDropped", rc.clientDropped - c.req0.clientDropped, 0);
    Compare(m, "unmappedProjectile", rc.unmappedProjectile - c.req0.unmappedProjectile, 0);
    Compare(m, "shotNotAnnounced", rc.shotNotAnnounced - c.req0.shotNotAnnounced, 0);
    if (!m.name) {  // equal counters are not the end: the rocket must be gone too, so a second blast is read
        const int standing = Count(kRocketCls, true);
        if (standing > 0) m = Miss{"projectiles standing", static_cast<uint64_t>(standing), 0, false};
    }
    if (m.name) {
        if (!m.over && !Past(c.at, kVerdictBound)) return;
        Warn("FAIL client arm=%s %s got=%llu want=%llu", kName, m.name, U(m.got), U(m.want));
        return ClientEnd();
    }
    const Clock::time_point from = c.dissAt != Clock::time_point{} ? c.dissAt : c.firedAt;
    const long long ms = (std::max)(0LL, static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                             c.appliedAt - from).count()));
    Info("client DONE PASS arm=%s sent=%llu applied=%llu ms=%lld", kName, U(sent), U(applied), ms);
    ClientEnd();
}

}  // namespace

void WitchHostTick() {
    Host& h = g_h;
    switch (h.phase) {
    case HP::Wait:
        if (h.at == Clock::time_point{}) {
            h.sync0 = ES::GetCounters();
            h.req0 = ER::GetCounters();
            h.at = Clock::now();
        }
        if (HD::PollDue() && HD::ChatHas(kReadyLine)) return HostEnter(HP::Spawn);
        if (Past(h.at, kVerdictBound)) {
            Warn("ABANDONED the client never said its state was in force");
            HostEnd();
        }
        return;
    case HP::Spawn: return HostSpawn();
    case HP::Rest: return HostRest();
    case HP::Judge: return HostJudge();
    case HP::Done: return;
    }
}

void WitchClientTick() {
    Client& c = g_c;
    if (c.at == Clock::time_point{}) c.at = Clock::now();
    switch (c.phase) {
    case CP::Wait: return ClientWait();
    case CP::Walk: return ClientWalk();
    case CP::Sit: return ClientSit();
    case CP::Judge: return ClientJudge();
    case CP::Done: return;
    }
}

void WitchReset() {
    if (g_c.phase != CP::Done) ClientEnd();
    PutEnv(g_h.env);
    g_c = Client{};
    g_h = Host{};
}

}  // namespace coop::dev::explosion_drill::detail
