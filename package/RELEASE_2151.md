# Watchers of the Void 0.9.2151 — ElvardFIC

Prepared 18 October 2026 for Voices of the Void Alpha 0.9.0n, Windows x64.
Community fork of Multivoid by Pelmentor; original license and third-party notices retained.
Protocol and DLL resource identify build 2151. Every peer must install this exact package;
2050 and 2150 cannot join it. The changes are described in CHANGELOG.md.

## Verification

- Clean MSVC x64 Release build using package/build_2151.ps1, in build/votv-coop-2151.
- Six test runners cover 12 focused checks: seven player-report regressions, ragdoll
  retry/alignment, selected-model physics, personal sleep, and two cooked-game native
  evidence checks. Run package/check_2151.ps1 to reproduce with the local toolchain.
- package/pack.ps1 checks DLL version, icon dimensions/format, package identity,
  required archive entries, changelog version and embedded DLL SHA256.
- package/verify_2151.ps1 checks Release configuration, loader exports/imports,
  production source inclusion, runner results and the copied delivery hashes.
- Logs, test results, source hashes and verification.json: build/release-2151-logs/.
- Delivery: workspace dist/WatchersOfTheVoid-0.9.2151.zip and SHA256SUMS-2151.txt.

## Remaining verification

No two-player gameplay session, FPS measurement, game installation or online upload
was performed. Offline checks do not close the player reports. Test host/client role
reversal, late join/rejoin, wisp capture, ragdoll/get-up with different skins, simultaneous
Cremator use, both gun effects, and nightmares with only one eligible player.

The reports came from build 2050. Missing models and mismatched static preview images
were not identified by name; no claim that those unknown assets have been added or
their mappings corrected. Preview retry and nested-directory discovery are code fixes.
Other open issues remain in agent/TODO.md, WORK.md and TESTLOG.md in the source tree.

## Publication

Upload the prepared ZIP under ElvardFIC / WatchersOfTheVoid, version 0.9.2151.
The version description is the 0.9.2151 section of CHANGELOG.md; the package README
includes compatibility and experimental status. See package/THUNDERSTORE.md for
the existing publication handoff. Preparation does not publish or change remote state.
