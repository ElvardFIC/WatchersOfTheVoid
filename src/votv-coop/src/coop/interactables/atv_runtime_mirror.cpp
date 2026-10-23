// coop/interactables/atv_runtime_mirror.cpp -- a client's mirror of an ATV the host announced (declared in
// coop/interactables/atv_sync.h). The host announces an ATV under its game Key; this client fresh-spawns a
// native ATV carrying that Key, physics on so it is grabbable, and destroys it when the host's goes.
// A spawn landing while this world is not ready waits in the park (atv_spawn_park.h). The map stays
// atv_sync.cpp's: this half asks it to adopt or drop a row (atv_sync_internal.h).

#include "coop/interactables/atv_sync.h"
#include "atv_spawn_park.h"
#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/interactables/atv_index_internal.h"
#include "coop/interactables/atv_sync_internal.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/session/world_load_episode.h"
#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/devices/atv.h"
#include "ue_wrap/engine/world_identity.h"

#include <cmath>
#include <cstdint>
#include <string>

namespace coop::atv_sync {
namespace {

namespace R = ue_wrap::reflection;
namespace A = ue_wrap::atv;
using ue_wrap::FVector;
using ue_wrap::FRotator;
using coop::net::StringFromWireKey;

std::wstring WireClassNameToString(const coop::net::WireClassName& in) {
    std::wstring s;
    const uint8_t n = in.len <= sizeof(in.data) ? in.len : static_cast<uint8_t>(sizeof(in.data));
    s.reserve(n);
    for (uint8_t i = 0; i < n; ++i) s.push_back(static_cast<wchar_t>(static_cast<unsigned char>(in.data[i])));
    return s;
}

}  // namespace

void OnAtvSpawn(const coop::net::AtvSpawnPayload& payload) {
    // From the host alone: the host's receive edge refuses a client's copy (client_authorship.h).
    auto* s = LaneSession();
    if (!s || !s->connected()) return;
    if (!detail::WireKeyNamesAnAtv(payload.key, "client")) return;
    const std::wstring key = StringFromWireKey(payload.key);
    // The payload is checked before any wait: a malformed spawn is dropped, only a sound one is deferred.
    if (!std::isfinite(payload.x) || !std::isfinite(payload.y) || !std::isfinite(payload.z) ||
        !std::isfinite(payload.pitch) || !std::isfinite(payload.yaw) || !std::isfinite(payload.roll)) {
        UE_LOGW("atv: OnAtvSpawn non-finite pose key='%ls' -- dropping", key.c_str());
        spawn_park::Discard(key);  // a malformed spawn is final
        return;
    }
    std::wstring className = WireClassNameToString(payload.className);
    if (className.empty()) {
        UE_LOGW("atv: OnAtvSpawn empty className key='%ls' -- dropping", key.c_str());
        spawn_park::Discard(key);
        return;
    }
    if (ue_wrap::world_identity::CurrentWorldKind() != ue_wrap::world_identity::WorldKind::Gameplay ||
        !IndexCurrent() || !coop::world_load_episode::HasQuiesced()) {
        spawn_park::Park(key, payload, "the receiving world is not ready");
        return;
    }
    // The ATV spawn drill's park arm: the first spawn reaching a ready world is parked as if it were not, so the
    // park's hand-back is what spawns it. A hand-back is already the park's work and is let through.
    static const bool s_parkFirst = coop::config::ResolveFlag(coop::config_registry::rows::atv_park_first_spawn);
    static bool s_parkedFirst = false;
    if (s_parkFirst && !s_parkedFirst && !spawn_park::Holds(key)) {
        s_parkedFirst = true;
        spawn_park::Park(key, payload, "the drill parks the first spawn");
        return;
    }
    if (!A::EnsureResolved()) { spawn_park::Park(key, payload, "the ATV wrapper is not resolved"); return; }
    // The key becomes a name through the string library, which resolves at boot, long before a gameplay
    // world: until it does the spawn waits (a parked entry keeps the world it first arrived in).
    if (!ue_wrap::fname_utils::Resolved()) {
        static bool sSaidUnresolved = false;
        if (!sSaidUnresolved) {
            sSaidUnresolved = true;
            UE_LOGW("atv: OnAtvSpawn key='%ls' waits -- the name library is not resolved (said once)", key.c_str());
        }
        spawn_park::Park(key, payload, "the name library is not resolved");
        return;
    }
    if (const AtvEntry* known = FindEntry(key)) {
        if (R::IsLiveByIndex(known->actor, known->idx)) { spawn_park::Discard(key); return; }
        EraseEntry(key);   // a dead row is not "already spawned"
    }
    const FVector loc{ payload.x, payload.y, payload.z };
    const FRotator rot{ payload.pitch, payload.yaw, payload.roll };
    void* spawned = A::SpawnMirror(className, loc, rot, key);  // physics LEFT ON -- a native idle grabbable ATV
    if (!spawned) {
        // In a world that reads ready, a refused spawn is the class, the key or the engine saying no: final,
        // not retried, and never a mirror under a key of its own making.
        UE_LOGW("atv: OnAtvSpawn SpawnMirror failed key='%ls' class='%ls' -- not retried", key.c_str(),
                className.c_str());
        spawn_park::Discard(key);
        return;
    }
    spawn_park::Discard(key);
    AdoptClientMirror(key, spawned);
    UE_LOGI("atv: spawned ATV mirror key='%ls' class='%ls' actor=%p loc=(%.0f, %.0f, %.0f)",
            key.c_str(), className.c_str(), spawned, loc.X, loc.Y, loc.Z);
}

uint32_t ParkedEver() { return spawn_park::ParkedEver(); }
size_t ParkPending() { return spawn_park::Pending(); }

void OnAtvDestroy(const coop::net::AtvDestroyPayload& payload) {
    if (!LaneSession()) return;  // from the host alone: its receive edge refuses a client's copy
    if (!detail::WireKeyNamesAnAtv(payload.key, "client")) return;
    const std::wstring key = StringFromWireKey(payload.key);
    spawn_park::Discard(key);  // a destroy ends a spawn still waiting
    if (!IndexCurrent()) return;  // the pass prunes a dead-world entry itself
    const AtvEntry* row = FindEntry(key);
    if (!row) return;
    const bool destroyed = row->isClientSpawnedMirror && R::IsLiveByIndex(row->actor, row->idx) &&
                           A::DestroyMirror(row->actor);  // our own fresh spawn
    EraseEntry(key);
    UE_LOGI("atv: ATV mirror key='%ls' %s", key.c_str(),
            destroyed ? "destroyed" : "dropped from the lane, not destroyed here");
}

}  // namespace coop::atv_sync
