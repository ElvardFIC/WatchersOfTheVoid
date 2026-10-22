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

## Development b2178 continuation (21 October 2026)

Focused PASS `run_placement.ps1` and `run_2161.ps1 -CompileSources`.
New `remote_destroy_lifetime_test.cpp` includes the actual production receiver with
native/reflection boundaries doubled. Before the fix it failed when a destroy
listener recycled the selected object's slot; after the fix it covers listener,
metadata lookup, drive/grab release and unpin replacement, death, world/session
changes, recursive destroy, listener registration, ordinary success, local consume,
conversion cleanup, deferred application and preserved load-tail behavior.

The location harness first failed because a new session serial at the same pointer
and world still counted as current. After the fix, session reuse, pose teardown,
pawn serial replacement, native bounds-query world changes, recursive ticks and
actual-overlap cancellation pass along with existing geometry/authority cases.
All five location-related production TUs compile separately.

Full qualification **PASS, 65/65 groups**, via
`package/check_current.ps1 -EvidenceDirectory build/continuation-check`:
unchanged full production snapshot, 781 build entries, final incremental Release
build up to date, loader ABI/imports/exports and 224 reliable kinds/18 routing
fault controls. Evidence: `build/continuation-check/{verification.json,identity.json,results.json,build.log,logs/}`;
initial Release compilation: `build/continuation-build.log`.
DLL SHA256: `9e5f4fd23b272efa6fd807df45cba466664acfe8639c5d4d639a9f23c9c4d5f8`.
Production snapshot: `5abdd52bf142d0a452b16f904163a7496622fe11be0bcc382975f0f965eeb1e7`.
An earlier changing-tree attempt was refused by the source-stamp check after the
final echo-mark ordering fix; this stable-tree result supersedes it. The final
regression also verifies no incoming-destroy mark survives a cancelled attempt.
The source protocol is b2178; the previously prepared package 0.9.2155 remains
b2177. Its outer ZIP SHA256 remains
`37430bd3701e56d72de7960d9f6b4afaf87311ef57e63d70d225eb7ac982048f`.
These later changes are recorded under Unreleased in the changelog.
No new ZIP, commit, push, game installation or publication was performed.

Bench58 remains open: no host old-rifle disappearance was reproduced in the game.
Existing-corpus inspection also checked inherited prop lifespan serialization,
native inventory cleanup and the gun-stealer's literal `arirGun` target; no broad
rifle-destruction or lifespan behavior was disabled. Walking-tree bytecode has
viewer-dependent behavior and latent timers, so its rate/authority route remains
deferred rather than being admitted solely by class. Token52 is unidentified;
fresh pak audit and real-peer acceptance remain deferred/unverified as before.

## Package 0.9.2155, stable Release b2177 (21 October 2026)

PASS `package/build_2155.ps1`: Windows x64 Release, **65/65 offline validation
groups**, unchanged full production snapshot across tests and final incremental
build, 781 build source entries, identity/imports/exports, 224 reliable message
kinds and 18 rejected routing fault controls.
DLL SHA256: `4ab599f644354070a272f79037c023dd65bcadc5ac55a16ab38f7017e02fa304`.
Evidence: `build/release-2155-logs/{verification.json,identity.json,results.json,initial-build.log,build.log,logs/}`.
The historical build directory remains `build/votv-coop-2154`; its DLL is b2177.

New production regressions cover:

- Owned/bounded event replay across recursive native callbacks, queued replacement,
  world/disconnect/session changes, metadata-readiness retries and consumed ambiguous
  native attempts. Preparation failures remain retryable; actual attempted native
  calls are not repeated after uncertain completion.
- Active-event begin/end sender serials, null/bounded frames, copied registry
  membership, failed/readiness join debt, recipient replacement and teardown.
- Owner-local night-eyes frequency, native interval restoration and context changes
  during getters. `event_rate_native_test.py` checks the existing cooked ticker's
  1800–2700s wait, 5% roll, day/night gates and owner-entity route. The complete
  random-event list is not covered; no fresh pak audit was performed.
- Exact-identity native held-trowel stock persistence, zero/refill, malformed schemas,
  changed slot bytes/actor/save/world/stock and callback rejection. `run_trowel.ps1`
  uses the production inventory function and extracts the actual saved-int helper
  and canonical offset from production for its array-schema harness.
- Admitted client placement recovery after exhausted send/transform/food-capture
  retries, bounded/full recovery pools, sweep immunity and no fallback shared birth.
  These checks include `prop_rifle_C` as a fixture, not a host disappearance repro.

Latest Bench58 owner answer: **host**, item **old rifle**. Components remain
unidentified. Existing `prop_rifle` leaf Blueprint has destructive LMB use without
a placement entry point; inherited behavior and actual disappearance cause remain
unverified. Decoding evidence: `build/release-2155-logs/prop_rifle-existing-corpus.txt`.
Use the existing `VOTVCOOP_PROP_BIRTH_KEY_PROBE=1` diagnostic before a reproduction;
capture both peers' birth/destroy caller/class/key and cleanup/drop logs.
Token52's counter/UI is still unidentified; no balances were changed for it.

Full native-contract audit remains unavailable against the incomplete corpus and
fresh extraction remains deferred. Full public-prose/fact-row CI retains the known
failures documented below. No real-peer event/frequency/join/reconnect, native
physics, trowel reconstruction or host-rifle acceptance is claimed.

Package/delivery validation is recorded separately in
`build/release-2155-logs/release-verification.json`: PASS, 32 archive members,
qualified embedded DLL and current documents/manifest/paks, matching outer copy.
All changes since package 2154,
including the fish integration, are listed in `package/CHANGELOG_PLUS.md`.
No commit, push, game installation or online publication is performed.

## Fish integration into main, protocol b2176 (21 October 2026)

Source: `watchers-of-the-void-fish` at
`e48f19b8717c8b695c6d235499d0baf31fccbf6b`, relative to `b6651cb91`;
main integration baseline: `74e1729e5`. See WORK.md for the behavior inventory
and HANDOFF_FISH.md for the original branch's qualification and open work.

The merged protocol keeps Kavotia=223 and placement-result=224; Watoy moves to
235, with new fork kinds 225..234. Placement requests/replies are 188/101 bytes;
cooking progress, tool/player identity and other new layouts require matching
b2176 peers. The authority table and all dispatch/lane/build/lifecycle entries
preserve main's Entity, event-frequency/delivery and personal-sleep changes.

Conflict-specific checks exercise the actual placement and coin controllers:
acceptance-only record publication, same-key refusal, active/live key reservations
before index enrollment, synchronous replies, immutable retries, retained replies,
actor/world/occupant changes, readiness, malformed strings/padding/cooking values,
pre-eid coin reservation and retirement after faulted native destruction.
Existing fault controls now expand the production .inc fragments before mutation;
the queue-cap control exercises simultaneous retained captures and callback arrivals.

Final stable-tree qualification: **PASS, 63/63 groups**, production snapshot
unchanged through tests and final incremental Release build; **781 source entries**.
Command: `package/check_current.ps1 -EvidenceDirectory build/fish-integration-check`.
DLL: `build/votv-coop-2154/main.dll`, **0.9.0n b2176**.
SHA256: `b069a8cfb542c86a7569c495302a882003ef5a245feb1f126210ce26b02355c1`.
Evidence: `build/fish-integration-check/{verification.json,identity.json,results.json,build.log,logs/}`.
Identity, imports/exports and all 224 reliable kinds pass; all 18 routing fault
controls are rejected. The full source/config hash is in verification.json.
The earlier changing-tree pass is superseded by this final run. Its failures
identified merged duplicate test doubles, moved production-fragment includes and
an outdated queue-cap fault fixture; focused reruns passed after adaptation.
No passing result is claimed for the changing-tree snapshot.

Additional validation limits:

- `native_physics_fingerprints_test.py`: three captured encoding controls and two
  documented derived signatures pass. This is not a fresh installed-game census.
- `public_prose_include_test.py`: all four include-context cases pass.
- `sync_native_contract_test.py` was not rerun: this workstation lacks 18 of its
  45 literal package inputs, and additional dynamic inputs are required. The
  owner's fresh pak/Blueprint audit remains deferred. The branch's original
  58-file installed-pak contract qualification is provenance, not a local result.
- The complete working-tree public-prose census, including new unstaged source
  files, remains above baseline on 14 counters (1905 source files;
  `build/fish-public-prose-working-tree.json`). The normal tracked-only command
  reports 15 counters because it cannot see new callers in untracked files.
- `fact_rows_gate.py` remains red across 525 rows/157 files, including missing
  native-hook references and unused existing rows; details are in
  `build/fish-fact-rows.log`. Full repository CI is not claimed green.

Two-peer gameplay, physical projectile/collision behavior, atomic resource
spending/refund, ordinary paid cement repairs, remaining dynamic grime sources
and scorch provenance through full world/save reload remain open. No package,
game installation, commit, push or publication was performed.

## Event synchronization: stable Release b2159, 40/40 groups (21 October 2026)

PASS `package/check_current.ps1 -EvidenceDirectory build/event-sync-check/validation-2159`:
all **40/40** groups, production-source snapshot unchanged across tests/final
incremental Release build, DLL identity/imports/exports and 762 source entries.
This result supersedes the earlier changing-tree attempt below.

DLL: `build/votv-coop-2154/main.dll`, **0.9.0n b2159**.
SHA256: `87a0ba2a4190fd383eb8abb31a13f1982e958f1964a87283fd8eefa80902f49c`.
Evidence: `build/event-sync-check/validation-2159/{verification.json,results.json,identity.json,build.log,logs/}`.

New `run_events.ps1` checks chosen/repeated native specials, direct F1 emission,
reflected scheduler calls, refused-send ordering and healthy-recipient dedupe,
peer-slot reuse, world/session replacement, reentrant send teardown, client-only
replay/admission, malformed dispatch and install-pump load-window retry. The rate
harness covers four exact spawner classes, default2x/live0.5..5x changes, native
re-arming, no cumulative scaling, failed setter retry, pre-session/late actors,
CDO/foreign-world/personal ticker exclusions, restoration/recycled identities and
reentrant engine callbacks. All four event production TUs compile separately.
Related location/gather, schedule/spawn authority, personal-sleep and world-sound
regressions pass as part of the 40-group run.

The complete eight-category event request is still OPEN. The owner requested
skipping the fresh .pak/Blueprint audit for now; the four rate-controlled
generators do not establish coverage of every repeatable random event. Full
two-peer effects, late join, simultaneous activation and gameplay remain untested.
No package, game installation, commit or publication was performed.

Additional source gates are outside that 40-runner suite: fact_rows_gate still
reports existing unused rows/missing native fingerprints in other lanes, and
l10n_pot --check reports stale catalogues/obsolete existing translation entries.
The new native interval row has a reference entry; no fingerprint was fabricated.

## Earlier source 2159 changing-tree attempt (21 October 2026)

The owner confirms parallel event work remains active. A later Release b2159 build
succeeded, but it is not the completed b2158 audit snapshot below. All39 runners
passed in the later attempt; identity verification refused the old DLL when source
protocol advanced during the run. Preserve that distinction rather than counting
the changing-tree attempt as a matching full validation.

The current checker now checks production-input hashes across tests and a final
incremental Release build. Syntax and source-stamp generation pass. Its stable full
run must follow completion of the other task. Current build metadata and the last
fully verified snapshot are separately recorded in build/todo-audit/verification.json.

## Full TODO audit: 39 groups and Release b2158 (21 October 2026)

PASS package/check_current.ps1: **39/39** groups, current Release identity/exports/
imports/source list, retained wire IDs202/203/204, and the reliable router's18 failure
controls. Evidence: build/todo-audit/verification.json, results.json and logs/.
Verified audit snapshot: 0.9.0n b2158; source count760. The mutable
build/votv-coop-2154/main.dll path has since been rebuilt by the parallel continuation.
SHA256: d8cd0c113cc0429160a70ec4e3fa042908e2c6b447e7208ae3a5e7a806300d23.

The audit fixed stale held-mirror publication/duplicate birth and the missing laptop
ON/OFF retry after loading/animation/power recovery. Both production regressions pass.
Restored the older API-drifted runners rather than relying on historical pass reports:
record/placement mutation controls, item/codec/pry/birth paths, native explosion suites,
cord performance, console/toys, resource wrappers, teleport, lamp and laptop recovery.
The expected assertion failures in mutation logs are rejected mutants, not failed groups.

Reviewed every listed fork candidate; upstream tab/camera/NPC-join/lobby fixes are
already merged. Local explosion lanes are retained because upstream's177 conflicts
with KitchenState177; affected-prop coasting remains unimplemented.

Bench58: owner now identifies an old rifle and components, in one isolated placement
case. Exact component and authoring role are unknown; the prop_rifle_C candidate's
placement refusal/sweep protection passes offline. The disappearance itself has no
new gameplay result. Token52 remains unidentified by the owner.

TODO now checks confirmed source work separately from gameplay. Cement/resource
transactions, remaining creature/event adapters and other explicitly open code work
are not marked complete. No two-peer gameplay, package, installation or publication.

## Continued 2158: placement acknowledgements and coin collection (21 October 2026)

PASS all 16 validation groups on the merged tree: the nine prior focused groups,
garden, laptop UI, world sound, placement/merged ABI, coin handoff/collect and its
production TU, native coin bytecode and reliablekind_gate --drill. The router
reports 213 kinds, no duplicate claims and all 18 injected failures rejected.
New compiled ABI checks pin retained PropScaleState 202, SharedScreen 203 and
LaptopUI 204, placement request/result sizes 184/8, host-only result authority,
Bulk ordering with PropSpawn/PropDestroy and incompatible-version refusal.

Placement regressions exercise invalid lengths/NaN, existing-key conflict,
forbidden fresh birth, failed spawn, send retry, repeated and reentrant requests,
same-key concurrent requests, eviction replay refusal, peer departure and reset
during send. Client checks cover refusal/timeout protection and recovery, stale
sender exclusion, duplicate ACKs, late adoption, both ACK/bind orders and late
acceptance. Save records are absent before acceptance and on refusal; capture/send
callbacks cannot revive reset work. Existing food/transform/birth tests also pass.

Coin regressions exercise host E/overlap and forwarded requests racing inside
native dispatch, including nested calls marked fromOurCode, pre-eid overlap,
native no-op retries, unreadable balance, partial dispatch failure, recycled
identity, world change, teardown, malformed/unauthorized requests and transport
retry reentry/reset. Personal map coins stay native on clients. Native asset
test pins E -> graph 441, overlap -> mainPlayer cast -> graph 441, then points,
hint and native destroy; the shared credit block ignores player parameters.

Full MSVC x64 Release build and final incremental link PASS. DLL file/product
version **0.9.0n b2158**, SHA256:
`a04891ee860b33033091bcd69c73662176f2c8162cde53275ac6fe461dae7252`.
package/verify_current_build.ps1 derives the expected identity from source and
passes Release configuration, imports/loader exports and all 760 listed source
paths. Evidence: `build/entity-sync-check/validation-2158/{tests.json,identity.json,
verification.json,logs}`. Previous b2157 evidence remains historical; the build
directory now holds b2158. Reconfirmed origin/multivoid-sync-2153 ancestry at
HEAD 023f256168f0940c74d1eea7a3bf34e02f2dd8fd with continuation edits present.

No two-peer gameplay tests or new release archive. Existing C4715 in
npc_pose_drive.cpp and EH option warnings remain. Deployment/packaging remains
separate; archived 2154 package identity checks describe b2156, while current
development checks use verify_current_build.ps1.

## Continued 2157: Kavotia state, authorship merge repair and personal effects (20 October 2026)

Implemented Kavotia's native aim/stance and charge-loop/pitch/windup snapshot on
host-only reliable kind 223 (25 bytes), sharing Grayboar's bounded queue helpers.
The adapter preserves native mesh initialization, refuses client latent AI/shot
entries, samples host actor ticks even for culled meshes, repairs a join through
heartbeat and stops stale loops independently of particle/pitch availability.
Actual-source tests cover authority/pre-bind/malformed frames, bounds, sequence
wrap, delayed binding, metadata/parameter retries, completed-operation retention,
component/actor replacement, reentrant callbacks/receive/teardown, four rows/tick,
32 pending per peer and full-pack join repair. Native bytecode assertions pin the
setup entry, native movement rates, charge graph and exact burst/montage names.
The shared fanout also has a Grayboar regression for peer departure erasing a cue
inside its send callback; a delivery revision aborts that pass before indexing the
changed queue, and remaining peers resume in FIFO order on the next tick.

CI exposed 38 pre-existing authorship omissions after the merge. Repaired 27
client-intent/owner-sample rows and 11 host-only rows by their real paths. The
reliable-kind gate passes all 212 kinds and 18 injected failure controls after
fixing its false-return parsing and stale family-dispatch mutation.

Wisp tests found an actual missing sender check on WispGrab; fixed it and added
immediate death/hold/tear cancellation on original-pawn loss/recycling/replacement,
native death and world changes. A grab rearmed during release survives without
receiving the old kill. Removed blanket client nightmare suppression; the updated
sleep test checks preserved native probability/entry, unchanged wake/rest state,
real rest-report layout and sender authority. Updated the stale preview-spawn stub.

Release `0.9.0n b2157` builds. All 13 groups pass: player reports, wisp animation/
release, ragdoll retry/skins, personal sleep, entity sync, garden, laptop UI, world
sound, three native asset audits and reliable-kind CI/drill. Evidence and DLL hash:
`build/entity-sync-check/validation-2157/verification.json`. Existing C4715 in
`npc_pose_drive.cpp` and MSVC EH-option warnings remain. No two-player gameplay or
new package; Kavotia fire/laying montage, voice and beam/impact/damage remain open.

## Sync-2153 ancestry audit and entity-route repair (20 October 2026)

Verified `multivoid-sync-2153` tip `bf276d14c4d30dc1ba95ea9ea184f7d9e89dedef`
is parent 2 of merge `7fced281e92c4cb2df0e37797bb533041022b087` and an ancestor
of current `main`. The merge's ten manually resolved paths retain local reliable
kinds 202/203, assign Laptop UI 204 and combine both Grayboar implementations.
The sync branch's laptop, garden, world-sound and placed-hose source files remain
connected to the build/session. Current source is protocol 2156.

The entity audit found that `npc_world_enum.cpp` logged and the native test read a
dead EX-spawn source table, while the executed table omitted the Fossilhound and
yellow-wisp tickers plus boar-war/tank controllers. There is now one executed table;
the runner asserts its required sources and rejects a second table. The existing
`ariral_sync` watched nonexistent `StealGun`/`TakeWeapon` exports. Native 0.9.0n
shows a timer export named `punching`; the gun stealer is now a late NPC class and
only a confirmed client host-mirror refuses that timer, preventing duplicate player,
prop and door effects. Gun attachment/held-item state remains open rather than being
claimed from pose replication.

The reusable part of the Grayboar action path now lives in
`coop/creatures/reliable_action_queue.h`: non-zero wrapping sequences, ordered
receive admission, bounded per-peer fanout, expiry, independent FIFO retry and
departed-peer cleanup. `grayboar_actions.cpp` consumes that layer without a wire
change. Its existing saturation, wrap, retry, expiry, reconnect, late-join and
reentrant playback tests all pass. Entity-specific native sampling and cosmetic
playback remain in adapters; gameplay verbs are not replayed by the shared queue.

`agent/tests/run_entity_sync.ps1` passes all entity/Grayboar/prop-drive regressions
and production compilation, including `npc_world_enum.cpp` and `ariral_sync.cpp`.
The existing `npc_pose_drive.cpp` C4715 warning remains. Native Python evidence was
updated but could not run in this environment because only the Windows Store Python
alias is installed. No two-player gameplay claim, DLL rebuild or package was made.

## Merge resolution - local branch with multivoid-sync-2153 (19 October 2026)

Resolved the Grayboar source/test conflicts by retaining local authority and
identity checks and adding the remote stale-replica and per-peer retry coverage.
Kept local wire kinds 202/203 and assigned LaptopUI kind 204. Source protocol is
2154. Merge whitespace check passed; no build or gameplay tests were run for the
combined tree, and no delivery artifact was created.

## Continued 2153: nonlethal hit animation/audio (19 October 2026)

Expanded grayboar_actions_test.cpp verifies four native-selected damage sections,
same-section repeat hits, finite health decrease, skipped/malformed arguments,
stopped/wrong montage, mirror/recycled identity, invalid rate and lethal precedence.
Client checks cover direct damage cue plus attack-audio stop, unchanged health,
four silent join snapshots and disconnect/recycle during sound setup. Native
asset tests pin damage1..4 RNG, skipAnim and the SoundCue/idle/attack audio chain.
Targeted C++/native tests PASS. New action values 4..7 reuse the existing 17-byte
payload; version stays 2153. Gameplay, simultaneous hits, native montage failure
and exact audio/network timing still require validation on matching peer DLLs.
PASS all nine focused runners and full MSVC Release b2153 build. git diff --check
is clean. Current DLL hash/scope: build/release-2152-logs/entity-validation.json.
Existing delivery ZIP untouched; no new version/package created.

## Continued 2153: AI continuations, collision and charged form (19 October 2026)

Expanded actual grayboar authority regressions to 24 owner/name registrations,
including separately declared charged overrides and their failed-watch retry.
All six AI latent destinations refuse client replicas; target/delegate cleanup,
construction, cosmetics and teardown retain native entry behavior. Existing
host/personal/offline, adopted/held identity and malformed-frame checks pass.

Native evidence pins collision impulse plus addDamage(10), lightning replacement,
charged explosion and death timer, player punch, random spin, senseUpdate/unhook,
setWalking mode changes and old dest delegate cleanup. Every watched function
must be exported by its declared base/charged package. Targeted C++ and native
tests PASS. Live replacement/effects, helper task retirement and two-player
results remain unverified. DLL/protocol stay 2153; no new package/version.
PASS all nine focused runners (five entity executables, twelve production units)
and full MSVC Release b2153 build. git diff --check is clean. Current hash and
scope: build/release-2152-logs/entity-validation.json. Existing ZIP untouched.

## Continued 2153: cosmetic boar turn and motor pitch (19 October 2026)

New grayboar_turn_test.cpp compiles the actual production module and verifies
host native sampling, 10 Hz limit, latest-state retries, constant-state join
heartbeat, world readiness, live host identity, malformed/range refusal,
host-only client acceptance, ordered/wrapping sequences, signed interpolation,
native turn overwrite, pitch=0.5+1.5*abs(turn), stale neutral fallback, queue cap,
missing audio metadata, recycled objects and reentrant/world/disconnect cleanup.
The entity runner now has five executables and twelve production-unit compiles.
Native asset tests pin BlueprintUpdateAnimation entry 3948, body/AI vector inputs,
ModifyBone consumers and the audio_loop_turn pitch formula. Targeted tests PASS.
Gameplay-visible steering and sound remain unverified on two peers; host/client
clock alignment and old helper delegates remain open. Development stays 2153.
PASS all nine runners in package/check_2152.ps1 and the full MSVC Release build
(454 rebuilt targets, b2153). git diff --check is clean. Current hash/evidence:
build/release-2152-logs/entity-validation.json. Existing delivery ZIP untouched.

## Continued 2153: adopted boar helper path abort (19 October 2026)

Added an active-path StopMovement call on a client replica's verified owned
grayboarPawn AIController. Idle paths and host/personal/unrelated controllers
are untouched. Actual production-source regressions cover late possession,
bad ownership, metadata/call retries, active/idle/invalid statuses, recycled
objects, world/session/authority changes, reentrant task completion and held
handoffs. Native bytecode confirms owner=self and the helper's AIController use.
Targeted boar executables and --boar-only native evidence PASS. Native engine
task/delegate lifecycle and visible turning still need two-player validation.
PASS all nine runners in package/check_2152.ps1 and full MSVC Release b2153 build
in the existing build directory. git diff --check is clean. Current DLL hash and
scope are recorded in build/release-2152-logs/entity-validation.json. No new ZIP.

## Continued 2153: boar navigation guards and build integration (19 October 2026)

Added client host-replica refusal for move/updMov/path checks, navigation retries
and completion, plus the native delayed move continuation at ubergraph entry 15.
Other graph entries remain native. Expanded actual-production tests and bytecode
evidence cover this bypass, identity/role gates and malformed EntryPoint frames.
Active pre-existing AI tasks and visible turning still require follow-up.

Fixed duplicate test/source/runner fragments and namespace closures found during
validation. Added missing includes/source-list entries for four already-called
creature modules so the existing full build links. Their gameplay is not verified
by this integration repair. Release DLL remains b2153; no new version/ZIP.
PASS all nine focused runners and full MSVC Release DLL build. Current evidence
and hash: build/release-2152-logs/entity-validation.json. No two-player validation.

## Continued 2153: death animation/audio and loot protection (19 October 2026)

Death is cosmetic action 3 on the existing 17-byte channel. A new host dying edge
plays the native death section at animationSpeed / 2; already-dying/skip-animation
paths do not emit another cue. Client playback silences idle/attack/roar, spawns
the native attached death sound, and never binds the native death delegates.
Joined death resumes the current montage position/rate without restarting sound.
Client host-replica damaged, death loot-notify and completion bodies are refused;
host/personal behavior remains native. This does not prove corpse/loot gameplay.

PASS all nine focused runners (four entity executables, eleven production-unit
compilations), expanded targeted boar tests and native asset evidence. Full MSVC
Release b2153 linked successfully in the existing build directory. Current hash
and scope: build/release-2152-logs/entity-validation.json. No ZIP/new version.
Still OPEN: two-player lethal client/host hits, death timing, single loot outcome,
retirement/reconnect, helper/turning, exact time alignment and joined audio.

## Continued 2153: joining during a boar action (19 October 2026)

Added a world-ready native montage snapshot for current attack/roar animation.
Registered host-boar candidates are validated by weak identity and sampled with
a per-tick budget. The 17-byte action payload carries current position/rate;
host/client queue time advances the resume position. The receiver verifies its
section and stops only the stock boar montage when the sampled action has ended.
One-shot sounds are not restarted for a join snapshot.

Production-module regressions cover active/stopped/dead/recycled candidates,
duplicate and departed slots, native position/rate, local waiting time, stale
section consumption and malformed positions. Exact network-time alignment, joined
audio, death and helper/turning remain OPEN; two-player validation is still needed.
Current build evidence/hash: build/release-2152-logs/entity-validation.json.
Development version remains 2153; no new package/version is created.
PASS all nine focused runners, including four entity executables and eleven
production-unit compilations. Full MSVC Release DLL builds as 0.9.0n b2153 in
the existing directory. The delivery ZIP is unchanged and predates this work.

## Continued 2153: live grayboar attack/roar cosmetics (19 October 2026)

Implemented host transition observation -> reliable kind 200 -> client direct
montage/section/audio playback, with no native damage/loot callback proxy replay.
Sequences, payload validation, bounded actor/animation retry and expiry, send
retry, slot departure and world/session reset are covered by production-module
regressions in grayboar_actions_test.cpp. Native evidence confirms the exact
`atttack`/`roar` sections, host animationSpeed and audio components.

PASS all nine focused runners. Entity runner now executes four regressions and
compiles eleven production units, including reliable dispatch. Full Release DLL
build stays 0.9.0n b2153; existing build directory reused, no ZIP/version created.
Hash and scope: build/release-2152-logs/entity-validation.json. Existing packaged
DLL/ZIP predates these changes. Matching current peer DLLs still need live tests.
Death cues, mid-action join, playback time alignment and helper/turning remain OPEN.

## Continued 2153: client target and roar authority (19 October 2026)

Extended the host-replica authority guard to grayboar sensePawn and detected.
Clients cannot independently assign a target/start target-driven movement through
sensePawn, or start the roar and bind target.OnDestroyed through detected.
The host, personal/local boars and inactive sessions keep native behavior.
Already-running helper tasks and pre-adoption target delegates remain follow-ups.

PASS run_entity_sync.ps1: three regression executables and nine production-unit
compilations. PASS full entity_sync_native_test.py: spawn catches, ancestry and
native target/move/roar side effects. Tests include an individual failed watch
registration recovering while installed neighbours remain intact. No two-player
test, shared action replay, new version, delivery DLL or ZIP produced.

## Continued 2153: client boar bite authority (19 October 2026)

Added production grayboar guards on the bite-zone overlap and attack montage's
OnNotifyBegin. Connected clients refuse these bodies only for live host replicas,
preventing independently started attack impulses and bite damage to int_objects.
Regression covers adopted/held identity, dead/recycled actors, descendants,
personal boars, host/disconnect fallthrough and watch-registration retry.
Extracted 0.9.0n bytecode confirms both guarded paths and their side effects.

Validation: package/check_2152.ps1 PASS all 9 runners, including the expanded
grayboar executable and nine production-unit compilations. Historical runner/log
names remain unchanged; current source/DLL/protocol stays 2153. No ZIP or new
version created. Two-player gameplay, shared montage/audio and mid-action join
remain unverified/open. Existing packaged DLL/ZIP does not contain this change.

## Package 0.9.2152 prepared (19 October 2026)

Owner requested release 2152 and an updated Thunderstore changelog. Built the current
MSVC x64 Release target, reran all nine focused runners successfully, and packaged
workspace dist/WatchersOfTheVoid-0.9.2152.zip. CHANGELOG_PLUS.md, package README,
RELEASE_2152.md and publication instructions now describe the current wisp, NPC
orientation/spawn and possessed-boar fixes, plus the unfinished behavior.

Package version 0.9.2152 intentionally contains DLL 0.9.0n b2153 / protocol 2153;
the internal identity preserves the incompatible 52-byte NPC pose layout. Packaging
and verification scripts explicitly check that identity. Archive structure, manifest,
changelog heading, 256x256 PNG icon, embedded DLL hash, loader imports/exports,
Release configuration, all 752 production source entries and delivery copy passed.
Evidence: build/release-2152-logs/verification.json and tests.json.

ZIP SHA256: 11632e4c18ee0c3aedfefd73ba9ed0cc85839b2ba923601f2d3a3a282794df02
DLL SHA256: 26e0c27442c650b0c5dd30869194584a90d78608913045497d7b8b756ecc041d
SHA256SUMS-2152.txt accompanies the ZIP. No gameplay verification, game installation,
commit, push or online publication performed; all existing open reports remain open.

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

Full requested taxonomy and unresolved behavior coverage: WORK.md, "Complete requested entity coverage and remaining work".

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


Additional offline result: actual wisp montage wrapper regression passes refused,
invalid or unreadable playback results, failed parameters/dispatch, callback recycling
of actor/AnimInstance, recovery and optional-section absence. Receiver regression also
passes; all three related production units compile via run_wisp_animation.ps1.
The native call's dispatch status is no longer confused with accepted playback.
The player-reported wisp animation still needs host/client gameplay confirmation.

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


Updated 18 October 2026. All accumulated source work through development batch 2192
is retained in release/protocol 2151 at the owner's request. Historical numbers
below identify work batches and prior observations. The complete open backlog is
[WORK.md](WORK.md); packaging does not close any unverified gameplay report.

Read [TODO.md](TODO.md) for current priorities and release restrictions. This file
combines current issue statuses with the original user report snapshot. Implemented
or locally checked does not mean multiplayer verified. Historical summaries are
removed with historical archives during the owner-approved cleanup.

| # | TestLog item | Current status |
|---|---|---|
|1|Client cannot break props|Код готов2143, дополнительная защита2159: FIFO повторов ударов и полная проверка границ параметров; needs new-build gameplay result|
|2|Prop jitter/rollback|2153 item batch: lowest-slot hold arbitration includes the local handle/physgun/gravity target; refused holds cannot apply a late release, and cached drives validate actor slot serials. Native yield and motion regressions pass. Initial grab remains optimistic until peer poses arrive; multiplayer jitter verification remains open.|
|3|Pushing one prop with another|2153 item batch: verified hitCpp feeder resolves nested component owners (bounded 64 hops), preserves deferred chained push/coast and excludes static/frozen/held/trash targets. Contact and host/receiver regressions pass; real PhysX contacts and two-player convergence remain unverified.|
|4|Client R+LMB duplication|Код готов2143; needs gameplay result|
|5|Selected-model ragdoll|Reopened: owner still reports incorrect ragdoll on2168 (report50). Preserve2144 code, but do not treat this behavior as fixed.|
|6|Mining farm|Reported working|
|7|Extinguisher foam|Код готов2145|
|8|Loose power outlet floating|Код готов2145|
|9|Physics gun / both gun effects|2153 item batch: physgun target/component identity checked; dedicated ATV/hinge channels can see non-prop targets without casting them as props. Native weapon-release cleanup, remote beam/audio effects and host-validated live root scaling implemented/tested. New kind 202 requires matching current DLLs. Skeletal bone scaling/arbitrary creature physics are not implemented by the prop lane; actual freeze/scale/contested-control gameplay remains open.|
|10|Nightmares|Previously reported working. 2151 requested behavior: only the player whose native sleep/enabled/bed/chance conditions win enters a nightmare. Shared acceleration ends via SleepState; host scene is no longer forced on other peers. Native checks/regressions/compile pass; gameplay pending.|
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
|25|Battery levels|2153 item batch: personal profiles capture the live held record before hashing (including zero charge) with class/key/serial/world validation. Supported mutable world pickup waits for reliable record acceptance before destroying its source. Capture/transition regressions and Release compile pass; battery hand/stow/drop/rejoin gameplay remains open.|
|26|Airy event|Осталось|
|27|Main-room computer walls becoming physics props|2153 item batch: seven factory-born cubicle parts and their post-finish state/coast are covered by the production birth harness; source registration remains independent of optional ambient assets. Actual struck-panel disappearance on host/client remains a gameplay check, not a closed report.|
|28|Drone sound difference|Код готов2143|
|29|Crowbar static state / dragging|2153 item batch: unstick preserves live local handle/gravity/hotbar ownership even without a localPlayer argument. Native refusal, non-component static flags, retry, recycle/world and callback tests pass. Unknown parent/replacement subclasses still require individual native/gameplay evidence.|
|30|Second ATV in corridor|Код готов2149 for new companion creation; old saved positions unchanged|
|31|Sleeping body pose|Source implemented in continued 2153: selected-model poseable body, mattress/moving-anchor and sleep/ragdoll/sit precedence; regression checks pass. Verify both peers, effects suppression and each rig in game.|
|32|Not enough space on equipment|Код готов2146 for ALL equippable items|
|33|Open container retains removed row|Код готов2146|
|34|Client crate/package loot invisible|2153 item batch: inherited MRE/AB route audited; exact personal foodBox final-bite wrapper birth admitted, while world wrapper output is host-owned. Final food destruction cannot overtake its bite request; post-finish host birth/state tests pass. Other unexamined package families and two-player loot appearance remain open.|
|35|Centipede / other living creatures invisible|Осталось|
|36|Computer chair rotation (duplicate report)|Same as11,2144|
|37|Riding a carried ATV / flight|Код готов2148/2149; actual menu and race convergence included|
|38|Skin model list / previews|Частично2151/2152: original sixteen preview tiles plus supplied Mynet/skeleton images; both requested meshes were already builtins. Catalog regression passed; runtime appearance/animation/ragdoll still needs gameplay, other preview gaps remain|
|39|Consumables refill on pickup/drop|2153 item batch: current held native record is captured for profiles, supported world pickup waits for record enqueue, and host food samples survive a missing world actor. Final-bite identity is captured before destruction with durable send retry; verified box output is host-owned and eater effects stay native/local. Food/consumable transition regressions pass; gameplay remains open.|
|40|Flesh Eerie summon / visibility / aggro|Осталось|
|41|Secret basement wall|Исключено по прямой просьбе пользователя; не возвращаться к исправлению|
|42|Event teleport to the triggering/base-entering player|Код2161: actual agrav occupancy and runtime-verified shared armed boxes; client pose fallback, initiator gather, repeat/reentry guards and send retries. Two focused checks/five TUs passed; multiplayer pending. Other custom paths and full event effects remain separate|
|43|Safe cracking|Код2162: both dial success and hacked converge on opened; client completion to host, single loot, saved lock publication/retries and host-lock protection against early/stale records. Initial unlocked drop record preserved. Gameplay pending; dial UI and generic child hinge identity unchanged|
|44|Sponge wetting|Частично2160/2169: native saved power/wetness now publishes after enterWater/wash/clamp/LMB; load no echo and final-use tail checked. 2169 adds current held wetness to outgoing profiles. Soap stock/foam/surface effects and personal-hand transitions remain unverified; two-player test needed|
|45|Grill ignition|Код2162: host ignition/off, active/coals/burn snapshot and late join, passive client heating/fuel tick, immediate coal record and request retries. Gameplay pending. Concurrent coal transactions/stale records, lid and personal item transitions remain.|
|46|Client-placed crops/faucets/hoses disappear after save reload|Merged source now includes pre-session saved-actor enrollment, care/harvest/pot state, accepted hose rerouting and canonical updates; garden/hose regressions pass. Verify actual save/reload, late join and simultaneous use. Missing historical actors are not migrated.|

## Additional user reports (15 October2026; OLD released versions)

|ID|Report|Status|
|---|---|---|
|47|Black disk/yellow-ring sky event starts on client independently|Visual code2164; common schedule-entry authority2165 covers all scheduled rows, boot/reflected calls and passEvents protection. Local checks pass; gameplay pending. Audio/emails/pre-session latent effects remain separate. This is NOT Bad Sun or black fog|
|48|Mimic (user identified; trees/white pillars/radar report)|Code2165: prop_impostor_C and impostorPropsMaster_C identified; client random generation and local-camera culling disabled. Reuses keyed prop class/disguise/birth/death/join; host controls lifetime. Focused checks/compilation pass; multiplayer/rendering/damage verification pending. Related to35; other creatures remain open|

## Additional user reports — 16 October 2026

Owner confirmed all ten reports were observed on build2168. They remain open current observations against2168; earlier related code is not gameplay confirmation. This workspace has not produced or inspected that2168 package, so its last locally verified package remains2146. Do not dismiss the reports as pre-fix behavior.

|ID|Report|Status|
|---|---|---|
|49|Insomnia demon neither appears for nor attacks the client|2151 code ready: native peer-local sleep-dependent spawn/pursuit/grab restored; removed ticker refusal and pose-only NPC mirroring. Native bytecode check and focused C++ compilation pass. Requires host/client gameplay: low/rested sleep, grab/dream and reconnect. Distinct from nightmares10 and mimic48.|
|50|Ragdoll display is still incorrect|2151 partial: spawn/lost-body retry, pelvis correction and model-authored physics selection added. Valid own rigs may use a different skeleton; common fallback retained, bone/pelvis/joint and stale-asset guards tested. Focused regressions and compile pass. T-pose in gameplay, local native body and exact limbs remain open; revisit5/38.|
|51|Client cannot plug electrical appliances into outlets|Partial2170: socket-endpoint authority fixes refusal at the far end of long cables; native length/occupancy and callback lifetime guarded. Client connect/graph parameter-frame bounds now fail closed. Two focused executables, native-bytecode check and two TUs pass. Report remains open for actual2168 symptom/gameplay verification; nearby failures may have another cause. Distinct from loose-outlet floating8.|
|52|Client token display remains desynchronized|Open: owner still cannot identify the token counter/UI (21 October). Do not assume credits or a specific token class.|
|53|Base light flickering is not synchronized|Partial2177–2180: host-authored ceiling/ambient phases; indexed clients suppress native writes; local ambient settings remain local. Failed applies retain latest state;2180 retries failed/unreadable join snapshots without advancing the all-peer baseline, with peer/world cleanup. Channel/wrapper regressions, prior native checks and five production TUs pass. Actual keys, loading/late join and two-player timing need gameplay verification.|
|54|Computer display does not show the same activity to host/client|SharedScreen and merged LaptopUI/operator code is present; wire/lifecycle/holder/widget regressions pass. Verify actual laptop/portable-PC tabs, text, scrolling, cursor, activity and operator visibility in game. Console text12 alone is insufficient.|
|55|Computer power-on and power-off are not synchronized|Partial2173: targets survive actor loading and native animation; failed sends and deferred join snapshots retry with latest state. Pending sends clear on peer departure/disconnect; 2171 echo suppression is preserved. Existing animation regression, new delivery/lifecycle regression and two production TUs pass. Still requires two-player checks, power loss/restoration and late join; no release built. Separate from water short-circuit/damage18/19.|
|56|ZIP disk model sometimes becomes a huge owo when ejected|Partial2172: laptop `zip` is now resolved as a masked Unreal bool bitfield, written without disturbing neighboring flags, and hashed as its logical value. Focused wrapper regression and production TU compile pass. This is a concrete wrapper defect but is not proven to cause the reported model/eject symptom; still needs in-game ZIP eject and multiplayer checks.|
|57|Opening canned food and eating it are not synchronized|Partial2176: world open/eat publish records (2174), held can state travels with HandItem (2175), and client profiles refresh live opened/type/foods before hashing (2176). Identity-checked copied records preserve unrelated data. Three profile/record regressions, native evidence and two production TUs pass. Protocol2175. World/held use, reconnect, final portion, stow/re-equip, drop/pickup and simultaneous consumption still need multiplayer verification.|
|58|Some items disappear when placed on the workbench|Open symptom; partial code2181–2186. Owner clarified17 October: simple placement, visible disappearance after1–2 seconds or later; no crafting reported. Owner identified an old rifle and components in an isolated case on21 October; exact component and authoring role remain unknown. Native rifle candidate is prop_rifle_C. Transform retries, local-birth protection and deletion identity rechecks implemented;2186 invalidates old sweep passes on bracket/world/reset changes and blocks recursive reconcile ticks. Focused tests/production compilation pass. Full sweep/gameplay symptom remains unverified.|

Related deletion-path fix2187: pile watches/pending state are now cleared only at final deletion, not provisional sweep selection. This prevents losing state for candidates later kept by completeness checks. Production compile and existing candidate regressions pass; no full pile/sweep gameplay test. This does not close report58.
Related record-delivery fixes2188–2192: failed captures retry; unindexed newborns retain validated source identity; retries are per recipient/broadcast; loading waits preserve attempts; successful ordinary sends now cancel only their satisfied older retry. Real record-queue regressions and production compile pass; not a proven cause/fix of report58.

Preserve this TestLog with the source changes. Do not mark remaining rows done solely
because a related older patch exists or a standalone compile passes.

## Original user report snapshot

Source: `E:/TestLog(eng).md`, read on 14 October 2026. Original report wording is
preserved below as evidence; it is not proof that an issue remains unfixed today.
The first 40 reports map to tracker IDs 1–40 above, in order.

The client cannot break props; hits register endlessly without the objects breaking on client side, on host's side hits do not register. (appeared in 2142RBE)

Props move erratically for the client; they seem to teleport back briefly, resembling a high-ping issue, and movement is delayed.

Inconsistencies occur when moving one object using another.

When a player throws an object using R+LMB, the object may duplicate (e.g., the signal box lid duplicates). (appeared in 2142RBE)

Ragdoll physics interact poorly with the model, though the model itself is visible; the lack of model "bones" causes the T-pose to drag along the floor during ragdoll states.

The mining farm functions correctly.

The fire extinguisher emits foam only for the user, yet the supply depletes for everyone.

If another player moves an unconnected power outlet, it remains floating in mid-air for everyone else.

The physics gun is unsynchronized while the gravity gun is synchronized; neither displays visual effects when viewed from another player's perspective.

Nightmares function correctly.

The chair rotates only for the player sitting in it; for others, the player spins in a seated pose while the chair itself remains stationary.

Console commands entered by one player do not synchronize across other players' consoles.

When the client entered a "funny rule" command, the music played only for the host; nothing happened for the client.

The "funny rule" display appears only for the host.

When using sound-emitting toys, the sound plays only for the user.

Micro-stutters still occur when putting an item away.

The stream from the pressure washer is visible only to the person using it.

Sparks and signs of the main computer breaking appear only for the person damaging it (sparks were seen only by the person spraying water at the computer).

The main computer doesn't shut down until the host breaks it.

Buckets are buggy; water disappears from the bucket if someone other than the host pours it in.

Cement is consumed from the bucket, but nothing appears on the trowel; it only appears for the person applying the cement to the trowel.

The outline for a brick wall is visible only to the person who placed it, and the wall itself is visible only to the person who built it.

When a friend's flashlight runs out of power, the host still sees it working.

When a grappling hook is attached to a wall, the friend cannot pick it up, but the host can.

Battery levels aren't synchronized; for example, the host might see 0.0% while the friend sees 100%, but once the friend touches the battery, it jumps to 100% for the host as well.

The Airy toy event isn't synchronized for the friend; while the host sees the animation and hears the sounds, the friend sees nothing—the only thing that appeared during the event was a trash bag, and the Airy toy spawned for the friend only after some time had passed.

computer walls in the main room after changing into the physic objects are disapearing from on client side.

sounds of the drone are different on client side. (appeared in 2142RBE)

Static objects pulled out with a crowbar do not synchronize; their static state changes only for the player who pulled them out, while their position updates via teleportation rather than smooth movement when dragged.

The second ATV spawns not in the garage, but in the room with the portrait of Stolas (the main corridor).

What if we tried adding a sleeping animation? I mean, flipping that white model over and placing it on the mattress.

There’s a fanny pack that increases inventory space. I bought two of them, but when my friend tries to equip one, he gets an "not enough space" message, same with backpacks, might be with other wearables.

Basically, when we Both open the same container and one of us takes an item out, the item remains visible to the other person—but if they try to take it, the game says there isn't enough space.

If my friend (not the host) breaks a wooden box / opens a package with food, I won’t be able to see the items falling out of them until he touches them.

When a centipede approached us, I could see it, but my friend couldn’t., its also a problem between all kinds of "alive" creatures.

If I or my friend sits on a chair near the computer (black, on which you can spin) That is, if it starts spinning, it won't be visible.

If I get on the ATV that my friend is holding, I’ll start moving through the air, and as soon as I get off, the ATV will be in his hands.

Skin menu has not every creature models init and has no preview image on it (i have images so when we start fixing it, ill send you them).

As far as we can tell, all interactable items—such as batteries, lighters, food (which you’ve already fixed), and fire extinguishers—aren't synchronizing correctly. If a friend picks one up and then puts it back down, it becomes full again, just like what used to happen with the food.

Regarding the Flesh Eerie: if a friend lights the summoning item, the boss doesn't appear for them—there are no sounds and no boss at all, except for the rockets hitting the Eerie; plus, the boss only aggros onto the host.

## Fork review ? 19 October 2026

Release 2153 build PASS; check_2152.ps1 PASS (9 runners); disk_utf8_test.py PASS (production Windows conversion and 16/32-bit string framing). git diff --check PASS. No gameplay run: client hashcodes, disk transfer, rack persistence and slow join remain unverified. Distribution ZIP unchanged. Sources/selection rationale: WORK.md, Fork and upstream review.

## Artifact cleanup verification - 19 October 2026

Approved inventory removed: 10,030 files / 885,751,603 bytes (844.7 MiB gross).
Current package SHA-256 unchanged; current DLL and dependency cache present.
Post-cleanup check_2152.ps1 PASS, all nine runners. Running them regenerates their
current test outputs. git diff --check PASS. No game runtime changes in cleanup.
Extra historical run_2141.ps1 check cannot compile: its fire_stubs Session lacks
SetLocalPropPose used by current cord_sync.cpp. This is an existing stale test API,
not a deleted input. Historical source-baseline comparison was removed; current
performance cases and mutation controls remain pending stub maintenance.

## Item batch validation - continued 2153

The eight selected reports have source fixes/regression coverage recorded in WORK.md,
Item synchronization batch. run_item_batch.ps1 PASS; all nine check_2152 runners PASS;
Release b2153, zero-UE4SS-import ABI and reliable-kind routing PASS. New harnesses
exercise live held records, native yield boundaries, scale validation/retry/reentry
and existing host/receiver motion. Final-bite tests cover native destruction before
POST, suppression of generic destroy, refused-send retry and no duplicate request.
Native evidence verifies the foodBox output has no player effects and the physgun
release input reaches real cleanup (its release() event alone is empty).
No two-player gameplay result or new distribution ZIP. New kind 202 requires identical
current DLLs despite retaining development protocol 2153. See build/item-batch-check/verification.json.

## Shared computer, ragdoll and sleeping body - 19 October 2026

Release `main.dll` b2153 built successfully after the changes. Focused regressions
passed: `run_shared_screen.ps1` (wire bounds/Unicode, holder trust, handoff, late join
and display retry), `run_ragdoll_retry.ps1`, `run_ragdoll_skin.ps1` and
`run_sleep_personal.ps1`. `git diff --check` passed for the changed paths. These tests
do not replace a two-peer in-game check of laptop/portable-PC UI, selected-rig ragdoll
entry/recovery, and bed/ragdoll/chair transitions. Distribution ZIP unchanged.
