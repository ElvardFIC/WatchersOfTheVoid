# TODO - current priorities

Current source/wire protocol: **2176**, game **0.9.0n**. Last verified Release:
**b2176**, **63/63 offline validation groups**; evidence:
`build/fish-integration-check/verification.json`.
Gameplay acceptance requires identical current DLLs on all peers.
Prepared package **0.9.2154 / DLL b2156** predates this source.

Unresolved implementation, investigations and gameplay checks remain here.
The fish integration and its concrete behavior changes are recorded in WORK.md.
Details and original report IDs: [WORK.md](WORK.md); verification history:
[TESTLOG.md](TESTLOG.md); native entity mapping:
[ENTITY_COVERAGE.md](ENTITY_COVERAGE.md).

## Event synchronization request - OPEN

- [ ] Complete verified native routes for Dreams, Ariral Reputation,
  Player-Activated, Time-Related, Endings, Story Mode, Random and Signal events.
  Every event needs its real trigger, chosen outcome, effects and join/lifetime
  checks. See WORK's event coverage table.
- [ ] Extend frequency coverage beyond the four audited Fossilhound, killer-wisp,
  sky-wisp and night-egg generators to the complete repeatable random-event list.
  Preserve personal dreams/Insomniac, story dates and native eligibility.
- [ ] Fresh game-pak/Blueprint audit remains deferred by the owner.
- [ ] Real-peer event, frequency, late-join, reconnect and simultaneous-trigger
  acceptance on identical current DLLs.

## Investigations requiring runtime evidence

- [ ] Bench58: an old rifle and components disappeared after simple placement
  (owner clarification, 21 October); authoring role and exact component are unknown.
  Native rifle candidate: `prop_rifle_C`. Reproduce the disappearance and capture
  role, class/key and `[PROP-DROP]` / `join_membership_sweep` logs.
- [ ] Token52: the owner cannot identify the counter/UI yet (21 October).
  Identify it before changing money or any token balance.
- [ ] Trowel key continuity and stock across reconstruction/unequip/drop:
  identity mismatch defers the profile. Coordinate the repair transaction with
  native `mainPlayer.updateHold` reconstruction from `saveSlot.hold[0]`.

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

Use matching current **b2176** DLLs. Record role, item/model, reproduction and logs.
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
- [ ] Electrical/light/computer (51/53–55): short/long cables, extensions,
  occupied/unpowered outlets; actual light keys, overridden setters, component
  readiness, flicker/load/join timing; shared laptop/portable-PC tabs, text,
  scroll, visibility, cursor/input/activity and operator body. ON/OFF during
  animation, power loss/restoration and reconnect.
- [ ] ZIP/cans (56/57): actual inserted/ejected ZIP class/model/scale on both
  peers; cans open/eat/final portion in hand/world, stow/re-equip, drop/pickup,
  simultaneous use and reconnect.
- [ ] Safe (43): dial/hacked completion, one loot result, persistent lock state,
  retry/stale-record handling and save/rejoin.
- [ ] Garden/sky (46/47): pre-session save/reload, care/harvest, simultaneous use
  and late join; shared schedule/effects for the black-disk/yellow-ring event.
  Missing historical garden save actors are not migrated.
- [ ] ATV: saved second-ATV positions, simultaneous carry/mount and stale menus.
- [ ] Disks/racks: client sv.request/hashcodes, floppy/ZIP transfers preserving
  exact text, filled rack save/rejoin and a slow joining peer.
- [ ] Whole-package smoke: swap host/client roles; two clients contest objects;
  late join/rejoin/disconnect/save/reload; partial/empty consumables, full inventory,
  equipped items/container rows, loose cables, flashlight crank/low battery/previews.

## Before a separately requested release

- [ ] Update that release's package identity checks. `verify_current_build.ps1`
  validates source/DLL; archived 2154 scripts describe the older b2156 artifact.

Commit, push, game installation and publication require an explicit request.
No other-agent workflow. Update the active tracking documents in place.
