# Agent context

Owner instruction, 15 November: prepare **0.9.2165**. The last
qualified package is **0.9.2163 / b2217** (offline), with 124 passing current
groups and a verified manager archive. Do not increment
the public version for each continuation. Keep internal build/protocol identity
separate, and change it only when required/explicitly requested, not per batch.
Name every planned fix and every completed fix individually. Earlier 2162
labels predate this instruction; retain their evidence as historical evidence.
Owner instruction, 6 November: finish tests and repository gates before Release
compilation; no parallel/preflight Release build. Any failed check blocks the
build, including full public-prose and native-function gates when reusing earlier
regressions. check_current -CheckGroups is test-only; -ChecksOnly runs the full
pre-build stage without a Release build. DLL/ABI and archive checks follow the
build because they inspect its artifacts.
Current source integration targets upstream `4fbf1acf0`. Earlier joint tower, radio and
optional EmoteWheel source, including native tower wrappers, physics-call and child-release guards,
prop-drive lifetime, failed-marker cleanup, swinger/cord and Vulkan context guards,
passed 142 offline groups across the full run and scoped recheck, the stable-source guard
and Release/ABI inspection at development protocol 2220. Exact source/build
evidence and the separate failing prose gate are in TESTLOG. UI01-UI09 changes
follow that build; their current verification and open implementation are recorded
separately in TESTLOG and WORK. The earlier DLL does not qualify those changes.
The retained 2163 archive identifies b2217. `release.json` now names the 2165
candidate (b2223); its Release DLL and package archive do not exist yet.
The earlier 4 November b2219 verification passed 134 groups and Release inspection;
its historical source stamp and gameplay limits are recorded in TESTLOG.md.

Before starting every new package version, clean the workspace with
`powershell -NoProfile -ExecutionPolicy Bypass -File package/cleanup_current.ps1 -WhatIf`,
review the selected paths, then run the same command without `-WhatIf`.
Remove generated artifacts, obsolete delivery files and redundant backups; preserve
active source/tests, build dependencies, the current qualified DLL/package, unique
research and verification/crash evidence. Record the result in the existing
`package/cleanup-inventory.json`. Do not create a backup tree before cleaning.
Reconcile TODO.md and WORK.md before implementation and update them as work lands.
Name the next development target separately from the last qualified release; bump
release.json only when preparing that version and record fresh verification.

The user leads the project. Assistant scope is bug fixes only; `another agent` is
retired. Historical archives and backup copies were removed at the owner's request.
When the owner says "продовжуй", handle 75 concrete fix items per pass, run the
relevant checks, and report implemented boundaries separately from game acceptance.
Before the next batch, reconcile TODO with completed work. Mark verified code
changes [x]; keep unfinished implementation and unperformed gameplay acceptance [ ].

Use the three active work documents: [TODO.md](TODO.md) for current priorities,
[WORK.md](WORK.md) for the unified backlog and implementation progress, and
[TESTLOG.md](TESTLOG.md) for user reports and verification status. Update them in
place; do not create numbered versions or parallel tracking files.

Completed per-change WORK reports have been removed; unresolved implementation and
gameplay follow-ups remain in WORK.md.

The Git checkout's agent/ is the only active copy. Run tests and tools from the
checkout; copied relative-path test fixtures are not a second independently runnable
test tree. Do not retain backup source trees or historical test outputs.
