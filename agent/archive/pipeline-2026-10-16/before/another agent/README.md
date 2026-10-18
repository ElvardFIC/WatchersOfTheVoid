# Start here — MultiVoidKindaPlus agent handoff

This folder explains our development workflow to the owner's English-speaking assistant and its coding agent. It is project context, not permission to publish, deploy, discard changes, or start unrelated work. Follow the owner's current request and your environment's instructions. Ask the owner to resolve a material conflict with older notes.

**Usage limits for the receiving assistant:** the owner has confirmed that it has only a monthly limit. Historical five-hour/weekly quota instructions in `agent/` apply to the original workspace assistant, not to you. Continue authorized work according to your own monthly allowance; do not inherit the old quota stopping points or fixed reserve percentages. Project scope, verification and release decisions still apply independently of usage limits.

## Read in this order

1. [WORKFLOW.md](WORKFLOW.md): how we investigate, implement, verify, and hand work back.
2. [RECENT_EXAMPLE_AND_STATUS.md](RECENT_EXAMPLE_AND_STATUS.md): a concrete recent change and its actual limits.
3. `../agent/TODO_CONTINUE.md`, then the newest applicable `WORK_*.md` and `TODO_*.md`.
4. `../agent/TESTLOG_STATUS.md`: the complete issue tracker. Read only the relevant older reports and tests after that.
5. [TRANSFER_CHECKLIST.md](TRANSFER_CHECKLIST.md): what must accompany this folder and how to verify the received project.
6. [BUILD_AND_RELEASE_STANDARD.md](BUILD_AND_RELEASE_STANDARD.md): the fork's build names, version mapping, English changelog format and package layout. Read before preparing any authorized release.

Do not read the entire historical archive on every continuation. It consumes the owner's quota without necessarily adding evidence. Some documents are in Russian; their status distinctions matter:

| Label | Meaning |
|---|---|
| Код готов | Implemented in source; this does not mean multiplayer verified |
| Частично | Partially addressed; read the stated limitations |
| Осталось | Work remains |
| Исключено | Deliberately excluded by the owner |

This handoff was updated on **16 October 2026**, at **source protocol2170 / last locally verified package2146**. Start with `../agent/WORK_2170.md` and `../agent/TODO_2170.md`; the tracker now includes reports49–58, all confirmed by the owner on build2168. That package has not been inspected in this workspace. Refresh these facts from code and the latest handoff before making changes. Older `WHAT_THIS_MOD_IS.md` and the workspace README contain useful background but stale version/current-work references.

Source2170 fixes cord socket-endpoint authority and guards native callback lifetime (TestLog51 partial, gameplay pending). Two focused executables, one native-bytecode check and two production compilations passed. Other new reports remain open. Source2169 validates and orders personal profiles, clears partial profiles on disconnect/slot changes, and refreshes held material stock before sending.2168 client wall melee damage and2167 wall enrollment remain implemented. Client wall repair and atomic material transactions remain unfinished. The recent example below explains the exact boundaries. No multiplayer test or release build of these changes has occurred.

## What the project is

MultiVoidKindaPlus (Multivoid+ in the game menu) is a community fork of Pelmentor's MultiVoid co-op mod for **Voices of the Void 0.9.0n**. Preserve attribution to the original author. We are fixing host/client differences, persistence, object lifecycles and shared gameplay while preserving the game's native behavior where practical.

The current work is C++ using Unreal reflection, native/Blueprint hooks and existing networking infrastructure. This is not an editable Unreal game project: important behavior is established by inspecting the locally installed game's cooked Blueprint bytecode and the mod's wrappers.

## Folder map

The current workspace is `C:/Users/Elvar/Downloads/MultiVoid+` (older notes use `E:/MyMods/MultiVoid+`) and the Git root is **its `src` child**, currently on branch `multivoid-plus`.

| Workspace-relative path | Purpose |
|---|---|
| `src/src/votv-coop/src/` | Production C++ implementations |
| `src/src/votv-coop/include/` | Production headers |
| `src/src/votv-coop/sources.cmake` | Production source list |
| `agent/` and `src/agent/` | Mirrored history, continuation notes, tracker and focused tests |
| `src/package/` | Release metadata, changelog and packaging scripts |
| `_research/` | Local investigation scripts, extracted assets and disassembly; not a portable dependency |
| `_toolchain/` | Local Git/compiler environment helpers |
| `dist/` | Built packages; not necessarily equal to current source |
| `incoming/`, `archive/` | Comparison material and historical copies; do not edit as the active project |

The nested `src/src/votv-coop/src` path is intentional. Do not flatten it again. If working from a standalone Git checkout, remove the outer workspace `src/` prefix from the paths above. Local game/tool paths will differ on another machine. Do not distribute cooked game assets with a patch.

This folder also has a matching copy at `src/another agent/`, so it can accompany the Git checkout. Both copies use `../agent/` for adjacent context. Keep the two copies identical when updating this guide. Nothing has been committed or uploaded merely by creating this folder.

## Suggested continuation prompt

> Read `another agent/README.md` and follow its reading order. Check the current working tree and latest agent handoff before editing. Continue [issue or scope supplied by the owner], preserving existing changes. Use focused validation, record what is implemented versus actually tested, and update the mirrored handoff/tracker. Do not build a release, commit, push, install or publish unless I explicitly authorize that step. Do not work on the excluded basement wall.

If the handoff files or current source are missing, request that material before assuming the GitHub branch includes our local work. Many current source files are untracked, and many others have uncommitted edits: a clean clone or a tracked-only patch is not a complete handoff. Do not infer that source2169 exists in a downloaded release or that an old DLL contains these changes.
