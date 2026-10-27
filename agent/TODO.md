# TODO - implementation backlog

Development target: **0.9.2162**. Last qualified package: **0.9.2161 / b2217**.
Only defects and unfinished implementation belong here. Validation, rendered
acceptance, qualification and results belong in WORK.md and TESTLOG.md.

## Priority regressions

- [ ] Fix remote flashlights: every peer currently sees only their own light.
- [ ] Fix the T-pose when players fall into ragdoll on every peer.
- [ ] Finish held-toy hook/function/schema ownership and bounded native work.
- [ ] Finish watoy parameter schema/ownership, independent copied-View cleanup,
  original hand provenance and bounded native presentation work.
- [ ] Finish first-dab brush provenance and thrown-sponge synchronization.

## Events

- [ ] Complete native routes for Dreams, Ariral Reputation, Player-Activated,
  Time-Related, Endings, Story Mode, Random and Signal events: actual triggers,
  selected outcomes, effects and join/lifetime ownership. Preserve personal
  dream/ending consequences, reputation tiers, calendar branches and story phases.
- [ ] Extend frequency control to the remaining repeatable random events.
  Complete treeSpawner, bp7Spawner and serverBreaker authority/effect routes,
  including protected-server floppy consumption. Change eligible-roll waits
  while preserving eligibility, probabilities, animation and effect duration.
- [ ] Complete level/map-script event routes once their asset headers are available.


## Shared items and resources

- [ ] Complete Console/events: Rufus/thiccfus cross-player targets/grabs, audio/HUD,
  corpse/loot, damage/explosion transport; llama marker lifecycle; Madness/variant
  AI, damage/effects and remaining funny-rule HUD/events and toy/squeeze sounds.
- [ ] Finish atomic cement/wall repair request/result with exact personal stock
  through hand/slot/drop, rejection and disconnect. Keep no-spend refusal until
  the transaction is complete; never replay an uncertain admitted native effect.
- [ ] Arbitrate concurrent water/cement/coal writers, stale in-flight records,
  dryTimer ownership and personal-hand transitions; finish final output/accounting.
- [ ] Finish personal debit/output/refund as one transaction, including cross-peer
  authority, personal overrides/raw takeObj and receipt-less recovery/persistence.
  Preserve the existing local base world-container escrow/refund boundary.
- [ ] Cover remaining dynamic grime sources and persist explosion/salt-rocket
  scorch provenance through full save/world reconstruction.
- [ ] Complete shared sky-event phases/cues, audio, delayed email/accounting and
  six pre-session latent continuations; add remaining custom effects/creatures.
- [ ] Complete remaining package/food subclass output routes and cross-peer
  portion/output arbitration, personal effects before host acceptance, native
  poison-pour/roach/rewards, uncertain-open refund recovery and persistence.
  Do not admit arbitrary replaceProp or replay uncertain effects.
- [ ] Fix remaining stow stalls, including unresolved-class lookups, while
  preserving pending item-state restoration.
- [ ] Fill missing model previews and custom-skin compatibility without removing
  rig exclusions. Keep skeletal scaling and arbitrary creature physics separate.

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
  names/effects/phases. Finish wisp animation readiness and delayed grab/release
  cancellation on body replacement, death, world change and reentrant cleanup.
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
