# TODO - implementation backlog

Development target: **0.9.2165**.
Only defects and unfinished implementation belong here. Technical scope is in
[WORK.md](WORK.md); checks, results and release evidence are in
[TESTLOG.md](TESTLOG.md).
Run tests and pre-build gates before Release compilation, as specified in README.

## Priority regressions

- [x] Joined-client prop jitter/support-penetration report closed per owner
  direction on 11 November 2026; no further implementation requested. This is
  not a new gameplay verification. Reopen if the symptom recurs with a repro.
  Broader hand-drive, parked-support and host/client publisher reviews remain
  tracked separately in WORK.
- [ ] Finish native function-schema review for held-toy variants beyond the
  validated Dingus/ABPlush inputs. Class-scoped Dingus capture hooks now cover
  all catalogued variants; verify any variant-specific native inputs and gameplay.
- [x] Watoy controller now resolves physics-hook ownership through the
  Blueprint caller actor (with component-outer fallback), matching native
  `AddForce` and `SetPhysicsLinearVelocity` hook provenance; start-cue and
  physics-owner fixture coverage passes. In-game timing/physics acceptance
  remains open.
- [ ] Finish thrown-sponge foam and resource synchronization beyond window dabs.

## Upstream integration

The 8 November selected integration (M1-M10) is recorded in WORK and
FORK_AUDIT_20261022.md. It reviews upstream `49023201c` and the current default
fork heads; it does not replace the existing controllers or close the semantic
reviews below. Native pickup/rest, module cleanup and personal-rest acceptance
remain unperformed.

- [ ] UP2: finish native continuation/session ownership review beyond the verified
  message-authority rules; track concrete findings in WORK. NJ01-NJ25 / NK01-NK50
  now own NPC join reads, registry/actor identities and per-recipient snapshot completion.
  KD01-KD25 / KX01-KX50 now retain Kerfus state/join delivery, guarded mirror
  writes and one-time native refresh dispatch across replacement.
  KA01-KA25 / KB01-KB50 guard Kerfus command capture, client FIFO retry and
  host/native action dispatch. SF01-SF25 / SM01-SM50 now own follow/services and
  guarded native player substitution. OC01-OC25 / OX01-OX50 now own Omega
  execution, rollback/startKill, staged hands and native replay. HD01-HD25 /
  HX01-HX50 now retain HandTaken delivery and own client cleanup/native inventory,
  refresh and grab-release continuations. Exact same-class item correlation,
  uncertain resource/latent reconciliation and native acceptance remain open.
  NH01-NH25 / NX01-NX50 guard ordinary NPC pose/hold continuations. Native
  transform/constraint priority and remaining transferred routes still need review.
  LS01-LS25 / LX01-LX50 implement independent source claim leases. VA01-VA25 /
  VX01-VX50 now guard drive admission and host-verb handoff continuations.
- [ ] UP4: finish uncertain native spawn/pull-output reconciliation and remaining
  tower timeline/effect review. UI01-UI05 implement tower PRE/POST and frame
  proofs, client FIFO delivery until ACK, committed writes/refund debt, captured
  refused-pull cleanup, per-recipient cord state/join delivery and native drag/head
  continuations. Unknown spawn/pull results remain recorded
  without replay; native acceptance is unperformed. Earlier CP/CQ and TQ/TO
  boundaries remain covered.
- [ ] UP5: finish handoff/support-contact ownership review and the original prop
  jitter investigation. End delivery EH01-EH25 / EX01-EX50, grant/revoke
  GD01-GD25 / GX01-GX50, swinger SJ01-SJ25 / SD01-SD50, stream SG01-SG15 / SX01-SX60,
  publisher HP01-HP25 / HT01-HT50, controller PH01-PH15 / HS01-HS60 and client
  PC01-PC25 / PS01-PS50 are implemented; evidence is in TESTLOG.
- [ ] UP6: finish Geom Octa current-phase/audio/effect joins and native mirror
  creation acceptance; finish Vulkan full context recovery/backend retirement.
  UI06/UI07 add retained decisions, ordered per-recipient NPC birth admission,
  guarded complete native transform reads, reconstruction guards and serialized
  Present/Destroy; GPU quiescence and native acceptance remain unverified here.
  NPC join delivery now retains refused/unbound rows and saved/adoption provenance;
  a pending recipient does not block other slots' prop enumeration or completion.

Source transfer now reaches upstream `4fbf1acf0`; these items are the remaining
semantic review. Earlier joint development b2220 source, including native tower wrappers,
physics-call, child-release, swinger/cord and Vulkan context guards, passed 142
offline groups across the full run and scoped recheck, plus Release/ABI inspection.
Evidence, source identities and the separate failing prose gate are in TESTLOG.
UI01-UI09 changes follow that build and have separate evidence; they are not
covered by its DLL/ABI qualification.
2163/b2217 remains the retained qualified archive.

Scope: [WORK.md](WORK.md#upstream-integration-in-progress). Completed conflict,
fixture and build checks are recorded in TESTLOG; gameplay remains unverified.
The 75 queue/coffee ownership boundaries are now individually recorded in TESTLOG;
UP4 still owns the remaining cord/tower review and native multiplayer acceptance.
Radio R01-R45/M01-M30 and tower T01-T45/K01-K30 are implemented. Their per-item
reports are in TESTLOG. Tower wrapper CW01-CW37 / RW01-RW38 are implemented;
uncertain operation/debit/output reconciliation, remaining lower-level timeline
and gameplay boundaries remain open.

The 4 November fork recheck adds three concrete follow-ups: concurrent disc
insert ownership, host-admitted module purchases and transient spawner provenance
at join. Their implementation constraints are in
[WORK.md](WORK.md#fork-follow-ups--reviewed-4-november-2026).
FC01-FC75 now guard floppy-claim continuations and client reads across native
callbacks; disc occupancy arbitration and debit/output/refund still remain open.

## Events

- [ ] Complete remaining native adapters for Dreams, Ariral Reputation,
  Player-Activated, Time-Related, Endings, Story Mode, Random and Signal events:
  actual triggers, selected outcomes, phases/effects and current-state late join.
  Preserve personal dream/ending consequences, reputation tiers, calendar
  eligibility and story phases.
- [ ] treeSpawner: transport the host-selected tree's visibility, mesh scale,
  movement and lifetime; complete client producer suppression and late join.
  Then scale only the native 600–900-second spawn wait, preserving mode gates
  and the tree's render/proximity-dependent behavior.
- [ ] serverBreaker: finish protection/repair sound and delegate consequences.
  Canonical floppy-slot delivery/retry boundaries SB1-SB5 are implemented in WORK.
  Preserve the host-selected box and native day/difficulty/upgrade probabilities; then scale
  only the eligible-roll wait (native 1200–600 seconds).
- [ ] Complete level/map-script event routes using the extracted map headers;
  preserve personal consequences and the native protection/exit branches.

## Shared items and resources

- [ ] Finish Rufus/thiccfus cross-player targets/grabs, audio/HUD, corpse/loot,
  damage and explosion transport; Madness/variant AI, damage/effects and remaining
  funny-rule HUD/events and toy/squeeze sounds.
- [ ] Finish llama marker/save/event-accounting lifecycle beyond the existing
  PlacedSave marker route and invocation-bound Begin recovery. Keep native host
  contributions authoritative; do not infer latent age from an absent sender or
  replay uncertain timer creation. Failed-marker cleanup now has an independent
  frame-tail retry path; its implementation and checks are in TESTLOG.
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

G01-G11 / R01-R40 / P01-P24 are implemented and covered by focused offline
fixtures: Ramiel ownership, roach reconciliation and Great Pyramid lifetimes.
Remaining behavior/effect routes below and native multiplayer acceptance stay open.

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

- [x] EmoteWheel 1.0.1: native optional-mod adapter, exact phase/variant/music
  clocks, owner validation, join/retry/cleanup and generation-matched hand display.
- [ ] Accept EmoteWheel in native multiplayer, including mixed installations,
  random variants, phase/music timing, held items and body/ATV/ragdoll transitions.
  Scope and offline boundaries: [WORK.md](WORK.md#requested-mod-integrations).
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

## Unfinished upstream/fork candidates — reviewed 29 October 2026

These are documented upstream limits or requests with missing local adapters,
not promises of an upstream fix or reproduced failures of our package.
Implementation evidence and exclusions: [WORK.md](WORK.md#upstream-gap-review--29-october-2026).

- [x] UG2 — Preserve a wall attachment's support identity and destruction
  binding when the receiver's trace misses; retire the attachment when its
  support disappears, including late join.
  Source: [upstream prop limits](https://github.com/VOTV-MP/Multivoid/blob/main/docs/props.md#known-limits).
- [x] UG3 — Replicate toolgun freeze/unfreeze/static changes at the action,
  including idle props; existing hold/join flag convergence is insufficient.
  Source: [upstream prop limits](https://github.com/VOTV-MP/Multivoid/blob/main/docs/props.md#known-limits).
- [ ] UG4 — Enforce laptop seat stabilization across peers so another player
  cannot carry the occupied chair away; release the protection on exit or teardown.
  Source: [upstream prop limits](https://github.com/VOTV-MP/Multivoid/blob/main/docs/props.md#known-limits).
- [ ] UG5 — Add host-owned red-phone ring selection and presentation instead
  of independent peer rolls.
  Source: [upstream signal limits](https://github.com/VOTV-MP/Multivoid/blob/main/docs/signals.md#known-limits).
- [ ] UG6 — Add an authoritative desk virusEvent route and its effects;
  preserve personal consequences and avoid replaying independent client viruses.
  Source: [upstream signal limits](https://github.com/VOTV-MP/Multivoid/blob/main/docs/signals.md#known-limits).
- [ ] UG7 — Own signal-begin world-prop births (including lifecrystal) so
  download-arm replay and host birth delivery cannot create two copies.
  Source: [upstream signal limits](https://github.com/VOTV-MP/Multivoid/blob/main/docs/signals.md#known-limits).
- [ ] UG8 — Implement runtime 3D-print result identity, mesh/content binding
  and pose/lifetime delivery to peers with matching files. Trace the reported
  path first; generic prop replication alone does not establish custom-mesh support.
  Source: [upstream request #49](https://github.com/VOTV-MP/Multivoid/issues/49).
