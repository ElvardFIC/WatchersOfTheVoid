// coop/props/prop_stick_sync.h -- the wall-attach component's stick and unstick (the cameras, the
// signs, the plasma TV), mirrored across peers.
//
// The stick is Blueprint-internal and invisible to a ProcessEvent observer except at one
// seam: its commit is reached only through a latent Delay resume, and the latent manager
// resumes ubergraphs through ProcessEvent, so an observer on
// ExecuteUbergraph_comp_wallAttachable sees every player-driven stick. The sticking peer
// captures the exact support actor/component after the native attach. Receivers attach to that
// component directly rather than trusting a local re-trace, and retire the mirror when the
// support ends play. Follows MTA's element-state RPCs (SET_ELEMENT_FROZEN, ATTACH_ELEMENTS in
// CElementRPCs.cpp): state messages, not event replay. docs/props.md carries the lane.

#pragma once

#include <cstdint>

namespace coop::net {
class Session;
struct PropStickStatePayload;
}  // namespace coop::net

// An unstick is state too, since not every unstick is a grab: a crowbar's pry frees a pryable and
// kicks it off the wall with nobody holding it. Every route reaches the component's unstick from
// script -- a grab's prelude, the pry's crowbarOpen, a fridge glow's knock -- so the script gate
// watches it. The unsticking peer broadcasts the unstick (flags 0) with the prop's pose and
// velocity once the body has freed it, and a receiver runs the component's own unstick with the
// tool and lets the copy go from there. A peer still loading gets neither message (the pre-world
// gate), so a join's snapshot row and its window correction carry the host's frozen and static, and
// the snapshot's following PropStickState carries the support identity needed to attach it exactly.

namespace coop::prop_stick_sync {

// Idempotent install: one index lookup a call until comp_wallAttachable_C is loaded, after which each
// half settles on its first attempt; run once on demand when a join's converge or a live message
// needs the lane first. Two halves, each latched on its own, so a recook that loses one leaves the
// other working: the stick observer (ExecuteUbergraph_comp_wallAttachable and the component's `prop`
// field) and the unstick verb (unstick and its script-gate watch). Caches
// `session`. Also observes overriding crowbarOpen bodies for pryable props with no
// wall-attach component; only a successful native freeing becomes an unstick.
void Install(coop::net::Session* session);

// Drain the stick commits and unsticks recorded since the last pass: verify (still live; still
// stuck for a stick, freed for an unstick) and broadcast as PropStickState. Also drains exact-support
// retries and retires attachments when their support ends play. MUST run before
// local_streams::Tick within the pump pass (stick-before-release ordering). Cheap no-op when
// no pending work. A temporary send refusal retries for at most two seconds while the
// original actor and outcome stay current. Game thread (net pump).
void Tick();

// Send an already-stuck prop's exact support identity after its PropSpawn in a join snapshot.
// A free prop is a no-op. Host/game thread only.
bool SendCurrentToSlot(coop::net::Session* session, void* prop, int peerSlot);

// Receiver for PropStickState (wired in event_dispatch_entity): a stick, or an unstick when the
// flags are 0. An unstick leaves a copy this peer's own grab or a drive owns where it is
// (remote_prop::IsOwnedHere). Game thread.
void OnStickState(const coop::net::PropStickStatePayload& payload, uint8_t senderPeerSlot);

// A join's snapshot row or window correction makes this copy's stuck state the host's
// (`physFlags`, read off the host's live prop): a copy stuck here that is free on the host gets the
// component's unstick, with the tool. A copy free here but stuck on the host waits for the
// snapshot's exact-support PropStickState; it must not guess by tracing locally. Non-component props
// converge their static bit through prop_C's base setter. Only on a difference;
// the caller converges frozen and sleep after it. Game thread.
void ConvergeStuck(void* actor, uint8_t physFlags);

// True iff `actor` carries the wall-attach component in its comp_wallAttachable variable: the
// wall-attachable lineage and the plasma TV, which carries the component without the class. They
// are the props whose frozen and static a hold may clear. False until the component class
// resolves. Game thread.
bool IsWallAttachable(void* actor);

// The component's own unstick on this peer's copy, with the tool: the peer that freed the prop had
// it off the wall, by hand or by a pry, so the copy takes that outcome. The tool skips the check that
// fails a hand grab of a pried-on prop and shows its "Tool required" hint, and on that branch clears
// the component's `holding`, which nobody here has set on the copy. Not mirrored back. False when the
// unstick half is not installed or a declared component does not resolve. Ordinary
// props without that member use the base state setter to clear static/frozen.
// Game thread.
bool ReplayUnstick(void* actor);

// Clear per-session state (the pending stick and unstick records). Net disconnect.
void OnDisconnect();

}  // namespace coop::prop_stick_sync
