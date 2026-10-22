# Regression tests

`ci_fingerprints_test.py` verifies the added game-function reference rows against
the saved native capture, including inherited resolution and absent declarations.
GitHub runs it beside the fact-row gate.

Current full audit: run `powershell -NoProfile -ExecutionPolicy Bypass -File
package/check_current.ps1` from the checkout after source edits finish.
It runs 65 groups, completes the incremental Release build and verifies that the
production inputs stayed unchanged, without repackaging. Results and
logs are in `build/todo-audit/`. `run_held_identity.ps1` compiles the actual held
broadcast seam; the older item/resource/lamp/laptop runners use the current typed
fact-row and save-codec APIs. The expected assertion failures in mutation logs
must produce the failure code required by their runner. Gameplay remains separate.

Run scripts from the canonical checkout. Numbered filenames identify the development
batch that introduced a regression, not the currently packaged build; do not remove
tests solely because their number is older.

`run_shared_screen.ps1` checks the shared UI wire format and operator/host authority,
handoff, send refusal, join/widget readiness, plus the production sleep/poseable
display's selected mesh/material/scale, mattress trace, moving anchor and teardown.
The ragdoll runners also cover animation warmup and articulated-bone validation,
batch completion/order and rig replacement. Gameplay with two peers remains required.
`run_ragdoll_skin.ps1` also runs the full native startup regression:
selected geometry before physics, preserved camera direction and delayed native
momentum commands, repeated entry/recovery and recycled-object guards.
Its skin wrapper checks component shutdown before mesh replacement and per-body
shutdown after the new physics asset is installed.
`run_ragdoll_native.ps1 -FaultControls` also verifies immediate native-construction
setup and the recovery camera handoff, including camera world position and yaw.
Removing either correction must fail its control. The opt-in installed-game
`VOTVCOOP_RAGDOLL_TRANSITION_TEST=1` matrix checks eight completed native wakeups,
delayed heading drift and subsequent controller turning on Scientist and Kerfur.
`ragdoll_transition_fingerprints_test.py` checks the two native camera references
against a real 0.9.0n capture, with two existing signatures as format controls.
It can regenerate those entries with `--capture <capture-file> --write`.
These harnesses do not simulate PhysX or close the all-role C-key launch report.
`run_shared_screen.ps1` also checks the production desktop traversal against an
old boot snapshot applied after native startup, repeated power pages, desktop
text/scroll/tab retries and missing widget dependencies. Its native test reads
the existing laptop corpus to verify rootswitch boot/desktop ownership; it does
not extract game assets or establish runtime host/client acceptance.

Current focused suite: `../../package/check_2152.ps1`. It invokes the player-report,
wisp animation, ragdoll retry, ragdoll skin and personal-sleep runners, entity orientation, plus the three
native-evidence Python checks. Native checks require the local research corpus.
The C++ runners require the local MSVC toolchain; inspect each runner for its setup.
The entity runner includes grayboar_actions_test.cpp, exercising the production
host transition observer, packet receiver and direct montage/audio path. Its test
double refuses unexpected reflected calls so gameplay-verb replay fails the test.

Keep the numbered runners, C++/Python sources and stub directories together: older
runners contain distinct regression and negative-control cases. Generated binaries
belong in build/ or _research/, never beside the test sources.

Historical archives, backup snapshots and generated test outputs were removed on
19 October 2026. Active regression sources and stubs remain. run_2141.ps1 tests the
current code and mutation controls without depending on an archived source copy.

`disk_utf8_test.py` requires Windows and cl on PATH (load the local toolchain environment first). It compiles production disk string functions and checks lossless wire framing for control characters and Unicode.

`run_item_batch.ps1` runs production harnesses for held-state capture, native local
hold yield, reliable root scale and host/receiver motion, seven existing item runners
and the cooked native-evidence check. Logs are in build/item-batch-check/.
These are offline checks; no PhysX or two-peer runtime is simulated.

## Fish integration

`package/check_current.ps1 -EvidenceDirectory build/fish-integration-check` runs
both main and fish regression groups, then verifies a stable production snapshot,
Release identity and loader ABI. The native physics fingerprint controls and
public-prose include-context tests are included. All peers need identical DLLs:
the final package 2155 contains current b2178, superseding the earlier local b2177 preparation.

Package 2155 qualification: `package/build_2155.ps1`, evidence in
`build/release-2155-logs/`. `run_events.ps1 -CompileSources` also compiles the five
event production TUs and runs replay/rate/active-lifetime harnesses. The new
`event_rate_native_test.py` decodes existing night-eyes bytecode; it does not extract
fresh game assets. `run_trowel.ps1` checks the production persistence wrapper and
the actual production integer-array helper/schema. `run_placement.ps1` includes
exhausted-retry recovery, sweep protection and bounded-pool cases.
It also runs `remote_destroy_lifetime_test.cpp` against the actual receiver and
local consume/convert cleanup, covering native replacement, recursive callbacks,
listener registration and unchanged load-tail behavior. `run_2161.ps1` now checks
session-serial reuse, native pose/bounds teardown and recursive location ticks.
b2178 qualification evidence is in `build/continuation-check/`; the owner's final
2155 release request promotes this qualified DLL into the package. Delivery hashes
are in `build/release-2155-logs/release-verification.json`.

Development b2179 extends `run_trowel.ps1` to held water/material persistence and
the production numeric-group commit. It checks all-or-nothing malformed/stale
refusal, zero/refill and unchanged native array ownership. `run_2153.ps1` also
checks local held water use/tick and cement/sponge/transfer persistence hooks.
Full development b2179 qualification passed 65/65 groups and the stable Release
build: `build/material-persistence-check/verification.json`. Package 2155 remains
b2178 and does not contain the material, C-key or PC boot corrections.

| Runner | Production behavior exercised |
|---|---|
| run_nonentity | Coin credit/retirement, placement replies, dry walls/material records, sky latent gates |
| run_cooking | Cooking progress, world/hand food readiness, profiles, replacement and output authority |
| run_drink_spills / run_gasoline / run_scorch | Exact launch provenance, host flight/fire, decals, quiet join and retirement |
| run_water_pour | Bucket/washer shots, source audio, projectile construction and host birth/coasting |
| run_grime_join / run_windows | Cleaning snapshots, nested hooks, latest final wipe and retained join delivery |
| run_kitchen / run_grill_hand / run_grill_edits | Lid/fuel/burn/audio, held state, saved runtime and ordered field edits |
| run_hand_tools / run_sponge_effects | Tool display lifetime, stop expiry, soap observation and cosmetic foam |
| run_toy_music / run_watoy / run_2157 | Maxwell phase/recovery, Watoy authority/motion and held toy audio |
| run_wall_creative / run_2166 | Creator/source/reach/rule validation, wall construction and immutable retry |
| run_item_records / run_2130 / run_2176 | Record ordering/reentrancy, native apply budgets, profile capture and stale sends |

The placement integration cases also enforce acceptance-only record publication,
refusal of existing same-key actors, cross-peer key reservations before indexing,
readiness, cooking bounds and malformed wire strings. `run_2130` expands local
production fragments before mutating them, so fault controls alter the live code.

`sync_native_contract_test.py ASSET_ROOT DECODER_ROOT` needs the branch's complete
extracted 0.9.0n corpus. This workstation lacks 18 of its 45 literal package inputs
(and the suite has additional dynamic inputs); it is not counted as passing.
The owner's fresh pak audit remains deferred. These offline tests do not simulate
PhysX or establish two-peer gameplay acceptance.
