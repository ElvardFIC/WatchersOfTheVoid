# Unified Work - unresolved backlog

[Historical reports 1](history/WORK-1.md)

## Current release 0.9.2157 / b2184 - package-label correction, 22 October 2026

The menu and server browser now show Watchers of the Void 0.9.2157 from
package/release.json. CMake regenerates the label when that metadata changes;
game target 0.9.0n and wire protocol 2184 remain separate compatibility values.
The packer requires matching runtime formatter evidence and verifies the label
again in the freshly unpacked installation's startup log.

PASS Release build, actual shared DisplayVersion formatter at startup, ABI drill,
one impaired LAN run per direction (Scientist and Kerfur, selected articulated
mesh, entry and recovery; 250ms lag,40ms jitter,5percent send loss), and isolated
startup of the full 32-member ZIP. Production/build inputs frozen:2646.
Evidence: build/version-2157-check/verification.json and final logs;
build/package-current/verification.json with archiveStartupVerified=true.
DLL SHA256:fa77a7c20c685840579e452191bc7f5f098152a0aa5f9dc961097392161f2e87
ZIP SHA256:ab9f59c44bf8cc3dc4f2761559c222a1a9e4a4ce6f35cc621d4417bb75e525f8

The gameplay, ragdoll physics and wire implementation are unchanged from the
preceding qualification below. Its complete physics/model/camera matrix was
not rerun on this label-only rebuild. Tests use NullRHI; native widget text
readback and an ordinary rendered/input check are not claimed. The full65 suite,
existing16 fact-row findings and script-name watch capacity remain open.
Entity, original game files and the normal installed profile are untouched.
The same 0.9.2157 filename replaces the previous ZIP; authorship is AquaFishhh.


## Preceding ragdoll qualification 0.9.2157 / b2184 - 22 October 2026

This supersedes the b2183 status below; package number and filename stay unchanged.
Selected-character physics now starts in the local native FinishSpawningActor
post-hook, before camera attachment and native impulses, without the polling pause.
At native recovery attachment, preserve the camera world pose and hand current yaw
to the capsule/controller once, preventing the late return to the pre-fall heading.
Native fallImpulse, physics and get-up animation remain the owners of movement.

PASS final exact-DLL native entry/recovery8/8 (instant selected simulation,
completed wakeup, no delayed yaw drift and subsequent turning), momentum8/8,
camera10/10,blocking dynamic-platform contact24/24,installed models30/30 and
remote sleep/sit/ragdoll transitions36/36. Impaired LAN3/3 per direction passes
with250ms lag,40ms jitter and5percent send loss; production construction hook
fires on all six entries without a manual watcher setup in the peer driver.

PASS focused native/skin/retry/stream, shared-screen C++ and personal-sleep
regressions; four native correction-removal controls, stream old-behavior controls,
ABI drill and18 reliable-routing negative controls. Both new camera dependencies
match their live-captured references; the fact-row gate retains16 baseline
violations and gains none. No new complete65-group pass.
The legacy external toolchain/private laptop corpus is unavailable here.
Production/build inputs frozen:2645; tested DLLs identical.
Evidence: build/ragdoll-transition-check/verification.json and final logs.

Full32-member ElvardFIC-WatchersOfTheVoid-0.9.2157.zip passes original branding,
models, loader layout, byte comparisons and actual isolated unpacked startup.
DLL SHA256:7bf4a68037a384605ae54fd9c8939564ba531f274d9b94fb6fc5f99617f647d2
ZIP SHA256:cf0c122bb787876eb62e13a01f4896bc94468622460f4c559e4a89405f30955a
Package proof: build/package-current/verification.json; archiveStartupVerified=true.

The first impulse observer missed the first spawn's watcher registration; fix the
acceptance setup before the native call. An artificial movable platform also
retained StaticMeshActor's WorldStatic channel, triggering native camera wall
travel on recovery. The final fixture explicitly verifies WorldDynamic and keeps
all physical responses blocking. No native wall-detection suppression was added.
The surface driver now uses ordinary C-key recovery through completed native
animation, rather than ForceWakeup, whose immediate reset could snap a sliding
head across scenery. A world change aborts the matrix instead of posting stale
pawn tasks. Failed intermediate runs remain in build/ragdoll-transition-check/; they are not
release evidence. Static terrain beyond the tested native ground remains open.

Native test logs also expose the existing 768-slot script-name watch capacity
being exhausted during world startup. Keep that registration-capacity issue open;
this change does not enlarge the registry or claim the other refused lanes fixed.

Entity and the normal installed profile are untouched. Owner rendered/input
acceptance, other terrain/vehicles, native bed/chair and uninstalled rigs remain
open. Branch updates retain AquaFishhh authorship and package0.9.2157.

## Historical release 0.9.2157 / b2183 — 22 October 2026

This status supersedes older package/build statements below. Existing open gameplay
and implementation tasks remain open unless explicitly accepted.

Integrated main 2e104004b with the selected-character native ragdoll changes.
Preserved upstream computer leases/desktop edits, balance retries, event cues,
container births and lifetime guards. DLL/protocol2183 includes both wire changes.
Public package version is 0.9.2157 at the owner's request; 2158/2159 below are
historical local iterations, not the current delivery.

PASS Release and focused native/skin/retry/stream, shared-screen C++/sleep,
events, balance, placement, trowel, water/material and personal-sleep groups;
ABI and reliable routing with all18 negative controls. The legacy full runner
cannot complete with the absent external toolchain/private laptop corpus.
No new complete65-group result is claimed.

PASS actual NullRHI game momentum8/8,camera10/10,surfaces24/24,catalog30/30,
remote sleep/sit/ragdoll transitions36/36 and impaired LAN3/3 per direction
(250ms outbound lag,40ms jitter,5percent send loss on both peers).
Frozen production/build inputs:2644; test DLLs identical.
Evidence: build/release-2157-check/verification.json and final logs.

Full32-member ElvardFIC-WatchersOfTheVoid-0.9.2157.zip passes package identity,
original icon/models/authors/loader checks and actual unpacked startup.
DLL SHA256:c4237cc74aed8932873bf1a6f6c7bb35d47a8e4566b1278135641ef9f2c78f9a
ZIP SHA256:dde82f9435b31896cef5fc43b4b385dbca471b62194781b4e115aad1c5c5d9b7
Package proof: build/package-current/verification.json; archiveStartupVerified=true.

The first integration candidate included a superseded pre-mesh collision/all-body
shutdown. Offline setup checks passed, but actual momentum fell to6/8 and camera
entries rose over50m. Keep that rejected run in integration-first-failed.log.
The final build retains the previously qualified staging order: pause component
simulation, place/rebuild the selected mesh, then stop its new per-body simulation
until warmup finishes. Both the ordering regression and all native matrices pass.

Normal installation/profile and Entity remain unchanged by this release work.
Owner handles rendered/input acceptance, especially local bed/chair interaction;
uninstalled rigs, other terrain/vehicles and more extreme outages remain open.
Both requested branches are intended to carry this exact release tree, preserving
prior main/fish history and AquaFishhh commit authorship.

## Historical work and qualifications

Current development source/wire protocol: **2182**, game **0.9.0n**. Last full qualification recorded:
**b2178**, **65/65 offline validation groups**; evidence:
`build/continuation-check/verification.json`.
Gameplay acceptance requires identical current DLLs on all peers.
Last prepared complete manager package **0.9.2159 / DLL b2182**; package evidence:
`build/package-current/verification.json`.
Development b2178 qualification passed in `build/continuation-check/`: 65/65 groups,
unchanged full production snapshot, Release identity/ABI and 781 source entries.
That full-suite result is historical. The last prepared package carries the separately
qualified b2182 ragdoll robustness update; it does not inherit the 65-group claim.
Use identical current DLLs for multiplayer acceptance.

Development **b2179** adds held water/material persistence before reconstruction.
It also corrects C-key ragdoll reconfiguration and warm-pose anchoring.
Shared-screen capture/apply now excludes the native PC boot/power UI.
Full 65/65 offline qualification and stable Release build pass for production
snapshot `828154d98e2208cf82869997293f059d422511e62a8a5b2f801b5d4592ce3dcc`;
781 build entries, Release identity/ABI check, and no production changes during
the run. DLL SHA256 `5a92cbeda18433d3a3dde41494ddf75a2e8f0979c7698136f1fe97af53271332`.
Package 2155 still contains qualified b2178; it does not contain this continuation.
The full public-prose gate remains 13 counters above baseline; the fact-row gate
reports 45 findings across 525 rows in 157 files. Resolve these separately from
the passing offline qualification. Multiplayer acceptance remains open.

This file retains unresolved implementation, investigation and gameplay work.
Priorities: [TODO.md](TODO.md). Source reports and verification history:
[TESTLOG.md](TESTLOG.md). Exact native entity mapping and per-class gaps:
[ENTITY_COVERAGE.md](ENTITY_COVERAGE.md).
Offline checks do not close gameplay reports.

## Ragdoll robustness b2182 — 22 October 2026, completed

Owner will check normal input/rendering and authorizes incomplete limb transfer,
slopes/moving surfaces/latency/installed models and sleep/chair transitions. No
extra models are installed; owner explicitly asks to use the available catalog.
Entity remains outside this task.

Reproduced two stream defects: wall-clock bone selection can repeatedly send
one partition at a compatible cadence; latest-whole-packet storage drops other
limbs when multiple packets arrive before a game tick. A bounded full-rig outbox
now rotates seven bodies per actual send round, shifts cycle phase against
periodic loss, and refreshes values without resetting the turn. The receive inbox
merges newest per bone, with sequence/generation guards and64-body bound; the
game thread drains at most10 wire-sized chunks. Wire packet size remains230bytes.
Tests and both former-behavior controls: agent/tests/run_ragdoll_stream.ps1.

Candidate LAN PASS3 rounds per direction at250ms outbound lag,40ms jitter and5%
send loss, enabled after world-ready. Candidate surfaces PASS24/24 (six skins x
flat,25/35-degree slopes,moving platform), selected physical body/contact/recovery;
remote sleep/sit/ragdoll precedence and teardown PASS6/6. Final DLL also verifies
the actual requested mesh, seated animation flag and platform displacement.

The initial fixture triggered the game's native wall-crossing detector and
travelled to sl_backroomsObsolete. Raw test relocation left lastLoc behind, so a
ray from that history to the camera crossed the platform. Native teleportWObackrooms
updates the history; use it for fixture moves. The collider also needed the proper
Block/Movable enum values. No wall detector or gameplay backrooms rule is disabled.
Intermediate surface logs are preserved and are not passing evidence.

Final DLL b35471ad15d0f417b26439f51c9b64e2f034987de7d2e51e3ba031ea54cbb5a1
and2644-entry frozen source PASS: momentum8/8,camera10/10,surfaces24/24,catalog30/30,
remote state transitions36/36,bad-link LAN6/6. Actual requested assets and seated
animation flag checked; moving fixture displacement175cm verified on all six skins.
Catalog uses native ground; fixture destruction is allowed to finish before return
teleport. Final run has no test travel/posted-task faults. Evidence:
build/ragdoll-robustness-check/verification.json and *-final.log.

Complete package0.9.2159 passes32-entry identity/icon/models/qualified-DLL checks
and actual unpacked startup: HEALTH PASS,skins4/4,enabled fork loader.
archiveStartupVerified=true. ZIP SHA256:
bb85eaff437b031ed101c88702d8b5c8ea676bb258d77e9a45b845aaea49e42b.
All private tests stopped; normal profile and Entity unchanged. Owner handles
rendered/input acceptance,including actual local bed/chair interactions. Arbitrary
uninstalled rigs,other terrain/vehicles and more extreme links remain unqualified.

## Player ragdoll native momentum b2181 — 22 October 2026

COMPLETED: final diagnostic DLL below passes momentum8/8, camera10/10 and selected
articulated LAN entry/recovery in both directions. Frozen2642-entry source and all
three runtime DLLs match. Complete 0.9.2158 archive passes32-member identity/layout,
icon/models/credits verification and actual unpacked startup (HEALTH PASS, skins4/4,
namespace/enabled loader). archiveStartupVerified=true. All private test instances
stopped. No public release/commit/push or normal profile change. The intermediate
partial LAN run described below remains an unexplained intermittent limitation.

Owner reports lost running inertia and asks whether selected models can retain
the game's original ragdoll system. The original solver/lifecycle is already used;
native playerRagdoll_C uses five-body kel_lmao_Physics, whose thighs/lowlegs differ
from the selected full model's independent limbs. Alternatives and the native
asset/Blueprint evidence are in [RAGDOLL_DESIGN.md](RAGDOLL_DESIGN.md).

Reproduced the defect with the actual engine substrate regression. Native
fallImpulse waits Delay(0), then applies fallVeloc to every body. Mesh/physics
rebuilding discarded the command; the old pelvis-only restore read zero before
the latent impulse and did not initialize new limb velocities.

Implemented a transition-scoped native velocity hold: retain stock per-bone motion,
queue the actual native linear-velocity calls while bodies are replaced, then
restore once after simulation starts. If the impulse ran early, initialize all
new limbs from the stock pelvis translation before restoring per-bone differences.
If it runs late, the original function runs normally. Keep the stock chest kick;
do not calculate another launch impulse. Unarm the seam after completion/cancel.
Object serial/parent lifecycle and existing camera/anti-launch safeguards remain.
NPC/shared defaults and Entity implementation are unchanged.

PASS offline attachment/entry/camera/momentum including early/during/late timing,
new limbs, repeated refresh, recovery, recycled body and disabled-hold control.
PASS candidate native 8/8 controlled movement cases on scientist and Kerfur:
walk 300, run 600, diagonal and airborne. First matrix recovery failures were
test setup: forceGetUp is delayed; forceWakeup plus recovery checks isolate cases.

Native b2181 DLL 6116e6755237f01f9d227caa9335dfe6167780e756efd1f01d8c8a7868ebd293
passed the eight controlled movement cases and ten camera cases. The optional
held-forward/run input probe generated zero movement under NullRHI; its cause is
unresolved and it is not an input-path pass. LAN on that DLL propagated entry and
recovery in both directions, but articulated-display verification was PARTIAL
(model=1, streamed=0). Do not claim final LAN qualification. Logs: momentum-final.log,
camera-readiness.log and partial-{host,client}.log under build/ragdoll-momentum-check/.

Final diagnostic b2181 DLL 3d77b13017ed02d4353ec19a0b7de47035663ab7ca041ea9ed8924c67131d3e2
adds bounded existing-emission rig/bone counts and a remote rig-change diagnostic.
Its own final momentum/camera/LAN logs pass. The LAN readiness harness now
continues through an 8-second game-thread timeout until its overall deadline,
with a separate result object for each task. Production networking is unchanged.

Remaining: rendered running/input acceptance and investigate intermittent incomplete
LAN display if reproduced. release.json points to the new qualification directory;
packaging will refuse until its verification.json is present and matches the DLL.
Do not offer an unqualified DLL-only archive. Evidence/scripts live under
build/ragdoll-momentum-check/, with current toolchain/runtime helpers under build/.

## Player ragdoll camera b2180 — 22 October 2026

The owner confirms the launch is fixed and reports an upward camera turn on
entry. Reproduced it in the native game: a level view became 89.75 degrees
upward immediately after ConfigurePlayerRagdollMesh. Native ragdollMode attaches
lag to head with a plush-specific (0,-90,0) relative rotation.

Preserve the controller's entry look and rebase the camera arm after mesh swap,
during the one-pose warmup and after physics initialization. Stop all correction
after simulation starts so the camera follows the head normally. Verify the arm
still belongs to the original live player/body before every write; recovery can
reparent it before ragdollActor clears. Physics geometry/constraints are unchanged.

Focused native/physics/lifecycle/display/wire checks pass, including a removed
camera-correction control. Ten isolated native cases cover two models and five
entry conditions; mesh-replacement view differences are below 0.1 degrees.
The angled NullRHI tests arrange controller and arm transforms together because
synthetic input pitch is reset between ticks. Final b2180 LAN and complete-package
checks are recorded in TESTLOG.md and build/ragdoll-camera-check/verification.json.
The normal profile and Entity implementation are unchanged.

## Player ragdoll hotfix b2179 — 21 October 2026

The owner's pre-release report takes priority over the general backlog. Entity
implementation is unchanged. Rebuilt the Release DLL from main and reproduced
the defects in isolated copies of the owner's actual 0.9.0n game.

Implemented:

- Treat Actor::K2_AttachToComponent as void and verify its actual parent. The
  old nonexistent ReturnValue read destroyed every successfully attached remote
  body, leaving the standing puppet until recovery.
- Preserve the selected model's world geometry. Never lower full-size legs to
  the plushie's pelvis. Set the component transform before rebuilding physics,
  evaluate a stable reference pose, then rebuild bodies and start simulation.
  Do not install a player-only AnimBP on the ragdoll Actor.
- Repair separated joint anchors when instantiating the common full Kerfur rig.
  Native asset audit and live measurements found a 1069.2 cm chest/pelvis anchor
  gap. Corrections exist only while SetPhysicsAsset copies its constraint
  templates; all shared defaults are restored on success and every failure.
  NPC physics assets and foreign authored rigs are not changed.
- Keep selected mesh/materials, the 17-body common rig, native ownership,
  camera/recovery and complete remote bone batches. No plushie substitute.
- Match WebRTC's C++ standard to Abseil for a clean MSVC Release link. Use a
  steady clock for the loading curtain so its game-thread callback does not
  dereference an absent/render-thread ImGui context.

Focused native, appearance/physics, remote lifecycle, display and wire checks
pass. Live local Kerfur entry changed from thousands of cm/s of launch velocity
to a normal fall: maximum pelvis rise 22.5 cm, then rest. Evidence and the final
network acceptance status belong in TESTLOG.md and build/ragdoll-evidence/.
Actual LAN acceptance passes in both directions: scientist client to host and
Kerfur host to client, with selected mesh, complete articulated display and
entry/recovery confirmed. Broader conditions remain in the Bodies checklist.
The old 65/65 qualification describes b2178 only; b2179 is a separate focused
hotfix and has not inherited that full-suite claim. Both peers require b2179.

Delivery correction, 22 October: the earlier manual DLL ZIP was incorrectly
offered where a full manager package was needed. The actual imported profile
confirmed inferred ragdoll/2179.0.0 identity and missing loader activation.
Complete ElvardFIC-WatchersOfTheVoid-0.9.2156.zip restores the fork's original
identity/icon, starter models and enabled loader layout. The unchanged qualified
DLL starts from that archive in an isolated manager layout with dev mode off.
Package metadata and verification now live in package/release.json and
package/package_current.ps1; the normal Default profile was not modified.

## Imported main history (superseded by current release status)

## Priority report: C-key ragdoll launch (5/50, 21 October 2026)

The owner reports that pressing C launches the player on every role and breaks
ragdoll. The model and runtime reproduction are not yet recorded. Development
b2179 disables collision and all skeletal bodies before mesh/physics replacement,
then aligns the warmed animation pelvis to the selected model's standing/crouched
entry anchor immediately before simulation starts. The native shell has another
rig and must not determine the selected body's pelvis height; that can put its
legs into the floor. Failed setup retries retain the selected-model anchor;
changed pawn/body/model identities and non-finite poses refuse the start.
The previous mutation order, premature pelvis alignment and different-rig entry
height fail the new production regressions; the corrected paths pass. These
tests do not run PhysX or establish that the reported launch is resolved in game.

Accept standing/crouched/repeated C entry and recovery on host and client with
matching b2179 DLLs, selected/common/custom rigs, floors, slopes and moving props.
Record the model, role, reproduction and logs in TESTLOG before closing 5/50.

## Priority report: stationary PC infinite loading (54/55, 21 October 2026)

The owner confirms that the stationary base computer remains loading on host and
client. The existing native laptop corpus assigns `rootswitch` page 0 to boot and
page 1 to the desktop, with a latent completion that clears `anim` and broadcasts
`pcLaunched`. The generic shared-screen mirror captured this outer switcher and
reapplied old boot snapshots every poll, restoring page 0 after native startup.

Development b2179 captures/applies only the desktop child. Boot widgets, the
outer switcher and its ancestors are outside the snapshot. A production-wrapper
regression fails on the previous code and passes with the correction; desktop
text/scroll/tab retry and native power-page ownership are covered. The existing
native corpus confirms the page contract; no fresh extraction was performed.
Accept actual startup on both roles, entering/leaving during boot, repeated
ON/OFF, power loss/restoration, late join and reconnect before closing 54/55.

## Backlog status - 21 October 2026

TODO separates the next implementation, investigation and gameplay actions.
This file keeps their details and every report ID still requiring work or
acceptance. IDs 1–58 are accounted for below except closed Mining (6) and
owner-excluded Wall41; 11/36 are duplicate reports of the same chair behavior.
Implemented code awaiting gameplay is still open for acceptance, rather than
an instruction to reimplement it. The b2178 Release/package checks above passed;
real-peer acceptance and the full CI findings below remain unresolved.
The fresh native asset audit remains deferred by the owner.

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

На момент інтеграції в main: протокол **2176** (поточний — **2179**), збережено `KavotiaState=223` і
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

Package 2155 improves the shared infrastructure without closing any complete
category: replay owns queued work across native callbacks, limits recursive work,
retries preparation failures and consumes ambiguous attempted native calls. Active
join snapshots retain readiness/send debt, copy native membership, validate sender
serials and cancel on session/world/recipient replacement. Offline lifetime and
reentrant tests pass; native effects and phase parity remain category-specific work.

Development b2178 additionally binds location-event state to the session-start
serial, including reuse of the same Session pointer/world. It validates actor
identity between native location/rotation reads and cancels gathering after native
callbacks tear down or replace the world/session. The location tick refuses
recursive entry. Production tests first failed on the old session-reuse behavior,
then passed with native pose/query teardown and real-overlap cancellation cases.

The eight-category request remains open. The owner deferred the fresh
game-pak/Blueprint audit on 21 October; the old D: installation is unavailable.
Keep that audit deferred. Do not infer native mapping from a wiki name or close
an event from its actor's transform route.

| Requested category | Remaining implementation or acceptance |
|---|---|
| Dreams | The native local sleep roll, wakeup/createDream entry, dream phase, and leaveDream/awoken return remain peer-local; SleepState carries rest/dream presentation only and DreamScene is ignored. Verify every named scene's actor effects, wake rewards and join/lifetime behavior on peers. Preserve independent nightmare settings and Insomniac conditions. |
| Ariral Reputation | Audit every reputation tier/case, shared reputation, randomized effects, bed/player relocation and late join. |
| Player-Activated | Map and verify every listed interaction, simultaneous activation and canonical side effects. |
| Time-Related | Audit real-calendar/holiday and 3AM branches, local calendar rolls and uncovered effects. |
| Endings | Establish and verify each ending's per-player versus session consequence; do not replay quit/travel on remote puppets. |
| Story Mode | Complete actor/phase/audio/bookkeeping adapters and current-state late join; start-of-event replay does not prove an active-phase snapshot. |
| Random | Audit the complete named list and remaining timer/probability sites. Black-fog client suppression alone does not mirror black fog. |
| Signals | Audit each processing/playback trigger, funny-rule requirement, chosen outcome, phases and late join. |

Frequency coverage now includes the five verified
`ticker_fossilhoundSpawner`, `ticker_yellowWispSpawner`, `ticker_wispSpawner`
and `ticker_egSpawner`, plus owner-local `ticker_eyers` routes.
The existing night-eyes Blueprint confirms a native 1800–2700s re-arm, 5% roll
and day/night gates. Only the wait is scaled; the existing owner-entity route
remains responsible for its creature. Host owns the first four generators.
The verified peer-symmetric `ticker_fireflySpawner` route is also frequency-scaled
on every peer; its local-camera eligibility/roll and emitter capture/replay remain
native. The shared setting changes only its tick wait.
`treeSpawner` (600–900s walking-tree output), `bp7Spawner` (camera-dependent rare
integer roll) and `flickerer` (power/generator effects) were inspected in the
existing corpus but not admitted without complete authority/effect routes.
Other repeatable random-event candidates remain to be inventoried. This is not a
fresh pak audit.
Preserve native eligibility, probabilities,
personal dream rolls and story dates; adjust eligible-roll intervals without
changing AI/animation, effect durations or sleep needs.

Real-peer acceptance must cover chosen/repeated events, simultaneous triggers,
live/persisted frequency changes, load windows, late join, reconnect and teardown.
