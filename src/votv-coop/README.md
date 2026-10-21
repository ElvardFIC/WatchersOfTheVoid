# The mod source

One binary, `main.dll`, shipped as the UE4SS mod folder `Mods\Multivoid\dlls\main.dll` plus
`enabled.txt` and started through UE4SS's C-ABI `start_mod()` contract
(`src/loader/cppmod_entry.cpp`). Reflection, hooking, transport and replication are the mod's own
substrate; the DLL imports nothing from UE4SS, which is the loader and the development tool. The
identity, game target plus build, is in the boot banner and the DLL's generated VERSIONINFO
(`version.rc.in`), not in the filename.

Current local non-Entity development uses protocol2175. Beer/wine and coffee launch
requests now run native flight, collision and electrical outcomes on the host. Local
temporary spheres stop simulating before transport; final-use hand generations
retain their class proof. Native pour audio has its own source-validated transient
lane, with exact caller-point capture and independent delivery/audio order.
Paid garden uses bind their native invocation and owning connection through
capture and send. Care/harvest bytes carry the original player number; the host
rejects stale occupants and interrupted authority probes. Matching peers and two-player gameplay checks
remain required. Current build evidence and open work are in agent/TESTLOG.md and
agent/TODO.md at the repository root; historical ZIPs do not include this source.
World and selected-hand gasoline-bottle breaks use host flight and canonical
decal identities, including cleaning/join/retirement. Hand launches prove the exact
current or recently consumed bottle; mirror factories are refused. Native gasoline
flames follow the host's fire state; local player traces remain native. Fire
requests retain their originating player. Partial native attempts cannot replay
additive fuel, and final extinguishing state retries blocked ready recipients.
Ordinary/salt-rocket explosion surface scorches now use host-native placement and
quiet canonical birth/cleaning/join/retirement. The inherited private-PC explosion
stays local. Observed source provenance survives reconnect in the same world;
unknown existing host scorches keep static cleaning, and full-save provenance
migration still needs work. Scorch decals cannot become liquid launch requests.
Observed looping toy music starts carry episode/position/pitch for late join and
stopped-component recovery, including queue age and estimated transit. Unknown
pre-observation or unsupported sound phase keeps the ordinary state path.
Music snapshots, cached playback, native actions and queued squeaks retain the
canonical actor key; a renamed source cannot inherit an old state or cue.
Client music commands wait for the host's world readiness before leaving the queue.

## Subtrees (principle 7, `docs/architecture.md`)

```
include/
  coop/      gameplay and network headers
  harness/   the test harness: scenarios, config, screenshot
  ue_wrap/   the engine wrapper: one module per engine or game surface; reflection access,
             struct offsets, function thunks; no network or gameplay logic
  ui/        the menus, the server browser, the HUD, the overlay

src/
  bootstrap/   dllmain.cpp (the detach backstop) and boot.cpp (the boot thread)
  loader/      cppmod_entry.cpp, the UE4SS start_mod() contract, and cppmod_stubs.asm
  ue_wrap/     the engine wrapper and the MinHook ProcessEvent detour
  coop/        gameplay and network: RemotePlayer, sessions, transport, the sync lanes
  ui/          the native screens and the ImGui overlay
  harness/     the scenario runner, the scenarios, sdk_check, screenshot, config

third_party/
  minhook/  GameNetworkingSockets/  imgui/  opus/   submodules, built into the DLL
  freetype/  miniaudio/                             vendored in-tree
```

A file that both dereferences engine memory and owns network state is split: the engine-touching
half goes to `ue_wrap/`, the network state to `coop/`, joined by a header API.

## Build and deploy

`BUILDING.md` at the repository root is the whole story: the toolchain, the configure, the build,
and the maintainer's deploy step, which copies `main.dll` to the local game copies and refuses a
DLL whose VERSIONINFO disagrees with the tree. The UE4SS substrate is installed once per copy by
a one-time per-copy UE4SS install.

## What is not in this tree

- UE4SS's code: the mod imports nothing from it. UE4SS itself is the loader every game copy
  runs (the pinned build and the one the mod manager delivers are the same), and
  the development copy also uses it for Live View, Lua probes and header dumps
  (`docs/re-workflow.md`).
- Game assets: never touched. Every sync rides reflected function calls and reads through cached
  offsets.
