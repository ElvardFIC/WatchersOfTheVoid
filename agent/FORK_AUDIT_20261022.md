# Refreshed fork review — 28 October 2026

The inventory below is historical. A fresh default-head review now enumerates
3 direct WatchersOfTheVoid forks and 44 publicly returned upstream forks
(46 other repositories after deduplication; 45 reachable). Marlore remains
unreachable. Exact heads and the current-upstream comparison are preserved in
build/fork-review-20261028/{forks,heads,comparison-current-upstream}.json.

The new direct fork gxrxrt/WatchersOfTheVoid, default fix/coop-stability at
fee106791, contributes 31 commits outside our branch/current upstream.
Five source adaptations are now implemented: generator_C watch ownership,
player-body prop coasting, exact slapper Character/source admission, and
readiness/lease-aware bounded inheritance for laptop and shared-screen state.
Per-item provenance, excluded changes and verification are in the current
[WORK](WORK.md) and [TESTLOG](TESTLOG.md). The fork's other wire kinds and
protocol 2903 were not merged into protocol 2217.

gv1nd's capacity fix remains superseded by our 8192-slot table; mega-maks2011
still contributes README text only. LoKoGoRus's new five-seat patch is
superseded by the existing 32-peer implementation. Several fresh fork heads
are identical to upstream snapshots and add no independent fixes. The newly
reachable rosonoka head retains pelmentor's older rewritten upstream history.
This review covers publicly returned default heads, not unpublished changes
or every nondefault branch of the upstream network.

# Selected upstream fixes

Compared fork `21941a2df` with VOTV-MP/Multivoid `93dd26cc2`: 39 graph commits
ahead and 137 behind. The changes below adapt selected source changes; they do
not merge upstream ancestry. GitHub's behind counter will therefore remain.
The full commit inventory and disposition is in
[UPSTREAM_COMMITS_20261022.json](UPSTREAM_COMMITS_20261022.json).

## Applied

| Behavior | Upstream provenance |
| --- | --- |
| A positive health/food/sleep fraction cannot round down to the empty byte; NaN encodes as zero. The packet layout stays unchanged. | `49d8401fe`, NaN part of `3ec8d7ad9` |
| Missing prop and trash bases resolve through the class index, retry after loading, and avoid game-thread caches on animation workers. A definitive trash verdict is cached only after the required bases resolve. | Final resolver from `6b46b3544`, `13e7193eb`, `82a28b0a4`, `873cee943`, `6a359b360`; the verdict guard from `8901caf18` was already present |
| Fossil hound mirrors receive the host mesh's world yaw through the existing body-yaw field. The bounded class-name check retries NAME_None and accepts subclasses. | Yaw subset of `2241217a3`, `853e058b1`, `1013319b2` |
| NPC transforms are cached only after both engine writes succeed; failed writes retry on a later tick without needing another packet. Identical poses skip transform writes. The fork's pitch and roll join the cache key. | Restore/adapt existing upstream `714c2167f`, `1e488fef2`; these are ancestors, outside the 137 |
| Plain grounded wisp mirrors replay their native landing fade and retry a refused call. Existing mirror marking is now connected to its apply function. | Restore the previously inherited `17cde3039` landing path, whose unused wrapper was removed by `0933b0f5a` |

Source authorship: the selected 137-commit changes are by **pelmentor**
(`pelmentr@gmail.com`), with their original **Claude Opus 5.5** co-author trailers.
Original commits remain available in the upstream remote; imported code is left
as reviewable working-tree changes, without rewriting history or assigning those
commits to another author.

The NPC adaptation retains the fork's full rotations, Kerfur carried props,
reports disc, cosmetic state and ATV sitting. No protocol version change is
needed for these changes: all message numbers and payload layouts remain intact.

## Deferred integrations

| Group | Concrete conflict |
| --- | --- |
| Parked creature graphs and hit judge | Upstream `PlayerHit=20` changes the semantics of the fork's `PlayerDamage=20`. Its brain parking requires hit forwarding and attack delivery together; copying only parking could remove attacks. Reconcile the fork's PvP, impact interception and player-state guards first. |
| Shared prop take/use reservations | Upstream request/reply/commit kinds 178/179/180 collide with `KitchenIntent`, `CordState`, `CordIntent`. Reservations, cancellation, commits, disconnect cleanup and every consuming verb need one integration across our birth/placement/hold identities. |
| Explosion copies and coasting | Upstream `ExplosionSpawn=177` collides with `KitchenState`. The fork already has `ExplosionEvent=188`, `Explosion=191`, source identity, deduplication, player damage ownership and prop-drive/scorch paths. A second explosion pipeline would risk duplicated effects and damage. |
| Drills and fingerprints for those groups | Depend on the deferred modules and their precise dispatch rows; they cannot prove the fork's existing lanes by themselves. |

The other fork candidates remain as assessed in the original inventory below.
In particular, the drive-rack
mass-removal rebase hides an unexplained state reset and could discard real
removal operations. Loading TTL and class ownership are already present here.

## Verification

New runner: [run_upstream.ps1](tests/run_upstream.ps1), included in
`package/check_current.ps1`. It exercises actual quantization and hound code,
and production prop resolver bodies under delayed loading and worker calls.
The extended entity fixture checks stationary pitch changes, identical re-sends,
failed final writes, hound yaw and wisp landing retry/one-shot behavior.

PASS 68/68 groups and Release build; production inputs stayed stable, with 786
build source entries. Evidence: `build/upstream-qualified/verification.json`.
The fact-row gate passed. Public-prose checks still fail on inherited HEAD counters;
this import adds no rule regressions and does not relax their baseline.
Rendered gameplay with two peers remains unverified; no installed game files,
published package or remote service are changed by this import.

---

# Fork and upstream review — 22 October 2026

Reference checkout: `db0f59a4916dcdc844162cddbe09a260147ce1a3`.
Upstream: `93dd26cc20444b2a72ab5c09df2e00f19f0c954a`.

## Scope and evidence

Enumerated 38 default branches in the upstream fork list, including this repository,
and both direct forks of WatchersOfTheVoid. Fetched 39 of those 40 branches;
`Marlore/temp-mail-generator` was listed by GitHub but its repository fetch returned
not found. The review covers those default branches, not unpublished or other
feature branches. Older forks retain upstream history that was subsequently
rewritten; graph-unique commits in those forks are not all original contributions.

Local fetched heads and commit lists: `../_research/fork-audit-20261022.json`.
Fork-list metadata: `../_research/forks-20261022.json`.

## Direct forks of this repository

| Fork | Finding | Disposition |
|---|---|---|
| [gv1nd](https://github.com/gv1nd/WatchersOfTheVoid/commit/937110bac) | Enlarges the script-name watch table to 4096 slots; also changes workflow routing and allows a registry gate to fail without failing the build. | Confirms the startup-capacity problem. This continuation uses 8192 slots at the original 3/8 maximum load and tests the production registration implementation with 1100 pending/resolved watches and eight reopens. The old budget fails the same fixture. Gate weakening is not adopted. |
| [mega-maks2011](https://github.com/mega-maks2011/WatchersOfTheVoid/commit/38b310c48) | One README change. | No synchronization implementation to transfer. |

The capacity correction was developed here after finding the local release notes'
overflow report; the independently published gv1nd correction corroborates it.
No fork commit is represented as a new commit by its original author unless it
has actually been imported with that authorship.

## Useful changes in the upstream fork network

| Source | Finding | Current relevance |
|---|---|---|
| [AwazonDotCom / DexxInTime](https://github.com/AwazonDotCom/Multivoid/commit/10383e817) | Preserves control characters and newlines in disk/hash-code strings instead of using chat sanitization. | The current checkout already has lossless disk wire conversion and a disk UTF-8 regression. Preserve that behavior; importing a second encoder is unnecessary. |
| [AwazonDotCom / DexxInTime](https://github.com/AwazonDotCom/Multivoid/commit/aa3588ad2) | Holds rack updates through loading and separates rack contents from generic save-record application; also rebases copied-drive mass. | The join hold and class ownership are already present. The copied-drive mass step is a candidate for a separate native rack audit; no gameplay result is inferred from the patch. |
| [Shinobu2](https://github.com/Shinobu2/Multivoid/commit/9eec3cf37) | Host birth records, prompt destroy delivery, held-mirror tracking and replacement of older parked records. | Current main already contains the corresponding birth/save-record and destroy-lifetime mechanisms, with later identity and callback guards. Preserve these rather than replacing them with the older versions. |
| [Dev-prizrakk](https://github.com/Dev-prizrakk/Multivoid/commit/b5773697f) | Sleeping bags and cargo lift; a second change keys the cremator door through its machine. | All three areas already have dedicated implementations in this checkout. Their recipient masks are extended for 32 seats in this continuation. |
| [Dev-prizrakk](https://github.com/Dev-prizrakk/Multivoid/commit/06b318d05) | Grab/adoption/sleep and blob-transfer corrections. | Useful historical checks. Current code contains later lifetime, byte-count and ownership protections; wholesale replacement would discard them. |
| [RiceMCUT](https://github.com/RiceMCUT/Multivoid/commit/60d2fefe6) | Eight-player lobby capacity, including host UI and master listing changes. | Useful capacity checklist. It predates the current per-peer acknowledgement payloads and cannot be copied as a complete 32-player implementation. Current changes cover admission, payloads, recipient masks, held-food addressing and both hosting UIs. |
| [tarasenkoroman9090-sketch](https://github.com/tarasenkoroman9090-sketch/Multivoid/commit/8e95e9561) | Restores cached host ATV fuel when a client mounts an empty vehicle; another commit periodically resends dish hashes. | Investigate the underlying authority/first-state issue before adopting either patch. A cached fuel rewrite can race legitimate consumption; periodic resend alone does not identify the lost publication. |
| [Cairn-client](https://github.com/Cairn-client/mod-votv) | Distribution pack, loader pin, branding and removal of skin packs. | Packaging reference, with no fix for the reported synchronization lanes. |

Other reachable default heads either add no graph-unique commits against the two
current references or mainly retain historical upstream/packaging changes.

## The 137-commit upstream gap

`git rev-list --left-right --count main...upstream/main` reports **35 / 137**
at the reference checkout. GitHub's behind count is correct. It measures ancestry,
not how many independent fixes are absent from the customized source tree.

The 137 commits cover these substantial groups and their associated drills:

- Parked-creature presentation and damage delivery, player-hit validation and
  small positive vital fractions (`240c9e092`, `dde16fbec`, `8915a7afc`, `49d8401fe`).
- Host-authorized shared-prop take/use reservations and commit/retry behavior
  (`90019da38`, `0e57ab2cf`, `ddd3caa98`), plus class-resolution improvements.
- Host explosion capture, client cosmetic application and post-blast prop coasting
  (`ff2740584`, `6517ab8ba`, `0f8083f81`, `13ffd28af`), with additional native drills.

A read-only merge preview reports **15 conflict entries**, including protocol,
dispatch, damage, NPC presentation, prop destruction, routing and build sources.
The preview is `../_research/upstream-merge-preview-20261022.txt`; it does not
change the working tree or record a merge.

The upstream ExplosionSpawn value **177** conflicts with this fork's
**KitchenState=177**. Existing fork explosion messages occupy their own lanes.
Importing this group requires explicit message allocation, authority and damage
review, not just accepting one side of the protocol conflict. The prop transaction
group also overlaps the fork's placement/held-item protections. The 137 commits
have therefore been reviewed as candidates, not merged by this bug-fix batch.

## Validation boundaries

The 32-seat client implementation needs matching protocol **2185** DLLs.
The master source changes `players_max` admission to 2–32; an already deployed
master retaining its old four-seat clamp must be updated before its automatic
lobby join path can support more than four players. No remote service was deployed.
Direct connection reaches the host's configured admission limit independently.

Offline tests and the Release build are recorded separately in TESTLOG.
They do not establish rendered host/client acceptance or 32 simultaneous players.
