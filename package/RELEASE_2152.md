> Historical release preparation. Current entity work uses protocol 2153 and a
> 52-byte EntityPose row. This document does not describe a ready 2153 package;
> pack/verify scripts still reject the new DLL until release preparation is updated.

# Watchers of the Void 0.9.2152 — ElvardFIC

Prepared 18 October 2026 for Voices of the Void Alpha 0.9.0n, Windows x64.
Community fork of Multivoid by Pelmentor; original license and third-party notices retained.
Protocol and DLL resource identify build 2152. Every peer must install this exact package.
Older builds, including 2151, are incompatible. Includes the accumulated 2151 fixes.

## Changes since 2151

- Retry wisp capture animation while its actor, AnimInstance or montage is not ready,
  at 100ms intervals within the original 3.5s window, on host and clients.
- Require accepted Montage_Play playback, rather than dispatch success alone.
  Guard subsequent animation calls against actor/AnimInstance recycling; an optional
  section-jump failure does not restart successful playback.
- Cancel a pending wisp death if release callbacks disconnect the session or replace
  the victim pawn. Consume hold state before native release to handle nested teardown.

## Verification

- Clean MSVC x64 Release build: package/build_2152.ps1, build/votv-coop-2152.
- Seven focused runners: package/check_2152.ps1 includes the previous player-report,
  ragdoll and sleep checks, native-game evidence, and the expanded wisp receiver,
  actual montage-wrapper and actual hold-release regressions.
- package/pack.ps1 checks icon, DLL version, archive identity, changelog heading,
  required files and embedded DLL SHA256.
- package/verify_2152.ps1 checks Release configuration, loader imports/exports,
  production-source inclusion, test results and delivery hashes.
- Evidence: build/release-2152-logs/ (build log, tests.json, source inventory,
  imports/exports and verification.json).
- Delivery: workspace dist/WatchersOfTheVoid-0.9.2152.zip and SHA256SUMS-2152.txt.

## Remaining verification

No two-player gameplay, FPS measurement, game installation or online publication was
performed. Verify host/client capture, late spawn/animation readiness, normal death,
disconnect during capture and pawn replacement. Offline checks do not close reports.

Missing models and mismatched static preview images from the build-2050 report still
need model names. Other remaining reports include shared computer display, token UI,
ZIP-disk ejection and disappearing workbench items. The complete source backlog is
in agent/TODO.md, WORK.md and TESTLOG.md.
