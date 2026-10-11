// coop/dev/hostile_drill.h -- the receive-boundary drill (RB arc, WP-1).
//
// A dev-gated harness that proves the receive boundary is ROBUST against a hostile peer: one peer
// sends deliberately malformed coop packets of a chosen kind, and the RECEIVING peer's mod judges
// whether the boundary refused them without stalling the session -- red on the vulnerable bytes,
// green on the fixed ones, the verdict PRINTED by the mod so the rig reads a marker, never a guess.
// It is the discipline of "write the test that proves the fix": the same judge reads the old bytes
// red and the new ones green. One arm (chat_ill_formed_red) is a MUST-FAIL control: it turns a guard
// off and must FAIL, which is how we know the guard carries weight.
//
// This is defensive testing of OUR OWN mod, in OUR OWN rig. Nothing here ships enabled: the enum row
// (hostile_drill) defaults off and gates every path -- a single read when off -- and the forged sends
// are the same shape as the dev-gated malformed sends the tree already carries (midnight_drill's NaN
// clock, drive_drill's forged row, the chat corrupt-wire knob this drill absorbs). Set on BOTH peers:
// the client sends, the host judges. Protocol waits on readiness (rig_ready milestones), never a clock.
#pragma once

namespace coop::net { class Session; }

namespace coop::dev::hostile_drill {

// Any arm set? A single enum read; every path below returns early when off.
bool IsEnabled();

void Install(coop::net::Session* session);
void Tick();
void OnDisconnect();

// The must-fail control: true ONLY while the chat_ill_formed_red arm is set, false in every other
// build. Resolved once into a static bool at first ask (like the chat flag rows), so it is cheap to
// ask per message. chat_sync asks it at BOTH of its strict-decode sites (Admissible and
// ShapeChatLine): skipping only one still refuses the line at the other, and the control could never
// fail. When true, the strict UTF-8 refusal is skipped (a lenient decode feeds the novelty ledger,
// which still runs); everything else stays, so an ill-formed line reaches the lobby record and the
// drill can observe it committed.
bool ChatGuardOff();

}  // namespace coop::dev::hostile_drill
