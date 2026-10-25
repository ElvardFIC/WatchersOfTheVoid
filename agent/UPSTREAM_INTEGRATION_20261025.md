# Upstream integration

Integration target: VOTV-MP/Multivoid `ad5315611` (25 October 2026).
Fork source baseline: `d00610643`; origin reconciliation: `68f0c2a7f`.
Later local native-action/hit-result fixes from `ddc711220` are merged before qualification.

## History

After fetching upstream, the fork had 607 graph commits ahead and 923 behind.
Upstream rewrote the history that previously ended at `93dd26cc2`; the
corresponding rewritten commit is `df76e6f66`. Their trees differ in 15 files,
primarily the removal of the obsolete private port-preparation gate.

The ordinary merge chose `1afb8c2ef` and reported conflicts across old changes
that both trees already contain. The integration instead uses the preserved
pre-rewrite shared commit `2276a14c8` as the three-way content base. This gives
53 conflicting paths, including documentation and translations. The merge
records both real branch tips as parents; neither branch's history is reset or
rebased. Original upstream authorship remains in its commits.

## Combined behavior

- Keep 32-peer admission, the remembered host selector and the matching master
  capacity. Adopt upstream's additional UI, localization, device and source fixes.
- Use upstream's host-judged ATV seat/syncer table, native keys, complete-set join
  reconciliation, trap/explosion requests and stream gates. Retain the fork's
  second-ATV placement/collision checks, save-key collision recovery, condition
  servicing, cockpit/modules, radio URLs, steering and quiet join snapshots.
- Import parked-creature presentation, hit forwarding and creature-hit judging.
  Retain the fork's PvP path, foreign native-state guards, full NPC rotations,
  Kerfur attachments, cosmetic state and sit presentation.
- Import shared-prop request/reservation/reply/commit handling and its consuming
  verbs, notebook/paper editing, lossless text codec and related native wrappers.
  Retain the fork's placement/birth/hand identities and native restore guards.
- Apply notebook class gates at the fork's owned record queue. Publish a client
  record only after successful native application; unknown or held targets wait.
  Preserve safe unlocks, non-refillable cans, grill runtime, exact source/class
  checks, queue revisions, reentrant replacement and birth-author waits.
- Use one native explosion capture/request/application pipeline. The previous
  fork blast controllers remain available to isolated regression fixtures but
  their runtime Install/Tick paths are superseded. Keep the fork's native radial
  collision/filter target discovery, including force-only blasts and previously
  untracked props. Both coast feeds enter the same idempotent prop-drive lane.
  The PC minigame's explosion remains local: no dissolve, broadcast or parking.
- Keep bounded drone queues, source/occupancy checks and absolute legacy timer
  requests. New timer presses toggle the host's current value, so two queued
  presses restore the original setting. Include upstream's scan-progress path
  without suppressing the fork's pending verdict/reply retries.

## Wire identity

Protocol/build: **2215**, game target **0.9.0n**. All peers must use this DLL.
All 226 published fork kind values remain reserved. Scalar `PlayerDamage=20`
and optimistic `AtvRelease=71` are explicitly refused; their replacements have
distinct values. Fifteen upstream additions occupy **238–252**, including
PlayerHit, PropActRequest/Reply/Commit, CreatureHitIntent, ExplosionSpawn,
ProjectileFired, ExplosionRequest, SleepingbagIntent, AtvExplodeRequest,
PropIgniteIntent and the four new ATV table/set messages. The existing 152-byte
ATV payload retains steering/cockpit fields and now carries the host's syncer.

## Verification

`package/check_current.ps1 -BuildDirectory build/integration
-EvidenceDirectory build/upstream-qualified-final-2215` records the full aggregate,
stable production-source snapshot, Release build and DLL ABI evidence in the
integration worktree. Individual logs and exact exit codes are retained there.

`run_upstream_integration.ps1` tests the actual blast receive validator and drone
and ATV condition controllers, plus the independent published-ID baseline. `run_atv_steering.ps1`
now extracts the actual complete-set sender and checks steering and retained
honks through joins. `run_item_records.ps1 -FaultControls` covers the merged
record lifecycle, denied class gates, held-target retries, delayed births and
source replacement, with eleven mutation controls.

Offline and cooked-contract checks do not establish rendered multiplayer
acceptance. No game installation, distribution archive or master deployment is
part of this integration. Existing gameplay acceptance items remain open.
