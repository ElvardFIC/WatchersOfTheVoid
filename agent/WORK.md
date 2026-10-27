# WORK — current state and open work

## Current feature - hosting game-mode selection (2162)

Owner explicitly requests six modes: Tutorial, Story Mode, Infinite Mode,
Ambience Mode, Sandbox Mode and Solar Mode. This authorizes feature work despite
the older bug-fixes-only default. Name G1-G3 before implementation, then remove
those completed entries from the implementation-only TODO.

G1 implemented: shared six-choice selection in the native host window and fallback
picker, and the captured mode in the session recap. Default remains Story. Mode
controls apply to new games; existing saves keep their own mode. Native controls
dim/ignore mode input for existing saves, repaint on return from the fallback,
clear stale deleted-slot selection and retire widget pointers on menu replacement.

G2 implemented: cooked enum ordinals are 2/0/1/6/4/7, not menu indices. Initialize
new-save mainMap before persistence, validating save/class identity around native
FName construction. Host boot explicitly passes the captured new-game mode, and
mode application waits for successful write/readback before travelling. Tutorial
routes to /Game/maps/tutorial3; campaign modes and Sandbox use the main map.
Reject invalid new-game ordinals before hosting/network actions. Existing client
save-transfer already carries gameMode and both saved/fresh client boot paths now
use the corresponding initial map; no wire shape/protocol change is required.

G3 implemented: tutorial3 and its chapter worlds are classified as playable,
including the degraded boot-survey fallback. Menu/preload remain non-gameplay.
An existing save still derives its own mode; selecting a new mode never mutates it.

PASS new actual-helper six-mode fixture/cooked contracts and expanded actual host
controllers across all 31 player limits and connection/listing/offline paths.
Related save/upstream/contract/routing groups and a separate sky regression pass.
Release evidence is build/game-mode-feature-final; detailed outcomes in TESTLOG.
Rendered menu geometry, real host/client Tutorial chapters and native Solar/
Sandbox behavior remain gameplay acceptance limits, not completed tests. Sandbox
creation uses the main map; a separate Sandbox map picker was not requested.
No new release archive, installation or publication; release metadata unchanged.

## Current implementation and TODO scope - 2162

The owner requested 75 existing tasks, then explicitly removed all checks from
TODO. The earlier B01-B75 selection is superseded: most selected entries were
rendered acceptance rather than implementation. Keep those acceptance limits and
validation results here/in TESTLOG, not in TODO. The cleaned list contained 38
implementation entries; three audit fixes below are complete and removed, leaving
35 unfinished entries. Do not claim 75 completed tasks or invent microchecks to
meet that number. Preserve the flashlight/T-pose defects as open implementation.

Completed A10: all four native flashlight setters now refuse dispatch if any
ParamFrame write fails, including visibility's second boolean. Completion can no
longer acknowledge a zero/default frame after missing/overflowing metadata.

Completed A11: ItemActivate now applies directly in the existing game-thread
inbox drain. Remove its redundant posted task, so no queued application survives
the originating session or occupant. The lane itself retains pre-pose state.

Completed A12: reject a resolved Player id belonging to a different sender slot.
Reject null payloads, non-exact lengths and invalid sender slots before reading
the body. Preserve valid host/relayed origins, missing-id fallback and self-echo
handling. No wire shape, public version or protocol change.

New run_flashlight_native extracts actual production setter/dispatcher bodies;
the native fixture uses real ParamFrame/call.cpp with reflection/native doubles.
It covers exact bytes, missing/overflowing fields, invalid/zero frames, dead
targets/native refusal, immediate application, Player spoofing, boundaries and
pre-pose retention. Registered in check_current; rendered gameplay remains open.
The 14 older supplemental fixture failures remain recorded in the audit below;
they are not converted into TODO implementation tasks or counted as repaired.


## Current audit target — 0.9.2162 (27 October 2026)

The owner's latest instruction explicitly selects 2162, superseding the older
2161-only continuation instruction. Last qualified package: 0.9.2161 / b2217.
No package/version/protocol change is part of this audit.

Owner reproduction report: every player sees only their own flashlight, and every
player enters a T-pose on falling. These are reported gameplay failures, not
reproduced/closed by this offline audit. No live game logs or rendered peer run
were available in this work. Prior doubled fixtures passing is not acceptance.

### Findings and disposition

1. **P0, repaired — corrupt merge.** b9a087de9 adds 60 duplicate lines in
   ue_wrap/desk/sat_console.cpp and one duplicate definition line in the flashlight
   fixture. Duplicated BindingsCurrent/Lease/ParamBounds, variables and an unmatched
   loop stop compilation. Both failing baseline groups pass after removing exactly
   that merge's additions. Production contents equal the pre-merge parent.
2. **P1, repaired after audit — unchecked native flashlight parameters.** In
   ue_wrap/engine/engine_mainplayer.cpp:514, :523, :533 and :542 the four light
   setters ignore ParamFrame::Set results, then return Call's result. Missing or
   incompatible parameter metadata can therefore dispatch a default/partial frame
   and be acknowledged as a successful apply; item_activate then clears retry debt.
   Existing controller fixtures replace these setters. Requires an actual-wrapper
   failure/retry regression. This conditional defect is not proof of the owner's
   constant missing-light cause.
3. **P1, repaired after audit — deferred flashlight delivery outlives its origin.**
   coop/dispatch/event_dispatch_entity.cpp:753 captures only slot and payload in a
   posted task. No session serial, world generation or occupant identity is retained.
   A task executed after Stop/Start or slot replacement can apply the old light to
   the new occupant; ApplyToPuppetOrDefer reconciles against the new current scope.
   Bind the originating identity at admission and reject retired work before apply.
4. **P1, repaired after audit — flashlight sender/target identity mismatch.** The ItemActivate
   branch selects a Player's slot from payload senderElementId (:732), without
   comparing it with msg.senderPeerSlot. VerifySenderEidRange checks host versus
   client ranges, not the client occupant. A client can name another client's
   existing Player id and target its puppet. ApplyToPuppet's belongs check verifies
   the selected slot, so it does not restore the missing sender check. Needs an
   actual-dispatcher impersonation/normal-relay regression.
5. **P1, investigation — ragdoll reference pose and takeover.**
   ue_wrap/engine/engine_ragdoll_skin.cpp:215 switches to single-node animation,
   SetSkeletalMesh reinitializes the pose and the function finishes with physics
   blend weight zero. It transfers appearance/world transform, not the current
   animated bone pose. engine_playerragdoll.cpp subsequently rebuilds bodies and
   sets simulation/blend to one; late recovery and remote creation use a 30ms
   wait. Native fixtures fake simulation/configuration and do not prove the
   rendered takeover or that a failed native call leaves a usable body. This is
   a specific risk consistent with a T-pose, not a verified explanation of all
   reported falls. Preserve the reported issue until rendered entry/fall/recovery,
   standing/crouched and host/client cases pass.
6. **P1, open — regression coverage drift.** There are 140 run_*.ps1 scripts;
   37 are absent from check_current's direct list. Some are nested already:
   run_ragdoll_native under run_ragdoll_skin, run_serverbox under run_events,
   and run_2179 under run_2180. Therefore 37 does not mean 37 unique uncovered
   mechanics. Execute them anyway: final result 23 pass, 14 fail. Older shared
   Session/Registry doubles still use four seats while production uses 32; the
   service-panel test hits that exact static assertion. Other failures are stale
   interfaces, missing fixture includes and obsolete source/mutation anchors.
   They cannot be counted as passes or assumed to be production failures.
7. **P2, repaired — broken validation paths.** Sixteen legacy PS runners pointed
   at a nonexistent checkout-local toolchain; fifteen used the inaccessible
   WindowsApps python alias. Eleven native-evidence scripts read research below
   the checkout instead of its sibling. Correct those paths without changing
   assertions or weakening controls. The remaining failures are listed below.
8. **P2, repaired — stale generated translations and development instructions.**
   Regenerate multivoid.pot/en_XA.po (443 entries). The template gate now passes.
   Align AGENTS/agent README/root README with the explicit 2162 audit target;
   preserve release.json and the historical 2161 package identity.

Failing supplemental runners (all retained as failures): run_2124 (four/32-seat
stub mismatch); run_2126 and run_2129 (generated control misses facts_test_support.h);
run_2127 (gravity fixture linker drift); run_2128 (removed lightswitch internals);
run_2129_audit (save callback signatures); run_2131 (Registry double API);
run_2132 (obsolete source-text assertion); run_2134 (removed lift Mirror API);
run_2135 (drone wrapper fixture dependencies); run_2136 (relocated pump assertion);
run_2137 (obsolete cancellation mutation anchor); run_2138 and run_2175
(hand fixture dependencies). Full logs: build/audit-supplemental-final/log names
are <runner>.log, with results.json in that directory.

### Verification and limits

PASS all 121 registered groups against a stable production source stamp; current
Release b2217, 854 production C++ entries, ABI gate including its injected negative
control, production 0.9.2161 menu formatter, 132 server tests, and the architecture
gates recorded in build/audit-gates/results.json. Recheck l10n_pot after generation:
PASS (the initial gate-results entry intentionally retains the failing baseline).
Supplemental final result: 23/37, with actual ragdoll native/stream fixtures passing.

Inventory accounts for all 1089 production/development units with no missing
listed files and no unlisted production/development .cpp. Comparison of all 2142
last-qualified snapshot inputs after the repair matches exactly except SAT-console
line endings (LF versus the snapshot's CRLF); normalized contents match too. This
does not prove all game behavior correct or absence of older defects.

Evidence: build/audit-current (interrupted failing baseline),
build/audit-current-fixed/verification.json (complete passing main run),
build/audit-supplemental-final/results.json, build/audit-gates/inventory.json,
abi.log, package-label.log and server-tests.log. Fresh local Release DLL SHA256:
03fe830ac55f758c65b8554e9840ae9c6113b19590f66567157d06983a3b8062.
The previous archive retains its qualified historical DLL; it is not repackaged
with this locally rebuilt DLL. No 2162 package, game installation or publication.

Earlier entries below retain their historical version and acceptance boundaries.

Active development: **0.9.2161 / b2217 / protocol 2217** (27 October 2026).
The owner explicitly keeps work in 2161. Continuations do not advance versions or
automatically prepare releases. The previous 2162 label was an assistant mistake;
older reports/archives retain their historical bytes and do not qualify new changes.

## Current delivery — 0.9.2161 offline test package (27 October)

Owner explicitly requested the 2161 build. Run package/build_current.ps1 without
qualification reuse: all 121 registered groups, Release/ABI, unchanged source
snapshot and the production version formatter pass. Package/rejection checks
pass for the 32-member manager archive. Public version remains 0.9.2161;
internal build/protocol remain 2217. No new version was started, so the new-version
cleanup workflow was not invoked.

The first complete run found one stale upstream integration assertion expecting
15 additions to the published protocol. Replace that count with the exact 16
name/ID pairs including existing LlamaState253, keeping all 226 published values
fixed. The subsequent complete 121-group run passes. Production code/DLL bytes
remain unchanged by this test maintenance. Update package README/RELEASE/CHANGELOG
to describe the delivered snapshot and retain explicit gameplay limits.

Archive: dist/ElvardFIC-WatchersOfTheVoid-0.9.2161.zip (15,384,506 bytes).
ZIP SHA256: 8e176d4970adb1056d6c3b7f09758eb8a26ab3b05f3677d7fd2b4d38835d0d0f.
DLL SHA256: 67f3e3c4d5a22a23b628aebcc277c7b9d005ad66fbbc35306a0f2b88773fb9a4.
Evidence: build/qualification-current/verification.json and
build/package-current/verification.json, rejection-controls.json and full-suite-retry.json.
Native manager startup and real-peer gameplay remain unverified. No game install,
commit, push, service deployment or publication.

## Latest implementation — TODO Console/events, 75 llama marker/event/Begin fixes

Continue the remaining Console/events llama implementation under 0.9.2161 /
b2217 / protocol 2217. All 75 planned items are listed individually in TODO.
Use the existing PlacedSave native-record route for llamaSoul rather than adding
a parallel actor lane or new wire kind. Validate native key creation/ownership,
record shape/transform and marker-only host authority while retaining keyed
save adoption and the existing teardown behavior for native saved actors.

Canonical client presentation must not mint native h or a native event refcount.
Settle only verified pre-session sender entries while preserving unrelated
events, then protect exact weak sources from legacy absent-sender false debits
through same-world disconnect. Keep native new activations/registered completions
working. Actor/class/world and native field identity bound every operation.

Capture the original Sphere collision and zero-timer baseline before suppressed
client base Begin, reserve exact invocation ownership and restore only after its
stack finishes and client authority has ended. Use isolated graph108 to arm the
native timer without replaying base initialization. Restore only still-owned
collision, never replace a new timer and never replay an uncertain creation.

PASS 75 new numbered actual-code lifecycle cases: marker (25), events (25) and
Begin restoration (25), with explicit engine/codec/key-registry/transport doubles.
Marker cases include actual existing PlacedSave/chunk/wire controller paths;
native wrappers use real ParamFrame. Existing garden and placed-record regressions
also pass. The current combined Release qualification is recorded in TESTLOG.

Integrate accounting Prepare before canonical Apply, and reserve/commit/cancel/
finish Begin recovery only through the exact captured invocation. Install current
role before callback-capable resolution, and drain after clearing old client
authority. A host-admitted jump explicitly consumes any delayed Begin recovery
lease, preventing another timer after that one-shot. Canonical presentation no
longer writes native h. Event cleanup is a validated callback-free ledger update;
legacy absent-sender protection retains its own script-gate hold until weak
source/world retirement. Marker records remain on the existing PlacedSave wire
and native actors survive disconnect/rejoin; only failed bodies freshly created
by the marker wrapper are cleanup-owned.

Rendered save/reload/native schemas and timing remain separate acceptance. No
native host event contribution is synthesized on clients; absent-sender protection
does not identify the causal age of an old/new latent continuation on the same
actor. Unknown metadata permits original event execution. Missing Begin POST
allows collision cleanup only, and uncertain graph108 admission is consumed once.
Unconfirmed/uncertain timer creation has no automatic replay. Collision-only
cleanup may wait for graph/KSL metadata as well; failed cleanup needs later
Install/Tick, with no idle SP retry pump. No version bump, package, install,
commit or publication.

## Earlier continuation — TODO Console/events, 75 llama recovery fixes

Continue the open llama timer/lifecycle/delivery follow-ups under 0.9.2161 /
b2217 / protocol 2217. The 75 named items are recorded individually in TODO.
Pause existing client native timers through an exact-handle lease, preserving
their remaining phase and leaving originally paused timers untouched. Restore
owned live-world leases on teardown or role/start replacement; retain refused
cleanup for later installation/tick rather than claim idle single-player retries.

Keep live cue debt through heartbeat, retain quiet/delivered recipient history,
bound same-pulse receipt lifetime and retain ordering/heard tombstones after
state expiry. Reconcile clocks, recheck canonical actor/announced world and the
recipient immediately around callback-capable preparation, and distribute send
work fairly. Retain temporary Begin flag ownership through stale invocation,
POST, disconnect and same-world role changes with refused-restoration retries.

Separate selected-target admission from source-owned completion after native
timer consumption. Finish the native tail once after an uncertain velocity call;
never repeat an admitted impulse. Queue prop motion until actual Coast admission,
yield to hand/Kerfur ownership, and retire bounded debt on source/session/world
replacement or expiry. Prevent unarmed census rows from exhausting source capacity.

PASS 75 new numbered actual-code cases across native timer wrapper/controller/
ParamFrame (25), delivery/Begin ownership (25) and launch/motion (25), plus the
earlier 100 llama groups. Revalidate current Release/sources through check_current;
the current combined qualification is in TESTLOG. Timer metadata unavailability
retains owned cleanup debt; callback predicates are followed by exact-handle
rechecks. A disconnect during native Pause drains cleanup as that call unwinds.
Cache the typed KSL CDO/class to avoid repeated global-object scans. Integrate
timer Install/Tick/OnDisconnect and queued native-host motion in llama_sync.
Newer nested session installation survives older teardown completion, and a
same-pulse heartbeat during transport admission retains delivered-pulse history
without acknowledging newer state debt.

Begin restoration refuses new admission at 128 pending records, with at most 32
already-admitted captures able to retire afterward. Timer rows and pending motion
are separately capped at 256. Motion retries every 100 ms for up to 10 seconds;
tombstones retain 256 keys, evicting oldest expired entries under pressure. Marker/save/event-
count lifecycle, never-created Begin timers/Sphere restoration and rendered
two-player acceptance remain separate open work. Identical-handle external pause
causality cannot be proved. Refused timer cleanup awaits a later Install/Tick;
there is no independent idle SP retry pump. No version bump, package, install,
commit or publication.

## Earlier continuation — TODO Console/events, 75 remote-player llama trigger fixes

Owner changes continuation size to 75 on 27 October; AGENTS, agent/README and
TODO now agree. All 75 named implementation items are recorded individually in
TODO. Keep public/internal/protocol identity unchanged; no package preparation.

Add a host target controller and a separate native trigger wrapper. At the native
500 ms cadence, capture an armed physical llama, query native Pawn overlap and
select the nearest registered, posed, alive, non-ragdoll remote eye with clear
static/dynamic visibility from Audio. Hold person/connection/body/class/world
identity across preparation; reject invalid coordinates, excessive eye/body
separation, readiness loss and target movement during admission. Local triggering
keeps the native graph but now requires the local host's own body in the overlap;
another player's overlap can no longer launch toward the unrelated host camera.

Native preparation checks yumpskare/h, the typed eight-byte timer and its activity,
component bindings, actual world positions, radius and mesh physics. Validate all
query/launch parameter spans and function incarnations before mutation. The
directed launch preserves native cross-product axes, rotation(false), velocity
delta*3+(0,0,100), replacement velocity and BoneName None. Clear only the captured
timer, never overwrite a callback's replacement, then enter graph tail 212 so
native audio/light/collision/event bookkeeping and the three-second continuation
still run on the host. Bound/deduplicate native output and always free its engine
allocation. Recheck timer activity, physics and source points before launch.

Reserve admission before callback-capable validation. Suppress nested and consumed
jump/check delegates, retain admitted uncertain attempts without another impulse,
retry preparation refusals and recognize a new timer. Bound source records to 256;
session start/world replacement retires them and rollback reopens polling. Feed
both remote and local jumps into the existing prop coasting lane; sample the
remote tail into existing LlamaState rather than introducing another wire kind.

PASS focused actual-code fixtures cover all 75 numbered items, including 39 native
wrapper/real-frame groups, 34 selector/lifetime groups at 32 slots and two state/
motion integration groups. Engine/reflection/registry/transport/object slots and
clock boundaries are doubled. Wider checks and current Release identity are in
TESTLOG. Rendered two-player acceptance, marker/save/event-count lifecycle and
pre-session timer restoration remain open. Already admitted native effects cannot
be undone; a retired target or uncertain native completion may end the attempt
after partial effects, and that timer's one-shot is consumed. No game launch,
archive, install, version change, commit or publication.

## Earlier continuation — TODO Console/events, llama jump/light/audio

Keep 0.9.2161 / b2217 / protocol 2217. The next 25 named items address llama's
native local-camera jump and light/audio state. Add host-only LlamaState (253),
normal lane/authorship/dispatcher and lifecycle integration. Its 44-byte payload
owns key, sequence/incarnation pulse, active/cue booleans and bounded remaining
sound lifetime. This is an ongoing development format; both peers need matching DLLs.

The host samples canonical h, captures the actual jump graph invocation after
native effects and publishes a coalesced source-owned state/edge. Stable current
key/actor/class and per-key revisions prevent old sampling from winning. Bound
the census/rows/captures; retain per-recipient person/connection/revision debt
through readiness loss. Each send owns bytes and recomputes remaining lifetime.
Join snapshots are quiet. Session/start/world/role/link and client host-connection
replacement retire state. Clock rollback cancels pending sound debt.

Clients refuse derived graph jump/check/resume/destroy gameplay. The native base
prop BeginPlay remains: temporarily clear yumpskare for that exact invocation,
then restore its original value in matching POST. Restore a successfully parked
flag if admission is canceled while its captured owner remains current. Typed
native light/audio bindings and exact function/parameter checks apply h/light and
Activate(false); admitted one-shots are consumed even after uncertain completion.
Native presentation leaves physics to the ordinary prop lane. Client state waits
for the keyed announced actor/components, rejects stale packets across wrap and
deduplicates pulses across retries or newer same-pulse callbacks.

PASS 17 actual-controller groups at 32 peers and eight actual-wrapper/real-frame
groups, plus cooked native llama/console/soul contracts. Engine/reflection/object
slots/clock/transport are doubled. The support fixture gains an opt-in independent
world-pointer double; existing tests keep their default behavior. Current combined
checks and Release identity are recorded in TESTLOG. Rendered motion/sound/light,
join/reconnect, hand transitions and save/reload remain unverified. Marker/save/
event-count lifecycle, remote-player trigger selection (native host camera/player
zero is still used) and pre-session timer restoration remain open. Local sound
lifetime bounds do not establish a synchronized audio clock or maximum reliable
transport delay. No game launch, archive, install, version bump, commit or publish.

## Earlier continuation — TODO Console/events, Rufus/thiccfus birth and pose

Keep 0.9.2161 / b2217 / protocol 2217. Add the verified Character family to the
late NPC allowlist; thiccfus inherits the Rufus graph, while prop_thiccfus is an
ordinary separate plush. The console's EX deferred spawn source now admits only
the two exact Character outputs. Enforce source output restrictions. Pending
catches hold object/class serials, session/start/world ownership, deduplicate
before capacity and discard retired records. Enrollment reads and validates
finite pose/scale before creating identity. A host alone still enrolls its NPCs.

Use the existing class-scoped script-gate API for ExecuteUbergraph_rufus. Readiness
is required before allocating a fresh mirror and again before FinishSpawning;
adoption gates before binding and removes a newly added mark after binding refusal.
The marked family's whole graph is refused, including BeginPlay, Tick, AI/latent
callbacks, damage, player-zero repositioning and corpse/plush creation. Unmarked
host/local actors continue natively. The cooked contract confirms these unsafe
branches and inherited graph; no live game or census capture has been performed.

Add a native Rufus presentation wrapper: validate current actor/mesh/arm/AnimBP
classes and field bindings, restore KeepRelative Mesh -> SpringArm using checked
attachment parameters/return/function, and write the typed animation's pawn field.
Read finite actual mesh yaw on the host and use the existing body-yaw pose field
on clients, preserving current mesh pitch/roll. Failed preparation/write remains
dirty in the actual NPC driver, so missing AnimBP or attachment retries while
stationary. Already attached resources do not repeat the native attach call.

PASS 25 focused groups using the actual native wrapper/ParamFrame and extracted
production catch/drain/enroll/adoption bodies. Fake registry/transport/script-gate/
object slots/engine boundaries are explicit. Existing NPC regressions pass, including
the new actual-driver stationary retry. Native mapping/graph and preflight ordering
contracts pass. Current combined qualification is in TESTLOG. Rendered birth/pose,
late join/destruction, cross-player targets/grab outcomes, damage/explosion feedback,
audio/HUD/event state and corpse mesh/loot transport remain open. Adoption does not
reverse earlier native effects; scale reads keep the engine helper's unit fallback
on a native read failure. The wider Console/events category remains open. No game
launch, archive, install, version change, commit or publication.

## Earlier continuation — TODO Console/events, Maxwell native playback

Follow the open Maxwell-phase subtask with the 25 named TODO items. Remain
0.9.2161 / b2217 / protocol 2217. Replace the native toy wrapper's loose pointer/
offset checks with captured object/class/world, field and dispatch bindings.
Indexed collection now handles null/bounded buffers, repeated variant results
and world retirement. The audio field must point to an actual AudioComponent.
Read only publishes coherent used/volume/pitch/playing values; ReadSqueakPitch
keeps caller output unchanged on failure. Present invalid pitch is rejected.

Clock reads hold the component/Sound/SoundWave class relationship and coherent
loop/duration/pitch fields. Native queries and float/bool parameters require exact
reflected spans/sizes; function identity is checked through dispatch. Apply owns
its incoming State, validates the full shape before native writes and preserves
adjacent isUsed bits. Phase seeks require the same duration/rate/resource before
and after Play. Prepare ToggleActive before touch, then recheck the original
function/component. Preserve once-only squeak admission and newer nested Playback
through monotonic invocation revisions. A refused/uncertain native operation does
not roll back effects already admitted; a later state apply compares actual playback.

PASS run_maxwell_native: 25 actual-wrapper/real-ParamFrame groups with typed fake
UObject slots and unique incarnation serials. Upgrade the older wrapper fixture
to the real ParamFrame too; retain its clock/no-restart/recovery tests. The runner
is integrated into check_current. Current combined checks and DLL qualification
are recorded in TESTLOG. Rendered music/phase, live reflection schemas, exact sample
timing and multiplayer acceptance remain open. Unsupported/nonlooping sources
keep ordinary play/stop without a guessed phase. Rufus/thiccfus, llama, grunts/
variant gameplay and the broader category remain open. No game launch, package,
install, version bump, commit or publication.

## Earlier continuation — TODO Console/events, Madness recovery

Follow Remaining implementation's first open category and its explicit Madness
playback-recovery subtask, with 25 named fixes in TODO. Remain 0.9.2161 / b2217 /
protocol 2217. Rufus/thiccfus, llama, grunts/variant AI/damage/effects and Maxwell
phase remain separate open work in that category.

The controller now retires by session serial, role/running state and world
pointer/generation. Host sampling retains master/class identity and validates
packet coherence; paused music does not accrue network age. Recipient debt binds
person/connection and revision, survives readiness loss and owns each send's bytes.
Clock rollback cannot create huge seeks. A new event waits for owned old cleanup;
partial current-event preparation retains live resources rather than restarting
them on every HUD retry.

The native wrapper owns source/class/function/context dependencies and validates
return spans before native allocation/query dispatch. It reads the current master
lineage and exact HUD/audio bindings, accepts a known null music field as stopped,
retains viewport retry and validates widget/audio factory results. The HUD label
and count update directly: cooked ui_madnessCombat::addCount calls master.killed,
and the master's native spawnGrunt path creates enemies. Cosmetic client replay
does not call that route. Monotonic View revisions preserve fresh nested work;
refused owned Stop/Remove cleanup remains available while the controller retains
the View. Exact class/slot identity protects foreign/reused resources.

PASS 25 actual controller/wrapper/ParamFrame groups, legacy console/Madness and
cooked native-contract checks; current qualification is in TESTLOG. These tests
double engine/reflection/transport/clock boundaries. Rendered music/seek/HUD,
late join/reconnect, event replacement, exact audio timing and live schemas remain
open. Unknown native results cannot be rolled back and final teardown has no
owner to retry a refused cleanup. The whole Console/events category remains open,
as do full regression/package qualification. No game launch, archive, install,
version change, commit or publication.

## Earlier continuation — 25 host companion lifecycle fixes

Remain 0.9.2161 / b2217 / protocol 2217. The next 25 TODO items address the
original missing second ATV report through the host controller. Move its private
state into atv_companion.h, included by atv_sync.cpp. World/session/host ownership,
checked source/minted tokens, finite pose and continuous two-second stillness now
gate minting. Riding/carrying, count uncertainty, source reuse, motion/rotation
and clock rollback restart readiness. The sole persisted coopatv-key rig is an
eligible source; a saved second rig gets a bounded five-second arrival window.
Search and announce clocks reset with the lifecycle; nested creation is blocked.

The source is confirmed after parking/count/occupancy preparation. A third body
is removed without a permanent success latch. Failed live-controller destruction
retains ownership and is retried, while foreign/reused bodies and foreign/missing
index keys receive no destructive action or blanket destroy packet. Publication
holds the current body through save-key assignment/readback, preserves existing
table state and retains refused spawn announcements. Rebinding verifies both rows
before/after the native write and moves the freshest owned entry, including its
world, rather than overwriting newer driver state captured by a callback.

PASS 25 actual private-lifecycle groups plus production controller compilation.
The fixture doubles engine/count/save/transport boundaries and uses real checked
object references and AtvEntry. Combined Release/source/DLL qualification is in
TESTLOG. Rendered host/client garage creation, late loaded rigs, save/reload,
reconnect and no-third-body acceptance remain open. Native key reservation policy
is retained: stale save records are not forcibly overwritten. Failed disconnect
cleanup is not retried after teardown and already accepted native effects cannot
be rolled back. Full regression/package remain open. No game launch, archive,
install, version change, commit or publication.

## Earlier continuation — 25 seat presentation fixes

Owner requests 25 items per continuation; update AGENTS/agent README/TODO in
place. Remain 0.9.2161 / b2217 / protocol 2217. The 25 named TODO items cover
pending/bounded/current-world chair admission, stable local occupancy, tracked
scope cleanup, AnimBP recovery, separate sitting/IK bool masks, checked mesh
setters, finite model geometry, coherent base capture, complete parent-space
rotation, refused/reentrant restore, 32-body capacity, seated yaw, final head and
flashlight aim, checked world rotation and seven ATV driver/seat transition guards.

The chair lane now retains temporary NoBody/NoTarget outcomes and confirms the
body anchor lies within 450 cm of the named chair. Cleanup detaches its tracked
references before restoring and stops when callbacks install a fresh session.
The engine sit wrapper resolves the current AnimBP and masks, holds exact mesh/
class/parent bindings, uses full pitch/yaw/roll inverse rotation and retains the
vertical part of forward motion. Authored bases are committed before the first
write, survive a refused second write, and remain available for later restoration;
nested fresh shifts survive an older clear. Capacity matches 32 player bodies.

Remote view work is shared by normal presentation and the final chair/ATV pass.
The head samples the finally placed body; flashlight world aim remains correct
when the support tilts. Missing rotation metadata no longer triggers a raw write.
The current world-rotation helper resolves/checks class/function/arguments.
Driver placement retains current rig/seat/body/person/transport/scope bindings,
requires readable finite transforms, validates fallback direction and stops after
a refused or retired write. The authoritative ATV seat table cancels a stale
chair anchor before the driver pass.

Verification is recorded in TESTLOG and qualification-current. Actual controller,
sit wrapper and ParamFrame plus extracted production view/rotation bodies are
tested at doubled engine/reflection/transport boundaries. Rendered chair/body/
head/light, custom rigs, banked vehicles, IK restore, joins/reconnect and 32-player
load remain open. A relative-location getter's zero-on-failure result cannot be
distinguished from authored zero; accepted native effects cannot be undone.
Original missing second ATV, full regression and package qualification remain
open. No game launch, archive, install, version bump, commit or publication.

## Earlier continuation — seated player and chair rotation

Keep 0.9.2161 / b2217 / protocol 2217. The owner's chair request is implemented
through ten named TODO items. The sender samples one checked pawn/chair/key pose,
detects 1 cm / 0.5 degree changes on body yaw and all chair axes, sends at up to
20 Hz and retains a one-second heartbeat. OFF contains no chair rotation and
refusal leaves the last accepted signature pending for a fresh sample/retry.
Completion cannot publish an old signature across session/revision/pawn/seat/
class replacement; nested Tick cannot drain twice.

The receiver owns the packet and admits the named chair before driving the body:
correct chair class/key, host intent reach and local-seat exclusion are required.
Unresolved chairs remain pending without moving the puppet. Body location/yaw
and all chair angles use one 50 ms render alpha with shortest angular arcs and
continuous rebasing; run chair/ATV placement after other prop presentation.
Stand-up cancels the anchor instead of rotating a released chair. Session/world/
role/roster/transport changes, retired chair identity and a three-second missing
heartbeat retire old work. The actual RemotePlayer sitting predicate now excludes
computer presentation as well as sleep/ragdoll, matching local pose release.

PASS ten actual-controller groups plus ten compiled removal controls at 32-slot
capacity and production compile of sit_pose, remote_player and net_pump. The
fixture extracts the real IsSitting predicate; reflection/object slots/engine/
transport/time are doubled. Cooked local sitBox/mainPlayer bytecode confirms
parent-chair assignment/attachment and unsit clearing. Current combined Release
and related regressions are recorded in TESTLOG. Rendered chair/body/head/light
alignment, late join/reconnect, moving chairs, transitions and ATV seat acceptance
remain open. A host lacking a posed sender may defer until the next heartbeat;
already accepted native effects cannot be undone. The original missing second
ATV and full regression/package remain open. No game launch, install, archive,
version change, commit or publication.

## Earlier continuation — ATV deferred spawn and garage placement

Keep 0.9.2161 / b2217 / protocol 2217. Ten named TODO fixes reject invalid poses,
refresh checked native spawn services, freeze class/key/pose inputs, retain the
original world/context, validate deferred return ownership before key writes,
retain the deferred actor through finish, restrict cleanup to its captured
incarnation, protect whole-rig teleport, refuse failed rig placement and retain
the original source identity/pose through garage parking queries.

SpawnMirror no longer reports success after a failed teleportVehicle call: the
root-only finish can leave separate wheel bodies behind. Cleanup runs only while
the captured actor/class and original world/generation remain valid. A foreign
begin return receives no key write or destroy. Return-key validation uses the
captured class offset, rechecks identity after conversion and rejects a key that
changed during conversion. Failure text no longer promises destruction when the
captured actor was retired. Parking stops after a retired trace source and only
publishes a location if the source stayed within 1 cm and 1 degree of its original
pose during the query. Wall/pit/tilt limits and native wheel-aware teleport remain.

PASS ten source-extracted spawn/parking groups and ten compiled removal controls;
the reflection/object-slot/ParamFrame/native-call boundaries are doubled. The
production ATV TU compiles. Earlier spawn argument/finish/cleanup and supported
parking/mount regressions pass. Combined Release and flashlight/console/lighting
qualification is recorded in TESTLOG. The original report (one ATV for everyone)
stays open until a live garage reproduction. Live schemas, traces, wheel placement,
join/reload/riding/save acceptance and full regression/package remain open. An
unreadable deferred return cannot be identified for safe cleanup; dispatched
native effects cannot be rolled back. No game launch, install, archive, version
change, commit or publication.

## Earlier continuation — lamp/ambient presentation and switch audio

Development remains 0.9.2161 / b2217 / protocol 2217. Ten named TODO fixes own
lamp and ambient source identity before preparation, hold the exact ambient
component, recover sound metadata, bound parameter reads, freeze audio packets,
recover attenuation resources, reject invalid floats, retain source/resource
currency and guard resource construction plus nested replay context.

Ambient reads validate the original actor/component/class/bottomLight binding
before reading and before publishing output. Setters retain native function and
current class binding through frame preparation and dispatch. The switch-audio
hook now asks the native-hook layer for the exact callee, separate from the BP
caller; replaced/retired callees cannot lend their frames to another layout.
Parameter spans must fit the captured function frame. A copied cue/volume/pitch
packet and checked cue/attenuation/function references remain current through
location reads; invalid or retired replay stays muted. The 100 cm sphere plus
1400 cm falloff and local/outside-replay native sound are retained. Missing hook
metadata/installation defers switch replay; missing attenuation permits the visual
with its UI cue suppressed until a later replay has spatial resources.

PASS ten new groups/controls and previous ten native-lighting groups/controls,
with actual wrapper and ParamFrame code. The real native thunk test exercises
PRE/POST callee identity and cleanup alongside its guard-page return checks.
Lamp/ambient and cooking/provenance/sound-reentry regressions pass. Current
combined qualification is recorded in TESTLOG. Rendered lighting/audio and
joins/reloads, original garage-companion reproduction, full regression and package
qualification remain open. Inner audio-wrapper dispatch preparation and full
reflected schemas are outside these boundary checks; accepted effects cannot be
undone. No game launch, install, archive, version bump, commit or publication.

## Earlier continuation — native root/switch lighting

Keep 0.9.2161 / b2217 / protocol 2217. Ten named fixes in TODO cover root and
switch metadata recovery, removal of both offset-fallback paths, three distinct
bool masks (root live state, switch presentation and breaker), bounded linked
root lookup, checked group verbs and switch replay lifetime. Metadata holds live
class/function references and compares current bindings; incomplete layouts keep
retrying. Class predicates remain available independently of property readiness.
Raw duplicate metadata pointers are replaced by the checked references.

Native group/use operations own actor/class/function, world and layout revision
through frame/sound preparation and completion. Missing owner/index/active
arguments cannot dispatch zero-filled defaults. Layout revisions retire older
preparation even when replaced functions remain alive. SwitchRoot bounds the
TArray header, owns the first linked actor/class and rechecks header/entry identity
after lookups. Getter failures leave caller output unchanged. The group's live
isActive and its active breaker remain separate reflected bools.

PASS ten real-wrapper plus actual ParamFrame groups and ten compiled removal
controls. Lamp/ambient and the current cooking suite pass, including existing
switch sound/reentry checks; its active wrapper fixture now supplies reflected
switch metadata rather than assigning a private function pointer. Current cooked
assets confirm owner/index, active and use names and inherited Key/objects on
triggerBase. Combined source/DLL qualification is recorded in TESTLOG.
Rendered lighting, joins/reloads, full regression/package and inner engine audio
dispatch remain open. The named sound-hook/lamp/ambient fixes are implemented
above. These guards do not undo a native
call that already ran or prove at-most-once retry after uncertain completion.
No archive, install, version bump, commit or publication.

## Earlier continuation — flashlight join delivery

Versions remain 0.9.2161 / b2217 / protocol 2217. The ten named TODO items are
implemented: local seed retry, cached-peer relay retry, fresh local sampling,
OFF-state replay, recipient ownership, author retirement, owned packet bytes,
revision-owned completion, a shared rotating 64-send budget and session/role/
readiness admission. Connect hooks queue bounded state instead of sending it
once and discarding refusal. Targeted debt is independent of regular broadcast
deduplication. Hosts retain the original source slot on cached relays; a client
may seed only its host link. Host recipient/author identities require a known
person and transport generation; the client's sole host link may use generation
zero and remains fenced by the current session scope.

The drain samples local light at each attempt, freezes cached packets before
transport, and clears only work whose target, source and revision remain current.
Newer cache/enqueue/publication work survives older completion. Source departure
or transport/roster replacement cancels its relays, including a link that becomes
disconnected before its generation is cleared. A nested Tick cannot duplicate the
drain or reset its budget. Departure clears recipient rows and source columns.

PASS ten actual-controller groups and ten compiled removal controls, including
32-slot/961-debt fairness, refusal, missing components, OFF, mode/battery changes,
target/source replacement, owned bytes, reentry and stopped/pre-world readiness.
Previous flashlight state/lifetime scenarios and six controls pass. Current
combined build/related regression evidence is in qualification-current; TESTLOG
records exact source/DLL identity. Rendered joins/reconnect and light visibility,
full regression and package qualification remain open. Unknown recipients need a
new valid queue request; these checks do not make net-thread admission atomic or
retract an accepted packet. No version bump, archive, install, commit or publish.

## Earlier audit — missing companion ATV, flashlight, console and lights

The owner's latest request covers all four paths; only one ATV appears for
everyone. Versions remain 0.9.2161 / b2217 / protocol 2217. Ten individually named
fixes are recorded in TODO: retryable hit installation, per-pump ensure outside
the initial latch, impulse-frame bounds, independent honk/mount retries, initial
light-group watch recovery, running-session light guards, checked begin spawn,
checked finish/result cleanup, checked rig teleport arguments and finite yaw.

The confirmed ATV blocker was two latches: a failed first hit-guard installation
could never retry, and the controller called it only during initial setup. Tick
requires the guard before it reaches companion creation. Both paths now recover;
the seven guards remain mandatory. The parking checks still reject walls, pits,
unknown trace results and tilted rigs, and saved second ATVs keep their position.
Whether this blocker caused the owner's particular in-game failure remains open.

The audit also found merged duplicate code in the production console wrapper,
which failed compilation: duplicated layout helpers, factory calls, argument
reads, native returns, input/clear statements and an unmatched nested panel loop.
Remove only these duplicates, retaining the existing ownership checks. Repair
duplicate flashlight test definitions/cases and update the old 2149 mount fixture
to the current stamped-seat/syncer model; deleted authority routines are not
claimed as tested. Ten source fixes plus this additional console repair landed.

PASS 11 selected current groups, combined Release/ABI and production 2161 label.
Additional compiled removal controls: console 23, flashlight state six, remote
click ten. Evidence and current source/DLL hashes are in qualification-current.
TESTLOG records exact scope. Rendered garage spawn for host/clients, reconnect/
saved-position behavior, flashlight/group/ambient lights and concurrent console
acceptance remain open, along with full regression/package qualification. The
targeted flashlight late-join retry work is completed in the continuation above. No game
launch, installation, archive, commit, publication or version change.

## Earlier console native-operation continuation

User allocation: this session owns console work; flashlight work belongs to the
other session. Versions remain 2161/b2217/protocol 2217. Ten individually named
fixes are in TODO: live layout bindings, atlas identity, factory ownership,
native init, command-field admission, writeToLog preparation, box class, clear
preparation, bounded/transactional argument reads and bounded panel enumeration.
Widget helpers retain object/class/world and layout revision through callbacks.
The command verb verifies that its field still contains the intended command;
clear resolves both offsets before either write and stops on retirement. Frame
header reads require room for the full FString/type, and output text/type commit
together. Panel enumeration checks current desk class, TArray capacity/count and
header identity before following entries. A stale factory result of the captured
terminal class is marked for retirement instead of being published.

PASS ten new real-wrapper/ParamFrame/FString groups, existing input and metadata
checks, 21 controller callbacks, 14 delivery groups and 23 compiled controls. A
current cooked asset check confirms clr resets consoleLine/logText and writeToLog
uses consoleLine. Both production TUs compile. Console-only evidence and source
hashes are in `build/sat-console-callbacks-check/console-verification.json`;
TESTLOG records the limits. Combined Release/full
regression/package and rendered multiplayer acceptance remain open. No flashlight
source or shared qualified DLL is changed by this console continuation. Native
argument/property schemas, session predicates inside all helpers, atomic rollback
and uncertain-call retry semantics are not established by these guards.

## Earlier continuation — ten console metadata and flashlight lifetime fixes

Four completed fixes in ue_wrap/desk/sat_console.cpp: keep throttled metadata retries
after the former five-pass permanent shutdown; clear every partial offset before retry;
retain and validate eleven class/function/CDO incarnations before using cached offsets;
and validate bounded FString headers/capacity/storage/terminators for command, output,
name and history reads. Replaced metadata invalidates the ready latch and can resolve
again. Reflection resolution also refuses recursive entry. Versions are unchanged.

PASS the 21 existing controller scenarios, actual-wrapper tests with four new groups,
thirteen compiled removal controls (nine controller, four wrapper), and native wrapper
production-header compilation. The six flashlight fixes below are also completed;
five selected current groups, Release/ABI and production label formatter pass. Current
qualification-current evidence describes the combined stable source, with FullRegression=false.

The owner clarified no other writer is active and authorised reconciliation. The
observed temporary Seat/Scope/Publish edits are superseded by one SessionScope and
PublishState implementation. Six completed fixes: retire receiver/cache/publication
state on session pointer/start serial/world/role/running/link transitions; retire a
deferred state and host cache when roster person/transport generation changes; require
exact registry actor-to-slot association; copy received payloads before callbacks;
validate source/binding before the first setter and again after binding getters; and
own send completion by publication revision and local pawn incarnation. Nested identical
publication cannot recurse into another send. A nested different state supersedes old
completion, and a late older send invalidates dedup so a fresh sample repairs transport
ordering. The debug toggle samples and publishes on the game thread through PublishState.

PASS six new flashlight scenarios and six separately compiled removal controls in
run_2145.ps1 -FaultControls. TESTLOG records final combined evidence. Rendered two-player
beam/mode/battery/join and console concurrent-author/clear/history acceptance, full
regression and a new package remain open. Targeted host late-join retry is now
implemented in the join-delivery continuation above; rendered convergence stays open.
No install/package, version bump, commit or publication was performed by this agent.

## Flashlight and console priority continuation — 26 October

Owner prioritises flashlight operation and sharing each player's console text.
The ten individually named fixes are in TODO: exact local publisher; coherent
player/light sampling; finite/range validation before apply/cache; newer-state
interruption; nested echo restoration; exact console POST matching; owned body
attribution; no truncated command execution; no malformed output escaping local
synchronisation; and unchanged-state publication after local pawn reincarnation.
Wire/public/internal versions stay 0.9.2161/b2217. Console bodies now retain their
function incarnation, stack, locals and watch tag, and readers receive a value copy
instead of a pointer into a vector that callbacks can reallocate or erase. A shared
command over 255 characters remains editable in the input rather than executing
its shortened prefix. Invalid native output is cancelled rather than rendered on
one peer alone. Flashlight signature float reads also use defined bit_cast.

PASS five selected current groups, Release/ABI and label formatter, plus 21 callback
scenarios and nine compiled console removal controls. TESTLOG records current
evidence. These checks establish the named code paths, not a reproduced diagnosis
of every owner-visible failure. Rendered two-player beam/mode/battery/joins and
concurrent console input/clear/history/reconnect remain open. Only submitted output
is shared; unfinished keystrokes are outside this change. Full regression and a
new package are unperformed. No installation, commit or publication.

## Remote flashlight-click continuation

Development remains 2161/b2217. Ten fixes cover invalid slots, registry association,
puppet/class/world currency, session/world/person/body dedup scope, retryable
preparation, sound-cache lifetime, attenuation-cache lifetime, per-slot reentry
ownership, finite positions and refusal of unattenuated fallback. The receiver
reserves a revision before resource/location calls and validates the same source
after each callback-capable preparation step. Same-state nested calls do not
duplicate work; a newer different state supersedes older preparation, including
a return to the last admitted state. The latch commits immediately before the
one native attempt and does not overwrite a nested completion afterward. Resource
caches hold checked object/class references; attenuation construction is guarded
against recursive allocation and the constant shared resource may survive a world
change. Native one-third attenuation tuning and local sound are preserved.

PASS ten new 32-slot groups, ten compiled removal controls and two production-TU
compiles. Three related groups, Release/ABI and the production 2161 formatter pass.
The runner is added to the current suite; qualification is recorded in TESTLOG.
Rendered acceptance, full regression and a new package qualification remain
open. Retry requires a subsequent apply; the void native playback wrapper does not
report audible completion or expose its internal dispatch admission. These checks
do not establish complete reflected-call metadata lifetime inside that wrapper.

## Held-toy audio continuation

Development remains 2161/b2217. Ten fixes cover full session scope, sender and
recipient connection identity, frozen pre-native hand/action provenance, exact
bounded PRE/POST ownership, admission before pulse dedup/rate charge, owned decode
payload, shared send budget, strict expiry/clock recovery and exact recipient
completion. Receive reconciles person/transport changes and freezes source proof
through authorization, native presentation and delivery. PRE reserves capacity
before callback-capable parameter reads and keeps the original actor, person,
generation, invocation/function/stack/tag. Unknown hand generation cannot be
adopted by a later hand. Native local sound is untouched; outgoing cosmetic replay
waits only for the already-addressed remote mirror. At most 64 send attempts run
per tick; transient expiry is strict at 1,500 ms through native/send callbacks.
Clock rollback retires captures/queues/painted rows while keeping receive watermarks.
Delivery completion compares exact captured recipient identity, preserving debt
installed by a reentrant join. The wire layout, public version and protocol stay.

PASS legacy held/world toy and contact fixtures, ten new 32-seat lifetime groups,
ten compiled removal controls and seven production-TU compiles. Eight related
groups, Release/ABI and the production 2161 formatter pass; details are recorded
in TESTLOG. Rendered acceptance, full regression and a new package
qualification remain open, as do hook-function/schema lifetime and bounded native
presentation/authorization work. Initial-hand readiness needs rendered acceptance.

## Watoy native-wrapper continuation

Development remains 2161/b2217. Ten fixes guard field object/class/world identity,
unknown zero-component metadata, complete source bindings, activation admission,
audio preparation, service CDO lifetime, mesh/template currency, checked/finite
native outputs, spawned candidate validation/cleanup and direct View reentry.
Read publishes complete state only while the sampled Audio/a links remain current.
Activate accepts the controller's requester predicate; ApplyAudio accepts the
presentation predicate and checks it between native query/volume/pitch/start calls.
Spawn owns source/mesh/template/service identities, refuses unreadable/non-finite
results and validates a particle-component result before publication. Stale owned
candidates are released separately from the live View. View revisions prevent an
older Apply/Clear from replacing or disabling a fresh same-View owner after reentry.
No wire layout, public version or protocol change.

PASS ten new actual-wrapper groups and ten new compiled removal controls, all
existing watoy controller/wrapper groups and ten controller controls, plus four
production-TU compiles. Seven related groups, Release/ABI and the production 2161
formatter pass; qualification is recorded in TESTLOG. Rendered acceptance
and a new full regression/package qualification remain open. Full reflected
parameter schemas/function ownership, cleanup across independently copied View
owners, original-hand provenance and native-work budgets are not established.

## Watoy controller continuation

Development stays 2161/b2217. Ten fixes cover full session scope, exact activation
POST, bounded PRE reservation/pruning, requester connection identity, recipient
connection identity, snapshot-based state flush, retired drain ownership, exact
revision pruning, cue admission before deduplication and owned receive payloads.
Requester person/transport/peer epoch remains checked through authorization and
native state reads, including deferred requests and remote-hand presentation.
Recipient snapshots prevent old state/cue debt from crossing slot reuse; join
explicitly binds the current recipient. Outbound state is copied before sends,
then completion re-finds the matching revision and recipient. Drains abort after
session retirement and validate detached retries before restoring them. Pruning
copies rows, checks the exact revision and detaches views before native release.
Teardown leaves a freshly installed session intact. No wire or version change.

PASS legacy controller/native wrapper, ten 32-seat lifetime groups, ten compiled
removal controls and four production-TU compiles. Seven related groups,
Release/ABI and the production 2161 formatter pass; details are recorded in TESTLOG.
Rendered multiplayer, full regression and
new package qualification remain open. This does not close native-wrapper
preparation lifetime, original hand provenance, bounded native work or full-save
parity; those are separate unverified implementation/acceptance boundaries.

## World toy music/squeak continuation

Development stays 2161/b2217. Ten fixes cover session scope, requester transport,
recipient transport, exact PRE/POST, reserved PRE capacity, admission before pulse
deduplication, owned receive payload, shared send budget, callback expiry/clock
rollback and consistent key validation. PRE records retain live function identity
and stack alongside invocation/actor. Requester proof is frozen before lookup and
authorization. Existing person/peer epochs now also reconcile transport replacement.
Reliable retries retain FIFO; all three outbound paths share 64 attempts per tick.
Pulse expiry is strict at 1,500 ms, including native preparation callbacks. A clock
rollback retires queued events/captures and cached state for fresh sampling while
preserving received sequence watermarks. No wire layout or version changes.

PASS ten new 32-seat lifetime groups, ten compiled removal controls, existing
wrapper/phase/clock fixtures and four production-TU compiles. Current focused
qualification passes seven related groups, Release/ABI and the production 2161
formatter; details are recorded in TESTLOG. Rendered multiplayer, full regression and
a newly qualified archive remain open; no new archive is prepared by continuation.

## Sponge-foam continuation

Ten implemented points are individually listed in TODO: full session retirement,
exact invocation/POST pruning, reserved PRE ownership, frozen item/hand identity,
target/class/key authorization, source occupancy, recipient occupancy, admission
before deduplication, bounded work/expiry/clock handling and native trace ownership.
Targets keep source, actor/class, key, person, hand and connection proof through
callback-capable work. Retained detached rows are revalidated before queue merge.
At most eight authorization/presentation rows and 64 send attempts run per tick.
The 1,500-ms transient TTL is checked across callbacks as well as on initial drain.
Presentation rotates by owned event ID independently of ordered wire delivery;
unready mirrors do not spend the authorization budget or starve later ready effects.

PASS the existing controller/wrapper, ten lifetime groups, trace replacement cases,
eleven compiled fault controls and four production-TU compiles. Five related groups,
Release/ABI and the production 2161 formatter pass. Current evidence:
`build/qualification-current/verification.json`. This is
focused evidence; a new full regression/package qualification remains open.
Foam never replays native cleaning or debits soap; rendered host/client acceptance
and ancillary native-effect parity remain open. Existing canvas/grime/save gaps remain.

## Native canvas preparation and draw admission (b2217)

Ten fixes replace stale layout/function caches, validate raw observers, guard
complete brush reads and refuse unknown stock/material values, check factory
bindings and MID type, protect pin/teardown ownership, fail closed on unreadable
signal state, fence native source/target/component/function preparation, validate
all argument offsets/sizes/Vector2D schemas, and retain pre-admission Retry rows
without repeating an admitted uncertain draw. Native canvas owns open/join and
its 0.25-second retriggerable close; the wrapper never calls setDraw/endDraw directly.
Only the audited base canvas entry can prepare a retry; inherited subclasses are
accepted, while unverified overrides wait. Finite negative final-use soap remains
valid by the native subtract-without-clamp/getStrength contract. Vector2D schema
identities and parameter links remain checked through actual draw admission.

PASS 14 controller and 14 actual-wrapper/ParamFrame scenario groups, 17 compiled
fault controls, both production-TU compiles and the cooked 0.9.0n canvas contract.
The existing `run_window_stroke` group covers both layers. Historical pre-foam
b2217 evidence retains its mistaken label in `build/release-2162/verification.json`
and the earlier `build/package-current/verification.json`; current focused evidence
is under `build/qualification-current/`.

This implements specific callback/metadata lifetime guards left open in 2161;
rendered multiplayer still needs acceptance. Brush values are first sampled at
outgoing drain; original-dab hand/material provenance and atomic material-scalar
sampling are not established. Thrown sponges, independent periodic grime,
dynamic-source completeness and full-save parity remain open.

## Bay-window stroke delivery (b2216)

Ten fixes retire work across all session/world/role/start/stop/link transitions,
retain refused FIFO dabs, seal brush payloads once, bound sends and expiry, own
callback completion, validate ranges and ready origins while accepting host-relayed
slots, preserve admitted rows at capacity, track serial/world scan identity,
guard each replay and recover hook installation/arming/function replacement.
`run_window_stroke.ps1 -FaultControls` passes 13 scenario groups, seven compiled
protection-removal controls and a production-TU compile. The runner is included
in `package/check_current.ps1`; historical b2216 qualification remains in
`build/release-2161/verification.json` and TESTLOG.

Native draw admission remains at-most-once even when its result is uncertain.
Brush values are sampled at first drain, so exact original-dab hand/material
provenance is still open. Thrown sponge strokes and independent periodic grime
remain unchanged; dynamic-grime completeness, full-save parity, complete wrapper
callback/metadata lifetime and rendered multiplayer acceptance are not established.

## Version-start housekeeping

Before every new version, preview and run `package/cleanup_current.ps1`, review
TODO against implemented work, and update TODO/WORK in place. Keep the active
source/tests, dependencies, current qualified package/DLL and unique verification,
crash and research evidence. Remove obsolete delivery copies, generated test
outputs, caches, staging and redundant publication drafts/backups.

The historical cleanup before the mistakenly labelled 2162 snapshot removed 752 files / 535.9 MiB and records retained
SHA256 values in `package/cleanup-inventory.json`. The cleanup now reads the current
archive from the packager's canonical `dist/` inside the checkout, matches names
with the ElvardFIC namespace and removes superseded versions without hardcoded
215x ranges. The workspace README identifies current 2161 development.
Historic release reports and source handoffs retain their original evidence;
TODO/WORK own current status. Gameplay acceptance remains open as listed below.

## Historical full-suite baseline

The pre-foam b2217 snapshot passed 95 groups and package/ABI checks under the
mistaken 2162 label; `build/release-2162/verification.json` preserves that evidence.
Those results do not qualify the changed foam controller/trace reader. Current
public development remains 2161 with focused evidence under qualification-current.
Upstream `ad5315611` and local native-action fixes remain integrated. Rendered
gameplay, full-manager startup, fresh full qualification and package preparation remain open.

[TODO.md](TODO.md) owns priorities and report checklists.
[TESTLOG.md](TESTLOG.md) owns completed-change reports, hashes and evidence.
[ENTITY_COVERAGE.md](ENTITY_COVERAGE.md) owns native mappings and per-class gaps.
This file keeps implementation boundaries and unresolved questions.

The owner now requests **10 concrete fixes per continuation**. TODO records
completed implementation with [x] separately from remaining work and rendered
acceptance; changing that status does not claim new gameplay evidence.

## Upstream integration awaiting gameplay acceptance

The complete upstream ancestry is merged without resetting or rebasing our
commits. Keep 32-player admission, stable published message values, owned native
lifetimes and record queues, personal PC explosions, ATV steering/cockpit/radio
state and blast-driven prop discovery. Adopt upstream host-judged damage,
creature hits/presentation, ATV authority/join sets, shared-prop transactions,
notebook/paper editing, explosion requests and device/UI/localization changes.
Actual received ATV condition results and post-application prop records are
published only after native application. Two queued drone timer presses toggle
host state twice. Details and verification boundaries: [TESTLOG.md](TESTLOG.md).

Rendered host/client acceptance remains open for these combined paths, including
concurrent actions, driver handoff, force-only blasts and late join. Every peer
requires b2217 for current checks. Current 2161 development is recorded above; no game
installation or master deployment was performed. Older acceptance items remain open.

## 24 October report batch

b2200 gives transient `dirthole_C` actors bounded, world-scoped identities and a
validated position/rotation/scale/soil record on the existing placed-object lane.
Both authors, acknowledgement, retry, retirement and late join use that lane.
Mirror construction waits for the BeginPlay suppression watch: replaying native
BeginPlay would search buried treasure and create loot again. This does not add
save-file persistence or complete buried-item/treasure transaction authority.

Can mutations first drain pending canonical state; the host prevents ordinary
client records from increasing portions, closing an opened can or changing its
type. Initial saved births are exempt. Other consumable families still need
their own native contracts and concurrent-use transactions.

Satellite claims use physical-panel position and the shared widget's native
`used` owner. Missing unpublished ATV bodies release the retry latch. Location
events retain remote activation but no longer gather participants. Host sleep
uses personal restoration until every connected player reports rest; awake,
joining and dreaming participants prevent global acceleration.

Report (2)'s item-copy trigger/build are unknown. b2202 implements a dedicated
phase adapter for report (6.1)'s blank-email candidate, `arirShipAppear_C`:
host visible/alpha, native light/radar, current-state joins and short-lived live
air cues reuse scene actors. Clients stop old timelines and refuse their own
camera/day disappearance and delayed mail chains. Exact scoped native trigger
calls bind/unbind only the photo delegate; local photo checks/rewards stay native.
The host still owns eligibility, animation timing and mail generation. The
original rendered reproduction, `arirShipVanishing_C`, other event variants and
shared reputation consequences remain open. These are distinct from actor spawn.
The optional bed-capacity suggestion is deferred; class-only sleep reports do
not identify a particular mattress. Rendered acceptance remains open for all
implemented fixes; the game disk is unavailable. See TODO's batch-specific IDs.

Artifact cleanup completed on 24 October 2026 through
[cleanup_current.ps1](../package/cleanup_current.ps1): 3,833 generated/obsolete
files removed (2,638.6 MiB), including old builds/staging and release archives.
Current DLL/dependencies, package 2158 and verification/crash evidence remain;
current DLL and delivery archive hashes are unchanged.

## Development changes awaiting acceptance

| Area | Implemented boundary | Still open |
|---|---|---|
| Prop actions/MRE delivery (b2214) | Source- and target/class-owned, numbered FIFO entries reserved before send; copied payloads, nonreentrant eight-row drain, exact completion, bounded refusal/expiry and replacement-queue preservation. Client world-MRE refusal/dedup/cue lifetime; active personal spawn scopes with exact source/function/player/invocation proof, including a cleared hand. Received opens guard authorization/body/metadata/mesh and retain an at-most-once admitted-dispatch latch; safe record capture/retry owns its revision. | Rendered host/client opening, actual accepted output/debit/refund recovery after uncertain native failure, remaining package subclasses, poison-pour/roach effects and native reward routing. The latch is conservative admission, not proof that loot committed. Local host-native actions are not converted into a received transaction. |
| Food callbacks/output (b2213) | Bounded reserved eat observations; known valid portions/address, exact source/actor/class/key currency and POST ownership; dead final-bite request retry; active native eaten provenance for box/banana/cup, including hand-cleared final bites; weak function/owner and invocation retirement. Host final output rechecks canonical zero portions after publication and metadata preparation. Six personal overrides and empty base are excluded from host replay. | Cross-peer concurrent portion/output arbitration and optimistic personal effects before host verdict, other package subclasses, full-save behavior and rendered multiplayer acceptance. Existing personal effects remain on the eater; this does not turn local feedback into an acknowledged transaction. |
| Wall material delivery (b2212) | World/serial/role/running source resets; weak pair/class ownership before enrollment; protected preparation and one replay permit; guarded native metadata/query/component/function currency; fair paced retries before admission; no automatic repeat of an admitted spend; invocation-owned POST and partial-state publication; nested queue/install preservation and independent retries for six native guards. Solo-host and last-peer native ownership remain. | Paid client repair request/result and exact personal-stock accounting across hand/slot/drop, host refusal and disconnect. Client repair still refuses without spending; rendered native impact/build/destruction acceptance remains open. |
| Local container recovery (b2211) | Pre-debit bounded owned escrow for base world-container extract; exact native debit/pocket receipts; checked full-record restore; failed/null/partial spawning and full-inventory refusal settle by retiring the exact carrier then atomically returning its saved record. Refund retries preserve current other rows/newer same-key data, notify the contents lane and rebuild names before guarded volume/mass. Source/class/invocation ownership and best-effort stopped-session settlement protect replacements. | Cross-peer debit/output arbitration, personal extraction overrides/raw takeObj paths, rendered acceptance and guaranteed disconnect recovery. Missing debit/output receipts retain unresolved bounded debt; no class-only actor guesses or speculative refunds. Debt is not persisted across world/session replacement. |
| Console input/playback (b2210) | Consume-before-submit with guarded native textbox preparation and same-input reentry refusal; captured source/widget/host binding through reads and counters; bounded invocation records preserve nested clear/output and prune skipped POST; exact POST/native busy ownership with paired graph-watch rollback; source/target-owned display retry, clear revision, host-binding text/busy retirement and single-invocation argument-matched replay. | Rendered client input, simultaneous console use and native command world effects. These local regressions do not close Rufus/thiccfus, llama, Madness/event variants or other entity effects. |
| Console delivery (b2209) | All 31 remote terminal seats; source and recipient incarnation guards through native command preparation and wire chunks; owned, identified queue entries with nonreentrant flush; revision-owned history retry and current busy reseed; consistent nested clear ordering across observers; widget-bound local holds and both busy levels; detached terminal/session teardown preserves nested installs. | Rendered simultaneous console use, real input and world command effects. Rufus/thiccfus, llama, event variants and other entity effects remain separate work. Current-state history is bounded; it does not replay native commands. |
| Menu/branding cleanup (b2208) | Removed the tester/reviewer roll, embedded list, startup/cache/parser and browser HTTP path; retired client API and build inputs. Current startup/README/package descriptions and Sponsor metadata no longer promote the original author. License/third-party notices and historical provenance remain. | Visual main-menu acceptance after restarting with the current DLL. No published archive or remote service was changed. |
| 2–32-player selection (b2207) | Ten completed fixes: separate total-capacity row and presets; remembered typed default; shared native/overlay selection with deferred slider writes; safe build/menu cleanup and endpoint controls; environment config, legacy HostLobby and environment announce propagation; normalized re-announcement; coherent browser/master counts with overflow-safe parsing. All 31 limits pass the real hosting controllers across both save choices, modes, listing and offline fallback. All 118 master tests and its local Release build pass. | Rendered selector input/layout, real two/32-player admission/full/reuse and larger-group performance. Matching remote master deployment remains unrecorded; no package or live service was changed. |
| Sleep synchronization (b2206) | Ten fixes: retained host flags and peer relays; complete awake-inclusive join repair; generation/readiness-bound votes and delivery; installed-session paced payments; pending wake/dream veto; native host-dream veto; active canonical wire input; world/session/role reset; exact client payload and nested-send completion. Current-state debt is bounded by seats and retries refused recipients at 100 ms. | Rendered unanimous clock, wake/dream and join/replacement transitions. Local/native nightmare probability and rewards stay personal. Packets identify bed classes, so the optional per-mattress capacity suggestion remains deferred. |
| Inventory/profile safety (b2205) | Ten fixes: strict personal, equipment/hand, world-slice and target outer headers; checked whole held-record staging; current native volume/name callbacks; live join restoration; coherent revision/request ownership; exact-recipient guarded host preparation/chunks; generation/GUID-bound sent latches and retries. | Rendered join/rejoin, held/container UI recovery and replaced recipients. Outer-header validity does not validate every nested record or allocation extent. New profile arrival cancels an old restore; it does not automatically restart that consumed save-ready hook. Original item-copy cause and removed-row/carrier refund accounting remain open. |
| World containers (b2204) | Five fixes: shared masked/world-slot boundary; checked scratch and retry without false publication; scoped current actor/component/store/sender commits and relays; revision-safe park replay/retirement; exact-recipient relay refusal debt. Birth completion precedes native post-commit callbacks. | Rendered concurrency and UI/volume recovery; original unknown item-copy reproduction; native removed-row/carrier refund accounting. Partial unpublished nested scratch retains the codec's pin policy. |
| Inventory restore (b2203) | Four fixes: checked preparation of all three arrays before publication; owned-input and actor/class/world/save/header/nested-call guards; masked Player plus exact personal slot zero; class-incarnation field caches shared across personal/world readers. | Rendered join/rejoin and profile restoration; unidentified original item-copy trigger; removed-row/carrier refund transaction and broader writer/debit arbitration. Failed unpublished nested scratch follows the existing buffer pin policy. |
| Blank-email Ariral appearance (b2202) | Three fixes: shared phase/material/light/radar; client-local appearance/disappearance/mail suppression with scoped local photo listeners; quiet current-state joins and retained guarded delivery/application. Indexed scene lookup, unique position fallback, native timeline stop and bounded one-second cosmetic cue retry. | Original rendered warp-arrow reproduction; other classes including arirShipVanishing; shared reputation and broader event completeness. No history replay or player movement is needed by this adapter. |
| Container births (b2201) | Three fixes: native-body extraction scope, incarnation/out-frame result validation, and retained ordered birth/state publication. Generic finish publication defers to that owner. | Rendered ejection/extraction, root cause of the original client item copy, and the separate removed-row/failed-carrier refund transaction. |
| Records (b2194–b2197) | Initial-author waits, capture readiness, guarded native dispatch and full-record carrier lifetime checks; failed restoration refuses pocket capture. | Cross-peer writer arbitration and debit/output/refunds. Extraction removes the native row before restoration: retain/retry its saved record or refund it on failure. |
| Materials (b2193) | Exact stock/row/hand eligibility is checked through capture and publication; superseded work stays dirty for a fresh attempt. | Delivered chunks cannot be recalled. Concurrent writers, dryTimer ownership, hand transitions and accounting. |
| Grill (b2191–b2192) | Ordered coal/lid work and held rows cancel across session restart, replacement and recipient retirement; interrupted sends retain delivery debt. | Personal debit, cooking output, refunds, disconnect accounting and gameplay acceptance. |
| Blast motion (b2189–b2190) | Accepted host blasts, including zero damage, discover eligible props at native impulse and use the existing drive/rest publisher. Native LAN covers ordinary and force-only cases. | Broad host/client physics, filters, simultaneous impulses, pickup/handoff and late join. Keep lanes 188/191; upstream ExplosionSpawn=177 conflicts with KitchenState=177. |
| ATV (b2188) | Steering replicates; join snapshots retain pending honks. Native LAN covers both directions and late join. | Rendered steering and driver/carrier handoff on current DLLs. |
| Bleeding (b2187) | Drop parameters reach ready observers, excluding the injured owner; damage and blur remain personal. | Rendered injuries, stopping bleeding, mid-bleed joins and refusal/reconnect. Exact stain layout, historical-stain joins, cleaning parity and full-save persistence remain unverified. |

Native blast/ATV evidence belongs to earlier DLLs. Offline/native checks do not
close rendered gameplay or real keyboard-input acceptance on b2217.

## Runtime investigations and implementation gaps

- **Bench58:** host old rifle vanished after placement. `prop_rifle_C` is a
  candidate, not a confirmed cause. Capture class/key, destruction caller/timing
  and drop/sweep logs with `VOTVCOOP_PROP_BIRTH_KEY_PROBE=1`.
- **Token52:** probably shared `saveSlot.Points`; confirm the original UI and
  accept credit/spend/refusal/join. Delivery guards and queued-credit cancellation
  across world replacement are implemented.
- **Trowel:** stock persists into `hold[0]`, including zero. Native graphs preserve
  non-None saved keys; capture a real mismatch before designing key repair.
- **Personal resources/paid repair:** complete atomic debit, accepted output and
  refund across rejection, retry, disconnect and hand/slot/drop. Persistence and
  capture guards do not complete the transaction. b2211 completes the proven local
  base-container failed-extraction refund path; cross-peer arbitration and the
  receipt-less/persistence cases above remain open.
- **Remaining systems:** console/toy/entity behavior, package subclasses and
  cross-peer food transactions beyond b2213's scoped output provenance,
  dynamic grime and full-reload scorch, sky-event client phases,
  stow performance and model previews remain in TODO. Fish boundaries:
  [HANDOFF_FISH.md](HANDOFF_FISH.md).

## Gameplay, validation and delivery

Immediate-join flashlights/switches/doors/drone contents and rendered 2–32-player
admission still need acceptance. The chooser and local master implementation are
complete; confirm matching capacity on the deployed master. No remote deployment is recorded.
Boundaries: [SYNC_2157_FIXES.md](SYNC_2157_FIXES.md).

Selected upstream adaptations still need rendered checks:
[FORK_AUDIT_20261022.md](FORK_AUDIT_20261022.md). Its original unmerged snapshot is
historical; upstream ancestry through `ad5315611` is now merged.
Ragdoll reports 5/50 and stationary-PC loading 54/55 remain open; TODO keeps
input, transition, power-cycle and handoff cases. Optional authored PhysicsAsset/
display retargeting is separate from current acceptance.

Report IDs 1–58 remain tracked in TODO except closed Mining (6) and excluded
Wall41; chair reports 11/36 are duplicates. Previously working nightmares (10)
need acceptance of newer personal-sleep behavior. Use identical current DLLs and
record role, item/model, reproduction and logs in TESTLOG before closing reports.

The latest fact-row gate passes **565 rows / 181 files**. Remaining public-prose
The latest fact-row gate passes **565 rows / 181 files**. Remaining public-prose
counters: documents **85/82**, src.comment_doc_row **155/0** and
src.files_not_swept **84/0**, with reading-room checkouts absent.
Full-manager-package startup and rendered multiplayer remain unverified.
Requalify changed source through current package scripts/metadata. GitHub descriptions
for 2160 are published; the mistakenly labelled 2162 archive is historical.

## Requested event synchronization and frequency

Existing replay/join/location lifetime guards, readiness retries and cancellation
do not establish category-wide effects or phase parity.

The eight-category request remains open. The resumed fresh audit indexed 42,941
pak entries and extracted 37,971 uasset/uexp files (18,854 asset packages).
The relevant inventory decoded 1,621 Blueprint packages / 22,691 functions,
including 69 scheduled-event rows and 190 focused dumps, with no package errors.
The raw inventory's zombieHordeController/ufoAlienDropper rejections were hint-text
false positives. Rechecking all 22,691 functions found no actual decode errors:
`build/requested-events-audit/reclassified-graphs.json`; the raw inventory is retained.
Level/map headers were not extracted; those graphs remain incomplete.
Evidence: `build/requested-events-audit/`.
The imported `sync_native_contract_test.py` passes against this fresh corpus.
Native mapping and each event's effects/phase/join behavior still require proof.

| Requested category | Remaining implementation or acceptance |
|---|---|
| Dreams | Wake rewards in the 11 audited scenes are personal vitals/stats/achievements; scene-local boulders/mannequins do not justify shared reward props. b2199 prevents remote puppet/ragdoll overlaps from consuming a local dream burger. Native sleep/createDream/leaveDream and rewards remain personal; verify scene effects and lifetime on peers, preserving independent nightmare/Insomniac conditions. |
| Ariral Reputation | calcRep uses -75/-25/25/75 tiers; launchEvent is empty, tier listeners and mainGamemode feeding/wakeup/punch/gifts carry real effects. Complete shared state, randomized outcomes, relocation and late join. |
| Player-Activated | Map and verify every listed interaction, simultaneous activation and canonical side effects. |
| Time-Related | Fresh mainGamemode audit finds real-calendar branches outside eventer (June 28 21:00, April 1, November 29 22:49). Complete these, 3AM eligibility, chosen outcomes and uncovered effects. |
| Endings | Establish and verify each ending's per-player versus session consequence; do not replay quit/travel on remote puppets. |
| Story Mode | Complete actor/phase/audio/bookkeeping adapters and current-state late join. Audited horde/dropper need wave/class budgets, alive sets, countdowns, timeline and owner-local health/effects; replaying their start can repeat rewards/save work. Their active attack/animation cadence must not be frequency-scaled. |
| Random | Fresh inventory identifies remaining generators; complete tree visibility/motion/lifetime, camera-dependent bp7 player effects and protected-server consumption/audio/delegates. Black-fog client suppression alone does not mirror black fog. |
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
Development b2198 admits host-owned `ticker_flickerer_C`: only its native
30–60s eligible-roll wait is scaled. Generator-dependent probability, short
visual delays/repetition and the existing lamp/ambient effect transport remain
native. Source/cooked/live-reference regressions pass; multiplayer acceptance remains.
`treeSpawner` (600–900s walking-tree output), `bp7Spawner` (camera-dependent rare
integer roll and per-player pause/quit) and `serverBreaker` (native 1200–600s wait)
remain outside frequency coverage until their complete effects are transported.
Serverbox state delivery alone does not cover protective floppy consumption,
repair audio or delegate notification. Walking-tree appearance/render-dependent
movement and bp7 player consequences also need explicit routes.
Preserve native eligibility, probabilities,
personal dream rolls and story dates; adjust eligible-roll intervals without
changing AI/animation, effect durations or sleep needs.

Real-peer acceptance must cover chosen/repeated events, simultaneous triggers,
live/persisted frequency changes, load windows, late join, reconnect and teardown.

Development b2199 retains serverbox delivery debt per accepted recipient; refused
or partially refused fan-outs, new occupants and refused repair corrections retry.
Clients retain the latest snapshot until native lists/fields and writes are ready;
unfinished re-skin work retries even after IsBroken changes. Box incarnations and
world/session/gamemode/occupancy changes cancel old work. Production regressions
reproduce the old lost retry; protected-server generator contracts also pass.
Dream-burger guards use incarnation-bound function offsets and frame bounds;
local/nonplayer native collection remains intact. Three removal controls fail.
These fixes do not close the category-specific phase or resource transactions.

Runtime investigations remain open. b2198 cancels queued balance credits when
the world changes. Fresh trowel graphs preserve a non-None saved key through
loadData/init/getKey/assignKey; capture a real mismatch before designing repair.
Two isolated b2198 launches overflowed the native VM stack before acceptance:
dumps prove recursive lib_C.end; Untitled_1.CustomEvent's borders guard is the
strongest initial-caller candidate, pending live confirmation. Evidence:
`build/requested-events-native/crash-analysis-20261024.md`. Do not suppress the
native guard. The owner confirmed the game disk is absent on 24 October, so
current-DLL rifle/trowel/balance and real-peer event checks cannot yet run.
