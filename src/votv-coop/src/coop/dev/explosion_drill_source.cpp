// coop/dev/explosion_drill_source.cpp -- see coop/dev/explosion_drill_internal.h.
//
// The source arms (source_host, source_client): a measurement of which peer's game reaches each request-eligible
// explosion source's birth, whether a client's rocket or sticky bomb exists on the host, and the salt rocket's
// fuse. The role the arm names walks six legs through the game's own verbs; the other role only reads its
// counts and reports. A leg ends on a readiness edge (the birth, the attach, the rest) or on its own failure
// bound and prints one line; a verb the Blueprint refuses ends that leg alone ("abandoned", never the run's dead
// marker). The run ends on a DONE line that carries a count, never a verdict.

#include "coop/dev/explosion_drill_internal.h"

#include "drive_drill_verbs.h"  // Throw: the player's own throw (co-located private header)

#include "coop/dev/director/director.h"  // LookAt
#include "coop/dev/drill_hand.h"
#include "coop/player/players_registry.h"
#include "coop/world/explosion_sync.h"

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_attach.h"
#include "ue_wrap/engine/engine_mainplayer.h"
#include "ue_wrap/engine/engine_pawn.h"
#include "ue_wrap/world/daynightcycle.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace coop::dev::explosion_drill::detail {

namespace R  = ue_wrap::reflection;
namespace E  = ue_wrap::engine;
namespace ES = coop::explosion_sync;
namespace OI = ue_wrap::object_index;

namespace {

constexpr auto  kStepBound = std::chrono::seconds(10);    // a step that waits on the game
constexpr auto  kBirthBound = std::chrono::seconds(10);   // from the verb that starts the fuse
constexpr auto  kAttachBound = std::chrono::seconds(5);   // a sticky bomb that never attaches is detonated anyway
constexpr auto  kSaltBound = std::chrono::seconds(20);    // from the ignite: the fuse is about ten seconds
constexpr float kAimTolDeg = 5.f;
constexpr float kLift = 50.f;

// The game's row names (the list_props table) and class leaves of the four held sources; the salt rocket's legs
// spawn it in the world.
enum class Kind : uint8_t { Grenade, Pipebomb, Rocket, Sticky, SaltAttached, SaltFree };
struct LegDef {
    const char*    name;
    Kind           kind;
    const wchar_t* row;
    const wchar_t* cls;
};
constexpr LegDef kLegs[] = {
    {"grenade", Kind::Grenade, L"grenade", L"prop_grenade_C"},
    {"pipebomb", Kind::Pipebomb, L"pipebomb", L"prop_pipebomb_C"},
    {"rocket", Kind::Rocket, L"fungun_rpg", L"prop_funGun_rpg_C"},
    {"sticky", Kind::Sticky, L"fungun_sticky", L"prop_funGun_stickyLauncher_C"},
    {"saltrocket", Kind::SaltAttached, nullptr, L"prop_saltrocket_C"},
    {"saltrocket", Kind::SaltFree, nullptr, L"prop_saltrocket_C"},
};
constexpr int kLegCount = static_cast<int>(sizeof(kLegs) / sizeof(kLegs[0]));

constexpr const wchar_t* kRocketCls = L"prop_rocket_C";
constexpr const wchar_t* kBombCls = L"prop_stickybomb_C";
// Every projectile a leg can leave in the world, swept at its end (only what the drill's start did not find).
constexpr const wchar_t* kSwept[] = {L"prop_grenade_C", L"prop_pipebomb_C", L"prop_rocket_C", L"prop_stickybomb_C"};

bool IsSalt(Kind k) { return k == Kind::SaltAttached || k == Kind::SaltFree; }

enum class SP : uint8_t { Fixture, Legs, Done };
enum class LStep : uint8_t { Begin, Held, Aim, Act, Act2, AwaitBomb, AwaitAttach, Detonate, Birth, SaltRest, SaltIgnite,
                             SaltWait, End, Settle };

// What one leg read. -1 is unread.
struct Reading {
    float       timer = -1.f, reload = -1.f;
    int         ammo = -1, active = -1, attached = -1, rain0 = -1, rain1 = -1;
    bool        thrown = false, fired = false, detonated = false, ignited = false, birth = false;
    int         rocketMax = 0, bombMax = 0;
    long long   ms = -1, explodeMs = -1;
    const char* skip = nullptr;
};

struct Src {
    SP                    phase = SP::Fixture;
    int                   leg = 0, measured = 0;
    LStep                 step = LStep::Begin;
    Clock::time_point     stepAt{}, verbAt{}, fixAt{};
    Vec                   B{};
    ES::Counters          base{};
    Reading               r;
    Rest                  rest;
    ue_wrap::CachedObjRef rocket, parent;  // the salt legs' own actors
    bool                  said = false;
    int                   lastRocket = -1, lastBomb = -1;
};
Src g_src;
std::vector<ue_wrap::CachedObjRef> g_pre;  // the projectiles the world held when the drill began
std::vector<void*>                 g_scan;

const char* RoleName(bool host) { return host ? "host" : "client"; }
const char* ArmName(Arm a) { return a == Arm::SourceHost ? "source_host" : "source_client"; }

void Enter(LStep s) {
    g_src.step = s;
    g_src.stepAt = Clock::now();
}
bool Late(std::chrono::seconds bound) { return Past(g_src.stepAt, bound); }
long long Since(Clock::time_point t) {
    return static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t).count());
}

// ---- reading the game ----

float AngDiff(float a, float b) {
    float d = std::fmod(a - b, 360.f);
    if (d > 180.f) d -= 360.f;
    if (d < -180.f) d += 360.f;
    return std::fabs(d);
}

bool ReadFloat(void* obj, const wchar_t* name, float& out) {
    const int32_t off = obj ? R::FindPropertyOffset(R::ClassOf(obj), name) : -1;
    if (off < 0) return false;
    out = *reinterpret_cast<const float*>(static_cast<const uint8_t*>(obj) + off);
    return true;
}
bool ReadInt(void* obj, const wchar_t* name, int& out) {
    const int32_t off = obj ? R::FindPropertyOffset(R::ClassOf(obj), name) : -1;
    if (off < 0) return false;
    out = *reinterpret_cast<const int32_t*>(static_cast<const uint8_t*>(obj) + off);
    return true;
}
// A Blueprint bool is one bit of a shared byte: the offset and the mask are the property's own.
bool ReadBool(void* obj, const wchar_t* name, bool& out) {
    int32_t off = -1;
    uint8_t mask = 0;
    if (!obj || !R::FindBoolProperty(R::ClassOf(obj), name, off, mask)) return false;
    out = (*(static_cast<const uint8_t*>(obj) + off) & mask) != 0;
    return true;
}

// The day cycle's isRaining: 1, 0, or -1 when the cycle or the member is unreadable.
int ReadRain() {
    void* cycle = ue_wrap::daynightcycle::Cycle();
    if (!cycle) return -1;
    static void*   cls = nullptr;
    static int32_t off = -1;
    static uint8_t mask = 0;
    void* c = R::ClassOf(cycle);
    if (c != cls) {
        cls = c;
        if (!R::FindBoolProperty(c, L"isRaining", off, mask)) off = -1;
    }
    if (off < 0) return -1;
    return (*(static_cast<const uint8_t*>(cycle) + off) & mask) ? 1 : 0;
}

// The local hand's actor, when it is live and of class `cls`.
void* HeldOf(void* player, const wchar_t* cls) {
    E::MainPlayerGrabState gs{};
    if (!E::ReadMainPlayerGrabState(player, gs)) return nullptr;
    void* h = gs.holdingActor;
    return h && R::IsLive(h) && R::NameEquals(R::NameOf(R::ClassOf(h)), cls) ? h : nullptr;
}

// mainPlayer's own call of a held prop's hand verb: the Blueprint function of that name, with the player.
bool CallHandVerb(void* held, void* player, const wchar_t* verb) {
    void* fn = held ? R::FindDispatchFunctionCached(R::ClassOf(held), verb) : nullptr;
    ue_wrap::ParamFrame f(fn);
    return fn && f.valid() && f.Set<void*>(L"player", player) && ue_wrap::Call(held, f);
}

struct Scan {
    int                 n = 0;
    std::vector<void*>* out = nullptr;
    bool                newOnly = false;
};
void ScanFn(void* ctx, void* obj, int32_t) {
    if (R::NameStartsWith(R::NameOf(obj), L"Default__")) return;
    auto* s = static_cast<Scan*>(ctx);
    if (s->newOnly)
        for (const ue_wrap::CachedObjRef& q : g_pre)
            if (q.Is(obj)) return;
    ++s->n;
    if (s->out) s->out->push_back(obj);
}
// The live instances of `cls` (its default object left out); with `newOnly`, those the drill's start did not find.
int Count(const wchar_t* cls, bool newOnly, std::vector<void*>* out = nullptr) {
    void* c = OI::ClassByName(cls);
    if (!c) return 0;
    Scan s;
    s.out = out;
    s.newOnly = newOnly;
    OI::ForEachInstance(c, &ScanFn, &s);
    return s.n;
}

void SnapshotPre() {
    g_pre.clear();
    for (const wchar_t* cls : kSwept) {
        void* c = OI::ClassByName(cls);
        if (!c) continue;
        OI::ForEachInstance(
            c,
            [](void*, void* obj, int32_t) {
                if (R::NameStartsWith(R::NameOf(obj), L"Default__")) return;
                ue_wrap::CachedObjRef ref;
                ref.Set(obj);
                g_pre.push_back(ref);
            },
            nullptr);
    }
}

void SweepNew() {
    for (const wchar_t* cls : kSwept) {
        g_scan.clear();
        Count(cls, true, &g_scan);
        for (void* a : g_scan)
            if (R::IsLive(a)) E::DestroyActor(a);
    }
}

// The explosion's birth on the acting peer: the host's capture at begin-play, the client's dissolve at the spawn.
bool BirthSeen(bool host) {
    const ES::Counters c = ES::GetCounters();
    return host ? c.captured > g_src.base.captured : c.dissolved > g_src.base.dissolved;
}

// ---- one leg's line ----

void Emit(const LegDef& d, bool host) {
    const Reading& r = g_src.r;
    const char* role = RoleName(host);
    char var[40] = "";
    if (IsSalt(d.kind))
        std::snprintf(var, sizeof(var), "variant=%s ", d.kind == Kind::SaltAttached ? "attached" : "free");
    if (r.skip) {
        Info("source leg=%s role=%s %sabandoned reason=%s", d.name, role, var, r.skip);
        return;
    }
    ++g_src.measured;
    switch (d.kind) {
    case Kind::Grenade:
        Info("source leg=%s role=%s timer=%.1f thrown=%d birth=%d ms=%lld", d.name, role, r.timer, r.thrown ? 1 : 0,
             r.birth ? 1 : 0, r.ms);
        break;
    case Kind::Pipebomb:
        Info("source leg=%s role=%s active=%d thrown=%d birth=%d ms=%lld", d.name, role, r.active, r.thrown ? 1 : 0,
             r.birth ? 1 : 0, r.ms);
        break;
    case Kind::Rocket:
        Info("source leg=%s role=%s reload_primary=%.1f ammo_primary=%d fired=%d live_prop_rocket_C_max=%d birth=%d "
             "ms=%lld", d.name, role, r.reload, r.ammo, r.fired ? 1 : 0, r.rocketMax, r.birth ? 1 : 0, r.ms);
        break;
    case Kind::Sticky:
        Info("source leg=%s role=%s reload_primary=%.1f ammo_primary=%d fired=%d live_prop_stickybomb_C_max=%d "
             "attached=%d detonated=%d birth=%d ms=%lld", d.name, role, r.reload, r.ammo, r.fired ? 1 : 0, r.bombMax,
             r.attached, r.detonated ? 1 : 0, r.birth ? 1 : 0, r.ms);
        break;
    case Kind::SaltAttached:
    case Kind::SaltFree: {
        char entry[24], boom[48];
        if (r.explodeMs >= 0) std::snprintf(entry, sizeof(entry), "%lld", r.explodeMs);
        else std::snprintf(entry, sizeof(entry), "none");
        if (r.birth) std::snprintf(boom, sizeof(boom), "explosion_ms=%lld", r.ms);
        else std::snprintf(boom, sizeof(boom), "no explosion in %lld s",
                           static_cast<long long>(std::chrono::duration_cast<std::chrono::seconds>(kSaltBound).count()));
        Info("source leg=%s role=%s %signited=%d raining=%d->%d explode_entry_ms=%s %s", d.name, role, var,
             r.ignited ? 1 : 0, r.rain0, r.rain1, entry, boom);
        break;
    }
    }
}

void Skip(const char* why) {
    g_src.r.skip = why;
    Enter(LStep::End);
}

// ---- the held legs ----
//
// Blueprint prop_grenade, playerHandUse_RMB: its entry runs only at timer == 0, then arms the fuse (timer = 3,
// fuse()); the throw is mainPlayer's throwHoldingProp. Blueprint prop_pipebomb, playerHandUse_RMB: activate()
// (active = true, the six-second fuse); its playerHandUse_LMB calls throwHoldingProp. Blueprint prop_funGun,
// playerHandUse_LMB: sets hold, player and character, then firePrimary(), which returns while reload_primary > 0
// and, at ammo_primary > 0, fires through the gun's own fireBulletPrimary (the rpg's spawns prop_rocket, the sticky
// launcher's prop_stickybomb). Its BeginPlay sets ammo_primary to maxMagazine_primary and reload_primary starts at
// 0, so a gun fresh in the hand needs nothing set; the leg reads both and abandons when either would stop the shot.
// Blueprint prop_funGun_stickyLauncher, playerHandUse_RMB: explode() on every bomb it holds.

void Act(const LegDef& d, void* player) {
    void* held = HeldOf(player, d.cls);
    if (!held) return Skip("the held actor is gone before its verb");
    Reading& r = g_src.r;
    g_src.base = ES::GetCounters();
    if (d.kind == Kind::Grenade) {
        if (!CallHandVerb(held, player, L"playerHandUse_RMB")) return Skip("playerHandUse_RMB did not run");
        g_src.verbAt = Clock::now();
        ReadFloat(held, L"timer", r.timer);
        if (!(r.timer > 0.f)) return Skip("the grenade stayed unarmed: timer is not above 0 after playerHandUse_RMB");
        return Enter(LStep::Act2);
    }
    if (d.kind == Kind::Pipebomb) {
        if (!CallHandVerb(held, player, L"playerHandUse_RMB")) return Skip("playerHandUse_RMB did not run");
        g_src.verbAt = Clock::now();
        bool active = false;
        if (ReadBool(held, L"active", active)) r.active = active ? 1 : 0;
        if (r.active == 0) return Skip("the pipe bomb stayed inactive after playerHandUse_RMB");
        return Enter(LStep::Act2);
    }
    ReadFloat(held, L"reload_primary", r.reload);
    ReadInt(held, L"ammo_primary", r.ammo);
    if (r.reload > 0.f) return Skip("reload_primary is above 0, so firePrimary returns");
    if (r.ammo == 0) return Skip("ammo_primary is 0, so firePrimary only clicks");
    g_src.verbAt = Clock::now();
    if (!CallHandVerb(held, player, L"playerHandUse_LMB")) return Skip("playerHandUse_LMB did not run");
    r.fired = true;
    Enter(d.kind == Kind::Sticky ? LStep::AwaitBomb : LStep::Birth);
}

// The throw after the arming: the grenade's is the player's own throwHoldingProp, the pipe bomb's its own LMB.
void Act2(const LegDef& d, void* player) {
    Reading& r = g_src.r;
    if (d.kind == Kind::Grenade) {
        r.thrown = coop::dev::drive_drill_verbs::Throw(player);
    } else {
        void* held = HeldOf(player, d.cls);
        r.thrown = held && CallHandVerb(held, player, L"playerHandUse_LMB");
    }
    if (!r.thrown) return Skip("the throw did not run");
    Enter(LStep::Birth);
}

void AwaitBomb() {
    Reading& r = g_src.r;
    r.bombMax = (std::max)(r.bombMax, Count(kBombCls, true));
    if (r.bombMax > 0) return Enter(LStep::AwaitAttach);
    if (Late(kStepBound)) Enter(LStep::End);  // fired, and no bomb stands: measured as it is
}

// A sticky bomb attaches on its hit (Blueprint prop_stickybomb, the hit event attaches it to what it struck).
void AwaitAttach() {
    Reading& r = g_src.r;
    g_scan.clear();
    r.bombMax = (std::max)(r.bombMax, Count(kBombCls, true, &g_scan));
    if (g_scan.empty()) return Enter(LStep::End);  // the bomb is gone before the detonate
    void* parent = nullptr;
    if (E::GetAttachParentActor(g_scan[0], parent) && parent) {
        r.attached = 1;
        return Enter(LStep::Detonate);
    }
    if (Past(g_src.stepAt, kAttachBound)) {
        r.attached = 0;
        Enter(LStep::Detonate);
    }
}

void Detonate(void* player) {
    void* held = HeldOf(player, L"prop_funGun_stickyLauncher_C");
    if (!held) return Skip("the launcher is gone before its detonate");
    g_src.base = ES::GetCounters();
    g_src.verbAt = Clock::now();
    g_src.r.detonated = CallHandVerb(held, player, L"playerHandUse_RMB");
    if (!g_src.r.detonated) return Skip("playerHandUse_RMB did not run on the launcher");
    Enter(LStep::Birth);
}

void Birth(const LegDef& d, bool host) {
    Reading& r = g_src.r;
    if (d.kind == Kind::Rocket) r.rocketMax = (std::max)(r.rocketMax, Count(kRocketCls, true));
    if (d.kind == Kind::Sticky) r.bombMax = (std::max)(r.bombMax, Count(kBombCls, true));
    if (BirthSeen(host)) {
        r.birth = true;
        r.ms = Since(g_src.verbAt);
        return Enter(LStep::End);
    }
    if (Past(g_src.verbAt, kBirthBound)) Enter(LStep::End);
}

// ---- the salt rocket legs ----
//
// Blueprint prop_saltrocket, attemptIgnite -> fire(): with no flameBase it lights the fuse and plays Timeline_0
// (four seconds); its finish waits one second, then, only when the rocket's attach parent actor exists and its root
// simulates physics, plays timeline `a`, whose finish calls explode(); otherwise it detaches and returns, so a free
// rocket never reaches explode(). explode() reads the day cycle's isRaining: while it is false the call sets it true
// and returns (no explosion), while it is true the rocket spawns explosion_saltrocket and destroys itself. The leg
// reads isRaining before the ignite and on every tick, so the call of explode() shows as the flag turning true.

void SaltBegin(const LegDef& d) {
    void* rcls = ResolveClass(L"prop_saltrocket_C", L"/Game/objects/prop_saltrocket.prop_saltrocket_C");
    if (!rcls) return Skip("prop_saltrocket_C is neither loaded nor loadable by path");
    const bool attached = d.kind == Kind::SaltAttached;
    if (attached && !g_src.parent.Get()) {
        void* pcls = ResolveClass(L"prop_paper_C", L"/Game/objects/prop_paper.prop_paper_C");
        if (!pcls) return Skip("prop_paper_C is neither loaded nor loadable by path");
        if (void* p = E::SpawnActor(pcls, {g_src.B.X, g_src.B.Y, g_src.B.Z + kLift})) g_src.parent.Set(p);
        if (!g_src.parent.Get()) {
            if (Late(kStepBound)) Skip("the world refused the parent's spawn");
            return;
        }
    }
    void* rk = E::SpawnActor(rcls, {g_src.B.X, g_src.B.Y, g_src.B.Z + 2.f * kLift});
    if (!rk) {
        if (Late(kStepBound)) Skip("the world refused the rocket's spawn");
        return;
    }
    g_src.rocket.Set(rk);
    g_src.rest = Rest{};
    Enter(LStep::SaltRest);
}

void SaltRest(const LegDef& d) {
    void* body = (d.kind == Kind::SaltAttached ? g_src.parent : g_src.rocket).Get();
    if (!body || !g_src.rocket.Get()) return Skip("the rocket or its parent is gone before the ignite");
    NotePhysics(g_src.rest, body);
    if (AtRest(g_src.rest)) return Enter(LStep::SaltIgnite);
    if (Late(kStepBound)) Skip("the body never came to rest");
}

void SaltIgnite(const LegDef& d) {
    void* rk = g_src.rocket.Get();
    if (!rk) return Skip("the rocket is gone before the ignite");
    if (d.kind == Kind::SaltAttached) {
        void* pr = g_src.parent.Get();
        void* root = pr ? E::RootComponentOf(pr) : nullptr;
        void* now = nullptr;
        if (!root || !E::AttachActorToComponentSocket(rk, root, nullptr)) return Skip("the rocket could not attach");
        if (!E::GetAttachParentActor(rk, now) || now != pr) return Skip("the rocket did not attach to its parent");
    }
    Reading& r = g_src.r;
    r.rain0 = ReadRain();
    r.rain1 = r.rain0;
    g_src.base = ES::GetCounters();
    g_src.verbAt = Clock::now();
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(rk), L"attemptIgnite");
    ue_wrap::ParamFrame f(fn);
    r.ignited = fn && f.valid() && ue_wrap::Call(rk, f);
    if (!r.ignited) return Skip("attemptIgnite did not run");
    Enter(LStep::SaltWait);
}

void SaltWait(bool host) {
    Reading& r = g_src.r;
    const int rain = ReadRain();
    if (rain >= 0) {
        r.rain1 = rain;
        if (r.rain0 == 0 && rain == 1 && r.explodeMs < 0) r.explodeMs = Since(g_src.verbAt);
    }
    if (BirthSeen(host)) {
        r.birth = true;
        r.ms = Since(g_src.verbAt);
        return Enter(LStep::End);
    }
    if (Past(g_src.verbAt, kSaltBound)) Enter(LStep::End);
}

// ---- the leg walk ----

void StartLeg() {
    g_src.r = Reading{};
    Enter(LStep::Begin);
}

void Cleanup(const LegDef& d) {
    if (IsSalt(d.kind)) {
        if (void* a = g_src.rocket.Get()) E::DestroyActor(a);
        if (void* a = g_src.parent.Get()) E::DestroyActor(a);
        g_src.rocket.Reset();
        g_src.parent.Reset();
    }
    // The bombs first: a launcher that leaves the hand blows the bombs it still holds.
    SweepNew();
    if (!IsSalt(d.kind)) coop::dev::drill_hand::TakeIntoHandNow(L"None", nullptr);
}

void Aim(void* player) {
    const ue_wrap::FRotator want = coop::director::LookAt(E::GetCameraLocation(), g_src.B);
    E::SetControlRotation(E::GetController(player), want);
    const ue_wrap::FRotator got = E::GetCameraRotation();
    if (AngDiff(got.Yaw, want.Yaw) < kAimTolDeg && AngDiff(got.Pitch, want.Pitch) < kAimTolDeg)
        return Enter(LStep::Act);
    if (Late(kStepBound)) Skip("the camera never reached the aim");
}

void LegTick(Arm arm, bool host) {
    const LegDef& d = kLegs[g_src.leg];
    void* player = coop::players::Registry::Get().Local();
    if (!player && g_src.step != LStep::End) {
        if (!Late(kStepBound)) return;
        if (g_src.step == LStep::Settle) return StartLeg();
        return Skip("the local player is gone");
    }
    switch (g_src.step) {
    case LStep::Begin:
        if (IsSalt(d.kind)) return SaltBegin(d);
        if (!coop::dev::drill_hand::TakeIntoHandNow(d.row, d.cls)) return Skip("the hand could not take the item");
        return Enter(LStep::Held);
    case LStep::Held:
        if (HeldOf(player, d.cls)) return Enter(LStep::Aim);
        if (Late(kStepBound)) Skip("the hand never held the item");
        return;
    case LStep::Aim: return Aim(player);
    case LStep::Act: return Act(d, player);
    case LStep::Act2: return Act2(d, player);
    case LStep::AwaitBomb: return AwaitBomb();
    case LStep::AwaitAttach: return AwaitAttach();
    case LStep::Detonate: return Detonate(player);
    case LStep::Birth: return Birth(d, host);
    case LStep::SaltRest: return SaltRest(d);
    case LStep::SaltIgnite: return SaltIgnite(d);
    case LStep::SaltWait: return SaltWait(host);
    case LStep::End:
        Emit(d, host);
        Cleanup(d);
        if (++g_src.leg >= kLegCount) {
            Info("source DONE arm=%s legs=%d measured=%d", ArmName(arm), kLegCount, g_src.measured);
            g_src.phase = SP::Done;
            return;
        }
        g_src.rest = Rest{};
        return Enter(LStep::Settle);
    case LStep::Settle:
        NoteSpeed(g_src.rest, Len(E::GetActorVelocity(player)));
        if (AtRest(g_src.rest) || Late(kStepBound)) StartLeg();
        return;
    }
}

void Abandon(const char* what) {
    Warn("ABANDONED %s", what);
    g_src.phase = SP::Done;
}

void ActTick(Arm arm, bool host) {
    switch (g_src.phase) {
    case SP::Fixture: {
        void* me = coop::players::Registry::Get().Local();
        if (g_src.fixAt == Clock::time_point{}) g_src.fixAt = Clock::now();
        const Fix f = ComputeB(me, me, g_src.B);
        if (f == Fix::Wait) {
            if (Past(g_src.fixAt, kStepBound)) Abandon("source fixture unreadable");
            return;
        }
        if (f == Fix::Blocked) return Abandon("source fixture blocked");
        if (f == Fix::NoTrace) return Abandon("source fixture trace unresolvable");
        SnapshotPre();
        Info("source %s fixture arm=%s B=(%.0f,%.0f,%.0f) projectiles_before=%d", RoleName(host), ArmName(arm),
             g_src.B.X, g_src.B.Y, g_src.B.Z, static_cast<int>(g_pre.size()));
        g_src.phase = SP::Legs;
        return StartLeg();
    }
    case SP::Legs: return LegTick(arm, host);
    case SP::Done: return;
    }
}

// The role the arm does not name: its count of the two projectile classes, said on every change.
void Passive(Arm arm, bool host) {
    if (!g_src.said) {
        g_src.said = true;
        Info("source passive role=%s arm=%s reads only", RoleName(host), ArmName(arm));
    }
    const int rockets = Count(kRocketCls, false), bombs = Count(kBombCls, false);
    if (rockets == g_src.lastRocket && bombs == g_src.lastBomb) return;
    g_src.lastRocket = rockets;
    g_src.lastBomb = bombs;
    Info("source passive role=%s live prop_rocket_C=%d prop_stickybomb_C=%d", RoleName(host), rockets, bombs);
}

}  // namespace

void SourceTick(Arm arm, bool isHost) {
    if ((arm == Arm::SourceHost) == isHost) ActTick(arm, isHost);
    else Passive(arm, isHost);
}

void SourceReset() {
    if (void* a = g_src.rocket.Get()) E::DestroyActor(a);
    if (void* a = g_src.parent.Get()) E::DestroyActor(a);
    g_src = Src{};
    g_pre.clear();
}

}  // namespace coop::dev::explosion_drill::detail
