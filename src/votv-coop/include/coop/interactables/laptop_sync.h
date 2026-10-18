// coop/interactables/laptop_sync.h -- the stationary PC's (Alaptop_C) power, and the LaptopState wire the portable
// PC's lid (coop/interactables/portable_pc_lid) rides as op 6.
//
// The edge is authored by the presser, the host re-fans it, and a joiner gets the ground-truth state rather than a
// replayed history. The PC's disc slot is coop/interactables/floppy_slot_sync's, its file quad
// coop/interactables/laptop_buffer_sync's. Game thread throughout, on the net-pump tick and the reliable dispatch.

#pragma once

#include "coop/net/protocol.h"

#include <cstdint>

namespace coop::net { class Session; }

namespace coop::laptop_sync {

void Install(coop::net::Session* session);

// 4 Hz: resolve, the power target, and the power edge poll: the isOpened edge, polled because every entry verb is
// EX-invisible. A receiver replays the native actionOptionIndex(b8) under the wire-apply echo guard when local
// differs from wire; one whose powered or anim gate declines retries until it converges, and
// coop/world/power_panel converges the wall-power input (every peer's setPower runs from the host's canonical).
// The portable PC (prop_portablePc_C) is a remote terminal to THIS laptop: its lid is
// coop/interactables/portable_pc_lid's, on op 6.
void Tick();

// Wire ingest (both roles). HOST: applies + re-fans (except origin). Op 6 goes to the lid lane.
void OnLaptopState(const coop::net::LaptopStatePayload& p, uint8_t senderSlot);

// HOST: queue the joiner's settled laptop power (op=3); retry actor loading and enqueue
// refusal. The lid lane sends its own rows.
void QueueConnectBroadcastForSlot(int peerSlot);

void OnPeerLeft(int peerSlot);
void OnDisconnect();

}  // namespace coop::laptop_sync
