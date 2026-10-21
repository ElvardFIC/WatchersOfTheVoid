# Watchers of the Void 0.9.2156

Prepared 22 October 2026 for Voices of the Void Alpha 0.9.0n, Windows x64.
Package **0.9.2156** contains **DLL 0.9.0n b2179 / protocol 2179**. Every peer must use identical current DLLs and the same game build.

This package carries the development b2179 identity, including held water/material stock persistence, shared-screen startup isolation and selected-model ragdoll setup corrections, as well as accumulated changes since package 2155. See the 0.9.2156 section of CHANGELOG_PLUS.md for the player-facing summary.

## Dreams

Each player's native sleep loop keeps its own eligibility and probability checks, nightmare settings, Insomniac condition, `wakeup()` and `createDream()` calls, dream scene and actor effects, and native `leaveDream()` / `awoken()` return. Sleep-state packets report rest and dream presentation; the legacy DreamScene packet does not cause another player's dream to be rolled, selected, entered or ended. Event-frequency scaling does not change native sleep waits, personal dream chances, Insomniac checks or story dates.

The installed-game Blueprint audit is deferred. Scene-specific rewards/effects, wake behavior and late-join/lifetime outcomes have not passed real-peer acceptance. The package does not claim multiplayer dream scenes are reproduced for other players.

## Build and qualification

- `powershell -NoProfile -ExecutionPolicy Bypass -File package/build_2156.ps1` builds Release and runs the current offline qualification groups against the unchanged production snapshot.
- The b2179 DLL identity, source snapshot, Release build and all validation results are captured under `build/release-2156-logs/` and `build/continuation-check/`.
- The fresh game-pak/Blueprint audit remains deferred. Real-peer event, frequency, late-join and reconnect acceptance has not been performed. Offline checks do not establish multiplayer gameplay acceptance.
- Full public-prose and fact-row CI gates retain known failures outside the release qualification groups.

## Packaging

Run `package/pack_2156.ps1`, then `package/verify_2156.ps1` in PowerShell with execution policy Bypass. Packaging verifies the qualified DLL, embedded documents, manifest, icon, archive layout and delivery hashes. It does not upload or publish the archive.

Deliverables: `WatchersOfTheVoid-0.9.2156.zip`, `SHA256SUMS-2156.txt` and `GITHUB_RELEASE_2156.md`. The ZIP uses the existing Unreal shimloader dependency, branding, paks, license and credits.

## Remaining event scope

The complete eight-category native event inventory, all repeatable random-event generators, dream outcome/effect acceptance and real-peer synchronization remain open in agent/WORK.md. See the changelog for the implemented scope and known limits.