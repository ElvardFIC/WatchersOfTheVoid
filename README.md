# Watchers of the Void

A community multiplayer fork for **Voices of the Void Alpha 0.9.0n**, maintained by **ElvardFIC**.
Based on [Multivoid by Pelmentor](https://github.com/VOTV-MP/Multivoid) and
[MultiVoidKindaPlus](https://github.com/AquaFishhh/MultivoidKindaPlus), with help from **AquariumFishTeam**.

Play co-op on Windows with up to four players, LAN and Internet sessions, remote player models,
text and positional voice chat, and host-authoritative world synchronization.
This is experimental software; multiplayer verification is still in progress.

## Fork status - 21 October 2026

The latest locally prepared package is **0.9.2155**, for game **0.9.0n**.
It contains DLL **b2178 / network protocol 2178** and integrates the fish branch
with main's Entity, Kavotia, event and personal-sleep work. It adds event replay
and join recovery, five supported frequency generators, placement/coin protection,
water/cleaning/grill state, console/toy recovery and trowel stock persistence.
The Release build and **65/65 offline validation groups** pass on a stable source tree.
See [release notes](package/RELEASE_2155.md) and [changelog](package/CHANGELOG_PLUS.md).
Local packaging does not publish a binary release online.

The final 2155 package also includes prop-destroy identity guards and location-event
session/reentry protection from b2178. Its Release and 65/65 offline groups pass on
unchanged production inputs; real-peer acceptance remains required.

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
