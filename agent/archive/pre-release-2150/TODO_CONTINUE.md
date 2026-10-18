# Current handoff

Updated 17 October 2026. Replace this current snapshot; do not prepend history.

## State

- Source protocol2178 (latest work2192); last locally verified package2146. The owner observed reports
  49-58 on build2168; that package has not been inspected here. Keep those reports open
  unless their exact symptoms are verified. Ragdoll5 is reopened.
- Laptop power targets survive loading/animation; failed sends/join snapshots retry latest state.
  TestLog55 partial: two regressions and two production TUs pass; no gameplay/package test.
  [WORK_2173](WORK_2173.md) extends [WORK_2171](WORK_2171.md); peer departure clears pending sends.
- ZIP bool uses its Unreal bit mask, preserving adjacent flags; its occupancy digest tracks only
  logical `zip`. TestLog56 remains partial: regression and production compile pass, but the wrapper
  defect is not proven to cause the eject symptom. No in-game check. [WORK_2172](WORK_2172.md).
- World/held cans publish state; client profiles now refresh live held-can fields before hashing.
  TestLog57 partial: regressions, native evidence and production compile pass; no multiplayer check.
  [WORK_2176](WORK_2176.md) extends [WORK_2175](WORK_2175.md) and [WORK_2174](WORK_2174.md).
- Cord endpoint authority, native cable-length/occupancy checks, callback guards and bounded
  reflected reads remain partial for TestLog51. Focused tests, native-bytecode check and two TUs
  pass; no two-player verification. [WORK_2170](WORK_2170.md).
- Earlier prerequisites: wall construction/enrollment/client melee damage and live
  material/profile ordering fixes are implemented. Client wall repair and atomic
  cement accounting remain unfinished. Preserve the no-spend repair refusal.
- TestLog53 partial: phases sync; failed applies and join sends retain latest state without corrupting
  broadcast dedup. Channel/wrapper tests and five TUs pass. [WORK_2180](WORK_2180.md); native evidence2178.

## Next actions

1. TestLog55: verify host/client ON/OFF during animation, power loss/restoration,
   reconnect and late join. The offline harness is not gameplay confirmation.
2. TestLog56: verify floppy/ZIP insertion and ejection in game: class, mesh/transform, returned
   item and both peers.
3. TestLog51: run actual client plug/unplug checks on short/long cables, extension sockets
   and powered/unpowered outlets. If nearby connections still fail, trace hooks and endpoint
   resolution rather than increasing reach.
4. For wall repair, use [WORK_2170](WORK_2170.md)'s updateHold/checkEquip evidence and
   [WORK_2169](WORK_2169.md)'s profile prerequisites. Native fix changes health before
   its caller spends cement: coordinate request/result, exact item stock and hand/drop
   transitions before enabling client repairs. Remote hand mirrors are display-only.
5. TestLog58: simple bench placement can disappear after1–2 seconds, not necessarily crafting.
   2182–2192 protect births/save retries and cancel satisfied retries; exact symptom unverified.
   Gameplay queues for lights/cans and other reports: [TESTLOG_STATUS.md](TESTLOG_STATUS.md).

## Scope and verification

Basement-wall41 is excluded. Release packaging is deferred until the agreed TestLog
work and review of all accumulated changes since the last verified package. Commit,
push, installation and publication require explicit authorization. Preserve dirty and
untracked work. Usage/reset percentages in historical notes are not current limits.

Run the affected WORK note's checks from the checkout. Keep this sole active handoff
under 60 lines; replace it as work advances. Update tracker rows and one relevant
WORK note. Preserve dirty/untracked work. Report investigation, focused checks,
production compilation, release build and multiplayer verification separately; mock
tests and native evidence do not establish two-player behavior.

Edit the checkout's agent/ only. Run `./agent/sync_mirrors.ps1`, then `-Check` for mirrors.
Read former snapshots in agent/archive/ only for a specific unresolved question.
