// coop/player/sleep_sync.h -- shares each peer sleep state with the host and clients.
// Sleep and dreams remain native and personal on each machine; this lane carries
// awake/resting/dreaming state and the bed class name. The host relays client
// changes and its own sleeping flag. Shared clock acceleration requires every
// connected participant to rest. Join snapshots include explicit awake rows.
// Refused delivery retains current-state debt per recipient generation.
// Nightmare conditions and probability remain local.

#pragma once

#include "coop/net/protocol.h"

#include <cstddef>
#include <cstdint>

namespace coop::net { class Session; }

namespace coop::sleep_sync {

void Install(coop::net::Session* session);

// Resolve (throttled), poll the local sleep/dream state and report changes.
// sleep_rest owns the client's local restoration speed. This lane writes no
// dream probability, sleep need, dilation, camera or wake state.
void Tick();
void PayOwed(coop::net::Session& session);

// Wire ingest (both roles; see SleepStatePayload). Every field is checked against the sender's role
// and slot here, and a refusal is one rate-limited warning naming the field. `len` is the message's
// payload length, which must be exactly the payload's size.
void OnReliable(const uint8_t* payload, size_t len, uint8_t senderSlot);

// Reserved legacy packet; ignored because replay bypasses personal conditions.
void OnDreamScene(const coop::net::DreamScenePayload& p, uint8_t senderSlot);

// HOST: a world-ready joiner receives current host and every peer state, with retry.
void QueueConnectBroadcastForSlot(int peerSlot);

// HOST: a leaver's state goes back to awake and every client is told.
void OnDisconnectForSlot(int slot);

// The rest state of `slot` as this peer knows it: on the host, from the clients' reports and its own
// polls; on a client, from the host's relays (slot 0 the host). Awake for a slot out of range.
coop::net::SleepPeerState PeerState(int slot);

// A slot's rest state changed as this peer knows it: the host's own polled state (slot 0), a client's report the host
// applied, the host's relay a client took, and a leaver set back to awake. The state itself changed, not only its bed.
// The subscriber runs inside the receive (or the poll), so it only records or does one engine call. One subscriber: a
// second would turn this into a list. Game thread.
using PeerRestFn = void (*)(int slot, coop::net::SleepPeerState state);
void SubscribePeerRest(PeerRestFn fn);

// Is the host asleep: on the host its own isSleep, on a client the last flag the host sent. For
// instruments. Game thread.
bool HostAsleepNow();

// Host clock acceleration requires every connected participant to be resting.
// A joining player with no accepted rest report keeps the world at normal speed.
// A pending wake/dream, reused occupancy or the host's own dream also vetoes it.
bool AllPlayersResting();

// Instruments (the world_latest drill, coop/dev/hostile_drill_world), game thread: how many of a
// client's rest reports the host applied from `slot`; whether this peer's own polled state is not awake.
uint32_t ReportsApplied(int slot);
bool LocalInBed();

// Why a bed was refused at the door: a length past the name's room, a character outside an identifier, a nonzero
// byte past the length, or an Awake report that carries a bed.
enum class BedRefusal : uint8_t { Length = 0, Charset = 1, Tail = 2, AwakeWithBed = 3 };
// How many reports `slot` sent that were refused for `why` (an instrument: the hostile drill's bed judge).
uint32_t BedRefusalsFrom(int slot, BedRefusal why);

void OnDisconnect();

}  // namespace coop::sleep_sync
