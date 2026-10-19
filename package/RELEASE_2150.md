# Watchers of the Void 0.9.2150 — ElvardFIC

Thunderstore package name: WatchersOfTheVoid. Publish under the ElvardFIC team/namespace.
This is a community fork of Multivoid by Pelmentor; original license notices are retained.
Branding and supplied-cover update prepared before publication; DLL rebuilt and ZIP reverified.

Prepared 18 October 2026 for Voices of the Void 0.9.0n at the owner's request.
Contains the entire current source tree, including work batches through WORK_2192
and the ragdoll animation-mode correction found during release regression checks.
Those larger work numbers are historical development identifiers. Protocol, DLL
resource, in-game version and package are consolidated to 2150. All peers must use
this exact package; an old development snapshot numbered 2150 is incompatible.

## Verification

- Clean MSVC x64 Release compile/link of all 1,191 build steps, followed by rebuild
  of the final ragdoll correction. Every production .cpp is in sources.cmake.
- All 41 focused regression runners from 2143 through 2188 (including stow2157)
  pass against the accumulated source. The changed canned-record include was also
  rechecked through run_2176. Three outdated test fixtures gained missing mock APIs;
  their behavioral assertions were retained. The ragdoll test exposed a real mode
  regression: AnimationBlueprint (0) was corrected to AnimationSingleNode (1).
- DLL import inspection: no UE4SS imports, no external MSVC runtime dependency.
  start_mod and uninstall_mod exports present. Version resource: 0.9.0n b2150.
- pack.ps1 checks required ZIP entries, manifest, changelog and the archived DLL's
  SHA-256 against the newly built binary. SHA256SUMS.txt accompanies the final ZIP.
- No two-player gameplay, game installation or online publication was performed.

Logs and per-run exit codes: build/release-2150-logs/ in the source workspace.
Build: powershell -NoProfile -ExecutionPolicy Bypass -File package/build_2150.ps1
Package: powershell -NoProfile -ExecutionPolicy Bypass -File package/pack.ps1
The build script uses the existing portable toolchain and cached vcpkg dependencies.

## Remaining work

The complete consolidated queue is agent/TODO_2150.md in the source workspace:
55 open tracker rows, including duplicates and implemented changes awaiting gameplay.
Priority reports remain insomnia demon, ragdoll appearance, client outlet connections,
token display, base flicker, shared computer screen, computer power, ZIP ejection,
canned food and workbench disappearance. Related patches do not close those reports.
Client wall repair, atomic materials, other creatures/events, sleeping pose and other
partial systems also remain. Mining/nightmares were reported working; the old
basement-wall report stays excluded at the owner's request.

Old TODO originals are preserved under agent/archive/pre-release-2150/. Research,
tests, WORK notes, old releases and previous DLLs are retained. Cleanup only removes
reproducible build/test products and obsolete staging; archive/cleanup-2150.json
records actions in the outer workspace. Source changes remain uncommitted.
