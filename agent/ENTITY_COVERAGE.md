# Entity coverage audit for VOTV 0.9.0n

This is the working map for the requested entity checklist. A class name here means the cooked
Blueprint was identified; it does not mean multiplayer behavior is complete.

- **dedicated**: identity plus class-specific state/action handling exists;
- **pose**: host birth/lifetime/transform exists, but class-specific actions or effects do not;
- **prop**: the ordinary keyed/keyless prop pipeline applies;
- **local**: native per-player conditions deliberately continue on each peer;
- **unrouted**: the native class is known but has no suitable multiplayer expression yet.

## Merge and baseline

The `multivoid-sync-2153` tip `bf276d14c4d30dc1ba95ea9ea184f7d9e89dedef` is the second parent of merge
`7fced281e92c4cb2df0e37797bb533041022b087`; both that tip and the pre-merge local parent are
ancestors of current `main`. The merge retained local reliable kinds 202/203, assigned Laptop UI
204, and combined the Grayboar authority work with the incoming stale-replica and per-peer retry
work. The sync branch's laptop, garden, world-sound and placed-hose modules remain in the build and
session integration. The continued executable audit found 38 missing authorship rows: the
receiving host rejected 27 kinds of legitimate client intents/owner samples as unknown. All 38
rows now follow their actual send/receive paths (27 Client, 11 Host). The historical router gate and its 18
injected failure controls passed for 213 kinds. Active development is 0.9.2161 / b2217;
current verification is in TESTLOG. The earlier complete audit was 2158 with 39 passing groups. The 21 October audit is
recorded in TODO/WORK/TESTLOG and build/todo-audit/verification.json. The mapping
review does not close the class-specific behavior adapters below.

## Confirmed native mapping and route state

| Requested name | Native 0.9.0n class or family | Current route | What is still missing |
|---|---|---|---|
| Grayboar | `grayboar_C` (`prop_C`) plus owned `grayboarPawn_C` helper (`Character`) and `grayboar_overcharged_C` | dedicated | Two-peer timing/audio, target/task lifecycle, corpse/loot and charged-effect acceptance |
| Great Pyramid | `piramid2_C` | dedicated `piramid_sync` plus world-actor lane | Live phases/destruction/join acceptance |
| Kavotia | `kavotia_C` (`Character`), spawned/held by `kavotiaPatrolController_C`; projectile `kavotiaShot_C` | NPC pose plus host aim/stance/charge/pitch/windup state; client latent AI/shot graph refused, native mesh setup retained | Fire/laying montage, voice, canonical beam/impact and damage; exact phase-time alignment and live spawn/join acceptance |
| Pagonioplo-Igetis | `igetis_C` | pose | `beamPut`, `beamOn1/2`, `put`, `throw`, magnets and visibility phases |
| Rozital Scouts | `tentacleBall_C`, controller `tentacleBallsFollower_C`, projectile base `tentacleBallShot_C` | pose, with client gameplay bodies refused | talk/light/shot/grab/action phases and join state |
| Soltomia | `soltomiaCleaning_C` | pose, duplicate BeginPlay child spawn refused | talking/open/cleaning state, audio and spawned sponge state |
| Antibreather | `antibreather_C`, spawner `antibreatherSPawner_C` | pose; per-viewer event count stays local | Cave/player-dependent birth and cross-peer attack results |
| Fossilhound | `fossilhound_C`, ticker `ticker_fossilhoundSpawner_C` | pose; per-viewer event count stays local | Live attack acceptance. The executed EX-spawn source gate now contains the ticker |
| Ariral follower/shooter/pig beater | `npc_arirFollower_C`, `npc_ariral_shooter_C`, `npc_ariral_pigBeater_C` | pose | held/thrown object ownership, shots/hits and phase state |
| Ariral chest slapper | `arirSlapper_C` (native follower child), spawned by `slapperSummoner_C` | late NPC pose; exact summoner output admission | Attacks, stolen chest/item ownership, audio/effects and shared lifetime acceptance |
| Ariral gun stealer | `npc_arirGunStealer_C` | late NPC pose; client mirror's real `punching` timer refused | Gun acquisition/attachment, montage and stolen-prop ownership. Native exports contain no `StealGun` or `TakeWeapon` |
| Grey | `grayTest_C` | pose; client native brain/camera bodies refused | target/action/effect state |
| Flamethrower Tank | `firetank_C` (`Pawn`), deployment actor `firetankDeploy_C` | pose | turret aim, fire, fuel, damage and destruction |
| Ramiel | `geomOcta_C` (`Character`), beam child `geomOctaLazer_C` | unrouted | movement, `shoot`, charge/beam/audio, collision and damage |
| Egg-shaped Geom | `eg_C`, event controller `superEgger_C` | pose; client gameplay bodies refused | retrieve/landing/special action state and damage |
| Boomerang UFO | `ufo_boofo_C`, child `ufo_boofo_spawn1_C` | unrouted | child births and phases |
| Joil/Joel UFO | `ufo_joel_C` | unrouted | movement/material/audio phases |
| Grey Ship | `skyUfo_C` and event-specific `morningUfo_C` are the native grey-UFO actors; the user-visible distinction still needs a live probe | pose | timer/material/audio phases and exact display-name split |
| Midas UFO | `midasufotest_C` (`actor_save_C`) | unrouted | save-derived birth state, trigger phase and beam/material state |
| Pill UFO | `ufo_pillfo_C` | unrouted | phases and join state |
| TriFO | `ufo_trifo_C` | unrouted | abduct/beam/timeline state |
| Orbs UFO | `ufo_ballfo_C` | unrouted | child balls, material/audio and destruction |
| Deer | `deer_C`, corpse `deer_ded_C` | dedicated `deer_sync` | Live spawn/death/rejoin acceptance |
| Fish | carried/caught family `prop_fish_C` and `prop_fish_0_C` through `prop_fish_21_C` | prop | Fishing birth/variant and inventory acceptance; no free-swimming Character class exists in the package |
| Cockroach | `cockroachMaster_C`; individual entries are master data, with `singleRoach_C` as a separate actor | dedicated `roach_sync` | Live movement/eating/crush/late-join acceptance |
| Blue Brain Cheese beast | `npc_goreSlither_cheese_C`, descendant of `npc_goreSlither_C`; item `prop_food_blueBrainCheese_C` | pose through the gore-slither root | Attack/death and cheese-event acceptance |
| Restocker | vending-machine event effect; no `Restocker_C` package exists | unrouted event phase | Identify the exact vending/event graph entry, flicker and refill authority |
| Eye | `eyer_C` | owner-entity lane | Attack/grab/damage acceptance |
| Wolfgang | `parentWolfgang_C` and `eggyWolfgang_C` derive from `actor_save_C`; head `centipedeHead_C`, segments and `wolfgangJr_C` | existing centipede visibility work only | Saved head/segment identity, feeding/egg/hatchling phases and late join |
| Tree with Eyes / Killer Birch | `birch_C` | unrouted | Damage/anger, sleep targeting, kill/fade and save effects. It is distinct from `walkingTree_C` |
| Living Mushroom | `npc_funguy_C`; spawners `mushroomMaster_C`/`mushroomSpawner_C` | pose; duplicate client spawns refused | Ragdoll/contact/action and mushroom-drop state |
| Space Jellyfish | `jellyfish_C`, source `jellyfishPath_C` | pose; duplicate client spawn refused | Ground/beam/material/audio phases and seven-child join state |
| Bone Man | `boneman_C` | unrouted | `changeLoc`, visibility/effects and lifetime |
| Bloody Skeleton | `npc_zombie_skeleBloody_C`, descendant of `npc_zombie_skele_C`/`npc_zombie_C`; trigger/spawner `trigger_bloodSkeleton_C`/`bloodSkeletonSpawner_C` | pose by zombie ancestry when spawned through a caught path | Trigger source, attacks/death/effects and join |
| Curious/Prying Eyes | ticker `ticker_eyers_C` spawns `eyer_C`; `radarEye_C` is a separate radar child actor | owner lane only for the personal `eyer_C` | Do not route `radarEye_C` or every eye-named actor as `eyer_C`; confirm the requested display-name association live |
| Mimic | `prop_impostor_C`, master `impostorPropsMaster_C` | prop; client master generation and mirror culling tick refused | Shared disguise/material, damage/gibs and lifetime acceptance |
| Flesh Slug | `npc_goreSlither_C` | pose | Attack, water conversion, damage/death and effects |
| Zombie Deer | `npc_zombie_deer_C`, descendant of `npc_zombie_C`; corpse item is separate | pose by zombie ancestry | Night condition, attacks/fade and corpse dependency |
| Ghost | `ghostcar_C`/`ghostcarSpawner_C` is one native ghost event family | prop/unrouted spawner | Confirm requested display name and camera/visibility phases |
| Killer Wisp (grab/tear boundary) | `killerwisp_C` | dedicated NPC pose and victim grab/tear; queued effects/holds bind observed bodies/world, mesh readiness precedes playback, native capture/release callbacks retain request ownership | Limb/blood presentation and live socket/animation/attack acceptance; this does not identify Ghost or Ghost Log |
| Ghost Log | no class-name match in the package; it is a save-load shapeshifter, so class must be captured live | unrouted | Exact runtime class, teleport/deafen and local/global ownership |
| Fireflies | `ticker_fireflySpawner_C` plus emitter relay | dedicated cosmetic relay | Live local spawn and rejoin acceptance |
| Walking Mannequin | `wMannequinSpawn_C` and mannequin prop family | dedicated `mannequin_sync` | Two-peer lifecycle/held-object acceptance |
| Black Spot | `paranormalSpot_C` is the exact black paranormal spot actor candidate | unrouted | Confirm display-name association and effect ownership live |
| Insomnia demons | `insomniac_C` and `insomniac_Child_C`; ticker `ticker_insomniacSpawner_C` | local; removed client-wide nightmare-probability suppression | Live acceptance of each peer's native spawn/target/camera/grab/dream conditions |
| Flesh Erie | `npc_angryErieFlesh_C` (`Character`) | unrouted | It is separate from live and zombie Erie; needs targeting/artillery/fire/damage route |
| Goat statue | `goat_C` | unrouted | Interaction/movement/effect ownership |
| Furfur ritual | `furfurAltarSpawner_C` (`actor_save_C`) plus altar props | prop/unrouted spawner | Ritual/run-end phase and spawned form ownership |
| Black Friend | `prop_ABfriend_C` | prop | Spawn availability and audiovisual effect acceptance |
| Fridge | `fridge_C` (`Character`) | unrouted | Native movement/effect/lifetime |
| Pool Entity | `poolwalker_C` (`Character`) | unrouted | Restricted-area target/pull/world transition |
| Rune Sphere | rune prop family `prop_rune_C`/`prop_rune_0..3_C`, with `airRuneBox_C`, `earthRuneZone_C`, `waterRuneTrigger_C` | prop plus unrouted ritual actors | Activation and ritual phase ownership |
| Black Sphere / Red Void Orb | `npc_orborb_C`; producer candidate `blackballSpawner_C` (`actor_save_C`) | pose for orb; spawner unrouted | Charge/crash visual, local Meta Paranoia and timed lifetime |
| Living Erie | `eriePlushLive_C` plus `_good_C`/`_evil_C` | unrouted | Interaction, grow/fire/damage/steal and ownership |
| Rufus / thiccfus | `rufus_C` (`Character`); `thiccfus_C` inherits it; separate plush `prop_thiccfus_C`; corpse `rufusCorpse_C` | NPC birth/pose, exact console EX catches and parked mirror graph; native mesh/AnimBP setup plus streamed body yaw | Live birth/join/animation/destruction acceptance; cross-player targeting/grab, sound/HUD/event state, damage/explosion feedback and corpse mesh/loot transport. Marking does not reverse earlier native effects. Girufus remains unmapped |
| Llama event | `prop_llama_C` (`prop_C`), singleton marker `llamaSoul_C` (`actor_save_C`) | Ordinary prop motion plus host-owned h/light state and expiring sound edge; quiet join seeds and client jump-graph authority | Rendered jump/motion/light/audio and join/reload acceptance; remote-player trigger selection (native host camera/player zero remains), marker/save/event-count lifetime, hand transitions and pre-session timer restoration; local cue expiry does not establish synchronized audio timing |
| Gnomes / Nisse | `tomtenisse_C`; death/soul actor `nisse_soul_C` (`actor_save_C`) | unrouted | Group spawn, presents, revenge, damage/death and shared lifetime |
| Krampus | `npc_krampus_C` | pose plus creature-event count | stab/fireball/jump/target/despawn actions and event acceptance |

Still lacking a defensible exact package association: Ghost Log, SKerfur-O event walker versus its
Kerfur/zombie forms, Abandoned Kerfur, Stolas statue, CorpseWalker, Crasher, Fast-shadow Figure,
Interloper, The Exit Door, The Head, Mountain People, Cursed Spider, Shadow Figure, Fake Kel and
Girufus. Candidate filenames alone are not admitted to an allowlist. Debug/test actors also remain
unavailable in normal play unless the native game already spawns them.

## Required multiplayer acceptance

For each row, record host and client birth, target, movement, actions, damage/death, viewer-local
effects, disconnect/reconnect and a join during the active phase. Pose-only rows stay open until
their named class-specific state is carried. Personal Insomniac/nightmare conditions stay local.

## Reusing the Grayboar transport

`coop/creatures/reliable_action_queue.h` is the common action-delivery layer. It supplies non-zero
wrapping sequences, ordered receive admission, bounded per-peer fanout, expiry, independent FIFO
retry and disconnect cleanup. The actual Grayboar path uses it, so its existing saturation, wrap,
retry, reconnect and late-join regressions exercise the shared code.

Each family still needs a native adapter. The adapter samples authoritative state, validates the
live element identity, defines the join snapshot and plays presentation without invoking gameplay
delegates. Kavotia now uses the same bounded fanout/FIFO/expiry code for a coalesced 25-byte state
snapshot (kind 223). Native actor ticks also sample it while the host's mesh is culled; client
AnimBP updates receive the native aim/stance inputs. Active charge-loop/pitch and windup edges
retry independently, and stale state stops the loop even if particle or pitch metadata is missing.
This snapshot carries active flags, not an exact montage/timeline playback position.
Kavotia cannot reuse Grayboar's montage sections;
UFO adapters additionally need child-actor and timeline state, while the Ariral gun stealer needs
canonical prop ownership and attachment state. A pose route alone remains insufficient.

Wisp continuation checks additionally cover host-only grab admission, immediate self-grab/death
cancellation after pawn replacement/recycling, native death and world-generation changes, as well
as a new grab armed inside release cleanup. Pending tear animations cannot migrate into a new
world. Live multiplayer acceptance is still required.
