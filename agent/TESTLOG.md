# Development b2189 - explosion prop motion (23 October 2026)

Managed host blast overlap victims coast after the actual delayed radial impulse.
PASS focused controller/native contract and four expected-failure controls.
PASS 71/71 groups, stable Release/ABI: `build/explosion-motion-qualified/verification.json`.
DLL SHA256: `f8129e89a15552da2925b0f6b6b6865283f0791ef3a3375a0d1de4232f0838ad`.
PASS exact-DLL native LAN: bucket moves 547.57 cm, reliable rest edge, live/rejoin
positions agree within 0.1 cm: `build/explosion-motion-qualified/native.json`.
Both compatibility reports HEALTHY; live impulse reference MATCH; fact rows 512/164.
Native Lua/NullRHI evidence does not establish rendered/input or all-blast acceptance.
Force-only target discovery remains open. Existing corpus reused; broader audit deferred.
Package 2158 stays b2186; no archive, profile installation or publication.

# Development b2188 - ATV steering and join honk (23 October 2026)

ATV handle/wheel steering is replicated; join snapshots retain pending live honks.
PASS 70/70, Release/ABI: `build/atv-steering-qualified/verification.json`.
PASS native LAN 6/6 + nonzero late join: `build/atv-steering-qualified/native.json`.
Two controls, 511/163 fact rows/nine controls and 225 routing kinds/18 controls pass.
Native field stimulus does not close rendered/keyboard acceptance; package stays b2186.

# Development b2187 - player bleeding (23 October 2026)

Owner blood-loss drops are host-validated/forwarded without echo; damage/blur stay local.
PASS 69/69, Release/ABI: `build/bleeding-qualified/verification.json`.
DLL SHA256: `b6cd023baaba1d2807a733ecd8114066c6cb72d2d052c97ecfe87578c1229a56`.
Native contract: [blood capture](tests/blood_capture_0.9.0n.txt), two live functions.
HEALTH PASS/reference MATCH/zero census problems: `build/bleeding-qualified/native.json`.
PASS fact rows 509/161/nine controls and routing 225/18; inherited destroy-watch errors remain.
Rendered bleeding and stain/join/cleaning/save parity stay open; package stays b2186.

# Package 0.9.2158 test build

PASS 68/68 exact-build offline groups, stable Release b2186 / protocol 2186,
loader identity/imports and 786 build source entries. Evidence:
`build/release-2158/verification.json`.
DLL SHA256: `d57443e02bf588ec1f3fe812af1acce0f9e3b93f564bf562645ed5539b55535a`.

PASS production menu/browser formatter with the generated header:
Watchers of the Void 0.9.2158. PASS archive: 32 members, exact source/DLL hashes,
manifest, 256x256 icon, starter models, activation marker and test disclaimers.
ZIP SHA256: `05e67388365d16902f283aef7a9c962b942ca844eb9d5fb2bc590702ab9d6929`.
The exported workspace copy has the same hash. Four negative controls reject
failed/incomplete suites, a stale label and another DLL; the DLL-only routing
fixture is also rejected. Evidence: `build/package-current/verification.json`
and `rejection-controls.json` in the same directory.

Native gameplay and archive startup remain unverified. Packaging explicitly uses
the offline-test qualification level; it does not reuse earlier native results.
Windows PowerShell 5 returned the snapshot as a nested array under the old packer;
reading the parsed array directly fixes iteration and source hash validation.

The GitHub About description and four README/release/changelog documents were
published in commit `565edebe2e86b357b24cb26681102967c38bd36a`. The local branch
was fast-forwarded to it without committing the remaining implementation changes.
No binary release, Thunderstore upload or master-server deployment was performed.

# Selected upstream qualification

PASS 68/68 groups and Release build, protocol 2185, product 0.9.0n b2185,
786 build source entries. Production inputs remained stable throughout the full
run. Evidence: `build/upstream-qualified/verification.json`.
DLL SHA256: `321c13ced4a507f862ef2e343d338684e8cca9abc013fad2605fba796d807c51`.

The targeted upstream fixture and extended entity runner passed before the full
run. They cover actual vital encoding, delayed hound names, prop/trash base loading,
worker isolation, full NPC rotations, deduplication and failed-write/landing retries.
The fact-row gate passed: 508 rows in 160 files.

The public-prose gate remains failing on the same four inherited counters as HEAD:
document count 84 against 82, naming counter 26 against 24, unresolved source
citations 155 against 0,
and src.files_not_swept 84 against 0. Comparing the complete proposed file set
against HEAD adds no rule regressions; the baseline was not relaxed. Diagnostic
counters: `build/upstream-qualified/prose-import-counters.json` and
`prose-head-counters.json` in that same directory.

Two-player rendered gameplay remains unverified. Nothing was packaged or deployed.

# Development b2185 qualification ? 22 October 2026

Owner report: player flashlights, switches, garage/base doors and drone-container
contents fail immediately after joining package 2157. These reports remain open
for rendered host/client acceptance. Implemented changes and boundaries:
[SYNC_2157_FIXES.md](SYNC_2157_FIXES.md).

PASS **67/67 current offline groups**, stable production inputs, Release DLL/source
identity, loader exports and forbidden-import check. The final incremental build
reports no work remaining; 785 production build entries. Full result:
`build/sync-2157-qualified/verification.json`.

The new production fixtures cover 1100 pending/resolved watches and eight reopens;
old-capacity refusal; container exact-recipient seeding through blocked delivery,
class/component/content readiness, occupancy replacement and reentry; all 2?32
admission limits and slot-31 acknowledgements/recipient masks/held-food addresses.
The extended flashlight fixture covers class/observer startup, incomplete setter
retry, connect-send refusal, newer callback state and world replacement.

The first attempt exposed two fixture link omissions after splitting the watch
registry; the shared gate fixture now includes the new production translation
unit. Both affected groups pass in the final run. The runner records CMake's
native stderr and checks its exit status, so dependency deprecation notices do
not terminate Windows PowerShell before compilation. Assertions remain enabled.
An intermediate run was stopped for the targeted multi-recipient seed correction;
it is not final qualification evidence.

Protocol/game: **2185 / 0.9.0n**. DLL: `build/votv-coop-2154/main.dll`.
DLL SHA256: `5b5b5a4a745f11c1e2dc31840f886b7188569a6faa77a94ef81451aaa8109207`.
Production snapshot SHA256: `9bc95f5f4c6c29e80604ccae7f23916b1ef80671f918513e668ecca558932908`.

No rendered game session, 32-player session, Rust build, remote deployment or new
release archive is claimed. The master source accepts 2?32, but an old deployed
four-seat master still needs updating for larger automatic lobbies. Use matching
b2185 DLLs on every peer. Fork/upstream review:
[FORK_AUDIT_20261022.md](FORK_AUDIT_20261022.md).

# TestLog — current status and source reports

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

## Ragdoll robustness b2182 / package0.9.2159 — 22 October 2026

Owner handles rendered/input acceptance; asks to finish partial network limbs and
contact/latency/model/sleep/sit acceptance. No additional imported models installed.

Reproduce old wall-clock selection starving bodies at a75ms send cadence. Reproduce
StreamSlot keeping only the last of three distinct bone packets before one game
tick. The new actual-send rotation and bounded per-bone receive merge pass cadence,
loss, burst, duplicate/reorder, sequence wrap, generation and reset controls for
1,5,17,35 and64 bodies. Both original-behavior controls fail as expected.
Evidence: stream-final.log and ragdoll-stream-check/legacy-{time,inbox}.log.

PASS focused native/skin/retry/stream, sleep/display, shared UI/wire/sync and ABI
checks; reliable routing224 kinds with18 negative controls. Full65 suite not rerun.
Candidate native LAN PASS6/6 (3 per direction), with actual GNS250ms outbound lag,
40ms jitter and5% send loss, applied after world-ready. Candidate native surfaces
PASS24/24 across sci_v1sc,walter_v1sc,rvi_scientist_v1sc,luther_v1sc,KerfurH/M:
flat,25deg,35deg and moving cube. Contact is a real colliding engine mesh; selected
physical body, finite motion,no excessive rise and native recovery checked.
Remote sleep->sit->ragdoll->sleep->stand passes6/6 on actual engine components.

Invalid initial fixture moved the pawn across a newly-created platform using a
raw snap, leaving native lastLoc behind. The game's wall ray legitimately called
OpenLevel(sl_backroomsObsolete), from mainGamemode graph7160/7183, not player death.
Native teleportWObackrooms updates lastLoc; fixture uses it, plus correct Block2 /
Movable2 collider settings. No gameplay wall/backrooms rules changed. Failed logs
surface-{initial,registered,no-mirror,travel,backrooms}.log retained. Cleanup faults
from those invalid old-world fixtures do not count as passes. Final logs must be
free of test travel/faults. Blueprint audit: native-gamemode-graph.txt and existing
native-playergraph.txt67430..68046, teleportWObackrooms934..1075.

PASS exact final DLL b35471ad15d0f417b26439f51c9b64e2f034987de7d2e51e3ba031ea54cbb5a1:
momentum8/8,camera10/10,surfaces24/24,installed catalog30/30,remote transitions36/36,
bad-link LAN6/6. Frozen2644-entry source and three identical DLL copies confirmed.
Requested mesh asset and seated animation flag checked; platform100cm/s with175cm
verified movement on all six skins. Camera max entry view jump0.069deg.
Catalog runs on native ground,with contact/physical body/selected asset/recovery.
The earlier catalog interrupted by artificial-fixture travel is retained as
catalog-fixture-interrupted.log; final run has no travel/posted-task faults.

PASS complete2159 archive32 members,original identity/icon/authors/models,and actual
unpacked startup: correct b2182 boot,HEALTH PASS,skins4/4,enabled fork namespace.
archiveStartupVerified=true. ZIP SHA256
bb85eaff437b031ed101c88702d8b5c8ea676bb258d77e9a45b845aaea49e42b.
Evidence build/ragdoll-robustness-check/{verification.json,*-final.log,source-before.json,
catalog-names.json},build/package-current/{verification.json,startup.log,UE4SS-startup.log}.
All private tests stopped; no normal installation/profile change or publication.
Owner owns ordinary rendered/input acceptance,including actual local bed/chair
interaction. Remaining scope limits are listed in the qualification JSON.

## Native entry momentum b2181 — development, 22 October 2026

Reproduced lost running motion in the actual engine substrate fixture before the
fix. Native fallImpulse resumes after Delay(0), adding saved fallVeloc to every
bone. Preserve its commands in order during physics rebuilding, restore the old
body motion once, and initialize newly added limbs with the original translation.
Late commands run normally; no invented movement kick or replacement solver.

PASS focused attachment/entry/camera/momentum, selected physics/joint templates,
remote lifecycle, articulated display, wire, sleep, shared screen, reliable routing
and ABI checks. Removing the actual velocity hold makes its regression fail;
removing camera correction also fails its control. Routing rejects all 18 controls.
Logs: build/ragdoll-momentum-check/*-final.log. Full 65-group suite was not rerun.

Native DLL 6116e6755237f01f9d227caa9335dfe6167780e756efd1f01d8c8a7868ebd293:
PASS 8/8 controlled movement cases on sci_v1sc and kerfur_omega_h (walk 300,
run 600, diagonal, airborne), with recovery. Native camera PASS 10/10, entry view
change below 0.1 degrees. An optional held-input probe yielded zero movement;
rendered input-path acceptance remains open. Initial recovery failures were a
test setup problem: forceGetUp waits and gates recovery, forceWakeup is immediate.
Retain the failed/intermediate logs; do not count them as successful cases.

LAN on that DLL: entry/recovery observed both ways, selected model correct, but
articulated display absent in the acceptance probe (PARTIAL, streamed=0). A prior
8-second game-thread wait aborted readiness while loading; the harness now respects
its overall bounded readiness deadline. That fix let the drive sequence execute.
Final DLL 3d77b13017ed02d4353ec19a0b7de47035663ab7ca041ea9ed8924c67131d3e2
adds rig/body/batch counts to existing limited emissions and remote rig-change logs.
PASS exact final DLL: momentum8/8, camera10/10 (max view jump0.056deg, max rise60.9cm
over entry centre in crouch), bidirectional LAN selected articulated display and
recovery. Source snapshot2642 entries frozen, all three DLL copies identical.
ABI/native/remote fixtures rerun after the logging changes. Qualification evidence
build/ragdoll-momentum-check/verification.json. The previous partial LAN is not
explained by a behavior change and remains an intermittent acceptance limitation.
PASS complete ElvardFIC-WatchersOfTheVoid-0.9.2158.zip (32 members) and actual
unpacked startup: correct b2181 boot, HEALTH PASS, starter skins4/4 and enabled
fork namespace. build/package-current/verification.json archiveStartupVerified=true.
ZIP SHA256 ba8277a78e51b03ac2cd9dd47707e36f2ed641525b1186a542f7d03d9f14b269.
All private game instances stopped; git diff --check clean. Original installation,
normal profile and Entity code remain unchanged.

## Ragdoll entry camera b2180 / package 0.9.2157 (22 October 2026)

Owner confirms the launch fix and reports the camera looking upward on entry.
Before correction, native game measurements changed from a level view to
89.75 degrees upward at the selected-mesh swap. The inherited camera arm still
had the native plushie's fixed head-relative rotation.

Capture the controller's entry rotation, rebase the head-attached arm after the
swap and through pose warmup/physics initialization, then stop camera writes.
Retain normal physical head following and native recovery. Cached object serials
and actual attach-parent reads reject recycled/reparented camera references.

PASS focused native attachment/entry/camera, selected rig/joint-template, remote
lifecycle/bone batches, articulated display and wire regressions. The camera
regression rejects an actual-substrate mutant with correction disabled. Release
ABI and reliable-kind gates pass with their negative controls (18 routing drills).

PASS ten native 0.9.0n transform/physics cases: sci_v1sc and kerfur_omega_h, each
with level, down/wrapped-yaw, up, crouched and faint entries, with recovery between
entries. Mesh-replacement view differences stay below 0.1 degrees; selected
bodies remain attached and physically move without sky launch. In the NullRHI test,
synthetic pitch resets between frames, so angled stimuli arrange native controller and
camera-arm transforms in the same posted task as ragdollMode. These checks do not
claim rendered-image acceptance or verification of the real input path.

The first LAN attempt completed client-to-host but its old screenshot-framing
teleport moved the reverse observer toward an unconverged puppet position. It
fell/died and disconnected before the host drove. Evidence is retained in
build/ragdoll-camera-check/peer-framing-{host,client}.log. The headless test now
opts out of screenshot teleports/view writes; normal gameplay is unaffected.

PASS final identical b2180 DLLs in both native LAN directions: scientist client
to host and Kerfur host to client, selected articulated display and entry/recovery.
PASS complete ElvardFIC-WatchersOfTheVoid-0.9.2157.zip package verification and
actual startup from the unpacked archive in an isolated manager layout, dev off.
Original fork identity/icon/models and ElvardFIC/AquaFishhh credits are retained.
Normal game/profile files were not modified. No online release was published.

Evidence: build/ragdoll-camera-check/{verification.json,source-before.json,
camera-final.log,peer-host-final.log,peer-client-final.log,native-final.log};
build/package-current/{verification.json,startup.log,UE4SS-startup.log}.
The full 65-group suite remains specific to b2178; it was not repeated for b2180.
Terrain, moving props, high latency, arbitrary imported rigs, sleep/chair transitions
and visual camera acceptance remain in the broader gameplay checklist.

## Corrected full manager package 0.9.2156 / b2179 (22 October 2026)

The previous ragdoll ZIP was a manual DLL replacement, not an installable fork
package. The owner's actual r2modman profile confirms it was inferred as
WatchersOfTheVoid-ragdoll, author WatchersOfTheVoid, version 2179.0.0. Its root
manifest/icon and loader enabled marker were absent; UE4SS did not start it.
The ragdoll code had been tested, but that delivery had not passed manager
installation verification. This packaging failure supersedes the earlier
delivery claim; the player-ragdoll qualification below still applies to its DLL.

Prepared the complete **ElvardFIC-WatchersOfTheVoid-0.9.2156.zip**, retaining the
existing fork name/namespace and original supplied 256x256 icon. README credits
ElvardFIC and co-maintainer AquaFishhh, preserving original Multivoid credits.
The root manifest declares version 0.9.2156 and unreal_shimloader 1.1.7. The
archive contains mod/enabled.txt, mod/dlls/main.dll, starter models/previews and
legal notices, using the original mod/ and pak/ manager routes.

PASS package/package_current.ps1: all 32 entries checked against current inputs,
unchanged qualified b2179 DLL, production source hashes, metadata, icon dimensions
and loader tree. The existing manager-routing predicate rejects the prior broken
archive and accepts the complete one. The author's namespace comes from the
canonical Author-Name-Version filename; the actual r2modman import source was
checked, as were the installed VotV schema's shimloader route rules.

PASS actual game startup from the newly unpacked archive in an isolated manager
layout, with dev mode disabled and default loader settings. UE4SS reports
ElvardFIC-WatchersOfTheVoid's enabled.txt and starts the module. The module reports
Watchers of the Void by ElvardFIC, game 0.9.0n, build b2179, correct namespaced
data path and HEALTH: PASS; the installed starter pak supplies all four skins.
The user's Default profile was inspected without modification.

The DLL is byte-identical to the one which passed the bidirectional ragdoll game
tests. No gameplay source changed for this delivery correction. Evidence:
build/package-current/{verification.json,startup.log,UE4SS-startup.log}.
Package identity has one source in package/release.json. The previous manual-only
delivery is retained under build/package-current/ as a rejected test control.

## Player ragdoll hotfix b2179 (21 October 2026)

Owner report: launch into the sky on entry; standing remote puppet followed by
a recovery teleport; selected character required instead of the native plushie.

Release built with MSVC x64 against the actual 0.9.0n game. The clean dependency
build exposed a WebRTC C++14 / Abseil C++20 link mismatch; the two ICE targets
now use the project's C++20 standard. Final incremental build reports no work.
This is a focused hotfix qualification, not another 65/65 b2178 full-suite run.

Reproduced before repair:

- Original game log repeatedly read an unknown ReturnValue from the void Actor
  attachment call, then destroyed the remote body. The actual-ParamFrame native
  regression failed on this behavior.
- Native entry regression failed because the selected mesh was lowered to the
  plushie's pelvis, putting the full-size feet below the floor.
- Isolated live Kerfur still rose 18–35 metres after the initial geometry fixes.
  Native asset audit found the full rig's mixed-scale chest pivot; the measured
  joint-anchor gap was 1069.2 cm. Entry velocity was zero before simulation.

PASS focused checks:

- run_ragdoll_native.ps1 -CompileSources: actual ParamFrame attachment contract,
  pelvis parent verification, recycled actor/body/mesh, missing parameters,
  selected geometry retained, one staged configuration and recovery; three
  production translation units compile.
- run_ragdoll_skin.ps1: selected mesh/materials, compatible common/custom rig,
  bodies/joints/pelvis, transform before physics initialization, no player AnimBP,
  stale objects and failures. Separated anchors are repaired while instantiating
  bodies and shared NPC templates restored on success, dispatch failure and a
  later joint-read failure. Foreign authored rigs are not retargeted.
- run_ragdoll_retry.ps1: actual remote lifecycle, drift, complete bone batches,
  parent ordering, rig replacement, malformed poses and retry/recovery.
- sleep_display_test.cpp: model/material/scale, articulated pose and sleep
  transitions. shared_screen_wire_test.cpp: actual protocol and bone validation.
- abi_gate.py --drill: no UE4SS imports; injected forbidden import rejected.
  reliablekind_gate.py --drill: 224 kinds and all 18 negative controls pass.
- Fault control with joint-anchor repair deliberately removed fails the native
  skin test. Original attachment and geometry failures are retained as controls.

Actual game evidence uses isolated executables/profiles and native PhysX under
NullRHI. Normal game/profile files were not modified. The loading curtain now
uses a steady clock: its former game-thread ImGui::GetTime call faulted without
a renderer and prevented the test client's join from completing.

PASS Kerfur local entry/fall/rest/recovery: 64 samples, maximum pelvis height
22.5 cm above the pre-entry actor centre; selected model, remote mirror model,
pelvis attachment and recovery all verified. PASS scientist sci_v1sc with the
final physics implementation: 52 samples, the same 22.5 cm maximum and all flags
true. Earlier broken builds rose thousands of centimetres in the same probe.

PASS real LAN in both directions: sci_v1sc client to host, then kerfur_omega_h
host to client. Both observers verify the selected mesh and complete articulated
network display; rising and falling edges observed, no observer death.
Final bidirectional run and artifact hashes are recorded in
build/ragdoll-evidence/verification.json. Its logs distinguish the reverse round.

Evidence: build/ragdoll-evidence/, build/ragdoll-fault-controls/ and the actual
native harnesses in agent/tests/. Remaining gameplay acceptance includes arbitrary
imported rigs, slopes/moving surfaces, high latency and sleep/chair transitions.
Entity implementation and the shared native physics asset defaults are unchanged.

## Imported main history (superseded by current release status)

## Recheck: PC boot, C-key ragdoll and resource identity (21 October 2026)

Repeated `run_ragdoll_skin.ps1`, `run_shared_screen.ps1` and `run_trowel.ps1`;
all passed. Then `package/check_current.ps1 -EvidenceDirectory
build/continued-20261021` passed **65/65 groups**, Release identity/ABI and the
stable-source check (781 entries). The production snapshot and DLL hashes match
the previously qualified b2179 build. This confirms the offline regressions;
the stationary-PC host/client loading report and all-role C-key ragdoll report
still need real-peer acceptance on matching b2179 DLLs.

The full public-prose and fact-row gates were not rerun: the WindowsApps Python
launcher failed with “The file cannot be accessed by the system.” Their last
recorded findings therefore remain unchanged (13 prose counters above baseline;
45 fact-row findings across 525 rows/157 files). The include-context gate passed
as part of the 65-group qualification and does not replace those full gates.

## Development b2179: PC boot, C-key ragdoll and held materials (21 October 2026)

Owner report: the stationary base PC stays loading on host and client. The
production shared-screen wrapper regression reproduces an old boot snapshot
writing `rootswitch` back to 0 after native startup has set it to 1. The previous
code fails the root-page assertion. Desktop-only traversal passes and preserves
native boot/power pages across repeated paints, while still restoring desktop
text, scroll and tabs with late-child retry. `run_shared_screen.ps1` passes,
including the existing wire, lifecycle and sleep-display cases.

`computer_boot_native_test.py` checks the existing laptop corpus: rootswitch 0
at ubergraph 29, 1 at 329, delayed continuation 315, anim=false at 550 and the
launched notification at 561. No fresh pak/Blueprint extraction was run. Actual
host/client startup, repeated power cycles, entering/leaving during boot, power
loss/restoration and join/reconnect remain gameplay acceptance for reports 54/55.

Owner report: pressing C launches the player on all roles and breaks ragdoll.
Model/runtime logs are not yet recorded. New production regressions fail on
the previous code: the skin setup changes mesh/physics asset before stopping all
old bodies, and local startup aligns the pelvis before animation warmup. The
latter starts simulation at the moved warm pose rather than the entry anchor.
An additional regression exposes the different-rig height error: the native
shell's pelvis cannot determine the selected model's standing/crouched height.
This is an offline reproduction of code defects, not of PhysX launch.

The skin setup now disables collisions and all old bodies before replacement.
Local startup captures the selected model's standing/crouched pelvis, preserves
that entry anchor across failed setup retries and
teleports the warmed pose to it immediately before simulation. Pawn, body, source
mesh and selected asset identities are checked across calls; missing/non-finite
poses and failed teleports refuse startup. Unreadable initial velocity becomes
zero, without reusing a previous entry's velocity.

Focused PASS: `run_ragdoll_skin.ps1` (actual production startup seam and skin
wrapper), `run_ragdoll_retry.ps1` (remote lifecycle and both production TUs).
Tests cover different native/selected pelvis heights, crouched entry, warmed-pose
movement, retry after configuration/read/teleport failure,
single startup, velocity preservation, non-finite positions and replacement or
teardown during reads/configuration/translation/start. Native C-key/faint and
standing/crouched/repeated recovery, slopes and selected/common/custom models
still require matching b2179 host/client gameplay. Reports 5/50 remain open.

The native `mainPlayer.updateHold` corpus reads `saveSlot.hold[0]` before replacing
the held actor. Existing profile snapshots refreshed water/material values only
in a copy; immediate native stock persistence covered only trowels. A new
production-controller regression first failed on the unchanged water-hand use
path because it never invoked persistence.

The controller now persists local held stock after water use/tick, material
changes and both native cement-transfer endpoints. The inventory wrapper accepts
the five exact supported classes, matching inner/outer keys and a current hand/
save/world/slot, then confirms live stock again. The codec validates all existing
bool/float/int groups against the snapshot before writing any changed values,
without native dispatch, engine allocation or replacing array headers.

Material focused PASS: `run_trowel.ps1`, `run_2153.ps1 -CompileSources`,
`run_2169.ps1 -CompileSources`, `run_nonentity.ps1`, `run_sponge_effects.ps1`.
Coverage includes zero/refill, unchanged base groups and array ownership,
malformed/missing shapes, stock changes, nested saved-value changes, actor
recycling, world/save/slot replacement and selected-hand controller hooks.
The scalar-memory test compiles the actual production codec function.

The first full attempt exposed an obsolete scalar-codec mock in
`inventory_view_2146_test.cpp`; it now uses the current API and `run_2146.ps1`
passes. Final qualification via `package/check_current.ps1
-EvidenceDirectory build/material-persistence-check` passes **65/65 groups**,
the stable-source guard and Release identity/ABI inspection (781 build entries).
Production snapshot SHA256:
`828154d98e2208cf82869997293f059d422511e62a8a5b2f801b5d4592ce3dcc`.
Development DLL `build/votv-coop-2154/main.dll` SHA256:
`5a92cbeda18433d3a3dde41494ddf75a2e8f0979c7698136f1fe97af53271332`.
Package 2155 and its delivery archive remain b2178. No package, installation or
publication is part of this continuation. Runtime resource transitions, trowel
key mismatch repair, concurrent writers and paid debit/output/refund transactions
remain open. Bench58 and Token52 are unchanged; no fresh asset audit was run.

Full public-prose and fact-row gates were rerun during this continuation: prose
still has 13 counters above baseline; fact rows reports 45 findings across 525
rows in 157 files (missing reference entries and unused declarations). Logs are
`build/material-persistence-check/public-prose.log` and `fact-rows.log`.
Passing offline groups do not close these full CI findings.

## Final package 0.9.2155 / DLL b2178 (21 October 2026)

At the owner's explicit request, promoted the qualified b2178 continuation into
the final **0.9.2155** package. This supersedes the earlier local b2177 package
described below. The 2155 changelog and GitHub notes now include all continuation
fixes; README, release instructions, checks and delivery metadata agree on b2178.

PASS final incremental Release build (`build/release-2155-logs/final-build.log`):
no work needed. Rechecked identity/imports/exports and the unchanged production
snapshot against `build/continuation-check/verification.json`, retaining its
**65/65 passing groups**, 781 source entries and b2178 qualification. No production
code changed during final packaging, so the full suite was not repeated.

PASS `package/pack_2155.ps1` and `package/verify_2155.ps1`: manifest version 0.9.2155,
256x256 PNG icon, 32 archive members, current embedded documents/paks, qualified
DLL, enabled loader layout and matching outer delivery copy.
Evidence: `build/release-2155-logs/release-verification.json`.
Delivery: outer `dist/WatchersOfTheVoid-0.9.2155.zip`, `GITHUB_RELEASE_2155.md`
and `SHA256SUMS-2155.txt`.

DLL SHA256: `9e5f4fd23b272efa6fd807df45cba466664acfe8639c5d4d639a9f23c9c4d5f8`.
ZIP SHA256: `ce2688393150abb294f3c3edb4acc489439042fc661fd09eefa80afb029764b1`.
All peers need this final b2178 package; the earlier local b2177 preparation is
incompatible. No online publication, commit, push or game installation performed.
Real-peer acceptance, host-rifle runtime evidence, Token52, full event coverage,
paid material transactions and the deferred native audit remain open as recorded below.
