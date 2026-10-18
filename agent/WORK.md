# Unified Work — development protocol 2153 (continued 2152); packaged release 2151

## Development 2152 - entity category synchronization (19 October 2026)

### Grayboar body movement follow-up (19 October 2026)

Native grayboar_C is a prop whose ReceiveTick applies force to StaticMesh; its
spawned grayboarPawn_C is attached to the body and supplies AI/pathfinding. A
Character pose for that helper cannot replace the body's physical motion.

Added grayboar_sync: a class-scoped pre-body ReceiveTick watch claims the registered
host prop in prop_drive_host. The existing stream supplies transform interpolation,
stationary repair, join resend, hand arbitration and liveness cleanup. No second
entity identity or per-frame world scan is created. Missing authoring retries on
the next native tick. Client, stopped-session, mirror/peer-range and unregistered
bodies cannot author this host stream. Disconnect clears the feeder's session.
The inherited grayboar ReceiveTick also covers variants using the same body.

run_entity_sync.ps1 passes three actual production regressions (NPC orientation,
boar feeder, prop-drive sender/receiver) and eight production-unit compilations.
The old prop-drive harness received no-op collision-feeder doubles for its new
production dependency; collision behavior is outside this movement test. Native
checks confirm the tick's physical-force call and helper spawn/attachment. Full
Release DLL b2153 builds successfully. All 9 package/check_2152.ps1 runners pass;
updated DLL hash is in build/release-2152-logs/entity-validation.json.
No new wire layout change in this follow-up.

This completes the host body movement feed only. Client AI/helper coordination,
target choice, attacks, montages/audio, corpse/loot behavior and multiplayer physics
remain OPEN. Do not equate the new transform feed with a completed boar family.
Restored all 14 Ukrainian taxonomy labels previously damaged by shell-pipe encoding.

Owner requested the complete entity taxonomy below, including behavior/functions,
not just visible transforms. This request is OPEN. Existing allowlist membership
only establishes a transport path; it does not prove attack, interaction, audio,
local-player effects, late-join phase, or multiplayer gameplay correctness.

Implemented in this pass:
- Added the native EX_CallMath spawn sources ticker_fossilhoundSpawner_C and
  ticker_yellowWispSpawner_C. Clients already suppress those tickers; previously
  the host catch omitted their output, so their creatures could be missing remotely.
- NPC poses now carry/interpolate pitch and roll as well as yaw, with shortest arcs,
  first-pose/teleport snap and rotation-only updates. This fixes lost actor tilt,
  not creature-specific montage or child-component animation.
- Removed grayboar_C from the Character NPC list: the native class derives from
  prop_C and must never receive ACharacter mesh/movement offsets. Its separate
  grayboarPawn_C derives from Character. Body/helper AI synchronization remains OPEN.
- Native evidence confirms the zombie-deer, skeleton, bloody skeleton, SKerfur-O
  and zombie-Erie classes inherit the existing zombie family; cheese-slither inherits
  goreSlither. Their presence in a family does not verify their special functions.

EntityPose is now 52 bytes, capped at 26 entries (1376-byte datagram). The wire protocol is
2153 because old 2152 peers parse a different row size; both peers must update.
The task continues the 2152 source work; this is not a published release.
The published/prepared 2151 ZIP is not upgraded by these source changes.

Validation: run_entity_sync.ps1 passes the actual production NPC interpolation
regression (orientation, teleport, shortest arcs, stationary turns, MTU) and six
production-unit compilations. entity_sync_native_test.py passes native spawn-path,
class-layout and descendant evidence, including Character ancestry for all 25
current NPC allowlist roots. package/check_2152.ps1 passes all 9 runners.
Full MSVC Release build passed and DLL resources verify 0.9.0n b2153.
All 9 runners passed again with protocol 2153. Hash/validation evidence:
build/release-2152-logs/entity-validation.json. No ZIP or gameplay verification.
The existing build script/directory retain their historical 2152 names. Packaging
scripts still target release 2152 and must be updated before preparing a 2153 ZIP. Two-player gameplay
and all entity-specific effects remain unverified.

### Complete requested entity coverage and remaining work

| Requested group and entities | Verified source coverage / remaining work |
|---|---|
| Розітали: Велика Піраміда; Кавотія; Пагоніопьло-Ігетіс; Скаути; Солтомія | piramid2 + piramid_sync, igetis and soltomiaCleaning have existing routes. Exact localized-name associations, phase/actions, destruction and late join need native and multiplayer verification; Kavotia/scouts not mapped in this pass. |
| Творіння Розіталів: Антибризер; Викопна Гончача | antibreather/fossilhound are Character families. Fossilhound ticker catch fixed. Antibreather's player/cave-dependent spawner and cross-peer attack routing remain open. |
| Арірали | arirFollower/ariral_shooter/ariral_pigBeater existing NPC routes. arirGunStealer is a distinct Character, currently missing from the list; stealing/held-item ownership and event phases must be implemented before declaring the family complete. |
| Сірі; Вогнеметний танк; Одержимий кабан | grayTest NPC and firetank world-actor routes exist. Firetank is Pawn; turret/fire/fuel/damage are not established by its transform stream. grayboar prop vs grayboarPawn Character split confirmed; wrong body NPC registration removed; host body motion now feeds prop drive. Helper AI, attacks and animation remain open. |
| Геоми: Раміїл; Яйцеподібний геом | eg and superEgger routes exist; mapping these to user-visible names and their special actions is not yet verified. Ramiel remains unmapped. |
| НЛО: Бумеранг; Джоїл; Корабель Сірих; Мідас; Пілюля; Трифо; Кулі | Existing routes include ufoDropper variants, arirShip, skyUfo, morningUfo, rozitBorg, kocker and tentacleBallsFollower. Exact seven-name mapping, child spawns, beam/material/audio phases and late join remain open. |
| Флора/фауна: Олень; Риба; Таракан | deer and deer_ded are Character lineage but have no NPC mirror route. Fish class/behavior not yet mapped. cockroachMaster has a dedicated existing roach lane; movement, eating, crushing and late-join gameplay remain to verify. |
| Blue Brain Cheese; Restocker; Eye; Вольфганг; Дерево з очима; Живий Гриб; Космічна медуза | cheese-slither is a goreSlither descendant; funguy and jellyfish have routes. eggyWolfgang derives from actor_save (not Character). Restocker/Eye/Wolfgang/tree exact behavior routes remain open; do not assume eyer identifies every eyes entity. |
| Кістяна людина; Кривавий скелет; Цікаві очі; Мімік; М'ясний слизень; Олень-зомбі | Zombie skeleton/bloody/deer lineage confirmed; goreSlither and owner-entity eyer paths exist. prop_impostor/impostorPropsMaster has prior mimic work. Exact bone-man/curious-eyes mapping and attacks/effects need verification. |
| Привид; Примарна колода; Світлячки; Ходячий манекен; Чорна пляма | Keep personal spawn conditions where native behavior requires them. Firefly has an existing personal/ambient path; remaining names, latent mannequin lifecycle, effects and phase synchronization need native audit. |
| Демони: SKerfur-O; Безокий Інсомник; Демон Безсоння; Покинутий Керфур; М'ясна Ейрі; Статуя цапа; Статуя Столаса; Фурфур | SKerfur-O zombie lineage confirmed. Insomniac and Child remain peer-local under previously agreed personal sleep behavior. npc_angryErieFlesh is a distinct Character not covered by the zombie-Erie descendant. Other demons/statues/rituals require individual behavior and run-end/target review. |
| Заборонені: Black Friend; CorpseWalker; Crasher; Fast-shadow Figure; Fridge; Interloper; The Exit Door; The Head; Басейнова сутність; Гірські Люди; Проклятий павук; Рунна сфера; Тіньова фігура; Фальшивий Кел; Чорна сфера | Exact native classes, spawn availability and local/global effects are not yet mapped. None is declared synchronized by this pass. Do not force debug/test actors into normal gameplay merely to populate this list. |
| Жартівні: Жива Ейрі; Жируфус; Руфус | Exact live-Erie/Girufus mapping and Rufus interactions/animation/ownership remain open. Do not confuse live Erie with zombie-Erie or angryErieFlesh. |
| Новорічні: Гноми; Крампус | npc_krampus is already in NPC lane; attacks/events need testing. Gnomes' classes, spawn conditions, interaction and shared lifetime remain open. |

Next implementation: resolve the boar body/helper ownership without duplicate brains;
then native per-family action/state transport and missing creature paths, including
Ariral gun-stealing, deer, mannequin and compound centipede. Do not add unknown names
blindly to an allowlist. Run host/client spawn, target selection, action, death,
disconnect and mid-event join for each row before closing it.

## Development 2152 - wisp animation readiness

### 2152 follow-up: puppet hold iteration across native callbacks (19 October 2026)

Puppet hold Tick previously retained a vector iterator across socket/location calls.
Native callbacks could clear, erase or grow that vector, invalidating the iterator
and allowing a stale post-disconnect movement write. Tick now iterates a snapshot,
checks a hold revision after socket lookup and before continuing after movement,
and validates both actors with CachedObjRef. It rechecks the current puppet binding
before moving it. Peer leave/re-engage invalidates the old iteration even for the
same slot and wisp. Invalid peer slots are refused. Disconnect clears puppet holds
before native self-release callbacks so callback-installed state is not erased later.

run_wisp_animation.ps1 passes the three regression executables and compilation of
all four production units. Actual hold tests cover socket-time disconnect, peer
leave/re-engage, puppet/wisp recycling, puppet replacement, movement-time teardown
and vector growth, normal multi-hold drive, invalid slots and stale hold removal.
Gameplay remains unverified; no DLL/ZIP, game installation or publication performed.

### 2152 follow-up: bind delayed grab actions to the original pawn

The death arm previously captured Local only at its deadline, so replacing the
pawn earlier in the grab window could kill the successor. OnWispGrab now stamps
the victim's CachedObjRef at receipt, before native attachment callbacks. Tick
consumes that reference with the deadline and revalidates identity after release.
Disconnect clears the reference. Invalid wisp/victim IDs and absent local pawns
cannot arm a death. Hold retries likewise retain their initial victim rather than
attaching whichever pawn Local names on a later frame.

run_wisp_animation.ps1 passes all three regression executables and compilation of
four production units. Added cases exercise replacement and slot recycling before
the death deadline, invalid/absent-victim grab rejection, disconnect reference
cleanup, pending-attach replacement/recycling rejection, and normal delayed attach.
Host/client gameplay remains pending. No new DLL/ZIP, installation or publication.

### 2152 follow-up: hold victim identity and attachment cancellation

ReleaseSelf now retains the actual attached pawn through CachedObjRef instead of
resolving Local again at cleanup time. A replacement pawn receives no detach or
movement reset; the original pawn is released while its slot/world remain valid.
A recycled original slot is rejected. Attachment attempts also compare their hold
generation and victim identity after native callbacks, so disconnect or a newly
installed pending hold cannot be overwritten by the old attempt's success.

run_wisp_animation.ps1 passes all three regressions and compiles all four production
units. The actual hold regression covers original-pawn cleanup after Local changes
or disappears, recycled slots, disconnect during initial/retry attachment, and
reentrant replacement with both attached and pending holds. Gameplay remains
unverified; no 2152 DLL/ZIP was prepared and the 2151 package is unchanged.

### 2152 follow-up: release-time death cancellation and reentrant cleanup

The due-death Tick previously continued after ReleaseSelf even if native release
callbacks disconnected the session or replaced the local pawn. It now consumes the
old death arm before release, captures the intended victim's CachedObjRef, and checks
the session epoch, pawn identity and slot serial after release. A successor pawn
cannot receive that pending kill. An absent/dead victim still gets hold cleanup.
OnDisconnect clears its death arm before invoking release callbacks.

ReleaseSelf now clears hold state before calling native detach/movement cleanup.
Nested teardown is a no-op for the consumed hold, and a callback's newly installed
hold is not wiped out when the outer release returns.

run_wisp_animation.ps1 passes three regression executables: receiver lifecycle,
actual native montage wrapper, and actual hold-release implementation. New cases
cover release-triggered disconnect, pawn replacement and recycling, one-shot normal
kill, absent victim cleanup, recursive teardown and new-hold preservation. Four
production units compile (tear mirror, grab hold, attack sync, native wisp wrapper).
No live gameplay verification or 2152 package; prepared 2151 ZIP remains unchanged.


Follow-up: PlayFatalityMontage previously returned true after a successful ProcessEvent
dispatch even if Montage_Play itself returned zero (playback refused). It now requires
successful parameter writes, dispatch and a readable finite positive ReturnValue.
Failures propagate to the bounded receiver retry. CachedObjRef guards both actor and
AnimInstance across native playback and section-name conversion; a recycled object
cannot receive the later jump. An unavailable/malformed optional section jump does
not turn already successful playback into a repeated restart.

run_wisp_animation.ps1 now also runs wisp_montage_test.cpp against actual production
wisp.cpp, covering zero/negative/NaN/infinite or unreadable results, parameter/dispatch
failure, actor/AnimInstance recycling, recovery and absent optional section. Receiver
regression and compilation of wisp.cpp, wisp_attack_sync.cpp and wisp_tear_mirror.cpp
pass. No 2152 package or two-player gameplay verification yet.

The owner continued bug fixing in 2152 after packaging 2151. Current source protocol
is 2152; the prepared delivery ZIP remains 0.9.2151 and is not replaced.

A live wisp actor did not guarantee a ready AnimInstance or montage. The receiver
previously consumed WispTear even when PlayFatalityMontage returned false, permanently
losing capture animation. PlayTearOnWisp now reports failure. Pending attacks retry
at 100ms intervals within the original 3.5s window; duplicate pending messages neither
bypass backoff nor extend the deadline. Puppet socket holding begins as soon as the
actor exists, independently of animation readiness. Host-local attacks now use the
same receiver/retry path. Actor lifetime and disconnect epoch are checked after calls.

run_wisp_animation.ps1 passes animation-not-ready then recovery, throttling, unchanged
deadline, expiry, no replay after success, delayed actor, sender/slot rejection and
reentrant disconnect both on queued and immediate failed playback. Both production
units (wisp_tear_mirror.cpp, wisp_attack_sync.cpp) compile with MSVC. This is a focused
offline check; live host/client animation remains unverified. No 2152 DLL/ZIP, game
installation, commit/push or publication has been performed.


## Release 2151 preparation - 18 October 2026

Current release/protocol: 2151; both peers must update. User reports were from build
2050; missing model names are unknown. The current changes cover personal horrors,
ragdoll retry/rig/pelvis/get-up handling, partial NPC class-cache installation and
first-spawn matching, deferred wisp capture and non-chair capture pose, host-authoritative
Cremator door/paint-only lever with cached searches, held physgun/gravgun cosmetic
replication, recursive custom-skin discovery and failed-preview retry/hiding.
No claim that unknown missing assets or static preview mappings are fixed.
Seven player-report regression cases, ragdoll retry, ragdoll skin, personal sleep and
two native-evidence checks pass (12 focused checks). Clean Release build and ZIP
verification evidence: package/RELEASE_2151.md and build/release-2151-logs/.
Gameplay reports remain open pending two-player testing, including measured door FPS.
No game installation or online publication is performed during preparation.


Consolidated 18 October 2026 at the owner's request. Release 2150 includes all
accumulated source work through development batch 2192, including later work IDs.
Those numbers are historical work IDs, not newer packaged releases.
Every row below is still open for implementation or actual two-player verification.
"Code ready" and focused offline checks do not close a gameplay report.
This is the single active work list; completed per-change WORK reports have been
removed after their unresolved follow-ups were captured here.
Keep implementation notes and follow-ups here; update existing rows instead of
creating a new numbered WORK report or a parallel backlog.

## First checks on both peers using the new 2150 package

### 2151 — model-authored ragdoll physics (5/38/50), 18 October 2026

ConfigurePlayerRagdollMesh previously rejected every skeleton other than the
hardcoded Kerfur skeleton, even when the selected mesh had its own valid authored
PhysicsAsset. Such a remote body never started and the upright puppet remained.
It now prefers the selected mesh's PhysicsAsset if it has at least three bodies,
enough live constraint templates, unique named bodies including pelvis, and all
physical bones resolve on the selected component. The standard 17-body Kerfur
asset remains the fallback only for its matching common skeleton. A two-body
default still falls back; an unsupported foreign rig is never force-fitted.
The selected physics resource is guarded with CachedObjRef through appearance
capture and target setup, including callback-induced recycling. Pelvis is required
because attachment, velocity synchronization and positional correction use it.

Native asset inspection confirmed SkeletalBodySetups (17), ConstraintSetup (16),
and unique physical bone names including pelvis on kerfurOmegaV1_Physics. This
checks compatibility prerequisites, not the complete constraint graph topology or
actual physics behavior. run_ragdoll_skin.ps1 passes own-rig selection without
loading the fallback, common-rig fallback for an incomplete default, foreign-rig
rejection with missing bones/pelvis/joints, duplicate bones, stale physics resources,
appearance bounds and setup failure propagation. Production engine_ragdoll_skin.cpp
compiles; git diff --check passes. The existing 2144 runner also passed.
No DLL/ZIP or gameplay test. T-pose reproduction, local native ragdoll and exact
limb replication remain open. Verify a common skin and a custom authored rig on
host/client, entry/get-up, body-loss retry and positional convergence in gameplay.

### 2151 — ragdoll positional drift (5/50), 18 October 2026

The sender already publishes pelvis world coordinates, but the receiver discarded
them and wrote only velocities. Different collisions accumulated permanent position
error; a stationary sender could never correct the mirror's resting location.
RagdollDisplay now measures its pelvis and translates the entire articulated mesh
toward the received position. First successful sample and displacement over 10m align
immediately; ordinary drift corrects by 25% per fresh packet, with a 2cm dead zone.
Moving the mesh with K2_AddWorldOffset, bTeleport=true and bSweep=false preserves
velocities and the relative joint pose; existing pelvis velocity feed remains.
An unavailable getter/translation can retry on the next packet. Coordinates and
velocities are checked for finiteness; target coordinates must fit kMaxCoord.
New bodies clear alignment state. No packet layout or release metadata change.

Expanded run_ragdoll_retry.ps1 passes initial alignment, resting drift convergence,
dead zone, large displacement, invalid coordinates, preserved velocity values,
failed translation retry and recreated-body alignment, plus earlier lifecycle cases.
Production remote_player_ragdoll.cpp and engine_playerragdoll.cpp compile with MSVC;
git diff --check passes. No live physics/host-client verification and no DLL/ZIP.
This addresses displacement, not T-pose or exact limb replication. Check slopes,
moving props, walls, high latency, get-up, selected skins and teleport in gameplay.
Engine API evidence: Epic USceneComponent K2_AddWorldOffset (world-space delta) and
K2_SetWorldLocation/K2_AddWorldTransform teleport parameter (keeps physics velocity).
The reflected parameter writes remain checked for the game's UE4 runtime.

### 2151 — remote ragdoll retry (5/50), 18 October 2026

Found a state-machine failure: OnWireBit latched ragdoll=true before Start, then
ignored every subsequent true packet even if spawn/configuration/attachment failed.
DriveAttached also restored the standing puppet after a body died, but the true
wire latch prevented recreation until the player exited and entered ragdoll again.
OnWireBit now retries an inactive display after 500ms while the sender remains
ragdolled. A live display is never respawned; a new rising edge starts immediately;
falling edges and teardown clear the retry deadline. No placeholder body is added.

run_ragdoll_retry.ps1 passed regression cases for transient spawn failure, packet
burst throttling, no duplicate live body, lost body, failed attachment/orphan
destruction, get-up and teardown. Production remote_player_ragdoll.cpp compiles.
git diff --check passed. This fixes failure recovery, not the entire T-pose report:
selected mesh/physics compatibility, local native body, remote drift and exact
limb pose still need investigation and live host/client verification. No DLL/ZIP.

### 2151 — insomnia demon (49), 18 October 2026

Native evidence: `mainGamemode.trySpawnInsomniac` rolls from the local saveSlot.sleep
below 20, refuses isSleep, and locates the spawn around camera 0. The ticker spawns
insomniac_C (or its rare Child variant), stores the normal actor in gamemode.insomniac
and clears it on destruction. The demon's own graph moves toward GetPlayerPawn(0),
checks local sleep/distance, drives the local camera/stopMovement during grab, punches
the local mainPlayer for spooker, and calls gamemode.createDream for the normal attack.
It has no general remote-pawn target path. The existing host-only ticker gate and
parked NPC mirror prevented the client's personal encounter from running.

Removed the ticker refusal, the insomniac NPC allowlist entry (also excludes derived
Child), and the obsolete EX-spawn source rows. Each peer now runs its native personal
encounter against its own sleep state; the host's demon is not a shared world NPC.
No new packet or version metadata change is needed for this source fix. Last package
remains 2150; current work targets 2151. The owner's follow-up also changes sleeping
nightmare handling as described below.

Validation: `tests/insomnia_native_test.py` passed against the locally extracted game
bytecode; `tests/run_insomnia.ps1` compiled both changed C++ translation units with
MSVC; git diff --check passed. No DLL/ZIP, installation or live two-player test.
Gameplay: low-sleep client sees pursuit and grab/dream; rested client does not inherit
the host's encounter; host behavior remains native; test simultaneous low sleep,
sleep recovery/despawn, disconnect/reconnect and rare Child behavior if reproducible.

### 2151 — personal sleeping nightmares (10), 18 October 2026

Owner requested both insomnia encounters and sleeping nightmares only for players
whose own conditions match. Native mainGamemode's sleep loop gates on isSleep and
nightmaresEnabled, rolls bedSleepProb, then calls wakeup before createDream. The bed
probability uses the local bed.dreamProb, or 0.15 without a bed; dreamProbability=-1
preserves this native path. This is a separate condition set from Insomniac's sleep<20.

Both roles now restore the native probability during shared acceleration; waiting
sleepers keep the existing suppression. Removed host dream capture/broadcast and
client forced replay/retry, which bypassed the recipient's conditions and could
overwrite a personal scene. DreamScene's numeric wire ID remains reserved; its
receiver ignores legacy broadcasts. Local nightmare wake edges still use SleepState
to end acceleration for everyone. Only the affected player enters that dream; other
players wake without being forced into it. Insomniac's own createDream path remains
native and personal. Existing personal scare triggers retain their native checks.

Validation: production sleep_sync.cpp compiles with MSVC. run_sleep_personal.ps1
passes client/host waiting policy, client native probability, sender authority,
client clamp, client nightmare falling-edge report, host interrupted End without
full rest, legacy packet rejection, wake-before-refill and disconnect restoration.
insomnia_native_test.py also verifies actual sleeping/enabled/bed/random gates and
wake-before-dream ordering. git diff --check passes. No DLL/ZIP or gameplay test.
Gameplay remains required with nightmares disabled on one peer, different beds,
simultaneous wins, one awake/rested peer, Insomniac attacks and reconnect.

- [ ] Insomnia demon spawn, visibility, client targeting and attacks (49).
- [ ] Selected-model ragdoll pose/entry/exit on each peer (5/50); do not restore the plushie fallback.
- [ ] Client appliance plug/unplug, short/long cables, extensions and unpowered outlets (51).
- [ ] Identify which token counter/UI is desynchronized; do not assume money (52).
- [ ] Shared base flicker, late join and load recovery (53).
- [ ] Computer shared screen/operator/activity (54), ON/OFF during animation, power loss and reconnect (55).
- [ ] ZIP insertion/ejection: actual returned item, class, model and scale on both peers (56).
- [ ] Cans: open/eat/final portion, held/world, stow/re-equip, simultaneous use and reconnect (57).
- [ ] Workbench: simple placement and disappearance after 1–2 seconds or later (58).
  Record the exact item, authoring peer and logs; crafting is not required by the report.

## Complete open tracker carried into 2150, with 2151 follow-ups

The statuses retain their original work IDs and evidence. Duplicate reports stay linked
rather than being silently dropped. Issues 6 (mining) and 10 (nightmares) were reported
working; issue 41 (old basement wall) remains explicitly excluded by the owner.

| ID | Problem | Remaining work / evidence |
|---|---|---|
|1|Client cannot break props|Код готов2143, дополнительная защита2159: FIFO повторов ударов и полная проверка границ параметров; needs new-build gameplay result|
|2|Prop jitter/rollback|Частично2151: one receiver drive per prop and late losing-release guard; native local grip contention/general jitter remain|
|3|Pushing one prop with another|Частично2157/2159: local/remote grips and deferred collision chains;2159 corrected empty legacy hit watch to actual hitCpp and checked component owner integration. Client-grab host contacts/convergence need two-player testing|
|4|Client R+LMB duplication|Код готов2143; needs gameplay result|
|5|Selected-model ragdoll|Reopened: owner still reports incorrect ragdoll on2168 (report50). Preserve2144 code, but do not treat this behavior as fixed.|
|7|Extinguisher foam|Код готов2145|
|8|Loose power outlet floating|Код готов2145|
|9|Physics gun / both gun effects|Частично2158: native physical-gun target now feeds ordinary prop movement/release and local ownership, including soft/super descendants. Both guns' beam effects, scale/freeze outcomes, non-prop targets and multiplayer contention remain; gameplay needed|
|10|Nightmares|Previously reported working. 2151 requested personal enabled/bed/chance behavior implemented; shared night interrupt retained. Native checks, state regressions and compile pass; new behavior needs multiplayer gameplay.|
|11|Rotating chair|Код готов2144|
|12|Console commands|Частично2154/2155: общий ввод/ответы/ошибки/clr и недавняя история при позднем входе; полные события, местные эффекты и единый порядок одновременного ввода остаются|
|13|Funny rule music|Частично2154/2155/2157: Madness music/HUD и музыка семейства Maxwell в мире/выбранной руке; точная фаза трека и прочие события остаются|
|14|Funny rule display|Частично2154: Madness HUD; прочие события остаются|
|15|Toy sounds|Частично2156/2157: Maxwell squeak/pitch world and selected hand; abplush Alt-use world/hand; ordered retries. Other toys and squeeze gameplay remain; needs gameplay test|
|16|Stutters when stowing items|Частично2157: removed full object-array lookups of unloaded claimed classes from save-coverage/pocket path, serial-safe cache. No frame profile or in-game confirmation; residual stutters remain unconfirmed.2165 follow-up removes recurring unresolved-class full walks in sleep resolver; production compile passed, no runtime profiling|
|17|Pressure-washer stream|Код готов2149; visual/audio only, impacts remain with water work|
|18|Computer sparks/damage presentation|Код готов2150 for native water shortCircuit; needs gameplay result|
|19|Computer shutdown from client damage|Код готов2150 for native water shortCircuit; ordinary addDamage is empty in this cooked class|
|20|Bucket water disappearing|Частично2153/2159/2169: world changed-state publication/empty repair/passive simulation;2159 adds remote selected-hand level/soap-presence/alcohol display, preserving owner's actual contents.2169 adds current held water/actual soap count/alcohol to the personal profile, with validated ordered receipt. Simultaneous writers/in-flight record arbitration and pouring gameplay remain; gameplay needed|
|21|Cement / trowel|Частично2153/2160/2166/2168/2169: trowel hand display; world units/dry and two-bag transfer publication, passive tick, exhausted-bucket guard;2166 adds concrete-bucket hand fill/dry/empty display and no-spend client wall-repair refusal. 2168 refreshes outgoing held-trowel profile concrete from the matching local hand, including zero; identity mismatches defer the poll. 2169 adds live held concrete-bucket/cement-bag profile amounts/dry and validated ordered profile receipt. Concurrent host transactions, dryTimer handoff and actual client wall repair remain; gameplay needed|
|22|Brick-wall outline and build|Partial2166/2167/2168: placement/progress, existing-wall enrollment, solo-host tracking and native-key save twins;2168 adds host-validated client melee damage to completed walls. Five focused2168 tests/four production compilations pass. Client repair, atomic material transactions, creative build and effects remain; no multiplayer test.|
|23|Depleted flashlight still lit remotely|Код готов2145; compared with archive2151, retained and regression passed|
|24|Client cannot pick up anchored hook|Код готов2152: host retires keyed anchor, creates one ordinary pickup near requesting client; client then picks it up normally (not an automatic inventory grant). Includes saved wall anchors, retries and late-join retirement. Gameplay pending|
|25|Battery levels|Частично2146/2147/2151; pending received record applied before pocket capture; direct hand/in-transit state still needs review|
|26|Airy event|Осталось|
|27|Main-room computer walls becoming physics props|Код общего пути2158/2159: независимая регистрация новых деталей и движение до остановки; подтверждены7 cubicleP-частей через обычную фабрику, проверены независимые появления. Нужна игра вдвоём на точных панелях. Снятие УДАРОМ, исчезновение У КЛИЕНТА; кто бьёт, не установлено. Пользователь предполагает все снимаемые ударами предметы; custom-пути пока не доказаны. Это не исключённые стены подвала|
|28|Drone sound difference|Код готов2143|
|29|Crowbar static state / dragging|Частично2148; unusual wall/parent/replacement classes unconfirmed|
|30|Second ATV in corridor|Код готов2149 for new companion creation; old saved positions unchanged|
|31|Sleeping body pose|Осталось; native sleepingPawn is camera/input, not a ready-made body. Anchor/ragdoll/selected-model integration needs implementation.|
|32|Not enough space on equipment|Код готов2146 for ALL equippable items|
|33|Open container retains removed row|Код готов2146|
|34|Client crate/package loot invisible|Частично2148/2150/2158: MRE floor/hand, host finished-birth retry; keyed spawn hook no longer waits for all optional ambient assets/observer registration. Other packages/client-local routes remain, gameplay pending|
|35|Centipede / other living creatures invisible|Осталось|
|36|Computer chair rotation (duplicate report)|Same as11,2144|
|37|Riding a carried ATV / flight|Код готов2148/2149; actual menu and race convergence included|
|38|Skin model list / previews|Частично2151/2152: original sixteen preview tiles plus supplied Mynet/skeleton images; both requested meshes were already builtins. Catalog regression passed; runtime appearance/animation/ragdoll still needs gameplay, other preview gaps remain|
|39|Consumables refill on pickup/drop|Частично2146/2147/2151; food earlier fix retained, pending canonical record drained before pocket capture; other transitions remain|
|40|Flesh Eerie summon / visibility / aggro|Осталось|
|42|Event teleport to the triggering/base-entering player|Код2161: actual agrav occupancy and runtime-verified shared armed boxes; client pose fallback, initiator gather, repeat/reentry guards and send retries. Two focused checks/five TUs passed; multiplayer pending. Other custom paths and full event effects remain separate|
|43|Safe cracking|Код2162: both dial success and hacked converge on opened; client completion to host, single loot, saved lock publication/retries and host-lock protection against early/stale records. Initial unlocked drop record preserved. Gameplay pending; dial UI and generic child hinge identity unchanged|
|44|Sponge wetting|Частично2160/2169: native saved power/wetness now publishes after enterWater/wash/clamp/LMB; load no echo and final-use tail checked. 2169 adds current held wetness to outgoing profiles. Soap stock/foam/surface effects and personal-hand transitions remain unverified; two-player test needed|
|45|Grill ignition|Код2162: host ignition/off, active/coals/burn snapshot and late join, passive client heating/fuel tick, immediate coal record and request retries. Gameplay pending. Concurrent coal transactions/stale records, lid and personal item transitions remain.|
|46|Client-placed crops/faucets/hoses disappear after save reload|Частично2163: exact native placement -> host save actors, completed hose points/links, plant/pot identity, retry/dedup/retirement/late join. Two tests/five TUs passed; game save/reload not tested. Later gardening, accepted hose rerouting, concurrency and pre-session loaded-actor enrollment remain.|
|47|Black disk/yellow-ring sky event starts on client independently|Visual code2164; common schedule-entry authority2165 covers all scheduled rows, boot/reflected calls and passEvents protection. Local checks pass; gameplay pending. Audio/emails/pre-session latent effects remain separate. This is NOT Bad Sun or black fog|
|48|Mimic (user identified; trees/white pillars/radar report)|Code2165: prop_impostor_C and impostorPropsMaster_C identified; client random generation and local-camera culling disabled. Reuses keyed prop class/disguise/birth/death/join; host controls lifetime. Focused checks/compilation pass; multiplayer/rendering/damage verification pending. Related to35; other creatures remain open|
|49|Insomnia demon neither appears for nor attacks the client|2151 code ready: restored native peer-local ticker and excluded insomniac_C and its subclasses from the NPC mirror lane. Native spawn/target/grab paths verified; focused compile passed. Host/client gameplay remains pending.|
|50|Ragdoll display is still incorrect|2151 partial: spawn/lost-body retry, pelvis correction and model-authored physics selection added. Valid own rigs may use a different skeleton; common fallback retained, bone/pelvis/joint and stale-asset guards tested. Focused regressions and compile pass. T-pose in gameplay, local native body and exact limbs remain open; revisit5/38.|
|51|Client cannot plug electrical appliances into outlets|Partial2170: socket-endpoint authority fixes refusal at the far end of long cables; native length/occupancy and callback lifetime guarded. Client connect/graph parameter-frame bounds now fail closed. Two focused executables, native-bytecode check and two TUs pass. Report remains open for actual2168 symptom/gameplay verification; nearby failures may have another cause. Distinct from loose-outlet floating8.|
|52|Client token display remains desynchronized|Open. Identify the token counter/UI and its authoritative value; do not assume credits or a particular token class without evidence.|
|53|Base light flickering is not synchronized|Partial2177–2180: host-authored ceiling/ambient phases; indexed clients suppress native writes; local ambient settings remain local. Failed applies retain latest state;2180 retries failed/unreadable join snapshots without advancing the all-peer baseline, with peer/world cleanup. Channel/wrapper regressions, prior native checks and five production TUs pass. Actual keys, loading/late join and two-player timing need gameplay verification.|
|54|Computer display does not show the same activity to host/client|Open. Other players cannot see who is using the computer or what they do. Trace shared screen/UI state, cursor/input and operator visibility; console text12 alone is insufficient.|
|55|Computer power-on and power-off are not synchronized|Partial2173: targets survive actor loading and native animation; failed sends and deferred join snapshots retry with latest state. Pending sends clear on peer departure/disconnect; 2171 echo suppression is preserved. Existing animation regression, new delivery/lifecycle regression and two production TUs pass. Still requires two-player checks, power loss/restoration and late join; no release built. Separate from water short-circuit/damage18/19.|
|56|ZIP disk model sometimes becomes a huge owo when ejected|Partial2172: laptop `zip` is now resolved as a masked Unreal bool bitfield, written without disturbing neighboring flags, and hashed as its logical value. Focused wrapper regression and production TU compile pass. This is a concrete wrapper defect but is not proven to cause the reported model/eject symptom; still needs in-game ZIP eject and multiplayer checks.|
|57|Opening canned food and eating it are not synchronized|Partial2176: world open/eat publish records (2174), held can state travels with HandItem (2175), and client profiles refresh live opened/type/foods before hashing (2176). Identity-checked copied records preserve unrelated data. Three profile/record regressions, native evidence and two production TUs pass. Protocol2175. World/held use, reconnect, final portion, stow/re-equip, drop/pickup and simultaneous consumption still need multiplayer verification.|
|58|Some items disappear when placed on the workbench|Open symptom; partial code2181–2186. Owner clarified17 October: simple placement, visible disappearance after1–2 seconds or later; no crafting reported. Items and host/client unknown. Transform retries, local-birth protection and deletion identity rechecks implemented;2186 invalidates old sweep passes on bracket/world/reset changes and blocks recursive reconcile ticks. Focused tests/production compilation pass. Full sweep/gameplay symptom remains unverified.|

## Additional unfinished details retained from earlier TODOs

Mirror-backup audit, 18 October 2026: the retired mirror contains no separate
production implementation to merge. Archived test variants have canonical
counterparts; differences include corrected checkout paths and later regression
coverage. The one-time RBE2143 import was already applied; do not rerun it.
Record retries through work2192 and sweep protection through work2187 are present
in production. run_2188.ps1 and run_2182.ps1 passed again during this audit, including
production compilation. These checks do not close the workbench gameplay report.

Keep these investigation details after removing mirror-backups:

- Sleeping body31: native playerSleepingPawn_C supplies camera/input, not a visible
  sleeping body or a verified mattress anchor. Use the selected player model with
  a verified bed/body anchor and sleep/ragdoll/sitting entry/exit precedence; suppress
  footsteps and inappropriate hand/head effects. Native evidence remains in the
  workspace _research/sleep_2166.txt, wakeup_2166.txt, sleep_graph_2166.txt,
  sleep_full_2166.txt and objects__misc__playerSleepingPawn_2164.txt.
- Cement21/22: verify native held-trowel key continuity in game. A mismatch defers
  the entire profile poll. mainPlayer.updateHold rebuilds the held actor from
  saveSlot.hold[0]; checkEquip reads the live actor but does not update that array.
  Any future repair transaction must coordinate unequip/drop and stock persistence.
- Light53: still verify overridden native setters, actual component readiness,
  late join and visible timing. Later channel retries supersede the old2178 note
  about a missing apply retry; separate reliable messages do not ensure frame-exact
  flicker alignment.

- [ ] Food/package final-bite destruction and subclass eaten() output; preserve local poison/player effects.
  Other package families and client-local loot need explicit provenance, not global replaceProp admission.
- [ ] Rejected client placement may remain awaiting binding: establish acknowledgement/refusal behavior.
- [ ] Coin pickup by E/overlap: one host credit, disappearance only after acceptance; map-placed coins remain local.
- [ ] Console event bodies/AI/damage for Rufus/thiccfus, llama, Madness grunts and variants;
  simultaneous typists' canonical order, periodic/local output and exact music phase remain.
- [ ] Wall repair requires an atomic request/result and exact personal cement stock across
  hand/slot/drop, retry/rejection/disconnect; keep the current no-spend refusal until complete.
  Creative construction, wet-wall timer persistence and exact impact/effects remain.
- [ ] Water/cement/coal concurrent writers and stale in-flight records; dryTimer ownership;
  sponge soapAmount/foam/surfaces, grill lid/final cooked food and personal-hand transitions.
- [ ] Enroll pre-session plants/faucets/hoses; later watering/fertilizing/harvest, pot transitions,
  accepted hose rerouting and concurrent use. Missing historical save objects are not migrated.
- [ ] Sky-event audio, delayed email/bookkeeping and pre-session latent chains; other custom
  event effects/creatures need host phase/cues rather than replaying whole commands on every peer.
- [ ] Stow frame-time capture if residual stutters remain; preserve pending item-state restoration.
- [ ] Skin preview gaps, custom skins and real animation/physics; do not remove compatibility exclusions.
- [ ] Existing saved second-ATV positions are unchanged; test near-simultaneous carry/mount and stale menus.
- [ ] Whole-package multiplayer smoke: host/client role reversal, two clients contesting objects,
  late join/rejoin, disconnect, save/reload, partial/empty consumables, full inventory,
  equipped items, container rows, loose cables, flashlight crank/low battery and model previews.

Exact prior TODO text is preserved in [archive/pre-release-2150](archive/pre-release-2150/).
Tests, native evidence and the issue tracker remain available. Completed per-change
WORK reports have been removed from the active folders; their remaining follow-ups
are captured above. Update this file rather than creating another parallel work list.
