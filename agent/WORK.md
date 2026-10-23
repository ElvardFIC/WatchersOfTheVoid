# Unified Work — unresolved backlog

Current development: **DLL b2197 / protocol 2197**, game **0.9.0n**, targeting
package **0.9.2159**. Full-record/container/pickup regressions and 15 fault controls
pass. Release/ABI and **74/74 offline groups** pass against stable sources.
Native blast/ATV evidence was recorded on earlier DLLs. Rendered gameplay and
real keyboard input still need acceptance.

Last prepared manager package: **0.9.2158 / DLL b2186 / protocol 2186**.
Its Release, 68/68 offline groups, public-version formatter and archive identity
checks passed. It does not include the b2187–b2197 fixes; no 2159
archive is prepared. Full-package startup and rendered multiplayer acceptance remain open.
Completed implementation/release reports and superseded build instructions have
been removed here; verification history remains in [TESTLOG.md](TESTLOG.md).
Priorities and detailed report checklists: [TODO.md](TODO.md).
Native entity gaps: [ENTITY_COVERAGE.md](ENTITY_COVERAGE.md).

## Version 0.9.2159 — record state and dispatch

Full-record carriers in **b2197 / protocol 2197** validate actor/class/world,
function and codec lifetime through capture/restore without reading a prop key.
Saved key replacement remains legal. Base-container extraction also validates
its owner, caller function and selected carrier across native work. Failed or
invalidated restoration refuses pocket capture and clears both success outputs.
Focused production tests and 15 codec/container/pickup controls pass:
`build/container-lifetime-qualified/` control logs. Stable Release/ABI and **74/74 groups**
pass: `build/container-lifetime-qualified/verification.json`.
The native row is removed before this seam. Retained-record retry or container
refund after failed restoration remains open; pickup refusal is not a complete
resource transaction. Current-DLL gameplay also needs acceptance.

Development **b2196 / protocol 2196** validates actor/key/class/world and cached
function identities through prop record capture and immediately before loadData.
The receiver passes its row/authority guard into the native codec. Superseded
native reads and resolver callbacks cannot dispatch the old record; replacement
rows remain queued. Codec reset cancels active work without retaining cache-row
pointers/iterators across native calls. Interrupted convention probes retry and
same-class recursive probes defer until a complete measurement succeeds.
Focused production regressions and seven codec/eleven receiver controls pass:
`build/record-dispatch-qualified/codec-controls.log` and `record-controls.log`.
Release/ABI and **73/73 groups** pass against stable production sources:
`build/record-dispatch-qualified/verification.json`.

Initial-author admission and capture readiness (b2194–b2195) remain implemented:
only applied author state releases the wait, including early arrivals and retries;
matching parked initial state can drain before pickup, while a new wait defers
capture/mutation. Historical qualification and reproduction evidence are in TESTLOG.
Cross-peer writer arbitration, debit/refunds and rendered acceptance remain open.

## Version 0.9.2159 — material publication revisions

Development **b2193 / protocol 2193** adds synchronous `PublishWhile` to the
existing record publisher. Material publications check exact stock fields, row
revision and world/actor/hand eligibility through getData and chunk sends.
Exact reads also catch nested native changes whose POST skips observation under
our dispatch guard; ordinary sampling thresholds are retained for dirty detection.
Superseded captures are refused; dirty work stays with the controller for a fresh
attempt. This path never stores the callback or queues an unguarded background
retry after capture/send refusal. Ordinary publishers retain their existing retries.
Production regressions reproduce the pre-fix stale water publication and cover
water/concrete/bag/trowel/sponge changes, hand transitions, host/client callback
cancellation and fresh retries. Four expected-failure controls pass:
`build/material-publish-qualified/fault-controls.log` and `stock-control.log`.
Release/ABI and **71/71 groups** pass against the exact-field correction:
`build/material-publish-qualified/verification.json`.
Already delivered chunks/records cannot be recalled. Cross-peer writer arbitration,
debit/refunds and rendered gameplay acceptance remain open.

## Version 0.9.2159 — grill lifetime and delivery

Development **b2191 / protocol 2191** binds ordered coal/lid work to the existing
session-start serial. Reusing the Session pointer, world and roster no longer
executes a queued old edit, resends a client request or keeps old terminal/replay
history. Native callbacks also stop immediately if a restart occurs before the
next Install/Tick; reinstalling inside a callback invalidates its old epoch.
Production regressions reproduce the pre-fix stale coal increment, verify fresh
work after restart and reject two controls that remove queue reset or immediate
callback validation. Evidence: `build/grill-session-qualified/fault-controls.log`.
Release/ABI and **71/71 groups** pass against stable production sources:
`build/grill-session-qualified/verification.json`. Personal-resource debit,
refunds, disconnect accounting and rendered acceptance remain open in TODO.

Development **b2192 / protocol 2192** adds internal revisions to held-grill rows.
An old native completion cannot admit a replacement with repeated wire identities;
an interrupted send ends its old fan-out and retains the replacement's delivery
debt. Recipient retirement also invalidates an in-flight acknowledgement when
the player number repeats. Local native reads cancel if their owner row retires.
The existing session-start serial clears hand rows/history and stops old loops;
callbacks and cleanup batches cancel immediately across another session start.
Production regressions reproduce the old false admission. Seven expected-failure
controls, production compilation and **71/71 groups** pass with stable Release/ABI.
Evidence: `build/grill-hand-qualified/verification.json` and `fault-controls.log`.
Wire payloads and the native resource transaction remain unchanged; gameplay
acceptance stays open.

## Version 0.9.2159 — explosion prop motion

Development **b2190 / protocol 2190** extends b2189 motion replication to
zero-damage blasts, which skip the native damage-overlap loop. Both gascan and
general controllers register accepted host-authoritative blasts independently
of damage, including client-requested authoritative replays. The native
`RadialForceComponent.FireImpulse` post hook queries that component's world sphere
and object-type filter before physics integrates positions. Native collision
overlap also finds large props whose actor origin lies outside the sphere.
The next gameplay tick enrolls targets through `Coast` before the drive publisher,
which owns movement and the reliable final rest edge. No synthetic impulse or
timer estimates are used; previous damage-overlap targets remain a query-failure fallback.

The bounded feeder rejects held/frozen/static and unsupported trash targets,
deduplicates each source/victim pair, validates cached object identities and
cancels stale world/session work. Regressions cover delayed/repeated impulses,
pickup, capacity, native source destruction and reentrant teardown. Both existing
blast controllers test host-only enrollment, including zero-damage births/replays.
The production wrapper tests native sphere/filter parameters, copied input arrays,
engine-owned output cleanup and refusal after native callbacks invalidate context.
Focused tests, five expected-failure controls and **71/71 full groups** pass with
stable Release/ABI: `build/force-only-qualified/verification.json`.

Exact-DLL native LAN confirms a host gascan with **damage 0 / force 5000** moves
an existing bucket 541.02 cm, emits the reliable rest edge and converges on the
live/rejoining client within 0.1 cm. Both compatibility reports are HEALTHY and
the native query reference matches. Evidence: `build/force-only-qualified/native.json`.
The Lua fixture sets damage/debris/fire to zero before native BeginPlay on a
disposable profile; it does not establish rendered/input acceptance. Earlier
ordinary-blast evidence remains in `build/explosion-motion-qualified/native.json`.

Remaining: broad host/client blast physics, object-type filters, simultaneous
impulses, pickup/handoff and late join. The existing cooked corpus was reused;
the broader audit stays deferred.

## Version 0.9.2159 — ATV steering and join sound

Development **b2188 / protocol 2188** sends the actual handle yaw in the ATV state
packet, using the former cockpit padding for signed hundredths of a degree. Size
remains 152 bytes; the protocol bump rejects peers that read it as padding.
Mirrors apply handle rotation and the native front-wheel constraint reducer after
the ATV's own tick, which otherwise recomputes local steering between packets.
The local driver/carrier keeps ownership; cached targets are invalidated on
actor, world, session or occupant replacement and retry native readiness.

The late-join snapshot no longer clears a pending honk or plays that old edge
for the joiner. `run_atv_steering.ps1` tests the actual producer, join function,
mirror controller and wrapper. Both missing-steering and honk-clearing negative
controls fail their intended assertions. Two live-captured function references
verify the native tick/constraint contract; the fact-row gate passes 511 rows.
Full qualification passes **70/70 groups**, stable Release b2188 and identity/ABI:
`build/atv-steering-qualified/verification.json`.
Exact-DLL native LAN passes six left/right/center cases in both directions and
nonzero steering at late join; both compatibility reports are HEALTHY. The fixture
sets native driver/input fields; rendered/keyboard acceptance is separate.
Evidence: `build/atv-steering-qualified/native.json`.

Remaining: rendered host/client steering and join/handoff acceptance. The
prepared 0.9.2158 package stays b2186; no new archive or publication is prepared.

## Version 0.9.2159 — bleeding synchronization

Implemented in development **b2187 / protocol 2187**: observe only
`effect_bloodLoss_C` calls to `bloodDecalMaster_C.addParticle`, send the native
drop parameters to the host, validate source identity, position, bounds and rate,
then forward to ready observers while excluding the injured owner. Receivers
invoke only the blood manager; local damage and viewport blur remain with the
injured player. Short bounded retries expire on world/session/occupancy changes.

`run_blood.ps1` passes production controller/wrapper regressions and verifies the
native contract against a two-function live 0.9.0n capture. The added function
reference passes the fact-row gate. Full qualification passes **69/69 groups**,
stable Release b2187, identity and loader ABI; evidence:
`build/bleeding-qualified/verification.json`.
Exact-DLL isolated NullRHI startup passes HEALTH and the new reference matches;
the census reports zero problem rows. Evidence: `build/bleeding-qualified/native.json`.

Remaining: rendered host/client injury acceptance, stopping bleeding, mid-bleed
joins and refusal/reconnect behavior. New joiners see subsequent drops; old drops
are not replayed. Native collision, splat audio and stains are simulated per peer;
identical stain positions, historical-stain joins, cleaning parity and full-save
persistence are outside this implementation and remain unverified. Package
0.9.2158 remains b2186; no 2159 archive or publication has been prepared.

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

The latest fact-row gate passes (513 rows/165 files), superseding the older missing
reference and unused-row findings. Three public-prose counters remain:
documents 85/82, unresolved source citations 155/0 and
src.files_not_swept 84/0. Detailed results are in TESTLOG.

Full-manager-package startup, rendered gameplay, larger lobbies and remote master
deployment remain unverified. Binary publication is pending. Requalify after source changes
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
