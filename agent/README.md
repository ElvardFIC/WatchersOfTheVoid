# Agent context

Owner instruction, 27 October: development now targets **0.9.2162**. The last
qualified package is **0.9.2162 / b2217** (offline). Do not increment
the public version for each continuation. Keep internal build/protocol identity
separate, and change it only when required/explicitly requested, not per batch.
Name every planned fix and every completed fix individually. Earlier 2162
labels predate this instruction; retain their evidence as historical evidence.

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
