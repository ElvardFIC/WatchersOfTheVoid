# Watchers of the Void 0.9.2157

For Voices of the Void Alpha 0.9.0n, Windows x64. Maintained by ElvardFIC and
AquaFishhh, based on Multivoid by Pelmentor. DLL/network build: **b2184**.

Import **ElvardFIC-WatchersOfTheVoid-0.9.2157.zip** through the mod manager.
The archive includes the original icon, models, enabled loader and shimloader
dependency. Every player must install the same version.

## Changes

The in-game menu and server-browser label now uses package version **0.9.2157**,
generated from the same metadata as the manager archive. Game compatibility stays
at 0.9.0n / protocol 2184. Packaging verifies the shared menu/browser label against
its manifest.

Selected-character ragdolls retain native physics, entry momentum and camera
direction. The selected rig now starts during native construction, removing the
model-swap pause. Recovery preserves the current heading through the game's
get-up animation, fixing the delayed forced turn even without mouse movement.
Fixes also cover sky launches, invisible remote bodies and missing limb updates.
The release includes current main's shared-computer handoff,
balance delivery, event retries and container-content corrections.
See [CHANGELOG_PLUS.md](CHANGELOG_PLUS.md) for the short version history.

## Verification

The display-label rebuild is checked in build/version-2157-check/: runtime output
of the shared menu/browser version formatter, archive startup and one impaired
LAN run in each direction on its exact DLL. Ragdoll/physics/wire implementation
files are compared to the preceding
qualification; their full matrix below belongs to that preceding DLL and was not
repeated for this display correction.

Release compilation, focused production regressions, DLL import checks and
reliable-message negative controls are recorded in build/ragdoll-transition-check/.
Native engine checks cover momentum, camera, installed models, contact surfaces,
remote state transitions and impaired bidirectional LAN synchronization. Eight
additional cases verify immediate selected-rig simulation, completed recovery,
stable heading and subsequent player turning.
Incline/moving-platform contact uses blocking WorldDynamic fixtures and ordinary
C-key recovery through the completed native animation; the model
catalog and camera cases also run on native ground. Static terrain beyond that
ground is not established by the platform matrix.
Package checks compare every archive member to its input and verify startup
from an isolated unpacked installation.

The native checks run without rendering. Ordinary visual/input acceptance remains
with the owner. The historical 65-group result belongs to earlier builds; this
release does not claim another complete pass. The legacy full runner cannot finish
on this machine because its external toolchain and private laptop corpus are
absent. All shared-screen C++ regressions were run separately. General event/entity
coverage, paid resource transactions and existing repository-wide CI findings
remain outside this release's focused qualification.
