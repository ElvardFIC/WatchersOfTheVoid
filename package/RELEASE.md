# Watchers of the Void 0.9.2160

For Voices of the Void Alpha 0.9.0n, Windows x64. Maintained by ElvardFIC and
AquaFishhh. DLL/network build: **b2215 / protocol 2215**.

> **Every version is a test version. It requires extensive testing and may be unstable.**
> Bugs, crashes and multiplayer desynchronization may occur. Automated checks do not
> establish stable gameplay or confirm that every reported issue is resolved.

Import **ElvardFIC-WatchersOfTheVoid-0.9.2160.zip** through the mod manager.
The archive includes the original icon, starter models, enabled loader and
shimloader dependency. Every player must install the same package and game version.

## Changes since 0.9.2158

- Finish the remembered 2-32-player chooser across hosting menus and admission.
- Add shovel-hole replication, can-portion guards, personal sleep voting and
  companion ATV recovery; implement the Ariral ship presentation adapter.
- Improve simultaneous console ownership, queued commands and native display
  callbacks; preserve fresh requests and state through refused sends and teardown.
- Guard inventory restoration and container delivery; recover proven local
  failed extractions without guessing a removed record or replacement actor.
- Strengthen food final-bite and MRE output ownership, native damage preparation,
  saw eligibility and hit construction. Keep unsupported client repair from spending stock.
- Retain bleeding, ATV steering, blast-driven prop motion and material delivery fixes.
- Integrate upstream damage/creature intents, ATV ownership/joins, editing/codecs
  and device/UI changes while preserving this fork's behavior.
- Remove the main-menu tester/reviewer roll and outdated promotional attribution;
  retain the license and third-party notices.

## Verification and limits

The exact production source snapshot passed **94/94 full offline regression groups**,
including 118 local master tests, cooked native contracts and routing controls.
The native-action/hit-result/wall tests additionally pass ten fault controls.
For this package's public-version change, those complete regression results are
reused only after matching the production snapshot and protocol. Release compilation,
loader imports/exports, the generated menu/browser label and archive integrity
are checked again. This is not a fresh rerun of all 94 groups on the new label.

Evidence in the build workspace: `build/release-2160/verification.json`, its retained
reference qualification, `source-before.json`, and `build/package-current/verification.json`.
Five negative package controls reject failed/incomplete/focused qualifications,
stale labels and mismatched DLLs. `dist/SHA256SUMS-current.txt` records delivery hashes.

This is an **offline-qualified test package**. Rendered multiplayer, real keyboard
input, full-manager startup and larger-lobby acceptance remain open. Paid client
repair/resource transactions, uncertain MRE output/refund recovery, ancillary
effects/rewards, remaining event variants and full-save parity are not complete.
Automatic lobbies above four players also need the matching master service;
this package does not deploy it.

Build recipe: `package/build_current.ps1 -ReuseQualification build/upstream-qualified-2215/verification.json`.
Without that option, the recipe runs the complete regression suite.
See CHANGELOG.md in the archive for version history.
