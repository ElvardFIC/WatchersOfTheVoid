# Watchers of the Void

**By ElvardFIC** — a community fork of **Multivoid**, originally created by **Pelmentor**.

Play Voices of the Void together, with accumulated co-op synchronization fixes and improvements.
Version **0.9.2156** targets **Voices of the Void Alpha 0.9.0n** on Windows.

Source: [ElvardFIC/WatchersOfTheVoid](https://github.com/ElvardFIC/WatchersOfTheVoid).

## What's included

- Host event delivery/recovery, active-event join retries, location-event lifetime guards and adjustable frequency for five supported generators.
- Placement replies/recovery, prop-destroy identity guards, coin pickup protection and food/cooking state through inventory and placement.
- Host water/spill effects, grime/window cleaning, grill edits, cement drying and held-trowel stock persistence.
- Ordered console output, toy/music recovery, garden/hose actions and current held-item profile capture.
- Accumulated creature, personal-sleep, laptop, selected-model and shared-interaction improvements.

See CHANGELOG.md for the full change list. Some systems are only partially implemented.

## Installation

Install with r2modman or Thunderstore Mod Manager; the Unreal shimloader dependency is included in the manifest.
Use a profile with this fork as the only Multivoid implementation: disable the original Multivoid or an earlier fork copy.
Every player must use this exact **0.9.2156** package and the same game version.

## Current status

This is an experimental co-op build. The x64 Release build and **65/65 passing
offline validation groups** are recorded in RELEASE_2156.md. The package contains
**DLL b2179 / network protocol 2179**, including the fish-branch integration and
the accumulated changes since package 2155. Every player must update together.

Event and creature coverage remains partial. The frequency setting preserves native
conditions and personal dreams; it currently covers four host generators and each
owner's night eyes. Complete event coverage and real-peer acceptance remain open.
Personal dreams retain each player's native trigger and outcome; peers receive
sleep presentation only. Dream effects, rewards, late join, and real-peer
acceptance remain unverified. The fresh native-asset audit remains deferred;
known full-repository CI failures are documented in the release notes.

The host's disappearing old rifle, unidentified token UI, trowel identity continuity,
paid cement repair and resource accounting still require investigation or completion.
See CHANGELOG.md for the full change list and remaining limits.

## Credits

- **ElvardFIC** — author and maintainer of Watchers of the Void.
- **MultiVoidKindaPlus** — the codebase this mod is based on, developed with help from **AquariumFishTeam**.
- **Pelmentor and Multivoid contributors** — original Multivoid project and foundation of this fork.
- **Zoomard** — tester and bug reporter.
- **Бритый 2-D <3** — tester and bug reporter.
- Development includes AI-assisted implementation. Cover supplied by ElvardFIC.

Original MIT license and third-party notices are included. This fork is not an official release
from the Voices of the Void developers or the original Multivoid author.
