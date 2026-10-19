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

The results below are historical **b2153** evidence, not validation of the merged
**2154** source. Rebuild and rerun checks for 2154 before using those results to
describe the current tree.

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

## Remaining implementation and investigations from WORK

Reviewed again against all sections of WORK.md on 20 October 2026. Newer implementation reports supersede
older "needs implementation" notes for sleeping bodies, shared UI, ragdolls and
food final-bite delivery. Those features still require gameplay verification.
Original report IDs below preserve links to the complete tracker.

- [ ] Rebuild merged protocol 2154 and rerun the nine focused runners, production
  compilation, ABI/reliable-kind routing checks and Release identity verification.
  Check retained kinds 202/203 and LaptopUI 204; record current hashes/evidence.
  Update packaging identity checks before any separately requested new release.
- [ ] Placement: define acceptance/refusal acknowledgement so rejected client
  placements cannot remain waiting indefinitely for host binding.
- [ ] Coins: E/overlap pickup must credit the host exactly once and remove the coin
  only after acceptance; preserve local map-placed coins.
- [ ] Console/events (12–15, 26, 42): complete Rufus/thiccfus, llama, Madness grunts
  and variant AI/damage/effects; canonicalize simultaneous typing, periodic/local
  output, other funny-rule HUD/events and music phase. Cover other toy/squeeze sounds.
- [ ] Cement/walls (21/22): extend the atomic transaction task above to creative
  construction, wet-wall timer persistence and exact impact/effects. Verify held
  trowel key continuity: a mismatch currently defers the entire profile poll;
  coordinate native hand reconstruction, unequip/drop and persistent cement stock.
- [ ] Water/cement/coal (20/21/44/45): arbitrate concurrent writers and stale
  in-flight records, dryTimer ownership and personal-hand transitions. Complete
  sponge soap stock/foam/surface effects and grill lid/final cooked-food behavior.
- [ ] Garden (46): audit merged care/hose routing against pre-session plant/faucet/
  hose enrollment, watering/fertilizing/harvesting, pot changes, accepted rerouting
  and simultaneous use. Do not assume missing historical save actors are migrated.
- [ ] Sky event (47): synchronize audio, delayed email/bookkeeping and pre-session
  latent chains; transport host phases/cues for other custom event effects/creatures.
- [ ] Packages/food (34/39): audit remaining package subclasses and eaten() output;
  require explicit client-local loot provenance. Preserve implemented final-bite
  delivery and eater-local hunger/poison; avoid global replaceProp admission.
- [ ] Stow performance (16): capture frame times if stutters persist, including
  unresolved-class lookup paths; preserve pending item-state restoration.
- [ ] Models (38): fill remaining preview gaps and verify custom skin compatibility
  without removing rig exclusions. Root prop scaling does not implement skeletal
  bone scaling or arbitrary creature physics; assess those separately if required.

## Entity coverage checklist

For every group, resolve native class/name mapping and missing behavior transport,
then verify host/client spawn, target, motion, actions, damage/death, local effects,
disconnect/reconnect and mid-event join. A transform route alone is insufficient.
Keep personal Insomniac/nightmare and other native personal spawn conditions local.

- [ ] Grayboar: complete remaining join/audio/network-time coordination; verify
  locomotion and movement-loop audio, host target detection, visible turn/pitch,
  helper task retirement/delegates and adoption/held-drive handoffs. Test repeated
  and simultaneous hits, playback failures, death timing, client weapon damage,
  single corpse/loot and host retirement. Verify collision damage, lightning
  replacement propagation and charged explosion/timer/punch/spin effects.
- [ ] Rozitals: Great Pyramid, Kavotia, Pagonioplo-Igetis, Scouts, Soltomia —
  resolve Kavotia/scouts and localized names; verify phases/actions/destruction/join.
- [ ] Antibreather / Fossilhound: player/cave-dependent Antibreather spawning
  and cross-peer attacks; verify the corrected Fossilhound ticker spawn in game.
- [ ] Arirals: verify follower/shooter/pigBeater and implemented gun-stealer paths
  in multiplayer, including stolen/held-item ownership and event phases.
- [ ] Greys / Flamethrower Tank: verify grayTest behavior and firetank turret, fire,
  fuel and damage independently of its Pawn transform feed.
- [ ] Geoms: map Ramiel and Egg-shaped Geom to native classes; verify special actions.
- [ ] UFOs: map Boomerang, Joil, Grey Ship, Midas, Pill, Trifo, Orbs;
  implement/verify child spawns, beam/material/audio phases and late join.
- [ ] Deer / Fish / Cockroach: verify deer/deer_ded; map fish; test roach movement,
  eating, crushing and late join.
- [ ] Blue Brain Cheese, Restocker, Eye, Wolfgang, Tree with Eyes, Living Mushroom,
  Space Jellyfish: establish exact behavior routes, especially actor_save-derived
  Wolfgang; do not identify every eyes entity as eyer.
- [ ] Bone Man, Bloody Skeleton, Curious Eyes, Mimic, Flesh Slug,
  Zombie Deer: finish exact mapping and attack/effect checks; verify mimic shared
  rendering/disguise, damage and lifetime, plus centipede visibility (35/48).
- [ ] Ghost, Ghost Log, Fireflies, Walking Mannequin, Black Spot:
  audit remaining names/effects/phases and test implemented mannequin lifecycle.
  Verify wisp animation readiness and delayed grab/release cancellation through
  pawn replacement, death, world changes and reentrant cleanup.
- [ ] Demons: SKerfur-O, Eyeless Insomniac, Insomnia Demon, Abandoned Kerfur,
  Flesh Erie, Goat Statue, Stolas Statue, Furfur — review individual targeting,
  rituals/run-end behavior; npc_angryErieFlesh is a separate Character (40).
- [ ] Forbidden: Black Friend, CorpseWalker, Crasher, Fast-shadow Figure, Fridge,
  Interloper, The Exit Door, The Head, Pool Entity, Mountain People,
  Cursed Spider, Rune Sphere, Shadow Figure, Fake Kel, Black Sphere —
  map native classes, spawn availability and local/global effects. Do not enable
  debug/test actors in normal play merely to populate this list.
- [ ] Living Erie / Girufus / Rufus: map distinct classes and interaction/animation/
  ownership; do not conflate live, zombie and angry flesh Erie.
- [ ] Gnomes / Krampus: map gnome spawn/interaction/shared lifetime and test Krampus
  attacks/events beyond its existing NPC route.

## Gameplay acceptance checklist

Use matching current 2154 builds. Record role, item/model, reproduction and logs;
offline tests do not close these reports. Existing item-batch and entity checks
above also apply. Issues 6 and the previously working nightmare behavior are not
reopened; verify the newer personal-sleep changes. Wall41 stays excluded.

- [ ] Props (1/4/7/8): client breaking, R+LMB duplication, extinguisher foam and
  loose-outlet floating. Keep outlet floating distinct from appliance connection.
- [ ] Chairs (11/36): rotation and transitions to laptop/sleep/ragdoll.
- [ ] Cremator: verify host-authoritative door and paint-only lever on both peers;
  measure door FPS to check the reported performance problem.
- [ ] Personal sleep (10/49): independent nightmare settings, different beds,
  chance/shared night interruption, simultaneous low sleep and local demon spawn,
  visibility, targeting, grab/attack and wake/release behavior.
- [ ] Bodies (5/31/38/50): every selected/common/custom rig, authored limbs,
  local body, entry/recovery, drift, retry/fallback and streamed remote pose;
  bed mattress/exhaustion anchors, moving beds and sleep/sit/ragdoll precedence.
  Verify sleeping footsteps and hand/head effects are suppressed.
- [ ] Electrical/light/computer (51/53–55): short/long cables, extensions,
  occupied/unpowered outlets; actual light keys, overridden setters, component
  readiness, flicker/load/join timing; shared laptop/portable-PC tabs, text,
  scroll, visibility, cursor/input/activity and operator body. Test ON/OFF during
  animation, power loss/restoration and reconnect.
- [ ] ZIP/cans (56/57): actual inserted/ejected ZIP item class/model/scale on both
  peers; cans open/eat/final portion in hand/world, stow/re-equip, drop/pickup,
  simultaneous use and reconnect.
- [ ] Safe (43): dial/hacked completion, one loot result, persistent lock state,
  retry/stale-record handling and save/rejoin.
- [ ] Garden/sky (46/47): real save/reload and late join after merged changes;
  verify shared schedule and effects for the black-disk/yellow-ring event.
- [ ] ATV: existing saved second-ATV positions, simultaneous carry/mount and stale menus.
- [ ] Disks/racks: client sv.request/hashcodes, floppy/ZIP transfers preserving
  exact text, filled rack save/rejoin and a slow joining peer.
- [ ] Whole-package smoke: swap host/client roles; two clients contest objects;
  late join/rejoin/disconnect/save/reload; partial/empty consumables, full inventory,
  equipped items/container rows, loose cables, flashlight crank/low battery/previews.

## Fork adaptations still requiring review

Audit these candidates against the merged 2154 source before adapting them;
existing shared-screen and garden implementations may already cover parts.

- [ ] Shinobu2 `9863f6ba0`, `291476241`, `56d7ec566`, `9eec3cf37`: review parked
  save ordering/reentrancy, deferred host destruction, held-mirror identity and
  post-birth publication together; retain rollback/duplicate negative controls.
- [ ] Upstream `eb55ae97d`: exclude joining slot from connect-edge NPC fan-out
  with coordinated Session/enrollment changes; preserve late-class/personal/boar paths.
- [ ] Upstream `3a138d025`, `c930d6aa5`: compare active-tab handling with current
  shared UI; tab propagation alone does not establish screen parity.
- [ ] Upstream `53b851c50`, `625187aaa`, `82ad7ef1d`: assess complete camera-bit
  authority/laptop-intent wrapper/network changes and tests against merged code.
- [ ] Upstream `001193570` through `93dd26cc2`: assess the dependent explosion
  capture/client damage/driven-prop coast series; review current wire-kind
  collisions and validate blast ownership before adoption.
- [ ] Upstream `d248fc88d`: assess lobby announce cancellation together with its
  required generation/proof API; do not copy as an isolated patch.
