# Watchers of the Void

A community multiplayer fork for **Voices of the Void Alpha 0.9.0n**, maintained by **ElvardFIC** and **AquaFishhh**.
Based on [MultiVoidKindaPlus](https://github.com/AquaFishhh/MultivoidKindaPlus),
with help from **AquariumFishTeam**.

Play co-op on Windows with configurable 2-32 player hosting, LAN and Internet sessions,
remote player models, text and positional voice chat, and shared-world synchronization.

> **Every version is a test version. It requires extensive testing and may be unstable.**
> Bugs, crashes and multiplayer desynchronization may occur. A successful build or
> automated test run does not guarantee stable gameplay.

## Development — 0.9.2163

The active public version is **0.9.2163**, targeting **VotV Alpha 0.9.0n**.
Internal DLL/network identity remains **b2217 / protocol 2217**. Continuations
add fixes to this version; a new public version requires the owner's request.

Current work includes Tutorial join placement and inventory-icon cache repair, killer-wisp lifetime guards, selected fork fixes and six selectable hosting game modes, reflected flashlight state and replacement replay, explicit ragdoll physics blending, guarded held-toy audio and watoy ownership, and capture of native held/thrown sponge window dabs,
with the existing co-op, 2-32-player, inventory, event and native-action fixes.
The current 0.9.2163 ZIP passed all 124 current offline validation groups,
the Release build, production version label and manager package checks.
Rendered host/client acceptance remains open. Current evidence and limits are in
[TESTLOG](agent/TESTLOG.md) and the [development notes](package/RELEASE.md).
Every peer used for testing must carry identical current DLL bytes.
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
them, and one reliable ordered channel whose payloads are 193 message kinds. Each machine's engine
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
