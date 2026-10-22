# Watchers of the Void 0.9.2158

For Voices of the Void Alpha 0.9.0n, Windows x64. Maintained by ElvardFIC and
AquaFishhh, based on Multivoid by Pelmentor. DLL/network build: **b2186**.

> **Every version is a test version. It requires extensive testing and may be unstable.**
> Bugs, crashes and multiplayer desynchronization may occur. Automated checks do not
> establish stable gameplay or confirm that every reported issue is resolved.

Import **ElvardFIC-WatchersOfTheVoid-0.9.2158.zip** through the mod manager.
The archive includes the original icon, starter models, enabled loader and
shimloader dependency. Every player must install the same package and game version.

## Changes

- Increase shared script-watch capacity used by doors, switches and containers.
- Retry flashlight initialization/apply and client container seeding through loading
  and refused sends, retaining exact recipients and current occupancy.
- Select 2-32 total players when hosting; extend admission, acknowledgements,
  masks and held-item addresses. Automatic lobbies above four players require
  the updated master service, which is not deployed by this package.
- Adopt selected upstream vital quantization, delayed prop/trash class resolution,
  fossil-hound body facing, NPC transform retries and plain-wisp landing fixes.
- Retain selected-character ragdolls and earlier fork synchronization changes.

## Verification and limits

The exact DLL is qualified with 68/68 offline groups, Release compilation,
loader imports/exports, generated menu/browser label and package integrity checks.
Evidence: `build/release-2158/verification.json` and
`build/package-current/verification.json` in the build workspace.

This is an **offline-qualified test package**, not a claim of native multiplayer
acceptance. Rendered gameplay, host/client synchronization, late join, reconnect
and larger lobbies still need testing. Previous native ragdoll matrices belong to
earlier DLLs and are not results for this exact build. Existing event/creature,
repair/resource and reported gameplay gaps remain open. Known inherited repository
documentation-baseline findings are recorded in the test log.

Packaging is local; GitHub/Thunderstore binary publication and server deployment
are separate actions. See [CHANGELOG_PLUS.md](CHANGELOG_PLUS.md) for version history.
