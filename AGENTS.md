# Project workflow

Read `agent/README.md`, `agent/TODO.md` and `agent/WORK.md` before continuing fixes.
Update the existing TODO/WORK documents as work lands; record verification in
`agent/TESTLOG.md`. Keep implemented code separate from unperformed gameplay checks.
For an owner's "продовжуй", handle 75 concrete fixes per continuation.
The active development target is **0.9.2162**, explicitly selected by the owner
on 27 October. The last qualified package is **0.9.2162 / b2217** (offline).
A continuation does not start a new
version, bump the public version/protocol, or prepare a new release automatically.
Change versions only when the owner explicitly requests it. Name every fix item
before implementation and list the completed items individually in the final report.

Before starting a new package version, preview `package/cleanup_current.ps1` with
`-WhatIf`, review its targets and then run it without `-WhatIf`, using PowerShell
with `-NoProfile -ExecutionPolicy Bypass`. Do this before bumping `release.json`,
while the current qualified archive and DLL still match that metadata.
Remove generated artifacts, obsolete files and redundant backups. Preserve active
source/tests, dependencies, the current qualified package and unique research,
verification and crash evidence. Use `package/cleanup-inventory.json` for the record.
Do not create backup source trees or numbered copies of tracking documents.

Use current build/package scripts and `package/release.json`. Historical test names
are not version selectors. Run relevant regression tests and qualify a changed
release before reporting it prepared. Commit, push, publish, install into the game
or deploy services only on an explicit request.
