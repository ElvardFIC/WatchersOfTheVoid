# Watchers of the Void 0.9.2165 — offline test package preparation

For Voices of the Void Alpha 0.9.0n, Windows x64. By ElvardFIC and AquaFishhh.
Public package version: **0.9.2165**. Intended internal DLL/network identity:
**b2223 / protocol 2223**.

Qualification is in progress. The 2165 Release DLL, package archive and final
qualification record have not been produced. Build only after the complete
regression suite and both prebuild gates pass.

Current check result: all 163 offline regression groups pass, and `fact_rows_gate`
passes. `public_prose_gate` still fails 13 counters; therefore the build workflow
stopped before Release compilation. The full evidence remains in the local verification log.

> **Every version is a test version. It requires extensive testing and may be unstable.**
> Automated checks do not establish stable gameplay or replace multiplayer testing.

This package is prepared locally and has not been uploaded to Thunderstore.
Every player must use the exact same archive and game version. Matching package
labels alone do not establish matching DLL bytes.

The proposed package carries the current upstream integration and selected fork
changes. Notable work includes guarded save recovery, Kerfur command capture,
sleep/rest state, event cues and actor lifetimes, along with the earlier wall,
toolgun and prop recovery improvements. The full change list is in CHANGELOG.md.

Package qualification must run the current offline regression suite, a Release
build and ABI checks, verify the production package label, and check the written
ZIP against the exact DLL, manifest, icon, starter models and manager layout.
Offline results do not establish rendered gameplay, two-peer synchronization
acceptance or live Thunderstore-manager startup.

Open implementation work includes occupied laptop-seat protection, host-owned
red-phone ring selection, desk virusEvent authority, signal-born world-prop
ownership and runtime 3D-print mesh/content synchronization. Other open
gameplay and runtime acceptance items remain documented in the project
development reports.
