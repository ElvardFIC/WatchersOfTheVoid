# Current upstream refresh checks

`run_handtaken.ps1` covers HD01-HD25 / HX01-HX50 against the actual Omega
controller and service/Registry stack, plus extracted updateHold/grab-release
wrappers and the actual inventory/save-record implementation. Transport, metadata,
engine dispatch, puppet/hand access and clock are doubled. Cases cover retained
recipient debt, FIFO capacity, native dispatch receipts, retryable unattempted
cleanup and replacement across ten callback boundaries. Class-only wire matching
and native multiplayer acceptance remain open.

`run_omega_command.ps1` covers OC01-OC25 / OX01-OX50 with the actual command
controller, services, Registry/Element/claims and extracted native actionName.
Seventy-five cases, 24 assertion controls and three production object compiles
cover service CAS/rollback, host reservations, startKill, staging receipts, sender/
actor/context retirement, owned native text and dispatch accounting. Metadata,
engine, hand/puppet access, transport and clock are doubled. HandTaken ownership
is covered above; exact item correlation and uncertain resource/latent consequences remain open.

`run_served_player.ps1` exercises SF01-SF25 / SM01-SM50 using actual follow
and service controllers, real Element/Registry/claims and extracted native
insert-read functions. Seventy-five named cases, 26 assertion controls and three
production objects cover leases, recipient/context replacement, active-read debt,
arm/watch reconciliation, puppet lookup, exact native results and typed out fields.
Hook/watch operations, puppet access, metadata, engine/transport and clock are
doubled; native acceptance and other Omega command continuations remain open.

`run_kerfus_intent.ps1` covers KA01-KA25 / KB01-KB50 against the actual
command controller, native action wrapper and real Element/Registry/claims.
Seventy-five cases, 25 assertion controls and two production object compiles
cover FIFO refusal retry, sender/target/context ownership, budgeted NoBody waits,
PRE frame/string bounds, native dispatch receipts and guarded watch settlement.
Metadata, reach judgement, follow attribution, engine/transport and clock are
doubled; follow/served-player lifetime and native acceptance remain open.

`run_kerfus_state.ps1` runs KD01-KD25 / KX01-KX50 against the actual state
manager and native wrapper, with real Element/Registry and drive claims. Seventy-five
named cases cover per-recipient retry/readiness, late joins, bounded fair pumps,
checkpointed writes, refresh receipts and replacement during ten native operations.
Nineteen fault controls fail at assertions; both production objects compile.
Metadata, engine dispatch, transport and clock are doubled; gameplay acceptance
remains open.

`run_prop_claim.ps1` exercises LS01-LS25 / LX01-LX50: independent source masks,
unique/context-bound tokens, partial/final release, hand/handoff/election expiry,
first-bind cleanup and legacy isolation. It runs actual core, hook, NPC-cleanup,
Kerfus and extracted watoy/handoff bodies with real Element/Registry code;
unexercised native routes fail if reached. Seventy-five named cases, 23 assertion
controls, 11 production object compiles and nine canonical producer-route checks
cover implementation; engine calls are doubled. Native transform/constraint
priority, contact acceptance and the original jitter root cause remain open.

`run_prop_verb.ps1` covers VA01-VA25 / VX01-VX50 using the actual publisher
admission/verb implementation and extracted client-to-host handoff commit.
Seventy-five named cases, 21 assertion-based mutations and six production object
compiles cover admission identity, per-read cancellation, duplicate/reentrant
opens, owned callback data, canonical keys/EIDs, handoff predecessor receipts and
lineage consumption. The controller, control-delivery and stream fixtures include
the same extracted admission support; native, transport and clock calls are doubled.
Multi-verb leases and native multiplayer/contact acceptance remain open.

`run_npc_pose.ps1` exercises NH01-NH25 / NX01-NX50 against the actual host
pose/hold controller, Registry/Element and extracted atomic transform body.
The named matrix replaces session, world, actor, class or Element at ten native
read continuations. Other cases cover owned batches, prior-row retirement,
MTU fairness, offline lifecycle, two robots holding one prop, hand arbitration,
failed thaw retry and nested teardown. Twenty-three assertion-based fault
controls and four production object compiles supplement the fixture. Engine,
hand state and drive calls are doubled; native contact and multiplayer
acceptance, drive-verb arbitration and the broader upstream audit remain open.

`run_npc_join.ps1` exercises NJ01-NJ25 / NK01-NK50 with the actual sender,
Element/Registry implementation, extracted transform/save-key reads and prop
completion function. The 75 cases cover retained refusals/unbound rows, canonical
bindings, Element and actor incarnations, fresh payloads, 32-slot fairness,
native reentrancy and independent completion receipts. Thirty assertion-based
mutations cover sender scope, pacing/budgets, save-key metadata and snapshot
closure. Production compile checks cover all eight changed translation units.
The sender permits at most 16 delivery attempts per invocation, with at most 12
NPC row attempts and 64 row scans; callback-owned event lanes retain their own budgets. Native materialization,
attack phases/audio, prop contacts and the broader upstream audit remain open.

The full-upstream work adds tower capture/refund and extracted native-spawn
attempt and client ACK-delivery fixtures to `run_tower_ops`; its fault controls
now cover 27 mutations. Paid inserts wait for ACK before another native spend;
refused pulls clean up their captured output and retain unknown receipts.
`run_cord_continuation` also checks recipient delivery and drag receipts; the
real wrapper fixture checks head/component/caller retirement. `run_geom_octa`
checks retained decision delivery, ordered per-recipient birth admission and session
reconstruction. Its extracted actor-transform fixture covers 71 refusal/root/
function/caller/world/schema/finite boundaries with unchanged outputs on failure.
Actual extracted Session readiness methods cover all 32 slots, bounds and eight
concurrent reset writers. Birth receipts also bind its world-ready reset revision,
including reset/ready between pumps and during native/send callbacks.
`-FaultControls` rejects nine birth/transform/readiness mutations at assertions; native
mirror creation acceptance and exact phase joins remain open. `run_vulkan_context`
extracts the actual Present/Destroy functions for a two-thread lifetime fixture;
`-FaultControls` rejects missing serialization independently on both Present and
Destroy. GPU quiescence remains the engine caller's contract; no unsynchronized
queue-idle call is added to Destroy.
Native rendering/contact acceptance, exact Geom phases/joins, full Vulkan
context recovery and the remaining semantic review are still open.

`run_cord_continuation.ps1` compiles the actual cord_sync with 32-peer transport,
object/session identity and native-operation doubles. CP01-CP25 cover owned
publication/outbox/release receipts, session retirement, sender/recipient proofs,
same-world scan preservation, nested replacement, refused retries and clocks.
CQ01-CQ50 cover ten replacement causes at five transport/native continuations.
`-FaultControls` rejects 25 separately compiled mutations at assertions;
`-CompileSources` compiles cord_sync without a DLL. Legacy run_2170 retains real
wrapper/ParamFrame and 4/32-peer checks; run_2145 retains 200-read idle repaint
coverage. UI04/UI05 add per-recipient canonical-state/join delivery and native
drag/head continuations; native acceptance remains open. The runner is included in check_current.

`run_prop_stream_scope.ps1` compiles the actual controller, stream, park, parity
and bind_wait implementations with eight extracted driver row functions.
SG01-SG15 cover guarded ingress, owned immediate/episode proofs, receiver
retirement, park debt, End accounting and failed writes. SX01-SX60 cover six
replacement causes at ten native boundaries. All 75 named cases pass;
`-FaultControls` rejects eighteen separately compiled mutations across the
controller/stream/park/parity seams. `-CompileSources` compiles all five changed
production translation units without a DLL. Native engine, transport, registry,
clock and rider-token operations are doubled. Control/join delivery and rendered
multiplayer contacts remain open. The runner is included in check_current.

`run_prop_publisher.ps1` compiles the actual host publisher and config registry.
HP01-HP25 cover owned snapshots, revision commits, Tick/Pump recursion, session
retirement, owner/root/hand checks, clocks and End retries. HT01-HT50 cover five
replacement causes at ten publication/capture boundaries. All 75 named cases
and integration controls pass; `-FaultControls` rejects sixteen independently
compiled mutations. `-CompileSources` compiles the changed production unit
without a DLL. Native engine, controller and transport seams are doubled.
Control/join delivery and native
contact acceptance remain open. This runner is included in check_current.

`run_prop_syncer_host.ps1` compiles the actual host controller and config registry,
and extracts eight production driver functions for row creation/election/retirement.
PH01-PH15 cover owned batches/input/pass/departure, row/occupant identity, clocks,
bounded admission and detached endings. HS01-HS60 cover six replacement causes
at ten continuation boundaries. All 75 named cases pass; `-FaultControls` rejects
fifteen separately compiled mutations and `-CompileSources` compiles both changed
production units without a DLL. Native calls and transport are doubled. The
retained answer/End delivery, swinger join retries and native contact acceptance
remain open. This runner is included in check_current.

`run_prop_syncer.ps1` compiles the actual client publisher/config registry and
extracts the production EndFromBody capture. PC01-PC25 cover owned episode
state, callback replies, clocks, root/hand handoffs and retained End delivery;
PS01-PS50 cover world/session/actor/class/episode cancellation at ten native
boundaries. All 75 named cases pass; `-FaultControls` rejects fifteen separately
compiled mutations, including End capture and orphan-End retry. `-CompileSources`
compiles both changed production translation units without linking a DLL.
The runner is included in check_current. Engine/transport seams are doubled;
host control delivery and rendered prop-contact acceptance remain open.

`run_tower_ops.ps1` compiles the actual operation controller and config registry.
TQ01-TQ20 cover owned FIFO/overflow work, admission, bounded rotating drains and
spend reservations; TO01-TO55 cover world/session/sender/tower/class cancellation
at eleven native boundaries. All 75 named cases and legal-action controls pass.
`-FaultControls` rejects eleven separately compiled mutations; `-CompileSources`
compiles both production tower translation units without linking a DLL.
The runner is included in check_current. `run_tower_rows.ps1 -FaultControls`
rejects 21 mutations, including operation-helper cancellation and failed-write
continuations. Native calls and transport are doubled; output/refund transaction
completion and native multiplayer acceptance remain open.

`run_physics_native.ps1` checks the actual engine physics wrapper with native
reflection/dispatch doubles: five bounded parameter schemas, class/function
incarnations, finite vectors, refused calls and bounded component discovery.
`run_child_release.ps1` checks the actual deferred child release with controlled
clock, hand, mesh and native callback doubles. NP01-NP45 and CR01-CR30 each have
a named case; each runner's `-FaultControls` rejects 11 independently compiled
faults. Both runners are included in check_current. These checks do not simulate
Unreal contacts or qualify the original joined-client resting-stack report.

`run_tower_wrappers.ps1` compiles both actual tower wrappers and real ParamFrame.
It covers CW01-CW37 / RW01-RW38: layout replacement and extents, bounded arrays,
actor/class/world/function retirement, component bindings, look-at ownership,
recursive interactions, exception restoration and the current gamemode tower.
`-FaultControls` rejects 13 independently compiled mutations for each wrapper.
Reflection, timeline reads and engine dispatch are doubled; native acceptance
is still pending. The runner is included in check_current.

`run_emote.ps1` checks the optional EmoteWheel controller, actual native wrapper,
316-emote/442-phase catalogue and derived native function references. Its
`-FaultControls` switch rejects three separately compiled ownership/hand/source
capture faults. Transport, reflection and engine execution are doubled; native
multiplayer acceptance remains pending. `generate_emote_catalogue.py <manifest>`
regenerates metadata from the supplied 1.0.1 manifest without copying assets.
The external mod is optional for the build and regression runner.

`run_radio.ps1` retains the radio/controller/native-wrapper and cooked Blueprint
checks, adds R01-R45/M01-M30 lifetime/reentry/32-peer checks, and is registered in
check_current. `-FaultControls` rejects 18 additional independently compiled
faults. `-ProductionCompile` compiles only the two radio translation units into
separate objects using the configured Release settings; it records their stable
source hashes and does not link or qualify a DLL. It supports scoped verification
while another session edits unrelated production code. Native gameplay is unverified.

`run_coffee.ps1` compiles the actual coffee intent/state controllers and the
coffee/appliance wrappers with real ParamFrame. It covers invocation nesting,
actor/function/session/world retirement, sender generations, bounded publication
debt and checked action writes. `-FaultControls` rejects 11 injected faults.
Native reflection, roster and transport seams are doubled; gameplay is unverified.
The runner and production queue/bucket suite are included in check_current.

`run_local_release.ps1` compiles the production release edge and Kerfur arming
function against the real protocol and CachedObjRef. It checks PvP nonce bytes
independently of the native throw sequence, robot transfer/one-edge consumption,
actor serial reuse, dead actors, slot 31/32, non-finite/slow/unreadable velocity,
trash suppression and host/client coasting. It is part of check_current.

The item-record and event-slot fixtures now exercise full discs beyond 32 KiB
and refuse data exceeding the blob transport capacity. Existing native apply,
canonical ordering, reentry and lifetime assertions remain in those suites.
`run_event_slots.ps1` also exercises SB1-SB5 host fan-out, refused delivery,
captured publication state and owned join retries against a 32-slot transport
double. The companion claim fixture covers FC01-FC75 across world/session,
actor/class and peer retirement, plus actual claim parsing and refused writes.
`-FaultControls` rejects eighteen separately compiled mutations;
`-CompileSources` compiles the production translation unit without linking a DLL.

The public-prose include suite covers private header context and simple local
macro includes/undef, including cross-unit isolation and unreferenced fragments.
Its eight checks prevent used profile/ATV helpers being removed as dead code.

`run_upstream_containers.ps1` executes the 320 production container codec,
escrow, reservation, take/put, overlap wait/arming and refusal selftest checks,
plus a slot-31 hold lifecycle. Pure decision bodies are extracted from their
production owners; serialization/state/selftest translation units compile directly.
Native lookup boundaries remain doubled, so this is not a gameplay acceptance test.

`run_fork_watch_cache.ps1` executes the real name-watch registration/cache bodies
and registry. It proves that 100000 dead re-ensures do not consume slots, equal
text at another address reuses the entry, and stale cache slots fall back safely.
The cache runner also checks 10000 CDO hits, serial/name retirement, hash-owner
mismatch and retryable misses against the production lookup. Both runners are
part of check_current.

`run_container_apply.ps1 -FaultControls` now targets the integrated container
apply rather than the retired client CAS policy. It preserves allocation/readiness,
packed personal flags, object/store/header identity, newer reentrant input,
sender/session/world retirement, arming callback retirement and commit timing.
Five mutations must fail: unchecked record, stale commit, lost binding, early
completion and lost newer parked row. Native extraction/recovery suites remain.
The migrated seed test executes the split production publisher, seed queue and
retry drain, retaining exact-recipient refusal and slot reuse controls.

The imported portable CI balance, rapid-record and coin drills use current
session/native admission contracts. On Windows they support portable MSVC/Ninja
as well as installed Visual Studio, and write artifacts under build/.

# Regression tests

`run_glowstick_grab.ps1 -FaultControls` executes the actual prop-event wrapper
with production ParamFrame and the extracted production grab-admission helper.
It checks exact glowstick release ownership, ordinary base fallback, readiness,
class/function/actor replacement, world retirement, reentry, disconnect, successor
holds and uncertain dispatch. Six mutations must fail assertions. The cooked
native contract checks releaseGlowstick's complete body and all eleven inherited
variants. This runner is included in check_current; engine constraint/rendered
acceptance remains separate.

`run_prop_drive_lifetime.ps1 -FaultControls` executes the actual prop-drive
receiver, host and bind-wait implementation. It checks ends received before
binding, reordered generations, local/remote hand ownership, reused object
slots, angular-only release velocity, sleeping rest and refused host sends.
It also checks host end ordering, input ownership, native reentry at each end
seam, first/steady pose and interpolation retirement, mesh replacement, nested
ticks and hand-yield successors. Eleven mutations must fail assertions, including
revision, continuation and component-ownership faults. This runner, `run_2124.ps1` and
`run_2126.ps1` are now included in `package/check_current.ps1`.

`run_llama_marker.ps1 -FaultControls` executes the actual marker wrapper and the
placed-save controller. Failed mirror construction has a throttled frame-tail
cleanup path that outlives a stopped session, with incarnation/world/reentry and
successful-adoption checks. Four mutations must fail assertions. The runner also
compiles all four production marker/placed-save translation units.

The supplemental runners use current typed function rows, two-argument save
sinks, actual batch-admission semantics and production peer capacity where
they share the general Session/Registry doubles. The service-panel fixture
explicitly accepts slot 31 and rejects slot 32. Boundaries outside each
fixture's scope are doubled; these are not live-engine multiplayer tests.

`run_2134.ps1` is a compatibility alias for `run_2136.ps1`, not an independent
suite. The obsolete lift Apply/Mirror/Release fixtures were removed after
comparison with their native-replay successor. Remaining request expiry,
departure, callback occupancy, reentry, epoch, parameter-binding and flag-mask
mutation checks were carried into the successor. Physics-loan restoration and
moving-latch release no longer apply: the current wrapper never takes that loan
or suppresses native lift motion. Native Blueprint evidence is in
`blueprint_lift_2136_test.py`.

`run_fork_review.ps1` executes the actual power-grid registration/readiness
fixture, player-body prop contact/coasting, laptop/shared-screen lease and
readable-UI fallback fixtures, plus cooked slapper lineage/spawn checks. It
is included by check_current. Loading widgets and observers cannot spend the
operator fallback budget; native readiness-read world changes cannot publish
an old screen. These offline fixtures do not establish rendered peer parity.

`audit_requested_mods.py` downloads and statically inspects the eleven owner-selected
Thunderstore packages, pins Fusion 0.8108.3, and records archive SHA-256, matching
manifests, entry lists, readable scripts and confirmed integration blockers under
build/requested-mods-audit. `--offline` re-inspects cached metadata/ZIPs without
network access. It never installs or runs downloaded code. Successful inspection
is not multiplayer qualification; every report retains runtime_verified=false.

`run_wisp_animation.ps1 -FaultControls` executes production pending-tear and
grab-hold controllers plus native montage/capture/release wrappers. It covers
body/world/slot retirement, mesh readiness, bounded retry, native callback
reentry, final kill ownership and partial-capture cleanup. Four removal controls
must fail at the targeted assertions. Engine/registry/transport boundaries are
doubled; socket/PhysX/rendered behavior still requires real-peer acceptance.
The host fixture also extracts production pending-destroy/NPC-watch drains,
world retirement, source/victim identity admission and reliable relay admission.
It covers reused ids/bodies, callbacks installing successor work, send refusal,
peer generation changes and send-time deadlines despite slow local cues.

`run_item_icons.ps1` is included by `run_game_modes.ps1`. It exercises empty
`.vimg` cache rejection with real Windows file handles, concurrent-writer
preservation, null texture-array invalidation, Tutorial bypass and actual
quick-slot refresh retry/settlement. Native importer contracts use the cooked
research corpus. The installed game cache is not changed by these fixtures.

The 2162 priority regressions extend `run_flashlight_native.ps1` with extracted
production readers against relocated fields/packed flags. `run_2145.ps1` covers
equipped crank lights and retained on/off state after puppet replacement.
`run_2157.ps1` validates captured dispatch/input ownership and a 32-peer native
presentation budget. `run_watoy.ps1` includes 32-seat presentation/send limits,
fair refused state delivery, readiness retry rotation, original-hand provenance
and shared View ownership/cleanup; fault controls remove the relevant guards.
`run_window_stroke.ps1` checks original native wash material/size at both draw
sites, first-dab settings and retirement during scalar queries. These are offline
native-boundary fixtures; rendered game behavior requires separate acceptance.

`run_game_modes.ps1` covers the six-choice hosting model, actual native new-save
map initialization and mode-application/readback/retry bodies, with reflected
objects and native FName construction doubled. Cooked contracts confirm the enum
ordinals, Tutorial's first map, save prefixes and native mainMap field. Wiring
assertions cover both menus, recap, host boot, world classification and existing
client mode transfer. `run_host_players.ps1` additionally executes the actual host
controllers for all six modes across each player limit and connection/listing
path. Release compilation complements these checks; rendered acceptance is separate.

`run_flashlight_native.ps1` extracts the production native flashlight setters and
ItemActivate dispatcher body. The setter fixture uses actual ParamFrame/call.cpp
with reflection/native boundaries doubled, checking exact argument bytes and no
dispatch after missing/overflowing parameters or invalid frames. The dispatcher
fixture covers synchronous admitted application without another posted task,
Player-id/sender-slot ownership, null/size/slot boundaries and pre-pose retention.
These checks do not establish visible rendered light or ragdoll behavior.

`run_llama_marker.ps1`, `run_llama_events.ps1` and
`run_llama_begin_restore.ps1` cover the marker/event/Begin continuation with 75
new numbered `PASS llama lifecycle N` groups. Marker tests include 19 actual
wrapper/record/ParamFrame groups and six actual PlacedSave/chunk/wire groups;
engine/codec/key-registry boundaries are doubled. Check exact native marker
class/key/record/transform, foreign-key refusal, keyed adoption, late census,
client mutation refusal and native actor preservation. The garden/2163 fixtures
have narrow marker boundary doubles so their previous family checks remain intact.

Event cases exercise verified sender/count cleanup, multiplicity, unchanged
unrelated storage/order, masked h, final snapshot validation, unavailable metadata,
weak source guard ownership and independent script-gate lifetime. Canonical Apply
now controls light/audio without writing native h. Begin cases capture Sphere/
zero-timer state, correlate invocations, preserve the oldest baseline, restore
only observed owned collision and admit isolated graph108 once after authority
and stack release. Read back native handles without inventing or replaying an
uncertain timer. Consumed source notification prevents delayed recovery after
an admitted host jump. Root integration adds Prepare/cancel/post/readiness cases.

Cooked contracts verify actor_save key-only load, native marker inheritance,
int_save capture, additive native event accounting and isolated108 timer creation.
The suites use actual production controllers/wrappers with fake engine/registry/
transport/clock boundaries; live schemas/rendering/save timing remain unverified.
The legacy false guard proves absence protection only, not old/new latent phase
causality. Missing POST and unknown admitted graph outcomes never synthesize
another timer; safe cleanup retries require later Install/Tick. No removal-control
or full rendered-pipeline count is claimed for these suites.

The llama recovery continuation adds `run_llama_timers.ps1` for the actual native
timer wrapper, actual lease controller and real ParamFrame. It exercises existing
client timers, original paused state, exact-handle pause/unpause, remaining phase,
uncertain completion, callback retirement, metadata recovery and same-world
teardown. Timer ownership is bounded; refused restoration is retried by later
Install/Tick calls, without an idle single-player polling claim. Identical-handle
external pause causality cannot be distinguished. Newly suppressed Begin timers
are not synthesized, and marker/save/event-count lifecycle remains separate.

The same continuation adds delivery/Begin restoration cases to `run_llama.ps1`
and launch-completion/motion cases to `run_llama_trigger.ps1`. New item numbers
use the separate `PASS llama recovery N` prefix, 1 through 75. Check heartbeat
cue debt, quiet/delivered history, bounded tombstones, clocks, canonical actor,
callback-side recipient replacement, send fairness, temporary flag restoration,
source-owned completion and confirmed paced motion admission. Engine/reflection/
registry/transport/object slots and clocks are doubled. Cooked graph checks and
Release linking complement these fixtures; neither proves rendered gameplay.

`run_llama_trigger.ps1` covers the remote-player activation continuation: 39
actual native-wrapper/real-ParamFrame groups and 34 actual selector/lifetime
groups at 32 slots. Two integration groups in `run_llama.ps1` bring coverage to
all 75 numbered TODO items. Check registry/life/pose/readiness, finite coordinates,
identity replacement, overlap and occlusion, nearest visible selection, local-body
native fallback, timer/physics/components, checked native launch preparation,
native math/velocity/tail, output cleanup, once-only admission, retry/new timer,
reentrancy, scope retirement/capacity and clock rollback. Both new production TUs
compile separately. The native contract checks cooked axes, velocity, timer,
overlap/Pawn filter and tail entry. Engine/reflection/registry/transport/object
slots and clock are doubled. The runners are in check_current; rendered gameplay,
marker/save/event counts and pre-session timer restoration remain open. No
removal-control or complete pipeline/rendered count is claimed.

`run_llama.ps1` covers the 25-item Console/events llama jump/light/audio subtask:
17 actual-controller groups at 32 peers and eight actual-wrapper/real-ParamFrame
groups. Cover graph/base-init authority, temporary flag restoration, source/key/
world/session ownership, coherent reads, typed native effects, admitted cue dedup,
bounded state, recipient readiness/replacement/requeue, ordering/wrap, pending
actors, expiry/rollback and quiet joins. Four production TUs compile. Engine,
reflection, object slots, transport and clocks are doubled. The opt-in custom
world-identity support macro leaves older fixtures unchanged. The cooked
`llama_native_contract_test.py` validates graph/console/marker dependencies.
Both runners are in check_current; reliablekind_gate checks full host-only route
integration. Rendered physics/effects, marker/save/event counts and audio timing
remain unverified. No removal-control count is claimed for this suite.

`run_rufus.ps1` follows TODO Console/events' Rufus/thiccfus birth/pose subtask.
Its 25 numbered groups include the actual native wrapper/ParamFrame, birth and
pending lifetime helpers and extracted production catch/drain/enroll/adoption
bodies. Fake registry/transport/engine/object slots/script-gate dispatch cover
ancestry/plush exclusion, exact sources, incarnation/scope/duplicate/finite gates,
whole-graph mirror refusal, admission readiness, native mesh attachment/AnimBP,
finite yaw and checked writes. Seven production TUs compile. `run_entity_sync`
also verifies stationary preparation retry in the actual NPC driver. The cooked
`rufus_native_contract_test.py` confirms family, console spawns, unsafe graph paths,
mesh/animation dependencies and source admission ordering. Both runners are in
check_current. Full pipeline/rendered behavior and cross-player gameplay/effects
remain open; no removal-control count is claimed for this suite.

`run_maxwell_native.ps1` follows TODO Console/events' Maxwell phase/recovery
subtask. Its 25 actual-wrapper/real-ParamFrame groups cover bounded/current-world
deduplicated collection, exact actor/audio/class/field identities, coherent state
and clock reads, unchanged failed output, checked query/float/bool parameters,
owned input, same-resource/duration/rate seeks, Stop/touch/toggle/squeak dispatch
and nested Playback revisions. Typed engine/reflection/object slots are doubled;
the production TU compiles. The older toy wrapper fixture now uses real ParamFrame
as well, retaining existing run_toy_music seek/no-restart/recovery coverage. The
runner is in check_current; sync_native_contract checks local cooked toy routes
and wave duration/loop metadata. Rendered phase, live schemas and enemy gameplay
remain outside these fixtures. No removal-control count is claimed for this suite.

`run_madness_recovery.ps1` follows TODO Console/events' playback-recovery subtask.
Twelve actual-controller groups at 32 slots cover scope/clock/shape/source, paused
age, recipient identity/readiness/departure, owned packets/revisions and old-event
cleanup. Thirteen actual-wrapper plus real ParamFrame groups cover native reads,
typed factories/return spans, widget/audio recovery, world/context, direct HUD
count/text, owned cleanup and nested/refused work. The legacy wrapper entry reuses
this fixture; old controller/console cases remain in run_2154. The native contract
checks cooked HUD/count -> master.killed -> spawnGrunt dependencies. Runtime/native
runners are in check_current. Engine/reflection/transport/clock are doubled;
rendered play and complete grunts/variant AI/damage/effects remain open.

`run_atv_companion.ps1` includes the actual private host lifecycle with real
CachedObjRef/AtvEntry and engine/count/save/transport doubles. Its 25 groups cover
scopes, incarnation, stillness and clock/count recovery, occupied sources, parking
revalidation, saved coop-key sources, bounded save arrival, reentry, typed results,
third-body/destroy retry, protected index cleanup, key publication/rebind state
and refused announcement retry. Compile the real controller. The runner is in
check_current. The outer hit-guard gate, live garage/spawn and native save files
remain outside this fixture; original gameplay acceptance stays open.
The legacy session-report runner delegates its ATV check here while retaining
the actual console classifiers, sleep controller and cooked-contract checks;
its old duplicated companion model/extractor has been removed.

`run_seat_presentation.ps1 -FaultControls` tests the 25-item seat continuation:
twelve real-controller groups and thirteen wrapper/view groups with 25 numbered
compiled removal controls. Include the actual sit wrapper and ParamFrame; extract
the actual view, world-rotation and sitting methods. Cover pending/bounded chair
admission, scope restore, reflected masks, class/function recovery, finite geometry,
full parent rotations, refused/reentrant mesh restore, partial writes, 32 bodies,
seated yaw, final head/world flashlight aim and ATV rig/seat/driver transitions.
Four production TUs compile. The runner is in check_current. Reflection/object
slots/engine/transport/clock are doubled; rendered alignment and live schemas are
unverified, and the relative-location getter cannot distinguish failure from zero.

`run_chair_sync.ps1 -FaultControls` includes the actual chair controller plus the
extracted production RemotePlayer sitting predicate. Ten groups and ten compiled
removal controls cover small-motion/all-axis updates, 20 Hz pacing and heartbeat,
joint shortest-arc body/chair interpolation, OFF cancellation, admission before
placement, late object/puppet discovery, scope/person/connection retirement,
owned validated samples, state transitions and reentrant send completion. Slot 31
and invalid slot 32 are exercised. Engine/reflection/object-slot/transport/clock
boundaries are fake; three production TUs compile. The legacy 2144 entry delegates
to this runner. `chair_native_contract_test.py` checks current cooked sitBox parent
chair, mainPlayer sittingOn attachment and unsit clearing. Both runners are in
check_current. These checks do not prove rendered multiplayer alignment or live
reflection schemas; the original garage-companion report remains open.

`run_atv_spawn_lifetime.ps1 -FaultControls` extracts current production TeleportRig,
keyed SpawnMirror and parking bodies. Nine spawn groups plus the current parking
fixture cover finite/frozen inputs, native service/class/key-offset recovery,
original world/context, validated deferred return type, finish and teleport
lifetime, incarnation-scoped cleanup, failed-placement publication and unchanged
source pose/output. Ten compiled source-removal controls must reach named runtime
assertions. Production atv.cpp compiles. Reflection/object slots/world/ParamFrame/
native calls are fake; this does not establish live schemas, real garage traces,
rendered wheels or the original missing second ATV. The runner is in check_current.
The shared spawn fixture now models object incarnation; parking tests stop queries
on actor/world retirement and refuse all six changed source-pose axes.

`run_light_presentation.ps1 -FaultControls` reuses the production lighting wrapper
and actual ParamFrame fixture for ten lamp/ambient/switch-audio groups and ten
compiled removal controls. It covers early actor/function/world capture, coherent
ambient component binding, sound metadata/callee and frame bounds, copied packets,
resource recovery, finite values, source retirement, allocation reentry and nested
scope restoration. It also compiles the real native-hook guard-page fixture and
checks exact PRE/POST callee identity and cleanup. Both production TUs compile.
The prior native-lighting fixture/control anchor is updated for sound readiness.
Both runners are in check_current; boundary fakes do not prove rendered completion
or the inner engine audio-wrapper's preparation lifetime.

`run_lightswitch_native.ps1 -FaultControls` includes the production lighting
wrapper and actual ParamFrame implementation. Ten groups and ten compiled
removal controls cover root/switch partial resolution and metadata replacement,
missing fields without fallback, three bool masks, bounded linked-root arrays,
checked group arguments and actor/class/function/world/layout lifetime through
group and switch preparation/completion. Reflection/native boundaries are fake;
the tests do not establish real reflected offsets or rendered multiplayer.
The current suite includes this runner. Active wrappers_2129 sound/provenance
tests now resolve switch metadata instead of injecting a private raw cache.

`run_flashlight_join.ps1 -FaultControls` includes the real flashlight controller
through the existing state fixture, with 32-slot transport/registry doubles. Ten
groups cover local/relay refusal, fresh sampling, OFF, recipient/author retirement,
owned packet bytes, newer completion/reentry, 64-attempt fairness and session/
role/link/lane/world admission. Ten compiled source-removal controls must reach
their named runtime assertions. The 961-debt stress case completes without
duplicate recipient/origin deliveries. The production TU compiles. This runner
is in check_current. Engine/transport fakes do not establish rendered joins or
atomic admission against net-thread teardown; unknown identities are refused.

`run_atv_recovery.ps1` runs the real hit guard and unchanged controller install/
verb bodies against delayed metadata, temporary interceptor capacity, impulse
frame bounds, throttled/idempotent retry and owner/mirror/offline dispatch.
It also runs unchanged native spawn/teleport bodies with argument-preparation,
finish-result and cleanup failures. `run_2149.ps1` now uses the current syncer/
stamped-seat mount queries and retains supported nearby parking/wall/pit/trace/
tilt tests, adding non-finite yaw. Deleted authority routines are not covered.
`run_2177.ps1` adds refused-watch recovery and stopped-session lamp/ambient
guards. All three runners and the cooked-console check are in check_current.
Engine boundary fakes do not prove real garage geometry or rendered multiplayer.
The audit repairs merged duplicate console code and duplicate flashlight fixture
definitions; two console controls now remove both lifecycle checkpoints.

`run_sat_console_callbacks.ps1 -FaultControls` includes ten console native-lifetime
groups in `sat_console_native_lifetime_test.cpp`, using the production wrapper,
ParamFrame, FString/FText and field-write code. They cover live binding replacement,
atlas/factory/init ownership, superseded commands, output/box preparation, clear
retirement, frame bounds and panel-array headers. Ten additional compiled controls
bring this runner to 23 controls (nine controller and fourteen wrapper controls).
`sat_console_native_contract_test.py` separately checks current cooked clr's two
backing-string resets. These focused checks do not qualify a shared Release DLL.

`run_flashlight_click.ps1 -CompileSources -FaultControls` executes the real remote
click controller with 32 slots, actual roster PerSlotState and CachedObjRef, and
native preparation fakes. Ten groups cover slot/body routing, puppet/class/world
retirement, dedup scope, readiness retry, sound/attenuation replacement, nested
state changes/roster replacement, non-finite location and missing attenuation.
Ten compiled removal controls run their named scenario and must reach assertions.
Both click-controller and item-activation TUs compile. The runner is included in
`package/check_current.ps1`. Native audio wrapper admission/metadata and rendered
multiplayer remain unverified; preparation retries only on a subsequent apply.

`run_2157.ps1 -CompileSources -FaultControls` keeps the held-toy/world-toy/contact
fixtures and adds ten 32-seat held-audio groups for session scope, sender/recipient
transport, frozen PRE hand identity, exact bounded PRE/POST, overflow retry,
receive-buffer mutation, 64-send budget, strict expiry/clock rollback and reentrant
join completion. Ten compiled controls run their named scenario and must fail an
assertion. Seven production TUs compile. Current output is `build/hand-toy-check/`;
the historical runner name does not select a release version. Unknown pre-use
hand generation is refused for network replay rather than adopted later. The
pitch-preparation fake checks source currency before the actual sound dispatch.

`run_watoy.ps1 -CompileSources -FaultControls` runs the legacy controller/native
wrapper plus ten 32-seat controller lifetime groups: session scope and teardown
reentry, exact POST, bounded PRE reservation/pruning, requester/recipient transport
replacement, state-flush replacement, fresh-queue preservation, revision pruning,
overflow retry and caller-owned packet mutation. Ten compiled removal controls
run their selected scenario and must reach an assertion failure. Four production
translation units compile. Rendered multiplayer and complete wrapper-preparation
lifetime remain unverified.
It also runs ten actual-wrapper lifetime groups for field/class ownership, unknown
zero metadata, source bindings, activation/audio predicates, service CDOs,
mesh/template retirement, checked/finite outputs, spawned candidate cleanup and
same-View Apply/Clear reentry. Ten additional compiled wrapper controls must reach
their selected assertion: twenty controls total. Native parameter-schema/complete
function ownership and independently copied View cleanup remain open.

`run_toy_music.ps1 -CompileSources -FaultControls` runs the existing native
wrapper/phase/clock fixtures plus ten 32-seat lifetime groups: session transitions,
source/recipient transport replacement, exact POST, reserved PRE, overflow retry,
receive-buffer ownership, shared 64-send budget, callback expiry/clock rollback
and consistent key restrictions. Ten separately compiled removal controls must
reach failing assertions. Four production TUs compile. The existing `run_2155`
and `run_2156` groups continue to exercise the world-toy controller fixture.
These checks do not establish rendered multiplayer audio acceptance.

`run_sponge_effects.ps1 -CompileSources -FaultControls` runs the existing sponge
controller/wrapper, ten lifetime groups at 32-seat capacity, and real trace-reader
replacement/recovery cases. Eleven compiled controls cover session serial, exact
POST, PRE reservation, frozen keys, actor class, source departure/connection,
recipient connection, overflow admission, native work budgets and player-link
currency. It checks callback expiry, clock rollback, FIFO, reused persons, failed
sends and fresh queues installed during teardown. Four production TUs compile.
These are transient cosmetic effects: no cleaning/soap debit replay or join history.
Current development remains public version 2161; focused checks do not qualify
an earlier ZIP or establish rendered native-effect parity.

`run_window_stroke.ps1 -FaultControls` executes the real bay-window stroke
controller in 14 scenario groups: refused FIFO and sealed brush values, send/drain
budgets and expiry, nested callbacks and source retirement, malformed input and
32-seat/relayed origins, queue overflow, serial/world scan identity, signal
playback and at-most-once native replay, observer retirement and hook recovery.
Eight controller controls must reach their named failing assertion. The actual
native wrapper and ParamFrame run 14 more groups plus nine compiled controls:
checked offsets/sizes/Vector2D, class/function/canvas/material retirement, stock
and hand changes, unknown signals, scalar failure, factory type/pin lifetime,
teardown reentry, preparation retry and at-most-once admitted draws. Both production
TUs compile. A cooked 0.9.0n test follows native canvas open/join, retriggerable
close, signal veto and separate drawing. The runner is in the full current suite.
This does not establish rendered canvas parity, original-dab brush provenance,
atomic material-scalar sampling, thrown-sponge transport or independent grime/save parity.

`run_prop_action_native.ps1 -FaultControls` covers nine controller scenario groups,
ten actual hit-wrapper/ParamFrame groups and four wall-wrapper groups. It checks
current actor/key/player/tool/metadata through native damage and slice preparation,
client input retirement, the actual isNotSawable getter and its reentry/refusal,
required hit bindings, exact schemas/weak pointers/blocking, destination lifetime,
layout recovery and native wall destruction. Ten source-copy controls reach their
intended failing assertions. Cooked evidence confirms the getter call, inherited
damage forwarding, the base slice no-op and the distinct hacksaw mesh-break path.
These checks do not accept rendered cutting or fragment synchronization.

`run_upstream_integration.ps1` validates the merged native blast decoder with
valid, force-only and salt-rocket packets, malformed lengths, non-finite values,
coordinate/scale/range limits and private-PC refusal. It runs the real drone
intent controller, including two queued timer toggles, and checks that all 226
published fork message values remain reserved beside fifteen distinct upstream
additions. It is included in the full current-source qualification. Integration
boundaries are recorded in [TESTLOG.md](../TESTLOG.md#upstream-integration-b2215-25-october-2026).

`run_prop_action_delivery.ps1 -FaultControls` runs the real action controller in
18 scenario groups. Coverage includes owned first-send payloads and nested FIFO,
source/key/class replacement, refusal/caps/eight-row budget/expiry, fresh queues
after teardown, world-MRE request/cue suppression, exact personal spawn scopes,
hand-cleared provenance, nine scope retirements, one admitted received opening,
preparation retry, 36 metadata/body/source cases, authorization replacement and
safe capture/requeue/publication ownership. Nine source-copy controls must fail
at their intended assertions. Both production TUs compile. Fresh cooked MRE
checks retain action-4 spawn/cue/destroy order, three loot finish paths plus the
ancillary poison pour, native initialization/weights and reward/roach/portion
side effects. Existing birth filters, action/safe and wall regressions remain
separate. No rendered gameplay or transactional output/refund recovery is claimed.

`run_food_callbacks.ps1 -FaultControls` executes the production food controller
in 18 scenario groups with controlled native/transport boundaries. It checks
untracked/capacity refusal, valid/addressable positive portions, reserved nested
eats, exact key and POST ownership, source/class retirement, dead final-bite
retry, eaten output scope and hand-cleared provenance, skipped POST, output caps,
verified factories, host refill protection and native function/owner retirement.
Eight compiled source-copy controls must reach their intended failing assertions.
The fresh cooked check follows each eaten entry and its branches: only box,
banana and cup spawn wrappers; six personal overrides and the empty base are
excluded from host replay. Both production TUs compile. `run_cooking` separately
covers the actual birth-authoring filter, wrapper and existing hand/world state
delivery. These checks do not establish concurrent cross-peer food transactions
or rendered multiplayer acceptance.

`run_host_players.ps1 -FaultControls` checks the actual native selector and config
registry: all 31 limits, presets/bounds, shared choice, deferred slider persistence,
concurrent selections, reopening and every partial-build allocation. The real
HostWithSave, HostLobby and environment announcement bodies exercise all 31 limits
across new/existing saves, direct/hidden/listed/brokered and offline fallback.
The real lobby parser tests normal, missing, malformed and 64-bit-overflow counts.
Seven removed-protection controls must fail. The runner also executes all 118
Rust master tests, including each 2–32 capacity through signed create, heartbeat,
browse and join. Cargo is required; a portable installation under
`../_toolchain/rust/` is used when Cargo is absent from PATH. These checks do not
simulate rendered multiplayer or deploy the master service.

`run_sleep_personal.ps1 -FaultControls` executes the actual sleep controller at
32-seat capacity. Native personal nightmare state/probability, sender authority
and real 36-byte wire shape remain covered. New delivery coverage checks refused
host flags/peer relays, coalescing and retry pacing, complete awake-inclusive joins,
generation/readiness replacement, paced wake veto, host dreams, pending teardown,
foreign sessions, stopped/disconnected/malformed input, world/serial/role changes,
reentrant delivery and exact/nested client publication. A 32-player vote and last
recipient/subject slot are exercised. Twelve controls remove each relevant guard,
including separate client payload/request protection and pending disconnect reset.
The production TU compiles. These tests do not establish rendered clock behavior.

`run_profile_join.ps1 -FaultControls` executes the production save-ready callback,
profile controller, receiver and chunk sender. It checks one-shot client requests,
owned items/vitals/pose, dead/missing-profile defaults, partial native failure,
reentrant profile/request changes and nested joins. Every native stage is tested
against world/session/save/request replacement. Host preparation and chunking
cover GUID ownership, disconnected/replaced recipients, nested sends, exact
occupancy latches and retry debt. Seven controls remove world/revision/request
guards, guarded chunks, GUID ownership, occupancy rearming and disconnect refusal.
The complete profile controller also compiles. This is not a rendered join test.

`run_container_apply.ps1 -FaultControls` compiles the production receive/apply/
relay functions with the real inventory/container wrappers, record codec and park.
It covers checked allocation/construction and retry, exact world-slot isolation,
accepted/rejected reentrant input, actor/component/store/header/session/recipient
changes, commit-bound birth completion, targeted relay debt, park replacement/
reset/retirement/caps/clock rollback and duplicate hashes after binding replacement.
Strict outer-header cases require an unreadable slice rather than false empty
state. Seven controls remove checked encoding, current-source proof, relay debt,
the masked boundary, old-park retirement, revision checks and strict slice reads.
Both production TUs compile.

`run_inventory_apply.ps1 -FaultControls` compiles the actual inventory wrapper,
container wrapper, record codec and extracted production InstanceOffset method.
It refuses each of 69 allocation points in a rich carried/worn/held fixture and
checks that all old array headers survive. Class/name/string/signal failures,
fixed empty-slot transforms, owned inputs, nested supersession, world/save/class/
gamemode replacement, changed headers, packed Player flags, exact slot-zero
isolation and replacement metadata are covered. Strict personal/equipment/hand/
target headers, genuine/reserved empty arrays, checked held-record replacement,
caller cancellation and container volume/name callback lifetime are also covered.
Nine controls remove checked encoding, lifetime guards, slot refusal, class-cache
refresh, strict personal/target reads, complete snapshots, checked hand staging
and derived-state lifetime checks. The fresh native
check reads the actual player component template and fixed equipment graph.
It does not establish rendered joins or the original unknown item-copy trigger.

`run_ariral_ship.ps1 -FaultControls` runs the production arirShipAppear phase
controller and wrapper/ParamFrame path. It checks host-only authority, graph and
latent-continuation refusal, scoped photo-listener admission, native local-photo
eligibility, validated wire data, ordered incarnations/tombstones, quiet seeds,
partial application and metadata readiness, live-cue deadlines, reentrant newer
states, bounded fair sends, recipient replacement and actor/world/session cleanup.
The wrapper verifies timeline stopping, packed bits, alpha/light, native emitter
parameters and event-sender isolation. Four removed-protection controls must fail.
Its fresh cooked contract binds the exact entries and blank-email loop to 0.9.0n;
it does not qualify arirShipVanishing, shared reputation or rendered multiplayer.

`run_container_birth.ps1 -FaultControls` runs the actual takeObj consumer and
generic finish feeder. It covers native-body scope use, nested/absent/skipped
POSTs, thread isolation, reincarnated function layouts, caller out storage,
failed results, final saved identity, separate spawn/state refusal, reentry,
actor/world/session/role changes, bounded fair retries and feeder ordering.
Four removed-protection controls must fail assertions. Its fresh native check
verifies the successful restored-object out and the separate record-only path;
it does not establish the original client dupe trigger or rendered peer acceptance.

`ci_fingerprints_test.py` verifies the added game-function reference rows against
the saved native capture, including inherited resolution and absent declarations.
GitHub runs it beside the fact-row gate.

Current full audit: run `powershell -NoProfile -ExecutionPolicy Bypass -File
package/check_current.ps1` from the checkout after source edits finish.
With no filter it runs 94 groups, including upstream integration, native prop actions, prop-action delivery, food callbacks, wall delivery, container recovery, console callbacks/delivery, host-player selection, profile-join, container-apply,
inventory-restore, Ariral phase, container-birth, session-report and canned-refresh runners,
completes the incremental Release build and verifies that the
production inputs stayed unchanged, without repackaging. Results and
logs are in `build/todo-audit/`. `run_held_identity.ps1` compiles the actual held
broadcast seam; the older item/resource/lamp/laptop runners use the current typed
fact-row and save-codec APIs. The expected assertion failures in mutation logs
must produce the failure code required by their runner. Gameplay remains separate.

For a focused qualification, call the script from PowerShell with
`-CheckGroups @('run_container_apply','run_inventory_apply','run_sync_2157')`.
The same stable-source, Release and ABI checks still run; `verification.json`
records `FullRegression=false` and the exact selected groups. Unknown groups fail.
A focused report does not replace the last full-suite qualification.
The b2215 full qualification in `build/upstream-qualified-2215/` covers 94 groups.
Focused qualifications and
separate fault-control runs are recorded in TESTLOG; they do not claim a full run.

`run_sat_console_delivery.ps1` exercises the actual console controller, wire
codecs, chunk sender and assembler with 32 independently replaceable peer slots.
Its 14 scenarios cover capacity, recipient/source replacement, owned/reentrant
queues, refused or mutated history, nested clear ordering, busy/widget lifetime,
native command preparation and detached teardown/install/eviction. Transport and
engine calls use controlled doubles. `-FaultControls` requires assertion failures
for eight production-source mutations; a compile failure does not count. The
runner also compiles the controller against production headers. `-Case` selects
one scenario, and `-Source` can test a source copy without editing production.

`run_sat_console_callbacks.ps1` checks the production controller and real wire
codec/chunker across 17 input, invocation, display and replay scenarios. These
include 63 read/consume retirement transitions, failed consumption and recovery,
nested Enter/clear, skipped or stale POST, latent local output, failed-row retry,
widget and host-binding replacement, scoped replay/preclaim and exact watch
rollback. Engine calls and transport use controlled doubles. The runner also
compiles production headers and tests the actual native input wrapper with real
ParamFrame, FString and FText code: 39 preparation invalidations, five post-call
invalidations, missing/dead textbox and native failures. `-FaultControls` requires
nine source-copy controls to reach their intended runtime assertions. `-Source`
and `-Case` support focused controller checks.

Run scripts from the canonical checkout. Numbered filenames identify the development
batch that introduced a regression, not the currently packaged build; do not remove
tests solely because their number is older.

`run_container_recovery.ps1` covers local base-container extraction with 16
production-controller scenarios, nine real refund-wrapper groups and fresh cooked
native ordering checks. It preserves a saved native capture for the new extract
fingerprint, with the existing takeObj reference as a hash-format control. The
wrapper uses the actual checked save codec, metadata and allocator seams; native
VM/transport boundaries remain doubled. `-FaultControls` requires seven compiled
source-copy defects to fail their intended assertions. `-WrapperOnly` omits the
controller fixture. The runner also compiles five production translation units.
Receipt-less faults retain unresolved debt; these checks do not establish
cross-peer transaction authority, persistent debt or rendered acceptance.

`run_wall_delivery.ps1` exercises the actual wall controller, native wrapper and
ParamFrame across 20 scenarios: admission versus post-admission failure, paced
fair retries, lifecycle/actor/class/component/function retirement, nested callbacks,
exact replay ownership, partial publication, invocation pruning/capacity, independent
guard registration, replacement sessions, solo hosting and no-spend repair refusal.
Engine objects and native dispatch use controlled doubles. `-FaultControls`
requires eight compiled controller/wrapper mutations to reach their intended
runtime assertions. Both production translation units also compile against their
real headers. The older 2166/2168/creative suites retain integration coverage;
paid client repair and rendered gameplay remain separate work.

`run_explosion_motion.ps1 -FaultControls` checks the production host feeder from
native overlap selection through the actual delayed radial impulse to deferred
coasting. It covers repeated blasts, source/target reuse, pickup/frozen/static
exclusions, capacity, session/world replacement and reentrant cleanup. Five
controls reject early motion, unrelated impulses, held-prop admission and stale
session work and missing force-only discovery. The production radial wrapper tests
actual ParamFrame sphere/filter arguments, copied inputs, engine-owned output cleanup
and refusal after native callback teardown. The existing gascan/general controllers test their host-only feeder
calls; the item-batch drive harness checks motion publishing and final rest edges.
The native script uses the existing cooked corpus and a live impulse-signature
capture with two existing engine-function controls. `native_explosion_motion_probe.lua`
is the opt-in isolated LAN fixture: set `EXPLOSION_MOTION_PROBE_DIR`, select source
gascan and target keys in `blast.txt`, record both baselines with `watch`, then
change it to `fire`, or `force` for zero damage with unchanged native impulse.
The latter sets damage/debris/fire to zero before native BeginPlay and records the
actual values in `host-force-only.tsv`. It invokes native gascan break at the target's original surface
and samples peer positions; use a disposable profile and matching current DLLs.
Accepted host blasts are registered independently of damage. A native overlap
query at FireImpulse also discovers force-only targets; the feeder does not scan
props or generate impulses. Ordinary rendered/input acceptance remains open.

`run_upstream.ps1` tests selected upstream adaptations: tiny positive/NaN vital
fractions, delayed fossil-hound class names and prop class resolution while loading
or called from a worker. The entity runner also checks applied-pose deduplication,
closed-window write retries, full rotations, hound body yaw and plain-wisp landing.

`run_shared_screen.ps1` checks the shared UI wire format and operator/host authority,
handoff, send refusal, join/widget readiness, plus the production sleep/poseable
display's selected mesh/material/scale, mattress trace, moving anchor and teardown.
The ragdoll runners also cover animation warmup and articulated-bone validation,
batch completion/order and rig replacement. Gameplay with two peers remains required.
`run_ragdoll_skin.ps1` also runs the full native startup regression:
selected geometry before physics, preserved camera direction and delayed native
momentum commands, repeated entry/recovery and recycled-object guards.
Its skin wrapper checks component shutdown before mesh replacement and per-body
shutdown after the new physics asset is installed.
`run_ragdoll_native.ps1 -FaultControls` also verifies immediate native-construction
setup and the recovery camera handoff, including camera world position and yaw.
Removing either correction must fail its control. The opt-in installed-game
`VOTVCOOP_RAGDOLL_TRANSITION_TEST=1` matrix checks eight completed native wakeups,
delayed heading drift and subsequent controller turning on Scientist and Kerfur.
`ragdoll_transition_fingerprints_test.py` checks the two native camera references
against a real 0.9.0n capture, with two existing signatures as format controls.
It can regenerate those entries with `--capture <capture-file> --write`.
These harnesses do not simulate PhysX or close the all-role C-key launch report.
`run_shared_screen.ps1` also checks the production desktop traversal against an
old boot snapshot applied after native startup, repeated power pages, desktop
text/scroll/tab retries and missing widget dependencies. Its native test reads
the existing laptop corpus to verify rootswitch boot/desktop ownership; it does
not extract game assets or establish runtime host/client acceptance.

Current focused suite: `../../package/check_2152.ps1`. It invokes the player-report,
wisp animation, ragdoll retry, ragdoll skin and personal-sleep runners, entity orientation, plus the three
native-evidence Python checks. Native checks require the local research corpus.
The C++ runners require the local MSVC toolchain; inspect each runner for its setup.
The entity runner includes grayboar_actions_test.cpp, exercising the production
host transition observer, packet receiver and direct montage/audio path. Its test
double refuses unexpected reflected calls so gameplay-verb replay fails the test.

Keep the numbered runners, C++/Python sources and stub directories together: older
runners contain distinct regression and negative-control cases. Generated binaries
belong in build/ or _research/, never beside the test sources.

Historical archives, backup snapshots and generated test outputs were removed on
19 October 2026. Active regression sources and stubs remain. run_2141.ps1 tests the
current code and mutation controls without depending on an archived source copy.

`disk_utf8_test.py` requires Windows and cl on PATH (load the local toolchain environment first). It compiles production disk string functions and checks lossless wire framing for control characters and Unicode.

`run_item_batch.ps1` runs production harnesses for held-state capture, native local
hold yield, reliable root scale and host/receiver motion, seven existing item runners
and the cooked native-evidence check. Logs are in build/item-batch-check/.
These are offline checks; no PhysX or two-peer runtime is simulated.

Current event qualification also includes `run_dream_identity.ps1`, serverbox
delivery/application regressions within `run_events.ps1`, and fresh cooked
dream/server-breaker/full-suite contracts. `package/check_current.ps1` accepts
`-AssetDirectory` (default `../_research/current-assets`); these contracts require
the extracted 0.9.0n uasset/uexp pairs and local `_research/kismet_dis.py` /
`dump_2135.py` decoder. They verify native contracts, not rendered peer acceptance.

`run_session_reports.ps1` exercises the actual physical-console classifiers,
companion retry controller and host/client sleep controller. Its native check
verifies the fresh hole/material/treasure, console-owner and can contracts.
The garden runners cover synthetic hole identities, schema, both authors,
acknowledgement, late join, duplicate creation, removal and suppression readiness;
the record runner checks stale can refill/reclose/type refusal and initial births.
`run_2174.ps1` covers canonical readiness before native can mutations. These do
not reproduce the unidentified client item copy or close warp-arrow presentation.

## Fish integration

`package/check_current.ps1 -EvidenceDirectory build/fish-integration-check` runs
both main and fish regression groups, then verifies a stable production snapshot,
Release identity and loader ABI. The native physics fingerprint controls and
public-prose include-context tests are included. All peers need identical DLLs:
the final package 2155 contains current b2178, superseding the earlier local b2177 preparation.

Package 2155 qualification: `package/build_2155.ps1`, evidence in
`build/release-2155-logs/`. `run_events.ps1 -CompileSources` also compiles the five
event production TUs and runs replay/rate/active-lifetime harnesses. The new
`event_rate_native_test.py` decodes existing night-eyes bytecode; it does not extract
fresh game assets. `run_trowel.ps1` checks the production persistence wrapper and
the actual production integer-array helper/schema. `run_placement.ps1` includes
exhausted-retry recovery, sweep protection and bounded-pool cases.
It also runs `remote_destroy_lifetime_test.cpp` against the actual receiver and
local consume/convert cleanup, covering native replacement, recursive callbacks,
listener registration and unchanged load-tail behavior. `run_2161.ps1` now checks
session-serial reuse, native pose/bounds teardown and recursive location ticks.
b2178 qualification evidence is in `build/continuation-check/`; the owner's final
2155 release request promotes this qualified DLL into the package. Delivery hashes
are in `build/release-2155-logs/release-verification.json`.

Development b2179 extends `run_trowel.ps1` to held water/material persistence and
the production numeric-group commit. It checks all-or-nothing malformed/stale
refusal, zero/refill and unchanged native array ownership. `run_2153.ps1` also
checks local held water use/tick and cement/sponge/transfer persistence hooks.
Full development b2179 qualification passed 65/65 groups and the stable Release
build: `build/material-persistence-check/verification.json`. Package 2155 remains
b2178 and does not contain the material, C-key or PC boot corrections.

| Runner | Production behavior exercised |
|---|---|
| run_nonentity | Coin credit/retirement, placement replies, dry walls/material records, sky latent gates |
| run_cooking | Cooking progress, world/hand food readiness, profiles, replacement and output authority |
| run_food_callbacks | Reserved eat admission, captured final bites, native eaten output provenance and host final-state guards |
| run_prop_action_delivery | Owned/reentrant action FIFO, MRE request/spawn/open admission and safe publication ownership |
| run_drink_spills / run_gasoline / run_scorch | Exact launch provenance, host flight/fire, decals, quiet join and retirement |
| run_water_pour | Bucket/washer shots, source audio, projectile construction and host birth/coasting |
| run_grime_join / run_windows | Cleaning snapshots, nested hooks, latest final wipe and retained join delivery |
| run_kitchen / run_grill_hand / run_grill_edits | Lid/fuel/burn/audio, held state, saved runtime and ordered field edits |
| run_hand_tools / run_sponge_effects | Tool display lifetime, stop expiry, soap observation and cosmetic foam |
| run_toy_music / run_watoy / run_2157 | Maxwell phase/recovery, Watoy authority/motion and held-toy audio; `run_2157` also checks class-scoped input watches for all catalogued Dingus variants |
| run_wall_creative / run_2166 | Creator/source/reach/rule validation, wall construction and immutable retry |
| run_item_records / run_2130 / run_2176 | Record ordering/reentrancy, native apply budgets, profile capture and stale sends |

The placement integration cases also enforce acceptance-only record publication,
refusal of existing same-key actors, cross-peer key reservations before indexing,
readiness, cooking bounds and malformed wire strings. `run_2130` expands local
production fragments before mutating them, so fault controls alter the live code.

`sync_native_contract_test.py ASSET_ROOT DECODER_ROOT` needs the branch's complete
extracted 0.9.0n corpus. This workstation lacks 18 of its 45 literal package inputs
(and the suite has additional dynamic inputs); it is not counted as passing.
The owner's fresh pak audit remains deferred. These offline tests do not simulate
PhysX or establish two-peer gameplay acceptance.

## Immediate-join regression and 32-seat checks

`run_atv_steering.ps1` compiles the production native wrapper and mirror controller:
actual handle yaw, native tick reset, front-wheel constraint calls, driver handoff,
readiness retry, actor/occupant replacement and reentrant teardown. The extracted
production join function must preserve a pending honk without replaying it to the
joiner. Live-captured tick/steering fingerprints verify the native contract.
`-FaultControls` must reject both the omitted steering sample and a snapshot
that clears the pending honk. `native_atv_steering_probe.lua` is an optional
isolated-game fixture: set `ATV_PROBE_DIR` to a generated evidence directory and
write `host 1`, `host -1`, `host 0` (or `client`) to its `driver.txt`. It samples
native handle yaw on both roles and uses native driver/input-field stimuli;
it does not establish keyboard or rendered acceptance. Runtime files stay in build/.

`run_blood.ps1` checks the production player-blood observer, host validation,
owner exclusion, native replay and bounded retries. It covers malformed/duplicate
requests, slot 31, recipient/source replacement, recursive callbacks and world/
session teardown. The two-function native capture verifies the `addParticle`
reference and separates world drops from the blood-loss actor's local damage.
Rendered host/client bleeding acceptance remains open.

`run_sync_2157.ps1` compiles the production watch registration/retirement code,
checks 1100 pending/resolved watches and repeated reopen cycles, and rejects the
previous table capacity. It compiles the actual container publication and seed
retry functions with refusal/readiness/occupancy/reentry cases and an old-retry
negative control. Production recipient selection and held-food addressing cover
slot 31; host admission covers every selected limit from two through 32. Protocol
payloads retain the existing datagram bounds. `run_2145.ps1` also checks deferred
flashlight component/setter applies, startup before the player class loads, refused
connect publication, newer callback state and world replacement. These remain
offline fixtures; rendered multiplayer and larger real groups require acceptance.

The upstream seam checks include `run_swinger_join.ps1` (the actual join controller
under reentrant pose/ownership/teardown callbacks and the actual park controller
with 32 rider leases), `run_vulkan_context.ps1` (device/window identity policy,
without a GPU), and `run_2170.ps1` (actual cord controller at 4 and 32 peers).
`merged_protocol_test.cpp`, run by `run_placement.ps1`, pins the selected intent
and host-state authorship and prevents blind relay. These tests leave native GPU,
contact-physics and multiplayer acceptance separate.

`build_order_test.ps1` executes the packaging workflow scripts in a disposable
fixture with external-command doubles. It checks failed tests/gates, selected
groups, ChecksOnly, successful ordering and qualification reuse; no release DLL
is compiled. Registered as `build_order` in check_current.


## Entity adapter boundaries (6 November 2026)

`run_geom_octa.ps1` verifies G01-G11; `run_entity_adapters.ps1` verifies
R01-R40/P01-P24, cooked native contracts and production object compilation.
The latter's `-FaultControls` rejects eight independently compiled mutations.
Reflection, native dispatch and transport are doubled; multiplayer acceptance
and the remaining entity behavior/effect routes are open.

| ID | Implemented boundary |
| --- | --- |
| G01 | Invocation-bound nested aggro capture. |
| G02 | Host session/world cancellation, including verdict callbacks. |
| G03 | Bounded aggro/graph/overlap frame reads. |
| G04 | Mirror state bound to actor incarnation. |
| G05 | Replay continuation cancellation after native callbacks. |
| G06 | Nested relay-tick refusal. |
| G07 | Overlap-hook installation retry. |
| G08 | No native verdict replay without an armed host-answer hook. |
| G09 | Exact actor/component/function/frame/bytecode overlap match and one write. |
| G10 | Scoped hook arming and disarming through teardown. |
| G11 | Checked native overlap/notify parameters and metadata recovery. |
| R01 | Clear roach work on session/world replacement. |
| R02 | Validate cached roach class/function incarnations. |
| R03 | Retry unavailable roach metadata instead of permanently disabling it. |
| R04 | Resolve master/property offsets against current objects. |
| R05 | Validate roach array Num/Max/Data. |
| R06 | Bound raw-array scans including null holes. |
| R07 | Capture component slot serials in roach rows. |
| R08 | Revalidate sampling after location callbacks. |
| R09 | Revalidate sampling after scale callbacks. |
| R10 | Refuse incomplete host population samples. |
| R11 | Reject invalid native roach position/scale values. |
| R12 | Retain refused host snapshot delivery. |
| R13 | Skip zero on roach sequence wrap. |
| R14 | Isolate snapshot progress per recipient. |
| R15 | Bind queued snapshots to peer occupancy generations. |
| R16 | Deliver empty population at join. |
| R17 | Converge empty populations through keepalive. |
| R18 | Require an active client for roach state reception. |
| R19 | Require exact page count and row count. |
| R20 | Reject conflicting headers within one snapshot. |
| R21 | Reject stale sequences with wrap-aware ordering. |
| R22 | Ignore duplicate completed/received pages. |
| R23 | Validate every wire roach row before assembly. |
| R24 | Retain complete snapshots while the master is unavailable. |
| R25 | Bind application to the master's exact incarnation. |
| R26 | Own snapshot data across reentrant reception. |
| R27 | Refuse nested roach ticks/application. |
| R28 | Check every location-set parameter before calling native code. |
| R29 | Retain scale-write failure for reconciliation. |
| R30 | Check deletion index/crush parameters. |
| R31 | Check creation location/size/bypass parameters. |
| R32 | Do not repeat uncertain creation/deletion effects. |
| R33 | Verify component-to-raw-index binding before deletion. |
| R34 | Retry reversible application failures. |
| R35 | Exclude snapshot reconstruction from consumption detection. |
| R36 | Detect consumed component reincarnations by serial. |
| R37 | Suppress consumption after master replacement. |
| R38 | Own consumption rows across send callbacks. |
| R39 | Validate consumed-intent sender/position/readiness. |
| R40 | Revalidate nearest-match target and sender through native reads/deletion. |
| P01 | Clear pyramid work on session/world replacement. |
| P02 | Require game-thread active-client gather reception. |
| P03 | Admit only the exact pyramid mirror for replay. |
| P04 | Require a canonical host wisp for host gather identity. |
| P05 | Bind host gather edges to actor serials. |
| P06 | Cancel post-send host accounting after teardown. |
| P07 | Bind pending gathers to pyramid/wisp incarnations. |
| P08 | Ignore duplicate gathers without extending their deadline. |
| P09 | Reject nonfinite gather positions/distances. |
| P10 | Revalidate both location-query continuations. |
| P11 | Consume admitted gather replay once, including uncertain dispatch. |
| P12 | Restore staged fields only on the still-owned pyramid. |
| P13 | Revalidate gathered-wisp tick enablement. |
| P14 | Refuse recursive pyramid ticks. |
| P15 | Scope replay admission to its exact pyramid/function. |
| P16 | Restore replay admission with RAII. |
| P17 | Bind restored-tick bookkeeping to actor incarnations. |
| P18 | Require live heading components. |
| P19 | Reject nonfinite heading yaw. |
| P20 | Stop heading writes after component/actor callback replacement. |
| P21 | Reject nonfinite head-look vectors. |
| P22 | Validate cached host wisp-target identity. |
| P23 | Validate join slot readiness and occupancy generation. |
| P24 | Own join iteration data across send callbacks. |
