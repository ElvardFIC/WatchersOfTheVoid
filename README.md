# Watchers of the Void

A community multiplayer fork for **Voices of the Void Alpha 0.9.0n**, maintained by **ElvardFIC**.
Based on [Multivoid by Pelmentor](https://github.com/VOTV-MP/Multivoid) and
[MultiVoidKindaPlus](https://github.com/AquaFishhh/MultivoidKindaPlus), with help from **AquariumFishTeam**.

Play co-op on Windows with up to four players, LAN and Internet sessions, remote player models,
text and positional voice chat, and host-authoritative world synchronization.
This is experimental software; multiplayer verification is still in progress.

## Fork status - 22 October 2026

The latest locally prepared package is **0.9.2156**, for game **0.9.0n**, with
DLL **b2179 / network protocol 2179**. It integrates the fish branch with main's
Entity, Kavotia, event and personal-sleep work; adds held water/material stock
persistence, shared-screen startup isolation and selected-model ragdoll setup
corrections; and carries the accumulated event, placement, creature and world
synchronization changes. Offline qualification and package scope are recorded in
[release notes](package/RELEASE_2156.md) and [changelog](package/CHANGELOG_PLUS.md).
Dream checks and outcomes stay native and personal; sleep replication is
presentation only. Dream effects/rewards and real-peer acceptance remain open.
Local packaging does not publish a binary release online.

Complete event/creature coverage, paid repairs and resource accounting remain open.
The fresh native-asset audit remains deferred; full public-prose and fact-row CI
have known failures. Offline checks do not establish that reported gameplay issues
are resolved. Two-player testing remains required, including the host's disappearing
old rifle, trowel continuity, late join and reconnect. Token52's UI is unidentified.

## Installation

Use a package explicitly published for this fork when available on this repository's
[Releases page](https://github.com/ElvardFIC/WatchersOfTheVoid/releases).
All players must install the same build and game version. Disable other Multivoid implementations
in the same profile. Earlier protocol builds are incompatible with this release.

The [upstream installation guide](https://github.com/VOTV-MP/Multivoid/blob/main/docs/install.md)
explains the loader and standard mod layout. Upstream downloads are releases of the original
Multivoid project; check the package name before installing.

## Development

See [BUILDING.md](BUILDING.md) and [CONTRIBUTING.md](CONTRIBUTING.md).
The mod lives in [src/votv-coop/](src/votv-coop/); the server is in [server/](server/).
Architecture and system documentation start at [docs/README.md](docs/README.md).
Local test runners, fixtures and historical evidence remain in the maintainer's workspace.

## Credits and license

- **ElvardFIC** — Watchers of the Void maintainer.
- **Pelmentor and Multivoid contributors** — original project and architecture.
- **MultiVoidKindaPlus / AquariumFishTeam** — fork foundation and development help.
- **Zoomard** and **Бритый 2-D <3** — testing and bug reports.

Original contributor credits remain in [docs/credits.md](docs/credits.md).
Development includes AI-assisted implementation.
The source retains the [MIT License](LICENSE) and [third-party notices](THIRD-PARTY-NOTICES.md).
This project is unaffiliated with the Voices of the Void developers.
