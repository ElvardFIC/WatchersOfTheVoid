# Build naming, versioning and release presentation

This is the MultiVoidKindaPlus fork's current convention, verified against `package/pack.ps1`, `package/CHANGELOG_PLUS.md`, the DLL version-resource template and the in-game version label. Paths below are relative to the standalone Git checkout. Updated16 October2026. It documents the release process; it does not authorize a build, installation or publication.

## Names and version mapping

Use one build number **N** consistently. The current game target is **0.9.0n**. The `0.9.N` package version is the Thunderstore-facing representation of that build, not a separately evolving gameplay/protocol version.

| Surface | Required format | Last packaged example |
|---|---|---|
| Package/project name | `MultiVoidKindaPlus` | `MultiVoidKindaPlus` |
| In-game mod name / DLL ProductName | `Multivoid+` | `Multivoid+` |
| In-game full version | `Multivoid+ 0.9.0n bN` | `Multivoid+ 0.9.0n b2146` |
| Build identifier in notes | `bN` | `b2146` |
| Protocol/build integer | `N` | `2146` |
| Thunderstore manifest version_number | `0.9.N` | `0.9.2146` |
| ZIP filename | `MultiVoidKindaPlus-0.9.N.zip` | `MultiVoidKindaPlus-0.9.2146.zip` |
| Human-facing release title, if needed | `MultiVoidKindaPlus 0.9.N` | `MultiVoidKindaPlus 0.9.2146` |
| DLL filename, including inside package | `main.dll` | `mod/dlls/main.dll` |
| DLL ProductVersion and FileVersion strings | `0.9.0n bN` | `0.9.0n b2146` |
| DLL numeric FILEVERSION / PRODUCTVERSION | `0,9,N,0` for this game target | `0,9,2146,0` |
| Changelog section heading | `## 0.9.N` | `## 0.9.2146` |

The title row standardizes a display label; it is not a claim that a GitHub release with that title already exists. Preserve exact spelling/capitalization of package and UI names. Do not rename `main.dll` to include a version. Do not append `RBE`, `[test version]`, `(test build)` or an agent-specific suffix by default. Some historical changelog entries have test labels and the imported2142 archive had an RBE suffix; these are not the default naming scheme. Add/remove special labels only when the owner requests that designation; keep testing limitations explicit in the release text.

Preserve the fork's branding and Pelmentor attribution. The current DLL description is `Multivoid+ - Voices of the Void co-op mod (community build on Multivoid b208)`. That upstream-base attribution is not the new build number. Preserve the owner's existing package introduction and branding assets unless asked to change them.

## Source revision versus shipped release

At this handoff, **source protocol2170** is reserved for accumulated work and **last packaged release2146** is still the latest locally verified package. The owner reports ten issues on build2168; that package has not been produced or inspected in this workspace. Treat those reports as2168 observations, not old pre-fix behavior. `package/pack.ps1` and the published-style changelog intentionally remain at2146. Do not run that old packaging configuration against a2170 DLL, and do not claim2170 has shipped.

For the next authorized release, use the actual final source build number, not automatically2147 and not necessarily2170 if development has advanced. Never reuse a released build number for different release bytes. Protocol/layout/meaning changes require a build/protocol increment; a new release also needs an identity distinct from the previous release. A reserved, never-released source number can become the release number when ready. Both peers must use the same game target and build.

## Where the values come from

| File | What to check/change when preparing an authorized release |
|---|---|
| `src/votv-coop/include/coop/net/protocol.h` | `kProtocolVersion` is the canonical build integer |
| `src/votv-coop/CMakeLists.txt` | `VOTVCOOP_GAME_TARGET`; CMake reads the protocol build for generated metadata |
| `src/votv-coop/include/coop/version.h.in` | Generated game-target/display template; do not hand-edit generated output |
| `src/votv-coop/version.rc.in` | DLL naming and string/numeric version-resource template |
| `src/votv-coop/src/coop/session/session_manager.cpp` | In-game `DisplayVersion()` combines game target and protocol integer |
| `package/pack.ps1` | `$version`, expected DLL ProductVersion and ZIP filename generation must agree with N |
| `package/CHANGELOG_PLUS.md` | Add the new English release entry at the top |

The packer **generates** `manifest.json`; there is no maintained `package/manifest.json` to update. Do not edit a leftover staging manifest instead of the script. The current manifest name is `MultiVoidKindaPlus`, website is `https://github.com/AquaFishhh/MultivoidKindaPlus`, and dependency is `Thunderstore-unreal_shimloader-1.1.7`. Preserve these and the owner's description unless changing them is part of the requested task. Do not silently copy upstream release naming or CI-ledger instructions from historical documentation over this fork's packaging convention.

## Changelog standard

Write the package changelog in English, newest version first. It contains **our Plus changes only**, not a copy of the entire upstream history. The packer copies `CHANGELOG_PLUS.md` to the archive's `CHANGELOG.md`.

Describe user-visible behavior accurately and distinguish completed work from partial coverage. Include the required same-build note, relevant carried-over changes and actual validation status. If the owner supplies a personal/informal message, translate it faithfully and retain its tone; do not replace it with invented promotional claims. Do not say that all TestLog issues are fixed or multiplayer tested when that has not happened.

Template (replace placeholders with the final facts):

```markdown
## 0.9.N

- Fixed [specific player-visible problem and resulting behavior].
- Added [specific feature and its supported scope].

All players need bN. Includes [relevant earlier changes, if useful].
[Checks actually completed]. [Remaining limitations / multiplayer testing needed].
```

Intermediate unbuilt source revisions belong in `agent/WORK_N.md`, `TODO_N.md` and the tracker. Do not manufacture a shipped changelog entry for every continuation. The eventual release entry should describe the accumulated result since the previous packaged build.

## Archive layout and final checks

The existing packer stages files under checkout `dist/stage` and writes checkout `dist/MultiVoidKindaPlus-0.9.N.zip`. It expects the compiled DLL at `build/votv-coop/main.dll`. Preserve this archive layout, with no extra enclosing directory:

```text
manifest.json
icon.png
README.md
CHANGELOG.md
LICENSE
THIRD-PARTY-NOTICES.md
mod/
  enabled.txt          # true
  dlls/
    main.dll
pak/
  scientists.pak
  ...intended mod .pak and preview .png assets...
```

Before the authorized release, review ALL changes since2146, finish the agreed TestLog scope and resolve known blockers with the owner. Configure/build the final source so generated version metadata matches it. The receiving assistant has only a monthly limit; allow enough capacity for build repairs without importing the original assistant's five-hour reserve percentages.

After packaging, inspect the actual ZIP: required entries, manifest name/version, changelog heading, DLL ProductVersion and its SHA-256 match with the newly built DLL. The current script already checks required entries, DLL identity before packaging and the archived DLL hash; preserve those guards rather than weakening them to fit a stale binary. Record the final ZIP path and verification result in the work note. Historical local delivery also copied the verified ZIP to the outer workspace `dist/`; if that copy is made, compare its hash rather than assuming it matches.

A successful package is not proof of two-player gameplay. Report full compilation, package verification and multiplayer testing separately. Creating a ZIP does not authorize upload to Thunderstore/GitHub or installation into the owner's game.
