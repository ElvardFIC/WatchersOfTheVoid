# Watchers of the Void 0.9.2155

Prepared 21 October 2026 for Voices of the Void Alpha 0.9.0n, Windows x64.
Package **0.9.2155** contains **DLL 0.9.0n b2178 / protocol 2178**.
Every peer must use identical current DLLs and the same game build.

This release includes all changes since package 2154 (DLL b2156), including the
`watchers-of-the-void-fish` integration and the event lifetime, night-eyes frequency,
trowel persistence, placement recovery, received-destroy identity and location-event
session/reentry fixes. The complete behavior inventory
is the 0.9.2155 section of [CHANGELOG_PLUS.md](CHANGELOG_PLUS.md).

## Build and qualification

- `powershell -NoProfile -ExecutionPolicy Bypass -File package/build_2155.ps1`
  builds Release and runs all 65 current offline validation groups. It reuses the
  canonical `build/votv-coop-2154` CMake directory; that historical directory name
  does not determine the package or DLL version.
- All **65/65 groups passed** on an unchanged production snapshot, followed by the
  final incremental Release build. Qualification evidence:
  `build/continuation-check/{verification.json,results.json,identity.json,build.log,logs/}`;
  initial b2178 compilation: `build/continuation-build.log`.
  Final packaging evidence: `build/release-2155-logs/release-verification.json`.
- Qualification verifies 781 build source entries, DLL identity/imports/exports,
  all 224 reliable message kinds and rejection of 18 routing fault controls.
- DLL SHA256: `9e5f4fd23b272efa6fd807df45cba466664acfe8639c5d4d639a9f23c9c4d5f8`.
- The night-eyes native check decodes the existing research corpus, preserving
  its 1800–2700-second native wait, 5% chance and day/night conditions before rate
  scaling. This is not a fresh installed-game audit.
- The fresh game-pak/Blueprint audit remains deferred. The complete native-contract
  suite cannot run against the incomplete local corpus. Full public-prose and
  fact-row CI gates have known failures and are outside the 65 passing groups.
- No real-peer gameplay acceptance has been performed.

## Packaging

Run `package/pack_2155.ps1`, then `package/verify_2155.ps1` under PowerShell with
execution policy Bypass. Packing requires the qualified DLL and unchanged production
inputs. Verification checks embedded DLL/documents, manifest, icon and archive
layout, then copies the package and GitHub notes into the outer workspace `dist/`.

Deliverables: `WatchersOfTheVoid-0.9.2155.zip`, `SHA256SUMS-2155.txt` and
`GITHUB_RELEASE_2155.md`. The ZIP uses the existing Unreal shimloader dependency,
branding, paks, license and credits. No upload, publication or game installation
is performed by these scripts.

## Remaining acceptance

The eight event categories still need full native trigger/outcome/effect/phase and
join coverage. Rate control covers five verified generators, not the complete random
event list. Test event/frequency changes, simultaneous triggers, late join and
reconnect with real peers using identical b2178 DLLs.

The owner confirms Bench58 happened to the host's **old rifle**. Decoding the existing
`prop_rifle` leaf Blueprint finds a destructive LMB-use path, without a placement
entry point; this does not exclude inherited behavior or prove the cause. Reproduce
simple placement and capture the rifle's key, birth/destroy caller, role and timing.
The existing `VOTVCOOP_PROP_BIRTH_KEY_PROBE=1` probe records births and their destroying
class on either role; enable it before launching the reproduction. Also retain
`[PROP-DROP]`, `remote_prop::OnDestroy` and `join_membership_sweep` log lines.
Client recovery protection is a separate fix. Token52 remains unidentified.
Received-destroy identity guards also cancel work after actor replacement and add
role/sender/class/key diagnostics; they do not establish the reported runtime cause.

Trowel persistence requires exact identity; mismatches remain deferred. Test native
`updateHold` reconstruction, unequip, drop and reconnect before closing continuity.
Paid repairs and general personal-resource debit/refund remain open, as do the
remaining grime sources and scorch provenance across a full world reload.
