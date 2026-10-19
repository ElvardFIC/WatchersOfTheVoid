# Watchers of the Void 0.9.2153 — AquariumFishTeam

Prepared 19 October 2026 for Voices of the Void Alpha 0.9.0n, Windows x64.
Package version **0.9.2153** contains DLL **0.9.0n b2156** and network protocol
**2156**. Every peer must use the same package and game build. This includes the
accumulated 0.9.2152 and earlier fixes.

## Changes since 0.9.2152

- Shared laptop UI, file and buffer actions.
- Shared gardening actions and plant records, plus hose routing and link state.
- Supported world and event sound cues on clients.
- More complete placed-object save and construction/wall synchronization.
- Further Grayboar action and turn presentation synchronization, retaining host
  authority for AI and damage.

See the 0.9.2153 section in CHANGELOG_PLUS.md for the release-facing summary and
the AquariumFishTeam note following the full change history.

## Build and checks

- Run `powershell -NoProfile -ExecutionPolicy Bypass -File package/build_2153.ps1`
  for the x64 Release DLL in `build/votv-coop-2153`.
- Run `package/check_2153.ps1` for garden/hose, laptop UI, world sound, and entity
  checks. Extracted native asset checks run only when
  `_research/current-assets` is present; otherwise the report marks them skipped.
  Results/logs are written to
  `build/release-2153-logs`.
- Run `package/pack_2153.ps1` to create and inspect the Thunderstore ZIP.
- Targeted offline checks do not replace a two-player gameplay session. Test garden
  watering/harvest and hoses, laptop file/buffer changes, event/world audio, building
  persistence, and Grayboar movement/actions, reconnect and late join on matching
  builds.

## Publication files

- GitHub release title: `Watchers of the Void 0.9.2153`
- GitHub release description: `CHANGELOG_PLUS.md` 0.9.2153 section followed by the
  AquariumFishTeam message at the end of the full changelog.
- Thunderstore package: `dist/WatchersOfTheVoid-0.9.2153.zip`
- Thunderstore team: `ElvardFIC`; package identifier after publication:
  `ElvardFIC-WatchersOfTheVoid-0.9.2153`.
- Two-player gameplay remains unverified; no release upload is performed by these
  preparation scripts.

## Verification recorded for this preparation

- Windows x64 Release build: passed, DLL product/file version `0.9.0n b2156`.
- Focused garden/hose, laptop UI, world sound and entity regression groups: run by
  `package/check_2153.ps1`; see `build/release-2153-logs/tests.json` for results.
- Native asset contract audit: skipped because extracted game assets are unavailable
  in this workspace.
- Two-player gameplay: not verified.
