// coop/dev/native_spawn_probe.cpp -- see coop/dev/native_spawn_probe.h.

#include "coop/dev/native_spawn_probe.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/creatures/npc_adoption.h"
#include "coop/creatures/npc_sync.h"
#include "coop/net/session.h"

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"  // PropertyOffsetAndClass
#include "ue_wrap/core/sdk_profile_names.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/engine/engine_pawn.h"  // DestroyActor
#include "ue_wrap/facts/function_row.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <string>

namespace coop::dev::native_spawn_probe {

UE_FACT_FN(kRowNb0Spawn, "dev.native_spawn_probe", L"GameplayStatics", L"BeginDeferredActorSpawnFromClass", Declared);

namespace {

namespace R  = ue_wrap::reflection;
namespace UH = ue_wrap::ufunction_hook;
namespace OI = ue_wrap::object_index;
namespace P  = ue_wrap::profile;

enum class Mode : unsigned char { Off, Record, Orb };
enum class Orb : unsigned char { Wait, Drive, Done };

// One source seen: the names are FNames (an index compare per call, no render), rendered when a line prints.
struct Key {
    R::FName src, out, fn;
    int32_t  offset;
    uint32_t count;
};
constexpr int kMaxKeys = 64;

constexpr uint64_t kRetryMs = 500;
constexpr int      kTries = 10;
constexpr uint64_t kOrbBoundMs = 60000;  // the failure bound: a spawn needs a nav point near the camera
constexpr int32_t  kOrbLaps = 241;       // blackballSpawner_C::maxlaps; the next wrap passes it
constexpr float    kOrbAngle = 540.f;    // past 360, so the next tick's wrap test is true

Mode ModeOf() {
    static const Mode mode = [] {
        const std::string v = coop::config::ResolveEnum(::coop::config_registry::rows::native_spawn_probe);
        return v == "record" ? Mode::Record : v == "orb" ? Mode::Orb : Mode::Off;
    }();
    return mode;
}

coop::net::Session* g_session = nullptr;
bool     g_hookDone = false;
bool     g_hookFailed = false;
int      g_tries = 0;
uint64_t g_triedAt = 0;
void*    g_gsCdo = nullptr;  // an engine CDO: lives for the process
Key      g_keys[kMaxKeys] = {};
int      g_nKeys = 0;
bool     g_fullSaid = false;

Orb      g_orb = Orb::Wait;
bool     g_orbSeen = false;
ue_wrap::CachedObjRef g_orbActor;
uint64_t g_orbStartAt = 0;
int32_t  g_lapsOff = -1;
int32_t  g_angleOff = -1;
int      g_orbInstances = 0;

uint64_t Now() { return ::GetTickCount64(); }

bool IsHost() { return g_session && g_session->role() == coop::net::Role::Host; }

bool SameName(const R::FName& a, const R::FName& b) {
    return a.ComparisonIndex == b.ComparisonIndex && a.Number == b.Number;
}

std::wstring Render(const R::FName& n) { return n.ComparisonIndex ? R::ToString(n) : std::wstring(L"none"); }

void Say(const char* verb, const Key& k, const char* tail) {
    UE_LOGI("[NB-0] %s role=%s %ls from %ls fn=%ls@%d%s", verb, IsHost() ? "host" : "client", Render(k.out).c_str(),
            Render(k.src).c_str(), Render(k.fn).c_str(), k.offset, tail);
}

// The post hook: game thread, deep inside the engine spawn, so allocation-free until a new key prints.
void OnSpawnPost(void*, void* source, void* result) {
    if (!result || !source || source == g_gsCdo) return;  // a ProcessEvent-dispatched spawn: the interceptor's or ours
    void* const cls = R::ClassOf(result);
    void* const srcCls = R::ClassOf(source);
    if (!cls || !srcCls || !coop::npc_sync::IsAllowlistedClass(cls)) return;
    const UH::CallerFrame frame = UH::CurrentCallerFrame();
    const R::FName fn = frame.function ? R::NameOf(frame.function) : R::FName{0, 0};
    const Key probe{R::NameOf(srcCls), R::NameOf(cls), fn, frame.codeOffset, 0};

    if (g_orb == Orb::Drive && !g_orbSeen && R::NameEquals(probe.src, L"blackballSpawner_C") &&
        R::NameEquals(probe.out, L"npc_orborb_C")) {
        g_orbSeen = true;
        g_orbActor.Set(result);
    }

    for (int i = 0; i < g_nKeys; ++i) {
        Key& k = g_keys[i];
        if (k.offset == probe.offset && SameName(k.fn, probe.fn) && SameName(k.src, probe.src) &&
            SameName(k.out, probe.out)) {
            ++k.count;
            return;
        }
    }
    if (g_nKeys >= kMaxKeys) {
        if (!g_fullSaid) {
            g_fullSaid = true;
            UE_LOGW("[NB-0] key table FULL at %d -- later new sources are not recorded", kMaxKeys);
        }
        return;
    }
    Key& k = g_keys[g_nKeys++];
    k = probe;
    k.count = 1;
    if (IsHost()) {
        Say("spawn", k, "");
        return;
    }
    char tail[24];
    std::snprintf(tail, sizeof tail, " after-sweep=%d", coop::npc_adoption::GhostSweepDone() ? 1 : 0);
    Say("spawn", k, tail);
    UE_LOGI("[NB-0] NATIVE %ls -> %ls", Render(k.src).c_str(), Render(k.out).c_str());
}

void TryHook() {
    if (g_hookDone || g_hookFailed || Now() - g_triedAt < kRetryMs) return;
    g_triedAt = Now();
    const bool final = ++g_tries >= kTries;
    g_gsCdo = R::FindClassDefaultObject(P::name::GameplayStaticsClass);
    const ue_wrap::facts::Resolved r = ue_wrap::facts::Resolve(kRowNb0Spawn);
    if (g_gsCdo && r && UH::InstallPostHook(r, &OnSpawnPost)) {
        g_hookDone = true;
        UE_LOGI("[NB-0] record live mode=%s", ModeOf() == Mode::Orb ? "orb" : "record");
    } else if (final) {
        g_hookFailed = true;
        UE_LOGI("[NB-0] BROKEN hook BeginDeferredActorSpawnFromClass");
    }
}

// ---- the orb drive ----

struct Write {
    int32_t lapsOff, angleOff, laps;
    bool    angle;
};

void WriteInstance(void* ctx, void* obj, int32_t) {
    const Write& w = *static_cast<const Write*>(ctx);
    auto* base = static_cast<uint8_t*>(obj);
    *reinterpret_cast<int32_t*>(base + w.lapsOff) = w.laps;
    if (w.angle) *reinterpret_cast<float*>(base + w.angleOff) = kOrbAngle;
}

// The drive's values to every blackballSpawner_C (laps alone, back to 0, when `drive` is false); the count.
int WriteSpawners(void* cls, bool drive) {
    Write w{g_lapsOff, g_angleOff, drive ? kOrbLaps : 0, drive};
    return static_cast<int>(OI::ForEachInstance(cls, &WriteInstance, &w));
}

// laps is an int and angle a float, by their reflected types; false (said) when the dump no longer holds.
bool ResolveOrbOffsets(void* cls) {
    std::wstring lapsType, angleType;
    const bool found = R::PropertyOffsetAndClass(cls, L"laps", g_lapsOff, lapsType) &&
                       R::PropertyOffsetAndClass(cls, L"angle", g_angleOff, angleType);
    if (found && g_lapsOff >= 0 && g_angleOff >= 0 && lapsType == L"IntProperty" && angleType == L"FloatProperty")
        return true;
    UE_LOGI("[NB-0] orb BROKEN blackballSpawner_C laps=%ls@%d angle=%ls@%d", lapsType.c_str(), g_lapsOff,
            angleType.c_str(), g_angleOff);
    return false;
}

void FinishOrb(void* cls, const char* answer) {
    if (cls && g_lapsOff >= 0 && g_angleOff >= 0) WriteSpawners(cls, /*drive=*/false);
    if (answer) UE_LOGI("[NB-0] orb native %s instances=%d", answer, g_orbInstances);
    UE_LOGI("[NB-0] orb DONE");
    g_orb = Orb::Done;
    g_orbActor.Reset();
}

void TickOrb() {
    if (g_orb == Orb::Done || IsHost() || !coop::npc_adoption::GhostSweepDone()) return;
    void* const cls = OI::ClassByName(L"blackballSpawner_C");
    if (!cls) return;
    if (g_orb == Orb::Wait) {
        if (!ResolveOrbOffsets(cls)) {
            FinishOrb(nullptr, nullptr);
            return;
        }
        g_orb = Orb::Drive;
        g_orbStartAt = Now();
        UE_LOGI("[NB-0] orb drive start");
    }
    if (g_orbSeen) {
        // The record fired inside the spawn call, before the Blueprint finished the actor; this Tick is after the
        // whole graph run, so the orb is finished and its 1 s delay has not begun to run out.
        if (g_orbActor.Alive()) ue_wrap::engine::DestroyActor(g_orbActor.Raw());
        FinishOrb(cls, "spawned");
        return;
    }
    if (Now() - g_orbStartAt >= kOrbBoundMs) {
        FinishOrb(cls, "not-spawned");
        return;
    }
    g_orbInstances = WriteSpawners(cls, /*drive=*/true);
}

}  // namespace

void Install(coop::net::Session* session) {
    if (ModeOf() == Mode::Off) return;
    g_session = session;
    TryHook();
}

void Tick(coop::net::Session* session) {
    if (ModeOf() != Mode::Orb || !g_hookDone) return;
    g_session = session;
    TickOrb();
}

void OnDisconnect() {
    if (ModeOf() == Mode::Off) return;
    for (int i = 0; i < g_nKeys; ++i) {
        char tail[24];
        std::snprintf(tail, sizeof tail, " count=%u", g_keys[i].count);
        Say("total", g_keys[i], tail);
    }
    g_nKeys = 0;
    g_fullSaid = false;
    g_orbActor.Reset();
    g_orbSeen = false;
    if (g_orb == Orb::Drive) {
        UE_LOGI("[NB-0] orb ABANDONED");
        g_orb = Orb::Wait;
    }
}

}  // namespace coop::dev::native_spawn_probe
