// coop/interactables/atv_sync.h -- ATV/quadbike (AATV_C) rig sync.
//
// Gameplay/network layer (principle 7): owns the wire protocol, the two authority predicates, the
// per-tick stream, the receiver-side CORRECTOR, the key->actor index, and the connect-snapshot.
// Talks to the engine ONLY through ue_wrap::atv.
//
// A peer that does not author an ATV does NOT freeze it (MTA CNetAPI::ReadVehiclePuresync +
// CClientVehicle::UpdateTargetPosition + CUnoccupiedVehicleSync). It leaves the rig simulating
// natively and corrects it toward the authority. AATV_C is a five-body constraint rig whose visible
// output IS suspension travel, so the freeze-and-teleport shape moved the body 29.58 cm where the
// native travel is 2-4 cm, and handed the other peer's copy a 158 cm/s launch on release.

#pragma once

#include <cstdint>

namespace coop::net {
class Session;
struct AtvStatePayload;
struct AtvReleasePayload;
struct AtvSpawnPayload;
struct AtvDestroyPayload;
}  // namespace coop::net

namespace coop::atv_sync {

// Resolve AATV_C + build the key->actor index; store the session pointer. Idempotent; retried
// every net-pump tick until the class loads. Game thread.
//
// Identity is the ATV's own game Key on every peer -- a keyed pose stream, not the
// eid/Element/spawn machinery (YAGNI for one always-present keyed actor; the grime and window-dirt
// syncs made the same call). The host announces every ATV of its index to a joiner and each one its
// index gains (AtvSpawn / AtvDestroy), and a mirror is spawned carrying the host's Key.
void Install(coop::net::Session* session);

// Receiver entry: an AtvState packet arrived (payload already memcpy'd + range-checked by
// event_feed). Resolves the ATV by Key, records who holds it, and CORRECTS the local simulating rig
// toward the wire pose/velocity -- a velocity write plus a bounded corrective term, warping via the
// game's own teleportVehicle only past a speed-scaled threshold. Ignored if this peer is itself the
// pose author of that ATV. Called from event_feed.
void OnReliable(const coop::net::AtvStatePayload& payload, uint8_t senderPeerSlot);

// Receiver entry: an AtvRelease packet arrived -- "the sender is no longer this ATV's author".
// Clears the seat and the author, and NOTHING else: nothing was frozen, the velocity already rode
// every AtvState, and on the host the cleared author is what makes it the ATV's idle syncer on the
// next tick. Ignored if this peer is itself the pose author. From event_feed.
void OnAtvRelease(const coop::net::AtvReleasePayload& payload, uint8_t senderPeerSlot);

// Receiver entry (CLIENT-only): an AtvSpawn arrived. The host announces an ATV of its index under
// its game Key. A key that cannot name an ATV is dropped; one this peer already holds live is a
// no-op (re-announce / connect dup). Otherwise fresh-spawns a native AATV_C carrying that Key
// (ue_wrap::atv::SpawnMirror, physics ON = grabbable) at the host pose and registers it under it,
// so the existing AtvState/AtvRelease key-stream drives it. From event_feed.
void OnAtvSpawn(const coop::net::AtvSpawnPayload& payload);

// Receiver entry (CLIENT-only): an AtvDestroy arrived. The host's ATV with that Key is gone ->
// K2_DestroyActor the fresh-spawned mirror + drop the index entry. From event_feed.
void OnAtvDestroy(const coop::net::AtvDestroyPayload& payload);

// HOST-only: snapshot every indexed ATV's pose, VELOCITY and holder to a freshly connected client
// `peerSlot` (adopt=1 -> the joiner warps to it). Carrying the velocity is what lets an ATV that is
// airborne or rolling at the join arrive moving and land, instead of hanging where it was
// (principle 8). Net-pump connect edge. Game thread.
void QueueConnectBroadcastForSlot(int peerSlot);

// Per-tick pump. Per ATV: resolve both predicates, latch the brain to tick ownership, and either
// stream at ~20 Hz (pose author), or stream at 5 Hz AND only on change (the host syncing an idle
// one -- a parked ATV costs zero packets), or do nothing at all (a mirror, which is a simulating
// body corrected at packet arrival). Refuses to run if the collision guard did not arm. Call every
// net-pump tick on the game thread.
//
// The two predicates are separate on purpose. POSE AUTHORITY (drives or carries it) decides who
// STREAMS. TICK OWNERSHIP (that, or "I am the host and nobody authors it") decides whose machine
// runs the brain: the accumulators, the wheel torque, and the collision-authored damage. On the
// host they are different sets, and fusing them breaks the idle syncer. On the wire they are
// `authorSlot`; `occupantSlot` stays SEPARATE because it is the SEAT that device_occupancy's
// E-press deny reads, and a peer merely grabbing an ATV must not deny a seat nobody is in.
void Tick();

// Session teardown: disarm the collision guard, give EVERY surviving ATV its brain back
// unconditionally (single-player must own its own vehicles again), destroy any runtime mirror we
// fresh-spawned, then clear the index.
void OnDisconnect();

// Interaction query: true iff `actor` is indexed as an ATV currently occupied by another network
// peer (occupantSlot != 0xFF && occupantSlot != LocalSlot()). Populates `outOccupantSlot` if non-null.
bool IsOccupiedByOther(void* actor, uint8_t* outOccupantSlot = nullptr);

// The indexed ATV (any ATV_C, the witch car included) whose seat `slot` occupies (occupantSlot == slot), or nullptr.
// A grabber is not seated. The same index guard as IsOccupiedByOther; an entry whose actor is no longer live by its
// index is skipped. Game thread.
void* ActorSeatedBy(uint8_t slot);

// Diagnostic read: does THIS peer own `actor`'s tick (run its brain) right now? False for an
// unindexed actor, and false whenever the lane is not running. Exposed for coop/dev/atv_probe, so
// an instrumented run can state which side of the mirror each sample came from. Game thread.
bool OwnsTick(void* actor);

// For the ATV spawn drill, game thread: HOST, the AtvSpawn broadcasts the index's pass sent this
// session (a key the index gained while a client was connected); CLIENT, the live ATV mirrors this
// peer fresh-spawned, the spawns its park ever held, and those it holds now.
uint32_t RuntimeBroadcasts();
int RuntimeMirrors();
uint32_t ParkedEver();
size_t ParkPending();

}  // namespace coop::atv_sync
