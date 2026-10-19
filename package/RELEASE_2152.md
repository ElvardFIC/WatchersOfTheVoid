# Watchers of the Void 0.9.2152 - ElvardFIC

Prepared 19 October 2026 for Voices of the Void Alpha 0.9.0n, Windows x64.
Community fork of Multivoid by Pelmentor; original license and third-party notices retained.

Package version: **0.9.2152**. DLL resources: **0.9.0n b2153**. Network protocol:
**2153** (52-byte EntityPose with full orientation). The package number follows the
owner's requested release number; the internal number preserves the incompatible
NPC wire-format change. All peers must install this exact package. Protocols 2151
and 2152 cannot join it. Build scripts/directories retain their 2152 release names.

## Changes since 2151

- Wisp capture: bounded animation-readiness retries on host and client, accepted
  native montage-playback checks and actor/AnimInstance identity guards.
- Wisp lifecycle: retain the original victim, protect hold movement across native
  callbacks, and handle disconnect, pawn replacement and nested release safely.
- NPC poses: pitch/yaw/roll replication, shortest-arc interpolation, first-sample
  and teleport snapping. Add Fossil Hound and yellow-wisp spawn interception.
- Possessed boar: use its native prop body rather than the Character allowlist;
  feed host motion into prop replication and route remote-player bite damage to
  its owner. Protect foreign health and stable adopted replicas across handoffs.
- Possessed-boar locomotion: supply interpolated movement velocity only to its
  native client AnimBP getter, enabling native locomotion/movement-loop audio
  input while preserving physics and other native callers.
- Retain all accumulated 2151 fixes. See CHANGELOG.md for the player-facing history.

## Verification

- MSVC x64 Release: package/build_2152.ps1, build/votv-coop-2152.
- Nine focused runners: package/check_2152.ps1 covers player reports, wisp
  animation/hold/release, ragdoll retry/skin, personal sleep, entity orientation,
  boar damage/replica identity/velocity getter, prop interpolation, and native
  extracted-game spawn/class/animation evidence.
- package/pack.ps1 checks icon, DLL version, archive identity, changelog heading,
  required files and embedded DLL SHA256.
- package/verify_2152.ps1 checks Release configuration, source protocol, loader
  imports/exports, production-source inclusion, all nine named runners and
  delivery hashes.
- Evidence: build/release-2152-logs/ (build log, tests.json, source inventory,
  imports/exports and verification.json).
- Delivery: workspace dist/WatchersOfTheVoid-0.9.2152.zip and SHA256SUMS-2152.txt.

## Remaining verification and implementation

Two-player gameplay and FPS measurement remain pending. Verify host/client spawn,
capture, movement, targeting, damage, death, disconnect/reconnect and mid-event join.
Offline checks do not establish gameplay correctness or close user reports.

Entity-category synchronization remains partial. Boar attack/roar/death montage and
sound cues, late-join action state, helper/turning coordination and corpse/loot
behavior need further work and gameplay verification. Other requested entity
families still have unresolved mappings and behavior; personal nightmares remain
peer-local. Full tracking remains in agent/TODO.md, WORK.md and TESTLOG.md.

Missing models and mismatched static preview images still need model names. Shared
computer display, token UI, ZIP-disk ejection and disappearing workbench items remain
open reports. No game installation or online publication was performed.
