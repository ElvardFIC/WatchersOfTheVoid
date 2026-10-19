# TODO - current priorities

Current source/wire protocol: **2153** (continued 2152 entity work), game **0.9.0n**.
Delivery package: **0.9.2152**, containing DLL **b2153 / protocol 2153**.
All peers must install the same package; older protocols are incompatible.

Owner instruction (19 October): continue source/DLL/protocol **2153** in place.
Do not create separate versions during builds without the owner's explicit order.

## Implementation priority - synchronization coverage

Owner instruction: prioritize synchronizing more events and interactions. Bug
fixes remain secondary but necessary when they block or invalidate that work.
Grayboar implementation is deferred at the owner's request; preserve its progress.
Choose an uncovered event/interaction or a missing behavior within an existing
lane, rather than extending defensive retries as the primary work item.

Current candidates from WORK/TESTLOG (report IDs refer to the existing tracker):

| Area | Synchronization still requested | Recorded status |
|---|---|---|
| Airy event (26) | Live Erie resurrection ritual and behavior | Owner confirmed ritual; prop_eriePlush / eriePlushLive located; behavior synchronization remains open |
| Flesh Eerie (40) | Summoning, shared visibility/audio, target selection and attacks | npc_angryErieFlesh identified as a Character; behavior remains open and distinct from zombie Erie |
| Console events (12-14) | Rufus/thiccfus, llama, Madness grunts/variants, AI/damage/effects, canonical command order | Partial text/history/music/HUD paths |
| Sky events (47) and event teleport (42) | Other audio, delayed emails/bookkeeping, pending phases and custom effects | Added native black-hole in/out and alienSounds audio; visual/authority and teleport routes exist; full behavior remains open |
| UFOs and creature families | Spawn, phases/actions, attacks/effects, deaths and mid-event join | Mixed existing lanes and unmapped classes; see WORK taxonomy |
| Demons, statues, rituals and seasonal entities | Native ritual/spawn conditions, targets, interactions and shared lifetime | Family-specific work remains; preserve personal sleep events |
| Shared computer screen (54) | Remaining windows/details/search/category/media, cursor/input and visible operator | Added shared PC/laptop main/media/viewer tabs and ordered shop cart, exclusive-holder handover and late join; two-player checks remain open |
| Water/sponge/grill (17,20,44,45) | Pouring/washer impacts, soap/foam/surfaces, grill lid/cooked outcome, shared consumable transactions | Partial state/visual/ignition paths |
| Gardening (46) | Durable care acceptance/refunds, poured/hose watering, pot changes, remaining sprinkler pickup, keyless pre-session plants and gameplay validation | Added saved-actor enrollment, host growth/state, guest harvest, owner-paid LMB care, completed guest hose rerouting, standalone sprinkler detachment, host faucet toggles/clicks, harvest audio and save-twin recovery |
| Walls/cement (21,22) | Client repair/material accounting, creative construction, curing persistence and effects | Placement/progress/damage paths exist; repair remains refused without spending |
| Physics/gravity guns (9) | Scale/freeze results, non-prop targets, shared ownership and beam gameplay validation | Ordinary-prop movement and both gun visual paths are already present in source |
| Toys, food/packages, coins (15,34,39,57) | Other toy actions/sounds, other package loot, final-bite outcomes, accepted shared coin pickup | Partial implementations; see detailed reports |
| Sleeping body (31) | Visible selected-model body, bed anchor and sleep/wake transitions | Open; native sleepingPawn is camera/input, not a ready body |

Each selected lane includes its gameplay outcomes and late-join state. Existing
source paths or passing offline tests alone do not close synchronization coverage.
The owner confirmed PC/laptop tabs and cart for screen (54), Live Erie ritual
and Meat Erie boss as separate requests. Remaining exact entity mappings require investigation;
do not guess which token UI (52), eyes entity or test actor a report means.

Current implementation checkpoint (19 October): run_garden_sync.ps1 passes seven
executables/four production compiles; run_world_sound.ps1 passes one/three;
run_laptop_ui.ps1 passes two/five. Native contracts pass against assets extracted
read-only from the owner's installed 0.9.0n game, including native cart and tab
side effects. EventCue IDs 1..5 carry sounds; PlacedSave opcode 9 carries faucet/
detach requests; reliable kind 202 carries shared laptop UI. Protocol remains
2153 by owner instruction; every peer needs the same rebuilt DLL. A complete
Release main.dll now builds in build/release-2153. Two-player gameplay is untested.
The candidate table remains OPEN; this does not complete the entire requested list.

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
Live attack/roar/death cues now require the matching active native montage section
after the flag transition and use its actual playback rate. Stopped/wrong-section
montages and failed metadata queries do not generate a cosmetic cue.
Action retries now keep FIFO per recipient and reserve 32 queued cues per peer;
one failed/full peer no longer blocks other peers or their join snapshots.
Client retries rotate entity heads, at most four attempts per tick and one action
per entity, so missing replicas and continuous new arrivals cannot starve others.
Queued cues retain weak actor identities; recycled/remapped replicas discard old
events. Join sampling rechecks peer generation, registry ownership and readiness
after reflected calls. These queue/lifecycle cases pass offline regressions.
Client playback now retains its verification/stop phase after an accepted start;
failed getters or cleanup cannot restart that montage. Live section selection is
verified before audio, and joined playback does not require JumpToSection.
Pending playback validates mesh/animation/asset and replica ownership across
calls; expiry attempts cleanup without extending the queue indefinitely.
Turn interpolation is also bound to the replica identity. Replaced bodies use
neutral steering/pitch until a fresh sample; expired rows free capacity on receipt.
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

Preserved grayboar follow-ups (deferred):
Client boar navigation now refuses move/updMov/path-check/retry/finish bodies and
the verified AI continuations 15/543/558/966/1016/1381. senseUpdate/saw/setWalking
cannot restart client helper decisions or mode changes. Each client body tick checks
the owned helper's current path and aborts Waiting/Paused/Moving navigation,
including late possession and adopted/held copies. Cosmetic turning now follows
the host AnimBP scalar; its visible result and native task/delegate lifecycle
still need two-player validation. This is not full helper synchronization.
1. Deferred: grayboar remaining join/audio/timing work and client helper/turning coordination.
   Live attack/roar/death/hit cosmetic cues are implemented;
   two-player playback and timing still need verification. Preserve native host targeting,
   damage ownership and corpse/loot authority; do not replay loot/damage as cosmetics.
2. Host/client tests for spawn, target, movement, action, damage, death, reconnect
   and mid-event join. Personal Insomniac/nightmare conditions remain peer-local.


## Secondary fixes and validation

- Run the ten priority gameplay checks in WORK.md on matching peer builds.
- Ragdoll: selected authored rig, entry/get-up, native local body and exact limbs
  remain gameplay follow-ups.
- Bench58: record exact item/authoring peer and logs for simple-placement loss;
  birth/save retry code does not prove the original symptom resolved.
- Token52: identify the exact counter/UI before changing balance code.
- Wall21/22: atomic cement request/result accounting across hand/slot/drop/retry/
  disconnect; preserve the existing no-spend refusal. Wall41 remains excluded.
  Host construction now deduplicates active and queued wall/material pairs,
  limits draining to eight feeds per tick and refuses recursive drains. Deferred
  feeds recheck component/world/actor identity; stale session/role/world callbacks
  cannot resume spending as offline gameplay. Atomic personal repair is still open.
- Full backlog and report status: [WORK.md](WORK.md), [TESTLOG.md](TESTLOG.md).

## Current verification and boundaries

Current checkout: origin/watchers-of-the-void-2150 at 0dce3afed, continued on
codex/continue-2153. All recursive build submodules are checked out at their pins.
run_entity_sync.ps1 passes five regression executables and compiles twelve
production units with MSVC 14.50.35717 / Windows SDK 10.0.26100.0.
Coverage includes per-peer queue saturation/order, fair client retries, actor
replacement and peer departure during join sampling; two-player checks stay open.
Expanded checks cover accepted-playback retries, no-op/failed seeks, cancellation,
mid-call replacement/authority loss, reentrant receipt and turn-state replacement.
Evidence: build/entity-sync-check/validation.log. Local environment setup:
`. ./build/local-msvc.ps1; ./agent/tests/run_entity_sync.ps1`.
The runner also accepts an already initialized x64 Developer PowerShell when
the maintainer's sibling _toolchain/env.ps1 is absent.

Wall checks: run_2168.ps1 -CompileSources passes five executables and five
production units, including wall_build_sync. Evidence: build/wall-validation.log.
Placement/record coverage: run_2166.ps1 -CompileSources passes four executables
and five production units; evidence: build/wall-placement-validation.log.
These checks do not verify two-player building.

The previous workspace's nine-runner/package results remain historical evidence
in WORK/TESTLOG. This checkout now has a restored local CMake/Ninja/vcpkg build
environment and a freshly built Release main.dll. The build inputs came from
the owner's existing MultiVoid+.zip; game resources were extracted read-only.
Release scripts distinguish package 2152 from DLL/protocol 2153.
Offline regressions do not establish two-player gameplay correctness.
Live grayboar cues use host-only reliable kind 200, cosmetic turn state uses 201
within development 2153. Install
matching current DLLs on both peers; the existing delivery ZIP predates this work.

The upstream merge is committed; this checkout has no merge in progress. Current
source/test changes remain uncommitted. Preserve all work. Commit, push, game
installation and publication require an explicit request. No other-agent workflow.
Update TODO/WORK/TESTLOG in place; detailed prior evidence remains in WORK/TESTLOG.
