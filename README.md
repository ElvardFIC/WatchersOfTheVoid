# Watchers of the Void

A community multiplayer fork for **Voices of the Void Alpha 0.9.0n**, maintained by **ElvardFIC**.
Based on [Multivoid by Pelmentor](https://github.com/VOTV-MP/Multivoid) and
[MultiVoidKindaPlus](https://github.com/AquaFishhh/MultivoidKindaPlus), with help from **AquariumFishTeam**.

Play co-op on Windows with up to four players, LAN and Internet sessions, remote player models,
text and positional voice chat, and host-authoritative world synchronization.
This is experimental software; multiplayer verification is still in progress.

## Fork status — 18 October 2026

The latest package present in the maintainer's workspace is **0.9.2151**.
Development targets **2152**, including wisp capture-animation retries, accepted montage-playback
checks, and safer release/death handling during disconnects or pawn replacement.
A 2152 delivery ZIP has not been verified as present. Local development changes are not necessarily
published in this repository; this page update does not publish a new binary release.

Recent local work also covers personal nightmares, ragdoll recovery, late-loaded NPCs,
Cremator interactions, held-tool effects and model-preview retries.
Offline checks do not establish that reported gameplay issues are resolved.
Two-player testing remains required. Open reports include missing models/preview mismatches,
the shared computer display, token UI, ZIP-disk ejection and disappearing workbench items.

## Installation

Use a package explicitly published for this fork when available on this repository's
[Releases page](https://github.com/ElvardFIC/WatchersOfTheVoid/releases).
All players must install the same build and game version. Disable other Multivoid implementations
in the same profile. Builds 2151 and 2152 use different protocol identities.

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
