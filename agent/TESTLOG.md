# TESTLOG - verification and open reports

Development target: **0.9.2164**. Last qualified offline package: **0.9.2163 / b2217**. Implementation backlog: [TODO.md](TODO.md) and [WORK.md](WORK.md). Maintained test coverage: [tests/README.md](tests/README.md).

## Owner disposition (11 November 2026)

The joined-client prop jitter/support-penetration report was considered resolved
and skipped at the owner's direction. No code change or gameplay validation was
performed for that report in this turn. Reopen it if the symptom recurs; the
separate handoff/support-contact ownership review remains open.

## Held-toy variant hook coverage (11 November 2026)

The Dingus wrapper and held-toy controller now share one list of catalogued
Dingus classes. The controller registers class-scoped PRE/POST capture watches
for `playerHandUse_RMB` and `actionOptionIndex` on each class, so a variant
override can be owned by its declaring class without changing inherited calls.
The controller fixture asserts both watches for every catalog entry and the
separate ABPlush `player_use` watch.

PASS `run_2157.ps1 -FaultControls -CompileSources`: controller, native input
capture, world toy, held-toy lifetime/budget suites, ten fault controls and all
seven selected production translation units. PASS `run_toy_music.ps1
-FaultControls -CompileSources`: wrapper/ParamFrame, playback phase/lifetime,
clock wrapper, ten fault controls and four selected production units.
These fixtures use metadata/engine/transport doubles; actual variant override
schemas and in-game acceptance remain unverified. No Release build, package,
install or protocol change.

## Watoy physics-hook ownership (11 November 2026)

The extracted `prop_watoy` Blueprint calls `AddForce` and
`SetPhysicsLinearVelocity` on its `StaticMesh`. The native hook callback's
`sourceObject` is the Blueprint caller (the Watoy actor), while `context` is
the component receiving the call. The former lookup incorrectly treated
`sourceObject` as a `comp_zerog_C`. Ownership now accepts the live Watoy caller
directly and retains component-outer fallback; suppression still requires the
exact toy mesh. Added actor/component owner regressions and a fault control for
the former class-only lookup. Existing start-cue sequencing and activation
admission were left unchanged.

PASS `run_watoy.ps1 -FaultControls -CompileSources`: controller, budget,
lifetime, wrapper, physics-owner regression, all fault controls and four
selected production translation units. These offline fixtures do not establish
in-game timing or physics acceptance. No Release build, package, install or
protocol change.

## Thrown-sponge and water-projectile sync (11 November 2026)

The water-projectile fixtures cover both `prop_sponge_p` washer shots and
`prop_sponge_bucketPour` births, plus collision splash/audio, the client washer
emitter, owner/source admission, resource capture, transform/velocity
reconstruction, terminal retry/identity guards, and session-serial retirement
when a `Session` object is reused.

PASS `run_water_pour.ps1 -CompileSources`: controller, washer-shot, bucket-sound,
wrapper/ParamFrame, floor-birth fixtures and six selected production translation
units. PASS `run_sponge_effects.ps1 -FaultControls -CompileSources`: four
controller/wrapper/trace suites, ten controller fault controls, the trace-player
fault control and four selected production units. This evidence does not verify
in-game draw material/size, full foam coverage, or stock/save/join parity; those
broader acceptance items remain open. No Release build, package, install or
protocol change.

## Selected upstream and fork integration (8 November 2026)

M1 resolves five pre-existing merge conflicts; M2 removes duplicate protocol declarations; M3 inventories 70 public default fork heads, 69 available. M4 applies zero End velocities with existing lifetime guards. M5 recognizes canonical mirrors in placement/recovery; M6 protects them during module/upgrade ghost cleanup. M7 installs the personal-rest controller. M8 includes the Steam formatter definition. M9 counts the eight-byte floppy canonical header; M10 rejects claims past the sender ceiling before mutation. Per-item provenance and exclusions: FORK_AUDIT_20261022.md.

PASS 163 validation groups across the full run and scoped entity recheck on production stamp `0c6c9e5b155b0008e02abe70738d173e66551f2848bf7c595872218b0613b81e`. The full run initially failed only the legacy zero-velocity assertion; updating that fixture and rerunning its whole group passed without changing production sources. New cases cover zero End velocity, component replacement, registered mirrors, exact-capacity canonical packing and oversized claims. Seven changed production units compile. Native fact rows PASS (626 rows / 199 files); public_prose_gate still FAILS 14 counters, including 23 uncalled declarations and 156 unswept sources. Evidence and original failure/recheck records: `build/fork-review-20261108/verification.json` and `final/prebuild` beneath that directory.

No Release compilation, new DLL/package, installation, commit, push or version/protocol change. The old merge is resolved but uncommitted. Native multiplayer pickup/rest, cleanup and sleep behavior remain unverified; offline checks do not establish the original jitter report's root cause.

## HandTaken delivery and client cleanup (8 November 2026)

Implemented HD01-HD25 / HX01-HX50: confirmed notices retain recipient-owned FIFO debt after refusal, reserve bounded capacity before native spending, and complete only the accepted receipt. Context/recipient/body replacement cancels stale work; command/service retirement does not erase confirmed debt. Client cleanup captures typed hand fields and weak actor/class identities, preserves replacement hands and shared props, retries refused inventory writes and unattempted native refresh/release, and consumes dispatched native work once. Transactional inventory writes accept caller guards; native updateHold and physics-handle release revalidate their captured targets and preserve callback-installed grab fields.
Verification: PASS 75 named controller cases, native wrapper/inventory cases, 21 assertion-based fault controls and three production object compiles. Actual controller, service/Registry stack, extracted native wrappers and production inventory/save-record code run with metadata, dispatch, hand/puppet/transport and clock doubles. Full regression and repository gates are running. Exact item/operation correlation remains open because the wire notice carries only a class; a delayed notice can still match a newer same-class item. Native multiplayer acceptance and uncertain resource/latent reconciliation remain open. No Release build, package, install or version/protocol change.
Evidence: `build/handtaken-controls.log`, `build/handtaken-check`, `build/handtaken-verification/verification.json`. Each HX cell separately cancels stale continuation after the named operation/replacement; cleanup never erases a newer receipt.

| ID | Completed fix | ID | Completed fix | ID | Completed fix | ID | Completed fix |
| --- | --- | --- | --- | --- | --- | --- | --- |
| HD01 | retain confirmed HandTaken notices after transport refusal. | HD02 | bind notice debt to recipient generation and world-ready revision. | HD03 | detach notice debt on session/world/stop/role/connection retirement. | HD04 | reserve bounded per-recipient notice capacity before native hand commands. |
| HD05 | preserve FIFO notice ordering without blocking other recipients. | HD06 | complete only the exact notice accepted by transport. | HD07 | guard notice pumping against recursive Tick/native callbacks. | HD08 | retry immutable packets without rerunning native verbs or inventory work. |
| HD09 | cancel notices after recipient body/puppet replacement. | HD10 | preserve confirmed notice debt after command/service retirement. | HD11 | admit HandTaken cleanup only in a live connected client context. | HD12 | reject empty, oversized and non-ASCII class notices. |
| HD13 | own client cleanup by an independent receipt, superseded by newer notices. | HD14 | capture local body actor/class identities and revalidate its registry binding. | HD15 | use bounded typed nonoverlapping local hand field layouts. | HD16 | retain held/grabbed actor/class serials during cleanup. |
| HD17 | validate class observations before any client mutation. | HD18 | compare captured hand fields before clearing inventory or pointers. | HD19 | retry refused inventory writes while the same captured cleanup owns them. | HD20 | pass cleanup ownership into transactional held-inventory writes. |
| HD21 | guard native updateHold resolution/dispatch and distinguish unattempted work. | HD22 | destroy only the captured distinct hotbar mirror, preserving shared world props. | HD23 | guard native physics-handle release and preserve callback-installed grab state. | HD24 | freeze only the captured released prop, preserving replacement grabs. |
| HD25 | retire client cleanup on teardown and consume uncertain native attempts once. |  |  |  |  |  |  |

| Operation | World | Session serial | Body actor serial | Body class serial | Notice/cleanup receipt |
| --- | --- | --- | --- | --- | --- |
| Host notice send | HX01 | HX02 | HX03 | HX04 | HX05 |
| Client hand layout | HX06 | HX07 | HX08 | HX09 | HX10 |
| Held class read | HX11 | HX12 | HX13 | HX14 | HX15 |
| Grabbed class read | HX16 | HX17 | HX18 | HX19 | HX20 |
| Release stream arm | HX21 | HX22 | HX23 | HX24 | HX25 |
| Held inventory clear | HX26 | HX27 | HX28 | HX29 | HX30 |
| Native updateHold | HX31 | HX32 | HX33 | HX34 | HX35 |
| Old mirror destroy | HX36 | HX37 | HX38 | HX39 | HX40 |
| Grab handle release | HX41 | HX42 | HX43 | HX44 | HX45 |
| Released prop freeze | HX46 | HX47 | HX48 | HX49 | HX50 |

## Omega host command transactions (8 November 2026)

Implemented OC01-OC25 / OX01-OX50: command attempts own robot/registry, sender and session/world proofs through service assignment, native replay and carried-object work. Service snapshots support compare-and-set assignment, host reservations and receipt-bound rollback; startKill preserves newer assignments. Temporary puppet hand fields retain bounded layouts, weak identities and per-body receipts, restoring only unchanged staged values. Native actionName validates a typed bounded frame and owns its FString input. Dispatched failures remain consumed without guessed rollback/replay. Queues retire old context/sender/target work; the receiver admits all nine defined commands.
Verification: PASS 75 named cases, 24 assertion-based fault controls and three production object compiles. Actual Omega controller, service ledger, Registry/Element/claims and extracted native replay functions run with engine/metadata, hand/puppet/transport and clock doubles. All 162 full regression groups pass on stable production sources. Native fact rows pass (626 rows / 199 files); public prose fails 14 counters above baseline, including 23 uncalled declarations and 156 unswept source files. HandTaken delivery/client cleanup, uncertain native resource reconciliation and wider latent review remain open. No Release build, package, install or version/protocol change.
Evidence: `build/omega-command-controls.log`, `build/omega-command-check`, `build/omega-command-verification/verification.json`. Each OX cell separately cancels old command work after its operation/replacement; owned temporary-hand cleanup can still restore unchanged fields.

| ID | Completed fix | ID | Completed fix | ID | Completed fix | ID | Completed fix |
| --- | --- | --- | --- | --- | --- | --- | --- |
| OC01<br>OC05 | Bind Omega execution to current session/serial/stop/role/connection and world.<br>Capture immutable prior service assignments instead of only ServedSlot. | OC02<br>OC06 | Capture canonical NPC registry identity and robot actor/class serials before execution.<br>Compare-and-set client service assignment against the captured prior receipt. | OC03<br>OC07 | Keep sender generation and world-ready revision current through native command work.<br>Rollback only the captured attempt and a still-valid prior assignment. | OC04<br>OC08 | Reserve actor command attempts and reject recursive/superseded execution.<br>Represent host takeover with an owned no-client reservation. |
| OC09<br>OC13 | Bind startKill service removal to actor/function/context and exact assignment.<br>Capture local/requester body identity and revalidate puppet lookups. | OC10<br>OC14 | Guard kill-watch registration and settlement across callbacks.<br>Stamp staged hand body/class/context lifetime. | OC11<br>OC15 | Restore the previous host-replay recursion flag after nested/native exits.<br>Validate pointer/FName hand layouts by property type, size, bounds and nonoverlap. | OC12<br>OC16 | Reject unreadable job state before service/hand changes, preserving equipment exemption.<br>Retain weak identities of previous/staged held and grabbed actors. |
| OC17<br>OC21 | Restore only hand fields still containing this attempt's staged values.<br>Fence hold retirement, release publication and drive claims after native callbacks. | OC18<br>OC22 | Use per-body staging receipts so older cleanup preserves a newer stage.<br>Validate recipient and owned staged class before HandTaken publication. | OC19<br>OC23 | Guard native floppy/hold/viewer before-and-after observations.<br>Validate native actionName actor/class/function/player and bounded typed frame. | OC20<br>OC24 | Fence carried-object physics work by command/service/object identity.<br>Expose actual dispatch and avoid rollback/replay after an uncertain native attempt. |
| OC25 | Detach command/staging work on teardown and reject stale queued context. |  |  |  |  |  |  |

| Operation | World | Session serial | Robot slot | Robot class | Service/attempt receipt |
| --- | --- | --- | --- | --- | --- |
| Job state read | OX01 | OX02 | OX03 | OX04 | OX05 |
| Service assignment | OX06 | OX07 | OX08 | OX09 | OX10 |
| Local player lookup | OX11 | OX12 | OX13 | OX14 | OX15 |
| Hand-source lookup | OX16 | OX17 | OX18 | OX19 | OX20 |
| Pre-command floppy read | OX21 | OX22 | OX23 | OX24 | OX25 |
| Native replay preparation | OX26 | OX27 | OX28 | OX29 | OX30 |
| Native replay dispatch | OX31 | OX32 | OX33 | OX34 | OX35 |
| Post-command held-object read | OX36 | OX37 | OX38 | OX39 | OX40 |
| Carried-object physics | OX41 | OX42 | OX43 | OX44 | OX45 |
| HandTaken send | OX46 | OX47 | OX48 | OX49 | OX50 |

## Follow and served-player ownership (8 November 2026)

Implemented SF01-SF25 / SM01-SM50: robot/player assignments retain actor/class, recipient-generation/readiness and session/world proofs. Monotonic leases isolate stale cleanup; follow attempts and tick/departure walks own their records. Unreadable active state no longer falls through to Serve or destroys a valid assignment. Native seams retain function/owner identities and reconcile arm/watch decisions; unknown arm results retry. Player substitution validates the exact callee, current service and puppet identity. Main-player output and native insert selection use bounded typed function schemas; native field names retain case-insensitive FName matching.
Verification: PASS 75 named cases, 26 assertion-based fault controls and three production object compiles. Actual follow/service controllers, Element/Registry/claims and extracted native insert-read functions run with metadata, hook/watch operations, puppet access, engine/transport and clock doubles. All 161 full regression groups pass on stable production sources. Native fact rows pass (626 rows / 199 files); public prose fails 14 counters above baseline, including 22 uncalled declarations and 155 unswept source files. Native multiplayer acceptance and remaining Omega command/latent continuations remain open. No Release build, package, install or version/protocol change.
Evidence: `build/served-player-controls.log`, `build/served-player-check`, `build/served-player-verification/verification.json`. Each SM cell separately cancels the old continuation after its operation/replacement.

| ID | Completed fix | ID | Completed fix | ID | Completed fix | ID | Completed fix |
| --- | --- | --- | --- | --- | --- | --- | --- |
| SF01<br>SF05 | Fence follow/service work by session pointer, serial, stop, role, connection and world.<br>Release follow services by lease so stale cleanup preserves newer assignments. | SF02<br>SF06 | Stamp robot actor/class slot serials and retain owned records through native callbacks.<br>Stop OnToggle when active state is unreadable or ownership changes. | SF03<br>SF07 | Bind service recipients to slot generation and world-ready reset revision.<br>Own follow Tick snapshots and recheck the installed activator after reads. | SF04<br>SF08 | Allocate monotonic service leases and complete only the captured service receipt.<br>Reserve follow attempts before native reads/Serve and preserve reentrant replacements. |
| SF09<br>SF13 | Retain unreadable active-state debt; retire only confirmed off/dead activators.<br>Retain native seam function/declaring-class identities and retry missing resolution. | SF10<br>SF14 | Keep host-toggle cleanup isolated to the follow-owned service.<br>Own arm/watch decisions across callbacks and reconcile changed wanted seams. | SF11<br>SF15 | Reject invalid slots/seam masks and preserve valid older service after refused admission.<br>Reject partial seam activation; retain retryability after failed arm/disarm. | SF12<br>SF16 | Keep pump/arm recursion guards active through disconnect and retire context debt.<br>Validate exact native callee and current service before rewriting player-0 results. |
| SF17<br>SF21 | Validate puppet identity, pose and actor/class lifetime through body/result lookups.<br>Keep legacy Serve/Unserve/ServedSlot entry points compatible with owned receipts. | SF18<br>SF22 | Bound main-player out-parameter metadata by function identity, type and frame extent.<br>Use owned departure snapshots and preserve newer records during cleanup callbacks. | SF19<br>SF23 | Bind insert-read selection to caller function/frame and guarded native schema.<br>Clear follow/services without erasing records admitted by later reentrant work. | SF20<br>SF24 | Preserve native missing-body fallback and explicit null insert rejection.<br>Cache the native insert-read schema by live class/function incarnation and retry metadata. |
| SF25 | Bound native insert variable storage and cancel metadata/read continuations on caller retirement. |  |  |  |  |  |  |

| Operation | World | Session serial | Robot slot | Robot class | Service receipt |
| --- | --- | --- | --- | --- | --- |
| Follow toggle active read | SM01 | SM02 | SM03 | SM04 | SM05 |
| Follow tick active read | SM06 | SM07 | SM08 | SM09 | SM10 |
| Native seam arm | SM11 | SM12 | SM13 | SM14 | SM15 |
| Main watch arm | SM16 | SM17 | SM18 | SM19 | SM20 |
| Puppet registry lookup | SM21 | SM22 | SM23 | SM24 | SM25 |
| Puppet pose/body lookup | SM26 | SM27 | SM28 | SM29 | SM30 |
| Native result lookup | SM31 | SM32 | SM33 | SM34 | SM35 |
| Main out metadata | SM36 | SM37 | SM38 | SM39 | SM40 |
| Main out pointer resolution | SM41 | SM42 | SM43 | SM44 | SM45 |
| Insert-read selection | SM46 | SM47 | SM48 | SM49 | SM50 |

## Kerfus command admission and dispatch (8 November 2026)

Implemented KA01-KA25 / KB01-KB50: owned client FIFO retries preserve refused sends, while host commands retain target identity, sender generation/readiness and world/session scope. NoBody and undispatched metadata waits refund the token; a dispatch attempt is consumed even if native execution fails. PRE frames and FString data are bounded and copied safely. Native action frames validate types, extents, actor/class/function/player lifetime and dispatch receipts. Watch registration owns its scope and rejects recursive registration.
Verification: PASS 75 named cases, 25 assertion-based fault controls and two production object compiles. Actual command controller, native action wrapper, Element/Registry and claims run with metadata, reach judgement, follow attribution, engine/transport and clock doubles. All 160 full regression groups pass on stable production sources. Native fact rows pass (626 rows / 199 files); public prose still fails 14 counters above baseline. Follow/served-player lifetime, native gameplay and the wider upstream review remain open. No Release build, package, install or version/protocol change.
Evidence: `build/kerfus-intent-controls.log`, `build/kerfus-intent-check`, `build/kerfus-intent-verification/verification.json`. Each KB cell separately cancels old work after its operation/replacement.

| ID | Completed fix | ID | Completed fix | ID | Completed fix | ID | Completed fix |
| --- | --- | --- | --- | --- | --- | --- | --- |
| KA01<br>KA05<br>KA09<br>KA13 | Retire command work on session pointer/serial/stop/role/connection and world changes.<br>Validate host Prop identity/incarnation and canonical Actor/Class bindings.<br>Bound ingress queues and keep slot 31 independent from blocked peers.<br>Cancel queued client commands on mirror/actor/class replacement. | KA02<br>KA06<br>KA10<br>KA14 | Own queued command and target identity receipts across callbacks.<br>Use owned target snapshots before reach authorization, without borrowed Elements.<br>Guard recursive pumps and clear detached work on disconnect.<br>Validate PRE function identity, frame extent and action storage before reads. | KA03<br>KA07<br>KA11<br>KA15 | Bind host queues to sender generation and world-ready reset revision.<br>Retain NoBody waits without spending the per-peer token.<br>Retain client command FIFO after transport refusal.<br>Copy bounded FString headers/data without unaligned reads or access violations. | KA04<br>KA08<br>KA12<br>KA16 | Reject invalid host IDs, reserved bytes and unsupported shared actions.<br>Preserve one command per peer per tick and the native 10/10 token budget.<br>Pace client retries and complete only the captured head receipt.<br>Preserve native colour/local options and offline operation. |
| KA17<br>KA21<br>KA25 | Refuse client shared verb bodies even while disconnected or metadata is unavailable.<br>Validate bounded, nonoverlapping player/action frame fields and property types.<br>Preserve newer queued work after reentrant enqueue, disconnect or sender replacement. | KA18<br>KA22 | Guard PRE metadata continuation by caller/actor/class/function/context ownership.<br>Expose action dispatch attempts separately from native success. | KA19<br>KA23 | Keep presser attribution scoped to the exact host invocation; consume uncertain attempts once.<br>Preserve legacy action wrapper compatibility and retry missing metadata. | KA20<br>KA24 | Validate actionOptionIndex actor/class/function and captured player lifetimes.<br>Guard watch registration/live settlement and diagnostic counters after callbacks. |

| Operation | World | Session serial | Actor slot | Class slot | Element |
| --- | --- | --- | --- | --- | --- |
| Client action metadata<br>Client name metadata | KB01<br>KB06 | KB02<br>KB07 | KB03<br>KB08 | KB04<br>KB09 | KB05<br>KB10 |
| Client send<br>Host clock | KB11<br>KB16 | KB12<br>KB17 | KB13<br>KB18 | KB14<br>KB19 | KB15<br>KB20 |
| Host reach<br>Host class gate | KB21<br>KB26 | KB22<br>KB27 | KB23<br>KB28 | KB24<br>KB29 | KB25<br>KB30 |
| Host local-player lookup<br>Action function metadata | KB31<br>KB36 | KB32<br>KB37 | KB33<br>KB38 | KB34<br>KB39 | KB35<br>KB40 |
| Action parameter metadata<br>Native action dispatch | KB41<br>KB46 | KB42<br>KB47 | KB43<br>KB48 | KB44<br>KB49 | KB45<br>KB50 |

## Kerfus state delivery and refresh (8 November 2026)

Implemented KD01-KD25 / KX01-KX50: per-recipient state/join debt retains refused or unreadable work, validates recipient generations/readiness revisions, and completes only the captured receipt. Client state application retains individual writes and unfinished refresh work; actual upd dispatch is consumed once. Completed duplicate packets recheck declarative state, restoring local drift without replaying an unchanged active edge. Native layouts follow actor/class/ancestor identities, field extents and masks; missing metadata retries. Session/world/registry replacement cancels old continuations.
Verification: PASS 75 named cases, 19 assertion-based fault controls and two production object compiles. Actual manager, native wrapper, Element/Registry and drive claims run with reflected metadata, engine/transport and clock doubles. All 159 full regression groups pass on stable production sources. Native fact rows pass (626 rows / 199 files); public prose fails 14 counters above baseline, including 21 uncalled declarations and 154 unswept sources. Native gameplay acceptance remains unperformed. No Release build, package, install or version/protocol change.
Evidence: `build/kerfus-state-controls.log`, `build/kerfus-state-scoped.log`, `build/kerfus-state-check` and `build/kerfus-state-verification/verification.json`. Every KX cell separately cancels the old continuation after its named operation/replacement.

| ID | Completed fix | ID | Completed fix | ID | Completed fix | ID | Completed fix |
| --- | --- | --- | --- | --- | --- | --- | --- |
| KD01<br>KD05<br>KD09<br>KD13 | Retire state work across session pointer/serial/stop/role and world changes.<br>Validate finite energy and supported flags while preserving native energy above 100.<br>Acknowledge only the captured publication; preserve newer state and join work.<br>Preserve immediate flag edges and half-unit/one-second energy gating, including clock rollback. | KD02<br>KD06<br>KD10<br>KD14 | Own host and client registry identity snapshots instead of Element/container references.<br>Retain separate immutable delivery receipts for every recipient.<br>Enumerate owned late-join rows without iterators surviving native calls.<br>Accept state only on the current client, for valid host IDs and mirror props. | KD03<br>KD07<br>KD11<br>KD15 | Check canonical actor/EID, Element incarnation and actor/class serials.<br>Pace refused/unreadable retries without an attempt-count cutoff.<br>Read fresh native state for a queued join and retry unreadable joins.<br>Retain bounded latest-state apply debt and preserve the existing deferred-bind route. | KD04<br>KD08<br>KD12<br>KD16 | Preserve unenrolled sampling and acquire drive leases only for a valid current host prop.<br>Bind sends to recipient generation and world-ready reset revision.<br>Include slot 31 and bound/fairly rotate delivery and client-apply work.<br>Stop and retain progress between individual declarative client writes. |
| KD17<br>KD21<br>KD25 | Carry an unfinished active-state refresh into a newer packet for the same incarnation.<br>Bound native fields by type/extent and reject overlapping or nonfinite state data.<br>Guard metadata settlement and make HostKnows reflect only current canonical records. | KD18<br>KD22 | Consume upd dispatch once, including native failure and reentrant replacement.<br>Guard upd actor/class/function/frame ownership and expose actual dispatch. | KD19<br>KD23 | Cancel client effects after actor, class, Element or world replacement.<br>Retry missing/replaced layouts without poisoning legacy wrapper entry points. | KD20<br>KD24 | Cache native state layouts by live class incarnation and preserve boolean masks.<br>Invalidate queued work on teardown without resetting an active stack's recursion guard. |

| Operation | World | Session serial | Actor slot | Class slot | Element |
| --- | --- | --- | --- | --- | --- |
| Host active read<br>Host charging read | KX01<br>KX06 | KX02<br>KX07 | KX03<br>KX08 | KX04<br>KX09 | KX05<br>KX10 |
| Host energy read<br>Recipient send | KX11<br>KX16 | KX12<br>KX17 | KX13<br>KX18 | KX14<br>KX19 | KX15<br>KX20 |
| Client class gate<br>Client active read | KX21<br>KX26 | KX22<br>KX27 | KX23<br>KX28 | KX24<br>KX29 | KX25<br>KX30 |
| Client active write<br>Client charging write | KX31<br>KX36 | KX32<br>KX37 | KX33<br>KX38 | KX34<br>KX39 | KX35<br>KX40 |
| Client energy write<br>Client upd dispatch | KX41<br>KX46 | KX42<br>KX47 | KX43<br>KX48 | KX44<br>KX49 | KX45<br>KX50 |

## Independent drive claims (8 November 2026)

Implemented LS01-LS25 / LX01-LX50: host mechanisms retain separate source masks and monotonic lease tokens. Release removes only its captured contribution; other sources keep the stream claimed. Legacy operations are isolated. Handoff/election and hand episodes clear incompatible tokens; context stamps expire abandoned contributions across first bind, world/session replacement and disconnect. Hook sweeps own their input/lease snapshots; robot cleanup preserves foreign claims before thaw; Kerfus and watoy retain source-bound leases across native callbacks. Nine producer routes use canonical source groups.
Verification: PASS 75 named cases, 23 assertion-based fault controls and 11 production object compiles. The actual core, hook controller, NPC cleanup, Kerfus sampler and extracted watoy/handoff bodies run with engine/transport doubles and real Element/Registry code. Targeted NPC, publisher, grant/revoke, watoy, grayboar and drive-lifetime regressions pass. All 158 full regression groups pass on stable production sources. Native fact rows pass (626 rows / 199 files); public prose still fails the same 14 counters above baseline. Native constraint/transform priority, contact physics and the original prop-jitter root cause remain open. No Release build, package, install or version/protocol change.
Evidence: `build/prop-claim-controls.log`, `build/prop-claim-check`, `build/prop-claim-verification/verification.json`. Each LX cell is a separately exercised interference case for the named source pair.

| ID | Completed fix | ID | Completed fix | ID | Completed fix | ID | Completed fix |
| --- | --- | --- | --- | --- | --- | --- | --- |
| LS01<br>LS05<br>LS09<br>LS13<br>LS17<br>LS21<br>LS25 | Bound claim-source enum values before indexing or shifting.<br>Fence claim/release by epoch, world pointer/generation and session serial/binding.<br>Keep the stream claimed when another source remains.<br>Return no foreign lease after row replacement during notification.<br>Reject old leases after disconnect/rebind, including same-EID reuse.<br>Bind robot carry release to its lease and preserve other owners' physics.<br>Preserve active source claims across ordinary impulse coasts. | LS02<br>LS06<br>LS10<br>LS14<br>LS18<br>LS22 | Allocate monotonically unique claim tokens without resetting on disconnect.<br>Validate canonical actor/class/EID before using a lease.<br>Revalidate the last-owner release after its clock callback; retain retryable ownership.<br>Preserve a newer source claim created by a callback.<br>Clear client claim metadata at host handoff and coast abandoned reservations.<br>Preserve Kerfus leases and owned host records across native callbacks. | LS03<br>LS07<br>LS11<br>LS15<br>LS19<br>LS23 | Return the same token for an idempotent active source claim.<br>Carry the caller's lifetime proof through claim admission and client handoff.<br>Isolate legacy Claim/Release from typed source ownership.<br>Reject an old release after its source is released and reacquired.<br>Clear host source metadata on client election and protect claimed rows.<br>Bind watoy activation/deactivation leases to its captured actor/state row. | LS04<br>LS08<br>LS12<br>LS16<br>LS20<br>LS24 | Retain independent source masks/tokens on one host row.<br>Release only the exact source, EID and token receipt.<br>Return no lease for refused or abandoned admission.<br>Invalidate source tokens on a new hand-release episode.<br>Bind hook release to owned per-pass leases and cancel nested/reset sweeps.<br>Use canonical source groups for carry starters, rockets, grayboars and client feeders. |

| Source pair A/B | Release A keeps B | Release B keeps A | Old A after reacquire | Legacy release isolation | Hand episode then stale release |
| --- | --- | --- | --- | --- | --- |
| Hook / Kerfur<br>Hook / Kerfus<br>Hook / Watoy<br>Hook / Rocket | LX01<br>LX06<br>LX11<br>LX16 | LX02<br>LX07<br>LX12<br>LX17 | LX03<br>LX08<br>LX13<br>LX18 | LX04<br>LX09<br>LX14<br>LX19 | LX05<br>LX10<br>LX15<br>LX20 |
| Hook / Grayboar<br>Kerfur / Kerfus<br>Kerfur / Watoy<br>Kerfus / Watoy | LX21<br>LX26<br>LX31<br>LX36 | LX22<br>LX27<br>LX32<br>LX37 | LX23<br>LX28<br>LX33<br>LX38 | LX24<br>LX29<br>LX34<br>LX39 | LX25<br>LX30<br>LX35<br>LX40 |
| Rocket / Watoy<br>Grayboar / Watoy | LX41<br>LX46 | LX42<br>LX47 | LX43<br>LX48 | LX44<br>LX49 | LX45<br>LX50 |

## Drive admission and verb handoff (7 November 2026)

Implemented VA01-VA25 / VX01-VX50: drive admission validates actor/class/world/session state between native class, hand, EID and key reads. It rejects duplicate streams and malformed keys, owns callback data and re-resolves committed rows after notifications. A client-to-host handoff records its exact predecessor revision; Claim, Coast and election consume that lineage instead of inheriting a newer row. Insertion replaces the caller's absent-target precondition with the committed row receipt. Release uses the row's captured weak identity and revalidates after its clock read, without probing dead or opaque actor pointers. Same-actor admission recursion remains bounded across teardown. Verification: PASS 75 named cases, 21 assertion-based fault controls, six production object compiles and targeted publisher/controller/lifetime/control-delivery/stream regressions. All 157 full regression groups pass on stable production sources. Native fact rows pass (626 rows / 199 files); public prose still fails 14 counters above baseline. The actual admission/verb functions and extracted actual handoff commit run with native/transport doubles; the controller fixture also checks lineage through the complete native-ending coordinator. Multi-verb claim leases, original prop-jitter root cause, native contacts and multiplayer acceptance remain open. No Release build, package or version/protocol change. Evidence: `build/prop-verb-controls.log`, `build/prop-verb-check`, `build/prop-verb-verification/verification.json`. Each VX cell exercises cancellation after the named read and replacement.

| ID | Completed fix | ID | Completed fix | ID | Completed fix | ID | Completed fix |
| --- | --- | --- | --- | --- | --- | --- | --- |
| VA01<br>VA05<br>VA09<br>VA13<br>VA17<br>VA21<br>VA25 | Capture actor slot/serial before admission metadata work.<br>Own reason text across native callbacks.<br>Revalidate between individual class/hand gates.<br>Recheck the prop's canonical EID after native key work.<br>Snapshot election inputs before native hand checks.<br>Resume Coast only from its own completed handoff.<br>Consume handoff lineage when Claim, Coast or election commits. | VA02<br>VA06<br>VA10<br>VA14<br>VA18<br>VA22 | Retain class slot/serial through admission.<br>Preserve a competing actor/EID row rather than inserting a duplicate.<br>Admit only valid, nonzero host-range prop IDs.<br>Allocate generation only at the still-owned commit.<br>Cancel election when a callback replaces its original row.<br>Guard the Kerfur gate and later Coast work by actor/context ownership. | VA03<br>VA07<br>VA11<br>VA15<br>VA19<br>VA23 | Cancel admission across disconnect epoch and world generation/pointer changes.<br>Bound recursive admission for the same actor across teardown.<br>Reject oversized/unrepresentable keys instead of truncating them.<br>Own notification/log fields and re-resolve the committed row after notification.<br>Record exact predecessor revision on a completed client-to-host handoff.<br>Commit hand-release generation and episode changes only to the captured row. | VA04<br>VA08<br>VA12<br>VA16<br>VA20<br>VA24 | Bind admission to the current bound session, serial, stop, role and connection.<br>Permit independent actor admissions without borrowing vector storage.<br>Refuse empty or None native keys.<br>Initialize client admission only on the exact surviving row receipt.<br>Resume Claim only from its own completed handoff.<br>Revalidate Release after its clock read before changing a row. |

| Read | World | Session serial | Actor slot | Class slot | Competing drive row |
| --- | --- | --- | --- | --- | --- |
| Prop descendant<br>Local hold<br>Last held actor<br>Remote hand | VX01<br>VX06<br>VX11<br>VX16 | VX02<br>VX07<br>VX12<br>VX17 | VX03<br>VX08<br>VX13<br>VX18 | VX04<br>VX09<br>VX14<br>VX19 | VX05<br>VX10<br>VX15<br>VX20 |
| Hotbar hand<br>Initial EID<br>Class name<br>Class suppression | VX21<br>VX26<br>VX31<br>VX36 | VX22<br>VX27<br>VX32<br>VX37 | VX23<br>VX28<br>VX33<br>VX38 | VX24<br>VX29<br>VX34<br>VX39 | VX25<br>VX30<br>VX35<br>VX40 |
| Native key<br>Final EID | VX41<br>VX46 | VX42<br>VX47 | VX43<br>VX48 | VX44<br>VX49 | VX45<br>VX50 |

## NPC pose and robot holds (7 November 2026)

Implemented NH01-NH25 / NX01-NX50: host pose walks own registry identities and invocation buffers, cancel retired native continuations, and revalidate earlier rows before publication. Robot holds own receipts and prop identities, preserve another robot's take, respect hand ownership and retry a failed thaw without repeating release. Teardown invalidates in-flight work without native cleanup calls. This does not establish the original joined-client jitter cause or finish drive-verb arbitration. Verification: PASS 75 named cases, 23 assertion-based fault controls and four production object compiles. All 156 full regression groups pass on stable production sources. Native fact rows pass (626 rows / 199 files); public prose still fails the same 14 counters above baseline. The actual controller, Element/Registry and atomic transform body run with engine/hand/drive doubles; native contact, mirror materialization and multiplayer acceptance are unperformed. No Release build, package or version/protocol change. Evidence: `build/npc-pose-controls.log`, `build/npc-pose-check`, `build/npc-pose-verification/verification.json`. Each NX cell is a separately exercised cancellation after the named read and replacement.

| ID | Completed fix | ID | Completed fix | ID | Completed fix | ID | Completed fix |
| --- | --- | --- | --- | --- | --- | --- | --- |
| NH01<br>NH05<br>NH09<br>NH13<br>NH17<br>NH21<br>NH25 | Reject nested pose ticks before join/native work.<br>Check Element incarnation, type, name and canonical actor binding.<br>Preserve rotating MTU fairness and reset rotation with context.<br>Read root transform atomically with caller cancellation.<br>Replace borrowed hold-vector rows with owned receipts.<br>Respect hand ownership before freezing or thawing a carried prop.<br>Clear hold receipts and invalidate active work without native teardown calls. | NH02<br>NH06<br>NH10<br>NH14<br>NH18<br>NH22 | Bind pose work to session pointer, serial, stop, role and running state.<br>Validate actor/class slot serials across native continuations.<br>Keep offline hold/death lifecycle without publishing a stale connection.<br>Reject nonfinite/out-of-range coordinates, velocity, speed and facing.<br>Stamp held-prop actor/class identities and invalidate reused addresses.<br>Retain failed thaw work for retry without repeating completed release. | NH03<br>NH07<br>NH11<br>NH15<br>NH19<br>NH23 | Cancel work on world generation/pointer or disconnect epoch changes.<br>Own each invocation's pose batch and final row receipts.<br>Recheck context after pending-spawn draining and diagnostic census.<br>Stop body-yaw fallback immediately after ownership changes.<br>Stamp client hold notes with session/world/actor identity.<br>Preserve another robot's hold when one robot drops the same prop. | NH04<br>NH08<br>NH12<br>NH16<br>NH20<br>NH24 | Copy registry identities instead of retaining NPC Element pointers.<br>Revalidate earlier rows before publication after later actor callbacks.<br>Retire only the captured, still-bound dead NPC identity.<br>Commit optional look/state/floppy/ATV fields only while current.<br>Check holdObject remains selected before freeze and claim.<br>Sweep removed/rebound NPC holders and guard release continuations. |

| Read continuation | Session | World | Actor slot | Class slot | Element |
| --- | --- | --- | --- | --- | --- |
| Transform<br>Velocity<br>Falling<br>Look target | NX01<br>NX06<br>NX11<br>NX16 | NX02<br>NX07<br>NX12<br>NX17 | NX03<br>NX08<br>NX13<br>NX18 | NX04<br>NX09<br>NX14<br>NX19 | NX05<br>NX10<br>NX15<br>NX20 |
| Kerfur body yaw<br>Fossilhound body yaw<br>Rufus body yaw<br>Kerfur state | NX21<br>NX26<br>NX31<br>NX36 | NX22<br>NX27<br>NX32<br>NX37 | NX23<br>NX28<br>NX33<br>NX38 | NX24<br>NX29<br>NX34<br>NX39 | NX25<br>NX30<br>NX35<br>NX40 |
| Floppy state<br>ATV state | NX41<br>NX46 | NX42<br>NX47 | NX43<br>NX48 | NX44<br>NX49 | NX45<br>NX50 |

## NPC join delivery (7 November 2026)

Implemented NJ01-NJ25 / NK01-NK50: recipient-owned NPC join delivery now retains refused, unreadable and unbound rows; validates canonical actor bindings and Element incarnations; reads current transforms without partial outputs; preserves save/adoption and off-prop retirement provenance. Same-address rebinds refresh their object serial without taking another Element's binding. SnapshotComplete has independent per-slot delivery debt, a captured bracket token and registry-coherence proof; a waiting recipient does not hold the prop enumeration queue. The sender never repeats native NPC creation, save or reward work during retry. Verification: PASS 75 named cases, 30 assertion-based fault controls, eight production object compiles and all 155 full regression groups on stable sources. Actual Element/Registry implementations, transform/save-key bodies and the prop completion function are exercised with engine/transport/clock doubles. The sender permits 16 delivery attempts per invocation, including at most 12 NPC row attempts, and 64 row scans; callback-owned event lanes retain their own budgets. Native fact rows pass (626 rows / 199 files); public prose still fails the same 14 counters above baseline. Native multiplayer/materialization and the remaining UP2/UP4/UP5/UP6 work remain open. No Release DLL/package or version/protocol change. Evidence: `build/npc-join-verification/verification.json`; named cases and mutations: `build/npc-join-controls.log`; objects: `build/npc-join-check`. Earlier upstream evidence below describes its own source identity.

| ID | Completed fix | ID | Completed fix | ID | Completed fix | ID | Completed fix |
| --- | --- | --- | --- | --- | --- | --- | --- |
| NJ01<br>NJ05<br>NJ09<br>NJ13<br>NJ17<br>NJ21<br>NJ25 | host authority<br>retain temporarily unready work<br>reject recycled Element identity<br>exact actor/class binding<br>current off-prop retirement key<br>native and scan budgets<br>per-recipient snapshot completion debt | NJ02<br>NJ06<br>NJ10<br>NJ14<br>NJ18<br>NJ22 | running/connected admission<br>retain refusals beyond five attempts<br>retain not-yet-bound NPC<br>owned canonical class name<br>finite/bounded complete payload<br>fair recipient rotation | NJ03<br>NJ07<br>NJ11<br>NJ15<br>NJ19<br>NJ23 | recipient bounds<br>retire only accepted rows<br>retire dead actor<br>readable save provenance<br>fresh transform on retry<br>preserve newer join receipts | NJ04<br>NJ08<br>NJ12<br>NJ16<br>NJ20<br>NJ24 | departed/zero generation<br>snapshot owned IDs<br>retire removed/deleting Element<br>save-key cache incarnation/schema<br>retry after clock rollback<br>refuse recursive drain |

Each NK cell stops the old continuation after the indicated boundary and replacement.

| Boundary | World | Session pointer | Session serial | Stop | Role | Recipient | Ready revision | Actor serial | Class serial | Element incarnation |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Location<br>Rotation<br>Scale<br>Save-key format | NK01<br>NK11<br>NK21<br>NK31 | NK02<br>NK12<br>NK22<br>NK32 | NK03<br>NK13<br>NK23<br>NK33 | NK04<br>NK14<br>NK24<br>NK34 | NK05<br>NK15<br>NK25<br>NK35 | NK06<br>NK16<br>NK26<br>NK36 | NK07<br>NK17<br>NK27<br>NK37 | NK08<br>NK18<br>NK28<br>NK38 | NK09<br>NK19<br>NK29<br>NK39 | NK10<br>NK20<br>NK30<br>NK40 |
| Recipient send | NK41 | NK42 | NK43 | NK44 | NK45 | NK46 | NK47 | NK48 | NK49 | NK50 |

## Full-upstream work (7 November 2026)

UI01 implements tower PRE/POST ownership, nesting, reserved admission, owned FIFO delivery until ACK and detached native claim cleanup; UI02 checks parameter-cache incarnations and frames; UI03 records committed writes, easy-repair confirmation, captured refused-pull output and unknown receipts, sequence bounds and refund debt. Only one paid insert spends before ACK. Unattempted creation retries, uncertain creation is not repeated, and temporary world-unready retains debt. UI04 tracks cord recipients independently; UI05 guards native head/component/caller continuations and newer pose/release receipts. UI09 also guards actual deferred spawn schemas/caller/world/actor ownership through begin/finish and unfinished cleanup. UI06 adds retained Geom decisions, reconstruction checks and ordered recipient-owned NPC birth admission. Refused birth work retries without losing newer decisions; native transforms retain root/function/caller ownership and publish all outputs together. Birth reads/sends/receipts bind the recipient's world-ready reset revision, including a false/true transition between pumps. Exact phase/audio/effect joins and native mirror creation acceptance remain open. UI07 serializes native Present/Destroy; GPU quiescence remains the engine caller's contract, without adding an unsynchronized queue-idle call to Destroy. Full context recovery/backend retirement remains open. UI08 has no demonstrated jitter root cause; reproduction steps were requested. Read-only local inspection found four older stack-overflow crash contexts, without a fresh jitter trace (`native-log-review.json`). UI09 repairs tower/Geom session retirement; this is not a completed review of all 436 transferred commits. Verification: 154 full regression groups passed on stable production sources, including the birth/transform/world-ready follow-up. PASS 84 assertion-based fault controls and ten production object compiles. Native fact rows pass (626 rows / 199 files); public prose still fails the same 14 counters above baseline. `pre-birth-verification.json` retains the earlier source identity. Full-upstream implementation and native gameplay acceptance remain incomplete; Release was not built. Evidence: `build/upstream-integration-verification/verification.json`; focused controls: `build/upstream-completion-tower-controls.log`, `build/upstream-completion-tower-rows.log`, `build/upstream-completion-cord-controls.log`, `build/upstream-completion-vulkan-controls.log`, `build/upstream-completion-geom-birth-controls.log`; ten production objects: `build/upstream-completion-verification/objects`. Engine/transport/identity/clock/GPU dispatch are doubled; Vulkan uses the actual extracted Present/Destroy functions with two threads; actor-transform extraction exercises 71 refusal/replacement/root/schema/finite boundaries. Actual Session world-ready methods cover all 32 slots, bounds and eight concurrent reset writers.

## Cord publication and intent continuations (7 November 2026)

Implemented CP01-CP25 / CQ01-CQ50 in cord_sync. PASS 75 named cases, preserved same-world scan/partial-scan state, nested publication/outbox/release replacement, slot 31, refused retries and backwards clocks. Twenty-five independently compiled mutations fail at assertions; production cord_sync and the native cord wrapper compile. PASS eleven scoped groups and stable-source guard, including legacy 4/32-peer intent, wrapper and 200-read idle repaint checks. Engine/transport, object/session identity and clock are doubled. Raw endpoint/property reads do not dispatch Blueprint code; the host reach row exercises IntentTarget::Authorize. Per-recipient canonical-state/join delivery and native drag-wrapper ownership remain open. No Release DLL or package. Each CQ cell cancels stale completion/continuation after the named boundary and replacement. Evidence: `build/cord-continuation-verification/verification.json`, `results.json` and `source-start.sha256`; named cases/mutations/object: `build/cord-continuation-check.log`; legacy wrapper/object and shared-fixture regression: `build/cord-continuation-2170.log` and `build/cord-continuation-2145.log`.

| ID | Completed cord fix | ID | Completed cord fix | ID | Completed cord fix | ID | Completed cord fix |
| --- | --- | --- | --- | --- | --- | --- | --- |
| CP01<br>CP05<br>CP09<br>CP13 | Retire replaced Session pointers while preserving the same-world scan index and partial scan.<br>Retire replaced worlds.<br>Own the targeted-join actor snapshot.<br>Preserve newer nested publication requests. | CP02<br>CP06<br>CP10<br>CP14 | Retire same-pointer session reconstruction.<br>Retire disconnected context debt.<br>Own Sent records across transport callbacks.<br>Own outbound intent bytes across transport callbacks. | CP03<br>CP07<br>CP11<br>CP15 | Retire changed stop counters.<br>Guard every Tick stage with the captured context.<br>Publish operation identity before callbacks.<br>Saturate heartbeat and drag-timeout arithmetic. | CP04<br>CP08<br>CP12<br>CP16 | Retire changed roles.<br>Own the host publication actor snapshot.<br>Commit only the exact publication receipt.<br>Own and revise coalesced intent-outbox entries. |
| CP17<br>CP21<br>CP25 | Erase only the sent outbox revision.<br>Bind timeout/RestoreHead continuation to its context.<br>Validate actor/class/endpoint proofs through publication and native intent work. | CP18<br>CP22 | Retain refused owned outbox work.<br>Bind targeted sends to recipient generation and departure. | CP19<br>CP23 | Own release receipts independently of actor presence.<br>Copy intent input before native callbacks. | CP20<br>CP24 | Complete only the sent release receipt.<br>Bind native intent work to sender generation/readiness/session. |

| Boundary | World | Session pointer | Session serial | Stop | Role | Cord | Cord class | Socket | Socket class | Peer |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Broadcast send completion<br>Targeted send completion<br>Intent-outbox send completion<br>Host reach judgement | CQ01<br>CQ11<br>CQ21<br>CQ31 | CQ02<br>CQ12<br>CQ22<br>CQ32 | CQ03<br>CQ13<br>CQ23<br>CQ33 | CQ04<br>CQ14<br>CQ24<br>CQ34 | CQ05<br>CQ15<br>CQ25<br>CQ35 | CQ06<br>CQ16<br>CQ26<br>CQ36 | CQ07<br>CQ17<br>CQ27<br>CQ37 | CQ08<br>CQ18<br>CQ28<br>CQ38 | CQ09<br>CQ19<br>CQ29<br>CQ39 | CQ10<br>CQ20<br>CQ30<br>CQ40 |
| Host connect completion | CQ41 | CQ42 | CQ43 | CQ44 | CQ45 | CQ46 | CQ47 | CQ48 | CQ49 | CQ50 |

## End recipient delivery (7 November 2026)

Implemented EH01-EH25 / EX01-EX50 in the shared prop_end_delivery ledger and both callers. PASS 75 named cases, two-translation-unit shared-state identity, reserved-field rejection before native effects, snapshot cancellation and recursive-drain controls. Twenty-seven End mutations, sixteen publisher mutations, fifteen controller mutations and eleven drive-stream mutations fail at assertions; both production objects compile. PASS eleven scoped groups and stable-source guard. Engine/transport, object/session identity and clock are doubled. Each Queue and Tick invocation sends at most 16 packets; this is not a global frame cap. Retry copies immutable End bytes after row retirement without repeating native release/physics. Native handoff/contact acceptance and the original jitter investigation remain open. No Release DLL or package. Each EX cell cancels stale publication/completion after the named boundary and replacement. Evidence: `build/prop-end-delivery-verification/verification.json`, `results.json` and `source-start.sha256`; End cases/mutations/objects: `build/prop-end-delivery-controls.log`; publisher/controller/stream controls: `build/prop-publisher-check/end-delivery-regression.log`, `build/prop-syncer-host-check/end-delivery-regression.log` and `build/prop-drive-lifetime-check/end-delivery-regression.log`.

| ID | Completed End-delivery fix | ID | Completed End-delivery fix | ID | Completed End-delivery fix | ID | Completed End-delivery fix |
| --- | --- | --- | --- | --- | --- | --- | --- |
| EH01<br>EH05<br>EH09<br>EH13 | Replace any-recipient broadcast success with per-slot End debt.<br>Drain publisher debt even with no driven rows.<br>Retire changed recipient generations, including slot 31.<br>Cancel replaced Session pointers. | EH02<br>EH06<br>EH10<br>EH14 | Skip the original syncer for an admitted client rest End.<br>Preserve immutable End bytes and owned receipts across callbacks.<br>Retire observed departures even when the generation remains unchanged.<br>Cancel reconstructed sessions and stopped links. | EH03<br>EH07<br>EH11<br>EH15 | Retain controller-judged End after row retirement.<br>Keep independent progress after partial acceptance.<br>Keep refusal debt beyond five attempts.<br>Cancel changed worlds and roles. | EH04<br>EH08<br>EH12<br>EH16 | Transfer publisher End debt before retiring its row.<br>Wait for connected recipients to become world-ready.<br>Pace retries and preserve debt under backwards clocks.<br>Cancel replaced actor/class incarnations. |
| EH17<br>EH21<br>EH25 | Validate actor/EID binding before and after native getters.<br>Bound and rotate recipient/batch work; reject recursive drains.<br>Keep existing no-End branches for host takeback/death and unobserved rows. | EH18<br>EH22 | Cancel superseded End requests without erasing their successor.<br>Validate End identity, bounds, finite data and reserved fields. | EH19<br>EH23 | Invalidate debt on new episodes, including transient same-byte successors.<br>Retain debt independently of the expired retirement guard. | EH20<br>EH24 | Complete against the captured request identity after transport.<br>Retry delivery without repeating native release, physics or reward work. |

| Boundary | World | Session | Actor | Class | Recipient | Boundary | World | Session | Actor | Class | Recipient |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Publisher initial class lookup<br>Publisher initial send completion<br>Publisher retry send completion<br>Controller initial EID lookup | EX01<br>EX11<br>EX21<br>EX31 | EX02<br>EX12<br>EX22<br>EX32 | EX03<br>EX13<br>EX23<br>EX33 | EX04<br>EX14<br>EX24<br>EX34 | EX05<br>EX15<br>EX25<br>EX35 | Publisher initial EID lookup<br>Publisher retry EID lookup<br>Controller initial class lookup<br>Controller initial send completion | EX06<br>EX16<br>EX26<br>EX36 | EX07<br>EX17<br>EX27<br>EX37 | EX08<br>EX18<br>EX28<br>EX38 | EX09<br>EX19<br>EX29<br>EX39 | EX10<br>EX20<br>EX30<br>EX40 |
| Controller retry EID lookup | EX41 | EX42 | EX43 | EX44 | EX45 | Controller retry send completion | EX46 | EX47 | EX48 | EX49 | EX50 |

## Grant/revoke recipient delivery (6 November 2026)

Implemented GD01-GD25 / GX01-GX50 in prop_syncer_host. PASS 75 named cases, pending grant/revoke supersession, death notices, partial recipient delivery, slot 31, host takeback and owned transport buffers. Twenty delivery mutations, fifteen host-controller mutations and eleven drive-stream mutations fail at assertions; the production object compiles. PASS ten scoped groups and stable-source guard, including the joint stream fixture with Windows headers. Function-row gate passes 626 rows / 199 files; full public prose still fails 14 counters. Engine/transport, object/session identity and clock are doubled. Retries retain transport refusals and wait for world-ready; ordinary request Refused answers still use their paced direct path. Per-recipient End delivery and native multiplayer acceptance remain open. No Release DLL or package. Each GX cell cancels stale publication/completion after the named boundary and replacement. Evidence: `build/prop-control-delivery-verification/verification.json`, `results.json` and `source-start.sha256`; control cases/mutations/object: `build/prop-control-delivery-controls.log`; existing host/stream controls: `build/prop-syncer-host-check/control-delivery-regression.log` and `build/prop-drive-lifetime-check/control-delivery-regression.log`.

| ID | Completed control-delivery fix | ID | Completed control-delivery fix | ID | Completed control-delivery fix | ID | Completed control-delivery fix |
| --- | --- | --- | --- | --- | --- | --- | --- |
| GD01<br>GD05<br>GD09<br>GD13 | Retain refused claim grants.<br>Bind delivery to recipient generation, including slot 31.<br>Pace transport retries.<br>Cancel stopped sessions. | GD02<br>GD06<br>GD10<br>GD14 | Retain refused throw grants.<br>Retire departed recipients even without a generation change.<br>Preserve debt under backwards clocks.<br>Cancel replaced worlds. | GD03<br>GD07<br>GD11<br>GD15 | Retain live-actor revocations after row retirement.<br>Wait for recipient world-ready without dropping debt.<br>Cancel reconstructed sessions.<br>Cancel changed roles. | GD04<br>GD08<br>GD12<br>GD16 | Deliver death revocations without reading dead actors.<br>Retry transport refusal beyond five attempts.<br>Cancel replaced Session pointers.<br>Cancel replaced actor incarnations. |
| GD17<br>GD21<br>GD25 | Cancel replaced class incarnations.<br>Supersede an undelivered grant with its revocation.<br>Keep revocation delivery through host takeback and preserve kind/sequence. | GD18<br>GD22 | Validate actor/EID binding across getters.<br>Invalidate grants when a successor row opens, including transient successors. | GD19<br>GD23 | Keep grants through ordinary revisions of their episode.<br>Bound and rotate drain work; reject recursive drains. | GD20<br>GD24 | Complete only the exact queued request; preserve a newer request.<br>Own payload bytes across queue replacement callbacks. |

| Boundary | World | Session | Actor | Class | Recipient | Boundary | World | Session | Actor | Class | Recipient |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Initial grant class lookup<br>Initial grant send completion<br>Retry grant send completion<br>Initial revoke EID lookup | GX01<br>GX11<br>GX21<br>GX31 | GX02<br>GX12<br>GX22<br>GX32 | GX03<br>GX13<br>GX23<br>GX33 | GX04<br>GX14<br>GX24<br>GX34 | GX05<br>GX15<br>GX25<br>GX35 | Initial grant EID lookup<br>Retry grant EID lookup<br>Initial revoke class lookup<br>Initial revoke send completion | GX06<br>GX16<br>GX26<br>GX36 | GX07<br>GX17<br>GX27<br>GX37 | GX08<br>GX18<br>GX28<br>GX38 | GX09<br>GX19<br>GX29<br>GX39 | GX10<br>GX20<br>GX30<br>GX40 |
| Retry revoke EID lookup | GX41 | GX42 | GX43 | GX44 | GX45 | Retry revoke send completion | GX46 | GX47 | GX48 | GX49 | GX50 |

## Swinger host join delivery (6 November 2026)

Implemented SJ01-SJ25 / SD01-SD50 in swinger_join_pose. PASS 75 named cases, unreadable pose retries, slot 31, partial acceptance, nested replacement and selective Forget. Nineteen separately compiled mutations and eleven retained drive-stream mutations fail at assertions; the production object compiles. PASS nine scoped groups, stable-source guard and the function-row gate (626 rows / 199 files). Full public prose still fails 14 counters, unchanged from the previous scope. Engine/transport and object/session identity are doubled; pacing is exercised with controlled due times. Grant/revoke/End recipient delivery and native multiplayer acceptance remain open. No Release DLL or package. Each SD cell cancels stale publication/completion after the named boundary and replacement. Evidence: `build/swinger-delivery-verification/verification.json`, `results.json` and `source-start.sha256`; host cases/mutations/object: `build/swinger-delivery-controls.log`; drive-stream controls: `build/prop-drive-lifetime-check/swinger-delivery-regression.log`.

| ID | Completed join-delivery fix | ID | Completed join-delivery fix | ID | Completed join-delivery fix | ID | Completed join-delivery fix |
| --- | --- | --- | --- | --- | --- | --- | --- |
| SJ01<br>SJ05<br>SJ09<br>SJ13 | Retain a recipient-owned join request.<br>Retire replaced worlds.<br>Filter portable keys and deduplicate the snapshot.<br>Pace refused/unavailable retries. | SJ02<br>SJ06<br>SJ10<br>SJ14 | Retire replaced Session pointers.<br>Retire changed roles.<br>Own lid snapshots across native callbacks.<br>Preserve debt under backwards-clock scheduling. | SJ03<br>SJ07<br>SJ11<br>SJ15 | Retire same-pointer session reconstruction.<br>Bind work to the original slot generation.<br>Publish requests before reentrant collection.<br>Keep refused work beyond five attempts. | SJ04<br>SJ08<br>SJ12<br>SJ16 | Retire stopped sessions.<br>Retain work until world-ready.<br>Reject recursive host drains.<br>Support isolated delivery through slot 31. |
| SJ17<br>SJ21<br>SJ25 | Own each outbound payload.<br>Cancel replaced actor/class/key bindings.<br>Forget only the matching stale host debt. | SJ18<br>SJ22 | Complete only the exact request sent.<br>Validate finite/bounded outgoing poses. | SJ19<br>SJ23 | Read the current native pose on each retry.<br>Keep per-key progress after partial acceptance. | SJ20<br>SJ24 | Distinguish a closed lid from an unreadable one.<br>Bound and rotate recipient/key work. |

| Boundary | World | Session | Actor | Class | Recipient | Boundary | World | Session | Actor | Class | Recipient |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Initial key binding<br>Initial location | SD01<br>SD11 | SD02<br>SD12 | SD03<br>SD13 | SD04<br>SD14 | SD05<br>SD15 | Initial open query<br>Initial rotation | SD06<br>SD16 | SD07<br>SD17 | SD08<br>SD18 | SD09<br>SD19 | SD10<br>SD20 |
| Initial send completion<br>Retry open query | SD21<br>SD31 | SD22<br>SD32 | SD23<br>SD33 | SD24<br>SD34 | SD25<br>SD35 | Retry key binding<br>Retry location | SD26<br>SD36 | SD27<br>SD37 | SD28<br>SD38 | SD29<br>SD39 | SD30<br>SD40 |
| Retry rotation | SD41 | SD42 | SD43 | SD44 | SD45 | Retry send completion | SD46 | SD47 | SD48 | SD49 | SD50 |

## Native stream caller scope (6 November 2026)

Implemented SG01-SG15 / SX01-SX60 across controller, stream, driver episodes, park and parity. PASS 75 named cases and lifetime/same-byte reacquisition, owned-input/bind-cleanup, guarded host End, transient-successor invalidation, failed-write and cleanup controls. Eighteen independently compiled scope mutations and eleven retained drive-stream mutations fail at assertions; five production objects compile. PASS eight scoped groups and stable-source guard. Engine/transport, registry, clock and rider tokens are doubled; per-recipient control/join delivery and rendered contacts remain open. Host-local episode proof is required to supersede a closed byte generation. No Release DLL or package. Each SX cell stops further application after the named boundary and replacement; movement-lease cleanup retains its own ownership. Evidence: `build/prop-stream-scope-verification/verification.json`, `results.json` and `source-start.sha256`; joint fixture/mutations/objects: `build/prop-stream-scope-check/verification.log`; receiver regression: `build/prop-drive-lifetime-check/source-scope-regression.log`.

| ID | Completed scope boundary | ID | Completed scope boundary | ID | Completed scope boundary | ID | Completed scope boundary |
| --- | --- | --- | --- | --- | --- | --- | --- |
| SG01<br>SG05<br>SG09<br>SG13 | Own guarded FeedAdmitted source.<br>Propagate host takeback retirement.<br>Prevent borrowed host End replay.<br>Own payloads and callbacks through native work. | SG02<br>SG06<br>SG10<br>SG14 | Guard ReleaseHost.<br>Persist episode lifetime across row revisions.<br>Commit End accounting after valid completion.<br>Suspend invalid lifetimes; retain park debt. | SG03<br>SG07<br>SG11<br>SG15 | Propagate the admission receipt.<br>Retire changed session/stop/role/world work.<br>Compose Park/Unpark guards.<br>Stop after refused velocity writes. | SG04<br>SG08<br>SG12 | Propagate rest-End retirement.<br>Bind actor/class and mesh incarnation.<br>Guard give-back/yield and child notification. |

| Boundary | World | Session | Sender | Actor | Class | Episode | Boundary | World | Session | Sender | Actor | Class | Episode |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Pose resolution<br>First-pose diagnostic location | SX01<br>SX13 | SX02<br>SX14 | SX03<br>SX15 | SX04<br>SX16 | SX05<br>SX17 | SX06<br>SX18 | Pose mesh capture<br>Park simulation write | SX07<br>SX19 | SX08<br>SX20 | SX09<br>SX21 | SX10<br>SX22 | SX11<br>SX23 | SX12<br>SX24 |
| Initial lerp location<br>End rotation | SX25<br>SX37 | SX26<br>SX38 | SX27<br>SX39 | SX28<br>SX40 | SX29<br>SX41 | SX30<br>SX42 | End location<br>End frozen/sleep verb | SX31<br>SX43 | SX32<br>SX44 | SX33<br>SX45 | SX34<br>SX46 | SX35<br>SX47 | SX36<br>SX48 |
| End linear velocity | SX49 | SX50 | SX51 | SX52 | SX53 | SX54 | End unpark completion | SX55 | SX56 | SX57 | SX58 | SX59 | SX60 |

## Host prop publisher ownership (6 November 2026)

Implemented HP01-HP25 / HT01-HT50 in prop_drive_host. PASS 75 named cases plus slow-heartbeat End, unreadable-pose fallback, slot 31, binding/class/root replacement, sender replacement during publication and nested disconnect recovery. Sixteen publisher mutations and eleven drive-stream mutations fail at assertions; the changed production object compiles. PASS seven scoped groups and stable-source guard. Engine/controller/transport are doubled; native contact acceptance and per-recipient control/join delivery remain open; stream caller proof is covered in the SG section. No Release DLL or package. Each HT cell cancels a continuation after the indicated boundary and replacement. Evidence: `build/prop-publisher-verification/verification.json`, `results.json` and `source-start.sha256`; publisher cases/mutations/object: `build/prop-publisher-check/verification.log`; stream controls: `build/prop-drive-lifetime-check/publisher-regression.log`.

| ID | Completed publisher boundary | ID | Completed publisher boundary | ID | Completed publisher boundary | ID | Completed publisher boundary | ID | Completed publisher boundary |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| HP01<br>HP06<br>HP11<br>HP16 | Own Tick receipts across growth.<br>Retire same-pointer reconstruction.<br>Cancel remaining Pump stages.<br>Preserve callback coast/keepalive. | HP02<br>HP07<br>HP12<br>HP17 | Commit only the captured revision.<br>Retire stopped sessions.<br>Check the original slot occupant.<br>Reject nonfinite poses. | HP03<br>HP08<br>HP13<br>HP18 | Reject recursive Tick.<br>Retire replaced worlds.<br>Defer callback arrivals.<br>Reject out-of-world positions. | HP04<br>HP09<br>HP14<br>HP19 | Reject recursive Pump.<br>Retire changed roles.<br>Rotate within the current table.<br>Reject nonfinite velocity. | HP05<br>HP10<br>HP15<br>HP20 | Retire changed Session pointers.<br>Guard contact-drain completion.<br>Preserve a newer resend request.<br>Refresh the actual primitive root. |
| HP21 | Bind root incarnation through capture. | HP22 | Recheck the hand before publication. | HP23 | Saturate backwards clocks. | HP24 | Retain refused End without stale commits. | HP25 | Retire only the accepted episode, once. |

| Boundary | World | Session | Actor | Class | Episode | Boundary | World | Session | Actor | Class | Episode |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Initial hand query<br>Host location | HT01<br>HT11 | HT02<br>HT12 | HT03<br>HT13 | HT04<br>HT14 | HT05<br>HT15 | Host pose turn<br>Host rotation | HT06<br>HT16 | HT07<br>HT17 | HT08<br>HT18 | HT09<br>HT19 | HT10<br>HT20 |
| Host velocity<br>Host End send | HT21<br>HT31 | HT22<br>HT32 | HT23<br>HT33 | HT24<br>HT34 | HT25<br>HT35 | Host End capture<br>Admitted pose turn | HT26<br>HT36 | HT27<br>HT37 | HT28<br>HT38 | HT29<br>HT39 | HT30<br>HT40 |
| Admitted publication | HT41 | HT42 | HT43 | HT44 | HT45 | Host publication | HT46 | HT47 | HT48 | HT49 | HT50 |

## Host prop-syncer controller ownership (6 November 2026)

Implemented PH01-PH15 / HS01-HS60 in prop_syncer_host and the driver's row helpers. PASS 75 named cases, slot 31, host takeover after slot reuse, live class/binding replacement, bounded election, native-key cancellation and nonfinite End controls. Fifteen host-controller mutations and eleven drive-stream regression mutations fail at assertions; both changed production translation units compile. PASS six scoped groups and stable-source guard. Native engine/transport are doubled; this does not qualify rendered multiplayer physics. Publisher ownership is recorded in the HP section; retained grant/revoke/End delivery and swinger join retries remain open. No Release DLL or package. Evidence: `build/prop-syncer-host-verification/verification.json`, `results.json` and `source-start.sha256`; host cases/mutations/object compiles: `build/prop-syncer-host-check/verification.log`; stream mutations: `build/prop-drive-lifetime-check/host-syncer-regression.log`. Each HS cell cancels the controller continuation after the indicated boundary and replacement. Application completion guards do not supply caller scope inside the stream's own native operations.

| ID | Completed host boundary | ID | Completed host boundary | ID | Completed host boundary | ID | Completed host boundary |
| --- | --- | --- | --- | --- | --- | --- | --- |
| PH01<br>PH05<br>PH09<br>PH13 | Own drained batches across callbacks.<br>Bind hand-back history to occupant/departure.<br>Own departure snapshots; reject invalid slots.<br>Saturate backwards-clock calculations. | PH02<br>PH06<br>PH10<br>PH14 | Reject recursive admission drains.<br>Stamp row revisions and actor/class incarnation.<br>Detach endings before native release/transport.<br>Own and bound throw-election inputs. | PH03<br>PH07<br>PH11<br>PH15 | Retire session/world/stop/role state.<br>Own claim/row/End input bytes.<br>Guard the opened grant across BeforeGrant.<br>Require the exact current End sender/generation. | PH04<br>PH08<br>PH12 | Bind rows to their original slot occupant.<br>Own periodic-pass snapshots.<br>Reject nonfinite/out-of-bounds row poses. |

| Boundary | World | Session | Sender | Actor | Class | Episode | Boundary | World | Session | Sender | Actor | Class | Episode |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Claim prop type<br>Claim reach authorization | HS01<br>HS13 | HS02<br>HS14 | HS03<br>HS15 | HS04<br>HS16 | HS05<br>HS17 | HS06<br>HS18 | Claim hand query<br>Grant BeforeGrant | HS07<br>HS19 | HS08<br>HS20 | HS09<br>HS21 | HS10<br>HS22 | HS11<br>HS23 | HS12<br>HS24 |
| First thrown-row location<br>Pass puppet location | HS25<br>HS37 | HS26<br>HS38 | HS27<br>HS39 | HS28<br>HS40 | HS29<br>HS41 | HS30<br>HS42 | Stream application completion<br>Pass prop location | HS31<br>HS43 | HS32<br>HS44 | HS33<br>HS45 | HS34<br>HS46 | HS35<br>HS47 | HS36<br>HS48 |
| Ending physics flags | HS49 | HS50 | HS51 | HS52 | HS53 | HS54 | Ending transport completion | HS55 | HS56 | HS57 | HS58 | HS59 | HS60 |

## Client prop-syncer ownership (6 November 2026)

Implemented PC01-PC25 / PS01-PS50 in prop_syncer_client; EndFromBody now accepts a caller guard between native reads. PASS all 75 named cases plus generation wrap/long-gap, root replacement, bound-eid and orphan-End retirement controls. Fifteen separately compiled syncer mutations and eleven existing drive-stream mutations are rejected at assertions. PASS production client/host object compiles, five scoped groups and stable-source guard. Function-row gate passes 626 rows in 199 files; full public prose still fails below. No Release build or package. Evidence: `build/prop-syncer-client-verification/verification.json`, `results.json` and `source-start.sha256`; detailed mutation/object log: `build/prop-syncer-client-check/verification.log`; stream regression: `build/prop-drive-lifetime-check/syncer-regression.log`. Engine/transport calls are doubled. Host controller ownership was addressed in the host-controller section; host join retries and native prop jitter/contact acceptance remain open. This is not a completed UP5 gameplay fix. Each PS cell is a completed cancellation boundary; episode includes same-gen replacement.

| ID | Completed client boundary | ID | Completed client boundary | ID | Completed client boundary | ID | Completed client boundary |
| --- | --- | --- | --- | --- | --- | --- | --- |
| PC01<br>PC05<br>PC09<br>PC13 | Own Tick snapshots across table growth.<br>Retire same-pointer session reconstruction.<br>Register Pending before claim transport.<br>Ignore duplicate live grants. | PC02<br>PC06<br>PC10<br>PC14 | Commit/erase only the captured episode revision.<br>Retire stopped-session state.<br>Preserve a grant delivered during that send.<br>Reject stale live grants; fresh native throw nonce proves replacement. | PC03<br>PC07<br>PC11<br>PC15 | Reject recursive Tick work.<br>Retire replaced-world state.<br>Refused send erases only its own Pending.<br>Distinguish cancelled from missing throws. | PC04<br>PC08<br>PC12<br>PC16 | Retire state after Session pointer replacement.<br>Retire changed-role state.<br>Own answer bytes across callbacks.<br>Detach before native throw cleanup. |
| PC17<br>PC21<br>PC25 | Own and revalidate backoff queries.<br>Reject nonfinite streamed velocity.<br>Retain refused Ends; close once accepted; cancel rebound/host-ended orphan debt. | PC18<br>PC22 | Saturate backwards pending-clock arithmetic.<br>Refresh and bind the actual primitive root. | PC19<br>PC23 | Saturate backwards rest/beat/age arithmetic.<br>Recheck hand ownership before publication. | PC20<br>PC24 | Reject nonfinite pose output.<br>Preserve interaction keepalive during callbacks. |

| Native boundary | World | Session | Actor | Class | Episode | Native boundary | World | Session | Actor | Class | Episode |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Claim key capture<br>Tick hand query | PS01<br>PS11 | PS02<br>PS12 | PS03<br>PS13 | PS04<br>PS14 | PS05<br>PS15 | Claim send completion<br>Sync location | PS06<br>PS16 | PS07<br>PS17 | PS08<br>PS18 | PS09<br>PS19 | PS10<br>PS20 |
| Sync rotation<br>Rest End capture | PS21<br>PS31 | PS22<br>PS32 | PS23<br>PS33 | PS24<br>PS34 | PS25<br>PS35 | Sync velocity<br>Rest End send | PS26<br>PS36 | PS27<br>PS37 | PS28<br>PS38 | PS29<br>PS39 | PS30<br>PS40 |
| Throw grant key | PS41 | PS42 | PS43 | PS44 | PS45 | Deferred break replay | PS46 | PS47 | PS48 | PS49 | PS50 |

## Tower operation ownership (6 November 2026)

Implemented TQ01-TQ20 / TO01-TO55 in fuse_tower_ops and the native kind helpers. PASS all 75 named cases, legal operations, slot 31, active duplicates, overflow during Wait and pinned spend reservations. Native calls/reflection are doubled; committed output/refund debt and native multiplayer acceptance remain open. Operation faults: 11 independent mutations rejected, production ops/kind objects compiled (`build/tower-ops-check/verification.log`). Tower-row/kind regression rejects 21 independent mutations (`build/tower-rows-check/ops-regression.log`). PASS five scoped groups and stable-source guard; fact_rows_gate passes 626 rows in 199 files. Full prose still fails; no new Release DLL or package. Evidence: `build/tower-ops-verification/verification.json`, `results.json` and `source-start.sha256` in the same directory. Each TO cell is a completed cancellation boundary; native helper guards also stop subsequent writes/effects when their caller is cancelled.

| ID | Completed queue/spend boundary | ID | Completed queue/spend boundary | ID | Completed queue/spend boundary | ID | Completed queue/spend boundary |
| --- | --- | --- | --- | --- | --- | --- | --- |
| TQ01<br>TQ05<br>TQ09<br>TQ13 | Detach waiting operations before native calls.<br>Require a connected, ready, current sender.<br>Reject recursive drains.<br>Preserve arrivals inside overflow callbacks. | TQ02<br>TQ06<br>TQ10<br>TQ14 | Restore current Wait to FIFO head.<br>Avoid backwards-clock timeout underflow.<br>Queue nested offers behind active work.<br>Drop queued/active duplicate sequences. | TQ03<br>TQ07<br>TQ11<br>TQ15 | Drop Wait after peer departure.<br>Refund rate capacity only to the current waiting owner.<br>Own detached overflow-refusal records.<br>Validate operation/kind before admission. | TQ04<br>TQ08<br>TQ12<br>TQ16 | Stamp queued session/world/peer ownership.<br>Bound and rotate frame drains.<br>Cancel overflow after ownership loss.<br>Clear departed ack/refusal/spend state. |
| TQ17 | Retire state on session/world reconstruction. | TQ18 | Bind recorded spends to sender/session. | TQ19 | Bind spends to actual tower/class incarnation. | TQ20 | Pin spends during validation; consume at decision. |

| Native boundary | World | Session | Sender | Tower | Class | Native boundary | World | Session | Sender | Tower | Class |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Part location<br>Insert difficulty | TO01<br>TO11 | TO02<br>TO12 | TO03<br>TO13 | TO04<br>TO14 | TO05<br>TO15 | Reach authorization<br>Tower state read | TO06<br>TO16 | TO07<br>TO17 | TO08<br>TO18 | TO09<br>TO19 | TO10<br>TO20 |
| Pull rule<br>Use rule | TO21<br>TO31 | TO22<br>TO32 | TO23<br>TO33 | TO24<br>TO34 | TO25<br>TO35 | Insert rule<br>Native use | TO26<br>TO36 | TO27<br>TO37 | TO28<br>TO38 | TO29<br>TO39 | TO30<br>TO40 |
| Native pull<br>Native easy insert | TO41<br>TO51 | TO42<br>TO52 | TO43<br>TO53 | TO44<br>TO54 | TO45<br>TO55 | Native insert | TO46 | TO47 | TO48 | TO49 | TO50 |

## Entity adapters (6 November 2026)

G01-G11/R01-R40/P01-P24: seven focused groups, eight independent fault controls, cooked native contracts and production object compiles pass; native dispatch/transport are doubled. Evidence: `build/entity-adapters-check/verification/results.json` and `fault-controls.log`; individual boundaries: [test coverage](tests/README.md#entity-adapter-boundaries-6-november-2026). Function-row gate passes 626 rows/199 files; full public prose still fails. No Release DLL or native multiplayer acceptance. The remaining entity routes in WORK/TODO stay open.

## Floppy-slot delivery and claims (6 November 2026)

Implemented SB1-SB5 and FC01-FC75 in floppy_slot_sync. The actual controller passes 75 named cancellation cases, claim parsing/uncertain-write corrections, 18 independently compiled fault controls and production object compilation. PASS run_events, server_breaker_native_test and disk_utf8 on a stable source snapshot; fact_rows_gate passes 626 rows / 199 files. Full prose still fails below. Evidence: `build/event-slot-check/verification.log`; scoped suite results and source stamp: `build/server-breaker-delivery-check/results.json` and `source-start.sha256`. Transport/reflection are doubled; no new DLL or native multiplayer acceptance. Each FC cell is a completed cancellation boundary; peer includes observed departure.

| ID | Completed boundary | ID | Completed boundary | ID | Completed boundary |
| --- | --- | --- | --- | --- | --- |
| SB1<br>SB4 | Require canonical acceptance by every ready recipient, including slot 31.<br>Cancel host publication/join after world/session/object/recipient replacement or departure. | SB2<br>SB5 | Retain refused delivery through later sweeps without another native consumption.<br>Keep paced, recipient-owned join debt; retain completed device kinds across other-kind refusal. | SB3 | Complete against captured bytes/digest; preserve newer and nested changes. |

| Boundary | World | Session | Actor | Class | Peer | Boundary | World | Session | Actor | Class | Peer |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Client initial digest<br>Client content read | FC01<br>FC11 | FC02<br>FC12 | FC03<br>FC13 | FC04<br>FC14 | FC05<br>FC15 | Client scalar read<br>Client final digest | FC06<br>FC16 | FC07<br>FC17 | FC08<br>FC18 | FC09<br>FC19 | FC10<br>FC20 |
| Client transport completion<br>Client refusal bookkeeping | FC21<br>FC31 | FC22<br>FC32 | FC23<br>FC33 | FC24<br>FC34 | FC25<br>FC35 | Client baseline completion<br>Host initial digest | FC26<br>FC36 | FC27<br>FC37 | FC28<br>FC38 | FC29<br>FC39 | FC30<br>FC40 |
| Host previous scalar<br>Host apply completion | FC41<br>FC51 | FC42<br>FC52 | FC43<br>FC53 | FC44<br>FC54 | FC45<br>FC55 | Host previous content<br>Host response continuation | FC46<br>FC56 | FC47<br>FC57 | FC48<br>FC58 | FC49<br>FC59 | FC50<br>FC60 |
| Client sweep digest<br>Laptop occupancy-settled digest | FC61<br>FC71 | FC62<br>FC72 | FC63<br>FC73 | FC64<br>FC74 | FC65<br>FC75 | Client overdue-answer continuation | FC66 | FC67 | FC68 | FC69 | FC70 |

## Current development verification (6 November 2026)

Workflow correction BO1-BO3: failed tests now stop before Release compilation; fresh/reused builds require native-function and full public-prose gates; scoped checks are test-only, with ChecksOnly available for the full pre-build stage. PASS nine build-order scenarios using the actual scripts with external-command doubles (`build/build-order-check/verification.log`); no release DLL was compiled. The 142-group evidence below predates this workflow change.

PASS **142/142 groups across the full run and one scoped recheck**, stable production sources, Release/ABI and source-list inspection, **1234 production entries**, **0.9.0n b2220 / protocol 2220**. BuildUpToDate=true; TwoPlayerGameplayVerified=false; Packaged=false.

The initial full run passed 141/142: run_2141's negative-control text anchor no longer matched after the cord guard moved into available(). The anchor was fixed; that group and all five of its negative controls then passed. Both runs verify the same production snapshot and DLL. The original failing result is retained.

- Combined evidence: `build/upstream-semantic-verification/coverage.json`.
- Full run: `build/upstream-semantic-verification/verification.json` and `logs/`.
- Scoped repeat: `build/upstream-semantic-recheck/verification.json` and `logs/`.
- DLL: `build/votv-coop-2164/main.dll`.
- DLL SHA-256: `714474dc8926d0aeb705871bfb88783740d8a35b0d9c4682799da1c51c35419f`.
- Production snapshot SHA-256: `b7167737d975d495bcaeab56162ce64a6e7e6e8a724ca712557d6e037ebf9bc1`.
- Four new independently compiled fault controls fail at assertions:
  `build/swinger-join-check/fault-controls.log`.

Previous 140/138/137-group evidence remains in `build/physics-child-verification/verification.json`, `build/tower-wrapper-verification/verification.json` and `build/backlog-20261105/verification.json`. Their focused controls retain the scope recorded in the corresponding sections below. The last native function-row check passed 626 rows / 199 files: `build/physics-native-check/fact-rows.log`. The separate full prose gate still fails; see the documentation check below. The development DLL has no new package, installation or publication recorded.

## Selected upstream integration seams (6 November 2026)

SI01-SI08 / CI01-CI02 / VI01-VI02 are implemented. Focused actual-controller fixtures pass, including 4/32-peer cord builds, 32 rider leases and slot-31 join capture. The compiled authority/lane policy covers the eight selected message kinds; it does not qualify every imported adapter. Full-run and scoped-recheck evidence for these sources is recorded above. Vulkan's fixture checks 12 context-identity changes and permits same-context resize; VI02 is source-reviewed and compiled, not a concurrent GPU execution test. Gameplay, GPU replacement and the concrete remaining WORK findings stay open.

| ID | Implemented boundary | ID | Implemented boundary | ID | Implemented boundary |
| --- | --- | --- | --- | --- | --- |
| SI01<br>CI02 | Own pending records through map growth/native callbacks.<br>Recheck drag/occupancy after disconnect and power callbacks. | CI01<br>SI03 | Preserve remotely dragged socket occupants.<br>Cancel on actor/class/world/session retirement. | SI02<br>VI01 | Register before apply; Forget/replacement cancels the old receipt.<br>Check Vulkan context before touching old fences. |
| SI04<br>SI08 | Recheck lid binding and hand/drive ownership before each write.<br>Acquire/return all 32 rider movement leases once. | VI02<br>SI06 | Recheck stopped/down inside the frame mutex.<br>Expire pending work even without a new index pass. | SI05<br>SI07 | Failed pose writes stop further writes; failed velocity zeroing retries.<br>Validate host capture, peer bounds and occupant generation. |

Focused logs: `build/swinger-join-check/cord-review.log` and `authority-review.log`. Four independently compiled mutations (receipt, deadline, four-rider cap and device identity) fail at assertions: `build/swinger-join-check/fault-controls.log`. Maintained runners: `run_swinger_join`, `run_vulkan_context`, `run_2170`, `run_placement`.

## Native physics and deferred children (6 November 2026)

Implemented NP01-NP45 / CR01-CR30 against the actual physics wrapper and child release coordinator. All 75 named cases pass; 22 independently compiled controls fail at assertions (11 per runner). Reflection, engine execution, hand state and clock are doubled. These results do not simulate contact physics or close the original joined-client resting-stack report. Failed declarative pose writes retain debt; a dispatched unpark is consumed once. Parent retirement releases a surviving unheld child without restoring an old pose. Evidence: `build/physics-native-check/fault-controls.log` and `build/child-release-check/fault-controls.log`; full verification is recorded above.

| ID | Implemented boundary | ID | Implemented boundary | ID | Implemented boundary | ID | Implemented boundary | ID | Implemented boundary |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| NP01<br>NP06 | Retire the primitive-class cache by slot and serial.<br>Retry unavailable classes/functions instead of permanently disabling physics. | NP02<br>NP07 | Retain native functions by slot and serial rather than raw pointer.<br>Retire a resolving thunk across world replacement. | NP03<br>NP08 | Retain each function's declaring owner and validate its binding.<br>Reject zero and negative native frame sizes. | NP04<br>NP09 | Refuse recursive resolution of an unfinished physics thunk.<br>Reject native frames larger than the fixed stack buffer. | NP05<br>NP10 | Publish a thunk only after all metadata and identities remain valid.<br>Validate SetSimulatePhysics boolean storage and mask. |
| NP11<br>NP16 | Validate the full linear-velocity vector parameter span and size.<br>Validate the simulation query's result storage and mask. | NP12<br>NP17 | Validate the linear-velocity BoneName parameter span and size.<br>Validate the centre-of-mass BoneName parameter span and size. | NP13<br>NP18 | Validate the full angular-velocity vector parameter span and size.<br>Validate the full centre-of-mass return vector span and size. | NP14<br>NP19 | Validate the angular-velocity BoneName parameter span and size.<br>Reject overlapping parameter spans, allowing only disjoint boolean bits. | NP15<br>NP20 | Validate the simulation query's BoneName parameter span and size.<br>Explicitly resolve and clear bAddToCurrent for velocity replacement. |
| NP21<br>NP26 | Reject non-finite linear velocity before native dispatch.<br>Cancel physics dispatch after world replacement. | NP22<br>NP27 | Reject non-finite angular velocity before native dispatch.<br>Validate the primitive ancestor incarnation through dispatch. | NP23<br>NP28 | Reject non-finite centre-of-mass output.<br>Reject completion after component retirement during native execution. | NP24<br>NP29 | Cancel physics dispatch after component slot/serial retirement during resolution.<br>Reject completion after function retirement during native execution. | NP25<br>NP30 | Cancel physics dispatch after the component's class is replaced.<br>Reject completion after declaring-owner retirement during native execution. |
| NP31<br>NP36 | Respect a refused native query instead of accepting its frame output.<br>Key component-member layouts by class incarnation rather than class name. | NP32<br>NP37 | Decode only the simulation result's declared boolean mask.<br>Bound Blueprint ancestor traversal and reject cycles. | NP33<br>NP38 | Commit centre-of-mass output only after successful current dispatch.<br>Validate component field extents against the actor's class size. | NP34<br>NP39 | Use aligned stack storage and byte copies for reflected vector/name fields.<br>Bound reflected component-field and layout-cache growth. | NP35<br>NP40 | Own resolved thunk metadata across nested physics calls.<br>Deduplicate inherited component offsets before physics changes. |
| NP41<br>CR01 | Accept only instanced object-pointer fields for component discovery.<br>Reject null or retired child/parent input. | NP42<br>CR02 | Validate the actor and component owner through each stop operation.<br>Reject a child that names itself as parent. | NP43<br>CR03 | Recheck the actor's component binding after the simulation query.<br>Bind deferred work to its world generation. | NP44<br>CR04 | Count a stopped component only when its setter succeeds.<br>Own capture records across native callbacks instead of referencing vector storage. | NP45<br>CR05 | Own component-offset snapshots and refuse recursive StopActorSimulating.<br>Publish a replacement wait before capture so older callbacks cannot overwrite it. |
| CR06<br>CR11 | Preserve a newer reentrant Defer for the same child.<br>Bind each wait to the child's class incarnation. | CR07<br>CR12 | Reject non-finite captured locations.<br>Bind each wait to the parent's class incarnation. | CR08<br>CR13 | Reject non-finite captured rotations.<br>Match parent-release notifications by object incarnation. | CR09<br>CR14 | Keep a location-read failure from publishing a fabricated pose.<br>Prevent backwards clocks from triggering timeout through unsigned underflow. | CR10<br>CR15 | Keep a rotation-read failure from publishing a partial pose.<br>Release a surviving unheld child when its parent disappears, without restoring an old pose. |
| CR16<br>CR21 | Preserve the existing timeout release for a parent that never ends.<br>Recheck hand ownership after the location setter. | CR17<br>CR22 | Traverse owned identities during parent-release processing.<br>Recheck hand ownership after the rotation setter. | CR18<br>CR23 | Traverse owned identities during tick processing.<br>Bind final unpark to the captured current mesh. | CR19<br>CR24 | Cancel old release work when a successor wait replaces it.<br>Retain declarative release debt after a location-write refusal. | CR20<br>CR25 | Yield to a new hand before restoring a release pose.<br>Retain declarative release debt after a rotation-write refusal. |
| CR26 | Defer nested pumps while preserving newly received parent-release work. | CR27 | Consume the release record before unpark to prevent duplicate native release. | CR28 | Detach disconnect work and refuse reentrant Defer during teardown. | CR29 | Preserve a child's current hand ownership during disconnect. | CR30 | Invalidate in-flight capture/release continuations on disconnect, even if the world stays live. |

## Native tower wrapper boundaries (5 November 2026)

Implemented CW01-CW37 / RW01-RW38. `run_tower_wrappers.ps1` compiles both actual wrappers and real ParamFrame; the 75 named cases pass. Its fault controls reject 13 independently compiled faults per wrapper (26 total). Native reflection, timeline reads, hit construction and engine execution are doubled. Current-state reads commit atomically; native effects already dispatched are never rolled back. Evidence: `build/tower-wrapper-check/fault-controls.log` and the current suite log. UP4 still owns operation queue/claim/debit/output/refund, lower-level timeline schema review and native multiplayer acceptance. A refused wrapper completion does not prove a native effect was absent; higher layers must retain their receipts.

Each ID below is a separate completed fix in the indicated wrapper.

| Coordinate | Radio | Completed fix | Coordinate | Radio | Completed fix | Coordinate | Radio | Completed fix |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| CW01<br>CW04 | RW01<br>RW04 | Reject a live object of the wrong tower class before raw field access.<br>Validate scalar field extents against the class allocation. | CW02<br>CW05 | RW02<br>RW05 | Resolve offsets again when the class incarnation changes.<br>Reject unresolved, out-of-range or invalid boolean masks. | CW03<br>CW06 | RW03<br>RW06 | Retire a failed-layout latch when its class incarnation changes.<br>Validate complete array-header extents before reading them. |
| CW07<br>CW10 | RW07<br>RW10 | Validate component-pointer field extents before reading them.<br>Cancel a state read after world replacement in a timeline callback. | CW08<br>CW11 | RW08<br>RW11 | Validate look-at flag and index extents before staging a use.<br>Cancel a state read after the tower class changes in a timeline callback. | CW09<br>CW12 | RW09<br>RW12 | Cancel a state read after the tower slot/serial changes in a timeline callback.<br>Cancel a state read after its timeline binding changes. |
| CW13<br>CW16 | RW13<br>RW16 | Publish state output only after every native read remains current.<br>Reject malformed or oversized puzzle array headers. | CW14<br>CW17 | RW14<br>RW17 | Reject malformed or oversized fuse array headers.<br>Bound fuse writes by both native length and the supported fuse count. | CW15<br>CW18 | RW15<br>RW18 | Reject fuse states outside empty/good/blown.<br>Reject invalid fuse values before any write. |
| CW19<br>CW22 | RW19<br>RW22 | Bound puzzle writes and require the exact native array length.<br>Cancel location output after part replacement during the native query. | CW20<br>CW23 | RW20<br>RW23 | Validate part-array headers and supported button/fuse indices.<br>Cancel use continuations when the tower incarnation retires. | CW21<br>CW24 | RW21<br>RW24 | Reject non-finite native part locations without publishing output.<br>Cancel use continuations when the player incarnation retires. |
| CW25<br>CW28 | RW25<br>RW28 | Cancel use continuations when the selected part retires or is rebound.<br>Own the caller's LookAt input across native callbacks. | CW26<br>CW29 | RW26<br>RW29 | Bind use dispatch to the resolved function incarnation.<br>Refuse recursive wrapper interactions while look-at staging is active. | CW27<br>CW30 | RW27<br>RW30 | Reject oversized reflected frames before allocating them.<br>Restore look-at only while the staged values still belong to this invocation. |
| CW31<br>CW34 | RW31<br>RW34 | Restore owned look-at state during exception unwinding.<br>Validate audio component binding through playback callbacks. | CW32<br>CW35 | RW32<br>RW35 | Bind insert dispatch to the fuse incarnation through frame construction.<br>Guard lever dispatch and its completion against retired bindings. | CW33<br>CW36 | RW33<br>RW36 | Guard parameterless painters against lookup/frame-time retirement.<br>Reject invalid button/fuse indices in ReadLookAt without publishing output. |
| CW37 | RW37 | Reject invalid fuse indices in ReadFuseLook without publishing output. | — | RW38 | Resolve the radio tower from the current gamemode binding while the previous tower remains live; validate field extent and both incarnations through resolution. |  |  |  |

## Release 0.9.2163 qualified offline (28 October 2026)

This is the retained package selected by `package/release.json`, separately from current development. PASS fresh package qualification: **124/124 groups**, Release/ABI, unchanged source/release inputs and a **32-member** manager archive with matching manifest, loader, branding, models and DLL. Rejection controls refuse incomplete/focused/failed suites, stale UI labels and mismatched DLL bytes.

- Archive: `dist/ElvardFIC-WatchersOfTheVoid-0.9.2163.zip`.
- DLL SHA-256: `5d1e5de9db22a3a3bcc3f82d3c3854a8bfdebe768ee96f827a5204f6c3078a8e`.
- Archive SHA-256: `dba1cbdfcf84d5d605da82f52430589c332f4f7cdfb621f1acfc41d8730aa147`.
- Checksums: `dist/SHA256SUMS-current.txt`.
- Qualification: `build/qualification-current/verification.json`.
- Package evidence: `build/package-current/verification.json`.

TwoPlayerGameplayVerified=false; archiveStartupVerified=false. Cleanup targets and retained hashes are in `package/cleanup-inventory.json`.

## Prop-drive lifetime and failed-marker cleanup (5 November 2026)

Actual marker wrapper, prop-drive receiver/host, bind-wait code and interpolation primitive are exercised against native/transport doubles. Coverage includes every native end seam, first/steady pose reentry, same-generation revisions, mesh/world/ body replacement, nested ticks, input mutation, failed cleanup and hand transfers. The current verification and retained 15 control results cover these boundaries.

- [x] LM1: Pump failed-marker deletion from the game-thread frame tail at most four times per second while debt exists, without waiting for another spawn.
- [x] LM2: Retain refused cleanup across disconnect and retry outside a running network session; only bodies created by this wrapper are eligible.
- [x] LM3: Refuse nested marker creation during construction or cleanup; keep cleanup guards through native callback reentry.
- [x] LM4: Retire cleanup on actor/class/world replacement, retain it while the current world is temporarily absent, and cancel it after verified adoption.
- [x] PD1: Detach an ending drive and close its generation before native calls; no live-map iterator crosses final transform, physics or child-release callbacks.
- [x] PD2: Use the same generation/watermark admission for host releases and client ends, rejecting older/duplicate host releases and old in-flight poses.
- [x] PD3: Stop final-pose continuations after disconnect, world/body/component replacement, a new hand, or a successor drive/end.
- [x] PD4: Validate direct pose/end input for finite bounded transforms, generation, key length, reserved bytes and defined end flags before changing stream state.
- [x] PD5: Own input packets and drained transport batches through native reentry; a caller mutation or disconnect cannot change the in-flight packet's values.
- [x] PD6: Traverse a snapshot of drive identities and apply owned records, with no live-map references across native interpolation/query/park callbacks.
- [x] PD7: Reconcile a replacement mesh even while its predecessor is still live; bind velocity and render continuations to the current component incarnation.
- [x] PD8: Exclude recursive drive ticks with a stack-owned guard; new input stays queued for the next outer tick, including callbacks that disconnect.
- [x] PD9: Bind pose/tick completion to its application revision, preserving a newer packet even when it has the same stream generation.
- [x] PD10: Use the canonical grab/gravity/holding/last-held/hand-axis predicate and commit hand-yield watermarks before callbacks can install a successor.

## Tower row ownership audit (4 November 2026)

T01-T45 / K01-K30 are implemented and included in the current joint verification. Rows retain recipient-generation/revision debt; delivery rotates at eight packets per pump. Apply receipts distinguish native effects from declarative repaint debt. Native solvers fill their puzzle arrays: canonical fuse values are staged before repair and exact cells reasserted afterwards. Radio setBroken emits stateChanged and starts its timer, so it is admitted once rather than replayed on repaint.

PASS actual row/kind coordinators, compiled RefusedTold/FIFO and cooked contracts; 18 independently compiled fault controls are rejected. Native execution, reflection and transport are doubled. Evidence: `build/tower-2164/fault-controls-final.log`. Host operation queue/claim/spend/refund, lower-level timeline schemas and native multiplayer acceptance remain UP4 follow-ups. Tower wrappers are recorded above.

| Completed item | Completed item | Completed item | Completed item | Completed item | Completed item |
| --- | --- | --- | --- | --- | --- |
| T01: Clear tower rows when the installed Session pointer changes.<br>T07: Clear delivery debt and diagnostics for all 32 slots independently of roster teardown. | T02: Clear tower rows on same-pointer session serial replacement.<br>T08: Accept rows only for the installed running connected client. | T03: Clear tower rows on a changed session stop counter.<br>T09: Validate row admission inside OnRows, independently of its router. | T04: Clear tower discovery/state on world-generation replacement.<br>T10: Reject oversized row payload counts before indexing. | T05: Clear tower host/client state on role replacement.<br>T11: Reject flags that the specified tower kind does not own. | T06: Clear the installed tower-row session pointer at disconnect.<br>T12: Reject negative coordinate tower identities. |
| T13: Reject nonzero radio tower identities.<br>T19: Own discovery output across callback-driven state reset. | T14: Reject light bits outside the declared puzzle width.<br>T20: Discard discovery results invalidated by session/world replacement. | T15: Discard duplicate kind/id rows deterministically.<br>T21: Commit class-version/discovery cache metadata only for its captured scope. | T16: Reclaim kept entries omitted from a newer full snapshot.<br>T22: Own the tower list while reading native rows. | T17: Validate cached tower incarnations by slot/serial instead of probing old raw pointers.<br>T23: Refuse a row read whose tower incarnation changed. | T18: Clamp discovery counts before reading a native discovery buffer.<br>T24: Commit host row snapshots only after all captured authority checks pass. |
| T25: Keep broadcast retry debt when the transport refuses the packet.<br>T31: Wait for connected world-ready peers before delivering join state. | T26: Do not clear a successor session dirty latch after an old send.<br>T32: Do not clear newly queued join debt after an older successful send. | T27: Exclude recursive host sends without losing newly requested work.<br>T33: Bind refused-claim answer delivery to sender generation. | T28: Preserve a dirty revision created while an earlier send is running.<br>T34: Consume only the refused sequence actually carried by a successful send. | T29: Prevent an older broadcast result from replacing a newer sent snapshot.<br>T35: Bound and fairly rotate per-tick refused-answer sends. | T30: Bind join delivery debt to the intended sender generation.<br>T36: Preserve new answer debt created inside an older transport callback. |
| T37: Own kept row/apply-context data through native callbacks.<br>T43: Bind panel retry clocks to tower identity rather than payload row position. | T38: Do not complete a newer kept revision from an older apply result.<br>T44: Refuse stale panel player/tower/results after native callbacks. | T39: Retain pending rows after refused reads or declarative writes.<br>T45: Reconcile session/world scope before exposing drill state. | T40: Prevent recursive application and cancel stale native continuations.<br>K01: Refuse coordinate state reads invalidated by actor retirement. | T41: Keep the apply guard scoped through disconnect/reset and exceptions.<br>K02: Bound coordinate light/fuse counts before local array access. | T42: Commit application counters/warnings only into their captured scope.<br>K03: Retain retry debt after a coordinate light-write refusal. |
| K04: Admit the coordinate repair native effect once per target revision.<br>K10: Consume the coordinate pull sound before native dispatch. | K05: Stop coordinate repair painters after authority retires.<br>K11: Consume the coordinate insert sound before native dispatch. | K06: Stop coordinate break painters after authority retires.<br>K12: Consume the coordinate failed-lever sound before native dispatch. | K07: Admit a coordinate click sound once across interrupted retries.<br>K13: Admit a coordinate lever movement once per target revision. | K08: Stop coordinate fuse writes at the first refused or retired operation.<br>K14: Guard the final coordinate lever-moving field update. | K09: Stop coordinate fuse painters after authority retires.<br>K15: Refuse radio tower reads invalidated by actor retirement. |
| K16: Bound radio cell/fuse counts before local array access.<br>K22: Consume the radio pull sound before native dispatch. | K17: Retain retry debt after a radio cell-write refusal.<br>K23: Consume the radio insert sound before native dispatch. | K18: Admit radio repair completion once and retain declarative convergence.<br>K24: Consume the radio damaged-fuse alert before native dispatch. | K19: Consume the radio broken alert before native dispatch.<br>K25: Guard radio confirm/failure sounds against stale continuation. | K20: Stop radio puzzle/fuse painters at refusal or authority retirement.<br>K26: Admit the radio confirm timeline once per target revision. | K21: Stop radio fuse writes at the first refused or retired operation.<br>K27: Consume the radio reset-lever sound before native dispatch. |
| K28: Admit the radio reset timeline once per target revision. | K29: Guard the final radio animation field update. | K30: Bind panel convergence to the captured row, tower and player scope. | ? |  |  |

## Optional EmoteWheel integration (4 November 2026)

Implemented optional native adapter; supplied mod scripts/assets are unchanged. Current joint verification includes run_emote, adjacent hand/ragdoll/seat suites and reliable routing. Three emote fault controls are rejected; additional native held-item regression evidence: `build/emote-held-regression-final.log` and `build/emote-check/results.log`.

- [x] EW1: all 316 emotes / 442 manifest-order phases and variants, with exact animation lengths, loops and sound associations; metadata only.
- [x] EW2: actual local Mesh montage/SpawnSound2D observation. The orbit camera can stop the first-person mesh ticking, so the source uses Mesh first.
- [x] EW3: a 64-byte kind 267 payload, owner/occupant/generation validation, finite bounded values, canonical host sequences and no blind relay.
- [x] EW4: exact remote phases/variants, current playback positions, drift correction and independent spatial music clocks. Missing animation and music channels commit separately instead of restarting the successful channel.
- [x] EW5: refused-send retries, current-state join replay and bounded 32-peer delivery; solo-host observation survives the first connection edge.
- [x] EW6: owned montage/audio cleanup, source lease expiry, body and animation instance replacement, world/session/stop/role/occupant retirement.
- [x] EW7: missing-pak and missing-resource caching, ordinary player/hand fallback and forwarding through a host without EmoteWheel resources.
- [x] EW8: production-controller/native-wrapper fixtures, injected faults and stable-source Release/ABI verification.
- [x] EW9: generation-matched visible held-component bone pose. The display actor moves while preserving its component-relative transform. Retired native continuations cannot fall through into ordinary puppet dereferences.

Reflection, roster, transport and engine execution are doubled. Function signatures are derived from declarations and captured encoding controls; no fresh live census is established. Mixed installations, model/hand/cigarette, ATV/ragdoll/sleep and phase/audio behavior require native acceptance.

## Radio ownership audit (4 November 2026)

R01-R45 / M01-M30 are implemented and included in the current joint verification. Bounds remain 256 discovery/dirty/cache entries, 64 requests/UI notes, 10000 catalogue entries, 256 reflected bytes, 32768 UTF-16 path units and one-day playback. A full dedupe cache does not prevent broadcasting an admitted state.

PASS controller, actual wrapper/ParamFrame, cooked contract and slot-31/32 cases; four retained and 18 additional fault controls are rejected. Scoped compiler/hash evidence: `build/radio-ownership-check/qualification.log` and `build/radio-ownership-check/production-compile.json`. The newer complete linked DLL verification is recorded above. Native playback/rendered acceptance is pending.

| Completed item | Completed item | Completed item | Completed item | Completed item | Completed item |
| --- | --- | --- | --- | --- | --- |
| R01: Fill canonical radio acknowledgements for all 32 peers.<br>R07: Retain actor incarnation while capturing a radio key. | R02: Clear every peer acknowledgement/attempt on world replacement.<br>R08: Reject identity captures invalidated while resolving element IDs. | R03: Clear every peer acknowledgement/attempt at disconnect.<br>R09: Refuse conflicting key and element-ID actor resolutions. | R04: Accept valid request senders 4 through 31 and reject slot 32.<br>R10: Retire state on installed Session pointer replacement. | R05: Consume host acknowledgements for local peer IDs 4 through 31.<br>R11: Retire state on same-pointer session serial replacement. | R06: Reject empty/None/non-ASCII/embedded-null malformed wire keys.<br>R12: Retire state on session stop-count replacement. |
| R13: Retire state when the session role changes.<br>R19: Retain the resolved actor incarnation through reach authorization. | R14: Retire callback results across world replacement.<br>R20: Refuse application after authorization retires sender/controller authority. | R15: Disable all radio hooks immediately at disconnect.<br>R21: Carry controller authority through the complete native radio apply. | R16: Require a running host and a connected world-ready request sender.<br>R22: Own host attempt snapshots across native callbacks. | R17: Bind acknowledgements and pending host attempts to sender transport generation.<br>R23: Prevent recursive admission of an in-flight request from the same slot. | R18: Revalidate sender scope after native body-availability queries.<br>R24: Keep receive exclusion active through occupant/controller replacement. |
| R25: Prevent an old apply result from acknowledging a successor occupant.<br>R31: Own canonical paint snapshots across recursive receives. | R26: Prevent an old publish result from replacing a newer sent-state cache.<br>R32: Cancel an older paint when a newer canonical revision arrives. | R27: Keep sent-cache references out of transport callbacks.<br>R33: Snapshot tick work instead of retaining map iterators through native calls. | R28: Bound discovered radio entries without evicting a live owner.<br>R34: Exclude recursive Tick while preserving admission through reset. | R29: Bound locally dirty radio entries without evicting a live edit.<br>R35: Own the pending request packet across transport callbacks. | R30: Bound sent-state cache entries.<br>R36: Never stamp a successor pending head with an old send result. |
| R37: Refuse cumulative acknowledgements newer than any offered request sequence.<br>R43: Validate the radio resolved from a UI widget before marking it dirty. | R38: Do not reset a newer host retry attempt with an older request.<br>R44: Discard cancelled UI permissions and validate scope after end-track queries. | R39: Keep the host retry deadline tied to its owned request across callbacks.<br>R45: Clamp canonical playback extrapolation before signed tick arithmetic. | R40: Track user-radio body permission by a live script invocation.<br>M01: Validate actor incarnation throughout radio-class classification. | R41: Preserve outer user-radio invocations through nested callbacks.<br>M02: Validate resolved radio-class incarnations before classification succeeds. | R42: Refuse stale widget/function/session observations in UI POST.<br>M03: Keep ATV radio exclusion closed while its class is unresolved. |
| M04: Refuse object-field reads after their source actor retires.<br>M10: Use overflow-safe link/url/stride bounds for radio data. | M05: Validate an object-valued field before exposing it to callers.<br>M11: Reject malformed local FString lengths/capacities before hashing. | M06: Bind reflected radio layout to the actor class incarnation.<br>M12: Reject unterminated or embedded-null catalog paths. | M07: Bind reflected radio layout to the data struct incarnation.<br>M13: Retain gamemode/catalog-owner incarnation through catalog lookup. | M08: Bind reflected radio layout to the openLink function incarnation.<br>M14: Own the selected catalog bytes instead of returning an engine-array pointer. | M09: Commit layout metadata atomically after native resolution.<br>M15: Use a per-operation layout snapshot through nested class resolution. |
| M16: Retain the media query target through dispatch-function lookup.<br>M22: Stop radio reads when its mediaPlayer binding changes during a query. | M17: Retain media query class/function incarnations through ParamFrame preparation.<br>M23: Commit a complete read State only while actor/player/controller remain current. | M18: Check caller authority immediately before each media query dispatch.<br>M24: Prepare every openLink argument before changing pause/mode fields. | M19: Leave query output unchanged when the target retires during dispatch.<br>M25: Own the selected path string through openLink frame preparation and dispatch. | M20: Free a returned GetUrl FString even when authority retires.<br>M26: Refuse a malformed openLink frame without changing the actor pause/mode. | M21: Validate returned GetUrl FString capacity and terminator.<br>M27: Guard Seek through player/function/class/caller retirement. |
| M28: Do not run Play/Pause after a Seek callback retires ownership. | M29: Validate playback-command ownership after its native attempt. | M30: Bound playback ticks before seeking and comparing native times. | ? |  |  |

## Queue and coffee ownership audit (4 November 2026)

The 75 reviewed boundaries below are included in the current joint verification. Q01-Q12, C01-C23, W01-W06 and A01-A07 were retained implementations verified by production fixtures; S01-S27, the final C20 key check and function-incarnation capture were completed in this audit. These are review boundaries rather than 75 separate newly reproduced defects.

Actual queue/bucket, coffee intent/state and coffee/appliance wrappers with real ParamFrame pass; 11 independent fault controls are rejected: `build/coffee-2164/fault-controls.log`. Publication retains at most 128 debts, retries eight per tick after 250 ms and preserves successor revisions. Native transport/roster/reflection are doubled; simultaneous presses, bag/cup fill, join, disconnect and world replacement still need multiplayer acceptance.

| Completed item | Completed item | Completed item | Completed item | Completed item | Completed item |
| --- | --- | --- | --- | --- | --- |
| Q01: initialize a token bucket exactly once even when its first clock is zero.<br>Q07: do not pop a successor occupant's head when an old callback returns Done. | Q02: do not move the refill clock backwards and credit the same elapsed interval twice.<br>Q08: do not refund an old Wait into the successor occupant's bucket. | Q03: apply a reduced burst ceiling before a same-clock spend.<br>Q09: cancel service when its clock callback replaces the queued head. | Q04: reject non-finite/negative token budgets instead of admitting a NaN stream.<br>Q10: consume an uncertain callback attempt on exception, without replaying it next tick. | Q05: prevent recursive service of the same pending intent.<br>Q11: release the service guard on callback/copy/clock exceptions. | Q06: own the served payload across occupant reset and native callbacks.<br>Q12: keep service admission closed through in-callback queue replacement. |
| C01: reject coffee press frames with null function/locals.<br>C07: reject malformed incoming device keys at the handler boundary. | C02: bound the action parameter read by the function frame.<br>C08: require the installed, running host and a world-ready connected sender at ingress. | C03: refuse unreadable client action frames instead of brewing locally.<br>C09: bind queued presses to the sender's transport generation. | C04: cancel a client press if key lookup retires its actor.<br>C10: clear coffee intents when the installed Session pointer changes. | C05: cancel a client send after session/world retirement during key lookup.<br>C11: clear them on same-pointer session serial changes. | C06: reject non-ASCII, empty, None and oversized outgoing device keys.<br>C12: clear them on world changes. |
| C13: clear the coffee intent session pointer at disconnect.<br>C19: refuse a recycled puppet incarnation after reach callbacks. | C14: clear queued presses at disconnect independently of roster teardown order.<br>C20: refuse a rekeyed/rebound device before native action admission. | C15: refuse a Tick argument belonging to another session.<br>C21: do not write old action results into successor-session counters. | C16: stop the slot drain after a callback retires the host scope.<br>C22: do not clear a successor occupant's wait state after an old callback. | C17: retain the authorized device incarnation through native action preparation.<br>C23: expire a press waiting indefinitely for the sender's body. | C18: pass the same puppet body that supplied the reach verdict.<br>S01: match coffee PRE/POST notes by invocation, not actor address alone. |
| S02: preserve an outer brew/hit note across nested watched calls.<br>S08: reject notes after a host/client role change. | S03: reject notes whose actor incarnation changed before POST.<br>S09: retire PRE snapshots invalidated by native flag/other-actor reads. | S04: reject notes whose UFunction incarnation changed before POST.<br>S10: consume a POST note before callbacks so duplicate POST cannot publish twice. | S05: reject notes from an earlier installed session pointer.<br>S11: retain the bag's exact incarnation while capturing PRE amounts. | S06: reject notes from an earlier same-pointer session serial.<br>S12: own the POST snapshot across recursive publication. | S07: reject notes from an earlier world.<br>S13: cancel publication when POST maker-flag reads retire the actor. |
| S14: stop the bag branch after maker publication retires the session.<br>S20: drop retry debt whose actor/session/world ownership expired. | S15: do not update new-session publication counters from an old callback.<br>S21: clear old notes and retry debt on Install context replacement. | S16: guard the publisher's key read against actor retirement.<br>S22: disable coffee state callbacks immediately at disconnect. | S17: reject rekeyed actors during publication/retry.<br>S23: bound cancelled-hit diagnostics and distinguish recycled maker addresses. | S18: keep controller authority through record capture/chunk delivery.<br>S24: bound class tallies and distinguish recycled class addresses. | S19: retain bounded retry debt for refused guarded publications.<br>S25: settle refused coffee state watch registrations once. |
| S26: validate OtherActor frame bounds before reading its pointer.<br>W05: cancel bag-field reads invalidated during offset resolution. | S27: bind awaited client-record admission to the current host/sender/actor scope.<br>W06: retry missing coffee bool metadata without requiring a new class address. | W01: guard coffee classification through class lookup and actor replacement.<br>A01: refuse appliance actions when the player parameter write fails. | W02: invalidate hasCup offsets on class serial replacement.<br>A02: refuse appliance actions when the action parameter write fails. | W03: invalidate hasCoffee offsets on class serial replacement.<br>A03: validate the appliance actor incarnation through function lookup. | W04: invalidate coffeeReady offsets on class serial replacement.<br>A04: validate the player incarnation through frame preparation. |
| A05: validate class/function incarnations through frame preparation. | A06: check the caller's authority immediately before native dispatch. | A07: report caller retirement after an attempted native action without replaying it. | ? |  |  |

## Real upstream history merge (4 November 2026)

Merge `a3632a687` has parents `353debaff` and upstream `4fbf1acf0e0ec7d14350132cd3de01d6a86aa552`, recording the formerly missing 436-commit ancestry. Eighty conflicts were reconciled while retaining fork wire IDs, 16-bit reliable kinds, 32 peers and transaction/lifetime guarantees. The fork cord owner remains authoritative; the alternate upstream cord lane was excluded to avoid duplicate ownership. Container acts retain IDs 264/265/266; EmoteState uses 267, with compile-time table coverage through that final ID. The earlier claim that source transfer alone completed history integration is superseded by this merge. UP2/UP4/UP5/UP6 semantic reviews remain open.

Historical verification retained for provenance:

| Evidence | Original scope |
| --- | --- |
| `build/upstream-history-2164/verification-final/verification.json` | b2219; 133 groups, Release/ABI and 1232 entries. |
| `build/coffee-2164/verification/verification.json` | b2219; 134 groups and stable source/Release verification. |
| `build/tower-2164/verification-complete/verification.json` | b2220 before the 5 November physics/marker changes; 137 groups. |

Their original DLL/source hashes remain in those JSON records; the latest build identity is stated only in Current development verification above.

## Integration error audit (30 October 2026)

UP1 documentation conflict reconciliation and UP7 fixture/build repairs are complete. The formerly failed supplemental fixtures were repaired; run_2134 is an alias of run_2136, not an independent suite. Their old failure lists and interrupted/combined qualification totals are superseded by the current full run. Original audit evidence: `build/work-todo-audit/recheck/audit-summary.json` and `build/work-todo-audit/server-tests.log` (132 server tests at that audit). The later source-sync server check also passed 132 tests; this cleanup reruns neither the server nor engine tests.

## Upstream integration b2215 (25 October 2026)

Retained reference for the test catalogue's integration boundaries. Published fork reliable IDs, coin admission, checked container writes, hold arbitration, server delivery, laptop/screen leases and native/extraction ownership were preserved through the later imports and real history merge. The current full run covers maintained upstream integration/refresh/container fixtures. Remaining semantic work belongs to UP2/UP4/UP5/UP6 in WORK.

## Joined-client prop physics report (29 October 2026)

The host appears stable while a joining client sees objects vibrate, launch or penetrate boxes, desks and workbenches. Exact versions, other mods, network conditions and whether client displacement reaches the host remain unknown. This has not been reproduced or closed. Prop-drive lifetime/end repairs do not establish the report's root cause; no dedicated stack-physics setting is present.

- [ ] Capture matching host/client builds, mod lists, network conditions and logs.
- [ ] Compare resting objects/stacks, pickup/drop and late join on both roles;
  trace pose/drive/park/end ownership and native simulation state.

## Open reports and unique diagnostic evidence

These remain open despite passing offline fixtures. Implementation constraints stay in TODO/WORK; the table retains report identity, observations and evidence.

| Report | Established boundary and remaining acceptance |
| --- | --- |
| Remote flashlight | Capture/reader/join/replacement fixtures pass. Verify live beam, modes, battery, clicks, initial join and same-component drift on matching peers. |
| Ragdoll 5/50 | C-key/faint/fall entry, selected-model pose transfer and recovery need current all-role acceptance, including slopes, crouch, repeated use and custom rigs. |
| PC boot 54/55 | The old shared-screen fixture reproduced rootswitch being reset to 0 during native boot; desktop-only traversal passes. Verify repeated power, input/handoff, power loss and join/reconnect. |
| Tutorial crash/icons | A local 28 October inspection found 2471 empty item .vimg files. Cache/profile-placement fixtures pass; regeneration and Tutorial/campaign transitions need native acceptance. |
| Tutorial stack overflow | Crashes B7D06444... and E90A44A5... record EXCEPTION_STACK_OVERFLOW; E90A44A5 has 11939 chained FFrames, 11937 on one node. `build/tutorial-crash-20261028.txt`. |
| Native VM recursion | Isolated launches proved recursive lib_C.end. Untitled_1.CustomEvent's borders guard is a candidate initiating caller, requiring confirmation. `build/requested-events-native/crash-analysis-20261024.md`; no guard suppression is recorded. |
| Bench58 host rifle | Reproduce disappearance after placement; capture class/key/destruction caller with VOTVCOOP_PROP_BIRTH_KEY_PROBE=1. prop_rifle_C remains a candidate. |
| Token52 | Identify the original UI and observe credit/spend/refusal/join. Shared saveSlot.Points remains a hypothesis. |
| Trowel | Capture an actual stock/key mismatch through hand/slot/drop. Native graphs preserve non-None keys; the report does not justify inventing a repair. |
| Item-copy report (2) | Build and copy trigger were unidentified. Images show shovel/noodle packaging without class/key/EID or transition evidence: `build/report-bugs/item-copy.png`, `item-copy-other-peer.png`. |
| Warp/blank-email report (6.1) | arirShipAppear's delayed email loop is a candidate; its phase adapter does not close reputation/email or Vanishing variants. Image and native dumps: `build/report-bugs/warp-event.png`, `arirShipAppear.txt`. |

Recorded historical native experiments on b2184 (22 October) covered NullRHI entry/recovery, momentum, camera, dynamic-platform contact, installed models, remote transitions and impaired LAN. They applied to DLL SHA-256 `7bf4a68037a384605ae54fd9c8939564ba531f274d9b94fb6fc5f99617f647d2`. Their old build directories are absent; this retained observation does not qualify current rendered/input behavior or b2220 gameplay.

## Pending native multiplayer acceptance

All items below remain unperformed for the current development DLL. Implemented routes and incomplete adapters are distinguished in TODO/WORK.

- [ ] Real 2?32-player admission, full/reused slots, selector/input and master capacity; no deployed-master qualification is recorded.
- [ ] Switches, doors, transformer repair, lamps/ambient and drone contents after join, concurrent use and replacement.
- [ ] Shared prop contact/blast motion, hold handoff and late observers; attached glowstick constraints/hooks and ordinary/wall-attachable behavior.
- [ ] Companion ATV creation, saved rigs, steering/honk, driver/carrier/seat handoffs, save/reload and reconnect, without duplicate vehicles.
- [ ] Seated body/head/light/chair angles, tilted supports, banked vehicles, IK recovery, custom rigs and 32-player presentation load.
- [ ] Sleep unanimity, awake/joining/dreaming vetoes, wake input, personal nightmares and rewards across replacement/reconnect. Per-mattress capacity remains deferred.
- [ ] Inventory/profile/container join, ejection/extraction and concurrent use; repair/resource/food/package/grill debit, output and refund through refusal, handoff, disconnect and save/reload.
- [ ] Console simultaneous input, history/clear/busy state and native command effects; laptop/shared-screen inherited-state handoff and slow-loading UI.
- [ ] Held/world toy and watoy timing, audio, physics and variant schemas; original-dab sponge/window foam, cleaning stock, stains and save/join parity.
- [ ] Event triggers/outcomes/repeats, loading, rate changes and current-state joins; personal dreams/endings/calendar gates, shared sky/audio/email consequences.
- [ ] Wisp grab/release/tear/kill with source disappearance, body/world/AnimInstance replacement and movement/camera recovery; remaining entity attacks/effects.
- [ ] Current manager archive startup, menu branding and combined mod profiles.

## Requested third-party mod compatibility (28 October 2026 audit)

Eleven pinned archives were statically inspected with recorded SHA-256 values; `audit_requested_mods.py --offline` passed. Evidence: `build/requested-mods-audit`. Reports retain runtime_verified=false. The audit was performed before installation; it establishes no native compatibility pass. EmoteWheel subsequently gained the optional adapter described above.

| Mod | Remaining profile/native acceptance |
| --- | --- |
| EmoteWheel 1.0.1 | Mixed installations, variants/phases/audio, visible hands and movement/death/ragdoll/body interruption. |
| Fusion 0.8108.3 | One merged table output, all six dependants' rows/classes and launch paths with spaces. Older-target text is a risk to verify. |
| VEmu 1.1.0 | Shared console/cartridge/control ownership and emulator AV/state; upstream explicitly excludes SmartTV AV. |
| Female Skin 1.0.0 | Resolve original Multivoid dependency; verify one active DLL, lowpoly_girl appearance/reconnect/material/animation/ragdoll and missing-asset fallback. |
| JeepMod 0.11.1 | Custom keys/trunk/container/upgrades/decals, fuel/damage, save/rejoin and one vehicle/inventory. Cooked Jeep_C derives from ATV_C. |
| FireTower 1.0.4 | Matching scenery/collision, remote door use, late join/save-load and ModActor lifetime. |
| PhotographyPlus 0.10.3 | Task/bestiary/photo submission, remote camera, personal progress, one reward and persistence. |
| unreal_shimloader 1.1.7 | Managed VFS/manual loader paths, matching PAKs/modules and no duplicate multiplayer DLL. |
| SmartTV 0.9.6 | Source/mode/play/pause/seek/join; personal device/OBS access. VEmu AV needs an explicit adapter. |
| BuyableJetpacks 1.0.0 | Store row, one delivery, equip/fuel/flight/drop, remote appearance and resource-preserving save/rejoin. |
| BuyableShovels 1.0.0 | Store row, one delivery, equip/dig/hit/drop, shared outcomes and save/rejoin. |

## Retained native research

| Evidence | Scope |
| --- | --- |
| `build/requested-events-audit/summary.json` and `reclassified-graphs.json` | 1621 relevant packages / 22691 functions / 69 scheduled rows; successful decode is not complete semantic event coverage. |
| `build/priority-level-scripts` | 263 extracted headers, 52 scripted maps and 172 functions; map adapters remain unfinished. |
| `build/backlog-20261105/native-unfinished-events.txt` | Current treeSpawner/walkingTree/blackFog/arirShipVanishing graphs and input hashes; mapping evidence only. |
| `build/report-bugs` | Original item-copy/warp images and native graph extracts used to investigate reports. |
| `build/glowstick-grab-check/verification/verification.json` | Original UG1 exact releaseGlowstick/inherited-variant proof and scoped verification; later full regression is recorded above. |

## Documentation check

Recheck: `build/omega-command-verification/prebuild/public_prose_gate.log`. 14 counters remain above baseline; other.agent is 290. TESTLOG remains below its 600-line cap (575 lines). The 267 comment-document rows, 23 uncalled declarations and 156 unswept source files remain open. This failing gate is separate from passing regression groups; its baseline has not been changed.
