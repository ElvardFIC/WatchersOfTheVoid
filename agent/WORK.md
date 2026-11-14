# WORK — unfinished implementation

BO1-BO3 build-order corrections are recorded in TESTLOG; the owner's required
tests-before-Release workflow is in README and AGENTS.md.


## Upstream continuation

- [x] M1-M10 selected upstream/fork integration: resolved the existing merge,
  removed duplicate protocol declarations, compared 69 available default heads,
  applied zero End velocities, canonical-mirror placement/cleanup recognition,
  personal-rest installation, Steam formatter include and floppy envelope/claim
  bounds. Per-item provenance and exclusions are in FORK_AUDIT_20261022.md;
  verification is in TESTLOG. Full upstream ancestry is still `4fbf1acf0`.
- [ ] Review Ghezra `74ab6e6cf` body-push admission against the existing drive
  leases before adding a publisher. Keep the native contact/jitter investigation
  open. Review BitRoit's repair-widget lifecycle and signed minigame sentinel
  independently of its fixed-delay exit/watchdog implementation.

- [x] HD01-HD25 / HX01-HX50: retained HandTaken recipient delivery, bounded
  FIFO reservations and guarded client cleanup/native inventory, refresh and grab
  release are implemented. Individual fixes and offline checks are in TESTLOG.

- [x] OC01-OC25 / OX01-OX50: Omega execution, compare-and-set service
  assignment/rollback, startKill and temporary-hand/native replay ownership are
  implemented. Individual fixes and offline verification are in TESTLOG.

- [ ] Finish exact HandTaken item/operation correlation: its current wire notice
  names only a class, so a delayed notice can still match a newer same-class item.
  Reconcile uncertain native debit/output results and remaining latent routes.
  Native multiplayer acceptance of the owned cleanup/delivery remains open.

- [x] SF01-SF25 / SM01-SM50: follow/service leases, recipient/context
  ownership, native player-result guards and insert-read schemas are implemented.
  Individual fixes and offline checks are in TESTLOG; native acceptance remains open.

- [x] KA01-KA25 / KB01-KB50: Kerfus command admission, client refusal retry,
  bounded PRE capture and guarded native action dispatch are implemented.
  Individual fixes and verification are in TESTLOG. Follow/service ownership is
  recorded above; native gameplay and remaining upstream review stay open.

- [x] KD01-KD25 / KX01-KX50: Kerfus recipient state/current-state joins,
  checkpointed mirror application and native refresh ownership are implemented.
  Individual fixes and offline verification are in TESTLOG. Native gameplay
  acceptance and the wider upstream continuation review remain open.

- [x] LS01-LS25 / LX01-LX50: independent mechanism claim leases and
  owner-specific release are implemented. Hook, carry, Kerfus, watoy and starter
  routes use canonical sources; native constraint/transform priority and contact
  acceptance remain open. Individual fixes and offline evidence are in TESTLOG.

- [x] VA01-VA25 / VX01-VX50: drive admission and host verb handoff
  continuations are implemented. Individual fixes and offline evidence are in
  TESTLOG. Independent source leases are recorded above; native contact acceptance remains open.

- [x] NH01-NH25 / NX01-NX50: ordinary NPC pose and robot-hold ownership
  guards are implemented. Individual fixes, replacement cells and offline
  verification are recorded in TESTLOG. Source lease integration is recorded above; native contact,
  materialization and the wider upstream continuation audit remain open.

## Upstream gap review — 29 October 2026

Seven remaining implementation candidates are tracked as UG2–UG8 in
[TODO.md](TODO.md#unfinished-upstreamfork-candidates--reviewed-29-october-2026).
The source limits describe upstream; local inspection below establishes the
missing adapter boundary, not a reproduced multiplayer defect. Recheck the integrated upstream and the latest fork audit before implementation.

| ID | Remaining implementation and current local boundary |
| --- | --- |
| UG2 | prop_stick_sync.cpp::ReplayStick falls back to frozen/static flags and physics-off after a missed trace. PropStickStatePayload carries identity, flags, pose and velocities, without the support identity. Add support/component resolution and destruction cleanup without attaching to a guessed surface. |
| UG3 | prop_stick_sync::ConvergeStuck and prop_wire_parity converge existing flags during other replication paths. Ghezra `70641f091` supplies a toolgun lane reference, but no equivalent host-admitted route is installed here. Add live action authority and final state delivery, preserving native freeze/static distinctions and current hold guards. |
| UG4 | The upstream laptop stabilizes its seat locally. No stabilizeSeat/occupied-chair reservation route was found in our coop controllers. Bind protection to the actual chair, operator and laptop lease; clear it on exit, body/world replacement and disconnect. Receiver-side competing-hold arbitration is already present and does not itself reserve an occupied chair. |
| UG5 | No red-phone ring adapter was found in coop controllers or protocol. Capture the native selected ring/cue and its lifetime; host ownership must not duplicate native client rolls. |
| UG6 | No desk virusEvent adapter was found. Dish precision, server state and power-panel replication cover parts of the aftermath, not the native virus trigger and phase ownership. Map the actual graph before defining its shared effects and join policy. |
| UG7 | download_arm_sync replays native begin; no objectRenderer/lifecrystal-specific birth scope was found in coop. Bind any shared signal output to one host-owned invocation across either arm/birth arrival order, including failed construction and replacement. Preserve personal camera/ending branches. |
| UG8 | Upstream #49 requests runtime 3D-print synchronization even with matching local files. No printer-specific output/content adapter was found in coop prop routes. Establish the actual printed class and mesh initialization, then add canonical birth/content/transform/lifetime and late-join handling. This source report does not establish our package's exact failure or require distributing users' files. |

Source links are attached to each TODO item. Existing event, entity, resource,
mod-integration and physics backlogs remain the owners of overlapping work;
do not duplicate them or import gameplay-only acceptance as implementation.

Development target: **0.9.2164**. [TODO.md](TODO.md) lists priorities.
This file contains implementation work and the constraints needed to finish it.
Checks, completed-change reports and release evidence belong in
[TESTLOG.md](TESTLOG.md).



## Upstream integration in progress

- [x] NJ01-NJ25 / NK01-NK50: recipient-owned NPC join rows and independent
  snapshot-completion delivery are implemented. They retain refused/unbound work,
  guard native reads by registry/actor/session/recipient ownership, and preserve
  newer brackets without replaying native creation. Individual completed boundaries
  and verification are in TESTLOG. Native materialization and remaining phase joins
  stay open.

Active full-upstream review requested on 7 November. Name work before implementation:

UI03 follow-up implemented: bound the client's sequence distance from its oldest
unanswered claim while uncertain refunds withhold acknowledgement.
UI01 follow-up implemented: retain owned client tower requests until ACK,
reserve claim capacity through POST callbacks, and detach acknowledged claims before native reap.
UI03 follow-up implemented: bind refused-pull cleanup to the captured
output actor rather than the item currently held; retain unknown cleanup receipts.
UI03 follow-up implemented: admit only one paid insert until its ACK so
client-side native spending cannot outrun the host's spend receipts behind a blocked request.
UI09 follow-up implemented: validate spawn parameter frames and retain
caller/world/actor proofs between deferred begin and finish; clean up only a proven unfinished actor.
UI06 follow-up implemented: ensure a recipient-owned NPC birth
precedes that recipient's Geom decision, retaining refused birth work and actor/class,
world/session, recipient-generation and native-pose continuation proofs. Aggregate
broadcast success must not authorize another recipient's events.
UI06 follow-up implemented: bind birth receipts and in-flight
birth reads/sends to each recipient's world-ready reset revision, including a
false/true transition entirely between game-thread pumps.

| ID | Implementation/review status |
| --- | --- |
| UI01 | Implemented tower PRE/POST actor/class/function/session proofs, nested invocation matching, abandoned-capture retirement, reserved POST capacity and owned FIFO delivery until ACK. Native ACK cleanup detaches claims before callbacks and preserves newer requests. |
| UI02 | Implemented parameter-cache incarnation checks and bounded/null-safe frame reads. |
| UI03 | Implemented committed write receipts, easy-repair confirmation, one paid insert transaction at a time until ACK, bounded sequence distance and retained refund work. Refused pulls target only a captured output; unknown cleanup receipts remain bounded and recorded. Unattempted spawns retry; uncertain attempts remain recorded and withhold acknowledgement without being repeated. Native reconciliation of uncertain outcomes remains open. |
| UI04 | Implemented per-recipient canonical cord state/join convergence, independent retry/heartbeat and world-ready handling. |
| UI05 | Implemented cord drag/head component, caller and sender continuations; nested newer poses/releases survive. Native multiplayer acceptance remains unperformed. |
| UI06 | Implemented retained host decisions, FIFO per recipient, supersession, session reconstruction guards and recipient-owned NPC birth admission before decisions. Birth refusal retries; actor/class/recipient/world-ready revision replacement cancels old receipts. Native transform snapshots retain root/function/caller ownership and commit all outputs together. Exact current-phase/audio/effect joins remain open; transport admission is not native mirror creation acknowledgement. |
| UI07 | Implemented Present/Destroy serialization through native present and protected counters. GPU quiescence remains the engine caller contract; no extra queue-idle call is made outside its queue lock. Device/context replacement recovery and complete backend-resource retirement remain open. |
| UI08 | Original jitter cause is not established. Requested exact reproduction steps and host-side displacement observations. Read-only local inspection found four older stack-overflow crash contexts, without a fresh jitter trace; no native acceptance or root-cause claim. |
| UI09 | Repaired same-pointer session/stop/role retirement in tower and Geom host/client routes, plus spawn schema/caller/world/actor continuations and owned unfinished cleanup. NJ/NK now own NPC join identities, native reads and completion receipts. NH/NX now guard ordinary NPC pose/hold continuations; LS/LX now own independent source claims; native transform/constraint arbitration and the remaining 436-commit semantic audit stay open; regression does not prove all transferred routes. |

- [x] CP01-CP25 / CQ01-CQ50: cord publication, outbox and release work now
  own their receipts across callbacks. Session/world/stop/role, sender/recipient
  generation and actor/class/endpoint proofs cancel stale continuations;
  nested successors and refused work survive. Same-world reconnect preserves
  the actor index and partial scan. Individual boundaries and offline evidence
  are in TESTLOG. UI04/UI05 implement per-recipient state delivery and native
  drag/head continuation guards; native multiplayer acceptance remains open.

- [x] EH01-EH25 / EX01-EX50: publisher and controller Ends now share
  recipient-owned delivery debt, retained after row retirement. Accepted slots
  are not resent; generation/departure, session/world, actor/class/EID and newer
  episodes cancel stale work. Retries do not repeat native release/physics.
  Invalid reserved fields are rejected before client-End retirement/release.
  Individual boundaries and offline evidence are in TESTLOG; native contacts remain open.

- [x] GD01-GD25 / GX01-GX50: grant/revoke answers retain recipient-owned
  debt, preserve newer requests and validate actor/class/EID/episode lifetime.
  Retries wait for world-ready and preserve kind/sequence through host takeback;
  death notices do not read dead actors. Individual boundaries and offline
  evidence are in TESTLOG. Native handoff/support-contact acceptance remains open.

- [x] SJ01-SJ25 / SD01-SD50: host swinger joins retain per-recipient/key
  delivery until accepted or invalidated. Retries wait for world-ready, read the
  current pose, preserve newer requests and cancel replaced session/world/
  actor/class/recipient work. Individual boundaries and offline evidence are in
  TESTLOG. Native handoff/support-contact acceptance remains open.

- [x] SG01-SG15 / SX01-SX60: actor-bound caller proofs now reach native stream,
  park/unpark and parity operations. Stored episode lifetime survives ordinary
  admission revisions; invalid sources suspend rendering while retaining park
  debt for an authoritative End. Individual boundaries and offline evidence are
  in TESTLOG. Native handoff/support-contact acceptance remains open.

- [x] HP01-HP25 / HT01-HT50: host publisher Tick/Pump now own row receipts
  across contact, pose, velocity and send callbacks. Session/world/row/root/hand
  replacement cancels old publication; newer resend/coast updates survive.
  Individual boundaries and offline evidence are in TESTLOG. Native
  handoff/support-contact acceptance remains open.

- [x] PH01-PH15 / HS01-HS60: host prop-syncer input/pass/departure snapshots,
  row/occupant receipts and detached ending continuations are implemented.
  Individual boundaries and offline evidence are in TESTLOG. End delivery is
  retained per recipient; native prop-contact acceptance is unperformed.

Review of the selected integration seams, 6 November: SI01-SI08, CI01-CI02
and VI01-VI02 are implemented; individual boundaries and test scope are in
TESTLOG. These fixes do not complete the umbrella items below.

| Item | Confirmed boundary | Concrete remaining implementation |
| --- | --- | --- |
| UP2 | The compiled message policy admits CordIntent/CoffeeActionIntent/FuseTowerState/PropSyncClaim only to host validation; CordState/PropSyncAnswer/SwingerRestPose/GeomOctaEvent are host-authored. None is blindly relayed; capacity remains 32. | Audit each native continuation against sender generation and world/session identity. Protocol classification does not validate the callbacks or transactions below, nor all 436 transferred commits. |
| UP4 | UI01-UI05 implement tower capture/function-frame proofs, committed writes/refund debt, owned client FIFO requests until ACK and captured refused-pull cleanup, plus recipient-owned cord state/join convergence and guarded native drag/head work. Earlier coffee, CP/CQ and TQ/TO guards remain exercised. | Finish reconciliation of uncertain native spawn/pull outputs and the remaining tower timeline/effect review. Unknown receipts remain recorded without guessing a target or replaying native work; native multiplayer acceptance remains unperformed. |
| UP5 | Swinger join applies owned actor/class/world receipts; failed velocity zeroing retains debt and timeouts work without an advancing index. Park supports retain 32 rider leases. Client PC01-PC25 / PS01-PS50, host controller PH01-PH15 / HS01-HS60 and publisher HP01-HP25 / HT01-HT50 own their continuations. SG01-SG15 / SX01-SX60 propagate caller proof through native application and retain episode/park ownership. SJ01-SJ25 / SD01-SD50 retain guarded host join delivery; GD01-GD25 / GX01-GX50 retain grant/revoke recipient debt; EH01-EH25 / EX01-EX50 retain End delivery across publisher/controller retirement. | Finish native handoff/support-contact ownership review and the original prop-jitter investigation. Trace ordinary hand-drive/support contacts in the original jitter report; offline continuation checks do not establish its root cause. |
| UP6 | UI06 retains host decisions per recipient with FIFO, supersession, target/class/session proofs, pacing and ordered recipient-owned NPC birth admission. UI07 keeps native Present/Destroy serialized through native present, preserving the engine caller's GPU-quiescence contract. Earlier G01-G11 and VI01-VI02 remain exercised. | Finish exact Ramiel current-phase/audio/effect joins and native mirror creation acceptance. Finish full Vulkan device/context replacement recovery and complete backend-resource retirement; native beam/attack/graphics acceptance remains unperformed. |

- [x] Native tower wrapper continuation: CW01-CW37 / RW01-RW38 implemented.
  The individual boundaries and offline checks are in TESTLOG. Reconciliation
  of uncertain operation/debit/output receipts, lower-level
  timeline schemas and gameplay remain open.

- [x] TQ01-TQ20 / TO01-TO55: own tower operation/overflow receipts and
  spend reservations; cancel native continuations after world/session/sender/
  tower/class replacement. Individual boundaries and offline evidence are
  recorded in TESTLOG. UI01-UI03 implement client PRE/POST capture, committed
  output and retained refund debt. Uncertain native output reconciliation remains open.

- [ ] UP2: finish semantic review of source integration through upstream
  `4fbf1acf0` (436 graph commits after `ad5315611`; 130 after the previous
  `dc4cd7307` source integration). The code is transferred and builds; the
  review still owns authority and lifetime seams beyond the exercised fixtures.
- [ ] UP4: finish ownership review for merged cord/coffee and coordinate/radio
  tower adapters against existing fork routes and 32-player capacity.
- [ ] UP5: finish prop syncer/swinger handoff review, including parked supports,
  bind-wait replacement, join ordering, hand arbitration and container recovery.

- [x] PC01-PC25 / PS01-PS50: own client prop-syncer episodes, guarded pose/End
  capture and retained End delivery. Native callbacks cannot overwrite a
  successor episode; root/hand and session/world replacements cancel old
  publication. Individual boundaries and offline evidence are in TESTLOG.

- [ ] UP6: finish remaining Geom Octa phases/delivery and Vulkan teardown across
  device, window and world replacement.

UP1 documentation conflict resolution and UP7 fixture/build verification are
recorded in [TESTLOG.md](TESTLOG.md#integration-error-audit-30-october-2026).
Earlier joint b2220 source, including the swinger/cord and Vulkan context fixes, passed
142 offline groups across the full run and scoped recheck, the stable-source
guard and Release/ABI inspection; see TESTLOG.
UI01-UI09 changes follow that build and have separate evidence in TESTLOG;
the earlier DLL/ABI inspection does not qualify them. This is development
verification, not a newly qualified release package.
The published fork IDs remain reserved; the renamed HitIntent keeps ID 242,
and the new container request/reply/commit use 264/265/266. The PvP throw nonce,
Kerfur release marker, food state and placement request IDs remain separate.
UP8 history integration is recorded in TESTLOG under the real upstream merge.
It resolves the 436-commit ancestry gap; the semantic/gameplay reviews above
remain open independently of Git ancestry.
The completed 75 queue/coffee boundaries are listed in TESTLOG's queue and coffee
audit. UP4 still owns cord/tower implementation review and native coffee gameplay.
Radio R01-R45/M01-M30 and tower T01-T45/K01-K30 are completed in TESTLOG.
UP4 still owns uncertain native output reconciliation, remaining lower-level
timeline schemas and native acceptance. Tower wrapper CW01-CW37 / RW01-RW38
are recorded in TESTLOG.

## Fork follow-ups — reviewed 4 November 2026

- [x] FC01-FC75: own floppy-claim, client sweep and laptop-settled continuations
  through world, session, actor/class and peer replacement. Individual boundaries
  and 75 passing cases are recorded in TESTLOG. Retain newer canonical answers,
  nested requests and corrections after uncertain native writes.

- [ ] Own concurrent disc inserts with exact device occupancy, sender generation,
  admitted eject/output completion and bounded retry/refund state. Ghezra
  `1bc4675d1` demonstrates the race; its fixed-delay, unbounded swap queue is not
  sufficient. Preserve canonical ordering and checked writes. FC01-FC75 cover
  continuation ownership; occupied-disc arbitration, debit/output and refunds
  remain unfinished.
- [ ] Add host-admitted module purchases separately from level upgrades,
  including exact row, reach, debit/output and duplicate/refusal behavior.
  Ghezra `13b120e67` is a reference; the current module branch still runs locally.
- [ ] Establish personal/shared provenance for transient spawner drops at join.
  Ghezra `10b3a8c8e` is a reference; lifespan alone must not exempt shared props,
  and the incomplete-snapshot valve must remain effective.

Other new Ghezra entity/resource/event candidates remain with their owners below.
The full 41-commit disposition is in [the fork audit](FORK_AUDIT_20261022.md).

## Client prop physics

- [x] NP01-NP45 / CR01-CR30: native physics frame/lifetime checks and deferred
  child-release ownership implemented. Per-item results are in TESTLOG. A surviving
  unheld child now unparks when its parent retires; failed pose writes retain debt.

- [ ] Fix joined-client object vibration, ejection and support penetration on
  boxes, desks and workbenches while host physics appears stable. Trace ordinary
  prop pose application, drive/park/end handoffs and simulation reactivation;
  resolve conflicting simulation/transform ownership and preserve stable resting
  contacts, collisions, pickup/drop and canonical host motion. The report does
  not yet establish the responsible route or whether client displacement reaches
  the host. Current config exposes no dedicated user-facing stack-physics fix;
  do not substitute arbitrary network tuning for a confirmed implementation.
  PD1-PD10 now protect prop-drive pose/end/tick callbacks, host end generations,
  component replacement and hand transitions; TESTLOG records their offline proof.
  NP01-NP45 / CR01-CR30 cover native physics frames/component discovery and
  deferred child releases. Ordinary hand-drive, parked supports and host/client
  publisher ownership still need review; rendered stack acceptance remains open.

## Player presentation and toys

- [ ] Finish remote flashlight presentation beyond the imported body-shadow fix
  from Ghezra `97f1f6072`. Continue from
  the existing retained state, replacement replay and same-component reconciliation;
  keep sender/slot ownership and current pawn/light bindings through native calls.
- [ ] Fix the falling-player T-pose. Resolve selected-model pose transfer and
  physics takeover in engine_ragdoll_skin/engine_playerragdoll and the remote
  display; preserve death-free puppet spawning and the standing warmup bridge.
- [ ] Finish held-toy variant schemas and hook/function ownership beyond the
  Dingus/ABPlush capture inputs. Keep PRE/POST provenance and bounded native work.
- [ ] Finish watoy start-cue and physics-hook ownership. Preserve the existing
  activation/audio/emitter schemas, original-hand provenance, shared View ownership
  and presentation budgets. Never replay an uncertain admitted activation.
- [ ] Finish thrown-sponge foam and resource ownership beyond window dabs;
  preserve the original draw's material/size and at-most-once native cleaning.
- [ ] Fix remaining stow stalls beyond indexed save-record class restoration;
  retain pending item-state restoration while classes are unavailable.
- [ ] Fill missing model previews and custom-skin compatibility while retaining
  rig exclusions. Treat skeletal scaling and creature physics as separate adapters.

## Event adapters and generator effects

- [ ] Finish Dreams scene-effect/lifetime adapters while keeping sleep,
  createDream/leaveDream, wake rewards, personal nightmare rolls and Insomniac
  consequences with their owner. Do not synthesize shared reward props.
- [ ] Finish Ariral Reputation state, selected outcomes, relocation and late join.
  Use calcRep's -75/-25/25/75 tiers and the actual tier/mainGamemode listeners;
  launchEvent itself is empty.
- [ ] Implement remaining Player-Activated trigger/outcome/effect routes with
  canonical authority for simultaneous activations.
- [ ] Finish Time-Related routes: 3AM eligibility and mainGamemode calendar
  branches outside eventer, including June 28 21:00, April 1 and November 29 22:49.
- [ ] Separate personal/session consequences for each Ending and implement its
  adapter; do not replay quit/travel on remote puppets.
- [ ] Finish Story Mode actor, phase, audio, bookkeeping and current-state joins.
  Horde/dropper need wave/class budgets, alive sets, countdowns and timeline
  ownership. Keep health/effects personal where native; avoid repeated rewards
  or save work from replaying a start.
- [ ] Finish Random event effects, including shared black fog beyond client
  producer suppression. Keep bp7Spawner's camera/roll/pause/quit owner-local;
  its ten-second eligible-roll scaling is already implemented.
  blackFog needs separate fade/hold state: its graph uses `a` for the fade and
  the 180-second hold. `set()` also controls birber.noBirb and ambience volumes;
  reverb/event accounting, the 40-second ghost timer (except mode 6), owner-camera
  whispers and the five-second thump loop need distinct ownership/cleanup.
- [ ] Map Signal processing/playback triggers, funny-rule gates and selected
  outcomes, then implement phases/effects and current-state joins.
- [ ] Implement level/map-script routes from the extracted headers. Use
  build/priority-level-scripts (263 headers, 52 scripted maps, 172 functions)
  and build/requested-events-audit; preserve personal and protection/exit branches.
- [ ] treeSpawner: transport the host-selected walking tree's visibility, mesh
  scale, movement and lifetime; finish client producer suppression and late join.
  Then scale only the native 600–900-second spawn wait, preserving mode,
  rendering and proximity gates.
  Native ReceiveTick enters the spawner graph at 742 (modes 0/1); the wait is
  selected at 10 and written at 48. walkingTree Begin at 1473 selects component
  mesh scale 1–2 and hides it. OnLanded enters at 1418, then reaches reveal at
  1251/1246. Preserve its separate 1/.25/3/60-second latent work, move-proxy
  delegates and render/distance/time-dilation conditions when suppressing mirrors.
- [ ] serverBreaker: finish protection/repair audio and delegate consequences.
  Protected-floppy canonical delivery is covered by SB1-SB5 below. Retain the
  selected box, native day/difficulty/upgrade probabilities, slot-canonical ordering
  and retry ownership. Then scale only the native 1200–600-second eligible wait.
- [x] SB1: deliver floppy-slot canonicals to every connected, world-ready client.
- [x] SB2: retain refused canonical delivery until a later sweep succeeds.
- [x] SB3: bind publication completion to the captured bytes and digest.
- [x] SB4: cancel host publication/join continuations after world, session,
  actor/class or recipient-generation replacement; retain nested requests.
- [x] SB5: retain join delivery across transport refusals with paced retries,
  recipient ownership and disconnect cleanup. Verification is in TESTLOG.
- [ ] Finish shared sky-event phases/cues, audio, delayed email/accounting,
  six pre-session latent continuations and remaining custom effects/creatures.
- [ ] Finish arirShipVanishing_C and other appearance/event variants beyond the
  arirShipAppear_C phase adapter; include shared reputation consequences.
  Vanishing uses a separate `vanishTimeline` alpha and `vanishradius` overlap
  query; begin/end overlaps delay checkVanish by 1/5 seconds. Its initial
  one-second physics pulse and zero-delay continuation must not re-enable
  simulation under a mirror park. It cannot reuse the Appear actor's schema.

Native event sites above were redecoded from the current local asset corpus on
5 November. Function bodies and input hashes are recorded in
`build/backlog-20261105/native-unfinished-events.txt`; this is mapping evidence,
not implementation or gameplay acceptance.

Frequency changes must preserve eligibility, probabilities, personal dream rolls
and story dates. Scale eligible-roll waits without changing AI/animation cadence,
effect durations or sleep needs.

## Console, items and resource transactions

- [ ] Finish Rufus/thiccfus cross-player targets/grabs, audio/HUD, corpse/loot,
  damage and explosion transport; Madness/variant AI, damage/effects and remaining
  funny-rule HUD/events and toy/squeeze sounds.
- [ ] Finish llama marker/save/event-accounting lifecycle beyond the existing
  PlacedSave marker route and invocation-bound Begin recovery. Keep native host
  contributions authoritative; do not infer latent age from an absent sender or
  replay uncertain timer creation. LM1-LM4 now retain refused failed-marker
  cleanup on the frame tail, including outside a session; see TESTLOG.
- [ ] Finish atomic paid wall/cement repair request/result and exact personal
  stock across hand/slot/drop, refusal and disconnect. Keep the current no-spend
  client refusal until debit, accepted output and refund form one transaction.
- [ ] Arbitrate concurrent water/cement/coal writers, stale in-flight records,
  dryTimer ownership and personal-hand transitions; finish output/accounting.
- [ ] Finish cross-peer personal debit/output/refund, personal extraction
  overrides/raw takeObj, receipt-less recovery and debt persistence across
  world/session reconstruction. Retain the local base-container escrow/refund
  boundary; never guess a refund or replay an uncertain admitted native effect.
- [ ] Finish remaining package/food subclass outputs, cross-peer portion/output
  arbitration and personal effects before host acceptance. Add native poison-pour,
  roach and reward routes plus uncertain-open refund recovery and persistence.
  Do not admit arbitrary replaceProp. Admission alone is not a committed output.
- [ ] Finish buried-item/treasure transaction authority and save-file persistence;
  mirror construction must not replay native treasure/loot-producing BeginPlay.
- [ ] Cover remaining dynamic grime sources and persist explosion/salt-rocket
  scorch provenance through full save/world reconstruction.

## Entity implementation

- [x] G01-G11 / R01-R40 / P01-P24: Ramiel decision ownership, roach snapshot/
  consumption reconciliation and Great Pyramid replay/pose lifetimes implemented.
  Individual boundaries: [test coverage](tests/README.md#entity-adapter-boundaries-6-november-2026).
  Offline checks are in TESTLOG; remaining behavior/effects below stay open.

- [ ] Grayboar: complete join/audio/time coordination, host targets, turn/pitch,
  helper/delegate retirement, held-item handoffs, weapon/collision damage, one
  corpse/loot, lightning replacement and charged explosion/punch/spin effects.
- [ ] Rozitals: complete Great Pyramid, Kavotia, Pagonioplo-Igetis, Scouts and
  Soltomia action/effect adapters. Kavotia needs fire/laying montage, speech,
  canonical beam/impact/damage and exact phase timing.
- [ ] Antibreather / Fossilhound: complete player/cave-dependent Antibreather
  spawning, cross-peer attacks and Fossilhound ticker/attack routes.
- [ ] Arirals: complete follower/shooter/pigBeater and gun-stealer behavior,
  stolen/held/thrown-item ownership, attachments, shots/hits and event phases.
  Slapper Character classification and exact summoner spawn admission are
  implemented; its attacks, item handoffs and effects still need adapters.
- [ ] Greys / Flamethrower Tank: complete grayTest targets/actions/effects and
  firetank turret, fire, fuel, damage and destruction.
- [ ] Geoms: complete Ramiel (geomOcta_C) movement/charge/beam/audio/damage and
  Egg-shaped Geom (eg_C) retrieval/landing/special actions.
- [ ] UFOs: complete child spawns, beam/material/audio phases and join state for
  Boomerang, Joil, Grey Ship, Midas, Pill, Trifo and Orbs; resolve Grey Ship names.
- [ ] Deer / Fish / Cockroach: complete deer/deer_ded spawn/death/rejoin,
  caught prop_fish_0..21_C inventory and roach movement/eating/crushing/join state.
- [ ] Blue Brain Cheese, Restocker, Eye, Wolfgang, Tree with Eyes, Living Mushroom,
  Space Jellyfish: complete exact behavior/effect routes. Wolfgang derives from
  actor_save; do not map every eyes entity to eyer.
- [ ] Bone Man, Bloody Skeleton, Curious Eyes, Mimic, Flesh Slug, Zombie Deer:
  complete attacks/effects/lifetime, missing name mappings, shared mimic
  rendering/disguise/damage and centipede visibility.
- [ ] Ghost, Ghost Log, Fireflies, Walking Mannequin, Black Spot: complete missing
  names/effects/phases. Finish remaining wisp limb/blood presentation beyond the
  implemented animation-readiness and body/world-bound grab/release cancellation.
- [ ] Demons: complete individual targets, rituals/run-end behavior for SKerfur-O,
  Eyeless Insomniac, Insomnia Demon, Abandoned Kerfur, Flesh Erie, Goat Statue,
  Stolas Statue and Furfur. Keep live/zombie/angry Flesh Erie classes distinct.
- [ ] Forbidden: complete missing class mappings, spawn availability and local/global
  effects for Black Friend, CorpseWalker, Crasher, Fast-shadow Figure, Fridge,
  Interloper, The Exit Door, The Head, Pool Entity, Mountain People, Cursed Spider,
  Rune Sphere, Shadow Figure, Fake Kel and Black Sphere. Keep debug actors out of normal play.
- [ ] Living Erie / Girufus / Rufus: complete Girufus mapping and distinct
  interaction, animation, damage, ownership and lifetime behavior.
- [ ] Gnomes / Krampus: complete group spawning, presents, revenge, interaction,
  damage/death/shared lifetime and Krampus attack/fireball/jump/target/despawn.

## Requested mod integrations

- [ ] EmoteWheel 1.0.1: native multiplayer acceptance of the implemented optional
  phase/variant/music/hand adapter. Include mixed installations and live census;
  implementation and offline boundaries are recorded above and in TESTLOG.
- [ ] VEmu 1.1.0: define shared console/cartridge/AV/input ownership and emulator
  state/audio/video transport. Upstream AV output excludes SmartTV; an explicit
  adapter is required to support that combination.
- [ ] JeepMod 0.11.1: finish custom trunk/key/container/upgrade routes beyond
  inherited ATV pose/seat admission; preserve one shared inventory and lifetime.
- [ ] PhotographyPlus 0.10.3: implement task/reward/save ownership and photo
  submission routing without duplicating host rewards or another player's photos.
- [ ] SmartTV 0.9.6: implement custom player mode/source/playback/join ownership
  beyond vanilla media routes; keep local device/OBS access local.
- [ ] Audit FireTower 1.0.4 door/ModActor ownership and add any missing shared
  interaction/lifetime routes; retain matching local scenery and collision.

## Tooling and regression maintenance

- [ ] Reduce public-prose findings in documents, src.comment_doc_row and
  src.files_not_swept; restore required reading-room references. Existing gate
  findings and evidence belong in TESTLOG.
