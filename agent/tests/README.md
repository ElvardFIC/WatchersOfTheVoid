# Regression tests

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
