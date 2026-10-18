# Receipt update for source2170

Current checkout: C:/Users/Elvar/Downloads/MultiVoid+/src. Verify protocol2170; agent/WORK_2170.md, TODO_2170.md and updated TESTLOG_STATUS.md with reports49–58 observed on2168; cord_sync.cpp and ue_wrap/devices/cord.cpp; tests/run_2170.ps1, cord_intent_2170_test.cpp, cord_wrapper_2170_test.cpp, cord_native_2170_test.py and cord_2170_stubs. The runner also uses existing state_2145_stubs, text_drone_stubs and stubs. Keep all earlier dependencies. TestLog51 partial; no multiplayer confirmation or full DLL. Last locally verified package2146; owner's2168 package uninspected. Do not include extracted game assets with the handoff.

# Historical receipt for source2169

Current checkout: C:/Users/Elvar/Downloads/MultiVoid+/src. Verify protocol2169, agent/WORK_2169.md, agent/TODO_2169.md, agent/tests/run_2169.ps1, profile_receive_2169_test.cpp, material_profile_2169_test.cpp, profile_2169_stubs and the updated building_wrapper_2153_test.cpp. Include player_inventory_sync.h/.cpp, inventory_wire.cpp, blob_chunks.h/.cpp and building_material.h/.cpp plus the comment update in subsystems.cpp. All earlier2166–2168 dependencies remain required. Personal-profile protection and live held-material snapshots are implemented; client repair and atomic cement transactions remain open. Last package2146; no full DLL or multiplayer validation.

# Historical receipt for source2168

The current checkout is C:/Users/Elvar/Downloads/MultiVoid+/src. Verify protocol2168, agent/WORK_2168.md, agent/TODO_2168.md, agent/tests/run_2168.ps1 and wall_damage_2168_test.cpp. Include updated prop_action_2143_test.cpp, wall_wrapper_2166_test.cpp and building_wrapper_2153_test.cpp plus their existing dependencies. Production changes are in protocol.h, prop_action_sync.cpp, wall_build.h/.cpp, building_material.h/.cpp and player_inventory_sync.cpp. Earlier2166/2167 files remain required. Client melee wall damage and held-trowel profile refresh are implemented; actual client repair and atomic cement transactions remain open. Last packaged release2146. No full DLL or multiplayer validation.
# Historical receipt for source2167

The current checkout is C:/Users/Elvar/Downloads/MultiVoid+/src. Verify protocol2167, agent/WORK_2167.md, agent/TODO_2167.md, agent/tests/run_2167.ps1 and wall_enrollment_2167_test.cpp. The updated placed_save_sync.cpp and wall_build_sync.cpp must accompany the handoff. Older2166 files listed below remain required dependencies; their snapshot is historical. Existing supported wall enrollment is implemented in2167; client damage/repair and cement transactions remain next. Last packaged release2146.
# Sending and receiving the current project

This is a preparation guide, not a claim that an archive has been created or uploaded. Updated for source2166,15 October2026.

## What to send

The active Git checkout is `E:/MyMods/MultiVoid+/src`, not the outer workspace. For a portable project handoff, include its current working files, including untracked source, headers, tests, documentation and intended mod assets. In particular, include adjacent `another agent/` and `agent/` folders. The outer copies are mirrored conveniences; the checkout copies make this handoff self-contained.

Do not send only a GitHub link, a release ZIP/DLL or a tracked-only `git diff`: none is guaranteed to contain the current local changes. Many post2146 implementations are untracked. Keep the nested repository layout; do not flatten the source tree again.

Exclude credentials, local account configuration, downloaded cooked game assets, compiler/toolchain bundles, generated build directories, old release archives and unrelated incoming material. Intended project assets such as mod-owned skin previews are different from extracted game assets. Keep Git history if useful for comparison, but the full current working files are essential. This guide does not authorize publishing the project.

## Quick receipt check

From the root of the received standalone checkout, confirm:

- `src/votv-coop/include/coop/net/protocol.h` contains `kProtocolVersion = 2166` (or a documented later revision).
- `agent/WORK_2166.md`, `agent/TODO_2166.md`, `agent/TODO_CONTINUE.md` and `agent/TESTLOG_STATUS.md` exist.
- `another agent/BUILD_AND_RELEASE_STANDARD.md` accompanies the workflow, so the receiving assistant preserves the fork's naming/version/changelog/package conventions.
- `src/votv-coop/src/coop/world/wall_build_sync.cpp`, `src/votv-coop/src/ue_wrap/actors/wall_build.cpp` and their headers exist; both implementation files appear once in `src/votv-coop/sources.cmake`.
- The existing `placed_save_sync.cpp`, `placed_save.cpp`, `placed_save_record.cpp`, `hand_extinguisher_sync.cpp` and `building_material.cpp` contain the2166 changes described in WORK_2166.
- `agent/tests/run_2166.ps1` and all four `wall_*_2166_test.cpp` files exist. The2159 runner's bucket tests contain the concrete-bucket cases.

These are representative completeness checks, not a substitute for including all current source. If an item is missing, obtain the current files rather than silently continuing from an older release.

## Local research and tooling

`_research/` contains local native disassembly and helper output; it is not a required distributable project dependency. The WORK notes retain exact class/function names and evidence paths. If additional native investigation needs absent evidence, ask for the relevant textual excerpt or inspect a compatible legally available local game installation. Do not assume the owner's absolute game path exists on your machine. Do not bundle cooked game assets to make tests pass.

The focused PowerShell runners use an available MSVC `cl`, falling back to the owner's `_toolchain/env.ps1`. Configure your own compiler environment when that helper is absent. Read project build documentation for actual build dependencies; do not infer that passing mock-based tests provides the game runtime or proves the release DLL links.

## Continue and return changes

Start with wall client damage/repair and existing-placement enrollment, or another TestLog scope specified by the owner. Preserve the documented partial status of materials/events/creatures. The receiving assistant has only a monthly limit; old five-hour/weekly stopping rules do not apply.

Return all changed and newly created files with an English work note: exact behavior, evidence, focused checks run, checks unavailable, and remaining scope. A patch is useful only if it also carries new files and identifies its base. Coordinate which copy is authoritative before merging concurrent work. Keep the adjacent handoff and tracker current; mirror the outer folders too when present. Do not package a release, commit, push, install or publish without the owner's authorization.
