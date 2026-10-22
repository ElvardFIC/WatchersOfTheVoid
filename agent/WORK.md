# Unified Work — unresolved backlog

Current local test package: **0.9.2158 / DLL b2186 / protocol 2186**, game **0.9.0n**.
Release, 68/68 offline groups, public-version formatter and archive identity checks
passed. Native package startup and rendered multiplayer acceptance remain open.
Completed implementation/release reports and superseded build instructions have
been removed here; verification history remains in [TESTLOG.md](TESTLOG.md).
Priorities and detailed report checklists: [TODO.md](TODO.md).
Native entity gaps: [ENTITY_COVERAGE.md](ENTITY_COVERAGE.md).

## Version 0.9.2159 — bleeding synchronization

- [ ] Synchronize blood after severe injuries: the host and other clients must
  see the injured player's bleeding as well as the injured client.
  Verify injuries to both host and client and blood visibility on the other peers.

## Immediate-join reports and larger lobbies

Watch registration capacity, flashlight readiness/retries and exact-recipient
container seed delivery are implemented. Both hosting UIs and admission support
2–32 seats; acknowledgements, recipient masks, held-food addressing and join
storage have been extended. Implementation boundaries:
[SYNC_2157_FIXES.md](SYNC_2157_FIXES.md).

Remaining: ordinary host/client acceptance of player flashlights, light switches,
garage/base doors and client drone contents immediately after joining. Verify
larger groups in game and deploy the matching master before automatic lobbies
above four seats. No remote master deployment is recorded.

## Selected upstream adaptations

Remaining: rendered host/client acceptance of vital quantization, late prop/trash
class resolution, fossil-hound yaw, NPC applied-pose caching and plain-wisp landing.
The implemented subsets and deferred protocol/transaction conflicts are recorded
in [FORK_AUDIT_20261022.md](FORK_AUDIT_20261022.md). Upstream ancestry was not merged.

## Ragdoll and stationary-PC acceptance

The C-key launch, selected-model setup, articulated remote display, entry momentum,
camera rebasing, immediate physics start and recovery heading have implementation
and native/offline evidence in TESTLOG. Remaining for reports 5/50: ordinary
rendered/input acceptance on both roles, standing/crouched/repeated entry/recovery,
running, slopes/moving surfaces, local bed/chair transitions and uninstalled rigs.
Investigate intermittent incomplete remote limbs if reproduced. Optional authored
PhysicsAsset/display-retargeting work remains in TODO; prior DLL qualifications
do not establish acceptance of the current build.

For stationary-PC loading (54/55), desktop-only snapshots preserve native boot and
power pages; host leases/revisions and final-edit retries are implemented. Remaining:
actual startup on both roles, operator handoff/final edits, entering/leaving during
boot, repeated ON/OFF, power loss/restoration, late join and reconnect.

## Other unresolved implementation and investigations

The detailed open lists and original report IDs are in TODO: host old-rifle
vanishing (Bench58), probable shared-balance Token52 acceptance, trowel key repair,
atomic personal-resource spending/output/refunds and paid repair, concurrent
resource writers, remaining console/toy/entity behavior, package/food outputs,
dynamic grime and full-reload scorch provenance, sky-event client phases,
explosion impulse follow-ups, stow performance and model previews/compatibility.
The fish integration's provenance and implementation boundaries remain in
[HANDOFF_FISH.md](HANDOFF_FISH.md). Imported controllers still require the
corresponding two-player acceptance checks in TODO.

Report IDs 1–58 remain tracked in TODO except closed Mining (6) and owner-excluded
Wall41; 11/36 describe the same chair issue. Previously working nightmares (10)
need acceptance of the newer personal-sleep behavior. Use identical current DLLs;
record role, item/model, reproduction and logs in TESTLOG before closing reports.

## Validation and delivery follow-ups

The latest fact-row gate passes (508 rows/160 files), superseding the older missing
reference and unused-row findings. Four inherited public-prose counters remain:
documents 84/82, naming 26/24, unresolved source citations 155/0 and
src.files_not_swept 84/0. Detailed results are in TESTLOG.

Native startup, rendered gameplay, larger lobbies and remote master deployment
remain unverified. Binary publication is pending. Requalify after source changes
using the current package scripts/metadata. The fresh game-pak/Blueprint audit
remains deferred by the owner; the existing corpus is incomplete for the imported
native-contract suite.

## Requested event synchronization and frequency

Package 2155 improves the shared infrastructure without closing any complete
category: replay owns queued work across native callbacks, limits recursive work,
retries preparation failures and consumes ambiguous attempted native calls. Active
join snapshots retain readiness/send debt, copy native membership, validate sender
serials and cancel on session/world/recipient replacement. Offline lifetime and
reentrant tests pass; native effects and phase parity remain category-specific work.

Development b2178 additionally binds location-event state to the session-start
serial, including reuse of the same Session pointer/world. It validates actor
identity between native location/rotation reads and cancels gathering after native
callbacks tear down or replace the world/session. The location tick refuses
recursive entry. Production tests first failed on the old session-reuse behavior,
then passed with native pose/query teardown and real-overlap cancellation cases.

The eight-category request remains open. The owner deferred the fresh
game-pak/Blueprint audit on 21 October; the old D: installation is unavailable.
Keep that audit deferred. Do not infer native mapping from a wiki name or close
an event from its actor's transform route.

| Requested category | Remaining implementation or acceptance |
|---|---|
| Dreams | The native local sleep roll, wakeup/createDream entry, dream phase, and leaveDream/awoken return remain peer-local; SleepState carries rest/dream presentation only and DreamScene is ignored. Verify every named scene's actor effects, wake rewards and join/lifetime behavior on peers. Preserve independent nightmare settings and Insomniac conditions. |
| Ariral Reputation | Audit every reputation tier/case, shared reputation, randomized effects, bed/player relocation and late join. |
| Player-Activated | Map and verify every listed interaction, simultaneous activation and canonical side effects. |
| Time-Related | Audit real-calendar/holiday and 3AM branches, local calendar rolls and uncovered effects. |
| Endings | Establish and verify each ending's per-player versus session consequence; do not replay quit/travel on remote puppets. |
| Story Mode | Complete actor/phase/audio/bookkeeping adapters and current-state late join; start-of-event replay does not prove an active-phase snapshot. |
| Random | Audit the complete named list and remaining timer/probability sites. Black-fog client suppression alone does not mirror black fog. |
| Signals | Audit each processing/playback trigger, funny-rule requirement, chosen outcome, phases and late join. |

Frequency coverage now includes the five verified
`ticker_fossilhoundSpawner`, `ticker_yellowWispSpawner`, `ticker_wispSpawner`
and `ticker_egSpawner`, plus owner-local `ticker_eyers` routes.
The existing night-eyes Blueprint confirms a native 1800–2700s re-arm, 5% roll
and day/night gates. Only the wait is scaled; the existing owner-entity route
remains responsible for its creature. Host owns the first four generators.
The verified peer-symmetric `ticker_fireflySpawner` route is also frequency-scaled
on every peer; its local-camera eligibility/roll and emitter capture/replay remain
native. The shared setting changes only its tick wait.
`treeSpawner` (600–900s walking-tree output), `bp7Spawner` (camera-dependent rare
integer roll) and `flickerer` (power/generator effects) were inspected in the
existing corpus but not admitted without complete authority/effect routes.
Other repeatable random-event candidates remain to be inventoried. This is not a
fresh pak audit.
Preserve native eligibility, probabilities,
personal dream rolls and story dates; adjust eligible-roll intervals without
changing AI/animation, effect durations or sleep needs.

Real-peer acceptance must cover chosen/repeated events, simultaneous triggers,
live/persisted frequency changes, load windows, late join, reconnect and teardown.
