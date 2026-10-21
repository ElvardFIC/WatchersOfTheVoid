# Unified Work - unresolved backlog

Current source/wire protocol: **2176**, game **0.9.0n**. Last verified Release:
**b2176**, **63/63 offline validation groups**; evidence:
`build/fish-integration-check/verification.json`.
Gameplay acceptance requires identical current DLLs on all peers.
Prepared package **0.9.2154 / DLL b2156** predates the current source.
Use identical current DLLs for multiplayer acceptance.

This file retains unresolved implementation, investigation and gameplay work.
Priorities: [TODO.md](TODO.md). Source reports and verification history:
[TESTLOG.md](TESTLOG.md). Exact native entity mapping and per-class gaps:
[ENTITY_COVERAGE.md](ENTITY_COVERAGE.md).
Offline checks do not close gameplay reports.

## Інтеграція watchers-of-the-void-fish — 21 жовтня 2026

Перенесено зміни з `e48f19b8717c8b695c6d235499d0baf31fccbf6b`
від спільної бази `b6651cb91` у поточний `main` (`74e1729e5`).
Походження та межі початкової перевірки: [HANDOFF_FISH.md](HANDOFF_FISH.md).

| Ділянка | Що змінює перенесений код |
|---|---|
| Розміщення предметів | Незмінні номери запитів, прийняття/відмова/повтор, збережені відповіді при переповненні мережевої черги, захист від повторного створення й зміни гравця/світу. Відхилений предмет можна забрати локально. |
| Монети | Єдине резервування перед зарахуванням; повторне видалення вже зарахованої монети при відмові native destroy. Невідомий результат не допускає повторного зарахування. |
| Вода, мийка, напої, бензин | Перевірені джерела запуску, native політ/зіткнення на хості, окремі звук/частинки на клієнтах, ідентичність та очищення розливів. Приватні вибухи комп'ютерних ігор залишаються локальними. |
| Бруд, вікна, сліди вибухів | Канонічні забруднення й очищення, тихе приєднання, повтор доставки фінального стану, життєвий цикл декалей, сліди вибухів і соляної ракети. |
| Кухня та гриль | Кришка, паливо, горіння та звук; окремі впорядковані зміни вугілля/кришки; стан гриля в руці та збереження host burn/active при пізніх записах клієнта. |
| Їжа | Прогрес готування проходить через світ, руку, розміщення, профіль і save record; повтор при неготовому акторі; вузько дозволені особисті cookItem births; host output для бананової шкірки та порожньої чашки. |
| Матеріали й будівництво | Збереження таймерів цементу/висихання стін і мила губки; косметичні піна/бризки без повторного списання; перевірене миттєве будівництво в creative. Платний атомарний ремонт лишається відкритим. |
| Консоль, музика, іграшки | Один host-ordered потік console output/clear/history; відновлення Madness audio/HUD; фаза, швидкість і відновлення Maxwell; окремий контролер Watoy у світі/руці та при пізньому вході. |
| Сад, профілі, небесна подія | Прив'язка care/faucet/detach до конкретного виклику й гравця; актуальний held state перед хешуванням профілю; хост володіє tp та шістьма latent продовженнями чорної діри. |
| Перевірки | Нові production harnesses і native-contract checks; перевірки .inc-контексту в CI; спільний регресійний запуск для обох наборів змін. |

Адаптація до main: протокол **2176**, збережено `KavotiaState=223` і
`PropPlacementResult=224`, `Watoy` перенесено на **235**; інші нові повідомлення
займають 225–234. `PropDropIntent` має 188 байт з `foodCookProgress`, відповідь
розміщення — 101 байт з ключем і класом. Узгоджено authority, dispatch і lanes.
Збережено Entity/Kavotia, персональний сон, частоту й доставку подій із main.

У конфліктних ділянках збережено захист main: save record передається лише після
прийняття хостом; наявний предмет з тим самим ключем не означає успіх розміщення;
одночасні й щойно завершені запити резервують ключ навіть до оновлення індексу.
Монети зберігають actor identity до появи eid та одноразовий guarded native entry.
Додаткові тести перевіряють ці межі разом із новим retry ledger.

Перевірка в грі з двома гравцями, атомарне списання/повернення особистих ресурсів,
платний ремонт, решта джерел бруду та перенесення scorch provenance через повне
перезавантаження світу залишаються відкритими. Поточний локальний corpus не містить
усіх потрібних новому native-contract suite assets; свіжий pak audit лишається
відкладеним за попередньою вказівкою власника. Початкові результати b2175 не є
результатами перевірки інтегрованого b2176. Поточні результати — у TESTLOG.md.

## Requested event synchronization and frequency

The eight-category request remains open. The owner deferred the fresh
game-pak/Blueprint audit on 21 October; the old D: installation is unavailable.
Keep that audit deferred. Do not infer native mapping from a wiki name or close
an event from its actor's transform route.

| Requested category | Remaining implementation or acceptance |
|---|---|
| Dreams | Verify all named variants and wake/reward consequences with real peers; preserve native personal sleep/Insomniac conditions and independent nightmare settings. |
| Ariral Reputation | Audit every reputation tier/case, shared reputation, randomized effects, bed/player relocation and late join. |
| Player-Activated | Map and verify every listed interaction, simultaneous activation and canonical side effects. |
| Time-Related | Audit real-calendar/holiday and 3AM branches, local calendar rolls and uncovered effects. |
| Endings | Establish and verify each ending's per-player versus session consequence; do not replay quit/travel on remote puppets. |
| Story Mode | Complete actor/phase/audio/bookkeeping adapters and current-state late join; start-of-event replay does not prove an active-phase snapshot. |
| Random | Audit the complete named list and remaining timer/probability sites. Black-fog client suppression alone does not mirror black fog. |
| Signals | Audit each processing/playback trigger, funny-rule requirement, chosen outcome, phases and late join. |

Frequency coverage still needs extension beyond the four audited
`ticker_fossilhoundSpawner`, `ticker_yellowWispSpawner`, `ticker_wispSpawner`
and `ticker_egSpawner` routes. Preserve native eligibility, probabilities,
personal dream rolls and story dates; adjust eligible-roll intervals without
changing AI/animation, effect durations or sleep needs.

Real-peer acceptance must cover chosen/repeated events, simultaneous triggers,
live/persisted frequency changes, load windows, late join, reconnect and teardown.

## Implementation and investigation details

### Console, funny-rule and custom events (12–15, 26, 42)

- Complete Rufus/thiccfus, llama, Madness grunts and variant event bodies,
  AI, damage and local effects.
- Verify imported host-ordered submitted/periodic/clear console output and
  Madness/Maxwell timing; complete other funny-rule HUD/events.
- Audit other toy/squeeze sounds and verify world/selected-hand playback.
- Verify initiator/base-entering-player teleport and other custom event paths;
  carry host phases/cues instead of replaying whole commands on every peer.

### Cement, walls and resource transactions (20–22, 44/45)

- Implement atomic wall-repair request/result with exact personal cement stock
  across hand/slot/drop, retries, rejection and disconnect. Keep the current
  no-spend client refusal until that transaction is complete.
- Verify held-trowel key continuity. A mismatch defers the entire profile poll.
  Native `mainPlayer.updateHold` rebuilds the hand from `saveSlot.hold[0]`;
  `checkEquip` reads the live actor without updating that array. Coordinate
  reconstruction, unequip/drop and persistent stock in the repair transaction.
- Verify imported creative construction and wet-wall timer persistence, exact
  impact/effects and client damage to completed walls.
- Arbitrate concurrent water/cement/coal writers and stale in-flight records;
  settle dryTimer ownership and personal-hand transitions.
- Verify imported sponge soapAmount/foam, grill lid/fuel/burn/audio and
  cooking-progress carriers on real peers. Complete remaining final cooked-food
  output and personal resource transactions.

### Packages, performance and models (16, 34/38/39)

- Audit remaining package subclasses and eaten() outputs using exact native
  provenance. Keep client-local loot admission scoped; do not admit all
  replaceProp output. Preserve final-bite delivery and eater-local hunger/poison.
- Capture frame times if stow stutters persist, including unresolved-class
  lookup paths. Preserve pending item-state restoration.
- Fill remaining preview gaps and verify custom skins/animation/physics.
  Missing model names are still unknown. Preserve rig compatibility exclusions
  and selected-model bodies; do not restore the plushie fallback.
- Skeletal bone scaling and arbitrary creature physics need separate assessment
  if required; root-prop scale does not cover them.

### Sky event (47)

Verify imported host audio, delayed email/bookkeeping and pre-session latent
chain ownership; complete the missing client phase/audio presentation.
Verify shared schedule, visuals and late join for the black-disk/yellow-ring
event. This report is separate from Bad Sun and black fog.

### Explosion adaptation

Assess and implement host tracking/coasting of affected props through native
delayed impulses. Validate held/frozen/static exclusions, blast ownership/range
and settled poses with two peers. Upstream series `001193570` through
`93dd26cc2` cannot be copied with ExplosionSpawn=177 because local
KitchenState uses 177. Retain local explosion lanes 188/191.

### Runtime evidence: Bench58 and Token52

Bench58 is simple placement followed by disappearance after 1–2 seconds or
later; crafting is not part of the report. On 21 October the owner identified
an old rifle and components in an isolated case. Authoring role and the exact
component remain unknown. Native rifle candidate: `prop_rifle_C`.
Capture the item/class/key, authoring peer, timing, `[PROP-DROP]` and
`join_membership_sweep` logs. Offline refusal/sweep checks do not reproduce
the disappearance.

Token52's counter/UI remains unidentified by the owner (21 October).
Identify the actual display and token before changing money or any balance.

## Complete open report tracker

Original report IDs remain linked; historical work numbers in TESTLOG are not
newer release numbers. Mining (6) stays closed. Nightmares (10) were previously
reported working; accept the newer personal-sleep changes without reopening the
old behavior. Old basement wall (41) remains excluded by the owner.

| ID | Report | Remaining work / acceptance |
|---|---|---|
| 1 | Client cannot break props | New-build gameplay result. |
| 2 | Prop jitter/rollback | Verify initial optimistic grab, lowest-slot hold arbitration, release fences and multiplayer convergence. |
| 3 | Pushing one prop with another | Real PhysX contacts, chained push/coast and two-player convergence; check static/frozen/held/trash exclusions. |
| 4 | Client R+LMB duplication | New-build gameplay result. |
| 5 | Selected-model ragdoll | Real selected-model pose, entry/recovery and local/remote limbs; linked to 38/50. |
| 7 | Extinguisher foam | New-build gameplay result. |
| 8 | Loose power outlet floating | New-build gameplay result; distinct from appliance connection (51). |
| 9 | Physics gun / both gun effects | Contested control, native release, beam/audio, freeze/unfreeze and root scale on real peers. Bone scaling/creature physics remain separate. |
| 10 | Personal nightmares | Independent enabled/bed/chance conditions, shared interruption, simultaneous wins, awake/rested peers and reconnect. |
| 11 | Rotating chair | Rotation and transitions to laptop/sleep/ragdoll on both peers. |
| 12 | Console commands | Complete event bodies/local effects and canonical simultaneous-input/output ordering. |
| 13 | Funny-rule music | Exact track phase and other event music. |
| 14 | Funny-rule display | Other funny-rule HUD/events. |
| 15 | Toy sounds | Other toys/squeeze paths and real world/selected-hand playback. |
| 16 | Stutters when stowing items | Capture runtime frame times if residual stutters remain; include unresolved-class paths. |
| 20 | Bucket water disappearing | Concurrent writers/in-flight-record arbitration, pouring and personal-hand transitions. |
| 21 | Cement / trowel | Atomic repair/material transactions, trowel key/stock continuity and dryTimer handoff. |
| 22 | Brick-wall outline/build | Client repair, creative construction, wet-wall persistence, exact effects and multiplayer damage/build acceptance. |
| 25 | Battery levels | Hand/stow/drop/pickup/rejoin, including zero charge and delayed record delivery. |
| 26 | Airy event | Exact behavior, effects and multiplayer acceptance. |
| 27 | Main-room computer panels become physics props | Struck-panel disappearance and final motion/state on host/client. |
| 29 | Crowbar static state / dragging | Atypical parent/replacement subclasses, live ownership, refusal/retry and actual dragging gameplay. |
| 31 | Sleeping body pose | Selected rigs, mattress/exhaustion anchors, moving beds, precedence and sleeping footstep/hand/head-effect suppression. |
| 34 | Invisible client crate/package loot | Remaining package families, exact local loot provenance and two-peer crate/MRE/AB/foodBox output. |
| 35 | Centipede / other creatures invisible | Per-class behavior, visibility and live acceptance in ENTITY_COVERAGE; linked to 48. |
| 36 | Computer chair rotation | Duplicate of 11; keep both report IDs linked. |
| 38 | Skin model list / previews | Remaining preview gaps, unknown missing model names and common/custom appearance/animation/ragdoll acceptance. |
| 39 | Consumables refill on pickup/drop | Partial/empty portions, hand/stow/drop/pickup/rejoin, final bite and eater-local hunger/poison. |
| 40 | Flesh Erie summon / visibility / aggro | Distinct angry-flesh-Erie spawn/target/action/damage route and gameplay. |
| 42 | Event teleport to triggering/base-entering player | Initiator/occupancy, repeat/reentry, multiplayer teleport and other custom effects. |
| 43 | Safe cracking | Dial/hacked completion, one loot result, persistent lock, retry/stale records and save/rejoin. |
| 44 | Sponge wetting | Soap stock/foam/surfaces, personal-hand transitions and real wetting/final-use gameplay. |
| 45 | Grill ignition | Concurrent coal/stale-record arbitration, lid, final cooked food and personal-item transitions. |
| 46 | Garden actors disappear after save reload | Pre-session save/reload, care/harvest/pot state, accepted hose rerouting, late join and simultaneous use. Missing historical save actors are not migrated. |
| 47 | Black disk/yellow-ring sky event | Audio, delayed email/bookkeeping, pre-session latent chains and shared schedule/effects/join acceptance. |
| 48 | Mimic | Shared rendering/disguise/material, damage/gibs and host lifetime in multiplayer; related to 35. |
| 49 | Client insomnia demon | Personal spawn/visibility/pursuit/targeting/grab/dream, low-sleep versus rested peers, recovery/despawn, reconnect and rare Child variant. |
| 50 | Ragdoll display still incorrect | T-pose, local native body, complete remote limbs, selected/common/custom rigs, retry/fallback and drift; linked to 5/38. |
| 51 | Client appliance plug/unplug | Near/far endpoints, short/long cables, extensions, occupancy, unpowered outlets and callback/load timing. Nearby failures may have another cause. |
| 52 | Client token display | Identify the actual counter/UI and reproduce; do not assume credits. |
| 53 | Base light flicker | Actual keys/overridden setters, component readiness, loading/late join and visible timing. Separate reliable messages do not ensure frame-exact alignment. |
| 54 | Shared computer display/activity | Laptop/portable-PC tabs, text, scroll, visibility, cursor/input/activity and operator body. |
| 55 | Computer ON/OFF | During animation/loading, power loss/restoration, failed delivery, late join and reconnect; separate from water short-circuit/damage. |
| 56 | Ejected ZIP becomes a huge owo | Actual inserted/ejected class, model and scale on both peers. The bool-wrapper defect is not a proven cause of the symptom. |
| 57 | Opening/eating cans | World/held open/eat/final portion, stow/re-equip, drop/pickup, simultaneous consumption and reconnect. |
| 58 | Items disappear on workbench | Reproduce simple-placement disappearance with old rifle/components; capture role, exact class/key and sweep/drop logs. |

## Entity behavior and gameplay follow-ups

Every group in TODO and every gap in ENTITY_COVERAGE remains open until its
class-specific behavior and multiplayer acceptance are covered. Finish missing
native adapters, child births, phase/action/audio/effect state, damage/death and
join snapshots. Do not add unknown names blindly to an allowlist or enable
debug/test actors merely to fill the taxonomy.

### Grayboar

- Resolve remaining network-time and joined-audio coordination. Local queue-age
  compensation does not cover network transit; live cues start on arrival.
  Verify current-action joins without restarting one-shot sounds.
- Verify locomotion/movement loops, visible turn/pitch and host target selection.
  Test native helper task completion, old target delegates, late/replaced
  possession, save adoption and held-drive handoffs.
- Exercise repeated/simultaneous nonlethal hits, native playback failures,
  attack/roar/death timing, client weapon damage, single corpse/loot and retirement.
- Verify collision damage, lightning replacement propagation, charged explosion
  timers, audiovisual effects, owner punches and spin.
- Test damage delivery under saturation and directional hit presentation, and
  investigate independently spawned client twins if reproduced.

### Kavotia and other families

Kavotia still needs fire/laying montage, speech and canonical beam/impact/damage
adapters, exact phase-time alignment and live spawn/join acceptance. Active flags
do not establish montage/timeline playback position.

Follow the current per-class map for Rozitals, Antibreather/Fossilhound,
Arirals, Greys/firetank, Geoms, UFOs, fauna, anomalous creatures, demons,
forbidden entities, Living Erie/Girufus/Rufus and gnomes/Krampus.
UFOs need child/timeline state; the gun stealer needs canonical prop ownership
and attachment. Keep live, zombie and angry flesh Erie distinct; do not treat
every eye-named actor as eyer.

### Wisp and personal horrors

Verify animation-not-ready recovery and bounded expiry, actual capture montage,
socket holding and release on both roles. Test pawn replacement/recycling,
native death, world travel, disconnect and a new grab during reentrant release.
Delayed tear/grab/death work must never migrate to a successor pawn or world.
Personal Insomniac/nightmare conditions remain local.

## Additional multiplayer acceptance

| Area | Required checks |
|---|---|
| Placement | Refusal/timeout recovery, late PropSpawn adoption, both reply/bind orders, device ejects, containers and live food/drive state. |
| Coins | Contest the same coin through host/client E and overlap; one host credit, accepted mirror removal, refusal retention and client-local map coins. |
| Cremator | Door/paint-only lever on both peers and measured door FPS. |
| Bodies | Common/custom rigs and selected limbs, entry/get-up/lost-body recovery, resting drift, slopes, moving props, walls, latency and teleport. Preserve rig exclusions and selected-model displays. |
| Sleep | Different beds, mattress/moving/exhaustion anchors, sleep/sit/ragdoll/laptop transitions and sleeping effect suppression. Native playerSleepingPawn remains the camera/input pawn. |
| ATV | Existing saved second-ATV positions, simultaneous carry/mount and stale menus. |
| Disks/racks | Client sv.request/hashcodes, exact floppy/ZIP text including line breaks, filled-rack save/rejoin and a slow joining peer. |
| Whole package | Swap host/client roles; two clients contest objects; late join/rejoin/disconnect/save/reload; partial/empty consumables, full inventory, equipped items/container rows, loose cables, flashlight crank/low battery and previews. |

Sleeping-body native evidence remains in the workspace `_research/`:
`sleep_2166.txt`, `wakeup_2166.txt`, `sleep_graph_2166.txt`,
`sleep_full_2166.txt` and `objects__misc__playerSleepingPawn_2164.txt`.

## Before a separately requested release

Update that release's package identity checks. `package/verify_current_build.ps1`
validates current source/DLL; archived 2154 package scripts describe b2156.
Commit, push, game installation and publication require an explicit request.
No other-agent workflow. Update active documents in place; do not create
numbered trackers, backup source trees or historical test-output copies.
