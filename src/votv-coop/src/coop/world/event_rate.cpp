#include "coop/world/event_rate.h"

#include "coop/config/config.h"
#include "coop/net/session_serial.h"
#include "coop/net/session.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/facts/function_row.h"

#include <chrono>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace coop::event_rate {
namespace {
namespace R = ue_wrap::reflection;
namespace W = ue_wrap::world_identity;
UE_FACT_FN(kInterval, "event_rate", L"Actor", L"SetActorTickInterval", Declared);

// These tick bodies re-arm their own spawn wait. The first four products have
// host-owned NPC/world-actor routes; night eyes use the existing owner-entity lane;
// fireflies use the peer-symmetric emitter capture/replay route. The flickerer
// produces only lamp/ambient setActive phases, carried by their host-only lanes.
// Fossilhound UG
// @1829 and yellow-wisp UG @1322 are native interval writes. Never include
// ticker_base, creature AI ticks or the personal Insomniac ticker.
constexpr const wchar_t* kSpawners[] = {
    L"ticker_fossilhoundSpawner_C", L"ticker_yellowWispSpawner_C",
    L"ticker_wispSpawner_C", L"ticker_egSpawner_C",
    L"ticker_eyers_C", L"ticker_fireflySpawner_C",
    L"ticker_flickerer_C",
};
struct Wait {
    ue_wrap::CachedObjRef actor;
    float native = 0.f;
    float applied = 0.f;
};
net::Session* g_session = nullptr;
ue_wrap::CachedObjRef g_hook;
std::unordered_map<void*, Wait> g_waits;
bool g_writing = false;
uint64_t g_epoch = 0;
uint32_t g_serial = 0, g_world = 0;
std::chrono::steady_clock::time_point g_scan{};

bool Host() {
    return g_session && g_session->running() && g_session->role() == net::Role::Host;
}
bool Active() { return g_session && g_session->running(); }
bool CanApply(void* actor) {
    // Night eyes already use the owner-entity lane. Each owner keeps their own
    // native night/day/chance roll; the shared setting scales only its wait.
    // Fireflies are peer-symmetric: every peer runs its local-camera roll and
    // the existing capture/replay path shares the resulting emitter.
    return Host() || (Active() && g_session->role() == net::Role::Client &&
        (R::ClassNameOf(actor) == L"ticker_eyers_C" ||
         R::ClassNameOf(actor) == L"ticker_fireflySpawner_C"));
}
bool InWorld(void* actor) {
    return actor && R::IsLive(actor) && W::CurrentWorld() &&
        W::CurrentWorldKind() == W::WorldKind::Gameplay && W::WorldOf(actor) == W::CurrentWorld();
}
bool Spawner(void* actor) {
    const auto name = R::ClassNameOf(actor);
    for (const auto* cls : kSpawners) if (name == cls) return true;
    return false;
}
float Rate() {
    const float value = config::ResolveFloat(config_registry::rows::world_event_rate);
    return std::isfinite(value) && value >= .5f && value <= 5.f ? value : 2.f;
}
bool Read(void* actor, float& interval) {
    ue_wrap::ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(actor), L"GetActorTickInterval"));
    return f.valid() && ue_wrap::Call(actor, f) && f.GetRaw(L"ReturnValue", &interval, sizeof(interval)) &&
        std::isfinite(interval) && interval > 0.f;
}
bool Write(void* actor, float interval) {
    ue_wrap::ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(actor), L"SetActorTickInterval"));
    if (!f.valid() || !f.Set<float>(L"TickInterval", interval)) return false;
    g_writing = true;
    struct Scope { ~Scope() { g_writing = false; } } scope;
    return ue_wrap::Call(actor, f);
}
void Apply(void* actor, bool nativeWrite) {
    if (g_writing || !InWorld(actor) || !Spawner(actor) || !CanApply(actor)) return;
    const auto epoch = g_epoch;
    const auto world = W::Generation(), serial = net::session_serial::Current();
    auto* session = g_session;
    ue_wrap::CachedObjRef ref; ref.Set(actor);
    const auto valid = [&] {
        return epoch == g_epoch && g_session == session && world == W::Generation() &&
            serial == net::session_serial::Current() && ref.Get() == actor && InWorld(actor) && CanApply(actor);
    };
    float current = 0.f;
    if (!Read(actor, current) || !valid()) return;
    auto& wait = g_waits[actor];
    // Each native re-arm supplies a fresh baseline, even if it happens to pick
    // exactly the last applied interval. A settings edit instead keeps baseline.
    if (nativeWrite || !wait.actor.Is(actor) || current != wait.applied) {
        wait.actor.Set(actor);
        wait.native = current;
        wait.applied = current;
    }
    const float desired = wait.native / Rate();
    if (current != desired && Write(actor, desired) && valid()) {
        const auto found = g_waits.find(actor);
        if (found != g_waits.end() && found->second.actor.Is(actor)) found->second.applied = desired;
    }
}
void IntervalPost(void* actor, void*, void*) { Apply(actor, true); }
void Collect(void* ctx, void* actor, int32_t) {
    if (!InWorld(actor)) return;
    ue_wrap::CachedObjRef ref; ref.Set(actor);
    static_cast<std::vector<ue_wrap::CachedObjRef>*>(ctx)->push_back(ref);
}
}

void Install(net::Session* session) {
    if (g_session != session || g_serial != net::session_serial::Current() || g_world != W::Generation())
        OnDisconnect();
    g_session = session;
    g_serial = net::session_serial::Current();
    g_world = W::Generation();
    if (!Active()) return;
    if (!g_hook.Alive()) {
        const auto fn = ue_wrap::facts::Resolve(kInterval);
        if (!fn || !ue_wrap::ufunction_hook::InstallPostHook(fn, &IntervalPost)) return;
        g_hook.Set(fn.Fn());
    }
    const auto now = std::chrono::steady_clock::now();
    if (g_scan.time_since_epoch().count() && now - g_scan < std::chrono::seconds(1)) return;
    g_scan = now;
    for (auto it = g_waits.begin(); it != g_waits.end();) {
        if (!it->second.actor.Alive()) it = g_waits.erase(it); else ++it;
    }
    // Also enroll actors loaded before hosting, and late ticker classes. Collect
    // first: reflected calls must not mutate the object index during its walk.
    std::vector<ue_wrap::CachedObjRef> actors;
    for (const auto* name : kSpawners)
        if (void* cls = ue_wrap::object_index::ClassByName(name))
            ue_wrap::object_index::ForEachInstance(cls, &Collect, &actors);
    for (const auto& ref : actors) if (void* actor = ref.Get()) Apply(actor, false);
}

void OnDisconnect() {
    g_session = nullptr;
    ++g_epoch;
    const auto waits = std::move(g_waits);
    g_waits.clear();
    for (const auto& [_, wait] : waits) {
        if (void* actor = wait.actor.Get(); actor && InWorld(actor)) {
            float current = 0.f;
            if (Read(actor, current) && current == wait.applied) Write(actor, wait.native);
        }
    }
    g_scan = {};
}
}
