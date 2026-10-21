// coop/world/event_cue_sync.cpp -- see coop/world/event_cue_sync.h.
//
// HOST-AUTHORITATIVE: only the host detects and broadcasts (clients run a dormant scheduler --
// time_sync pins TimeScale=0 -- so they never fire an event). The detection is the eventer's own
// runEvent, watched through the script gate: the cue's emitter is spawned synchronously inside that
// body (an EX_CallMath SpawnEmitterAtLocation, invisible to our ProcessEvent detour), so the watch
// sees the fire the moment it happens, with its row name. Clients replay the emitter through a
// reflected SpawnEmitterAtLocation.

#include "coop/world/event_cue_sync.h"
#include "coop/world/world_sound_sync.h"

#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/players_registry.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/engine/world_identity.h"

#include <atomic>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

namespace coop::event_cue_sync {
namespace {

namespace P  = ue_wrap::profile;
namespace R  = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;
namespace sg = ue_wrap::script_gate;

std::atomic<coop::net::Session*> g_session{nullptr};

// ---- the cue registry: cosmetic emitter cues -----------------------------------------
// cueId == index. APPEND-ONLY -- the id is on the wire. starRain is bytecode-verified: the eventer's
// runEvent('starRain') spawns eff_shootingStar_rain at (0,0,6000) and returns (trigger_eventer @4709),
// and no other Blueprint spawns that template. A cue is detected by its runEvent row, so a new one
// needs its row and a location the body fixes; a cue whose position the body computes, or that spawns
// after a delay, needs the spawned component captured inside the body instead.
struct CueDef {
    const char* name;             // log label, and the list_events row that fires it
    const wchar_t* row;           // the runEvent row name
    const wchar_t* templateName;  // the UParticleSystem object the SpawnEmitter call uses
    ue_wrap::FVector loc;         // where the body spawns it
};
constexpr CueDef kCues[] = {
    { "starRain", L"starRain", L"eff_shootingStar_rain", { 0.f, 0.f, 6000.f } },  // Meteor Shower / Shooting Star
};
constexpr int kCueCount = static_cast<int>(sizeof(kCues) / sizeof(kCues[0]));

// Resolved on demand (a cue's particle asset loads with the eventer's class): the templates for the
// replay and the join snapshot, the rows' names for the watch. Game thread.
ue_wrap::CachedObjRef g_cueTemplates[kCueCount];
R::FName g_cueRows[kCueCount] = {};
bool g_rowsResolved = false;

// Reflected spawn path (resolved once -- cloned from firefly_sync).
ue_wrap::CachedObjRef g_gameplayStaticsCdo, g_spawnEmitterFn;

std::atomic<unsigned> g_replays{0};  // emitters this peer replayed (ReplayCount)

// The host's watch on runEvent, registered once per process.
constexpr int kTagEventCue = 0x45564355;  // 'EVCU'
UE_FACT_FN(kRowRunEvent, "event_cue_sync", nullptr, L"runEvent", AnyClass);
bool g_watchInstalled = false;
bool g_watchLive = false;
ue_wrap::CachedObjRef g_runEventFn;
int32_t g_offEventParam = -1;

struct Pending {
    coop::net::EventCuePayload payload{};
    std::chrono::steady_clock::time_point expires{};
    ue_wrap::CachedObjRef source; // join snapshots expire with their native component
};
struct Recipient { std::deque<Pending> queue; uint32_t generation = 0; };
std::array<Recipient, coop::net::kMaxPeers> g_out;
std::deque<Pending> g_in;
uint32_t g_world = 0, g_serial = 0;
uint64_t g_epoch = 0;
bool g_flushing = false;

bool As(coop::net::Role role) {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->running() && s->connected() && s->role() == role;
}
void Clear() {
    ++g_epoch; g_in.clear(); g_out = {};
    g_world = ue_wrap::world_identity::Generation();
    g_serial = coop::net::session_serial::Current();
}
void Refresh() {
    if (g_world != ue_wrap::world_identity::Generation() || g_serial != coop::net::session_serial::Current()) Clear();
    auto* s = g_session.load(std::memory_order_acquire);
    for (int slot = 1; slot < coop::net::kMaxPeers; ++slot) {
        auto& peer = g_out[slot];
        const auto generation = As(coop::net::Role::Host) && s->HasPeerConn(slot) ? s->peerGenerationForSlot(slot) : 0;
        if (generation != peer.generation) { peer = {}; peer.generation = generation; }
    }
}
void Flush();

void* CueTemplate(int cueId) {
    if (!g_cueTemplates[cueId].Alive())
        g_cueTemplates[cueId].Set(R::FindObject(kCues[cueId].templateName, L"ParticleSystem"));
    return g_cueTemplates[cueId].Get();
}

bool ResolveSpawn() {
    if (!g_gameplayStaticsCdo.Alive()) {
        g_gameplayStaticsCdo.Set(R::FindClassDefaultObject(P::name::GameplayStaticsClass));
        g_spawnEmitterFn.Reset();
    }
    if (void* cdo = g_gameplayStaticsCdo.Get(); cdo && !g_spawnEmitterFn.Alive())
        g_spawnEmitterFn.Set(R::FindFunction(R::ClassOf(cdo), L"SpawnEmitterAtLocation"));
    return g_gameplayStaticsCdo.Alive() && g_spawnEmitterFn.Alive();
}

// The exact runEvent and its row parameter; the eventer's class loads with the world.
bool ResolveRunEvent() {
    if (g_offEventParam >= 0 && g_runEventFn.Alive()) return true;
    void* cls = ue_wrap::object_index::ClassByName(L"trigger_eventer_C");
    if (!cls) return false;
    g_runEventFn.Set(R::FindFunction(cls, L"runEvent"));
    g_offEventParam = g_runEventFn.Alive() ? R::FindParamOffset(g_runEventFn.Get(), L"event") : -1;
    return g_offEventParam >= 0;
}

void Queue(int slot, int cueId, const ue_wrap::CachedObjRef& source = {}) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!As(coop::net::Role::Host) || slot <= 0 || slot >= coop::net::kMaxPeers ||
        !s->HasPeerConn(slot) || !s->IsSlotWorldReady(slot)) return;
    auto& queue = g_out[slot].queue;
    if (queue.size() >= 32) { UE_LOGW("event_cue: peer %d cue queue full", slot); return; }
    const ue_wrap::FVector& loc = kCues[cueId].loc;
    queue.push_back({{static_cast<uint32_t>(cueId), loc.X, loc.Y, loc.Z},
        std::chrono::steady_clock::now() + std::chrono::seconds(10), source});
}

// HOST: the eventer's body has run for a row; if the row spawns a cue, send it. Every host fire
// counts, whoever called runEvent (the scheduler, the dev menu, the game's own event menu).
void OnRunEventPost(const sg::Call& call) {
    if (!GT::IsGameThread() || !As(coop::net::Role::Host) || !call.locals || !call.object ||
        !ResolveRunEvent() || call.function != g_runEventFn.Get()) return;
    if (!R::IsLive(call.object) || !ue_wrap::world_identity::CurrentWorld() ||
        ue_wrap::world_identity::WorldOf(call.object) != ue_wrap::world_identity::CurrentWorld()) return;
    const int size = R::FunctionFrameSize(call.function);
    if (g_offEventParam < 0 || size < int(sizeof(R::FName)) || g_offEventParam > size - int(sizeof(R::FName))) return;
    Refresh();
    const auto epoch = g_epoch;
    ue_wrap::CachedObjRef source; source.Set(call.object);
    R::FName row{};
    std::memcpy(&row, call.locals + g_offEventParam, sizeof(row));
    if (!g_rowsResolved) {
        for (int i = 0; i < kCueCount; ++i) g_cueRows[i] = ue_wrap::fname_utils::StringToFName(kCues[i].row);
        g_rowsResolved = true;
    }
    if (epoch != g_epoch || g_world != ue_wrap::world_identity::Generation() ||
        g_serial != coop::net::session_serial::Current() || !As(coop::net::Role::Host) || !source.Alive()) return;
    for (int i = 0; i < kCueCount; ++i) {
        if (row.ComparisonIndex != g_cueRows[i].ComparisonIndex || row.Number != g_cueRows[i].Number) continue;
        for (int slot = 1; slot < coop::net::kMaxPeers; ++slot) Queue(slot, i);
        Flush();
        UE_LOGI("event_cue: host broadcast '%s' (cue %d) at (%.0f, %.0f, %.0f)", kCues[i].name, i,
                kCues[i].loc.X, kCues[i].loc.Y, kCues[i].loc.Z);
        return;
    }
}

bool Replay(const coop::net::EventCuePayload& payload) {
    const auto epoch = g_epoch;
    if (!ResolveSpawn()) return false;
    void* tmpl = CueTemplate(static_cast<int>(payload.cueId));
    if (!tmpl) return false;
    void* worldCtx = coop::players::Registry::Get().Local();
    if (!worldCtx || !R::IsLive(worldCtx) || !ue_wrap::world_identity::CurrentWorld() ||
        ue_wrap::world_identity::WorldOf(worldCtx) != ue_wrap::world_identity::CurrentWorld()) return false;
    ue_wrap::CachedObjRef local; local.Set(worldCtx);
    const auto asset = g_cueTemplates[payload.cueId], cdo = g_gameplayStaticsCdo, fn = g_spawnEmitterFn;

    ue_wrap::FVector loc{ payload.x, payload.y, payload.z };
    ue_wrap::FVector scale{ 1.f, 1.f, 1.f };
    ue_wrap::FRotator rot{ 0.f, 0.f, 0.f };
    ue_wrap::ParamFrame f(fn.Get());
    if (!f.valid() || !f.Set<void*>(L"WorldContextObject", worldCtx) || !f.Set<void*>(L"EmitterTemplate", tmpl) ||
        !f.SetRaw(L"Location", &loc, sizeof(loc)) || !f.SetRaw(L"Rotation", &rot, sizeof(rot)) ||
        !f.SetRaw(L"Scale", &scale, sizeof(scale)) || !f.Set<bool>(L"bAutoDestroy", true) ||
        !f.Set<uint8_t>(L"PoolingMethod", 0) || !f.Set<bool>(L"bAutoActivateSystem", true)) return false;
    if (epoch != g_epoch || g_world != ue_wrap::world_identity::Generation() ||
        g_serial != coop::net::session_serial::Current() || !As(coop::net::Role::Client) ||
        local.Get() != worldCtx || coop::players::Registry::Get().Local() != worldCtx ||
        asset.Get() != tmpl || !cdo.Alive() || !fn.Alive()) return false;
    if (!ue_wrap::Call(cdo.Get(), f)) {
        UE_LOGW("event_cue: '%s' (cue %u) replay dispatch FAILED", kCues[payload.cueId].name, payload.cueId);
        return true; // native dispatch may already have spawned; do not duplicate it
    }
    if (epoch != g_epoch || g_world != ue_wrap::world_identity::Generation() ||
        g_serial != coop::net::session_serial::Current() || !As(coop::net::Role::Client)) return true;
    g_replays.fetch_add(1, std::memory_order_relaxed);
    UE_LOGI("event_cue: replayed '%s' (cue %u) at (%.0f, %.0f, %.0f)",
            kCues[payload.cueId].name, payload.cueId, payload.x, payload.y, payload.z);
    return true;
}

void Flush() {
    if (g_flushing) return;
    g_flushing = true;
    struct Busy { ~Busy() { g_flushing = false; } } busy;
    Refresh();
    const auto epoch = g_epoch;
    const auto now = std::chrono::steady_clock::now();
    const auto current = [&] {
        return epoch == g_epoch && g_world == ue_wrap::world_identity::Generation() &&
            g_serial == coop::net::session_serial::Current();
    };
    if (As(coop::net::Role::Host)) {
        auto* s = g_session.load(std::memory_order_acquire);
        for (int slot = 1; slot < coop::net::kMaxPeers; ++slot) {
            auto& peer = g_out[slot];
            if (!s->IsSlotWorldReady(slot)) { peer.queue.clear(); continue; }
            const auto generation = peer.generation;
            for (int work = 0; !peer.queue.empty() && work < 4; ++work) {
                const auto pending = peer.queue.front();
                if (now >= pending.expires || (pending.source.Raw() && !pending.source.Alive())) { peer.queue.pop_front(); continue; }
                const bool sent = s->SendReliableToSlot(slot, coop::net::ReliableKind::EventCue, &pending.payload, sizeof(pending.payload));
                if (!current() || !As(coop::net::Role::Host)) return;
                if (!s->HasPeerConn(slot) || generation != s->peerGenerationForSlot(slot)) { peer = {}; break; }
                if (!sent) break;
                peer.queue.pop_front();
            }
        }
    } else if (As(coop::net::Role::Client)) {
        for (int work = 0; !g_in.empty() && work < 4; ++work) {
            const auto pending = g_in.front();
            if (now >= pending.expires) { g_in.pop_front(); continue; }
            const bool played = Replay(pending.payload);
            if (!current() || !As(coop::net::Role::Client)) return;
            if (!played) break;
            g_in.pop_front();
        }
    }
}

}  // namespace

void Install(coop::net::Session* session) {
    if (!GT::IsGameThread()) return;
    if (g_session.load(std::memory_order_acquire) != session) {
        g_session.store(session, std::memory_order_release); Clear();
    }
    world_sound_sync::Install(session);
    if (!g_watchInstalled)
        g_watchInstalled = sg::WatchName(kRowRunEvent, kTagEventCue, nullptr, &OnRunEventPost);
    if (g_watchInstalled && !g_watchLive) {
        sg::ResolvePendingNames();
        if (sg::NameWatchLive(kRowRunEvent, kTagEventCue)) {
            g_watchLive = true;
            UE_LOGI("event_cue: the host's cue fires are seen at runEvent (a script-gate watch)");
        }
    }
    Flush();
}

void OnReliable(const coop::net::EventCuePayload& payload) {
    if (world_sound_sync::OnReliable(payload)) return;
    if (!GT::IsGameThread() || !As(coop::net::Role::Client)) return;
    if (payload.cueId >= static_cast<uint32_t>(kCueCount) ||
        !std::isfinite(payload.x) || !std::isfinite(payload.y) || !std::isfinite(payload.z) ||
        std::fabs(payload.x) > 2'000'000.f || std::fabs(payload.y) > 2'000'000.f || std::fabs(payload.z) > 2'000'000.f) return;
    Refresh();
    if (g_in.size() >= 32) { UE_LOGW("event_cue: playback queue full"); return; }
    g_in.push_back({payload, std::chrono::steady_clock::now() + std::chrono::seconds(10), {}});
    Flush();
}

void QueueConnectBroadcastForSlot(int peerSlot) {
    // HOST, at the joiner's world-ready. Each live cue emitter was sent when it spawned, and the
    // send gate dropped it for this still-loading slot; the gate opens in the same handler that
    // calls this, so every live one goes out here exactly once and every later one by its own send.
    if (!GT::IsGameThread()) return;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!As(coop::net::Role::Host) || peerSlot <= 0 || peerSlot >= coop::net::kMaxPeers ||
        !s->HasPeerConn(peerSlot) || !s->IsSlotWorldReady(peerSlot)) return;
    Refresh();
    const auto epoch = g_epoch, generation = uint64_t(s->peerGenerationForSlot(peerSlot));
    struct Ctx { void* tmpls[kCueCount]; std::vector<std::pair<ue_wrap::CachedObjRef, int>> found; };
    Ctx ctx{};
    bool anyLoaded = false;
    for (int i = 0; i < kCueCount; ++i) anyLoaded |= (ctx.tmpls[i] = CueTemplate(i)) != nullptr;
    if (!anyLoaded) return;  // no cue asset loaded -> no live cue possible
    void* pscCls = ue_wrap::object_index::ClassByName(L"ParticleSystemComponent");
    if (!pscCls) return;
    ue_wrap::object_index::ForEachInstance(pscCls, [](void* c, void* psc, int32_t index) {
        auto& ctx = *static_cast<Ctx*>(c);
        if (!R::IsLiveByIndex(psc, index) ||
            ue_wrap::world_identity::WorldOf(psc) != ue_wrap::world_identity::CurrentWorld()) return;
        void* tmpl = ue_wrap::engine::GetParticleSystemTemplate(psc);
        for (int i = 0; i < kCueCount; ++i) {
            if (!tmpl || tmpl != ctx.tmpls[i]) continue;
            ue_wrap::CachedObjRef source; source.Set(psc);
            ctx.found.emplace_back(source, i);
        }
    }, &ctx);
    // Never send from inside the object-index callback: send callbacks can retire objects.
    if (epoch != g_epoch || !As(coop::net::Role::Host) || g_session.load() != s ||
        generation != s->peerGenerationForSlot(peerSlot)) return;
    for (const auto& [source, id] : ctx.found) if (source.Alive()) Queue(peerSlot, id, source);
    Flush();
    // No native components means no join replay.
}

void OnDisconnect() {
    g_session.store(nullptr, std::memory_order_release);
    Clear();
    world_sound_sync::OnDisconnect();
}

unsigned ReplayCount() { return g_replays.load(std::memory_order_relaxed); }

}  // namespace coop::event_cue_sync
