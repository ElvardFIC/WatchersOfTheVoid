# WORK — unfinished implementation

## Upstream gap review — 29 October 2026

Eight implementation candidates are tracked as UG1–UG8 in
[TODO.md](TODO.md#unfinished-upstreamfork-candidates--reviewed-29-october-2026).
The source limits describe upstream; local inspection below establishes the
missing adapter boundary, not a reproduced multiplayer defect. Recheck the
in-progress upstream merge before implementation.

| ID | Remaining implementation and current local boundary |
| --- | --- |
| UG1 | remote_prop_drive.cpp uses CallBaseGrabPrelude for non-wall-attachable props and explicitly excludes subclass preludes. Add the glowstick's releaseGlowstick behavior with exact native ownership. Garden detachSprinkler already has garden_actions routing; do not assume that entire subsystem is missing. |
| UG2 | prop_stick_sync.cpp::ReplayStick falls back to frozen/static flags and physics-off after a missed trace. PropStickStatePayload carries identity, flags, pose and velocities, without the support identity. Add support/component resolution and destruction cleanup without attaching to a guessed surface. |
| UG3 | prop_stick_sync::ConvergeStuck and prop_wire_parity converge existing flags during other replication paths. No dedicated toolgun setPropProps action route was found. Add live action authority and final state delivery, preserving native freeze/static distinctions and current hold guards. |
| UG4 | The upstream laptop stabilizes its seat locally. No stabilizeSeat/occupied-chair reservation route was found in our coop controllers. Bind protection to the actual chair, operator and laptop lease; clear it on exit, body/world replacement and disconnect. Receiver-side competing-hold arbitration is already present and does not itself reserve an occupied chair. |
| UG5 | No red-phone ring adapter was found in coop controllers or protocol. Capture the native selected ring/cue and its lifetime; host ownership must not duplicate native client rolls. |
| UG6 | No desk virusEvent adapter was found. Dish precision, server state and power-panel replication cover parts of the aftermath, not the native virus trigger and phase ownership. Map the actual graph before defining its shared effects and join policy. |
| UG7 | download_arm_sync replays native begin; no objectRenderer/lifecrystal-specific birth scope was found in coop. Bind any shared signal output to one host-owned invocation across either arm/birth arrival order, including failed construction and replacement. Preserve personal camera/ending branches. |
| UG8 | Upstream #49 requests runtime 3D-print synchronization even with matching local files. No printer-specific output/content adapter was found in coop prop routes. Establish the actual printed class and mesh initialization, then add canonical birth/content/transform/lifetime and late-join handling. This source report does not establish our package's exact failure or require distributing users' files. |

Source links are attached to each TODO item. Existing event, entity, resource,
mod-integration and physics backlogs remain the owners of overlapping work;
do not duplicate them or import gameplay-only acceptance as implementation.

Development target: **0.9.2164**. [TODO.md](TODO.md) lists priorities.
This file contains implementation work and the constraints needed to finish it.
Checks, completed-change reports and release evidence belong in
[TESTLOG.md](TESTLOG.md).

## Client prop physics

## Upstream integration in progress

- [ ] UP1: resolve existing documentation conflicts without dropping either side.
- [ ] UP2: integrate upstream dc4cd7307 (306 commits after ad5315611), retaining
  fork-specific authority, transaction and lifetime guards at overlapping seams.
- [ ] UP3: preserve published kind IDs; widen reliable kind encoding and bound
  kind-indexed tables under a new internal protocol revision.
- [ ] UP4: reconcile cord/coffee and coordinate/radio tower ownership with our
  existing adapters and 32-player capacity.
- [ ] UP5: integrate prop syncer, swinger, bind-wait and join ordering while
  retaining hand arbitration, rest delivery retries and container recovery.
- [ ] UP6: integrate Geom Octa and Vulkan overlay dependencies/lifecycle.
- [ ] UP7: adapt affected regression fixtures to the integrated interfaces and
  run current checks and a fresh development Release build.

## Client prop physics

- [ ] Fix joined-client object vibration, ejection and support penetration on
  boxes, desks and workbenches while host physics appears stable. Trace ordinary
  prop pose application, drive/park/end handoffs and simulation reactivation;
  resolve conflicting simulation/transform ownership and preserve stable resting
  contacts, collisions, pickup/drop and canonical host motion. The report does
  not yet establish the responsible route or whether client displacement reaches
  the host. Current config exposes no dedicated user-facing stack-physics fix;
  do not substitute arbitrary network tuning for a confirmed implementation.

## Player presentation and toys

- [ ] Fix remote flashlights so peers see each other's lights. Continue from
  the existing retained state, replacement replay and same-component reconciliation;
  keep sender/slot ownership and current pawn/light bindings through native calls.
- [ ] Fix the falling-player T-pose. Resolve selected-model pose transfer and
  physics takeover in engine_ragdoll_skin/engine_playerragdoll and the remote
  display; preserve death-free puppet spawning and the standing warmup bridge.
- [ ] Finish held-toy variant schemas and hook/function ownership beyond the
  Dingus/ABPlush capture inputs. Keep PRE/POST provenance and bounded native work.
- [ ] Finish watoy start-cue and physics-hook ownership. Preserve the existing
  activation/audio/emitter schemas, original-hand provenance, shared View ownership
  and presentation budgets. Never replay an uncertain admitted activation.
- [ ] Finish thrown-sponge foam and resource ownership beyond window dabs;
  preserve the original draw's material/size and at-most-once native cleaning.
- [ ] Fix remaining stow stalls beyond indexed save-record class restoration;
  retain pending item-state restoration while classes are unavailable.
- [ ] Fill missing model previews and custom-skin compatibility while retaining
  rig exclusions. Treat skeletal scaling and creature physics as separate adapters.

## Event adapters and generator effects

- [ ] Finish Dreams scene-effect/lifetime adapters while keeping sleep,
  createDream/leaveDream, wake rewards, personal nightmare rolls and Insomniac
  consequences with their owner. Do not synthesize shared reward props.
- [ ] Finish Ariral Reputation state, selected outcomes, relocation and late join.
  Use calcRep's -75/-25/25/75 tiers and the actual tier/mainGamemode listeners;
  launchEvent itself is empty.
- [ ] Implement remaining Player-Activated trigger/outcome/effect routes with
  canonical authority for simultaneous activations.
- [ ] Finish Time-Related routes: 3AM eligibility and mainGamemode calendar
  branches outside eventer, including June 28 21:00, April 1 and November 29 22:49.
- [ ] Separate personal/session consequences for each Ending and implement its
  adapter; do not replay quit/travel on remote puppets.
- [ ] Finish Story Mode actor, phase, audio, bookkeeping and current-state joins.
  Horde/dropper need wave/class budgets, alive sets, countdowns and timeline
  ownership. Keep health/effects personal where native; avoid repeated rewards
  or save work from replaying a start.
- [ ] Finish Random event effects, including shared black fog beyond client
  producer suppression. Keep bp7Spawner's camera/roll/pause/quit owner-local;
  its ten-second eligible-roll scaling is already implemented.
- [ ] Map Signal processing/playback triggers, funny-rule gates and selected
  outcomes, then implement phases/effects and current-state joins.
- [ ] Implement level/map-script routes from the extracted headers. Use
  build/priority-level-scripts (263 headers, 52 scripted maps, 172 functions)
  and build/requested-events-audit; preserve personal and protection/exit branches.
- [ ] treeSpawner: transport the host-selected walking tree's visibility, mesh
  scale, movement and lifetime; finish client producer suppression and late join.
  Then scale only the native 600–900-second spawn wait, preserving mode,
  rendering and proximity gates.
- [ ] serverBreaker: finish protected-floppy consumption delivery to every
  recipient, protection/repair audio and delegate consequences. Retain the
  selected box, native day/difficulty/upgrade probabilities, slot-canonical ordering
  and retry ownership. Then scale only the native 1200–600-second eligible wait.
- [ ] Finish shared sky-event phases/cues, audio, delayed email/accounting,
  six pre-session latent continuations and remaining custom effects/creatures.
- [ ] Finish arirShipVanishing_C and other appearance/event variants beyond the
  arirShipAppear_C phase adapter; include shared reputation consequences.

Frequency changes must preserve eligibility, probabilities, personal dream rolls
and story dates. Scale eligible-roll waits without changing AI/animation cadence,
effect durations or sleep needs.

## Console, items and resource transactions

- [ ] Finish Rufus/thiccfus cross-player targets/grabs, audio/HUD, corpse/loot,
  damage and explosion transport; Madness/variant AI, damage/effects and remaining
  funny-rule HUD/events and toy/squeeze sounds.
- [ ] Finish llama marker/save/event-accounting lifecycle beyond the existing
  PlacedSave marker route and invocation-bound Begin recovery. Keep native host
  contributions authoritative; do not infer latent age from an absent sender or
  replay uncertain timer creation. Refused cleanup needs an owned retry path.
- [ ] Finish atomic paid wall/cement repair request/result and exact personal
  stock across hand/slot/drop, refusal and disconnect. Keep the current no-spend
  client refusal until debit, accepted output and refund form one transaction.
- [ ] Arbitrate concurrent water/cement/coal writers, stale in-flight records,
  dryTimer ownership and personal-hand transitions; finish output/accounting.
- [ ] Finish cross-peer personal debit/output/refund, personal extraction
  overrides/raw takeObj, receipt-less recovery and debt persistence across
  world/session reconstruction. Retain the local base-container escrow/refund
  boundary; never guess a refund or replay an uncertain admitted native effect.
- [ ] Finish remaining package/food subclass outputs, cross-peer portion/output
  arbitration and personal effects before host acceptance. Add native poison-pour,
  roach and reward routes plus uncertain-open refund recovery and persistence.
  Do not admit arbitrary replaceProp. Admission alone is not a committed output.
- [ ] Finish buried-item/treasure transaction authority and save-file persistence;
  mirror construction must not replay native treasure/loot-producing BeginPlay.
- [ ] Cover remaining dynamic grime sources and persist explosion/salt-rocket
  scorch provenance through full save/world reconstruction.

## Entity implementation

- [ ] Grayboar: complete join/audio/time coordination, host targets, turn/pitch,
  helper/delegate retirement, held-item handoffs, weapon/collision damage, one
  corpse/loot, lightning replacement and charged explosion/punch/spin effects.
- [ ] Rozitals: complete Great Pyramid, Kavotia, Pagonioplo-Igetis, Scouts and
  Soltomia action/effect adapters. Kavotia needs fire/laying montage, speech,
  canonical beam/impact/damage and exact phase timing.
- [ ] Antibreather / Fossilhound: complete player/cave-dependent Antibreather
  spawning, cross-peer attacks and Fossilhound ticker/attack routes.
- [ ] Arirals: complete follower/shooter/pigBeater and gun-stealer behavior,
  stolen/held/thrown-item ownership, attachments, shots/hits and event phases.
  Slapper Character classification and exact summoner spawn admission are
  implemented; its attacks, item handoffs and effects still need adapters.
- [ ] Greys / Flamethrower Tank: complete grayTest targets/actions/effects and
  firetank turret, fire, fuel, damage and destruction.
- [ ] Geoms: complete Ramiel (geomOcta_C) movement/charge/beam/audio/damage and
  Egg-shaped Geom (eg_C) retrieval/landing/special actions.
- [ ] UFOs: complete child spawns, beam/material/audio phases and join state for
  Boomerang, Joil, Grey Ship, Midas, Pill, Trifo and Orbs; resolve Grey Ship names.
- [ ] Deer / Fish / Cockroach: complete deer/deer_ded spawn/death/rejoin,
  caught prop_fish_0..21_C inventory and roach movement/eating/crushing/join state.
- [ ] Blue Brain Cheese, Restocker, Eye, Wolfgang, Tree with Eyes, Living Mushroom,
  Space Jellyfish: complete exact behavior/effect routes. Wolfgang derives from
  actor_save; do not map every eyes entity to eyer.
- [ ] Bone Man, Bloody Skeleton, Curious Eyes, Mimic, Flesh Slug, Zombie Deer:
  complete attacks/effects/lifetime, missing name mappings, shared mimic
  rendering/disguise/damage and centipede visibility.
- [ ] Ghost, Ghost Log, Fireflies, Walking Mannequin, Black Spot: complete missing
  names/effects/phases. Finish remaining wisp limb/blood presentation beyond the
  implemented animation-readiness and body/world-bound grab/release cancellation.
- [ ] Demons: complete individual targets, rituals/run-end behavior for SKerfur-O,
  Eyeless Insomniac, Insomnia Demon, Abandoned Kerfur, Flesh Erie, Goat Statue,
  Stolas Statue and Furfur. Keep live/zombie/angry Flesh Erie classes distinct.
- [ ] Forbidden: complete missing class mappings, spawn availability and local/global
  effects for Black Friend, CorpseWalker, Crasher, Fast-shadow Figure, Fridge,
  Interloper, The Exit Door, The Head, Pool Entity, Mountain People, Cursed Spider,
  Rune Sphere, Shadow Figure, Fake Kel and Black Sphere. Keep debug actors out of normal play.
- [ ] Living Erie / Girufus / Rufus: complete Girufus mapping and distinct
  interaction, animation, damage, ownership and lifetime behavior.
- [ ] Gnomes / Krampus: complete group spawning, presents, revenge, interaction,
  damage/death/shared lifetime and Krampus attack/fireball/jump/target/despawn.

## Requested mod integrations

- [ ] EmoteWheel 1.0.1: replace the upstream empty network bridge with owned
  emote/variant/phase/music state, interruption, late join and body/world cleanup.
- [ ] VEmu 1.1.0: define shared console/cartridge/AV/input ownership and emulator
  state/audio/video transport. Upstream AV output excludes SmartTV; an explicit
  adapter is required to support that combination.
- [ ] JeepMod 0.11.1: finish custom trunk/key/container/upgrade routes beyond
  inherited ATV pose/seat admission; preserve one shared inventory and lifetime.
- [ ] PhotographyPlus 0.10.3: implement task/reward/save ownership and photo
  submission routing without duplicating host rewards or another player's photos.
- [ ] SmartTV 0.9.6: implement custom player mode/source/playback/join ownership
  beyond vanilla media routes; keep local device/OBS access local.
- [ ] Audit FireTower 1.0.4 door/ModActor ownership and add any missing shared
  interaction/lifetime routes; retain matching local scenery and collision.

## Tooling and regression maintenance

- [ ] Repair stale supplemental fixtures without weakening assertions or
  mutation controls. The unresolved inventory is in
  build/audit-supplemental-final/results.json; reconcile it with newer results
  before changing a runner. Known drift includes four/32-seat doubles, missing
  includes, changed callbacks/Registry APIs and obsolete source/mutation anchors.
- [ ] Fix the generated run_2126 mutation harness include path and obsolete
  ServicePanel ack-capacity fixture exposed by the fork review.
- [ ] Reduce public-prose findings in documents, src.comment_doc_row and
  src.files_not_swept; restore required reading-room references. Existing gate
  findings and evidence belong in TESTLOG.
