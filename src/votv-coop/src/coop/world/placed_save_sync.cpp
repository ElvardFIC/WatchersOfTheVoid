#include "coop/world/placed_save_sync.h"
#include "coop/world/wall_build_sync.h"
#include "coop/element/intent_authority.h"
#include "coop/element/object_scan_hub.h"
#include "coop/items/save_record_wire.h"
#include "coop/net/blob_chunks.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
#include "ue_wrap/actors/placed_save.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/world/keyed_objects.h"
#include <chrono>
#include <algorithm>
#include <deque>
#include <map>
#include <random>
#include <vector>

namespace coop::placed_save_sync {
namespace {
namespace P = ue_wrap::placed_save;
namespace R = ue_wrap::reflection;
namespace SG = ue_wrap::script_gate;
namespace W = coop::save_record_wire;
using Record = ue_wrap::save_record::SaveRecord;
enum Op : uint8_t { Create, Remove, State, Retired };
constexpr int kTag = 0x504C5341;
constexpr size_t kMaxRows = 1024;
constexpr uint64_t kRetryMs = 1000;
struct Row {
    ue_wrap::CachedObjRef ref;
    Record record;
    bool local = false, ready = false, accepted = false, incoming = false, settled = true;
    bool dead = false, remove = false, dirty = false, link = false, retireLookup = false;
    uint8_t owner = 0;
    uint32_t playerNo = 0;
    uint64_t next = 0, linkAt = 0, manualUntil = 0;
};
std::map<std::wstring, Row> g_rows;
net::Session* g_session = nullptr;
blob_chunks::Assembler g_assembler;
ue_wrap::CachedObjRef g_finish;
uint32_t g_world = 0, g_nonce = 0, g_keySeq = 0, g_blobSeq = 0;
uint64_t g_epoch = 0, g_sweep = 0, g_tickAt = 0;
int g_applying = 0;
bool g_busy = false, g_watched = false;
bool g_scanRegistered = false;
std::map<int32_t, ue_wrap::CachedObjRef> g_discoveredWalls;
std::deque<ue_wrap::CachedObjRef> g_enrollWalls;
struct Applying { Applying() { ++g_applying; } ~Applying() { --g_applying; } };
uint64_t Now() {
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
bool Host() { return g_session && g_session->role() == net::Role::Host; }
bool Active() { return g_session && g_session->connected() && ue_wrap::game_thread::IsGameThread(); }
bool Tracking() { return g_session && g_session->running() && ue_wrap::game_thread::IsGameThread() && (Host() || g_session->connected()); }
void ResetWorld() {
    g_rows.clear(); g_assembler.Clear(); g_sweep = g_tickAt = 0; ++g_epoch;
    g_discoveredWalls.clear(); g_enrollWalls.clear();
    g_world = ue_wrap::world_identity::Generation();
}
void World() { if (g_world != ue_wrap::world_identity::Generation()) ResetWorld(); }
bool Ours(const std::wstring& key) {
    if (key.size() != 22 || key.compare(0, 5, L"mvpl_") || key[13] != L'_') return false;
    for (size_t i = 5; i < key.size(); ++i) {
        if (i == 13) continue;
        if (!((key[i] >= L'0' && key[i] <= L'9') || (key[i] >= L'a' && key[i] <= L'f'))) return false;
    }
    return true;
}
std::wstring Mint() {
    if (!g_nonce) { std::random_device rd; g_nonce = uint32_t(rd()) | 1u; }
    wchar_t key[32];
    swprintf(key, 32, L"mvpl_%08x_%08x", g_nonce, ++g_keySeq);
    return key;
}
bool Send(Op op, const Record& record) {
    std::vector<uint8_t> bytes;
    W::AppU8(bytes, op);
    if (op == Remove || op == Retired) W::AppWStr(bytes, record.key);
    else W::SerSave(bytes, record);
    return blob_chunks::SendBlob(g_session, net::ReliableKind::PlacedSave, ++g_blobSeq, bytes);
}
bool ValidKey(const std::wstring& key) {
    if (key.empty() || key == L"None" || key.size() > 128) return false;
    for (auto c : key) if (c < 32 || c > 126) return false;
    return true;
}
// Discovery rides the shared sliced index pass. Never run getData/getKey from
// its callback: those can reenter the engine or allocate objects.
bool WallClass(void* actor) { return P::IsWall(P::ClassKind(R::ClassNameOf(actor))); }
bool ScanReady() { return true; }
void ScanBegin(void*, bool) {
    if (!Tracking() || !Host()) return;
    World();
    std::erase_if(g_discoveredWalls, [](const auto& p) { return !p.second.Get(); });
}
void ScanWall(void*, void* actor) {
    if (!Tracking() || !Host() || !actor || !R::IsLive(actor)) return;
    const auto world = ue_wrap::world_identity::CurrentWorld();
    if (!world || ue_wrap::world_identity::WorldOf(actor) != world) return;
    World();
    ue_wrap::CachedObjRef ref; ref.Set(actor);
    auto it = g_discoveredWalls.find(ref.Idx());
    if (it != g_discoveredWalls.end() && it->second.Is(actor)) return;
    if (g_discoveredWalls.size() >= kMaxRows || g_enrollWalls.size() >= kMaxRows) return;
    g_discoveredWalls[ref.Idx()] = ref; g_enrollWalls.push_back(ref);
}
size_t ScanComplete(void*, bool, uint32_t) { return g_discoveredWalls.size(); }
// Preserve native save keys so a joiner's save-loaded twin is reused. Only the
// host may enroll legacy keys; client Create/Remove still require minted keys.
bool EnrollWall(const ue_wrap::CachedObjRef& ref) {
    void* actor = ref.Get();
    if (!actor || !P::IsWall(P::Type(actor))) return true;
    if (!ue_wrap::keyed_objects::Available() || !P::Ready(actor)) return false;
    const auto epoch = g_epoch;
    Applying applying;
    Record raw, record;
    if (!P::Capture(actor, raw) || !ref.Get() || epoch != g_epoch) return false;
    // A census cannot rename an actor: a join snapshot may already contain its
    // old identity. Wait for native key initialization; express births mint in Finished.
    if (!ValidKey(raw.key)) return false;
    if (!P::Normalize(raw, record)) return false;
    auto it = g_rows.find(record.key);
    if (it != g_rows.end() && (it->second.dead || it->second.ref.Raw())) return true;
    if (it == g_rows.end() && g_rows.size() >= kMaxRows) return false;
    void* existing = ue_wrap::keyed_objects::Resolve(record.key.c_str());
    if (epoch != g_epoch || !ref.Get()) return false;
    if (existing && existing != actor) return false;
    if (!P::SetKey(actor, record.key) || epoch != g_epoch || !ref.Get()) return false;
    // Refetch after native getKey; no iterator may cross a native callback.
    it = g_rows.find(record.key);
    if (it != g_rows.end() && (it->second.dead || it->second.ref.Raw())) return true;
    Row row; row.ref = ref; row.record = std::move(record);
    row.ready = row.accepted = row.dirty = true;
    g_rows.insert_or_assign(row.record.key, row);
    return true;
}
void EnrollWalls() {
    const auto epoch = g_epoch;
    const auto count = (std::min)(size_t(4), g_enrollWalls.size());
    for (size_t i = 0; i < count; ++i) {
        const auto ref = g_enrollWalls.front(); g_enrollWalls.pop_front();
        const bool done = EnrollWall(ref);
        if (epoch != g_epoch || !Tracking() || !Host()) return;
        if (!done && ref.Get()) g_enrollWalls.push_back(ref);
    }
}
void Finished(void*, void*, void* result) {
    if (!Tracking() || g_applying || !result) return;
    const auto kind = P::Type(result);
    if (kind == P::Kind::None || !P::PlacementCaller(kind, ue_wrap::ufunction_hook::CurrentCallerFrame().function)) return;
    World();
    if (g_rows.size() >= kMaxRows) { UE_LOGW("placed_save: placement table full; new placement not shared"); return; }
    const auto key = Mint(); const auto epoch = g_epoch;
    ue_wrap::CachedObjRef ref; ref.Set(result);
    Applying applying;
    if (!P::SetKey(result, key) || !ref.Get() || epoch != g_epoch) return;
    Row row; row.ref = ref; row.local = true; row.record.key = key;
    row.settled = kind != P::Kind::Hose;
    g_rows.emplace(key, std::move(row));
}
void HoseFinished(const SG::Call& c) {
    if (!Active() || g_applying || c.fromOurCode) return;
    World();
    for (auto& [key, row] : g_rows)
        if (row.local && row.ref.Is(c.object)) { row.settled = true; row.next = 0; break; }
}
SG::Verdict BeforeAction(const SG::Call& c) {
    if (!Active() || Host() || g_applying || c.fromOurCode) return SG::Verdict::Run;
    World();
    for (auto& [key, row] : g_rows)
        if (row.ref.Is(c.object)) { row.manualUntil = Now() + 2000; break; }
    return SG::Verdict::Run;
}
// Save-loaded actors keep their persistent identity. Never announce the world's
// naturally generated trees: only keys minted by the placement lane are enrolled.
void Loaded(const SG::Call& c) {
    if (!Tracking() || g_applying || c.fromOurCode || P::Type(c.object) == P::Kind::None) return;
    if (Host() && P::IsWall(P::Type(c.object))) { ScanWall(nullptr, c.object); return; }
    World();
    Record record; const auto epoch = g_epoch;
    ue_wrap::CachedObjRef ref; ref.Set(c.object);
    Applying applying;
    if (!P::Capture(c.object, record) || !ref.Get() || epoch != g_epoch || !Ours(record.key)) return;
    if (auto it = g_rows.find(record.key); it != g_rows.end()) {
        if (it->second.dead) { it->second.ref = ref; it->second.next = 0; }
        return;
    }
    if (g_rows.size() >= kMaxRows) return;
    Row row; row.ref = ref; row.record = record; row.accepted = true;
    row.ready = true; g_rows.emplace(record.key, std::move(row));
}
bool Reach(uint8_t slot, const Record& r) {
    const auto target = element::IntentTarget::ForClientIntent(*g_session, slot, 1000.f);
    ue_wrap::FVector p{r.xform[4], r.xform[5], r.xform[6]};
    // A hose is committed from its completed far end, not from the faucet where
    // its actor root still stands. Both are valid positions for its placer.
    if (P::ClassKind(r.className) == P::Kind::Hose && !r.vectors.empty() && !r.vectors[0].empty()) {
        const auto& end = r.vectors[0].back(); p = {end[0], end[1], end[2]};
    }
    return bool(target.AuthorizeSegment(p, p));
}
void Dead(Row& row) {
    row.dead = true; row.remove = false; row.incoming = false; row.link = false;
    row.accepted = true; row.dirty = Host(); row.next = 0;
}
}

void Install(net::Session* session) {
    if (g_session != session) { g_session = session; ResetWorld(); }
    wall_build_sync::Install(session);
    if (!g_scanRegistered) {
        element::scan_hub::Register({"placed_walls", nullptr, &ScanReady, &WallClass,
            &ScanBegin, &ScanWall, &ScanComplete, 0});
        g_scanRegistered = true;
    }
    if (!g_watched) {
        g_watched = SG::WatchClassName(L"faucet_C", L"loadData", kTag, nullptr, &Loaded) &&
            SG::WatchClassName(L"groundHose_C", L"loadData", kTag, nullptr, &Loaded) &&
            SG::WatchClassName(L"growingPlant_C", L"loadData", kTag, nullptr, &Loaded) &&
            SG::WatchClassName(L"customWall_C", L"loadData", kTag, nullptr, &Loaded) &&
            SG::WatchClassName(L"customWall_unfinished_C", L"loadData", kTag, nullptr, &Loaded) &&
            SG::WatchClassName(L"groundHose_C", L"finish", kTag, nullptr, &HoseFinished) &&
            SG::WatchClassName(L"faucet_C", L"actionOptionIndex", kTag, &BeforeAction, nullptr) &&
            SG::WatchClassName(L"groundHose_C", L"actionOptionIndex", kTag, &BeforeAction, nullptr) &&
            SG::WatchClassName(L"growingPlant_C", L"actionOptionIndex", kTag, &BeforeAction, nullptr);
    }
    if (!g_finish.Get()) {
        void* cls = ue_wrap::object_index::ClassByName(L"GameplayStatics");
        void* fn = cls ? R::FindFunction(cls, L"FinishSpawningActor") : nullptr;
        if (!fn) return;
        const int off = R::FindParamOffset(fn, L"ReturnValue"), size = R::FunctionFrameSize(fn);
        if (off < 0 || size < off || size - off < int(sizeof(void*))) return;
        if (ue_wrap::ufunction_hook::InstallPostHook(fn, &Finished)) g_finish.Set(fn);
    }
}
void Receive(const net::BlobChunkPayload& chunk, uint8_t slot) {
    if (!Active() || slot >= players::kMaxPeers || (Host() ? slot == 0 : slot != 0)) return;
    World();
    std::vector<uint8_t> bytes;
    if (chunk.chunks > 300 || !g_assembler.OnChunk(chunk, slot, bytes) || bytes.size() > 64 * 1024) return;
    size_t offset = 0; uint8_t op = 0; Record record;
    if (!W::RdU8(bytes, offset, op) || op > Retired || (Host() ? op > Remove : op < State)) return;
    if (op == Remove || op == Retired) {
        if (!W::RdWStr(bytes, offset, record.key)) return;
    } else {
        Record input;
        if (!W::DeSave(bytes, offset, input) || !P::Normalize(input, record)) return;
    }
    if (offset != bytes.size() || !ValidKey(record.key)) return;
    // Legacy identity is host-authored and wall-only. Unknown legacy tombstones
    // carry no schema, so their lookup below must enforce that same class gate.
    if (!Ours(record.key) && (Host() || (op == State && !P::IsWall(P::ClassKind(record.className))))) return;
    auto it = g_rows.find(record.key);
    if (Host()) {
        if (it != g_rows.end()) {
            // Repeated creation never reapplies stale planting/fuel/geometry state.
            if (op == Create) { if (it->second.accepted) it->second.dirty = true; return; }
            const Row row = it->second;
            if (row.dead) { it->second.dirty = true; return; }
            void* actor = row.ref.Get();
            if (actor && !element::IntentTarget::ForClientIntent(*g_session, slot, 1000.f).Authorize(actor) &&
                !Reach(slot, row.record)) return;
            if (!actor && row.owner != slot) return;
            const auto epoch = g_epoch;
            Applying applying;
            if (actor) ue_wrap::engine::DestroyActor(actor);
            if (epoch != g_epoch) return;
            if (row.ref.Get()) return; // failed native destroy is not a retirement
            it = g_rows.find(record.key);
            if (it != g_rows.end()) Dead(it->second);
            return;
        }
        if (g_rows.size() >= kMaxRows || !roster_ledger::Get(slot).playerNo) return;
        // A cancel may complete before a partially sent creation blob. Retain
        // its tombstone so the delayed creation cannot resurrect the item.
        Row row; row.record = record; row.owner = slot;
        row.playerNo = roster_ledger::Get(slot).playerNo;
        if (op == Remove) {
            const auto epoch = g_epoch;
            if (!ue_wrap::keyed_objects::Available()) return;
            void* existing = ue_wrap::keyed_objects::Resolve(record.key.c_str());
            if (epoch != g_epoch || existing) return; // not an unknown, cancelled creation
            Dead(row);
        }
        else {
            if (!Reach(slot, record)) return;
            row.ready = true; row.incoming = true;
        }
        g_rows.emplace(record.key, std::move(row));
    } else {
        if (it == g_rows.end()) {
            if (g_rows.size() >= kMaxRows) return;
            Row row; row.record = record; row.ready = true;
            row.incoming = op == State; row.dead = op == Retired; row.accepted = row.dead;
            row.retireLookup = row.dead;
            g_rows.emplace(record.key, std::move(row)); return;
        }
        if (op == Retired) {
            const auto epoch = g_epoch; const auto ref = it->second.ref;
            Dead(it->second);
            Applying applying;
            if (void* a = ref.Get()) ue_wrap::engine::DestroyActor(a);
            if (epoch != g_epoch) return;
        } else if (!it->second.dead) {
            // The creator already owns the correctly initialized native actor.
            // Acknowledgement must neither respawn it nor overwrite a newer local use.
            const bool wasAccepted = it->second.accepted;
            it->second.accepted = true;
            if (it->second.ref.Raw() && !it->second.ref.Get()) {
                if (it->second.manualUntil >= Now() || !wasAccepted) it->second.remove = true;
                else { it->second.ref.Reset(); it->second.local = false; }
            }
            // Walls are host-driven after placement: the creator must receive
            // progress/health too, rather than keeping its original empty outline.
            if ((!it->second.local || P::IsWall(P::ClassKind(record.className))) && !it->second.remove) {
                it->second.record = record; it->second.incoming = true;
                it->second.next = 0;
            }
        }
    }
}
void Tick() {
    if (g_busy || !ue_wrap::game_thread::IsGameThread()) return;
    g_busy = true;
    struct Busy { ~Busy() { g_busy = false; } } busy;
    // Drain native construction queued just before the last guest disconnected.
    if (!Tracking()) { wall_build_sync::Tick(); return; }
    World();
    const auto now = Now();
    if (now < g_tickAt) return;
    g_tickAt = now + 100;
    const auto epoch = g_epoch;
    wall_build_sync::Tick();
    if (epoch != g_epoch) return;
    if (Host()) EnrollWalls();
    if (epoch != g_epoch) return;
    if (now >= g_sweep) {
        g_sweep = now + 1000;
        g_assembler.Sweep(std::chrono::steady_clock::now(), std::chrono::seconds(20));
    }
    std::vector<std::wstring> keys;
    for (const auto& [key, row] : g_rows) keys.push_back(key);
    int work = 0;
    for (const auto& key : keys) {
        if (epoch != g_epoch) return;
        auto it = g_rows.find(key); if (it == g_rows.end()) continue;
        if (now < it->second.next) continue;
        // Resting accepted rows need only a weak-lifetime check. Do not copy a
        // potentially 4096-point hose record on every frame/poll.
        if (it->second.accepted && !it->second.dead && !it->second.incoming &&
            !it->second.remove && !it->second.dirty && !it->second.link && it->second.ref.Get()) {
            it->second.next = now + 500; continue;
        }
        Row row = it->second;
        void* a = row.ref.Get();
        if (row.dead && (a || row.retireLookup)) {
            if (++work > 4) break;
            it->second.next = now + 500;
            Applying applying;
            if (!a && row.retireLookup) {
                if (!ue_wrap::keyed_objects::Available()) continue;
                a = ue_wrap::keyed_objects::Resolve(key.c_str());
                if (epoch != g_epoch) return;
                if (a && (P::Type(a) == P::Kind::None || (!Ours(key) && !P::IsWall(P::Type(a))))) continue;
                it = g_rows.find(key); if (it == g_rows.end()) continue;
                it->second.ref.Set(a); it->second.retireLookup = false;
            }
            if (a) ue_wrap::engine::DestroyActor(a);
            continue; // failed destruction is retried; never silently abandon a live twin
        }
        if (a && row.local && !row.ready && row.settled) {
            if (++work > 4) break;
            it->second.next = now + 250;
            Applying applying;
            Record raw, normalized;
            if (!P::Ready(a) || !P::Capture(a, raw) || !P::Normalize(raw, normalized) ||
                normalized.key != key || !row.ref.Get()) continue;
            if (epoch != g_epoch) return;
            it = g_rows.find(key); if (it == g_rows.end()) continue;
            it->second.record = normalized; it->second.ready = true;
            it->second.accepted = Host(); it->second.dirty = Host();
            it->second.next = 0; continue;
        }
        if (row.incoming) {
            if (++work > 4) break;
            it->second.next = now + 500;
            if (Host() && roster_ledger::Get(row.owner).playerNo != row.playerNo) { g_rows.erase(it); continue; }
            if (!ue_wrap::keyed_objects::Available()) continue;
            Applying applying;
            void* existing = ue_wrap::keyed_objects::Resolve(key.c_str());
            if (epoch != g_epoch) return;
            if (existing) {
                // A joiner may already have loaded this exact actor from the save.
                // Never bind another class under an incoming identity.
                if (R::ClassNameOf(existing) != row.record.className) continue;
                if (Host()) {
                    Record raw, normalized;
                    if (!P::Capture(existing, raw) || !P::Normalize(raw, normalized)) continue;
                    // An existing host actor wins a colliding creation identity.
                    row.record = std::move(normalized);
                } else if (!P::Apply(existing, row.record)) continue;
                a = existing;
            } else a = P::Spawn(row.record);
            if (epoch != g_epoch) return;
            if (!a) continue;
            it = g_rows.find(key); if (it == g_rows.end()) continue;
            it->second.record = row.record;
            it->second.ref.Set(a); it->second.incoming = false; it->second.accepted = true;
            it->second.link = true; it->second.dirty = Host(); it->second.next = 0;
            continue;
        }
        if (!a && row.ref.Raw() && !row.dead) {
            if (!row.ready) { g_rows.erase(it); continue; }
            if (Host()) Dead(it->second);
            else if (!row.accepted || row.manualUntil >= now) it->second.remove = true;
            else {
                // Local lifespan/weather simulation is not permission to destroy
                // the host's plant. Ask for its current canonical record instead.
                it->second.ref.Reset(); it->second.local = false; it->second.accepted = false;
            }
            row = it->second;
        }
        if (a && row.link && now >= row.linkAt) {
            if (++work > 4) break;
            it->second.linkAt = now + 1000;
            Applying applying;
            const bool linked = P::Link(a, row.record);
            if (epoch != g_epoch) return;
            it = g_rows.find(key); if (it == g_rows.end()) continue;
            if (linked && row.ref.Get()) it->second.link = false;
        }
        if (Active() && ((Host() && row.dirty) || (!Host() && (row.remove || (row.ready && !row.accepted))))) {
            if (++work > 4) break;
            it = g_rows.find(key); if (it == g_rows.end()) continue;
            it->second.next = now + kRetryMs;
            const Op op = Host() ? (row.dead ? Retired : State) : (row.remove ? Remove : Create);
            if (Host() && !row.dead && a) {
                Applying applying;
                Record raw, normalized;
                if (!P::Ready(a) || !P::Capture(a, raw) || !P::Normalize(raw, normalized) ||
                    normalized.key != key || !row.ref.Get()) continue;
                if (epoch != g_epoch) return;
                row.record = std::move(normalized);
                it = g_rows.find(key); if (it == g_rows.end()) continue;
                it->second.record = row.record;
            }
            const bool sent = Send(op, row.record);
            if (epoch != g_epoch) return;
            it = g_rows.find(key);
            if (Host() && sent && it != g_rows.end()) it->second.dirty = false;
        }
    }
}
void QueueConnectBroadcastForSlot(int) {
    if (!Active() || !Host()) return;
    World();
    // Queued, bounded by Tick's work budget and retried on backpressure. Includes
    // tombstones for actors present in the joiner's older save capture.
    for (auto& [key, row] : g_rows) if (row.accepted) { row.dirty = true; row.next = 0; }
}
void OnPeerLeftSlot(int slot) {
    if (slot < 0 || slot >= players::kMaxPeers) return;
    g_assembler.ClearSlot(uint8_t(slot));
    // Accepted objects belong to the host save, not to the departing connection.
    std::erase_if(g_rows, [slot](const auto& pair) { return pair.second.incoming &&
        Host() && pair.second.owner == slot && !pair.second.accepted; });
}
void MarkChanged(void* actor) {
    if (!Tracking() || !Host() || g_applying || !actor) return;
    World();
    for (auto& [key, row] : g_rows) if (row.ref.Is(actor)) {
        row.dirty = true; row.next = 0; break;
    }
}
void TrackWall(void* actor) {
    if (!Tracking() || !Host() || g_applying || !actor) return;
    World();
    for (const auto& [key, row] : g_rows) if (row.ref.Is(actor)) return;
    ue_wrap::CachedObjRef ref; ref.Set(actor);
    EnrollWall(ref);
}
void OnDisconnect() { wall_build_sync::OnDisconnect(); g_session = nullptr; ResetWorld(); }
}
