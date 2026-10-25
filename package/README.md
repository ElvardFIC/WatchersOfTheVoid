# Watchers of the Void

**By ElvardFIC, co-maintained by AquaFishhh** — community co-op for Voices of the Void.

Play Voices of the Void together, with accumulated co-op synchronization fixes and improvements.
Version **0.9.2161** targets **Voices of the Void Alpha 0.9.0n** on Windows.
The in-game mod label also shows **Watchers of the Void 0.9.2161**.

Source: [ElvardFIC/WatchersOfTheVoid](https://github.com/ElvardFIC/WatchersOfTheVoid).

## What's included

- Selected-character ragdolls with synchronized limbs, preserved entry momentum/view, corrected physics and remote body attachment.
- Host event delivery/recovery, active-event join retries, location-event lifetime guards and adjustable frequency for five supported generators.
- Placement replies/recovery, prop-destroy identity guards, coin pickup protection and food/cooking state through inventory and placement.
- Host water/spill effects, grime/window cleaning, grill edits, cement drying and held-trowel stock persistence.
- Ordered console output, toy/music recovery, garden/hose actions and current held-item profile capture.
- Accumulated creature, personal-sleep, laptop, selected-model and shared-interaction improvements.
- Configurable 2-32 player hosting and selected upstream item/creature-state fixes.
- Shared shovel holes, guarded canned-food portions and final-bite/MRE output handling.
- Console queues and panel ownership, personal sleep voting, companion ATV recovery and Ariral ship presentation.
- Checked inventory/container restoration, local failed-extraction recovery and safer native damage/saw requests.
- The current upstream integration, preserving this fork's player capacity, item identity and delivery guards.
- Bay-window sponge-stroke FIFO retries, bounded replay and session/world lifetime guards.
- Retried native canvas preparation, checked draw/scalar bindings and guarded brush/canvas ownership.

See CHANGELOG.md for the full change list. Some systems are only partially implemented.

## Installation

Install with r2modman or Thunderstore Mod Manager; the Unreal shimloader dependency is included in the manifest.
Use a profile with this fork as the only Multivoid implementation: disable the original Multivoid or an earlier fork copy.
For a downloaded build, import **ElvardFIC-WatchersOfTheVoid-0.9.2161.zip** using
the manager's **Import local mod** action. The package identity remains
**ElvardFIC-WatchersOfTheVoid**. Remove the mistakenly imported **ragdoll** entry
if it was created from the earlier DLL-only archive.
Every player must use this exact **0.9.2161** package and the same game version.

## Current status

> **Every version is a test version. It requires extensive testing and may be unstable.**
> Bugs, crashes and multiplayer desynchronization may occur. Automated checks do not
> guarantee stable gameplay. Test with matching host/client packages and report issues.

This test package contains **DLL b2217 / network protocol 2217** and includes the
changes since 0.9.2158, plus the current upstream integration.
Automatic joins above four players also require the updated master service.

The active version remains **2161** across development continuations. Current
sponge-foam changes have focused regression coverage; full regression and a new
manager-package qualification remain open. Existing archives describe earlier
snapshots. Current evidence and limits are recorded in RELEASE.md and agent/TESTLOG.md.
Rendered gameplay, real-peer synchronization and full event coverage remain
unverified for this exact package. Paid client repair, resource transactions,
ancillary MRE effects and other reported gameplay cases remain open.

## Credits

- **ElvardFIC** — author and maintainer of Watchers of the Void.
- **AquaFishhh** — co-maintainer, synchronization and player-ragdoll improvements.
- **MultiVoidKindaPlus** — the codebase this mod is based on, developed with help from **AquariumFishTeam**.
- **Zoomard** — tester and bug reporter.
- **Бритый 2-D <3** — tester and bug reporter.
- Development includes AI-assisted implementation. Cover supplied by ElvardFIC.

MIT license and third-party notices are included. This fork is unaffiliated
with the Voices of the Void developers.
