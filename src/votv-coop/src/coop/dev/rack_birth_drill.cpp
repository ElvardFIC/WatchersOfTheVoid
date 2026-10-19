// coop/dev/rack_birth_drill.cpp -- see coop/dev/rack_birth_drill.h.

#include "coop/dev/rack_birth_drill.h"

#include "coop/config/config.h"
#include "coop/element/registry.h"
#include "coop/interactables/drive_rack_sync.h"
#include "coop/interactables/signal_wire.h"
#include "coop/net/blob_chunks.h"
#include "coop/net/session.h"
#include "coop/session/net_pump.h"

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/desk/drive_chain.h"
#include "ue_wrap/engine/engine.h"  // SpawnActor + TryGetActorLocation (the host's rack spawn)

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace coop::dev::rack_birth_drill {
namespace {

namespace R  = ue_wrap::reflection;
namespace DC = ue_wrap::drive_chain;

constexpr int kFilledRows = 4;

std::atomic<coop::net::Session*> g_session{nullptr};

ue_wrap::CachedObjRef g_rack;    // the rack: the host's spawn, the client's mirror
uint32_t g_rackEid = 0;          // 0 = not known (the host's eid comes at the spawn drain)
bool     g_spawnTried = false;
bool     g_held = false;         // HOST: the rack is spawned and filled
bool     g_judged = false;       // HOST: the verdict is said
uint64_t g_canonAtHold = 0;      // HOST: the canonicals the rack lane had sent when the rows were written
bool     g_done = false;         // CLIENT: the rows arrived

bool Enabled() {
    static const bool s = coop::config::ResolveFlag(::coop::config_registry::rows::rack_birth_drill);
    return s;
}

// Fixed content, so the two peers' digests can be compared (a non-zero date: no receiver stamps one).
DC::RackRow MakeRow(int k) {
    DC::RackRow r;
    r.has = true;
    r.row.name = L"RACK-BIRTH-" + std::to_wstring(k);
    r.row.id = L"RB-" + std::to_wstring(k);
    r.row.object = L"rackbirth";
    r.row.signal = L"rackbirth";
    r.row.size = 10.f;
    r.row.decoded = 10.f;
    r.row.date = 638000000000000000LL;
    r.row.frequency = static_cast<uint8_t>(k);
    r.row.quality = 3;
    return r;
}

// Fnv64 over [has byte + serialized row] x16, read straight from the rack's arrays.
bool RackDigest(void* rack, uint64_t& outDigest, int& outFilled) {
    DC::RackRow rows[DC::kRackSlots];
    int n = 0;
    if (!rack || !DC::ReadRack(rack, rows, n)) return false;
    std::vector<uint8_t> acc;
    outFilled = 0;
    for (int i = 0; i < DC::kRackSlots; ++i) {
        acc.push_back(rows[i].has ? 1 : 0);
        if (rows[i].has) ++outFilled;
        std::vector<uint8_t> rb = coop::signal_wire::Serialize(rows[i].row);
        acc.insert(acc.end(), rb.begin(), rb.end());
    }
    outDigest = coop::blob_chunks::Fnv64(acc);
    return true;
}

void HostFail(const char* why) {
    UE_LOGW("[RACK-BIRTH] FAIL host: %s", why);
    g_judged = true;
}

// The rack's eid, found in the registry as the pair whose actor is the spawned one.
void FindHostRackEid() {
    std::vector<coop::element::Registry::ActorIdPair> pairs;
    coop::element::Registry::Get().SnapshotActorsByType(coop::element::ElementType::Prop, pairs);
    for (const auto& p : pairs)
        if (p.actor && g_rack.Is(p.actor)) { g_rackEid = static_cast<uint32_t>(p.id); return; }
}

// Spawn at the desk play slot (+100, 0, +60), fill rows 0-3 and read the digest, all in this tick.
void HostSpawnAndFill() {
    g_spawnTried = true;
    void* cls = R::FindClass(L"prop_driveRack_C");
    void* slot = DC::SlotActor(DC::kRoleDeskPlay);
    if (!cls || !slot) { HostFail("the rack could not be spawned (class or desk slot missing)"); return; }
    ue_wrap::FVector loc{};
    if (!ue_wrap::engine::TryGetActorLocation(slot, loc)) {
        HostFail("the rack could not be spawned (the desk slot's location could not be read)");
        return;
    }
    void* rack = ue_wrap::engine::SpawnActor(cls, {loc.X + 100.f, loc.Y, loc.Z + 60.f});
    if (!rack) { HostFail("the rack could not be spawned (SpawnActor returned null)"); return; }
    g_rack.Set(rack);
    for (int k = 0; k < kFilledRows; ++k) {
        if (!DC::WriteRackRow(rack, k, MakeRow(k))) {
            HostFail("the rack's rows could not be written");
            return;
        }
    }
    DC::CallRackGen(rack);
    g_canonAtHold = coop::drive_rack_sync::HostCanonicalsSent();
    uint64_t digest = 0;
    int filled = 0;
    if (!RackDigest(g_rack.Get(), digest, filled)) { HostFail("the rack's rows could not be read"); return; }
    UE_LOGI("[RACK-BIRTH] host HOLD filled=%d digest=%016llx", filled, static_cast<unsigned long long>(digest));
    g_held = true;
}

void HostTick(coop::net::Session& s) {
    if (g_judged || !s.running() || !s.AnyWorldReadyPeer()) return;
    if (!g_spawnTried) { HostSpawnAndFill(); return; }
    if (!g_held) return;
    if (g_rackEid == 0) FindHostRackEid();
    if (g_rackEid == 0 || !coop::drive_rack_sync::HostSeesRack(g_rackEid)) return;
    if (coop::drive_rack_sync::HostCanonicalsSent() > g_canonAtHold)
        UE_LOGI("[RACK-BIRTH] host ANNOUNCED eid=%u", g_rackEid);
    else
        UE_LOGW("[RACK-BIRTH] FAIL host: the rack lane took rack eid=%u in silently -- its rows left for no client",
                g_rackEid);
    g_judged = true;
}

// The one live drive rack in the registry (a fresh save holds none).
bool ResolveRack() {
    if (g_rack.Alive()) return true;
    std::vector<coop::element::Registry::ActorIdPair> pairs;
    coop::element::Registry::Get().SnapshotActorsByType(coop::element::ElementType::Prop, pairs);
    for (const auto& p : pairs) {
        if (!p.actor || !R::IsLiveByIndex(p.actor, p.internalIdx)) continue;
        if (!DC::IsRackClass(R::ClassOf(p.actor))) continue;
        g_rack.Set(p.actor);
        g_rackEid = static_cast<uint32_t>(p.id);
        return true;
    }
    return false;
}

void ClientTick() {
    if (g_done || !coop::net_pump::IsWorldSettled() || !ResolveRack()) return;
    uint64_t digest = 0;
    int filled = 0;
    if (!RackDigest(g_rack.Get(), digest, filled) || filled != kFilledRows) return;
    UE_LOGI("[RACK-BIRTH] client DONE filled=%d digest=%016llx -- PASS", filled,
            static_cast<unsigned long long>(digest));
    g_done = true;
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
}

void Tick() {
    if (!Enabled()) return;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !DC::EnsureResolved()) return;
    if (s->role() == coop::net::Role::Host) HostTick(*s);
    else ClientTick();
}

void OnDisconnect() {
    g_session.store(nullptr, std::memory_order_release);
    g_rack.Reset();
    g_rackEid = 0;
    g_spawnTried = false;
    g_held = false;
    g_judged = false;
    g_canonAtHold = 0;
    g_done = false;
}

}  // namespace coop::dev::rack_birth_drill
