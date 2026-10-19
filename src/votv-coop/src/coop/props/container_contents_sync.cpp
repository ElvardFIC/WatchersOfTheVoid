// coop/props/container_contents_sync.cpp -- world-container contents over the wire: the
// addObject/takeObj verb edge marks a container dirty, a 250 ms sweep ships its GObjStack slice as
// a blob, the receiver writes the slice back and re-derives the volume and names through the
// engine's own verbs; a client's edit is a compare-and-swap the host arbitrates and relays. See
// coop/props/container_contents_sync.h.

#include "coop/props/prop_save_data.h"
#include "coop/props/container_contents_sync.h"
#include "coop/props/container_birth.h"
#include "coop/props/container_park.h"
#include "coop/props/container_slice_wire.h"
#include "coop/props/container_write_policy.h"

#include "coop/element/registry.h"
#include "coop/items/save_record_wire.h"
#include "coop/net/blob_chunks.h"
#include "coop/net/session.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/actors/inventory.h"     // ResolveSaveSlot
#include "ue_wrap/actors/container_view.h"
#include "ue_wrap/actors/prop.h"          // WalksToBase
#include "ue_wrap/actors/save_record.h"
#include "ue_wrap/core/component_calls.h"  // CallParamless
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"  // ClassByName -- the host's O(1) class resolve for a client's contents
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/facts/function_row.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace coop::props::container_contents_sync {
namespace {

namespace R  = ue_wrap::reflection;
namespace SR = ue_wrap::save_record;
namespace W  = coop::save_record_wire;
namespace sg = ue_wrap::script_gate;
namespace wp = coop::props::container_write_policy;
namespace pk = coop::props::container_park;
namespace cw = coop::props::container_slice_wire;
namespace ci = ue_wrap::container_inventory;
namespace cb = coop::props::container_birth;

using coop::element::LivePropActor;

// Verb ids: addObject marks dirty; takeObj also arms the container-extraction window read by
// prop_drop_intent (a client-extracted item's actor is admitted as a host-authoritative drop intent).
constexpr int kVerbDirty   = 1;   // addObject
constexpr int kVerbTakeObj = 2;   // takeObj

UE_FACT_FN(kRowAddObject, "container_contents_sync", nullptr, L"addObject", AnyClass);
UE_FACT_FN(kRowTakeObj,   "container_contents_sync", nullptr, L"takeObj",   AnyClass);

// The sweep drains an edge-driven set, not a poll; 250 ms coalesces a burst (a loot roll fires
// addObject four times) into one broadcast.
constexpr uint64_t kSweepMs = 250;

// A slice that does not fit is retried every sweep. The warning is a static timestamp, several
// seconds apart: the same container would otherwise log on every sweep for the rest of the session.
constexpr uint64_t kUnsentWarnMs = 5000;

std::atomic<coop::net::Session*> g_session{nullptr};

bool g_verbsRegistered = false;
bool g_verbEntered = false;   // the watch has fired at least once on this peer
bool g_announced = false;
uint64_t g_nextSweep = 0;
uint32_t g_nextSeq = 1;

coop::blob_chunks::Assembler g_asm;

// Containers marked dirty by the verb edge, drained on the next sweep, keyed by element id rather
// than the component pointer: IsLive cannot detect an address recycled by a new object between the
// edge and the drain, and an eid re-resolves forward, so a destroyed container stops resolving.
std::set<uint32_t> g_dirty;

// The hash of the last blob sent and of the last applied, per eid. Skipping an unchanged blob is
// what bounds the orphaned-buffer cost: a steady-state re-broadcast applies and allocates nothing.
std::map<uint32_t, uint64_t> g_sentHash;
std::map<uint32_t, uint64_t> g_appliedHash;

// Client: the content this peer believes the host now publishes for an eid -- host truth it
// applied, or its own slice once the transport took it, since an accepted client write becomes
// exactly what the host publishes next. Not the same map as g_appliedHash, although both are
// written at the same moment: a local mutation clears g_appliedHash (or a corrective re-publish is
// skipped as a duplicate and the peer never converges) and keeps g_baseHash (that is the edit's
// base; cleared, the peer declares base 0 and the host refuses every write). Fusing them produced
// both failures in turn.
std::map<uint32_t, uint64_t> g_baseHash;

// Containers whose broadcast the transport refused; retried by the sweep.
std::set<uint32_t> g_retry;

// The takeObj extraction window. Never consumed: a container's hand-view actor may be the first
// FinishSpawn after takeObj and must not steal the admission from the actual extracted world item.
// The drain rechecks the hand axis and filters that view. The time bound also covers UI paths that
// spawn just after takeObj returns.
std::atomic<uint64_t> g_takeObjArmMs{0};
constexpr uint64_t kTakeObjWindowMs = 250;
constexpr size_t kTakeObjCallDepthCap = 64;
thread_local bool g_takeObjCallStack[kTakeObjCallDepthCap]{};
thread_local size_t g_takeObjCallDepth = 0;
thread_local size_t g_takeObjOverflowDepth = 0;
thread_local size_t g_activeTakeObjCalls = 0;

uint64_t NowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

bool IsHost() {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->role() == coop::net::Role::Host;
}

// Reflected offsets, resolved once and cached.

int32_t g_offInvIndex  = -2;  // propInventory_C.Index
int32_t g_offInvPlayer = -2;  // propInventory_C.Player  -- the world-vs-PERSONAL discriminator
int32_t g_offInvOwner  = -2;  // propInventory_C.Owner   -- the Aprop_container_C
int32_t g_offGObjStack = -2;  // saveSlot_C.GObjStack
int32_t g_offPropInv   = -2;  // prop_container_C.propInventory

// An offset resolved once; -1 means looked and failed, never retried, never guessed.
int32_t CachedOffset(int32_t& slot, void* cls, const wchar_t* name) {
    if (slot == -2) {
        slot = cls ? R::FindPropertyOffset(cls, name) : -1;
        if (slot < 0)
            UE_LOGW("container_contents: could not resolve %ls -- lane inert for it", name);
    }
    return slot;
}

template <class T> T ReadAt(const void* base, int32_t off) {
    T v{};
    std::memcpy(&v, reinterpret_cast<const uint8_t*>(base) + off, sizeof(T));
    return v;
}

// The propInventory component of a container actor, or null.
void* InventoryOf(void* containerActor) {
    if (!containerActor) return nullptr;
    if (CachedOffset(g_offPropInv, R::ClassOf(containerActor), L"propInventory") < 0) return nullptr;
    void* inv = ReadAt<void*>(containerActor, g_offPropInv);
    return (inv && R::IsLive(inv)) ? inv : nullptr;
}

// The owning Aprop_container_C of a propInventory component, or null.
void* OwnerOf(void* inv) {
    if (!inv) return nullptr;
    if (CachedOffset(g_offInvOwner, R::ClassOf(inv), L"Owner") < 0) return nullptr;
    void* owner = ReadAt<void*>(inv, g_offInvOwner);
    return (owner && R::IsLive(owner)) ? owner : nullptr;
}

// Boundary 1, fail closed: true only for a world container this lane may author. Player true is a
// personal inventory (mainPlayer and ui_playerInventory share the same global GObjStack, and
// GObjStack[0] is the local player's inventory by construction, baked by the player container's
// component template), and authoring it from the host would wipe that peer's inventory; an
// unresolvable offset refuses too. The same flag is the address assertion in
// ue_wrap::inventory::ReadLivePersonalStore, fail-closed in the other direction. GObjStackSlot and
// that reader each resolve GObjStack and Index themselves; folding them is a refactor of a shipped
// lane for its own arc.
bool IsWorldContainerInventory(void* inv) {
    if (!inv) return false;
    if (CachedOffset(g_offInvPlayer, R::ClassOf(inv), L"Player") < 0) return false;
    return ReadAt<uint8_t>(inv, g_offInvPlayer) == 0;
}

// The live TArray<Fstruct_save> slot for this component's contents inside the global GObjStack;
// null if the save slot, the offsets or the index do not resolve. The contents array is the
// struct_mObject element's single field, at +0.
uint8_t* GObjStackSlot(void* inv) {
    if (!inv) return nullptr;
    void* save = ue_wrap::inventory::ResolveSaveSlot();
    if (!save) return nullptr;
    if (CachedOffset(g_offGObjStack, R::ClassOf(save), L"GObjStack") < 0) return nullptr;
    if (CachedOffset(g_offInvIndex, R::ClassOf(inv), L"Index") < 0) return nullptr;
    const int32_t idx = ReadAt<int32_t>(inv, g_offInvIndex);
    if (idx < 0) return nullptr;  // -1 = never initialised; nothing to ship or apply
    const SR::Arr stack = SR::ReadArr(save, g_offGObjStack);
    if (idx >= stack.num) return nullptr;
    return const_cast<uint8_t*>(stack.data) + static_cast<size_t>(idx) * SR::kMxStride;
}

// The container base and the inventory component's class, looked up per use, one index lookup each: a
// class not loaded yet, or still loading, answers null and is asked for again. Game thread, as every
// caller is.
void* ContainerClass() { return ue_wrap::object_index::ClassByName(L"prop_container_C"); }
void* InventoryClass() { return ue_wrap::object_index::ClassByName(L"propInventory_C"); }

// A record whose class is a container: a class not loaded yet is not one now, and is asked for again at
// the next record.
bool RecordIsNestedContainer(const SR::SaveRecord& r) {
    if (r.className.empty()) return false;
    void* const base = ContainerClass();
    if (!base) return false;
    void* const cls = ue_wrap::object_index::ClassByName(r.className.c_str());
    return cls && ue_wrap::prop::WalksToBase(cls, base);
}

// Is this a propInventory_C component, and is that a container actor: the verb filter matches on
// the verb name alone, so any class with an addObject would arrive, and the apply side must not
// read a cached component offset off an eid that resolved to something else.
bool IsInventoryComponent(void* obj) {
    void* base = InventoryClass();
    return base && obj && ue_wrap::prop::WalksToBase(R::ClassOf(obj), base);
}
bool IsContainerActor(void* actor) {
    void* base = ContainerClass();
    return base && actor && ue_wrap::prop::WalksToBase(R::ClassOf(actor), base);
}

// Boundary 2: a nested container's ints[0][0] is its own GObjStack index, a slot number in the
// sender's array that must not survive the wire. Clearing ints[] is wrong: prop_container::loadData
// reads ints[0][0] unguarded, Array_Get zero-fills an out-of-range read, so an empty array yields
// index 0, which propInventory::init's `index >= 0` guard passes, and the container reuses
// GObjStack[0], a slot owned by someone else. The sentinel -1 is what the guard is written against
// (the CDO's default); every other entry is preserved.
void NeuterNestedIndex(SR::SaveRecord& r) {
    if (r.ints.empty()) r.ints.resize(1);
    if (r.ints[0].empty()) r.ints[0].resize(1);
    r.ints[0][0] = -1;
}

// Does this record still carry a slot number from wherever it came from?
bool CarriesForeignIndex(const SR::SaveRecord& r) {
    return !r.ints.empty() && !r.ints[0].empty() && r.ints[0][0] != -1;
}

// Ready: `out` holds the live slice (possibly empty -- an emptied container is still a slice).
// Unreadable: the slot, the offsets or the index do not resolve. Nothing to send.
// OverCap: the slice exists and the record cap refuses a truncated ship. Not on the wire.
enum class SliceRead { Ready, Unreadable, OverCap };

SliceRead ReadContents(void* inv, std::vector<SR::SaveRecord>& out) {
    uint8_t* slot = GObjStackSlot(inv);
    if (!slot) return SliceRead::Unreadable;
    const SR::Arr objs = SR::ReadArr(slot, 0);  // struct_mObject.obj @ +0
    if (static_cast<size_t>(objs.num) > cw::kMaxRecords) {
        // Retried every sweep from DrainDirty; the line is the permanent-failure case.
        static uint64_t s_nextMs = 0;
        const uint64_t now = NowMs();
        if (now >= s_nextMs) {
            s_nextMs = now + kUnsentWarnMs;
            UE_LOGW("container_contents: %d records exceeds the %zu cap -- refusing to ship a "
                    "truncated slice (not sent; retried)", objs.num, cw::kMaxRecords);
        }
        return SliceRead::OverCap;
    }
    out.clear();
    out.reserve(static_cast<size_t>(objs.num));
    for (int32_t i = 0; i < objs.num; ++i) {
        SR::SaveRecord r;
        SR::ReadSaveRecord(objs.data + static_cast<size_t>(i) * SR::kSaveStride, r);
        if (RecordIsNestedContainer(r)) NeuterNestedIndex(r);
        out.push_back(std::move(r));
    }
    return SliceRead::Ready;
}

// Broadcast one container.

// False if the slice was not put on the wire (the caller arms the retry): the transport refused,
// the record cap refused, or the blob is over the transport ceiling. True when there is nothing
// to send (unreadable inventory) or the fan-out hash is unchanged. Run by both peers: on the host
// toSlot < 0 fans out; on a client the same call reaches the host alone, the author-to-arbiter edge.
// RederiveManagedState is defined below.
void RederiveManagedState(void* owner, void* inv);

bool BroadcastContainer(coop::net::Session* s, uint32_t eid, void* inv, int toSlot, bool force) {
    std::vector<SR::SaveRecord> recs;
    const SliceRead read = ReadContents(inv, recs);
    // Nothing resolvable -- not a transport failure, and not a slice waiting to go out.
    if (read == SliceRead::Unreadable) return true;
    // The records exist and were not shipped. False so the caller retries; the next call reads
    // the live inventory again. This blob is not kept, and g_sentHash is not written.
    if (read == SliceRead::OverCap) return false;
    // The base being edited from: for a client the last host truth it applied; the host authors
    // from its own state and sends 0.
    uint64_t baseHash = 0;
    const bool birth = !IsHost() && toSlot < 0 && cb::IsBirthSlice(eid);
    if (birth) {
        baseHash = cw::kBirthBase;
    } else if (!IsHost()) {
        auto it = g_baseHash.find(eid);
        if (it != g_baseHash.end()) baseHash = it->second;
    }
    const std::vector<uint8_t> blob = cw::Pack(eid, baseHash, recs);
    const uint64_t h = cw::ContentHash(eid, recs);
    if (!force && !birth) {
        auto it = g_sentHash.find(eid);
        if (it != g_sentHash.end() && it->second == h) return true;  // unchanged -- say nothing
    }
    if (blob.size() > coop::blob_chunks::MaxBlobBytes()) {
        // Same latch as the record cap: a container that never fits is retried every sweep, and
        // the bytes just packed are thrown away so the next sweep re-reads. g_sentHash stays at
        // the last slice that actually went out.
        static uint64_t s_nextMs = 0;
        const uint64_t now = NowMs();
        if (now >= s_nextMs) {
            s_nextMs = now + kUnsentWarnMs;
            UE_LOGW("container_contents: eid=%u blob %zu B exceeds the transport ceiling -- not sent "
                    "(retried; contents stay diverged until the live slice fits; there is no bulk path)",
                    eid, blob.size());
        }
        return false;
    }
    const bool ok = (toSlot < 0)
        ? coop::blob_chunks::SendBlob(s, coop::net::ReliableKind::ContainerContents,
                                      g_nextSeq++, blob)
        : coop::blob_chunks::SendBlobToSlot(s, toSlot, coop::net::ReliableKind::ContainerContents,
                                            g_nextSeq++, blob);
    if (ok) {
        if (toSlot < 0) cb::FirstSent(eid);
        if (toSlot < 0) g_sentHash[eid] = h;  // only a FAN-OUT establishes what every peer has
        // A client's own accepted slice IS the host's next published truth, and the author is
        // deliberately excluded from the relay that carries it -- so it advances its base here.
        // Optimistic and self-correcting: a refusal is answered by the host re-publishing its
        // truth to this peer, which lands in this same map.
        if (!IsHost()) g_baseHash[eid] = h;
        // Any publication, fan-out or a targeted seed, establishes what the receiver was told, the
        // baseline a later client write is judged against. With the two maps fused the host refused
        // every client write after a join: the seed is targeted, g_sentHash stayed empty, and the
        // CAS compared against 0 for every container in the world.
        if (IsHost()) wp::NotePublished(eid, h);
        // The author of a mutation re-derives its own volume, mass and names here: the host
        // excludes the author from the relay, and the native take path does not call
        // updateVolumesAndMass, so the mutator's own displayed volume went stale while every other
        // peer converged. A targeted send -- a joiner's seed, the truth re-sent to a refused
        // author -- changed nothing on this peer, so it re-derives nothing; the seed sends every
        // container in the world in one frame.
        if (toSlot < 0) ci::RederiveShownState(ci::OwnerOf(inv), inv);
        UE_LOGI("container_contents: eid=%u shipped %zu records (%zu B)%s%s%s",
                eid, recs.size(), blob.size(),
                toSlot < 0 ? "" : " [targeted]",
                IsHost() ? "" : " [client-authored]",
                birth ? " [birth]" : "");
    }
    return ok;
}

// Host: an accepted client-authored slice on to every other peer, never back to the author:
// echoing a peer's own state reverts a newer local value and primes the baseline over it,
// silently eating that player's next action.
void RelayToOthers(coop::net::Session* s, uint8_t authorSlot, const std::vector<uint8_t>& blob) {
    if (!s || !IsHost()) return;
    size_t sent = 0;
    for (int slot = 1; slot < coop::net::kMaxPeers; ++slot) {
        if (slot == static_cast<int>(authorSlot)) continue;   // NEVER back to the author
        if (!s->IsSlotConnected(slot)) continue;
        if (coop::blob_chunks::SendBlobToSlot(s, slot, coop::net::ReliableKind::ContainerContents,
                                              g_nextSeq++, blob)) {
            ++sent;
        }
    }
    if (sent) {
        UE_LOGI("container_contents: relayed slot-%u authored slice to %zu other peer(s)",
                static_cast<unsigned>(authorSlot), sent);
    }
}

// The dirty drain.

void DrainDirty(coop::net::Session* s) {
    // Transport-refusal retries fold into the same set: a refused send re-enters the next sweep
    // rather than re-running the read twice in one tick.
    if (!g_retry.empty()) {
        g_dirty.insert(g_retry.begin(), g_retry.end());
        g_retry.clear();
    }
    if (g_dirty.empty()) return;

    std::set<uint32_t> dirty;
    dirty.swap(g_dirty);

    for (uint32_t eid : dirty) {
        // Resolved forward from the eid every sweep: a container destroyed since the edge stops
        // resolving.
        void* actor = LivePropActor(eid);
        if (!actor) {   // gone: nothing is owed for it any more
            cb::Forget(eid);
            continue;
        }
        if (!ci::IsContainer(actor)) continue;
        void* inv = ci::InventoryOf(actor);
        if (!inv || !ci::IsWorldInventory(inv)) {   // BOUNDARY 1 (fail-closed)
            // A first publication owed for it waits for the component instead of ending here, and
            // is given up, said, at its deadline.
            const cb::Owed owed = cb::FirstOwed(eid, NowMs());
            if (owed == cb::Owed::None) continue;
            if (owed == cb::Owed::Pending) { g_retry.insert(eid); continue; }
            UE_LOGW("container_contents: eid=%u -- its inventory never resolved; the first publication "
                    "is given up", eid);
            continue;
        }
        // A transfer in progress is not published: what the host added to it so far merges into the
        // author's slice when it lands, and is published with it.
        if (IsHost() && cb::Awaited(actor, 0)) { g_retry.insert(eid); continue; }
        if (!BroadcastContainer(s, eid, inv, -1, /*force=*/false)) g_retry.insert(eid);
    }
}

// The apply.

// The setter-managed state (currVol, Mass, the display names) is re-derived through the engine's
// own verbs, never raw-written; updateVolumesAndMass calls only Get Volume, and the ejector
// checkObjectsVolume (which calls takeObj) is not called. Each verb is looked up on the instance's own
// class through the memoised dispatch lookup, which climbs to the class that declares it --
// updateVolumesAndMass is declared only on Aprop_container_C, and every real container is a subclass --
// and holds its answer by the class's slot and serial, so no function of a class that is gone is
// called. A verb that does not resolve is said once: the applied contents then show a stale currVol or
// stale names.
void RederiveManagedState(void* owner, void* inv) {
    void* const updateVol =
        owner ? R::FindDispatchFunctionCached(R::ClassOf(owner), L"updateVolumesAndMass") : nullptr;
    void* const recalcNames = inv ? R::FindDispatchFunctionCached(R::ClassOf(inv), L"recalculateNames") : nullptr;
    if ((owner && !updateVol) || (inv && !recalcNames)) {
        static bool s_said = false;
        if (!s_said) {
            s_said = true;
            UE_LOGW("container_contents: re-derive verb MISSING (updateVolumesAndMass=%p recalculateNames=%p) "
                    "-- applied contents will show a STALE currVol / names", updateVol, recalcNames);
        }
    }
    ue_wrap::CachedObjRef invRef;
    invRef.Set(inv);
    if (updateVol)   ue_wrap::component_calls::CallParamless(owner, updateVol);
    if (recalcNames && invRef.Get()) ue_wrap::component_calls::CallParamless(invRef.Get(), recalcNames);
}

// What an inbound blob did to this peer; a bool cannot express it, since "handled" and "changed
// something" are different facts and the relay needs the second. Gated on a bool that was true
// for both an accepted and a refused client write, a refused write was relayed to every other
// client stamped as slot 0, host truth, which peers running no CAS applied unconditionally.
enum class Ingest {
    Park,      // the eid does not resolve yet (birth skew / mid-join) -- park and retry
    Handled,   // dealt with and deliberately NOT applied: refused, malformed, non-container,
               // BOUNDARY 1, or a no-op duplicate. NEVER relayed.
    Applied,   // this peer's state actually changed. The only outcome the host may pass on.
};

// Applied, Handled or Park; see Ingest.
Ingest ApplyContents(uint32_t eid, const std::vector<SR::SaveRecord>& recs, uint64_t blobHash) {
    void* actor = LivePropActor(eid);
    if (!actor) return Ingest::Park;
    // The wire eid must name a container before a cached component offset is read off it.
    if (!ci::IsContainer(actor)) {
        UE_LOGW("container_contents: eid=%u does not resolve to a container -- refusing", eid);
        return Ingest::Handled;
    }
    void* inv = ci::InventoryOf(actor);
    if (!inv || !ci::IsInventory(inv)) return Ingest::Park;
    {   // Identical contents -> do nothing. The raw-write orphans the previous arrays, so a
        // no-op apply must not allocate at all (that is what bounds the leak).
        auto it = g_appliedHash.find(eid);
        if (it != g_appliedHash.end() && it->second == blobHash) return Ingest::Handled;
    }
    if (!ci::IsWorldInventory(inv)) {           // BOUNDARY 1 (fail-closed)
        UE_LOGW("container_contents: eid=%u resolves to a PERSONAL inventory (or an unresolvable "
                "Player flag) -- refusing to apply", eid);
        return Ingest::Handled;                       // resolved; deliberately not applied
    }
    uint8_t* slot = ci::ContentsSlot(inv);
    if (!slot) return Ingest::Park;

    // The allocator pre-flight: without it an empty array would silently replace real contents.
    if (void* probe = R::EngineAlloc(16)) {
        R::EngineFree(probe);
    } else {
        UE_LOGW("container_contents: eid=%u -- EngineAlloc unavailable; refusing to write", eid);
        return Ingest::Handled;
    }

    const int32_t n = static_cast<int32_t>(recs.size());
    void* buf = SR::AllocZeroed(static_cast<size_t>(n), static_cast<size_t>(SR::kSaveStride));
    if (!buf && n > 0) {
        UE_LOGW("container_contents: eid=%u -- alloc of %d records failed; leaving contents", eid, n);
        return Ingest::Handled;
    }
    // The HOST applies a CLIENT's contents with its world up, so it resolves each record's class
    // names through object_index (an attacker's unknown name is an O(1) miss, not a whole-array walk);
    // a CLIENT applying the host's broadcast may be mid-load, so it keeps FindClass (its index can trail).
    const SR::ClassResolver resolve = IsHost() ? &ue_wrap::object_index::ClassByName : nullptr;
    for (int32_t i = 0; i < n; ++i)
        SR::WriteSaveRecord(reinterpret_cast<uint8_t*>(buf) + static_cast<size_t>(i) * SR::kSaveStride,
                            recs[i], resolve);
    // The previous buffer is orphaned: freeing the nested sub-arrays, minted FStrings and signal
    // rows recursively is far more crash-prone than leaking them, and the engine never double-frees
    // a buffer it has lost. inventory::ApplyToSaveObject orphans the same way once per join; this
    // lane is steady-state, and the content-hash gate above bounds it, since an apply happens only
    // when the contents changed.
    SR::WriteArrHeader(slot, 0, buf, n);

    g_appliedHash[eid] = blobHash;
    // The base a later local edit declares; never cleared by our own verb edge.
    if (!IsHost()) g_baseHash[eid] = blobHash;
    ue_wrap::CachedObjRef owner;
    owner.Set(OwnerOf(inv));
    RederiveManagedState(owner.Get(), inv);
    if (void* currentOwner = owner.Get()) ue_wrap::container_view::Refresh(currentOwner);
    UE_LOGI("container_contents: eid=%u applied %d records", eid, n);
    return Ingest::Applied;
}

Ingest ParseAndApply(const std::vector<uint8_t>& blob, uint32_t& outEid, uint8_t senderSlot,
                     wp::Source src) {
    size_t o = 0;
    uint64_t baseHash = 0;
    if (!cw::ParseHeader(blob, o, outEid, baseHash)) return Ingest::Handled;
    // A container a client threw: the host built its copy empty from the intent and has published
    // nothing for it, so any slice of that author is the newest version of the transfer, whatever
    // base it names -- an edit made while the first slice waited in the pen included. There is no
    // reach to measure either: the throw put it where it is.
    void* const bornActor = (IsHost() && senderSlot != 0) ? LivePropActor(outEid) : nullptr;
    const bool birth = bornActor && cb::Awaited(bornActor, senderSlot);
    // Unjudged is not unbounded: a birth slice spends the same arrival budget an edit does, so an
    // author cannot make the host parse slices for this container at any rate while it waits. Past
    // the budget it is HELD in the pen rather than refused: it is the thrower's only copy of the
    // contents and is sent once, and a replay spends nothing, so the pen's own per-author cap and TTL
    // are what bound it.
    if (birth) {
        auto* s = g_session.load(std::memory_order_acquire);
        if (!s) return Ingest::Handled;
        if (wp::SpendArrival(outEid, senderSlot, NowMs(), *s, src, /*held=*/true) == wp::Decision::TooFast)
            return Ingest::Park;
    }
    // Host arbitration before anything is touched; a refusal is answered by re-publishing the
    // host's truth to the author, so it converges instead of sitting on a divergent view.
    if (IsHost() && senderSlot != 0 && !birth) {
        auto* s = g_session.load(std::memory_order_acquire);
        // No session, no arbitration, and a client slice is never applied unjudged: the refusal
        // that cannot be explained is still a refusal.
        const wp::Decision d = s ? wp::Accept(outEid, baseHash, senderSlot, NowMs(), *s, src)
                                 : wp::Decision::StaleBase;
        // No body to measure the author against yet: HOLD, do not refuse. The pen replays it and
        // the reach is judged the moment that peer has posed; the hold is bounded per author and
        // by the TTL, so a peer that never poses costs the host eight slices for thirty seconds.
        if (d == wp::Decision::NoBodyYet) return Ingest::Park;
        if (d != wp::Decision::Accept) {
            // Every refusal answers with the host's truth so the author converges -- except the
            // rate one, because a bound on how fast an author may spend the host must not make
            // the host spend more the faster it is pushed. No peer of this build can reach that
            // bound (one slice per container per 250 ms sweep, and only when it changed).
            void* actor = LivePropActor(outEid);
            void* inv = actor && IsContainerActor(actor) ? InventoryOf(actor) : nullptr;
            if (s && inv && d != wp::Decision::TooFast && IsWorldContainerInventory(inv)) {
                // A slice that does not fit is not an answer. The sweep retries it; leaving it
                // untracked kept the author on the divergent copy until some later edit.
                if (!BroadcastContainer(s, outEid, inv, static_cast<int>(senderSlot), /*force=*/true))
                    g_retry.insert(outEid);
            }
            // Handled, not Applied: never relayed; third peers run no arbitration.
            return Ingest::Handled;
        }
    }
    std::vector<SR::SaveRecord> recs;
    const char* why = "";
    if (!cw::ParseRecords(blob, o, recs, &why)) {
        UE_LOGW("container_contents: eid=%u -- %s; dropped", outEid, why);
        return Ingest::Handled;
    }
    // BOUNDARY 2, the INBOUND half. The send side neuters a nested container's own GObjStack index
    // because it names a slot in the SENDER's array; enforcing that outbound alone trusts every
    // sender to be this build. Written through unchanged, prop_container::loadData reads ints[0][0]
    // unguarded and the nested container would bind whatever sits in that slot on THIS machine --
    // another container's contents, or a player's inventory.
    size_t foreignIndices = 0;
    for (auto& r : recs) {
        if (!ci::IsContainerRecord(r)) continue;
        if (ci::CarriesInventoryIndex(r)) ++foreignIndices;
        ci::ClearInventoryIndex(r);
    }
    if (foreignIndices) {
        // A hit is a peer that is not this build, or a second producer that skipped the boundary.
        UE_LOGW("container_contents: eid=%u -- %zu nested-container record(s) arrived carrying a "
                "foreign GObjStack index; neutered before the write", outEid, foreignIndices);
    }
    // What the host put into its copy while the transfer was in flight. That copy was born empty, so
    // all of it is the host's own and is kept beside the author's: replacing it would lose the host's
    // item, refusing the slice would lose the author's. Read raw: these records stay in this array.
    bool merged = false;
    if (birth) {
        void* actor = LivePropActor(outEid);
        void* inv = actor && ci::IsContainer(actor) ? ci::InventoryOf(actor) : nullptr;
        std::vector<SR::SaveRecord> hostRecs;
        if (inv && ReadContents(inv, hostRecs, /*neuterNested=*/false) && !hostRecs.empty()) {
            if (recs.size() + hostRecs.size() <= cw::kMaxRecords) {
                recs.insert(recs.end(), hostRecs.begin(), hostRecs.end());
                merged = true;
            } else {
                // Past the slice cap the two cannot travel as one. The thrower's half is the only copy
                // of its contents; the host's was added within the transfer's own round trip, and no
                // other peer has seen it: the thrower's slice is applied alone, and every peer
                // converges on it.
                UE_LOGW("container_contents: eid=%u -- the host's %zu record(s) do not fit beside the "
                        "thrower's %zu under the %zu cap; the thrower's slice is applied alone and the "
                        "host's records are dropped", outEid, hostRecs.size(), recs.size(), cw::kMaxRecords);
            }
        }
    }
    const uint64_t contentHash = cw::ContentHash(outEid, recs);
    const Ingest outcome = ApplyContents(outEid, recs, contentHash);
    // Host, client-authored and accepted. Two records, and the second one was missing: g_sentHash
    // keeps the host's own drain from re-broadcasting the identical slice back the long way round,
    // while NotePublished moves the compare-and-swap baseline onto what the relay is about to hand
    // every other peer. Without it the baseline stayed at the last HOST fan-out while the receivers
    // moved on, and the second peer to edit a container was refused for being as up to date as the
    // first.
    if (outcome == Ingest::Applied && IsHost() && senderSlot != 0) {
        if (birth) cb::Complete(bornActor);
        if (merged) {
            // The author lacks the host's part: the merged contents go to every peer, the author
            // included, by the host's own fan-out on the next sweep.
            g_sentHash.erase(outEid);
            g_dirty.insert(outEid);
            UE_LOGI("container_contents: eid=%u slot %u transfer MERGED with the host's own records -- "
                    "published to all on the next sweep", outEid, static_cast<unsigned>(senderSlot));
            return outcome;
        }
        g_sentHash[outEid] = contentHash;
        wp::NotePublished(outEid, contentHash);
        UE_LOGI("container_contents: eid=%u slot %u ACCEPTED -- the published baseline is now "
                "%llu, which is what the relay carries", outEid, static_cast<unsigned>(senderSlot),
                static_cast<unsigned long long>(contentHash));
        // The relay belongs HERE and not at the arrival site: a slice that waited in the pen and
        // applied on a replay is the host's new truth just as much as one that applied on arrival,
        // and relaying only the second kind left every other peer without it, silently, while the
        // baseline above told the host they had it.
        if (auto* s = g_session.load(std::memory_order_acquire))
            RelayToOthers(s, senderSlot, blob);
    }
    return outcome;
}

// The park's replay: true when the blob was dealt with, false while its container still does not
// resolve. The author rides with the blob because a parked slice must pass the SAME arbitration it
// would have passed on arrival -- replaying a client's write as slot 0 would hand it the host's
// own authority, and the host does park client writes (every inbound blob it cannot resolve yet).
bool ReplayParked(const std::vector<uint8_t>& blob, uint8_t authorSlot) {
    uint32_t eid = 0;
    return ParseAndApply(blob, eid, authorSlot, wp::Source::Replay) != Ingest::Park;
}

// The verb edge.

// Fires at the entry of addObject and takeObj on the game thread, a change notice rather than an
// action: it remembers which component was touched. No role gate: the verb is a local virtual
// call, so by the time it is seen the item has already moved on this machine, and an intent the
// host could deny cannot exist here. Gated on the host it dropped every client extraction: the
// client took one of two burgers, the host's slot stayed at two, and the world gained a burger.
sg::Verdict OnVerbEntry(const sg::Call& br) {
    if (br.tag == kVerbTakeObj) {
        const bool inventoryCall = br.object && IsInventoryComponent(br.object);
        if (g_takeObjCallDepth < kTakeObjCallDepthCap) {
            g_takeObjCallStack[g_takeObjCallDepth++] = inventoryCall;
            if (inventoryCall) ++g_activeTakeObjCalls;
        } else {
            ++g_takeObjOverflowDepth;
        }
    }
    // The first statement, ahead of every filter: a resolved verb name does not prove this callback
    // runs, and a registration once returned true with the callback inert for a whole session. If
    // this line is absent from a log, the lane is dead.
    if (!g_verbEntered) {
        g_verbEntered = true;
        UE_LOGI("container_contents: the verb watch ENTERED for the first time on this peer "
                "(role=%s) -- the addObject/takeObj edge is LIVE",
                IsHost() ? "HOST" : "CLIENT");
    }
    if (!br.object) return sg::Verdict::Run;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return sg::Verdict::Run;
    // The verb filter matches on the name alone, so the context is discriminated here: the first
    // non-propInventory context carrying an addObject would otherwise poison the offset cache for
    // the session.
    if (!IsInventoryComponent(br.object)) return sg::Verdict::Run;
    // A takeObj on an inventory component arms the extraction window; addObject must not.
    if (br.tag == kVerbTakeObj) g_takeObjArmMs.store(NowMs(), std::memory_order_relaxed);
    void* owner = OwnerOf(br.object);
    if (!owner) return sg::Verdict::Run;
    const uint32_t eid =
        static_cast<uint32_t>(coop::element::Registry::Get().EidForActor(owner));
    if (eid == static_cast<uint32_t>(coop::element::kInvalidId)) return sg::Verdict::Run;
    g_dirty.insert(eid);   // resolve identity AT THE EDGE; deref nothing later
    // The local change is stamped, so the host can tell a stale client write from a clean one.
    // Host-only: nothing on a client ever reads it, and an ungated stamp grows a row per container
    // that peer has ever touched.
    if (IsHost()) wp::NoteLocalChange(eid, NowMs());
    // And the applied hash is dropped: it means "an identical blob is a no-op" only while our state
    // still equals what we applied, and after our own mutation a corrective re-publish of the
    // unchanged host truth would look like a duplicate; a client whose write was refused once kept
    // its diverged contents forever that way.
    g_appliedHash.erase(eid);
    return sg::Verdict::Run;
}

void OnVerbExit(const sg::Call& br) {
    if (br.tag != kVerbTakeObj) return;
    if (g_takeObjOverflowDepth) {
        --g_takeObjOverflowDepth;
        return;
    }
    if (!g_takeObjCallDepth) return;
    if (g_takeObjCallStack[--g_takeObjCallDepth] && g_activeTakeObjCalls)
        --g_activeTakeObjCalls;
}

}  // namespace

// From event_feed's client-side SnapshotBegin and SnapshotComplete dispatch, on the game thread.
void NoteJoinSnapshotBracket(bool open) { pk::NoteJoinBracket(open, NowMs()); }

// True inside the actual takeObj body for a relevant inventory component. The second proof is
// time-windowed to cover extraction UI code that creates its actor immediately after takeObj.
bool TakeObjInCall() { return g_activeTakeObjCalls != 0; }

// A non-consuming window prevents a hand-view actor from consuming the world item's admission.
bool TakeObjWindowOpen() {
    const uint64_t armed = g_takeObjArmMs.load(std::memory_order_relaxed);
    const uint64_t now = NowMs();
    return armed != 0 && now >= armed && now - armed <= kTakeObjWindowMs;
}

void Install(coop::net::Session* session) {
    // This lane owns a container's GObjStack slice behind a host-arbitrated compare-and-swap; the
    // container's own save record carries the index into that stack, with none of the arbitration.
    coop::prop_save_data::DeclareClassOwnedElsewhere(L"prop_container_C");
    g_session.store(session, std::memory_order_release);
}

void Tick() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->running()) return;

    if (!g_verbsRegistered) {
        g_verbsRegistered =
            sg::WatchName(L"addObject", kVerbDirty, &OnVerbEntry, nullptr) &&
            sg::WatchName(L"takeObj",   kVerbTakeObj, &OnVerbEntry, &OnVerbExit);
        static bool s_saidFailed = false;
        if (!g_verbsRegistered && !s_saidFailed) {
            // Once. This block retries every tick, and the failure it reports is the gate
            // refusing permanently (a full table, or no detour), so an unlatched line here is a
            // warning per tick for the rest of the session.
            s_saidFailed = true;
            UE_LOGW("container_contents: verb registration FAILED -- the lane is inert");
        }
    }
    sg::ResolvePendingNames();

    if (!g_announced) {
        g_announced = true;
        UE_LOGI("container_contents: installed (GObjStack slice lane, the addObject/takeObj watch)");
    }

    const uint64_t now = NowMs();
    if (now < g_nextSweep) return;
    g_nextSweep = now + kSweepMs;

    g_asm.Sweep(std::chrono::steady_clock::now(), std::chrono::seconds(10));

    // Both peers drain: the host fans its changes out, a client ships the container it mutated to
    // the host, which arbitrates and relays. Both sweep their parked inbound blobs -- the host's
    // were never swept at all, so a client write it could not resolve on arrival was neither
    // retried nor evicted for the life of the session.
    cb::Sweep(now, g_dirty);
    DrainDirty(s);
    pk::Sweep(&ReplayParked, NowMs());
}

void OnContentsChunk(const coop::net::BlobChunkPayload& p, uint8_t senderSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s) return;
    // The acceptance matrix, before the assembler so no peer drives reassembly for a direction it
    // may not send: the host accepts only a non-zero slot (slot 0 is its own fan-out coming back);
    // a client accepts only slot 0, since peers never hear each other directly.
    if (IsHost()) {
        if (senderSlot == 0) return;  // our own fan-out echoed back -- not a thing to apply
    } else if (senderSlot != 0) {
        UE_LOGW("container_contents: blob from non-host slot %u on a CLIENT -- REJECTED "
                "(the host is the only authority a client accepts)",
                static_cast<unsigned>(senderSlot));
        return;
    }
    std::vector<uint8_t> blob;
    if (!g_asm.OnChunk(p, senderSlot, blob)) return;  // incomplete

    uint32_t eid = 0;
    const Ingest outcome = ParseAndApply(blob, eid, senderSlot, wp::Source::Arrival);
    if (outcome == Ingest::Park) {
        // The container's element is not bound yet: parked (latest wins per eid) and retried by the
        // sweep until the TTL.
        pk::Admit(eid, senderSlot, std::move(blob), NowMs());
        return;
    }
}

void QueueConnectBroadcastForSlot(int peerSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !IsHost()) return;

    std::vector<coop::element::Registry::ActorIdPair> pairs;
    coop::element::Registry::Get().SnapshotActorsByType(coop::element::ElementType::Prop, pairs);
    size_t sent = 0;
    for (const auto& pr : pairs) {
        // IsLiveByIndex: the snapshot does not protect the actor pointer.
        if (!pr.actor || !R::IsLiveByIndex(pr.actor, pr.internalIdx)) continue;
        if (!ue_wrap::prop::WalksToBase(R::ClassOf(pr.actor), base)) continue;
        void* inv = InventoryOf(pr.actor);
        if (!inv || !IsWorldContainerInventory(inv)) continue;   // BOUNDARY 1 (fail-closed)
        // False is a slice that did not go out (cap, ceiling, or a refused send). The connect
        // pass does not run again; the dirty sweep is what retries once the live slice fits.
        if (BroadcastContainer(s, static_cast<uint32_t>(pr.id), inv, peerSlot, /*force=*/true)) ++sent;
        else g_retry.insert(static_cast<uint32_t>(pr.id));
    }
    UE_LOGI("container_contents: connect seed -> slot %d: %zu world containers", peerSlot, sent);
}

// The dev-instrument seams (see the header).

size_t SnapshotWorldContainers(WorldContainer* out, size_t want) {
    if (!out || want == 0) return 0;
    std::vector<coop::element::Registry::ActorIdPair> pairs;
    coop::element::Registry::Get().SnapshotActorsByType(coop::element::ElementType::Prop, pairs);
    size_t n = 0;
    for (const auto& pr : pairs) {
        if (n >= want) break;
        // IsLiveByIndex: the snapshot does not protect the actor pointer.
        if (!pr.actor || !R::IsLiveByIndex(pr.actor, pr.internalIdx)) continue;
        if (!ci::IsContainer(pr.actor)) continue;
        void* inv = ci::InventoryOf(pr.actor);
        if (!inv || !ci::IsWorldInventory(inv)) continue;   // BOUNDARY 1, the shipped one
        out[n++] = WorldContainer{static_cast<uint32_t>(pr.id), pr.actor, inv};
    }
    return n;
}

bool VerbWatchEntered() { return g_verbEntered; }

bool ContentsDigest(uint32_t eid, int32_t& outCount, float& outVol) {
    outCount = -1;
    outVol = 0.f;
    void* actor = LivePropActor(eid);
    if (!actor || !ci::IsContainer(actor)) return false;
    void* inv = ci::InventoryOf(actor);
    if (!inv || !ci::IsWorldInventory(inv)) return false;
    if (uint8_t* slot = ci::ContentsSlot(inv)) outCount = SR::ReadArr(slot, 0).num;
    ci::CurrentVolume(inv, outVol);
    return true;
}

void OnDisconnect() {
    g_dirty.clear();
    g_retry.clear();
    wp::Reset();
    pk::Reset();
    g_sentHash.clear();
    g_baseHash.clear();
    g_appliedHash.clear();
    cb::Reset();
    g_asm.Clear();
    g_nextSweep = 0;
    g_takeObjArmMs.store(0, std::memory_order_relaxed);
    g_announced = false;
    g_verbEntered = false;
}

}  // namespace coop::props::container_contents_sync
