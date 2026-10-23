// coop/interactables/atv_index.cpp -- the ATV lane's index: which actor is which wire key. The hub's shared
// scan pass drives the callbacks below and fills the map atv_sync.cpp owns; the runtime-spawned identity
// (synthetic keys, the AtvSpawn / AtvDestroy announce) lives here with them. Declared in
// atv_index_internal.h; atv_sync.cpp keeps the authority, the stream and the connect snapshot.

#include "coop/interactables/atv_index_internal.h"
#include "coop/interactables/atv_sync_internal.h"

#include "coop/element/object_scan_hub.h"      // the shared sliced scan pass
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"  // WireKeyFromString / FnvKey (shared)
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/types.h"           // FVector, FRotator
#include "ue_wrap/devices/atv.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace coop::atv_sync {

namespace R = ue_wrap::reflection;
namespace A = ue_wrap::atv;
using ue_wrap::FVector;
using ue_wrap::FRotator;
using coop::net::WireKeyFromString;
using coop::net::FnvKey;

namespace detail {

// Runtime-spawned ATV identity. Nothing sells an ATV, but list_props row 'atv' spawns an ATV_C
// from the spawn menu; such an ATV has no save twin on the other peers and its own save key is
// minted randomly per peer, so the host gives it a synthetic wire key ("coopatv#N") and announces
// it, and clients fresh-spawn a native AATV_C under that key. A save-placed ATV (a deterministic
// key both peers loaded) keeps its real key.
std::unordered_set<std::wstring>        g_savePlacedKeys;    // host: real keys seen before any client connected, so save-placed
std::unordered_set<void*>               g_savePlacedActors;  // host: the actors present before any client, so a save ATV that mints its key late is not misread as a runtime spawn
std::unordered_map<void*, std::wstring> g_synthForActor;     // actor -> synthetic wire key (host runtime-spawned + client mirror)
uint32_t                                g_synthCounter = 0;  // host: the monotonic synthetic-key id

namespace {
const wchar_t* const kSynthPrefix = L"coopatv#";  // distinguishes synth keys from real ATV keys ("atv"/base64)
}  // namespace

bool IsSynthKey(const std::wstring& k) {
    return k.compare(0, 8, kSynthPrefix) == 0;  // "coopatv#" is 8 chars
}

namespace {
// A WireClassName from a wide class name (ASCII), truncated at 63.
void FillWireClassName(coop::net::WireClassName& out, const std::wstring& name) {
    out.len = 0;
    for (size_t i = 0; i < name.size() && i < sizeof(out.data); ++i)
        out.data[out.len++] = static_cast<char>(name[i]);
}
}  // namespace

// Host: announces a runtime ATV so clients fresh-spawn a native mirror; slot < 0 broadcasts (a
// new ATV mid-session), slot >= 0 seeds one joiner. Reads the class and the current pose.
void SendAtvSpawn(const std::wstring& synthKey, void* actor, int slot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || !actor) return;
    void* cls = R::ClassOf(actor);
    if (!cls) return;
    FVector loc; FRotator rot;
    if (!A::GetRootTransform(actor, loc, rot)) return;
    coop::net::AtvSpawnPayload p{};
    WireKeyFromString(synthKey, p.synthKey);
    FillWireClassName(p.className, R::ToString(R::NameOf(cls)));
    p.x = loc.X; p.y = loc.Y; p.z = loc.Z;
    p.pitch = rot.Pitch; p.yaw = rot.Yaw; p.roll = rot.Roll;
    if (slot < 0) s->SendReliable(coop::net::ReliableKind::AtvSpawn, &p, sizeof(p));
    else          s->SendReliableToSlot(slot, coop::net::ReliableKind::AtvSpawn, &p, sizeof(p));
}

namespace {
void SendAtvDestroy(const std::wstring& synthKey) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return;
    coop::net::AtvDestroyPayload p{};
    WireKeyFromString(synthKey, p.synthKey);
    s->SendReliable(coop::net::ReliableKind::AtvDestroy, &p, sizeof(p));
}
}  // namespace

// The scan-hub consumer: the hub's shared sliced pass drives these callbacks, preserving the
// sender state for keys that persist (only the actor and index update) and classifying identity
// as above (a save-placed ATV keeps its real key; a host-side runtime ATV gets a synthetic key
// and an announce). A spawn landing inside the pass window at a join is announced on the next
// pass, when the joiner is connected.
uint32_t g_indexGen = 0;  // world gen of the last completed pass (stale-gen index = EMPTY)
namespace {
size_t   g_lastLogCount = SIZE_MAX;
uint64_t g_lastLogHash  = 0;
struct ScanFound { std::wstring wireKey; void* obj; int32_t idx; std::wstring realKey; };
std::vector<ScanFound> g_scanFound;   // pass scratch (GT-only)
bool g_scanIsHost = false;            // pass context, captured at pass begin
bool g_scanCapturing = false;

void HubPassBegin(void*, bool) {
    g_scanFound.clear();
    auto* s = g_session.load(std::memory_order_acquire);
    g_scanIsHost = s && s->role() == coop::net::Role::Host;
    const bool isHost = g_scanIsHost;
    // The baseline window: before any client is connected, every keyed ATV the host has is
    // save-placed; after that a newly appearing key is a runtime spawn. Accumulated, not a
    // single-frame latch, so a save ATV slow to mint its key still lands in the set.
    g_scanCapturing = isHost && (!s || !s->connected());
}

void HubMatch(void*, void* obj) {
    const bool isHost = g_scanIsHost;
    const bool capturing = g_scanCapturing;
    auto& found = g_scanFound;
    {
        const std::wstring nm = R::ToString(R::NameOf(obj));
        if (nm.rfind(L"Default__", 0) == 0) return;  // skip CDO
        if (!R::IsLive(obj)) return;
        if (capturing) g_savePlacedActors.insert(obj);  // capture the ACTOR (even before it mints its key)
        std::wstring realKey = A::GetKeyString(obj);
        if (realKey.empty() || realKey == L"None") return;  // not yet keyed -- the next pass picks it up
        std::wstring wireKey;
        auto sf = g_synthForActor.find(obj);
        if (sf != g_synthForActor.end()) {
            wireKey = sf->second;                              // already synth (host runtime-spawn / client mirror)
        } else if (capturing) {
            g_savePlacedKeys.insert(realKey);                 // baseline: a save-placed ATV
            wireKey = realKey;
        } else if (isHost && g_savePlacedKeys.find(realKey) == g_savePlacedKeys.end() &&
                   g_savePlacedActors.find(obj) == g_savePlacedActors.end()) {
            // Host: a mid-session ATV not in the save set is a runtime spawn: a synthetic wire key
            // and an announce, so the clients fresh-spawn a mirror.
            wireKey = std::wstring(kSynthPrefix) + std::to_wstring(++g_synthCounter);
            g_synthForActor[obj] = wireKey;
            SendAtvSpawn(wireKey, obj, /*slot*/ -1);          // broadcast to all connected clients
            UE_LOGI("atv: runtime-spawned ATV detected -- synthKey='%ls' class='%ls' (host-announced AtvSpawn)",
                    wireKey.c_str(), R::ToString(R::NameOf(R::ClassOf(obj))).c_str());
        } else {
            wireKey = realKey;                                 // save-placed default ATV (both peers have it)
        }
        found.push_back({ std::move(wireKey), obj, R::InternalIndexOf(obj), std::move(realKey) });
    }
}

size_t HubPassComplete(void*, bool isFull, uint32_t worldGen) {
    const bool isHost = g_scanIsHost;
    auto& found = g_scanFound;
    // Entries whose ATV vanished are dropped; a gone host-synthetic ATV sends AtvDestroy so the
    // clients tear down their mirror. On a full pass `found` is authoritative; on a tail pass it is
    // only the new tail, so liveness by index prunes instead.
    for (auto it = g_atvs.begin(); it != g_atvs.end();) {
        bool keep;
        if (isFull) {
            keep = false;
            for (auto& f : found) if (f.wireKey == it->first) { keep = true; break; }
        } else {
            keep = R::IsLiveByIndex(it->second.actor, it->second.idx);
        }
        if (keep) { ++it; continue; }
        if (IsSynthKey(it->first)) {
            if (isHost) SendAtvDestroy(it->first);
            if (it->second.actor) g_synthForActor.erase(it->second.actor);
        }
        it = g_atvs.erase(it);
    }
    // Update or add. A wire key can outlive its actor (a save-placed key is deterministic and a
    // client join runs two level loads), and the successor must not inherit the dead actor's
    // authority, seat, correction clock and change-gate baseline: the entry stays, everything that
    // described the actor resets.
    for (auto& f : found) {
        AtvEntry& e = g_atvs[f.wireKey];
        if (e.actor && e.actor != f.obj) {
            e.occupantSlot = 0xFF; e.authorSlot = 0xFF;
            e.wasPoseAuthor = false;
            e.haveLastSync = false; e.lastIdleSendMs = 0; e.lastPktMs = 0;
            e.lastErrCm = -1.f; e.stallPackets = 0; e.restReplaces = 0; e.lastRestPlaceMs = 0;
        }
        e.actor = f.obj;
        e.idx   = f.idx;
    }
    g_indexGen = worldGen;
    // The keys hash over the whole index, since on a tail pass `found` is only the new arrivals.
    uint64_t keysHash = 0;
    for (auto& kv : g_atvs) keysHash ^= FnvKey(kv.first);
    if (g_atvs.size() != g_lastLogCount || keysHash != g_lastLogHash) {
        g_lastLogCount = g_atvs.size();
        g_lastLogHash  = keysHash;
        UE_LOGI("atv: index rebuilt -- %zu live ATV(s), keysHash=0x%016llX (%s pass, +%zu new) "
                "(compare host vs client for cross-peer Key stability)",
                g_atvs.size(), static_cast<unsigned long long>(keysHash),
                isFull ? "full" : "tail", found.size());
    }
    g_scanFound.clear();
    return g_atvs.size();
}
}  // namespace

void RegisterWithScanHub() {
    static bool sDone = false;
    if (sDone) return;
    sDone = true;
    coop::element::scan_hub::Register(coop::element::scan_hub::Consumer{
        "atv", nullptr, &A::EnsureResolved, &A::IsAtv,
        &HubPassBegin, &HubMatch, &HubPassComplete, /*settleScans*/ 15});
}

}  // namespace detail

}  // namespace coop::atv_sync
