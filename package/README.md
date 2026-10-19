# Watchers of the Void

**By ElvardFIC** — a community fork of **Multivoid**, originally created by **Pelmentor**.

Play Voices of the Void together, with accumulated co-op synchronization fixes and improvements.
Version **0.9.2153** targets **Voices of the Void Alpha 0.9.0n** on Windows.

Source: [ElvardFIC/WatchersOfTheVoid](https://github.com/ElvardFIC/WatchersOfTheVoid).

## What's included

- Improvements to item state, inventory/container transfers and replicated object lifetimes.
- Work on appliance connections, computer power, base lighting and canned-food synchronization.
- Improvements to props, construction, selected player models, supported events and shared interactions.

See CHANGELOG.md for the full change list. Some systems are only partially implemented.

## Installation

Install with r2modman or Thunderstore Mod Manager; the Unreal shimloader dependency is included in the manifest.
Use a profile with this fork as the only Multivoid implementation: disable the original Multivoid or an earlier fork copy.
Every player must use this exact **0.9.2153** package and the same game version.

## Current status

This is an experimental co-op build. The x64 Release build and focused offline checks
are recorded in RELEASE_2153.md; two-player gameplay verification is still required.

Build 2153 adds shared laptop UI actions, gardening and hose synchronization, supported
world/event sounds, placed-object/building updates, and further Grayboar action handling.
The package contains DLL **b2156 / network protocol 2156**. Entity synchronization is
still partial and gameplay verification is pending. Includes accumulated 2152 and
earlier changes; every player must update because older protocols are incompatible.

Missing model assets and static preview mismatches still need the model names to
investigate. Token display, shared computer screen, ZIP-disk ejection and disappearing
workbench items remain open reports. See CHANGELOG.md for the scope of this release.

## Credits

- **ElvardFIC** — author and maintainer of Watchers of the Void.
- **MultiVoidKindaPlus** — the codebase this mod is based on, developed with help from **AquariumFishTeam**.
- **Pelmentor and Multivoid contributors** — original Multivoid project and foundation of this fork.
- **Zoomard** — tester and bug reporter.
- **Бритый 2-D <3** — tester and bug reporter.
- Development includes AI-assisted implementation. Cover supplied by ElvardFIC.

Original MIT license and third-party notices are included. This fork is not an official release
from the Voices of the Void developers or the original Multivoid author.
