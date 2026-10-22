# Watchers of the Void

A community multiplayer fork for **Voices of the Void Alpha 0.9.0n**, maintained by **ElvardFIC** and **AquaFishhh**.
Based on [Multivoid by Pelmentor](https://github.com/VOTV-MP/Multivoid) and
[MultiVoidKindaPlus](https://github.com/AquaFishhh/MultivoidKindaPlus), with help from **AquariumFishTeam**.

Play co-op on Windows with up to four players, LAN and Internet sessions, remote player models,
text and positional voice chat, and host-authoritative world synchronization.
This is experimental software; multiplayer verification is still in progress.

## Fork status - 22 October 2026

The current DLL is **b2184 / protocol 2184**, for game **0.9.0n**. Its menu and
server-browser label reads **Watchers of the Void 0.9.2157**, generated from the
manager package metadata. This display correction is checked by running the shared
menu/browser formatter and one impaired LAN run per direction; the complete
ragdoll matrix below describes the preceding qualification of the unchanged
gameplay code.
It initializes the selected ragdoll during native construction and preserves the
current heading through get-up, removing the entry pause and delayed forced turn.
It retains limb synchronization, native momentum and launch fixes.
Eight completed entry/recovery cases, eight momentum cases, ten camera cases,
24 surface cases,30 installed models,36 remote state transitions and six impaired
LAN runs pass. Rendered/input acceptance remains with the owner.
See [ragdoll work state](agent/RAGDOLL_CONTINUATION.md).
The complete package is **ElvardFIC-WatchersOfTheVoid-0.9.2157.zip**, with the
original fork icon, models and enabled loader layout. See [release notes](package/RELEASE.md) and
[changelog](package/CHANGELOG_PLUS.md).
Local packaging does not publish a binary release online.

The previous b2178 Release passed 65/65 offline groups. That full suite has not
been repeated for b2184; the current qualification is specific to ragdoll changes.

Complete event/creature coverage, paid repairs and resource accounting remain open.
The fresh native-asset audit remains deferred. Repository checks are described in
[testing](docs/testing.md). Offline checks do not establish that reported gameplay issues
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

Transport is GameNetworkingSockets: 13 unreliable pose and state streams, a voice stream beside
a reliable channel with 224 message kinds. Each machine's engine performs its local simulation.

See [BUILDING.md](BUILDING.md) and [CONTRIBUTING.md](CONTRIBUTING.md).
The mod lives in [src/votv-coop/](src/votv-coop/); the server is in [server/](server/).
Architecture and system documentation start at [docs/README.md](docs/README.md).
Local test runners, fixtures and historical evidence remain in the maintainer's workspace.

## Credits and license

- **ElvardFIC** — Watchers of the Void maintainer.
- **AquaFishhh** — co-maintainer, synchronization and player-ragdoll improvements.
- **Pelmentor and Multivoid contributors** — original project and architecture.
- **MultiVoidKindaPlus / AquariumFishTeam** — fork foundation and development help.
- **Zoomard** and **Бритый 2-D <3** — testing and bug reports.

Original contributor credits remain in [docs/credits.md](docs/credits.md).
Development includes AI-assisted implementation.
The source retains the [MIT License](LICENSE) and [third-party notices](THIRD-PARTY-NOTICES.md).
This project is unaffiliated with the Voices of the Void developers.
