# TODO - current priorities

## Development b2185 ? immediate-join regressions and 2?32 seats

The owner's new 2157 reports remain open for gameplay acceptance: player
flashlights, light switches, garage/base doors and the client's drone contents.
Implementation, native boundaries and checks are in [SYNC_2157_FIXES.md](SYNC_2157_FIXES.md).

- [x] Increase the shared script-name watch budget while retaining the sparse
  lookup load; exercise 1100 pending/resolved watches and repeated reopen cycles.
- [x] Initialize flashlight sampling independently of observer readiness; retry
  incomplete remote applies and preserve newer payloads through callbacks.
- [x] Retry container join seeds to the exact occupancy, across transport and
  class/component/content readiness; do not satisfy them through another peer.
- [x] Add 2?32 player selection to both hosting UIs and enforce it at admission;
  extend acknowledgements, recipient masks, held-food addressing and join arrays.
- [x] Review reachable fork default branches and the 137 upstream commits:
  [FORK_AUDIT_20261022.md](FORK_AUDIT_20261022.md). No upstream merge or remote push.
- [x] Full b2185 qualification: 67/67 groups, stable production snapshot and Release
  identity/ABI pass in build/sync-2157-qualified/verification.json. Recorded in TESTLOG.
- [ ] Accept the reported lanes and larger groups in ordinary rendered multiplayer
  using identical b2185 DLLs. Offline tests do not close these reports.
- [ ] Update the deployed master before automatic lobbies above four seats;
  the local master source accepts 2?32. No remote service was deployed here.

The published package statements below describe b2184, not this development DLL.


## Current release 0.9.2157 / b2184 - package-label correction, 22 October 2026

The menu and server browser now show Watchers of the Void 0.9.2157 from
package/release.json. CMake regenerates the label when that metadata changes;
game target 0.9.0n and wire protocol 2184 remain separate compatibility values.
The packer requires matching runtime formatter evidence and verifies the label
again in the freshly unpacked installation's startup log.

PASS Release build, actual shared DisplayVersion formatter at startup, ABI drill,
one impaired LAN run per direction (Scientist and Kerfur, selected articulated
mesh, entry and recovery; 250ms lag,40ms jitter,5percent send loss), and isolated
startup of the full 32-member ZIP. Production/build inputs frozen:2646.
Evidence: build/version-2157-check/verification.json and final logs;
build/package-current/verification.json with archiveStartupVerified=true.
DLL SHA256:fa77a7c20c685840579e452191bc7f5f098152a0aa5f9dc961097392161f2e87
ZIP SHA256:ab9f59c44bf8cc3dc4f2761559c222a1a9e4a4ce6f35cc621d4417bb75e525f8

The gameplay, ragdoll physics and wire implementation are unchanged from the
preceding qualification below. Its complete physics/model/camera matrix was
not rerun on this label-only rebuild. Tests use NullRHI; native widget text
readback and an ordinary rendered/input check are not claimed. The full65 suite,
existing16 fact-row findings and script-name watch capacity remain open.
Entity, original game files and the normal installed profile are untouched.
The same 0.9.2157 filename replaces the previous ZIP; authorship is AquaFishhh.

- [x] Match the in-game public version to package 0.9.2157 and replace its archive.
- [ ] Owner rendered/input acceptance of the displayed label and ragdoll changes.


## Preceding ragdoll qualification 0.9.2157 / b2184 - 22 October 2026

This supersedes the b2183 status below; package number and filename stay unchanged.
Selected-character physics now starts in the local native FinishSpawningActor
post-hook, before camera attachment and native impulses, without the polling pause.
At native recovery attachment, preserve the camera world pose and hand current yaw
to the capsule/controller once, preventing the late return to the pre-fall heading.
Native fallImpulse, physics and get-up animation remain the owners of movement.

- [x] Remove the selected-model polling pause on ordinary ragdoll entry.
- [x] Preserve recovery heading through native get-up and subsequent turning.
- [x] Verify immediate physics, momentum, models and impaired two-player display.
- [x] Replace the full 0.9.2157 ZIP and verify a fresh unpacked startup.
- [ ] Owner's rendered/input check of entry smoothness and recovery.
- [ ] Investigate existing script-name watch capacity exhaustion during startup.

PASS final exact-DLL native entry/recovery8/8 (instant selected simulation,
completed wakeup, no delayed yaw drift and subsequent turning), momentum8/8,
camera10/10,blocking dynamic-platform contact24/24,installed models30/30 and
remote sleep/sit/ragdoll transitions36/36. Impaired LAN3/3 per direction passes
with250ms lag,40ms jitter and5percent send loss; production construction hook
fires on all six entries without a manual watcher setup in the peer driver.

PASS focused native/skin/retry/stream, shared-screen C++ and personal-sleep
regressions; four native correction-removal controls, stream old-behavior controls,
ABI drill and18 reliable-routing negative controls. Both new camera dependencies
match their live-captured references; the fact-row gate retains16 baseline
violations and gains none. No new complete65-group pass.
The legacy external toolchain/private laptop corpus is unavailable here.
Production/build inputs frozen:2645; tested DLLs identical.
Evidence: build/ragdoll-transition-check/verification.json and final logs.

Full32-member ElvardFIC-WatchersOfTheVoid-0.9.2157.zip passes original branding,
models, loader layout, byte comparisons and actual isolated unpacked startup.
DLL SHA256:7bf4a68037a384605ae54fd9c8939564ba531f274d9b94fb6fc5f99617f647d2
ZIP SHA256:cf0c122bb787876eb62e13a01f4896bc94468622460f4c559e4a89405f30955a
Package proof: build/package-current/verification.json; archiveStartupVerified=true.

The first impulse observer missed the first spawn's watcher registration; fix the
acceptance setup before the native call. An artificial movable platform also
retained StaticMeshActor's WorldStatic channel, triggering native camera wall
travel on recovery. The final fixture explicitly verifies WorldDynamic and keeps
all physical responses blocking. No native wall-detection suppression was added.
The surface driver now uses ordinary C-key recovery through completed native
animation, rather than ForceWakeup, whose immediate reset could snap a sliding
head across scenery. A world change aborts the matrix instead of posting stale
pawn tasks. Failed intermediate runs remain in build/ragdoll-transition-check/; they are not
release evidence. Static terrain beyond the tested native ground remains open.

Native test logs also expose the existing 768-slot script-name watch capacity
being exhausted during world startup. Keep that registration-capacity issue open;
this change does not enlarge the registry or claim the other refused lanes fixed.

Entity and the normal installed profile are untouched. Owner rendered/input
acceptance, other terrain/vehicles, native bed/chair and uninstalled rigs remain
open. Branch updates retain AquaFishhh authorship and package0.9.2157.

## Historical release 0.9.2157 / b2183 — 22 October 2026

This status supersedes older package/build statements below. Existing open gameplay
and implementation tasks remain open unless explicitly accepted.

Integrated main 2e104004b with the selected-character native ragdoll changes.
Preserved upstream computer leases/desktop edits, balance retries, event cues,
container births and lifetime guards. DLL/protocol2183 includes both wire changes.
Public package version is 0.9.2157 at the owner's request; 2158/2159 below are
historical local iterations, not the current delivery.

PASS Release and focused native/skin/retry/stream, shared-screen C++/sleep,
events, balance, placement, trowel, water/material and personal-sleep groups;
ABI and reliable routing with all18 negative controls. The legacy full runner
cannot complete with the absent external toolchain/private laptop corpus.
No new complete65-group result is claimed.

PASS actual NullRHI game momentum8/8,camera10/10,surfaces24/24,catalog30/30,
remote sleep/sit/ragdoll transitions36/36 and impaired LAN3/3 per direction
(250ms outbound lag,40ms jitter,5percent send loss on both peers).
Frozen production/build inputs:2644; test DLLs identical.
Evidence: build/release-2157-check/verification.json and final logs.

Full32-member ElvardFIC-WatchersOfTheVoid-0.9.2157.zip passes package identity,
original icon/models/authors/loader checks and actual unpacked startup.
DLL SHA256:c4237cc74aed8932873bf1a6f6c7bb35d47a8e4566b1278135641ef9f2c78f9a
ZIP SHA256:dde82f9435b31896cef5fc43b4b385dbca471b62194781b4e115aad1c5c5d9b7
Package proof: build/package-current/verification.json; archiveStartupVerified=true.

The first integration candidate included a superseded pre-mesh collision/all-body
shutdown. Offline setup checks passed, but actual momentum fell to6/8 and camera
entries rose over50m. Keep that rejected run in integration-first-failed.log.
The final build retains the previously qualified staging order: pause component
simulation, place/rebuild the selected mesh, then stop its new per-body simulation
until warmup finishes. Both the ordering regression and all native matrices pass.

Normal installation/profile and Entity remain unchanged by this release work.
Owner handles rendered/input acceptance, especially local bed/chair interaction;
uninstalled rigs, other terrain/vehicles and more extreme outages remain open.
Both requested branches are intended to carry this exact release tree, preserving
prior main/fish history and AquaFishhh commit authorship.

## Historical work and qualifications

Current development source/wire protocol: **2182**, game **0.9.0n**. Last full qualification recorded:
**b2178**, **65/65 offline validation groups**; evidence:
`build/continuation-check/verification.json`.
Gameplay acceptance requires identical current DLLs on all peers.
Last prepared complete manager package **0.9.2159 / DLL b2182**; package verification:
`build/package-current/verification.json`. It supersedes the incomplete DLL-only
ragdoll archive; original fork name/namespace, icon, models and loader layout restored.
b2178 adds prop-destroy and location-event lifetime guards. Focused
regressions and full stable-source qualification pass in `build/continuation-check/`.
The final 2155 archive includes these changes and supersedes the earlier local b2177 package.

Development **b2179** persists exact-identity held water/material stock before
native hand reconstruction, corrects local ragdoll setup on C and prevents shared
screen repaint from restoring the computer's boot page. Full 65/65 offline
qualification and stable Release build pass; DLL SHA256:
`5a92cbeda18433d3a3dde41494ddf75a2e8f0979c7698136f1fe97af53271332`.
These changes are unreleased and are not in package 2155. Multiplayer acceptance,
key mismatch repair and paid transactions remain open.

Unresolved implementation, investigations and gameplay checks remain here.
The fish integration and its concrete behavior changes are recorded in WORK.md.
Details and original report IDs: [WORK.md](WORK.md); verification history:
[TESTLOG.md](TESTLOG.md); native entity mapping:
[ENTITY_COVERAGE.md](ENTITY_COVERAGE.md).

## Ragdoll stream/contact robustness — b2182, 22 October 2026

- [x] Reproduce cadence starvation and loss of limb batches between game ticks.
- [x] Rotate full-rig samples at actual sends; merge newest received values per
  bone, bound storage/draining, reject old sequence/generation data.
- [x] Focused stream regression with old-behavior controls, loss, cadence,
  coalescing, reset, reorder and sequence wrap.
- [x] Candidate native surfaces24/24 and remote state transitions6/6; candidate
  bad-link LAN repeated3 rounds per direction (250ms lag,40ms jitter,5% loss).
- [x] Final exact-DLL qualification: momentum8/8,camera10/10,surfaces24/24,
  catalog30/30,remote transitions36/36,impaired LAN6/6; complete2159 startup.
- [ ] Owner's normal rendered/input acceptance (owner handles it).

## Native ragdoll entry momentum — b2181, 22 October 2026

- [x] Reproduce loss of the delayed native fallImpulse through physics rebuilding.
- [x] Preserve native per-bone velocity commands before/during/after the swap,
  initialize new limbs, retain the chest kick and reject stale-body replay.
- [x] Native offline regression and disabled-hold control; candidate native 8/8
  walk/run/diagonal/airborne cases on scientist and Kerfur.
- [x] Assess stock five-body display-only retargeting versus a compatible full
  native physics asset. See RAGDOLL_DESIGN.md; no alternative claimed implemented.
- [x] Qualify exact final b2181: eight momentum cases, ten camera cases and
  selected articulated LAN display in both directions; frozen-source evidence
  build/ragdoll-momentum-check/verification.json.
- [x] Complete 0.9.2158 package archive/startup verification:32 members, original
  icon/namespace/models, qualified DLL, HEALTH PASS, skins4/4, enabled loader.
- [ ] Rendered running/input acceptance and investigate the earlier intermittent
  partial LAN acceptance if reproduced (final diagnostic run passed).
- [ ] Optional future rigging work: authored/cooked player PhysicsAsset or a
  separately validated stock-body display retargeter, after choosing its tradeoffs.

## Player ragdoll camera — b2180, 22 October 2026

- [x] Preserve entry view when replacing the plushie head with the selected
  skeleton. Rebase the camera arm through warmup/physics initialization, then
  retain native head following and recovery. Guard reparenting/recycled slots.
- [x] Add native camera regressions and a control that fails with correction
  disabled. Check level/up/down views, crouched entry and fainting on scientist
  and Kerfur in ten isolated actual-game transform/physics cases.
- [x] Retain launch and articulated remote-body fixes; qualify the final b2180
  DLL and complete 0.9.2157 package. Evidence: build/ragdoll-camera-check/ and
  build/package-current/verification.json. NullRHI checks do not cover rendering.

## Critical player ragdoll hotfix — b2179

- [x] Fix remote body teardown caused by reading a return value from the void
  Actor attachment function; verify the resulting parent relationship.
- [x] Remove plush-pelvis lowering, initialize selected-model geometry before
  physics, and correct the common rig's separated chest joint at instantiation.
  Live Kerfur entry/fall/rest/recovery passes (maximum pelvis rise 22.5 cm).
- [x] Preserve the selected model/materials and articulated limbs; add native
  regressions including restoration of shared physics templates after failures.
- [x] Real LAN acceptance of complete streamed limbs in both directions:
  scientist client -> host and Kerfur host -> client; entry and recovery pass.
- [ ] Broader Bodies checklist below: uninstalled rigs,other terrain/vehicles,
  more extreme links and ordinary local bed/chair interactions. Controlled surfaces,
  bad-link LAN,installed30 models and remote transitions are qualified in b2182.

The older package 2155 contains b2178. Use complete package 0.9.2157 on every peer
when checking the historical camera change; use current 0.9.2158 for momentum.
The old 65/65 record does not qualify the new DLL.

## Imported main history (superseded by current release status)

## Status and next steps - 22 October 2026

- [x] Shared PC: add host leases and ordered revisions, seed a new operator,
  capture the last desktop edit before releasing the device, retry partial/refused
  sends, and cancel stale work through world/session/occupant changes. The boot
  isolation fix remains. Protocol 2180 is incompatible with the earlier screen
  payload; all peers need the current DLL. Host/client gameplay remains open.
- [x] Remote ragdoll: translate cached partial bone batches with the current pelvis
  so a teleport cannot leave limbs at old coordinates; keep the standing fallback
  visible until simulation succeeds; reject invalid poses and recycled bodies.
  Retry, selected-skin and personal-sleep regressions pass. PhysX acceptance is open.
- [x] Event cues: retain star-rain playback through missing local pawn/assets/frame
  dependencies, retry each recipient, and validate source/recipient/world/session
  lifetime. Ambiguous attempted native spawns are not duplicated. This does not
  complete all eight event categories or active-phase particle alignment.
- [x] General balance (probable Token52): acknowledge delivery only after an
  accepted per-peer send; retain join/refused-order corrections and failed HUD
  repaints, preserve newer incoming values and cancel stale deferred credits.
- [x] Container contents: restore the three missing birth-slice call sites for
  accepted client placement, host reservation before record restoration and host
  birth publication. Regression cases cover refusal, duplicates and teardown.
- [x] Fact rows: connect seven live watches to their existing typed rows and remove
  23 unused declarations. All 30 unused-row findings are resolved; 15 missing
  native reference entries remain. The gate/baseline was not relaxed.

The sections below separate implementation, investigation and gameplay acceptance.
An unchecked gameplay item can already have implemented and offline-tested code.
The complete report tracker is in WORK: IDs 1–58 except closed Mining (6) and
owner-excluded Wall41; duplicate reports 11/36 remain linked.

| Status | What remains |
|---|---|
| Investigate | Reproduce the host's old-rifle disappearance (58), accept the probable shared-balance Token52 correction, and verify trowel reconstruction/key continuity. |
| Implement | Atomic resource transactions and paid repair; concurrent resource writers; remaining event/entity behavior, food outputs, grime sources and explosion follow-ups. |
| Accept in game | Prioritize base-PC loading, final screen edits/operator handoff, general balance and all-role C-key ragdoll on matching b2180 peers, then run the gameplay/entity checklists. Package 2155 still carries b2178. Record results in TESTLOG before closing reports. |
| Validate | Full b2180 qualification is running. Fact-row findings are down to 15 missing references across 502 rows/157 files. Full prose results are recorded in TESTLOG; offline qualification does not establish full CI or gameplay acceptance. |
| Deferred by owner | Fresh game-pak/Blueprint extraction and audit; use existing evidence without claiming complete native coverage. |

- [x] Implement held-stock persistence for water/soap/alcohol, concrete units/dry/
  timer, cement-bag units, sponge wetness/soap and trowel concrete (b2179).
  Commit existing numeric groups together after identity, shape and stock checks;
  preserve array ownership and refuse mismatches. Gameplay acceptance is open.
- [x] Correct ragdoll setup ordering (5/50): disable collision and all old bodies
  before changing mesh/physics asset; align the warmed pose to the selected
  model's standing/crouched entry pelvis before restarting simulation. Preserve
  the anchor across retries and reject changed pawn/body/model identities.
  Offline regressions and the b2179 full qualification pass; the
  owner's all-role C-key launch report remains open for gameplay acceptance.
- [x] Keep native PC boot/power UI out of shared-screen snapshots (54/55).
  Capture and repaint only the desktop subtree; an old snapshot can no longer
  restore the boot index after native startup completes. Production regression
  and existing native-contract check pass; host/client gameplay remains open.

Start with reproducible failures and resource identity/transaction gaps; then
complete missing native behavior routes and accept them on real peers. Evidence
collection can proceed while implementation continues. The local package is
already prepared; build/pack work is conditional on further source changes.

## Event synchronization request - OPEN

- [ ] Complete verified native routes for Dreams, Ariral Reputation,
  Player-Activated, Time-Related, Endings, Story Mode, Random and Signal events.
  Every event needs its real trigger, chosen outcome, effects and join/lifetime
  checks. WORK's event coverage table includes dream wake/rewards, reputation
  tiers/relocation, simultaneous activations, calendar/3AM branches, per-player
  ending consequences, active story phases and signal processing/playback.
- [ ] Extend frequency coverage beyond the five verified Fossilhound, killer-wisp,
  sky-wisp, night-egg and owner-local night-eyes generators and the peer-symmetric
  firefly route to the complete repeatable random-event list. Night eyes now
  preserve native night/day and 5% chance checks; preserve personal
  dreams/Insomniac and story dates.
  `treeSpawner`, `bp7Spawner` and `flickerer` still need complete authority/effect
  routes before admission. Change eligible-roll waits, preserving eligibility,
  probabilities, AI/animation, effect durations and sleep needs.
- [ ] Fresh game-pak/Blueprint audit remains deferred by the owner.
- [ ] Real-peer event, frequency, late-join, reconnect and simultaneous-trigger
  acceptance on identical current DLLs, including repeated/chosen events,
  live/persisted frequency settings, load windows and teardown.

## Investigations requiring runtime evidence

- [ ] Bench58: the owner confirms **the host's old rifle** disappeared after
  simple placement (21 October). Earlier components remain unidentified.
  Native candidate: `prop_rifle_C`; its leaf Blueprint has destructive LMB use,
  without a placement entry point. Inherited behavior/cause remains unverified.
  Reproduce with `VOTVCOOP_PROP_BIRTH_KEY_PROBE=1` and capture class/key, destroy
  caller/timing, `[PROP-DROP]`, `remote_prop::OnDestroy` and sweep logs.
  Client placement exhaustion recovery is implemented separately; it does not
  establish a fix for the host report.
  Development b2178 also prevents a received destroy from using an actor replaced
  during listeners/native grab release and adds role/sender/class/key diagnostics.
  This reproduces a code defect offline; the owner's disappearance still needs runtime evidence.
- [ ] Token52: clarified on 22 October as probably the general balance. Accept
  shared `saveSlot.Points` after join/rejoin, credit, spending and refused orders
  on both roles. The balance-delivery defects now have production regressions;
  the exact original UI/reproduction is still not confirmed.
- [ ] Trowel key continuity and stock across reconstruction/unequip/drop:
  identity mismatch defers the profile. Coordinate the repair transaction with
  native `mainPlayer.updateHold` reconstruction from `saveSlot.hold[0]`.
  Exact-identity concrete changes now persist into the existing `hold[0]` scalar
  before reconstruction, including zero; mismatch repair and runtime acceptance remain.

## Remaining implementation

- [ ] Console/events (12–15, 26, 42): complete Rufus/thiccfus, llama, Madness grunts
  and variant AI/damage/effects; verify the imported host-ordered submitted/periodic/clear console output,
  Madness playback recovery and Maxwell phase. Other funny-rule HUD/events and
  remaining toy/squeeze sounds still need coverage.
- [ ] Cement/walls (21/22): atomic repair request/result with exact personal stock
  across hand/slot/drop, retries, rejection and disconnect; creative construction and persisted
  wet-wall timers now have imported controllers; verify native impact/effects. Keep no-spend refusal until
  the transaction is complete.
- [ ] Water/cement/coal (20/21/44/45): arbitrate concurrent writers and stale
  in-flight records, dryTimer ownership and personal-hand transitions. Imported
  stock persistence, sponge foam, ordered grill coal/lid edits and cooking-progress
  carriers need gameplay acceptance; final output/accounting gaps remain.
  b2179 adds immediate held water/material persistence before reconstruction;
  this does not arbitrate cross-peer writers or complete spending/refunds.
- [ ] Personal resources: finish debit, output acceptance and refund as one
  transaction; cover rejection, retry, disconnect and hand/slot/drop transitions.
  Imported stock/profile persistence alone does not complete these transactions.
- [ ] Grime/scorch: cover remaining dynamic grime sources and preserve explosion/
  salt-rocket scorch provenance through a full save/world reload. Same-world
  reconnect coverage does not establish persistence across world reconstruction.
- [ ] Sky event (47): verify the imported host ownership of audio, delayed
  email/bookkeeping and all six pre-session latent continuations. Transport
  presentation phases/cues for clients and other custom effects/creatures.
- [ ] Packages/food (34/39): audit remaining package subclasses and eaten() output;
  require explicit client-local loot provenance. Preserve final-bite delivery and
  eater-local hunger/poison; avoid global replaceProp admission.
- [ ] Stow performance (16): capture frame times if stutters persist, including
  unresolved-class lookup paths; preserve pending item-state restoration.
- [ ] Models (38): fill remaining preview gaps and verify custom skin compatibility
  without removing rig exclusions. Assess skeletal bone scaling and arbitrary
  creature physics separately if required.
- [ ] Explosion follow-up: assess and implement host tracking/coasting of affected
  props through native delayed impulses; validate held/frozen/static exclusions,
  blast ownership/range and settled poses on two peers. Retain local explosion
  lanes 188/191; upstream ExplosionSpawn=177 conflicts with KitchenState=177.

## Entity coverage checklist

Use the exact native mapping and per-class gaps in ENTITY_COVERAGE. For every
group, finish missing behavior transport and verify spawn, target, motion,
actions, damage/death, local effects, disconnect/reconnect and mid-event join.
Keep personal Insomniac/nightmare and native personal spawn conditions local.

- [ ] Grayboar: remaining join/audio/network-time coordination; verify locomotion
  and movement-loop audio, host target detection, visible turn/pitch, helper task
  retirement/delegates and adoption/held-drive handoffs. Test repeated/simultaneous
  hits, playback failures, death timing, client weapon damage, single corpse/loot
  and host retirement. Verify collision damage, lightning replacement propagation
  and charged explosion/timer/punch/spin effects.
  Check damage under saturation and directional hits; investigate independently
  spawned client twins if reproduced.
- [ ] Rozitals: Great Pyramid, Kavotia, Pagonioplo-Igetis, Scouts, Soltomia —
  finish action/effect adapters and verify phases/actions/destruction/join.
  Kavotia still needs fire/laying montage, speech, canonical beam/impact/damage
  and exact phase-time alignment.
- [ ] Antibreather / Fossilhound: player/cave-dependent Antibreather spawning
  and cross-peer attacks; verify Fossilhound ticker spawn and attacks in game.
- [ ] Arirals: follower/shooter/pigBeater and gun-stealer gameplay, including
  stolen/held/thrown-item ownership, attachment, shots/hits and event phases.
- [ ] Greys / Flamethrower Tank: grayTest targets/actions/effects; firetank turret,
  fire, fuel, damage and destruction.
- [ ] Geoms: Ramiel (`geomOcta_C`) movement/charge/beam/audio/damage and
  Egg-shaped Geom (`eg_C`) retrieval/landing/special actions.
- [ ] UFOs: child spawns, beam/material/audio phases and late join for Boomerang,
  Joil, Grey Ship, Midas, Pill, Trifo and Orbs. Grey Ship display-name split needs
  a live probe.
- [ ] Deer / Fish / Cockroach: deer/deer_ded spawn/death/rejoin, caught
  `prop_fish_0..21_C` variants/inventory, roach movement/eating/crushing/late join.
- [ ] Blue Brain Cheese, Restocker, Eye, Wolfgang, Tree with Eyes, Living Mushroom,
  Space Jellyfish: finish exact behavior routes and effects. Wolfgang is
  actor_save-derived; do not identify every eyes entity as eyer.
- [ ] Bone Man, Bloody Skeleton, Curious Eyes, Mimic, Flesh Slug, Zombie Deer:
  attack/effect/lifetime checks, unresolved display-name associations, mimic shared
  rendering/disguise/damage and centipede visibility (35/48).
- [ ] Ghost, Ghost Log, Fireflies, Walking Mannequin, Black Spot: unresolved
  names/effects/phases and lifecycle gameplay. Verify wisp animation readiness and
  delayed grab/release cancellation through pawn replacement, death, world changes
  and reentrant cleanup.
- [ ] Demons: SKerfur-O, Eyeless Insomniac, Insomnia Demon, Abandoned Kerfur,
  Flesh Erie, Goat Statue, Stolas Statue, Furfur — individual targeting,
  rituals/run-end behavior. Keep distinct live/zombie/angry flesh Erie classes.
- [ ] Forbidden: Black Friend, CorpseWalker, Crasher, Fast-shadow Figure, Fridge,
  Interloper, The Exit Door, The Head, Pool Entity, Mountain People,
  Cursed Spider, Rune Sphere, Shadow Figure, Fake Kel, Black Sphere —
  finish missing mappings, spawn availability and local/global effects.
  Do not enable debug/test actors in normal play to populate this list.
- [ ] Living Erie / Girufus / Rufus: unresolved Girufus mapping and distinct
  interaction/animation/damage/ownership/lifetime behavior.
- [ ] Gnomes / Krampus: group spawn, presents, revenge, interaction, damage/death
  and shared lifetime; Krampus attack/fireball/jump/target/despawn events.

## Gameplay acceptance checklist

Use matching current **b2182** DLLs for ragdoll acceptance.

## Imported main history (superseded by current release status)

Use matching development **b2180** DLLs after qualification for this continuation.
Package 2155 contains **b2178**, and the existing 2156 preparation targets b2179;
neither contains the current shared-screen protocol, balance or container fixes.
Record role, item/model, reproduction and logs.
Offline tests do not close gameplay reports. Mining (6) stays closed; previously
working nightmares (10) only need acceptance of the newer personal-sleep behavior.
Wall41 remains excluded.

- [ ] Item batch (2/3/9/25/27/29/34/39): grab jitter and contested control;
  actual collision push/convergence; both gun effects and freeze/root scale;
  struck panels; crowbar replacement/parents; crate/MRE/foodBox loot; final bite
  and local poison; battery hand/stow/drop/rejoin.
- [ ] Placement: refusal/recovery and late PropSpawn adoption with two peers,
  device ejects, containers and live food/drive state.
- [ ] Coins: contest one coin via E/overlap on both peers; one host credit,
  accepted mirror removal, refusal retention and client-local map coins.
- [ ] Props (1/4/7/8): client breaking, R+LMB duplication, extinguisher foam and
  loose-outlet floating. Keep outlet floating distinct from appliance connection.
- [ ] Water/surfaces (17–20/44): pressure-washer stream/audio and host impacts,
  pouring/drinks/fuel, computer water short-circuit sparks/damage/shutdown,
  canonical grime/cleaning, quiet joins and final-state retries. Verify scorch
  lifetime/reconnect separately from the unresolved full-reload provenance.
- [ ] Inventory/containers (23/24/28/32/33): exhausted and cranked flashlight on
  both peers; anchored-hook retirement, one ordinary pickup, retry and late join;
  drone audio; all equippable items with full inventory; simultaneous container
  removal and row refresh on the other peer.
- [ ] Chairs (11/36): rotation and transitions to laptop/sleep/ragdoll.
- [ ] Cremator: host-authoritative door and paint-only lever on both peers;
  measure door FPS against the reported performance problem.
- [ ] Personal sleep (10/49): independent nightmare settings, different beds,
  chance/shared night interruption, simultaneous low sleep and local demon spawn,
  visibility, targeting, grab/attack, wake/release, reconnect and rare Child variant.
- [ ] Bodies (5/31/38/50): selected/common/custom rigs, authored limbs, local body,
  entry/recovery, T-pose, drift, retry/fallback and streamed remote pose; slopes,
  moving props/walls, high latency and teleport. Bed mattress/exhaustion anchors,
  moving beds, sleep/sit/ragdoll precedence and sleeping effect suppression.
  First verify the 21 October C-key launch report on host and client: standing,
  crouched and repeated entry/recovery, floor/slopes and each selected rig.
- [ ] Electrical/light/computer (51/53–55): short/long cables, extensions,
  occupied/unpowered outlets; actual light keys, overridden setters, component
  readiness, flicker/load/join timing; shared laptop/portable-PC tabs, text,
  scroll, visibility, cursor/input/activity and operator body. ON/OFF during
  animation, power loss/restoration and reconnect.
  First verify the 21 October stationary base-PC infinite-loading report on
  both roles, including entering/leaving during boot and repeated power cycles.
- [ ] ZIP/cans (56/57): actual inserted/ejected ZIP class/model/scale on both
  peers; cans open/eat/final portion in hand/world, stow/re-equip, drop/pickup,
  simultaneous use and reconnect.
- [ ] Safe (43): dial/hacked completion, one loot result, persistent lock state,
  retry/stale-record handling and save/rejoin.
- [ ] Garden/sky (46/47): pre-session save/reload, care/harvest, simultaneous use
  and late join, pot state and accepted faucet/hose rerouting; shared schedule/
  effects for the black-disk/yellow-ring event.
  Missing historical garden save actors are not migrated.
- [ ] ATV (30/37): new companion spawn versus saved second-ATV positions,
  simultaneous carry/mount, stale menus and race convergence.
- [ ] Disks/racks: client sv.request/hashcodes, floppy/ZIP transfers preserving
  exact text, filled rack save/rejoin and a slow joining peer.
- [ ] Whole-package smoke: swap host/client roles; two clients contest objects;
  late join/rejoin/disconnect/save/reload; partial/empty consumables, full inventory,
  equipped items/container rows, loose cables, flashlight crank/low battery/previews.

## Validation follow-ups

- [ ] Resolve the full public-prose and fact-row CI findings recorded in
  TESTLOG/HANDOFF_FISH, including missing live native-hook references; rerun the
  full gates and record current results. The earlier selected-file prose sweep
  and passing include-context checks do not establish full repository CI.
- [ ] Complete the native-contract audit when the owner resumes the deferred
  asset audit. The existing corpus is incomplete for the imported suite.

## Release preparation

Package 2155 build/pack/verify scripts validate b2178, 65 passing groups, the
unchanged production snapshot and embedded/delivery hashes. Requalify after source
changes; archived 2154 scripts describe the older b2156 artifact.

Commit, push, game installation and publication require an explicit request.
No other-agent workflow. Update the active tracking documents in place.
