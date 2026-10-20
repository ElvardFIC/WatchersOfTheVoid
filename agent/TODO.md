# TODO - current priorities

Current source/wire protocol: **2159**, game **0.9.0n**. Matching local Release
**b2159** and **40/40 validation groups** pass on the stable source snapshot.
Evidence: `build/event-sync-check/validation-2159/verification.json`.
Peers require identical current DLLs. Prepared package metadata still describes
**0.9.2154 / DLL b2156**; no release package was created by this continuation.

## Event synchronization request - OPEN

- [x] Host-chosen/repeated runSpecialEvent delivery; direct F1 HostFire emission
  without duplicate location sends; one shared replay-policy table.
- [x] Per-recipient refused-send retry, client load-window replay retry and
  peer/world/session cleanup. Focused dispatch and lifecycle regressions pass.
- [x] F1 > Administration > Server settings: `world.event_rate` slider,
  0.5..5x, default2x, live/persisted/replicated. Currently covers Fossilhound,
  killer-wisp, sky-wisp and night-egg spawn intervals; restores on disconnect.
- [ ] Extend verified native routes to the complete eight-category event request.
  Every event needs its real trigger, chosen outcome, effects and join/lifetime
  checks. The four rate-controlled generators do not close the full random list.
- [ ] Fresh game-pak/Blueprint audit deferred by the owner for now. Preserve
  personal dreams/Insomniac and story dates. See WORK's event coverage table.
- [ ] Real-peer event, frequency, late-join, reconnect and simultaneous-trigger
  acceptance on identical current DLLs.

## Full TODO audit - 21 October 2026

A checked implementation means source review plus the named offline checks. It
never closes a separate gameplay acceptance item. All sections below were reviewed;
remaining behavior adapters and transactions are still implementation work.
Evidence and repeatable runner: `package/check_current.ps1`,
`build/todo-audit/verification.json` retains the earlier changing-tree audit.
The current matching **b2159 / 40-group** result is in the event-sync evidence
above. Detailed notes remain in WORK/TESTLOG.

- [x] Run `package/check_current.ps1` on the stable current tree: 40/40 groups,
  production-source hashes across tests and final Release build, DLL identity,
  imports/exports and 762 source entries pass for b2159. Gameplay remains open.
- [x] Placement request/result lifecycle and coin E/overlap/client arbitration.
  Source fixes from the previous continuation are retained; gameplay remains below.
- [x] Held identity: use the registry reverse for both locals and mirrors. A keyed
  or keyless mirror cannot be expressed again or publish its stale record back to
  the owner. Owned held records still publish; key lookup reentry checks actor
  serial, world and connection. `run_held_identity.ps1` exercises production code.
- [x] Laptop power recovery: retry the retained ON/OFF target after native animation,
  loading and wall-power recovery. Reset replay state at disconnect. The broad
  audit found the missing retry call; `run_2173.ps1` now passes both regressions.
- [x] Item batch source fixes: hold arbitration/push, gun effects/root scale, panel
  births, pry, MRE/AB/foodBox output, final bite/local poison and live held records.
  `run_item_batch.ps1` passes. Skeletal bone scaling and creature physics are separate.
- [x] Shared screen/operator state and laptop/portable-PC UI transport are present.
  Shared screen and laptop UI regressions pass; two-peer input/power acceptance stays open.
- [x] Selected-model sleeping bodies and articulated ragdoll source work are present.
  Sleep/ragdoll regressions pass; do not restore the plushie fallback or close rigs
  without actual entry/recovery and limb checks in game.
- [x] Garden source audit: pre-session saved plant/faucet/hose enrollment, care and
  harvest intents, canonical state and accepted hose rerouting are implemented.
  Garden/hose regressions pass. Missing historical save actors are not migrated.
- [x] Grayboar movement, locomotion, turn, hit/attack/roar/death cosmetics and client
  helper/gameplay authority guards are implemented. Kavotia aim/stance/charge state
  uses the common bounded action queue. The remaining adapters are listed below.
- [x] Wisp host-only grab and delayed-action cancellation through pawn replacement,
  death, travel and reentrant release; native personal Insomniac conditions retained.
- [x] Fork candidates reviewed against the actual merged tree; decisions below.
- [x] Restored old regression runners after API drift: typed fact rows/hooks,
  save-codec resolver parameters, placement admission, pry signatures and toolchain/
  native-evidence paths. The cord performance check and its five failure controls
  now run. Placement queue overflow has a current failure control.

## Investigations requiring runtime evidence

- [ ] Bench58: owner now identifies **an old rifle and components**, in an isolated
  simple-placement case (21 October); authoring role and exact component remain
  unknown. The native rifle candidate is `prop_rifle_C`. Its placement refusal and
  sweep protection pass the production harness; this does not reproduce disappearance.
  Capture role, class/key and `[PROP-DROP]` / `join_membership_sweep` logs in game.
- [ ] Token52: owner cannot identify the counter/UI yet (21 October). Keep open;
  do not change money or any token balance based on an assumed identity.
- [ ] Trowel key continuity and stock across reconstruction/unequip/drop: identity
  mismatch still defers the profile. Native mainPlayer.updateHold rebuilds from
  saveSlot.hold[0]; an atomic repair transaction must coordinate that lifecycle.

The sync branch is merged; current continuation changes remain uncommitted.
Preserve existing work. Commit, push, game installation and publication require an
explicit request. No other-agent workflow. Update TODO/WORK/TESTLOG in place.

## Current item batch follow-up

The eight requested item areas have source fixes and offline regressions in continued
2153; see WORK.md, Item synchronization batch. Validate all eight on matching current
DLLs: grabs/collision push, both gun effects and contested control, freeze/root scale,
struck panels, crowbar replacement/parents, crate/MRE/foodBox loot, food final bite and
local poison, battery hand/stow/drop/rejoin. Skeletal bone scaling and arbitrary
creature physics remain outside the root-prop lane. Reliable kind 202 is added within
development 2153; existing delivery ZIP does not contain this work.

## Remaining implementation and investigations from WORK

Full TODO/source/fork review: 21 October 2026. See the current audit above.
Newer implementation reports supersede
older "needs implementation" notes for sleeping bodies, shared UI, ragdolls and
food final-bite delivery. Those features still require gameplay verification.
Original report IDs below preserve links to the complete tracker.

- [x] Rebuilt and verified the audit snapshot at protocol 2158; original nine focused groups,
  garden/laptop/sound, placement/coins/native coin evidence and reliable routing
  pass (16 groups in the prior continuation; expanded current audit above). Full Release compilation, retained IDs 202/203/204, ABI and
  DLL identity/imports/exports checked; current hashes are in TESTLOG/evidence.
- [ ] Before a separately requested release, update that release's package identity
  checks. `verify_current_build.ps1` validates current source/DLL; archived 2154
  package scripts still correctly describe its older b2156 artifact.
- [x] Placement acceptance/refusal: nonzero request IDs, host outcome/retry/cache,
  replay protection and bounded binding wait. Retain the local actor on refusal;
  prevent fallback authoring and publish saved state only after acceptance.
- [ ] Placement gameplay: verify refusal/recovery and late PropSpawn adoption with
  two peers, device ejects, containers and live food/drive state.
- [x] Coins: host E, overlap and client requests share one identity reservation
  before native credit, including pre-eid overlap. Unknown outcomes do not credit
  again; native no-ops may retry. Client mirrors await canonical destroy/balance;
  client-local map coins remain native. Retry callbacks survive disconnect/reset.
- [ ] Coins gameplay: contest one coin via E/overlap on both peers; verify one host
  credit, accepted mirror removal, refusal retention and local map-coin behavior.
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
- [x] Garden (46) source audit: merged pre-session enrollment, care/harvest/pot
  state and accepted rerouting are covered by the garden/hose regression suite.
- [ ] Garden (46) runtime: pre-session save/reload, care and simultaneous use on
  real peers. Missing historical save actors are not migrated.
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
  Kavotia and scouts are mapped in ENTITY_COVERAGE; finish their action/effect
  adapters and verify phases/actions/destruction/join.
- [ ] Antibreather / Fossilhound: player/cave-dependent Antibreather spawning
  and cross-peer attacks; verify the corrected Fossilhound ticker spawn in game.
- [ ] Arirals: verify follower/shooter/pigBeater and implemented gun-stealer paths
  in multiplayer, including stolen/held-item ownership and event phases.
- [ ] Greys / Flamethrower Tank: verify grayTest behavior and firetank turret, fire,
  fuel and damage independently of its Pawn transform feed.
- [ ] Geoms: Ramiel is `geomOcta_C`, Egg-shaped Geom is `eg_C`; implement missing
  action/effect routes and verify special actions.
- [ ] UFOs: package mappings are in ENTITY_COVERAGE; Grey Ship display-name split
  still needs a live probe. Implement/verify child spawns, beam/material/audio phases
  and late join for Boomerang, Joil, Grey Ship, Midas, Pill, Trifo and Orbs.
- [ ] Deer / Fish / Cockroach: verify deer/deer_ded and caught `prop_fish_0..21_C`;
  no free-swimming Character is present in the native corpus. Test roach movement,
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

Use matching current **b2158** DLLs (including this continuation). Record role, item/model, reproduction and logs;
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

## Fork review and remaining adaptation

Reviewed against current 2158 source and local commit contents on 21 October.
Checked rows close the review/adaptation decision, not multiplayer acceptance.

- [x] Shinobu2 `9863f6ba0`, `291476241`, `56d7ec566`, `9eec3cf37`: parked
  records already use replacement sequences, copied dispatch and callback-safe
  retirement; hosts already refuse destroy deferral; finished births already
  publish final records after native return. Fixed the remaining locals-only held
  lookup and stale mirror publication. Record/placement failure controls and the
  held-identity regression pass; owned held-state restoration is retained.
- [x] Upstream `eb55ae97d`: already an ancestor. Session fan-out, world-ready gate,
  enrollment and connect replay consistently exclude the joining slot. Existing
  late-class/personal-creature/boar guards remain; live join is still unverified.
- [x] Upstream `3a138d025`, `c930d6aa5`: already ancestors. Typed laptop widget/tab
  transport is built alongside SharedScreen/LaptopUI; active-tab support does not
  replace the wider shared UI acceptance checks.
- [x] Upstream `53b851c50`, `625187aaa`, `82ad7ef1d`: all ancestors. The camera
  wrapper, intent validation/reach and host-bit authority are present and build;
  reliable routing includes their kind 206. No partial patch needed.
- [x] Upstream `001193570` through `93dd26cc2`: assessed as a dependent series.
  Its ExplosionSpawn=177 conflicts with local KitchenState=177. Retain the existing
  ExplosionEvent=188 / Explosion=191 lanes, local-player damage guards and canonical
  native blast capture; both explosion regression/failure-control/native suites pass.
  Do not describe this decision as implementation of upstream affected-prop coasting.
- [ ] Explosion follow-up: assess and implement host tracking/coasting of affected
  props through native delayed impulses; validate held/frozen/static exclusions,
  blast ownership/range and settled poses on two peers before adopting that behavior.
- [x] Upstream `d248fc88d`: already an ancestor, including the generation/proof API.
  Host reads entry generation before its leading Stop and expects exactly one bump;
  an additional Stop cancels the create. No isolated backport needed.
