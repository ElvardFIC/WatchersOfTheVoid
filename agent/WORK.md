# Unified Work - unresolved backlog

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
  Package 2155 persists exact-key/class local held concrete updates into the
  existing `hold[0]` integer scalar immediately after native `updConcrete`.
  It validates actor/save/world/slot identity, original slot bytes and live stock
  across native callbacks, never allocates/replaces a record and includes zero.
  Malformed schemas and identity changes refuse the write; no key repair or paid
  transaction is enabled. Production helper and reentrant boundary tests pass.
- Verify imported creative construction and wet-wall timer persistence, exact
  impact/effects and client damage to completed walls.
- Arbitrate concurrent water/cement/coal writers and stale in-flight records;
  settle dryTimer ownership and personal-hand transitions.
- Verify imported sponge soapAmount/foam, grill lid/fuel/burn/audio and
  cooking-progress carriers on real peers. Complete remaining final cooked-food
  output and personal resource transactions.

### Personal resources, grime and scorch

Implemented in development b2179: exact-identity held stock now writes back to
the existing `saveSlot.hold[0]` numeric groups after native water use/tick,
material changes and cement transfer endpoints. This covers water level/soap/
alcohol, concrete units/dry/dryTimer, cement-bag units, sponge wetness/soap and
trowel concrete. Native `updateHold` can read the updated values when it rebuilds
the hand. The commit validates all numeric group shapes/current values before
writing any of them; it allocates no engine arrays and preserves unrelated data.
Hand/world/save/slot/stock changes refuse the write. Legacy incomplete schemas
and mismatched keys remain refused; no identity repair is enabled. Focused
production-wrapper, scalar-memory and controller regressions pass. Two-peer
reconstruction/unequip/drop acceptance and the transactions below remain open.

- Complete personal resource debit, output acceptance and refund as an atomic
  transaction, including rejection, retries, disconnect and hand/slot/drop
  transitions. Imported profile/stock persistence does not close this work.
- Cover remaining dynamic grime sources with canonical births, cleaning identity
  and retirement. Verify imported quiet joins and final-state delivery on peers.
- Preserve explosion/salt-rocket scorch provenance through a full save/world
  reload. Known provenance currently survives reconnect within the same world;
  that does not establish migration across world reconstruction.

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
an old rifle and components in an isolated case. The latest owner clarification
confirms **the host's old rifle**; exact components remain unknown. Native rifle
candidate: `prop_rifle_C`. Existing leaf bytecode contains `playerHandUse_LMB`
spawning an explosion and applying impact damage; no placement entry point is
present in that leaf. This does not exclude inherited behavior or establish cause.
Host placement follows Finish/Init and deferred host adoption; client intermediate
suppression and join reconciliation are client paths. The receiver destroy path
can also remove a host actor on a peer's keyed/eid destroy; capture both logs.
Enable `VOTVCOOP_PROP_BIRTH_KEY_PROBE=1` before launching a reproduction to record
birth class/key, destroying class and session state. Retain timing, `[PROP-DROP]`,
`remote_prop::OnDestroy` and `join_membership_sweep` logs.
Package 2155 separately retains admitted client placements after exhausted transform,
food-readiness or send retries. Their identity-stamped local recovery survives sweep
and blocks fallback shared admission, allowing pickup; a full recovery pool retains
the pending item. Offline checks pass, including a rifle-class fixture, but this
does not reproduce or resolve the host's disappearance.

Development b2178 fixes a separate receiver defect reproduced in the production
`remote_prop_destroy.cpp` harness: a listener/native release could recycle the
selected actor's slot and the receiver still issued K2_DestroyActor to its replacement.
One scoped object/world/session-start identity now covers listener delivery, drive
cleanup, grab release and unpin. Recursive destruction of the same incarnation is
suppressed, listener registration uses an owned snapshot, and local-consume/convert
cleanup shares the guard. Cancellation consumes the old request rather than
retrying its key against a replacement. Existing key/eid fallback policy is retained.
The incoming-destroy mark is written only after the final cancellation check, so
an aborted attempt cannot hide the actor's next genuine destroy.
New `[PROP-DROP] destroy-received` logs include role/sender/class/key/eid/world.
Normal destruction, deferred load-tail behavior and listener/spend delivery pass
the focused production harness. This does not identify the owner's runtime cause.

Further existing-corpus inspection: `prop.getData/loadData` stores/restores native
lifespan, but the network save codec replaces incoming base fields with the local
base record; no lifespan clearing was introduced. `npc_arirGunStealer` resolves
the literal key `arirGun` and later destroys that object, not any rifle by class.
`walkingTree` has viewer-dependent visibility/movement/despawn and native latent
timers, so simple host pose/rate admission would not complete its route. Decoded
evidence is in `build/placement-check/*-native.txt`; no fresh pak audit was run.

Token52 was clarified on 22 October as probably the general balance. The shared
`saveSlot.Points` lane had three concrete delivery defects: host send refusal was
recorded as success, a failed join/refused-order correction was discarded, and
client HUD callbacks could consume a newer pending total. b2180 fixes these with
per-recipient accepted-send state, retained correction requests, atomic pending
value acknowledgement and repaint retry. It also rejects work from replaced
sessions/occupants and cancels queued development credits after teardown.
Production tests cover zero/negative balances, late readiness, partial fan-out,
same-slot reuse, reentrant updates and world changes. The original UI/report still
needs runtime confirmation; no client-authored balance delta has been added.

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
| 5 | Selected-model ragdoll | Prioritize all-role C-key launch acceptance of b2179 setup/anchor correction; selected-model pose, entry/recovery and local/remote limbs; linked to 38/50. |
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
| 17 | Pressure-washer stream | Implemented visual/audio route needs gameplay acceptance; verify host impacts with the water routes. |
| 18 | Computer sparks/damage presentation | Verify native water shortCircuit presentation on both peers; separate from ordinary ON/OFF (55). |
| 19 | Computer shutdown from client damage | Verify the native water shortCircuit route; ordinary addDamage is empty in this cooked class. |
| 20 | Bucket water disappearing | b2179 adds held water/soap/alcohol persistence before reconstruction. Concurrent writers/in-flight-record arbitration, pouring and personal-hand gameplay remain. |
| 21 | Cement / trowel | Atomic repair/material transactions, trowel key/stock continuity and dryTimer handoff. |
| 22 | Brick-wall outline/build | Client repair, creative construction, wet-wall persistence, exact effects and multiplayer damage/build acceptance. |
| 23 | Depleted flashlight still lit remotely | Verify exhaustion/crank/low-battery transitions and remote light state on the current package. |
| 24 | Client cannot pick up anchored hook | Verify host anchor retirement, one ordinary pickup near the requester, saved anchors, retries and late join; pickup is not an automatic inventory grant. |
| 25 | Battery levels | Hand/stow/drop/pickup/rejoin, including zero charge and delayed record delivery. |
| 26 | Airy event | Exact behavior, effects and multiplayer acceptance. |
| 27 | Main-room computer panels become physics props | Struck-panel disappearance and final motion/state on host/client. |
| 28 | Drone sound difference | Verify the implemented audio correction on real peers. |
| 29 | Crowbar static state / dragging | Atypical parent/replacement subclasses, live ownership, refusal/retry and actual dragging gameplay. |
| 30 | Second ATV in corridor | Verify new companion creation separately from existing saved second-ATV positions; old positions are unchanged. |
| 31 | Sleeping body pose | Selected rigs, mattress/exhaustion anchors, moving beds, precedence and sleeping footstep/hand/head-effect suppression. |
| 32 | Not enough space on equipment | Verify the implemented handling for all equippable items, including full inventory and equip/unequip transitions. |
| 33 | Open container retains removed row | Verify simultaneous access/removal and row refresh on the other peer without stale capacity errors. |
| 34 | Invisible client crate/package loot | Remaining package families, exact local loot provenance and two-peer crate/MRE/AB/foodBox output. |
| 35 | Centipede / other creatures invisible | Per-class behavior, visibility and live acceptance in ENTITY_COVERAGE; linked to 48. |
| 36 | Computer chair rotation | Duplicate of 11; keep both report IDs linked. |
| 37 | Riding a carried ATV / flight | Verify simultaneous carry/mount, menu state and race convergence on both peers. |
| 38 | Skin model list / previews | Remaining preview gaps, unknown missing model names and common/custom appearance/animation/ragdoll acceptance. |
| 39 | Consumables refill on pickup/drop | Partial/empty portions, hand/stow/drop/pickup/rejoin, final bite and eater-local hunger/poison. |
| 40 | Flesh Erie summon / visibility / aggro | Distinct angry-flesh-Erie spawn/target/action/damage route and gameplay. |
| 42 | Event teleport to triggering/base-entering player | Initiator/occupancy, repeat/reentry, multiplayer teleport and other custom effects. |
| 43 | Safe cracking | Dial/hacked completion, one loot result, persistent lock, retry/stale records and save/rejoin. |
| 44 | Sponge wetting | b2179 adds held wetness/soap persistence before reconstruction. Soap stock/foam/surfaces, personal-hand transitions and real wetting/final-use gameplay remain. |
| 45 | Grill ignition | Concurrent coal/stale-record arbitration, lid, final cooked food and personal-item transitions. |
| 46 | Garden actors disappear after save reload | Pre-session save/reload, care/harvest/pot state, accepted hose rerouting, late join and simultaneous use. Missing historical save actors are not migrated. |
| 47 | Black disk/yellow-ring sky event | Audio, delayed email/bookkeeping, pre-session latent chains and shared schedule/effects/join acceptance. |
| 48 | Mimic | Shared rendering/disguise/material, damage/gibs and host lifetime in multiplayer; related to 35. |
| 49 | Client insomnia demon | Personal spawn/visibility/pursuit/targeting/grab/dream, low-sleep versus rested peers, recovery/despawn, reconnect and rare Child variant. |
| 50 | Ragdoll display still incorrect | All-role C-key launch remains a gameplay report after b2179 setup/anchor correction. T-pose, local body, remote limbs, common/custom rigs, retry/fallback and drift; linked to 5/38. |
| 51 | Client appliance plug/unplug | Near/far endpoints, short/long cables, extensions, occupancy, unpowered outlets and callback/load timing. Nearby failures may have another cause. |
| 52 | Client token display | Identify the actual counter/UI and reproduce; do not assume credits. |
| 53 | Base light flicker | Actual keys/overridden setters, component readiness, loading/late join and visible timing. Separate reliable messages do not ensure frame-exact alignment. |
| 54 | Shared computer display/activity | b2179 excludes boot/power UI from shared-screen repaint. Accept stationary-PC startup and laptop/portable-PC tabs, text, scroll, visibility, cursor/input/activity and operator body. |
| 55 | Computer ON/OFF | Prioritize the host/client base-PC infinite-loading report after b2179 boot-page isolation. Animation, power loss/restoration, failed delivery, late join and reconnect remain; separate from water short-circuit/damage. |
| 56 | Ejected ZIP becomes a huge owo | Actual inserted/ejected class, model and scale on both peers. The bool-wrapper defect is not a proven cause of the symptom. |
| 57 | Opening/eating cans | World/held open/eat/final portion, stow/re-equip, drop/pickup, simultaneous consumption and reconnect. |
| 58 | Items disappear on workbench | Host's old rifle confirmed; reproduce simple placement with birth/destroy caller, class/key and peer logs. Components unidentified; client recovery is separate. |

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
| ATV (30/37) | New companion creation versus existing saved second-ATV positions, simultaneous carry/mount, stale menus and race convergence. |
| Disks/racks | Client sv.request/hashcodes, exact floppy/ZIP text including line breaks, filled-rack save/rejoin and a slow joining peer. |
| Whole package | Swap host/client roles; two clients contest objects; late join/rejoin/disconnect/save/reload; partial/empty consumables, full inventory, equipped items/container rows, loose cables, flashlight crank/low battery and previews. |

Sleeping-body native evidence remains in the workspace `_research/`:
`sleep_2166.txt`, `wakeup_2166.txt`, `sleep_graph_2166.txt`,
`sleep_full_2166.txt` and `objects__misc__playerSleepingPawn_2164.txt`.

## Validation follow-ups

- Resolve the recorded full public-prose/fact-row CI findings, including missing
  live native-hook references, then record a fresh full-gate result in TESTLOG.
  HANDOFF_FISH records 13 baseline prose counters and fact-row findings across
  524 rows/156 files at its original handoff; those are historical measurements,
  not a new b2178 CI result. The later package entry also retains known failures.
  Passing selected-file prose and include-context checks does not close full CI.
- Complete the native-contract audit once the owner resumes fresh asset work.
  The available corpus is incomplete for the imported native-contract suite;
  keep fresh game-pak/Blueprint extraction deferred until then.

## Release preparation

Package 2155 build/pack/verify scripts require b2178, all 65 current groups and
unchanged production inputs. Packaging verifies archive documents/DLL, manifest,
icon, paks and outer delivery hashes. Requalify after source edits; archived 2154
scripts describe b2156. Complete package history is in CHANGELOG_PLUS.md.
Commit, push, game installation and publication require an explicit request.
No other-agent workflow. Update active documents in place; do not create
numbered trackers, backup source trees or historical test-output copies.
