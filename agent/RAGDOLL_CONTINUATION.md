# Resume ragdoll momentum work — 22 October 2026

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

## Completed b2182 /2159 — latest status, supersedes historical work below

User's second/third items completed for the controlled acceptance scope: both
reproduced limb-stream losses fixed, native contact24/24,installed catalog30/30,
remote sleep/sit/ragdoll transitions36/36,impaired LAN6/6,momentum8/8,camera10/10.
Final DLL b35471ad15d0f417b26439f51c9b64e2f034987de7d2e51e3ba031ea54cbb5a1,
2644-entry frozen source,all three runtime DLLs identical. Verification:
build/ragdoll-robustness-check/verification.json. No final test travel/GT faults.

Full archive dist/ElvardFIC-WatchersOfTheVoid-0.9.2159.zip qualified and actual
unpacked startup passes,archiveStartupVerified=true. ZIP SHA256:
bb85eaff437b031ed101c88702d8b5c8ea676bb258d77e9a45b845aaea49e42b.
All private tests stopped. No commit/push/publication/normal profile installation.
Do not repeat these checks without new source changes or a new reported concern.

Owner explicitly handles normal rendered/input acceptance. Native local bed/chair
input remains in that manual scope; automated transitions checked actual remote
components,seated pose flag and cleanup. Uninstalled imported rigs,other terrain/
vehicles and stronger outages are not claimed. Current source/protocol2182 and
prepared complete package2159; use identical package on all peers.
Next incorporate owner feedback; general non-Entity TODO retains its priorities.
The optional cooked-asset/construction redesign is assessed,not implemented or
required for this completed network fix. Intermediate records below are historical.

## Active task b2182 — supersedes the b2181 completion below

Owner will check ordinary rendered gameplay. Current request: fix intermittent
partial limb transfer, check slopes/moving surfaces/latency/installed models and
sleep/sit transitions. Owner confirms no additional imported models; use installed
catalog only. No subagents authorized. No Entity edits/publication/normal profile
installation requested.

Implemented two reproduced stream fixes in ragdoll_pose_buffer.h, outbound/remote
streams, Session, local_streams and puppet_drive: rotate7 bodies per actual send
(phase shift for periodic loss), publish full rig, merge newest received per bone,
bounded64 bodies/10 game-thread drain chunks, seq/rig/reset guards. Wire230bytes.
Protocol2182, intended package2159, metadata release.json points to new evidence.
Original momentum/camera/anti-launch code unchanged this turn.

Focused stream/native/skin/retry/sleep/display/wire/shared-screen/routing/ABI pass.
Old batching and latest-packet inbox controls reproduce missing limbs. Candidate
native contact24/24 and remote transitions6/6 pass; candidate impaired LAN3rounds
each direction passes. Build helper . ./build/local-msvc.ps1 then cmake --build
build/ragdoll-release --parallel4. Tests run_ragdoll_stream.ps1 -FaultControls,
run_ragdoll_native.ps1 -CompileSources -FaultControls, skin/retry plus manual
shared-screen compiles (old shared runner unconditionally sources missing ../_toolchain).

Current frozen-source DLL SHA256:
b35471ad15d0f417b26439f51c9b64e2f034987de7d2e51e3ba031ea54cbb5a1.
Source snapshot build/ragdoll-robustness-check/source-before.json. Final native
processes RUNNING; use existing PIDfiles under build/ragdoll-runtime, never trust
an old PID without exact canonical private exe path check. Logs in private Bin.
Solo runs VOTVCOOP_RAGDOLL_ALL_ACCEPTANCE=1: momentum8,camera10,surfaces24,catalog30,
remote transitions36. TEST_SKINS env comes from catalog-names.json (30 names).
Peers repeat3rounds each direction with fake_link wr+10:lag=250&jitter=40&loss_send=5.
At19:09 final solo momentum8 passed; camera/surfaces/catalog still running. Peers
in round1. All must be read after completion, no assumption of success.

Important fixture corrections: raw move across a newly created platform left
native lastLoc behind and triggered the native wall ray -> sl_backroomsObsolete.
Use the game's teleportWObackrooms helper (Transform.Scale is velocity, set0).
Block enum2 and Movable enum2 required on test cube. Cube destruction also needs
150ms to remove its PhysX shape before the return teleport. Catalog now tests the
30 actual requested meshes on native ground, separate from artificial surfaces;
an earlier catalog run crossed a pending fixture and travelled, retained as
catalog-fixture-interrupted.log. Do NOT weaken acceptance or disable native wall
detection. Surface/camera/etc final logs must have no travel/posted-task faults.
Native audit evidence under robustness-check/assets and native-gamemode-graph.txt.

Once final solo complete: stop exact private exe, preserve full log as
surfaces-final.log, camera-final.log and momentum-final.log. Stop each peer only
after3 strict PASS verdicts, save peer-{role}-final.log. Run
build/ragdoll-robustness-check/finalize-evidence.ps1. Script requires surface24,
catalog30,36 remote transitions, six verified175cm platform movements, poor-link
6LAN verdicts, matching DLLs and frozen sources. Review errors honestly; no fake
verification.json. ABI gate should be run again on final DLL.

After strict pass: update README/package docs/CHANGELOG to2159/2182, WORK/TODO/
TESTLOG and this handoff to complete; then package_current.ps1, isolated package
smoke, save startup/UE4SS logs, VerifyOnly archiveStartupVerified=true. Deliver
dist/ElvardFIC-WatchersOfTheVoid-0.9.2159.zip (32 originalidentity/icon/models entries).
Stop all private game instances and git diff --check. All ordinary profile files
unchanged. State limits: NullRHI, controlled surfaces/link, installed rigs only;
remote state transitions use actual engine components/pose flag, native local
bed/chair input remains part of owner's ordinary-game acceptance. Full65 historical.

## Final status — supersedes intermediate instructions below

Finished and locally delivered full 0.9.2158/b2181 package. Exact diagnostic DLL
below qualified: momentum8/8, camera10/10, bidirectional articulated LAN, frozen
2642-entry source, three identical runtime DLLs. Complete archive32members plus
actual unpacked startup pass, archiveStartupVerified=true. ZIP:
dist/ElvardFIC-WatchersOfTheVoid-0.9.2158.zip, SHA256
ba8277a78e51b03ac2cd9dd47707e36f2ed641525b1186a542f7d03d9f14b269.
All private game processes stopped. No commits/push/publication/profile changes.
Do not rerun completed qualification/packaging without a new source change.

Next "продолжай": first incorporate owner feedback on normal keyboard running,
camera and remote model. Remaining acceptance: rendered/input path, intermittent
partial LAN if reproducible, terrain/moving surfaces/latency/custom rigs and sleep/
chair transitions. The cleaner construction-time compatible PhysicsAsset design
is assessed, not implemented. General non-Entity TODO resumes after ragdoll feedback.
The weekly indicator reached100% used at the final quota check (rounded account
value; ordinaryUsageAllowed was still true). Recheck on continuation.

User's "продолжай" means continue this exact task. Fix running inertia, preserve
selected-model/native ragdoll, camera and anti-launch fixes. Entity belongs to
the collaborator. Do not publish, install in the normal profile, or hand over an
unqualified DLL-only ZIP. Package identity ElvardFIC-WatchersOfTheVoid, credits
ElvardFIC/AquaFishhh. Latest prepared full archive is 0.9.2158/b2181.

## Current source

b2181/protocol2181; intended next package 0.9.2158. Main momentum implementation
in src/votv-coop/src/ue_wrap/engine/engine_playerragdoll.cpp is complete and passes
the actual substrate fixture. Native fallImpulse is latent Delay(0), then adds
fallVeloc to every named body. Rebuilding physics discarded it; old pelvis-only
restore sampled before it arrived. A scoped native pre-hook now holds exact
linear-velocity commands FIFO while bodies are absent, restores original motion
and common translation on new limbs, then replays commands once. Preserve absolute,
additive and NAME_None semantics, chest kick, cancellation/recycled-object guards.
Physics solver/lifecycle is still native. No artificial running kick.

Design audit: RAGDOLL_DESIGN.md. Stock plush has 5 bodies/4 joints; selected full
rig has 17/16. Best future simplification is a compatible cooked player PhysicsAsset
and construction-time native body selection before registration/simulation. No
alternative asset/retargeter has been implemented. Do not blindly replace the
full rig with the plush asset or modify shared defaults permanently.

## Evidence and open blocker

build/ragdoll-momentum-check/native-final.log and focused *-final.log pass; native
regression rejects disabled velocity hold and disabled camera correction. Routing
224 kinds/18 negative controls, ABI, skin/joint templates, retry, shared UI/wire,
sleep/display and sync checks passed. Full65 suite is historical b2178 only.

DLL 6116e6755237f01f9d227caa9335dfe6167780e756efd1f01d8c8a7868ebd293:
momentum-final.log PASS8/8 (two skins x walk300/run600/diagonal/airborne), camera-readiness.log
PASS10/10 (<0.1deg at swap). These are controlled native-entry stimuli, not keyboard
running acceptance. Optional held-forward/run probe produced zero movement under
NullRHI; cause unresolved. Recovery test now forceWakeup after letting body land;
forceGetUp was delayed/gated and contaminated the next case.

LAN partial-{host,client}.log: entry/recovery and selected mesh correct both ways,
but articulated display absent (model=1 streamed=0), no final PASS. Earlier test
aborted loading after one 8s GT task timeout. Readiness harness now continues until
overall deadline, uses separate per-task result storage, then requires12 stable
seconds. Screenshot framing disabled so observer does not teleport/fall/die.

Current diagnostic DLL SHA256:
3d77b13017ed02d4353ec19a0b7de47035663ab7ca041ea9ed8924c67131d3e2.
Only changes since 6116 DLL: existing limited local RagdollPose emission log gains
rig signature/body/batch counts; remote rig-change log prints sender/local signature.
Look for `rig=` / `RagdollDisplay: incoming` to distinguish absent pose reads,
signature incompatibility, batching or display creation failure. No diagnosis
has yet been proven. Do not claim LAN regression fixed or assert a cause.

## Running private tests / safety

PID files build/ragdoll-runtime/{solo,host,client}-pid.txt are authoritative.
Logs build/ragdoll-runtime/VotV/Binaries/Win64/ragdoll-{solo,host,client}.log.
All are isolated instances, NullRHI. Check exact process Path before stopping;
old PID may be reused by another executable. Current helpers:
build/run-ragdoll-game.ps1; build/run-ragdoll-peer.ps1 -Role host|client.
Peer mods: InertiaPeerMods-host/client, user dirs InertiaPeerUser-host/client.
Save names containing2179 are fixture names, not DLL versions. Public master
disabled via localhost:9; LAN localhost37829. Readiness/observer600s.
Runtime Engine/Content are junctions to original game: NEVER recursively delete
the runtime tree. Use Get-Content for live logs (ReadAllText share mode fails).
Every Start-Process must be hidden. Stop only exact private executable, wait exit.

Latest diagnostic DLL: momentum-final.log PASS8/8; peer-host-final.log and
peer-client-final.log PASS in both directions, with selected articulated display.
Sender/receiver signatures match (scientist4d1cd479, Kerfur00aa4522). No behavior
change explains the previous partial result; record it as an intermittent test
limitation, not a proven production fix. Peers are stopped. Solo now reruns camera10
on exact diagnostic DLL. ABI/native/remote fixtures are also rerunning (log files).
If ending the turn, stop private tests and
record latest result/next step here. Do not leave test processes consuming resources.

## Build and release after resolving LAN

PowerShell `. ./build/local-msvc.ps1`, then
`cmake --build build/ragdoll-release --parallel 4` (requires ordinary build approval).
Python bundled at C:/Users/zzYaRa/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe.
Focused native runner agent/tests/run_ragdoll_native.ps1 -CompileSources -FaultControls.
Current production snapshot source-before.json has2642 entries; regenerate hashes
after any production change, then freeze source through final native tests.
Current build diagnostic-build.log. No need to reconfigure CMake/dependencies.

Final evidence script build/ragdoll-momentum-check/finalize-evidence.ps1 checks
source/DLL identity, identical solo/peer DLL bytes, momentum-final.log8/8,
camera-final.log10/10, peer-host-final.log +peer-client-final.log strict PASS with
articulated display, focused checks, then writes verification.json. Never weaken
acceptance to PARTIAL. release.json points at this qualification and source snapshot;
packaging refuses incomplete evidence.

After strict pass update WORK/TODO/TESTLOG and README/package docs to prepared status,
run package/package_current.ps1, then build/package-current/run-package-smoke.ps1
with all previous private instances stopped. Verify current boot/version, skin4/4,
HEALTH pass, namespace/enabled marker in unpacked archive. Copy smoke logs to
build/package-current/startup.log and UE4SS-startup.log, rerun -VerifyOnly, require
archiveStartupVerified=true. Full archive must be
dist/ElvardFIC-WatchersOfTheVoid-0.9.2158.zip (32entries/originalicon/models).
Run git diff --check. No commit/push/public release requested.
