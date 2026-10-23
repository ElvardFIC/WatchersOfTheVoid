// coop/player/sleep_sync.h -- who rests: each peer's sleep state, known to the host and to the other clients.
// Game thread throughout.
//
// Every peer sleeps NATIVELY on its own machine: the game's own sleep entry, view, restore and wake
// (a client's world is held at the normal rate by coop/player/sleep_rest). Nothing here holds a peer's
// dilation, camera or wake, and nothing waits for anyone. What crosses the wire is the fact of rest:
//   - each peer's state (awake, resting in a bed or fainted, dreaming in a nightmare) with the bed's
//     class name, sent by a client to the host on a change and relayed by the host to every other client;
//   - the host's own sleeping flag, which a client shows as one chat line when it rises.
// sleep_rest permits shared clock acceleration only when every connected participant is resting.
// Late join: a joiner gets the host flag and every slot, including explicit awake
// rows. Refused delivery retains current-state debt for that recipient generation.
// A client reports its initial state and repeats a non-awake state on host flags.
// World/session/role changes clear snapshots, paced reports and delivery debt.
//
// Every peer retains its own native nightmare conditions and probability. A dream
// scene is personal; the legacy DreamScene packet cannot start one on another peer.

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
