# TODO - current priorities

Current source/wire protocol: **2154** (merged 2153 synchronization work), game **0.9.0n**.
Delivery package: **0.9.2152**, containing DLL **b2153 / protocol 2153**.
The packaged DLL predates the current source; peers need matching b2154 builds.

The prior in-place development target was 2153. The merge added a wire kind, so
the protocol is now 2154; no new build or delivery package has been produced.

## Entity category request - OPEN

The owner's complete taxonomy, native class mappings and unresolved behaviors are
in [WORK.md](WORK.md), "Complete requested entity coverage and remaining work".
Do not close a creature from allowlist membership or transform replication alone.

Current grayboar work: host physical-body movement feed, owner-directed bite damage,
foreign-health protection, stable adopted-replica authority across drive handoffs,
and native AnimBP locomotion/movement-loop audio input from interpolated velocity.
Host native sensePawn scans Pawn candidates; actual multiplayer target detection
still needs testing. No generic client AI or gameplay-verb replay was added.
Client host-replica bite-overlap and attack-notify bodies are now refused, covering
the local attack impulse and arbitrary-object damage as well as player health.
Live attack/roar/death and four nonlethal hit reactions are implemented and await two-player
verification. Host AnimBP turn now drives client bone steering and turn-loop pitch
with a 100 ms interpolation. Exact time alignment and helper coordination remain open.
Mid-action join now samples the native active montage position/rate and resumes
that animation with local queue-age compensation. It does not restart one-shot
audio, or compensate the unsynchronized host/client clocks/network transit.
Hit reactions use the host's active damage1..4 section after a verified health
decrease with skipAnim=false; clients play the native damage cue without health
writes. Mid-hit joins resume the selected section silently.
Host replicas also refuse native sensePawn target assignment and detected roar/
target-delegate binding on clients. Active helper paths from before save adoption
are now stopped on the verified owned AI controller. The old target-destroyed
delegate was audited: it only unbinds/clears the target and remains native.
Two-player task lifecycle verification remains open; this does not establish shared AI.
Client host replicas now also refuse native damaged, death loot-notify and death
completion bodies. Host death cues use the native half animation rate and attached
death sound; mid-death join resumes only the currently playing death animation.
Corpse/loot counts and native host retirement still need two-player verification.
Client host replicas now also refuse the separate collision impulse/10-damage
body, lightning replacement and charged-form explosion/death timer/punch/spin
bodies. Charged overrides are watched on their actual declaring class. Native
host outcomes, replacement propagation and charged effects still need gameplay checks.

Next implementation:
Client boar navigation now refuses move/updMov/path-check/retry/finish bodies and
the verified AI continuations 15/543/558/966/1016/1381. senseUpdate/saw/setWalking
cannot restart client helper decisions or mode changes. Each client body tick checks
the owned helper's current path and aborts Waiting/Paused/Moving navigation,
including late possession and adopted/held copies. Cosmetic turning now follows
the host AnimBP scalar; its visible result and native task/delegate lifecycle
still need two-player validation. This is not full helper synchronization.
1. Grayboar remaining join/audio/timing work and client helper/turning coordination.
   Live attack/roar/death/hit cosmetic cues are implemented;
   two-player playback and timing still need verification. Preserve native host targeting,
   damage ownership and corpse/loot authority; do not replay loot/damage as cosmetics.
2. Host/client tests for spawn, target, movement, action, damage, death, reconnect
   and mid-event join. Personal Insomniac/nightmare conditions remain peer-local.


## Other open priorities

- Run the ten priority gameplay checks in WORK.md on matching peer builds.
- Ragdoll selection, entry/recovery, local body and limb streaming now have code and
  focused regressions. Verify every selected rig in a two-peer game.
- Shared computer UI state, operator visibility and selected-model sleeping bodies
  are implemented. Verify laptop/portable-PC, bed, faint/ragdoll and chair changes
  between two matching peers.
- Bench58: record exact item/authoring peer and logs for simple-placement loss;
  birth/save retry code does not prove the original symptom resolved.
- Token52: identify the exact counter/UI before changing balance code.
- Wall21/22: atomic cement request/result accounting across hand/slot/drop/retry/
  disconnect; preserve the existing no-spend refusal. Wall41 remains excluded.
- Full backlog and report status: [WORK.md](WORK.md), [TESTLOG.md](TESTLOG.md).

## Current verification and boundaries

package/check_2152.ps1 passes all 9 runners, including expanded entity/boar tests.
Full MSVC Release DLL resources identify **0.9.0n b2153**. Hash and evidence:
build/release-2152-logs/verification.json and entity-validation.json.
Release scripts explicitly distinguish package 2152 from DLL/protocol 2153;
workspace dist/ contains the 2152 ZIP and SHA256SUMS-2152.txt.
Offline regressions do not establish two-player gameplay correctness.
Live grayboar cues use host-only reliable kind 200, cosmetic turn state uses 201
within development 2153. Install
matching current DLLs on both peers; the existing delivery ZIP predates this work.

All 21 merge conflicts are resolved; the merge still awaits its commit. Current
source/test changes remain uncommitted. Preserve all work. Commit, push, game
installation and publication require an explicit request. No other-agent workflow.
Update TODO/WORK/TESTLOG in place; detailed prior evidence remains in WORK/TESTLOG.

Fork review: see WORK.md, "Fork and upstream review". Development 2153 now includes exact disk text, rack canonical ownership/join wait and retryable trash classification. Rebuild is local; delivery ZIP predates these fixes. Test client hashcodes and filled racks after save/rejoin.

Cleanup follow-up: historical run_2141 needs Session::SetLocalPropPose stub maintenance before its performance/mutation checks can run against current cord_sync. Main check_2152 suite passes after cleanup.

## Current item batch follow-up

The eight requested item areas have source fixes and offline regressions in continued
2153; see WORK.md, Item synchronization batch. Validate all eight on matching current
DLLs: grabs/collision push, both gun effects and contested control, freeze/root scale,
struck panels, crowbar replacement/parents, crate/MRE/foodBox loot, food final bite and
local poison, battery hand/stow/drop/rejoin. Skeletal bone scaling and arbitrary
creature physics remain outside the root-prop lane. Reliable kind 202 is added within
development 2153; existing delivery ZIP does not contain this work.
