# Regression tests

Run scripts from the canonical checkout. Numbered filenames identify the development
batch that introduced a regression, not the currently packaged build; do not remove
tests solely because their number is older.

Current focused suite: `../../package/check_2152.ps1`. It invokes the player-report,
wisp animation, ragdoll retry, ragdoll skin and personal-sleep runners, entity orientation, plus the three
native-evidence Python checks. Native checks require the local research corpus.
The C++ runners require the local MSVC toolchain; inspect each runner for its setup.

Keep the numbered runners, C++/Python sources and stub directories together: older
runners contain distinct regression and negative-control cases. Generated binaries
belong in build/ or _research/, never beside the test sources.

Cleanup on 18 October 2026 removed generated Python bytecode. Historical archive
duplicates are recorded in ../archive/cleanup-2026-10-18.json.
