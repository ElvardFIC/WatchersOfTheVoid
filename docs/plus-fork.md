# Watchers of the Void — by ElvardFIC

Source repository: https://github.com/AquaFishhh/MultivoidKindaPlus

Original project: https://github.com/VOTV-MP/Multivoid, created by Pelmentor.
The original license, attribution and Git history are retained. Plus development
is authored and maintained by ElvardFIC, with implementation assistance from OpenAI
Codex. This is an independent community fork.

## Branches and contributions

The last committed `multivoid-plus` snapshot contains changes through b2142; the local
working tree now packages all accumulated changes through WORK_2192 as release2150.
It preserves
the existing local b2081 commit and records later work as a source snapshot;
there are no invented historical per-release commits. `main` initially follows
the original project. These branches have diverged: the snapshot is not a claim
that current upstream changes have been integrated.

For a future upstream PR, fetch upstream, start a separate branch from
`upstream/main`, and port one coherent fix together with its tests and relevant
documentation. Check whether upstream already addresses the issue. Follow
CONTRIBUTING.md and explain the trigger, resulting behavior and actual validation.
Do not submit the entire cumulative snapshot as an unrelated collection of fixes.

Local remotes: `origin` is AquaFishhh's fork; `upstream` is VOTV-MP/Multivoid.
New commits use AquaFishhh's GitHub noreply address. Earlier authorship is retained.

## Current source and validation

Release2150 targets Voices of the Void 0.9.0n. All players need the same new package.
See package/RELEASE_2150.md for current build/regression evidence and
agent/TODO_2150.md for the consolidated open backlog. Gameplay remains unverified.
The following b2142 summary is historical.
Changes include gravity-gun motion, local-file radio, service panels and kitchen
devices, garage lift, item records, world transfer, shared fire/explosions, client
performance fixes and food placement state. See package/CHANGELOG_PLUS.md for the
release history. Older changelog statements reflect their release-time status.

The user confirmed b2141 microfreezes fixed. b2142 compiled successfully and passed
three focused actual-source test executables, the ABI gate and archive checks.
Its food multiplayer test is still pending: bite a food, have the client put it
through selected hand/inventory and place it down, repeat, then check on the host.
Offline checks are not multiplayer verification.

## Build and packaging

Use BUILDING.md for the original build dependencies and configuration.
`package/pack.ps1` is adapted to this repository root and packages the built
`build/votv-coop-2150/main.dll`, built by `package/build_2150.ps1` in Release mode.
It checks the DLL version and the written archive. Local toolchains, build
outputs, research dumps and game assets are not committed. Packaging still needs
the separately obtained starter-skin pak expected by the script; it is not
distributed as source under this repository's MIT license.

`agent/tests/` retains the Plus source tests and stubs, with source paths adapted
to this checkout. The release integration suite is `package/check_2150.ps1`;
ordinary bug fixes should run only affected focused tests.
Historical runners may depend on the original local `_toolchain` environment,
`_research` baselines, or extracted native-game assets. Those inputs are not
published, and historical suites are not claimed to run on a clean clone.
Private development notes, user test logs and account-limit records stay local.
