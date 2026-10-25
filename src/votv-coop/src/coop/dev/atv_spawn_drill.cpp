// coop/dev/atv_spawn_drill.cpp -- see coop/dev/atv_spawn_drill.h.

#include "coop/dev/atv_spawn_drill.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/interactables/atv_sync.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/save/save_transfer.h"  // WorldTakenFor
#include "coop/session/join_progress.h"
#include "coop/session/net_pump.h"  // HasAnnouncedWorldReady
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/devices/atv.h"
#include "ue_wrap/engine/engine.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace coop::dev::atv_spawn_drill {
namespace {

namespace E = ue_wrap::engine;
namespace A = ue_wrap::atv;

constexpr float    kAheadCm       = 600.f;   // in front of the host's player, clear of it
constexpr float    kLiftCm        = 50.f;
constexpr uint64_t kCheckEveryMs  = 250;
constexpr int      kSlot          = 1;       // HOST: the joiner the drill serves
constexpr int      kReads         = 5;       // CLIENT: reads in a row a verdict must hold: a second at 4 Hz

// Arm, Key, Destroy, Count: the host's steps; Judge: the client's, from its world-ready announce.
enum class Step : uint8_t { Arm, Key, Destroy, Count, Judge, Done };
Step     g_step = Step::Arm;
uint64_t g_nextCheckMs = 0;
int      g_session = 1;
ue_wrap::CachedObjRef g_atv;   // HOST: the ATV the drill spawned
bool     g_destroyed = false;  // HOST, divergence: the stream's end destroyed it
int      g_readyReads = 0;     // CLIENT: reads in a row with the arm's evidence and no ghost
int      g_ghostReads = 0;     // CLIENT: reads in a row with a ghost
int      g_respawnReads = 0;   // CLIENT, join: reads in a row after a mirror was spawned for a copy the save held

const std::string& Mode() {
    static const std::string s = coop::config::ResolveString(::coop::config_registry::rows::atv_spawn_drill);
    return s;
}
bool Enabled() { return Mode() == "run" || Mode() == "join" || Mode() == "park" || Mode() == "divergence"; }
// The arms whose ATV must be in the world the host takes for the joiner.
bool Taking() { return Mode() == "join" || Mode() == "divergence"; }
void Fail(const char* what) {
    UE_LOGW("[ATV-SPAWN-DRILL] FAIL in session %d: %s", g_session, what);
    g_step = Step::Done;
}
void FailKey(const char* fmt, const std::wstring& key) {
    char why[160];
    std::snprintf(why, sizeof(why), fmt, key.c_str());
    Fail(why);
}
void Abandon(const char* why) {
    UE_LOGW("[ATV-SPAWN-DRILL] ABANDONED in session %d: %s", g_session, why);
    g_step = Step::Done;
}
bool Due() {
    const uint64_t now = ::GetTickCount64();
    if (now < g_nextCheckMs) return false;
    g_nextCheckMs = now + kCheckEveryMs;
    return true;
}

// The live ATV actors by key, from the lane's own walk.
struct Seen {
    std::unordered_map<std::wstring, int> perKey;
    size_t actors = 0;
};
Seen ReadAtvs() {
    Seen seen;
    coop::atv_sync::ForEachAnnounceableAtv([](void* ctx, void*, const std::wstring& key) {
        auto* s = static_cast<Seen*>(ctx);
        ++s->perKey[key];
        ++s->actors;
    }, &seen);
    return seen;
}

void HostTick(coop::net::Session* s) {
    if (g_step == Step::Done || !Due()) return;
    if (g_step == Step::Arm) {
        // join, divergence: the ATV must be in the world the host takes for the slot, so a slot whose world was
        // taken already is a window missed, the drill unable to do its part.
        if (Taking() && coop::save_transfer::WorldTakenFor(kSlot)) { Abandon("the capture preceded the spawn"); return; }
        if (!(Taking() ? s->IsSlotReady(kSlot) : s->IsSlotWorldReady(kSlot))) return;
        void* me = coop::players::Registry::Get().Local();
        ue_wrap::FVector at{};
        if (!me || !E::TryGetActorLocation(me, at)) return;
        void* cls = ue_wrap::object_index::ClassByName(L"ATV_C");
        if (!cls) { Abandon("the ATV's class is not loaded"); return; }
        const ue_wrap::FVector fwd = E::GetActorForwardVector(me);
        void* spawned = E::SpawnActor(cls, {at.X + fwd.X * kAheadCm, at.Y + fwd.Y * kAheadCm, at.Z + kLiftCm});
        if (!spawned) { Abandon("the ATV did not spawn"); return; }
        g_atv.Set(spawned);
        UE_LOGI("[ATV-SPAWN-DRILL] host (%s): an ATV spawned in front of this player", Mode().c_str());
        g_step = Step::Key;
        return;
    }
    if (g_step == Step::Key) {
        void* atv = g_atv.Get();
        if (!atv) { Abandon("the drill's ATV is gone before it had a key"); return; }
        if (coop::atv_sync::AnnounceableAtvKey(A::GetKeyString(atv))) {
            g_step = Mode() == "divergence" ? Step::Destroy : Step::Count;
            return;
        }
        if (Taking() && coop::save_transfer::WorldTakenFor(kSlot)) Abandon("the ATV had no key at the capture");
        return;
    }
    if (g_step == Step::Destroy) {
        if (g_destroyed) g_step = Step::Count;   // the stream's end destroyed it (OnSlotStreamComplete)
        return;
    }
    UE_LOGI("[ATV-SPAWN-DRILL] host count atvs=%zu", ReadAtvs().actors);
    g_step = Step::Done;
}

void ClientTick() {
    if (g_step == Step::Done) return;
    if (g_step == Step::Arm) {
        if (coop::net_pump::HasAnnouncedWorldReady()) g_step = Step::Judge;
        return;
    }
    if (!Due()) return;
    const Seen seen = ReadAtvs();
    for (const auto& kv : seen.perKey)
        if (kv.second > 1) { FailKey("two actors for key '%ls'", kv.first); return; }
    // The judged state: the host's set applied, the join over, nothing waiting in the park. Only in it can an
    // ATV the host never announced be called a ghost, and only in it is the arm's evidence final.
    const bool judged = coop::atv_sync::SetEdgesApplied() >= 1 &&
                        coop::join_progress::CurrentPhase() == coop::join_progress::Phase::Idle &&
                        coop::atv_sync::ParkPending() == 0;
    const std::vector<std::wstring> named = coop::atv_sync::ReceivedSpawnKeys();
    std::wstring ghost;
    for (const auto& kv : seen.perKey)
        if (std::find(named.begin(), named.end(), kv.first) == named.end()) { ghost = kv.first; break; }
    const bool haveGhost = judged && !ghost.empty();
    const uint32_t adopted = coop::atv_sync::AdoptedEver(), spawned = coop::atv_sync::SpawnedEver();
    const std::string& mode = Mode();
    // The arm's own evidence: the drill's ATV is the only one a run needs a mirror for.
    const bool armOk = mode == "run"        ? spawned >= 1
                       : mode == "park"     ? spawned >= 1 && coop::atv_sync::DeliberateParks() >= 1
                       : mode == "join"     ? spawned == 0 && adopted >= 1
                                            : coop::atv_sync::DestroysApplied() >= 1;
    g_ghostReads = haveGhost ? g_ghostReads + 1 : 0;
    if (g_ghostReads >= kReads) { FailKey("a ghost ATV '%ls' the host does not hold", ghost); return; }
    g_respawnReads = mode == "join" && spawned > 0 ? g_respawnReads + 1 : 0;
    if (g_respawnReads >= kReads) { Fail("an ATV the save held was spawned again"); return; }
    g_readyReads = judged && !haveGhost && armOk ? g_readyReads + 1 : 0;
    if (g_readyReads < kReads) return;
    UE_LOGI("[ATV-SPAWN-DRILL] client DONE in session %d (%s): %zu ATV key(s), one actor each (adopted %u, spawned %u) -- PASS",
            g_session, mode.c_str(), seen.perKey.size(), adopted, spawned);
    UE_LOGI("[ATV-SPAWN-DRILL] client count atvs=%zu", seen.actors);
    g_step = Step::Done;
}

}  // namespace

void Tick(coop::net::Session* s) {
    if (!Enabled() || !s) return;
    if (s->role() == coop::net::Role::Host) HostTick(s);
    else if (s->connected()) ClientTick();
}

void OnSlotStreamComplete(int slot) {
    if (slot != kSlot || Mode() != "divergence" || g_step == Step::Done) return;
    if (g_step != Step::Destroy) { Abandon("the stream completed before the ATV had a key"); return; }
    void* atv = g_atv.Get();
    if (!atv || !A::DestroyMirror(atv)) { Abandon("the drill's ATV could not be destroyed at the stream's end"); return; }
    g_destroyed = true;
}

void OnDisconnect() {
    if (!Enabled()) return;
    ++g_session;
    g_step = Step::Arm;
    g_nextCheckMs = 0;
    g_atv.Reset();
    g_destroyed = false;
    g_readyReads = g_ghostReads = g_respawnReads = 0;
}

}  // namespace coop::dev::atv_spawn_drill
