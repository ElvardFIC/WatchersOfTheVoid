# Watchers of the Void

**By ElvardFIC, co-maintained by AquaFishhh** — a community fork of **Multivoid**, originally created by **Pelmentor**.

Play Voices of the Void together, with accumulated co-op synchronization fixes and improvements.
Version **0.9.2157** targets **Voices of the Void Alpha 0.9.0n** on Windows.
The in-game mod label also shows **Watchers of the Void 0.9.2157**.

Source: [ElvardFIC/WatchersOfTheVoid](https://github.com/ElvardFIC/WatchersOfTheVoid).

## What's included

- Selected-character ragdolls with synchronized limbs, preserved entry momentum/view, corrected physics and remote body attachment.
- Host event delivery/recovery, active-event join retries, location-event lifetime guards and adjustable frequency for five supported generators.
- Placement replies/recovery, prop-destroy identity guards, coin pickup protection and food/cooking state through inventory and placement.
- Host water/spill effects, grime/window cleaning, grill edits, cement drying and held-trowel stock persistence.
- Ordered console output, toy/music recovery, garden/hose actions and current held-item profile capture.
- Accumulated creature, personal-sleep, laptop, selected-model and shared-interaction improvements.

See CHANGELOG.md for the full change list. Some systems are only partially implemented.

## Installation

Install with r2modman or Thunderstore Mod Manager; the Unreal shimloader dependency is included in the manifest.
Use a profile with this fork as the only Multivoid implementation: disable the original Multivoid or an earlier fork copy.
For a downloaded build, import **ElvardFIC-WatchersOfTheVoid-0.9.2157.zip** using
the manager's **Import local mod** action. The package identity remains
**ElvardFIC-WatchersOfTheVoid**. Remove the mistakenly imported **ragdoll** entry
if it was created from the earlier DLL-only archive.
Every player must use this exact **0.9.2157** package and the same game version.

## Current status

This package contains **DLL b2184 / network protocol 2184** and retains the
fish-branch integration and accumulated Watchers of the Void improvements.
The selected ragdoll starts immediately during native construction. Getting up
preserves the current heading without the delayed forced turn. Limb batches
rotate at actual sends and survive coalesced game ticks.
Focused regressions plus native completed entry/recovery8/8,momentum8/8,
camera10/10,surfaces24/24,installed
models30/30 and remote sleep/sit/ragdoll transitions36/36 pass. Six bidirectional
LAN runs pass with250ms outbound lag,40ms jitter and5% loss on both peers.
Visual/input acceptance remains with the owner. The full65-group result belongs
to b2178 and has not been repeated for b2184.
These full ragdoll matrices belong to the preceding DLL. The display-label
rebuild preserves its gameplay/wire implementation and is checked separately by
the shared menu/browser formatter at runtime, startup and one impaired LAN run
per direction.
See RELEASE.md for the current qualification. Every player must update together.

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
- **AquaFishhh** — co-maintainer, synchronization and player-ragdoll improvements.
- **MultiVoidKindaPlus** — the codebase this mod is based on, developed with help from **AquariumFishTeam**.
- **Pelmentor and Multivoid contributors** — original Multivoid project and foundation of this fork.
- **Zoomard** — tester and bug reporter.
- **Бритый 2-D <3** — tester and bug reporter.
- Development includes AI-assisted implementation. Cover supplied by ElvardFIC.

Original MIT license and third-party notices are included. This fork is not an official release
from the Voices of the Void developers or the original Multivoid author.
