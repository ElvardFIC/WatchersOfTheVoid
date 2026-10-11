// coop/dev/hostile_drill.h -- the receive boundary's drill: a hostile peer, judged by the receiver.
//
// A dev-gated harness for the receive boundary's answer to a hostile peer: one peer sends deliberately
// malformed coop packets of a chosen kind, and the RECEIVING peer's mod judges whether the boundary
// refused them without stalling the session, the verdict PRINTED by the mod so the rig reads a marker,
// never a guess. Each arm proves its one attack, on the row content it forges; chunk framing and every
// other kind are proven elsewhere or by a later arm. The discipline is "write the test that proves the
// fix": the same judge reads green on the gated bytes and red on bytes without the gate (the commit
// before the fix, or a lane that deletes the gate in source and never lands), so a PASS is known to be
// one the judge could have refused.
//
// This is defensive testing of OUR OWN mod, in OUR OWN rig. Nothing here ships enabled: the enum row
// (hostile_drill) defaults off and gates every path -- a single read when off -- and the forged sends
// are the same shape as the dev-gated malformed sends the tree already carries (midnight_drill's NaN
// clock, drive_drill's forged row). Set on BOTH peers: the client sends, the host judges; the host's
// gates read nothing of it. Protocol waits on readiness (rig_ready milestones), never a clock.
#pragma once

namespace coop::net { class Session; }

namespace coop::dev::hostile_drill {

// Any arm set? A single enum read; every path below returns early when off.
bool IsEnabled();

void Install(coop::net::Session* session);
void Tick();
void OnDisconnect();

}  // namespace coop::dev::hostile_drill
