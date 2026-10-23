// coop/dev/explosion_drill_source_measure.cpp -- see coop/dev/explosion_drill_source.h.
//
// The source arms' MEASURE legs and the host's thrown-prop watch: three facts the explosion design could not state
// without a number.
//   sticky_parent: where a sticky bomb goes when the prop it is stuck to moves, and what becomes of it when that prop
//     is destroyed.
//   selfpill: how long after the acting player is lit while gasoline-pilled the explosion is born, how large, and what
//     becomes of the pill.
//   PassiveThrown: on the host, a grenade or pipe bomb a client threw that is destroyed before any explosion.
// The legs keep the held legs' shape: a readiness edge, or a failure bound that ends the leg alone.

#include "coop/dev/explosion_drill_internal.h"

#include "explosion_drill_source.h"  // the source arms' shared types (co-located private header)

#include "coop/player/players_registry.h"
#include "coop/world/explosion_request.h"
#include "coop/world/explosion_sync.h"

#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/vitals.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_attach.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace coop::dev::explosion_drill::detail {

namespace R  = ue_wrap::reflection;
namespace E  = ue_wrap::engine;
namespace ES = coop::explosion_sync;
namespace OI = ue_wrap::object_index;
namespace V  = ue_wrap::vitals;

namespace {

// ---- sticky_parent ----
//
// Any Aprop_C that simulates physics will do for the crate; prop_paper is the one the salt legs already rest and
// attach to. The shot is aimed at the crate where it came to rest, so the bomb sticks to it, not to the wall behind.
// The move is straight up: a teleport (K2_SetActorLocation, no sweep) of a fixture, not a player, and nothing to
// collide with on the way. The destroy is the drill's own (the game's K2_DestroyActor on the acting host, the local
// echo-suppressed destroy on a client): a prop has no break verb of its own, its breaking is its physics-impact
// component's damage accumulation, which ends in the same actor destroy.

// ---- selfpill ----
//
// Blueprint mainPlayer, ignite(fuel): burningTime = max(fuel, burningTime), the burning effect and audio on,
// startBurning(); extinguishFire() ends it. While the player burns and saveSlot.gasolinepilled is above 0, the burn's
// tick spawns explosion_C at the player with force, damage and scale Lerped on gasolinepilled / 100 and then writes
// gasolinepilled = 0. The field is the one prop_gascan raises on drinking; the leg writes 30 into it.

constexpr float kPilled = 30.f;
constexpr float kPillFuel = 5.f;

std::vector<ue_wrap::CachedObjRef> g_expPre;  // the explosion actors the world held when the pill was lit

void SnapshotExplosions() {
    g_expPre.clear();
    void* c = OI::ClassByName(L"explosion_C");
    if (!c) return;
    OI::ForEachInstance(
        c,
        [](void*, void* obj, int32_t) {
            if (R::NameStartsWith(R::NameOf(obj), L"Default__")) return;
            ue_wrap::CachedObjRef ref;
            ref.Set(obj);
            g_expPre.push_back(ref);
        },
        nullptr);
}

// The X scale of the newest explosion actor born since the snapshot (the pill's blast), or -1 when none stands.
float ExplosionScale() {
    void* c = OI::ClassByName(L"explosion_C");
    if (!c) return -1.f;
    float scale = -1.f;
    OI::ForEachInstance(
        c,
        [](void* ctx, void* obj, int32_t) {
            if (R::NameStartsWith(R::NameOf(obj), L"Default__")) return;
            for (const ue_wrap::CachedObjRef& q : g_expPre)
                if (q.Is(obj)) return;
            *static_cast<float*>(ctx) = E::GetActorScale3D(obj).X;
        },
        &scale);
    return scale;
}

bool CallNoParam(void* obj, const wchar_t* verb) {
    void* fn = obj ? R::FindDispatchFunctionCached(R::ClassOf(obj), verb) : nullptr;
    ue_wrap::ParamFrame f(fn);
    return fn && f.valid() && ue_wrap::Call(obj, f);
}

// ---- the host's watch of the props a client threw ----

struct Thrown {
    ue_wrap::CachedObjRef ref;
    std::wstring          key, cls;
    uint32_t              capturedAt = 0;  // the host's capture count when the prop first stood on the host
};
std::vector<Thrown> g_thrown;
constexpr size_t kMaxThrown = 32;
constexpr const wchar_t* kThrownCls[] = {L"prop_grenade_C", L"prop_pipebomb_C"};

bool Known(void* actor) {
    for (const Thrown& t : g_thrown)
        if (t.ref.Is(actor)) return true;
    return false;
}

}  // namespace

// ---- sticky_parent ----

void CrateBegin() {
    Reading& r = g_src.r;
    if (r.spawnTries > 0 && Clock::now() - r.spawnAt < kSpawnGap) return;
    void* cls = ResolveClass(kCrateCls, kCratePath);
    if (!cls) return Skip("prop_paper_C is neither loaded nor loadable by path");
    void* p = E::SpawnActor(cls, {g_src.B.X, g_src.B.Y, g_src.B.Z + kLift});
    if (!p) return Refused("the world refused the crate's spawn");
    g_src.parent.Set(p);
    g_src.rest = Rest{};
    Enter(LStep::CrateRest);
}

void CrateRest(const LegDef& d) {
    Reading& r = g_src.r;
    void* crate = g_src.parent.Get();
    if (!crate) return Skip("the crate is gone before the shot");
    if (r.parentSim < 0) {
        r.parentSim = E::IsActorRootSimulatingPhysics(crate) ? 1 : 0;
        if (r.parentSim == 0) return Skip("the crate does not simulate physics");
    }
    NotePhysics(g_src.rest, crate);
    if (!AtRest(g_src.rest)) {
        if (Late(kStepBound)) Skip("the crate never came to rest");
        return;
    }
    Vec at{};
    if (!E::TryGetActorLocation(crate, at)) return Skip("the crate's location is unreadable");
    g_src.aim = at;
    BeginItem(d);
}

void AwaitAttachCrate() {
    Reading& r = g_src.r;
    void* bomb = r.track.Get();
    if (!bomb) return Skip("the bomb is gone before it attached");
    void* parent = nullptr;
    if (E::GetAttachParentActor(bomb, parent) && parent) {
        if (parent != g_src.parent.Get()) {
            std::snprintf(r.skipText, sizeof(r.skipText), "the bomb attached to %ls, not to the crate",
                          R::ClassNameOf(parent).c_str());
            return Skip(r.skipText);
        }
        r.attached = 1;
        return Enter(LStep::Follow);
    }
    if (Past(g_src.stepAt, kAttachBound)) {
        r.attached = 0;
        Skip("the bomb never attached within its bound: the shot missed the crate");
    }
}

void Follow() {
    Reading& r = g_src.r;
    void* crate = g_src.parent.Get();
    void* bomb = r.track.Get();
    Vec   c0{}, b0{};
    if (!crate || !bomb || !E::TryGetActorLocation(crate, c0) || !E::TryGetActorLocation(bomb, b0))
        return Skip("the crate or the bomb is gone before the move");
    r.bomb0 = b0;
    if (!E::SetActorLocation(crate, {c0.X, c0.Y, c0.Z + kMoveCm})) return Skip("the crate could not be moved");
    Enter(LStep::Followed);
}

// One tick after the move: how far the bomb went with the crate; then the crate is destroyed.
void Followed(bool host) {
    Reading& r = g_src.r;
    void* bomb = r.track.Get();
    Vec   b1{};
    if (bomb && E::TryGetActorLocation(bomb, b1)) r.followed = Dist(r.bomb0, b1);
    g_src.base = ES::GetCounters();
    r.destroyAt = Clock::now();
    Remove(g_src.parent.Get(), host);
    Enter(LStep::CrateGone);
}

// The bomb's fate for the fate window, or until it is gone and a birth was seen: the window is the observation, the
// edges end it early. Then the launcher's own detonate runs (the held legs' Detonate and Birth steps).
void CrateGone(bool host) {
    Reading& r = g_src.r;
    const bool gone = !r.track.Get();
    if (gone && r.goneMs < 0) r.goneMs = Since(r.destroyAt);
    if (BirthSeen(host)) r.fateBirth = 1;
    if ((gone && r.fateBirth) || Past(r.destroyAt, kFateWindow)) {
        r.bombAlive = gone ? 0 : 1;
        Enter(LStep::Detonate);
    }
}

// ---- selfpill ----

void PillIgnite(void* player) {
    Reading& r = g_src.r;
    if (r.pillBefore < 0.f) V::Read(V::Field::GasolinePilled, &r.pillBefore);
    if (!V::Write(V::Field::GasolinePilled, kPilled)) return Skip("gasolinepilled could not be written");
    V::Read(V::Field::GasolinePilled, &r.pill0);
    Info("source leg=selfpill role=%s gasolinepilled set to %.0f (attempt %d, startInvinc %s)", RoleName(g_src.host),
         r.pill0, r.attempt, r.attempt == 0 ? "on" : "off");
    SnapshotExplosions();
    g_src.base = ES::GetCounters();
    r.selfSent = coop::explosion_request::GetCounters().requestsSent[2];
    g_src.verbAt = Clock::now();
    void* fn = R::FindDispatchFunctionCached(R::ClassOf(player), L"ignite");
    ue_wrap::ParamFrame f(fn);
    if (!(fn && f.valid() && f.Set<float>(L"fuel", kPillFuel) && ue_wrap::Call(player, f)))
        return Skip("ignite did not run on the player");
    Enter(LStep::PillWait);
}

void PillWait(bool host, void* player) {
    Reading& r = g_src.r;
    float v = 0.f;
    if (V::Read(V::Field::Burning, &v) && v > 0.5f) r.burning = 1;
    if (V::Read(V::Field::BurningTime, &v)) r.burnMax = (std::max)(r.burnMax, v);
    // The client's birth is the self request it sent: a self request is only ever a birth whose source is the local
    // player, where the first explosion seen may be any other.
    const bool born = host ? BirthSeen(true) : coop::explosion_request::GetCounters().requestsSent[2] > r.selfSent;
    if (born) {
        r.birth = true;
        r.ms = Since(g_src.verbAt);
        V::Read(V::Field::GasolinePilled, &r.pill1);
        r.scale = host ? ExplosionScale() : -1.f;
        return Enter(LStep::End);
    }
    if (!Past(g_src.verbAt, kPillBound)) return;
    V::Read(V::Field::GasolinePilled, &r.pill1);
    if (r.attempt != 0) return Enter(LStep::End);
    // The guard on, and the burn gave no explosion: say what the game showed, then the same leg with the guard off.
    Info("source leg=selfpill role=%s no birth in %lld s with startInvinc on (isBurning seen=%d burning_time_max=%.1f "
         "gasolinepilled=%.1f); running the leg again with it off", RoleName(host),
         static_cast<long long>(std::chrono::duration_cast<std::chrono::seconds>(kPillBound).count()), r.burning,
         r.burnMax, r.pill1);
    WriteBool(player, L"startInvinc", false);
    CallNoParam(player, L"extinguishFire");
    r.attempt = 1;
    r.burning = 0;
    r.burnMax = 0.f;
    PillIgnite(player);
}

// ---- the line, the put-back, the watch ----

void EmitMeasure(const LegDef& d, bool host) {
    const Reading& r = g_src.r;
    const char* role = RoleName(host);
    if (d.kind == Kind::StickyParent) {
        char last[48], followed[16], gone[24];
        LastText(last, sizeof(last));
        if (r.followed >= 0.f) std::snprintf(followed, sizeof(followed), "%.0f", r.followed);
        else std::snprintf(followed, sizeof(followed), "none");
        if (r.goneMs >= 0) std::snprintf(gone, sizeof(gone), "%lld", r.goneMs);
        else std::snprintf(gone, sizeof(gone), "none");
        Info("source leg=%s role=%s parent_sim=%d attached=%d followed=%s bomb=%s gone_ms=%s birth=%d detonated=%d "
             "det_birth=%d det_ms=%lld last=%s", d.name, role, r.parentSim, r.attached, followed,
             r.bombAlive == 0 ? "gone" : (r.bombAlive == 1 ? "alive" : "unread"), gone, r.fateBirth,
             r.detonated ? 1 : 0, r.birth ? 1 : 0, r.ms, last);
        return;
    }
    char scale[40];
    if (!host) std::snprintf(scale, sizeof(scale), "see the dissolved line");
    else if (r.scale >= 0.f) std::snprintf(scale, sizeof(scale), "%.3f", r.scale);
    else std::snprintf(scale, sizeof(scale), "unread");
    Info("source leg=%s role=%s guard=%s gasolinepilled=%.0f->%.0f burning=%d burning_time_max=%.1f birth=%d "
         "ignite_to_birth_ms=%lld scale=%s", d.name, role, r.attempt == 0 ? "on" : "off", r.pill0, r.pill1, r.burning,
         r.burnMax, r.birth ? 1 : 0, r.ms, scale);
}

// The pill leg puts back what it changed: the burn ended and the pill as it was found. Run at the leg's end and, for
// a disconnect inside the leg, from SourceReset (pillBefore is only read once the leg began).
void CleanupMeasure(const LegDef& d, bool /*host*/) {
    if (d.kind != Kind::SelfPill) return;
    CallNoParam(coop::players::Registry::Get().Local(), L"extinguishFire");
    if (g_src.r.pillBefore >= 0.f) V::Write(V::Field::GasolinePilled, g_src.r.pillBefore);
}

// A grenade or pipe bomb on the host that no run of the host's own made: the client's, thrown at it. Each is followed
// to its end; one that ends with no capture since it first stood is named by its key (<unresolved> when the prop
// never showed one). Captures are the host's only count, so the line is exact for the serial legs the client walks
// and a capture from anywhere else, while the prop stands, hides it.
void PassiveThrown() {
    const uint32_t captured = ES::GetCounters().captured;
    for (const wchar_t* cls : kThrownCls) {
        g_scan.clear();
        Count(cls, true, &g_scan);
        for (void* a : g_scan) {
            if (!R::IsLive(a) || Known(a)) continue;
            if (g_thrown.size() >= kMaxThrown) g_thrown.erase(g_thrown.begin());
            Thrown t;
            t.ref.Set(a);
            t.cls = R::ClassNameOf(a);
            t.capturedAt = captured;
            g_thrown.push_back(t);
        }
    }
    for (size_t i = 0; i < g_thrown.size();) {
        Thrown& t = g_thrown[i];
        if (void* a = t.ref.Get()) {
            if (t.key.empty() || t.key == L"None") t.key = ue_wrap::prop::GetInteractableKeyString(a);
            ++i;
            continue;
        }
        if (captured == t.capturedAt)
            Info("source passive role=host leg=unshared cls=%ls destroyed key=%ls before birth", t.cls.c_str(),
                 t.key.empty() || t.key == L"None" ? L"<unresolved>" : t.key.c_str());
        g_thrown.erase(g_thrown.begin() + static_cast<std::ptrdiff_t>(i));
    }
}

void MeasureReset() {
    g_thrown.clear();
    g_expPre.clear();
}

}  // namespace coop::dev::explosion_drill::detail
