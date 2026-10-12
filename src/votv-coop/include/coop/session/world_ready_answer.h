// coop/session/world_ready_answer.h -- the host's answer to a client's ClientWorldReady announce.
//
// The first announce of an occupancy completes the join: it marks the slot world-ready and runs the
// connect replay. A later announce is rate-limited by a per-slot IntentBucket. A legitimate world-change
// re-announce (net_pump::MaybeRequestReAnnounce) is spaced at least 2.0 s apart by the client's quiesce
// probe, so it always finds a token and its replay runs; a flood empties the bucket -- a rate no honest
// client reaches -- and the slot is ended (MV-H34). No debt and no Tick: MTA ends a repeated in-game
// notice outright (CRPCFunctions.cpp:95-124, PlayerInGameNotice / InitialDataStream, both IsJoined-guarded,
// banning nothing); the burst of 2 is the one divergence, a margin for the re-announce MTA has no
// counterpart of.
#pragma once

namespace coop::net { class Session; }

namespace coop::world_ready_answer {

// Host only, slots 1..kMaxPeers-1 (a no-op otherwise). The whole body of event_feed's ClientWorldReady
// case: the first announce completes the join, a later one runs under the replay bucket or ends the slot.
// Game thread, from the event_feed drain.
void OnAnnounce(coop::net::Session& session, int slot);

}  // namespace coop::world_ready_answer
