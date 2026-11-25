# Watchers of the Void

A community multiplayer fork for **Voices of the Void Alpha 0.9.0n**, maintained by **ElvardFIC** and **AquaFishhh**.
Based on [MultiVoidKindaPlus](https://github.com/AquaFishhh/MultivoidKindaPlus),
with help from **AquariumFishTeam**.

Play co-op on Windows with configurable 2-32 player hosting, LAN and Internet sessions,
remote player models, text and positional voice chat, and shared-world synchronization.

> **Every version is a test version. It requires extensive testing and may be unstable.**
> Bugs, crashes and multiplayer desynchronization may occur. A successful build or
> automated test run does not guarantee stable gameplay.

## Current candidate — 0.9.2165

Release preparation targets **0.9.2165** for **VotV Alpha 0.9.0n**, with
**b2223 / protocol 2223**. The last qualified package remains **0.9.2163 / b2217**;
its archive is retained. The 2165 Release build and ZIP are not ready: the full
public-prose repository gate still fails, so Release compilation has not started.
The candidate has not been published to Thunderstore.

The source includes the current upstream and selected fork integration, guarded
save recovery and actor lifetimes, Kerfur command capture, sleep/rest and event-cue
handling, plus the accumulated co-op, inventory, event and native-action fixes.
Automated checks do not establish rendered host/client acceptance. Current
qualification status and limits are summarized in the [development notes](package/RELEASE.md). Every peer must use identical DLL bytes.
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
These chooser improvements are included in package 0.9.2161.
Transport is GameNetworkingSockets: 13 unreliable pose and state streams, a voice stream beside
them, and one reliable ordered channel whose payloads are 235 message kinds. Each machine's engine
re-derives animation, physics and rendering from the streamed state. The host is authoritative for
world state, randomness and NPC simulation; a client acts by naming an intent that the host
performs.

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
