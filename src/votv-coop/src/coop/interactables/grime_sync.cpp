// coop/interactables/grime_sync.cpp -- see coop/interactables/grime_sync.h; the surface grime
// dirt sync. A sibling of the window sync (the keyed monotone-min-wins scalar channel) keyed
// by a quantised world-position string instead of a key name: the save gives every decal it
// holds an identical transform on each peer, so its position is its cross-peer identity. The
// one engine difference from the window: the apply repaints through the decal's own
// apply-material verb, not a pure setter. A fall is seen at the verbs that make one, watched on
// the script gate before and after their bodies: grime_C::clean (a sponge stroke, the rain) and
// the wall fixer's repair of a crack (wallFixer_fix), the two verbs that lower `process`. A verb
// that takes it below zero destroys the decal inside its body; the engine's end of play reports
// that destroy with its reason (coop/element/death_seam), so a wipe to destruction and a
// sublevel's stream-out are told apart by what the engine says, never by where the camera
// stands. If a third keyed-float channel appears, generalise the window and grime pair into a
// shared adapter and channel, as the interactable sync did for its bool features.

#include "coop/interactables/grime_sync.h"
#include "coop/props/drink_spill_sync.h"

#include "coop/config/config.h"
#include "coop/element/death_seam.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"  // WireKeyFromString / StringFromWireKey / FnvKey (shared)
#include "coop/player/players_registry.h"  // coop::players::kMaxPeers
#include "coop/player/roster_ledger.h"
#include "ue_wrap/core/cached_obj_ref.h"

#include "ue_wrap/engine/engine.h"          // TryGetActorLocation (the grime's world position -> posKey)
#include "ue_wrap/devices/grime.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/engine/world_identity.h"     // gen-stamped index (dead-world guard)
#include "coop/element/object_scan_hub.h"      // the shared sliced scan pass

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <array>
#include <map>
#include <algorithm>
#include <set>

namespace coop::grime_sync {
namespace {

namespace R = ue_wrap::reflection;
namespace G = ue_wrap::grime;
namespace E = ue_wrap::engine;
namespace sg = ue_wrap::script_gate;
namespace DS = coop::element::death_seam;

using coop::net::WireKeyFromString;
using coop::net::StringFromWireKey;
using coop::net::FnvKey;

// The process changes in wipe steps, and a step can be smaller than this epsilon, so a fall is
// measured against the floor and small ones add up (OnLowerPost); it also skips an idempotent apply.
constexpr float kProcessEps = 0.0005f;
// The position quantisation grid (cm). A decal's saved position is bit-identical across peers
// (the same save, a static actor), so any deterministic quantisation yields the same key on
// both; the grid has to be fine enough that distinct decals rarely share a cell. The rig's save puts
// 1117 decals on 1087 keys: 30 stand in a cell another of their type holds, and the lane treats a
// cell's decals as one -- the floor is kept per key, so a fall on one lowers its twins at the next full
// pass.
constexpr double kPosGrid = 2.0;
bool ProbeLog() {
    static const bool s_enabled = ::coop::config::ResolveFlag(::coop::config_registry::rows::grime_log);
    return s_enabled;
}

std::atomic<coop::net::Session*> g_session{nullptr};

// An indexed decal: the actor, its slot and the serial the slot held when a pass took it (the key cache's
// own), so a new object the allocator put at the address in the same slot is never read as the decal.
struct Ref { void* actor; int32_t idx; int32_t serial; };
bool Holds(const Ref& r) { return R::IsLiveByIndex(r.actor, r.idx) && R::SlotSerial(r.idx) == r.serial; }
std::mutex g_indexMutex;
std::unordered_map<std::wstring, Ref> g_byKey;

// This peer's own name for one decal: its location's float bits and its type. It tells a cell's twins
// apart and never travels, so a value is enough, with no string.
struct ExactKey {
    uint64_t dynamic=0;
    uint32_t x = 0, y = 0, z = 0;
    int32_t type = 0;
    bool operator==(const ExactKey& o) const { return dynamic==o.dynamic && x == o.x && y == o.y && z == o.z && type == o.type; }
};
struct ExactKeyHash {
    size_t operator()(const ExactKey& k) const {
        uint64_t h = 1469598103934665603ull;  // FNV-1a over the four words
        for (uint32_t w : {k.x, k.y, k.z, static_cast<uint32_t>(k.type)}) h = (h ^ w) * 1099511628211ull;
        h=(h^k.dynamic)*1099511628211ull;
        // A word-wise multiply leaves the low bits a function of the words' low bits alone, and a round
        // coordinate's float has them all zero; the table picks its bucket from the low bits. murmur3's
        // 64-bit finalizer spreads every bit into them.
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdull;
        h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53ull;
        h ^= h >> 33;
        return static_cast<size_t>(h);
    }
};

// A register of the running world: another world's is dropped the first time it is touched in this
// one, a value that arrives before the new world's first pass included. A peer leaving does not clear
// it. Game thread only.
template <class K, class V, class H = std::hash<K>>
struct WorldRegister {
    std::unordered_map<K, V, H> map;
    uint32_t gen = 0;
    std::unordered_map<K, V, H>& Get() {
        const uint32_t now = ue_wrap::world_identity::Generation();
        if (now != gen) {
            map.clear();
            gen = now;
        }
        return map;
    }
};

// The floor: per key, the lowest process every peer knows the key's decals at in this world -- a fall
// that was sent, by this peer or another, a destroy (zero, sent as a wipe), or the host's snapshot as
// adopted. It is the lane's shared register: a decal the index takes (a stream-back, one the join
// brought, one that was not here when its value arrived) is lowered to it, every decal of the key's cell
// alike, and the host's snapshot sends it for every key its index does not hold. A value only this peer
// holds enters it only while no peer is connected, a solo host's falls, which the join snapshot sends.
WorldRegister<std::wstring, float> g_floor;
std::unordered_map<std::wstring, float>& Floor() { return g_floor.Get(); }

// Per decal (ExactKey), for a key with no floor yet: the process the decal had when this peer's first
// fall on it began, what every peer holds for a decal nobody has moved in this world, so falls under the
// epsilon add up from it.
WorldRegister<ExactKey, float, ExactKeyHash> g_unmoved;

// Per decal (its exact place), the process it had when it streamed out, with its key: the stream-back
// comes in at the level's value, and the pass that indexes it restores exactly that decal from here. The
// host's snapshot also sends it for a key its index does not hold. Kept out of the floor, which would
// lower the decal's cell-mates to a value only this peer holds.
struct Parked {
    std::wstring key;
    float value;
};
WorldRegister<ExactKey, Parked, ExactKeyHash> g_parked;

size_t g_lastLogCount = SIZE_MAX;  // GT-only: dedup the rebuilt log
uint64_t g_lastLogHash = 0;        // GT-only

// The cross-peer identity of a static grime decal: its quantised world position and type.
// The same save gives the identical saved transform, so the identical key on both peers.
// Fits the wire key for any in-base coordinate. `exact` is this peer's own name for the one decal
// (ExactKey). False when the location read fails: a key is never made from the origin a failed read
// leaves.
bool KeysOf(void* grime, std::wstring& key, ExactKey& exact) {
    if(drink_spill_sync::ExcludeFromStaticIndex(grime))return false;
    ue_wrap::FVector loc{};
    if (!E::TryGetActorLocation(grime, loc)) return false;
    int32_t type = 0; G::ReadType(grime, type);
    auto q = [](float v) -> long { return std::lround(static_cast<double>(v) / kPosGrid); };
    key = L"g_";
    key += std::to_wstring(q(loc.X)); key += L'_';
    key += std::to_wstring(q(loc.Y)); key += L'_';
    key += std::to_wstring(q(loc.Z)); key += L'_';
    key += std::to_wstring(type);
    std::memcpy(&exact.x, &loc.X, sizeof(exact.x));
    std::memcpy(&exact.y, &loc.Y, sizeof(exact.y));
    std::memcpy(&exact.z, &loc.Z, sizeof(exact.z));
    exact.type = type;
    const auto dynamic=drink_spill_sync::KeyFor(grime);
    if(!dynamic.empty()){key=dynamic;exact.dynamic=FnvKey(dynamic);}
    return true;
}

std::wstring PosKey(void* grime) {
    std::wstring key;
    ExactKey exact;
    return KeysOf(grime, key, exact) ? key : std::wstring();
}

// The world generation of the last completed hub pass; a stale-generation index is treated
// as empty on every read path (the dead-world guard).
uint32_t g_indexGen = 0;
bool IndexCurrent() { return g_indexGen == ue_wrap::world_identity::Generation(); }

void* ResolveFast(const std::wstring& key) {
    if (!IndexCurrent()) return nullptr;  // a stale-generation index is another world's actors
    std::lock_guard<std::mutex> lk(g_indexMutex);
    auto it = g_byKey.find(key);
    if (it != g_byKey.end() && Holds(it->second))
        return it->second.actor;
    return nullptr;
}

// Lowers the floor of `key` to `value`; true when it moved.
bool Lower(const std::wstring& key, float value) {
    auto& floor = Floor();
    const auto it = floor.find(key);
    if (it != floor.end() && value >= it->second - kProcessEps) return false;
    floor[key] = value;
    return true;
}

void ClearFalls();
#include "grime_join.inc"
#include "grime_falls.inc"
struct PendingPaint {ue_wrap::CachedObjRef actor;std::wstring key;float target=0;uint64_t sequence=0;};
std::map<void*,PendingPaint> g_paints;
uint32_t g_paintWorld=0;
uint64_t g_paintSequence=0;
void* g_paintCursor=nullptr;
bool g_drainingPaint=false;
void PaintWorld(){const auto world=ue_wrap::world_identity::Generation();if(world!=g_paintWorld){g_paints.clear();g_paintCursor=nullptr;g_paintWorld=world;}}
// Write `target` into a live decal and repaint it, unless it already reads that -- or, `lowerOnly`, unless
// it already reads that or less: a fall lowers a decal and never raises one this peer cleaned further.
// The apply runs no verb, so it is never heard back as a fall.
void ApplyResolved(void* actor, const std::wstring& key, float target, const char* why, bool lowerOnly) {
    PaintWorld();ue_wrap::CachedObjRef ref;ref.Set(actor);
    const auto world=g_paintWorld;const auto epoch=g_joinEpoch;
    auto* session=g_session.load();
    const auto floor=Floor().find(key);const bool haveFloor=floor!=Floor().end();const float floorValue=haveFloor?floor->second:0;
    auto pending=g_paints.find(actor);uint64_t sequence=pending==g_paints.end()?0:pending->second.sequence;
    const auto current=[&]{if(world!=ue_wrap::world_identity::Generation() || epoch!=g_joinEpoch ||
        session!=g_session.load() || !ref.Get())return false;
        const auto floor=Floor().find(key);if((floor!=Floor().end())!=haveFloor || (haveFloor && floor->second!=floorValue))return false;
        const auto row=g_paints.find(actor);return sequence?(row!=g_paints.end() && row->second.sequence==sequence):row==g_paints.end();};
    float cur = 0.f;
    if (!std::isfinite(target) || !G::ReadProcess(actor, cur) || !current())return;
    if(!sequence && std::fabs(target-cur)<kProcessEps)return;
    if (lowerOnly && cur < target) {g_paints.erase(actor);return;}
    sequence=++g_paintSequence;g_paints[actor]={ref,key,target,sequence};
    const bool ok = G::WriteProcessAndApply(actor,target,current);
    if(!current())return;if(ok)g_paints.erase(actor);
    if (ProbeLog())
        UE_LOGI("grime: applied process=%.3f (was %.3f, %s) ok=%d key='%ls'", target, cur, why, ok ? 1 : 0,
                key.c_str());
}
void DrainPaint(){
    if(g_drainingPaint)return;PaintWorld();const auto epoch=g_joinEpoch;g_drainingPaint=true;
    struct Busy{~Busy(){g_drainingPaint=false;}}busy;std::set<void*> visited;
    for(unsigned work=0;work<8 && !g_paints.empty();++work){
        auto it=g_paints.upper_bound(g_paintCursor);if(it==g_paints.end())it=g_paints.begin();
        const auto actor=it->first;const auto pending=it->second;g_paintCursor=actor;if(!visited.insert(actor).second)break;
        if(!pending.actor.Get()){g_paints.erase(actor);continue;}
        const auto floor=Floor().find(pending.key);const float target=floor==Floor().end()?pending.target:floor->second;
        ApplyResolved(actor,pending.key,target,"retry",true);
        if(epoch!=g_joinEpoch || g_paintWorld!=ue_wrap::world_identity::Generation())return;
    }
}

// The shared-scan hub consumer: the per-module walk is gone, and the hub's shared sliced pass
// drives these callbacks. Two settle scans rather than fifteen: grime churns whenever the
// player washes (decal deaths re-arm the demand), so fifteen would pin a cleaning session in
// full-demand cadence; new decals append and the tail passes catch them, and recycled-slot
// arrivals heal within the hub's backstop. The key cache: a decal is static, so its key never
// changes, yet computing it dispatches a location UFunction, and doing that for a thousand
// unchanged decals on every rebuild was a recurring frame hitch; only a new decal computes
// it, and a full pass rebuilds the cache from the live set. An entry carries the slot and its
// engine serial (reflection::AllocateSlotSerial, the weak-pointer rule), so a new decal the
// allocator put at a dead one's address in the same slot never inherits the dead one's key.
struct CachedKey {
    std::wstring key;
    ExactKey     exact;  // KeysOf's own name for the one decal
    int32_t      idx;
    int32_t      serial;
};
std::unordered_map<void*, CachedKey> g_posKeyByActor;   // actor -> cached keys (GT-only)
std::unordered_map<void*, CachedKey> g_scanNextCache;   // pass scratch: full-pass cache rebuild
struct Found {
    std::wstring key;
    ExactKey     exact;
    Ref          ref;
};
std::vector<Found> g_scanFound;  // pass scratch (GT-only)
uint64_t g_scanSequence=0;
uint32_t g_scanWorld=0;
bool g_scanIsFull = false;                                  // pass context

void HubPassBegin(void*, bool isFull) {
    ++g_scanSequence;g_scanWorld=ue_wrap::world_identity::Generation();
    g_scanFound.clear();
    g_scanNextCache.clear();
    g_scanIsFull = isFull;
}

void HubMatch(void*, void* obj) {
    const auto sequence=g_scanSequence;ue_wrap::CachedObjRef identity;identity.Set(obj);
    if (R::NameStartsWith(R::NameOf(obj), L"Default__")) return;  // skip the CDO (alloc-free)
    if (!R::IsLive(obj)) return;
    drink_spill_sync::ObserveExisting(obj);
    if(drink_spill_sync::ExcludeFromStaticIndex(obj))return;
    const int32_t idx = R::InternalIndexOf(obj);
    auto cit = g_posKeyByActor.find(obj);
    CachedKey ck;
    if (cit != g_posKeyByActor.end() && cit->second.idx == idx &&
        R::SlotSerial(idx) == cit->second.serial) {
        ck = cit->second;
    } else {
        if (!KeysOf(obj, ck.key, ck.exact)) return;  // no location read, so no identity to index it under
        ck.idx = idx;
        ck.serial = R::AllocateSlotSerial(idx);
    }
    if(!identity.Get() || g_scanWorld!=ue_wrap::world_identity::Generation() || sequence!=g_scanSequence)return;
    if (g_scanIsFull) g_scanNextCache.insert_or_assign(obj, ck);   // full pass rebuilds the cache
    else              g_posKeyByActor.insert_or_assign(obj, ck);   // tail pass: cache the NEW actor's key
    const int32_t serial = ck.serial;
    g_scanFound.push_back(Found{std::move(ck.key), ck.exact, Ref{obj, idx, serial}});
}

size_t HubPassComplete(void*, bool isFull, uint32_t worldGen) {
    const auto sequence=g_scanSequence,epoch=g_joinEpoch;
    const auto current=[&]{return sequence==g_scanSequence && epoch==g_joinEpoch && worldGen==ue_wrap::world_identity::Generation() && g_scanWorld==worldGen;};
    if(!current())return 0;
    const auto found=g_scanFound;
    if(isFull)drink_spill_sync::IndexComplete(worldGen);
    const size_t added = found.size();
    if (isFull) g_posKeyByActor.swap(g_scanNextCache);  // keep live actors' cached keys, drop dead ones
    uint64_t posHash = 0;
    size_t   total;
    {
        std::lock_guard<std::mutex> lk(g_indexMutex);
        if (isFull) g_byKey.clear();                           // full pass: rebuild from scratch
        for (const auto& f : found) if(Holds(f.ref))g_byKey[f.key] = f.ref;
        if (!isFull) {                                         // tail pass: prune dead entries (cheap, O(index))
            for (auto it = g_byKey.begin(); it != g_byKey.end(); ) {
                if (Holds(it->second)) ++it;
                else { g_posKeyByActor.erase(it->second.actor); it = g_byKey.erase(it); }
            }
        }
        for (auto& kv : g_byKey) posHash ^= FnvKey(kv.first);  // recompute over the index (cheap, O(index))
        total = g_byKey.size();
        g_indexGen = worldGen;
    }
    // Every decal this pass found is lowered to its floor: one that streamed back in, one a join brought
    // after its value arrived, one this peer had not indexed when a peer's fall came. A pass spans frames,
    // so a decal matched early may have ended since: only one still live is read.
    size_t lowered = 0;
    auto& floor = Floor();
    if (!floor.empty()) {
        for (const auto& f : found) {
            if (!Holds(f.ref)) continue;
            const auto it = floor.find(f.key);
            if (it == floor.end()) continue;
            float cur = 0.f;
            const bool read=G::ReadProcess(f.ref.actor,cur);if(!current())return total;
            if(!read || !Holds(f.ref))continue;
            const auto latest=Floor().find(f.key);if(latest==Floor().end() || cur<=latest->second+kProcessEps)continue;
            ApplyResolved(f.ref.actor,f.key,latest->second,"floor at index",true);
            if(!current())return total;++lowered;
        }
    }
    // A decal that streamed back in takes the process it streamed out at, this decal alone.
    auto& parked = g_parked.Get();
    if (!parked.empty()) {
        for (const auto& f : found) {
            const auto it = parked.find(f.exact);
            if (it == parked.end() || !Holds(f.ref)) continue;
            const auto value=it->second.value;const auto key=it->second.key;
            ApplyResolved(f.ref.actor,key,value,"its stream-out",true);if(!current())return total;
            const auto latest=parked.find(f.exact);if(latest!=parked.end() && latest->second.key==key && latest->second.value==value)parked.erase(latest);
            ++lowered;
        }
    }
    if (total != g_lastLogCount || posHash != g_lastLogHash || lowered > 0) {
        g_lastLogCount = total;
        g_lastLogHash = posHash;
        UE_LOGI("grime: index rebuilt -- %zu live grime decal(s), posHash=0x%016llX (%s pass, +%zu new, %zu lowered "
                "to their floor) (compare host vs client for cross-peer position stability)",
                total, static_cast<unsigned long long>(posHash), isFull ? "full" : "tail", added, lowered);
    }
    if (ProbeLog())
        for (auto& f : g_scanFound)
            UE_LOGI("grime[probe]: key='%ls' idx=%d actor=%p", f.key.c_str(), f.ref.idx, f.ref.actor);
    g_scanFound.clear();
    g_scanNextCache.clear();
    return total;
}

void RegisterWithScanHub() {
    static bool sDone = false;
    if (sDone) return;
    sDone = true;
    coop::element::scan_hub::Register(coop::element::scan_hub::Consumer{
        "grime", nullptr, &G::EnsureResolved, &G::IsGrime,
        &HubPassBegin, &HubMatch, &HubPassComplete, /*settleScans*/ 2});
}

UE_FACT_FN(kRowClean, "grime_sync", nullptr, L"clean", AnyClass);
UE_FACT_FN(kRowWallFixerFix, "grime_sync", nullptr, L"wallFixer_fix", AnyClass);
constexpr int kTagGrimeClean = 0x4752494D;  // 'GRIM'
constexpr int kTagGrimeFix   = 0x47524958;  // 'GRIX'
bool g_cleanWatched = false;
bool g_fixWatched = false;
bool g_endSubscribed = false;

struct LowerCapture {
    ue_wrap::CachedObjRef actor;
    net::Session* session=nullptr;
    uint64_t invocation=0,epoch=0;
    uint32_t world=0;
    net::Role role=net::Role::Host;
    bool connected=false;
    float before=0;
    bool Current()const{return actor.Get() && world==ue_wrap::world_identity::Generation() && epoch==g_joinEpoch &&
        session==g_session.load() && (!session || (session->role()==role && session->connected()==connected)) &&
        sg::IsInvocationActive(invocation);}
};
std::vector<LowerCapture> g_lowerCaptures;
void PruneLowerCaptures(){std::erase_if(g_lowerCaptures,[](const LowerCapture& row){return !row.Current();});}

// The decal's cross-peer key, from the index's cache: only for a decal the current world's index holds
// under the slot and serial it still carries. A decal no pass has reached yet has none, and its fall is
// not sent; the floor of the peer it reaches next is the other side of that.
const CachedKey* CachedOf(void* actor, int32_t idx, int32_t serial) {
    if (!IndexCurrent()) return nullptr;
    const auto it = g_posKeyByActor.find(actor);
    if (it == g_posKeyByActor.end()) return nullptr;
    const CachedKey& ck = it->second;
    if (ck.idx != idx || ck.serial == 0 || ck.serial != serial) return nullptr;
    return &ck;
}

std::wstring KeyOf(void* actor, int32_t idx, int32_t serial) {
    const CachedKey* ck = CachedOf(actor, idx, serial);
    return ck ? ck->key : std::wstring();
}

void Send(const std::wstring& key, float value) {
    QueueFall(key,value);
}

sg::Verdict OnLowerPre(const sg::Call& call) {
    PruneLowerCaptures();
    if(!call.invocation || !call.object || g_lowerCaptures.size()>=128)return sg::Verdict::Run;
    LowerCapture row{};row.actor.Set(call.object);row.invocation=call.invocation;
    row.world=ue_wrap::world_identity::Generation();row.epoch=g_joinEpoch;row.session=g_session.load();
    if(row.session){row.role=row.session->role();row.connected=row.session->connected();}
    if(row.Current() && G::IsGrime(call.object) && row.Current() && G::ReadProcess(call.object,row.before) &&
       row.Current() && std::isfinite(row.before))g_lowerCaptures.push_back(row);
    return sg::Verdict::Run;
}

// After the body: the process it left, sent when it fell. A verb that took it below zero destroyed the
// decal inside the body; its end of play sends the wipe instead (OnGrimeEnd).
void OnLowerPost(const sg::Call& call) {
    const auto captured=std::find_if(g_lowerCaptures.rbegin(),g_lowerCaptures.rend(),[&](const LowerCapture& row){return row.invocation==call.invocation && row.actor.Raw()==call.object;});
    if(captured==g_lowerCaptures.rend())return;
    const auto capture=*captured;g_lowerCaptures.erase(std::next(captured).base());
    if(!capture.Current())return;
    const int32_t idx = capture.actor.Idx();
    float cur = 0.f;
    if (!G::ReadProcess(call.object, cur) || !capture.Current() || !std::isfinite(cur) || cur >= capture.before) return;
    const CachedKey* ck = CachedOf(call.object, idx, R::SlotSerial(idx));
    if (!ck || !capture.Current()) return;
    const auto exact=ck->exact;
    const std::wstring key = ck->key;
    // Measured against the floor, the lowest value every peer knows for the key, or, before the key has
    // one, the process this decal had when this peer's first fall on it began: falls smaller than the
    // epsilon add up and go out once they pass it.
    auto& floor = Floor();
    const auto it = floor.find(key);
    if(it!=floor.end() && cur>it->second+kProcessEps){
        // A newer minimum can land inside native material/clean callbacks. The
        // older body must not leave its later, higher scalar on the actor.
        ApplyResolved(call.object,key,it->second,"newer minimum during clean",true);return;
    }
    auto& unmoved = g_unmoved.Get();
    const float from = it != floor.end() ? it->second : unmoved.emplace(exact, capture.before).first->second;
    if (cur >= from - kProcessEps) return;
    unmoved.erase(exact);
    Lower(key, cur);
    Send(key, cur);
}

// A decal's end of play. A stream-out leaves the index and parks its process under its exact place,
// for its stream-back; a destroy -- a clean or a repair past zero, leaves raked or expired, fuel burnt
// out -- ends the decal on this peer, sent as a wipe to zero and kept as the floor for a joiner's
// snapshot.
void OnGrimeEnd(const DS::ActorEnd& end) {
    if(!end.streamedOut)drink_spill_sync::OnRemoved(end.actor);
    const CachedKey* ck = CachedOf(end.actor, end.actorIndex, end.actorSerial);
    if (!ck) return;
    const std::wstring key = ck->key;
    const ExactKey exact = ck->exact;
    // The actor ended before this drain; its memory is read only while its slot still holds it under the
    // serial it ended with, which a purge in between resets.
    float at = 0.f;
    const bool held = R::ObjectAt(end.actorIndex) == end.actor && R::SlotSerial(end.actorIndex) == end.actorSerial;
    const bool read = end.streamedOut && held && G::ReadEndedProcess(end.actor,end.actorIndex,end.actorSerial,at);
    {
        std::lock_guard<std::mutex> lk(g_indexMutex);
        const auto it = g_byKey.find(key);
        if (it != g_byKey.end() && it->second.actor == end.actor) g_byKey.erase(it);
    }
    g_posKeyByActor.erase(end.actor);
    if (end.streamedOut) {
        if (read) {g_parked.Get().insert_or_assign(exact, Parked{key, at});JoinAway(key,at);}
        return;
    }
    Lower(key, 0.f);
    Send(key, 0.f);
}

}  // namespace

void Install(coop::net::Session* session) {
    if(g_session.load()!=session || g_joinWorld!=ue_wrap::world_identity::Generation())ResetJoins();
    g_session.store(session, std::memory_order_release);
    RegisterWithScanHub();  // the hub builds the index on its own cadence
}

void RegisterDynamic(void* actor){
    std::wstring key;ExactKey exact;if(!KeysOf(actor,key,exact))return;
    const int32_t idx=R::InternalIndexOf(actor),serial=R::AllocateSlotSerial(idx);
    const auto prior=g_posKeyByActor.find(actor);
    {std::lock_guard<std::mutex> lock(g_indexMutex);
        if(prior!=g_posKeyByActor.end() && prior->second.key!=key){const auto old=g_byKey.find(prior->second.key);
            if(old!=g_byKey.end() && old->second.actor==actor)g_byKey.erase(old);}
        if(IndexCurrent())g_byKey[key]={actor,idx,serial};}
    g_posKeyByActor[actor]={key,exact,idx,serial};
    const auto floor=Floor().find(key);if(floor!=Floor().end())ApplyResolved(actor,key,floor->second,"dynamic birth floor",true);
}

void ResetDynamic(){
    ++g_scanSequence;
    const auto dynamic=[](const std::wstring& key){return key.starts_with(L"ds_");};
    std::erase_if(Floor(),[&](const auto& item){return dynamic(item.first);});
    std::erase_if(g_parked.Get(),[](const auto& item){return item.first.dynamic!=0;});
    std::erase_if(g_unmoved.Get(),[](const auto& item){return item.first.dynamic!=0;});
    std::erase_if(g_posKeyByActor,[&](const auto& item){return dynamic(item.second.key);});
    std::erase_if(g_scanNextCache,[&](const auto& item){return dynamic(item.second.key);});
    std::erase_if(g_scanFound,[&](const auto& item){return dynamic(item.key);});
    {std::lock_guard<std::mutex> lock(g_indexMutex);std::erase_if(g_byKey,[&](const auto& item){return dynamic(item.first);});}
    std::erase_if(g_paints,[&](const auto& item){return dynamic(item.second.key);});
    std::erase_if(g_falls,[&](const auto& item){return dynamic(item.first);});
    for(auto& join:g_joins)std::erase_if(join.keys,[&](const auto& item){return dynamic(item.first);});
}

void OnReliable(const coop::net::KeyedScalarPayload& payload, uint8_t senderPeerSlot) {
    std::wstring key = StringFromWireKey(payload.key);
    if (key.empty()) { UE_LOGW("grime: OnReliable empty key -- dropping"); return; }
    // The trust boundary (the same as the window's): adopt (as is, able to re-dirty) is honoured
    // only from the host; a client edge is forced to a min-wins live fall.
    const bool adopt = (payload.adopt != 0) && (senderPeerSlot == 0);
    // The floor takes the value whether or not the decal is here to show it. A peer's fall is written only
    // to a decal reading above it, and a pass lowers a decal to its floor and never raises one, so the host's
    // snapshot raises only a decal this peer indexes when the value arrives.
    auto& floor = Floor();
    if (adopt) floor[key] = payload.value;
    else if (!Lower(key, payload.value)) return;  // not below what this peer already holds
    if (void* actor = ResolveFast(key))
        ApplyResolved(actor, key, floor[key], adopt ? "adopted" : "a peer's fall", /*lowerOnly=*/!adopt);
}

void QueueConnectBroadcastForSlot(int slot) {
    auto* session=g_session.load();
    if(!session || !session->connected() || session->role()!=net::Role::Host || slot<1 || slot>=net::kMaxPeers)return;
    if(g_joinWorld!=ue_wrap::world_identity::Generation())ResetJoins();
    const auto person=roster_ledger::Get(slot).playerNo;if(!person)return;
    auto& join=g_joins[slot];join={};join.person=person;join.sequence=++g_joinSequence;join.waiting=true;
}
void Tick() {
    PruneLowerCaptures();
    DrainJoins();
    DrainFalls();
    if (!G::EnsureResolved()) return;
    DrainPaint();
    RegisterWithScanHub();  // safety net for any order where Tick precedes Install
    // The two lowering verbs' watches and the decals' end of play, each asked again until it takes. A
    // name watch matches the verb on any class; the pre half keeps only a grime decal.
    if (!g_cleanWatched) g_cleanWatched = sg::WatchName(kRowClean, kTagGrimeClean, &OnLowerPre, &OnLowerPost);
    if (!g_fixWatched) g_fixWatched = sg::WatchName(kRowWallFixerFix, kTagGrimeFix, &OnLowerPre, &OnLowerPost);
    if (!g_endSubscribed && DS::Install()) g_endSubscribed = DS::SubscribeClass(L"grime_C", &OnGrimeEnd);
    sg::ResolvePendingNames();
}

std::wstring DebugPosKeyForActor(void* actor) {
    // The dev drill's parity probe: the real quantiser, cache-independent.
    return actor ? PosKey(actor) : std::wstring();
}
void OnPeerLeftSlot(int slot){if(slot>=0 && slot<net::kMaxPeers)g_joins[slot]={};}
void OnDisconnect(){g_session.store(nullptr);ResetJoins();g_lowerCaptures.clear();}

}  // namespace coop::grime_sync
