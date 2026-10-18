# Historical material — read only when needed

Cleanup on 18 October 2026 removed 35 byte-identical duplicate files. The retained
paths and SHA-256 values are in cleanup-2026-10-18.json. Consult that mapping when
an older manifest names a removed mirror copy; unique historical material remains.

The active priorities are in ../TODO.md; the current TestLog is ../TESTLOG.md.
Material here preserves earlier instructions and evidence, not current policy. Paths in
old snapshots are relative to their former locations. Do not load this archive during
ordinary context recovery or mirror it back into the outer workspace.

- pipeline-2026-10-16/before/: byte-for-byte copies of eight documents before the
  pipeline cleanup. Includes the complete former continuation history and tracker.
- mirror-backups/ was audited and removed on 18 October 2026 at the owner's request.
  No separate production fixes needed merging. Canonical tests retain the newer
  coverage; useful unfinished investigation details were added to ../WORK.md.
  mirror-removal-2026-10-18.json records the removed inventory and audit results.

WORK_<revision>.md and prior TestLog snapshots are preserved here as historical evidence.
Older TODO_<revision>.md files were moved byte-for-byte to pre-release-2150/ when the
owner requested one release backlog; manifest.json records their hashes. Old relative
links inside historical notes refer to those original locations. Current open work is
in ../WORK.md. Future continuations update that list and the current handoff, without
duplicate TODOs or TestLogs.
