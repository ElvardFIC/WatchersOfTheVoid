# TODO — current priorities

## Version 0.9.2159

- [x] Implement owner-to-host bleeding drops and validated host forwarding to
  other players, without echoing the injured player's native drop. Development
  **b2187 / protocol 2187**; Release and **69/69 offline groups** pass.
- [ ] Accept visible bleeding from host and client injuries in rendered multiplayer
  with identical b2187 DLLs, including stopping blood loss and joining mid-bleed.
  Native drop collisions/stains run independently on each peer; exact stain layout,
  historical-stain joins and persistence are not established by this change.

## Current baseline and pending acceptance

Current local test package: **0.9.2158 / DLL b2186 / protocol 2186**, game **0.9.0n**.
Release and **68/68 offline groups**, package label and archive checks passed.
Verification history remains in [TESTLOG.md](TESTLOG.md); completed release reports
and superseded build instructions have been removed from this backlog.

- [ ] Verify native startup from the current full manager package and ordinary
  rendered host/client gameplay, including the displayed public version.
- [ ] Accept the 2157 immediate-join reports: player flashlights, light switches,
  garage/base doors and client drone contents. Watch capacity, flashlight retries
  and exact-recipient container seeds are implemented; details:
  [SYNC_2157_FIXES.md](SYNC_2157_FIXES.md).
- [ ] Accept 2–32-player hosting/admission in game; update the deployed master
  before automatic lobbies above four seats. Only the local master source changed.
- [ ] Accept the selected upstream adaptations in game: vital quantization,
  late prop/trash class resolution, fossil-hound body yaw, NPC pose caching and
  plain-wisp landing. Deferred conflicts: [FORK_AUDIT_20261022.md](FORK_AUDIT_20261022.md).
- [ ] Accept ragdoll entry smoothness, running momentum, camera and recovery
  heading with real input; check intermittent incomplete remote limbs if reproduced.
  Native/offline results for previous DLLs remain historical evidence in TESTLOG.
- [ ] Optional future rigging: authored/cooked player PhysicsAsset or a separately
  validated stock-body display retargeter, after choosing its tradeoffs.

Use identical current DLLs on every peer. Prioritize reproducible failures,
resource identity/transactions and the PC/ragdoll reports below. Unchecked gameplay
items can already have implemented code; they require acceptance, not reimplementation.
Mining (6) stays closed; Wall41 stays excluded; chair reports 11/36 are duplicates.
Fresh game-pak/Blueprint extraction remains deferred by the owner.
Details: [WORK.md](WORK.md), [ENTITY_COVERAGE.md](ENTITY_COVERAGE.md).

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

Use matching development **b2187** DLLs for new checks; the prepared 0.9.2158
package remains **b2186**. Record role, item/model, reproduction and logs.
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

- [ ] Resolve the remaining public-prose counters recorded in TESTLOG:
  document count 85/82, unresolved source citations 155/0 and
  src.files_not_swept 84/0. The latest fact-row gate passes (509 rows/161 files);
  older missing-reference/unused-row findings are superseded.
- [ ] Complete the native-contract audit when the owner resumes the deferred
  asset audit. The existing corpus is incomplete for the imported suite.

## Release preparation

Package 0.9.2158 is prepared locally with offline-test qualification.
Requalify changed source and build the next package through package/release.json,
package/check_current.ps1 and package/package_current.ps1. Native startup and
rendered multiplayer acceptance remain open; see TESTLOG for verification limits.

Commit, push, game installation and publication require an explicit request.
No other-agent workflow. Update the active tracking documents in place.
