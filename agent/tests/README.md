# Regression tests

Current full audit: run `powershell -NoProfile -ExecutionPolicy Bypass -File
package/check_current.ps1` from the checkout after source edits finish.
It runs 63 groups, completes the incremental Release build and verifies that the
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
public-prose include-context tests are included. Matching b2176 peers are required.

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
