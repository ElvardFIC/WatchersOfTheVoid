# Watchers of the Void

A community multiplayer fork for **Voices of the Void Alpha 0.9.0n**, maintained by **ElvardFIC** and **AquaFishhh**.
Based on [MultiVoidKindaPlus](https://github.com/AquaFishhh/MultivoidKindaPlus),
with help from **AquariumFishTeam**.

Play co-op on Windows with configurable 2-32 player hosting, LAN and Internet sessions,
remote player models, text and positional voice chat, and shared-world synchronization.

> **Every version is a test version. It requires extensive testing and may be unstable.**
> Bugs, crashes and multiplayer desynchronization may occur. A successful build or
> automated test run does not guarantee stable gameplay.

## Prepared test build

Package **0.9.2160**, DLL **b2215 / protocol 2215**, for **VotV Alpha 0.9.0n**.
The menu and server browser show **Watchers of the Void 0.9.2160**.
All players must update together.

This package includes the remembered 2-32-player chooser, shovel-hole and food-state
replication, console ownership/queues, personal sleep voting, ATV recovery/steering,
Ariral ship presentation, inventory/container restoration and guarded native actions.
It also includes the current upstream integration while retaining this fork's fixes.
Automatic lobbies above four players also require the updated master service.

The identical production source snapshot passed **94/94 offline groups**. Those
results are reused for the public-version change; Release compilation, the new
version label and archive checks are fresh. Details and remaining checks are in
the [release notes](package/RELEASE.md).
Rendered multiplayer remains unverified for this package. Full event/creature
coverage, resource accounting and several previously reported gameplay issues remain open.

The prepared mod-manager package is **ElvardFIC-WatchersOfTheVoid-0.9.2160.zip**.
Binary download availability follows the published release/package listing;
this description alone does not publish the archive.
See the [changelog](package/CHANGELOG_PLUS.md).

## Installation

Use a package explicitly published for this fork when available on this repository's
[Releases page](https://github.com/ElvardFIC/WatchersOfTheVoid/releases).
All players must install the same build and game version. Disable other Multivoid implementations
in the same profile. Earlier protocol builds are incompatible with this release.

The [upstream installation guide](https://github.com/VOTV-MP/Multivoid/blob/main/docs/install.md)
explains the loader and standard mod layout. Upstream downloads are releases of the original
Multivoid project; check the package name before installing.

## Development

The development hosting window offers **Players (including you)** under session
settings: choose 2–32 with minus/plus or use 2/4/8/16/32 shortcuts. Both hosting
menus remember the same choice for your next session (`net.player_limit`).
These chooser improvements are included in package 0.9.2160.

Transport uses GameNetworkingSockets with positional voice and reliable state lanes.
Each machine's engine performs its local simulation.

See [BUILDING.md](BUILDING.md) and [CONTRIBUTING.md](CONTRIBUTING.md).
The mod lives in [src/votv-coop/](src/votv-coop/); the server is in [server/](server/).
Architecture and system documentation start at [docs/README.md](docs/README.md).
Local test runners, fixtures and historical evidence remain in the maintainer's workspace.

## Credits and license

- **ElvardFIC** — Watchers of the Void maintainer.
- **AquaFishhh** — co-maintainer, synchronization and player-ragdoll improvements.
- **MultiVoidKindaPlus / AquariumFishTeam** — fork foundation and development help.
- **Zoomard** and **Бритый 2-D <3** — testing and bug reports.

Development includes AI-assisted implementation.
The source retains the [MIT License](LICENSE) and [third-party notices](THIRD-PARTY-NOTICES.md).
This project is unaffiliated with the Voices of the Void developers.
