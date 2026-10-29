# TODO - implementation backlog

Development target: **0.9.2164**.
Only defects and unfinished implementation belong here. Technical scope is in
[WORK.md](WORK.md); checks, results and release evidence are in
[TESTLOG.md](TESTLOG.md).

## Priority regressions

- [ ] Finish held-toy native variant schemas and hook/function ownership beyond
  the validated Dingus/ABPlush capture inputs; preserve bounded native work.
- [ ] Finish watoy start-cue and physics-hook ownership beyond the validated
  activation/audio/emitter schemas and native activation admission.
- [ ] Finish thrown-sponge foam and resource synchronization beyond window dabs.

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
- [ ] serverBreaker: finish protected-floppy consumption delivery to every
  recipient, protection/repair sound and delegate consequences. Preserve the
  host-selected box and native day/difficulty/upgrade probabilities; then scale
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

## Unfinished upstream/fork candidates — reviewed 29 October 2026

These are documented upstream limits or requests with missing local adapters,
not promises of an upstream fix or reproduced failures of our package.
Implementation evidence and exclusions: [WORK.md](WORK.md#upstream-gap-review--29-october-2026).

- [ ] UG1 — Implement remote glowstick grab/release semantics instead of the
  generic base grab prelude; preserve subclass effects and one native activation.
  Source: [upstream prop limits](https://github.com/VOTV-MP/Multivoid/blob/main/docs/props.md#known-limits).
- [ ] UG2 — Preserve a wall attachment's support identity and destruction
  binding when the receiver's trace misses; retire the attachment when its
  support disappears, including late join.
  Source: [upstream prop limits](https://github.com/VOTV-MP/Multivoid/blob/main/docs/props.md#known-limits).
- [ ] UG3 — Replicate toolgun freeze/unfreeze/static changes at the action,
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
