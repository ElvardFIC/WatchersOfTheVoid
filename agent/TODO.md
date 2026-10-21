# TODO - current priorities

Source of truth: **ElvardFIC/WatchersOfTheVoid main**, fetched baseline
`b6651cb91d4a7ee5ddc9c92fc558d3e3913b3b00`. Historical version numbers below
do not establish which implementation is newest. Game target remains 0.9.0n.

Owner's scope, 20 October 2026: work through the **non-entity** backlog;
the other owner handles entities. No entity implementation is assigned here.
Current source handoff is on `watchers-of-the-void-fish`; the new placement wire
layout, material/cooking extensions, host-ordered console display, Watoy and sponge
effects use protocol 2175, including grill world/hand presentation, persistent burn
state and instant wall completion requests. Older implementations cannot silently interoperate.
Existing delivery packages are historical; current peers must use matching DLLs.

Continuation point for the same non-entity scope: the instant-wall, grill presentation,
hand-tool lifetime and kitchen queue work is implemented and qualified offline in2162.
Previous2163 adds grill hand/stock/effect replication, saved burning state and host
burn protection from ordinary client records. It also carries full forced personal
profiles (current vitals and held state), guarded chunk cancellation and nested
capture/send ordering. Focused tests pass; final Release qualification is recorded below.
Hand toy packets also carry playerNo, retire stale/disappeared mirrors and keep
current deliveries across replacement/teardown. Sponge foam and Madness HUD/audio
now fence native callbacks, queue sends and recipient/source changes. All remain
subject to two-peer gameplay verification.
Current2164 implements the audited bucket-pour route: exact owned-bucket birth
admission, post-native velocity/soap capture, host collision/latent authority and
separate splash/sound delivery. The host also streams native water motion. Three
actual production harnesses and native contracts pass; two-peer timing is unverified.
Faucet/detach requests now retain the originating person, wait for host readiness,
preserve per-key order and cancel interrupted chunk sends. Garden regressions pass.
Grime joins and final cleaning updates now retain failed sends. Partial material
paints retry with current floors; the native wrapper follows class reloads and
guards component/native callbacks. General prop records also cancel obsolete
same-key capture/normalization/apply/send work and interrupted chunk transfers.
Current2165 adds host-ordered coal/lid field edits, terminal replies and host stock
preservation against ordinary/parked full records. Focused tests pass; complete
Release/hash qualification is now2167; see the current verification below.
Current2166 adds actual pressure-washer projectile birth/flight, host collision and
retirement, launch sound and initial/impact presentation. It also closes the parent's
independent client cleaning path for both exact water projectiles and preserves
the final spawn transform. Focused water and hand-tool checks pass. Two-peer play,
shared dynamic grime/spill births remain open.
Current2167 adds exact bucket drip/whole-throw launch cues, host validation, owner
exclusion and short retained delivery. Mixed bucket/impact cues preserve recipient
and local presentation order. Nested grime clean/fix calls have independent,
invocation-bound snapshots, newer-minimum repaint and Cancel/native-fault cleanup.
Grill observations and window wipes now use the same invocation lifetime; window
disconnect drops the session pointer. Failed live window updates now retry with
the current minimum and protect newer queued wipes during send. Retained window
join delivery and native wrapper reload/paint guards still require work. New audio/
emitter hooks also need live fingerprint references and two-peer installation checks.
Current2168 adds retained window joins, serial/index/native-metadata reload guards
and retryable partial paints. Exact beer/wine spills now have host-validated births,
independent cleaning identities, owner prediction replacement, quiet saved-actor
adoption/join and retained retirement. Host break/fire deposits are observed too.
Other grime factories, projectile/electrical authority and atomic liquid stock stay
open. Food world/hand publication, native writes, join retries and final-bite
invocation/retry lifetimes now fence superseding callbacks and changed occupants.
World toy delivery also protects same-person rejoin/role changes and nested music
captures; exact music phase remains open. Offline evidence is in WORK/TESTLOG;
two-peer gameplay remains required.
Current2169 moves exact beer/wine launches to host-native flight and collision,
including native electrical outcomes. Client temporary spheres freeze immediately;
final-use source proofs survive hand consumption. Canonical impact sound now also
reaches the owner. Other grime factories, pour audio and atomic stock accounting
remain open. Paid garden reservations bind unique invocations and retain their
original player/slot/connection through read and transport. Resource rejection/
refund and two-peer timing are still open. Offline Release and regression
qualification is recorded below; no gameplay acceptance row is closed by it.
Current2170 adds exact coffee-cup launch/decal handling, preserving inventory-owner
and host-world food authority. Native source pour sound has a separate validated
channel with exact caller-point capture, independent FIFO tracks and no join replay.
Canonical spatial cues retain the original person's replay history after departure;
optional audio metadata does not block flight. Other grime, accounting and gameplay
remain open.
Current2171 carries the captured player number in care/harvest request bytes.
The host rejects obsolete occupants, zero/out-of-range people and legacy layouts;
native authority probes also retain session/world/role/readiness and peer epoch.
This does not implement stock rejection/refund or close the garden gameplay row.
Current2172 adds host-native gasoline-bottle break spills, canonical gasoline
decal identities, cleaning/join/retirement and native flame presentation. Client
copies cannot burn down the decal, spread world fire or decide native expiry;
player-local flame traces remain native. Fire intents now carry the originating
player and fence connection/role/readiness, newer desired states and interrupted
native fields/sends. Full Release and offline regression qualification passes;
two-peer fire/save/cleaning timing remains open.
Current2173 positions observed looping toy playback on late join/recovery, using
native duration/pitch, queue age and half-RTT. Unknown pre-observation/cue phase
stays unknown; heartbeats do not restart the same episode. Toy requests and native
actions bind their original person/invocation/connection. Offline qualification
passes; two-peer timing and hand/world phase continuity stay open.
Current2174 adds exact selected-hand gasoline launch capture, current/retired class
proof and host flight, while refusing mirror factories and unbound client projectiles.
Native fire attempts cannot replay partial additive fuel; final extinguishing state
retries blocked recipients. Plush Alt input binds the pre-activation snapshot.
Toy sampling, cached playback, action/touch captures and queued squeaks retain the
canonical actor key across native callbacks and refuse a renamed source.
Client music commands retain their queue until the host is world-ready.
Current2175 adds canonical host explosion/salt-rocket surface scorch births,
independent cleaning identities, quiet late join/adoption and retained retirement.
Client surface traces are refused. The inherited private-PC explosion stays local;
observed private decals are excluded. Known provenance survives same-world reconnect,
but full save/world reload loses it; unknown host scorches retain static cleaning.
Other grime factories and two-peer checks remain open.
Grill accepted replies now survive transport backpressure past their retention
deadline, and publication retains the original actor key through native callbacks.
Those regressions pass; personal inventory debit/output/refund is still open.
Next address personal coal/lid spending/output acceptance and refund, and ordinary
wall repair with an exact personal cement request/result.
The current no-spend refusal stays until that accounting is complete. Garden care
rejection/refund and two-peer hand/drop/save/rejoin checks remain open as well.

## Entity category request - OPEN

The owner's complete taxonomy, native class mappings and unresolved behaviors are
in [WORK.md](WORK.md), "Complete requested entity coverage and remaining work".
Do not close a creature from allowlist membership or transform replication alone.

Current grayboar work: host physical-body movement feed, owner-directed bite damage,
foreign-health protection, stable adopted-replica authority across drive handoffs,
and native AnimBP locomotion/movement-loop audio input from interpolated velocity.
Host native sensePawn scans Pawn candidates; actual multiplayer target detection
still needs testing. No generic client AI or gameplay-verb replay was added.
Client host-replica bite-overlap and attack-notify bodies are now refused, covering
the local attack impulse and arbitrary-object damage as well as player health.
Live attack/roar/death and four nonlethal hit reactions are implemented and await two-player
verification. Host AnimBP turn now drives client bone steering and turn-loop pitch
with a 100 ms interpolation. Exact time alignment and helper coordination remain open.
Mid-action join now samples the native active montage position/rate and resumes
that animation with local queue-age compensation. It does not restart one-shot
audio, or compensate the unsynchronized host/client clocks/network transit.
Hit reactions use the host's active damage1..4 section after a verified health
decrease with skipAnim=false; clients play the native damage cue without health
writes. Mid-hit joins resume the selected section silently.
Host replicas also refuse native sensePawn target assignment and detected roar/
target-delegate binding on clients. Active helper paths from before save adoption
are now stopped on the verified owned AI controller. The old target-destroyed
delegate was audited: it only unbinds/clears the target and remains native.
Two-player task lifecycle verification remains open; this does not establish shared AI.
Client host replicas now also refuse native damaged, death loot-notify and death
completion bodies. Host death cues use the native half animation rate and attached
death sound; mid-death join resumes only the currently playing death animation.
Corpse/loot counts and native host retirement still need two-player verification.
Client host replicas now also refuse the separate collision impulse/10-damage
body, lightning replacement and charged-form explosion/death timer/punch/spin
bodies. Charged overrides are watched on their actual declaring class. Native
host outcomes, replacement propagation and charged effects still need gameplay checks.

Next implementation:
Client boar navigation now refuses move/updMov/path-check/retry/finish bodies and
the verified AI continuations 15/543/558/966/1016/1381. senseUpdate/saw/setWalking
cannot restart client helper decisions or mode changes. Each client body tick checks
the owned helper's current path and aborts Waiting/Paused/Moving navigation,
including late possession and adopted/held copies. Cosmetic turning now follows
the host AnimBP scalar; its visible result and native task/delegate lifecycle
still need two-player validation. This is not full helper synchronization.
1. Grayboar remaining join/audio/timing work and client helper/turning coordination.
   Live attack/roar/death/hit cosmetic cues are implemented;
   two-player playback and timing still need verification. Preserve native host targeting,
   damage ownership and corpse/loot authority; do not replay loot/damage as cosmetics.
2. Host/client tests for spawn, target, movement, action, damage, death, reconnect
   and mid-event join. Personal Insomniac/nightmare conditions remain peer-local.


## Other open priorities

- Run the ten priority gameplay checks in WORK.md on matching peer builds.
- Ragdoll selection, entry/recovery, local body and limb streaming now have code and
  focused regressions. Verify every selected rig in a two-peer game.
- Shared computer UI state, operator visibility and selected-model sleeping bodies
  are implemented. Verify laptop/portable-PC, bed, faint/ragdoll and chair changes
  between two matching peers.
- Bench58: record exact item/authoring peer and logs for simple-placement loss;
  birth/save retry code does not prove the original symptom resolved.
- Token52: identify the exact counter/UI before changing balance code.
- Wall21/22: atomic cement request/result accounting across hand/slot/drop/retry/
  disconnect; preserve the existing no-spend refusal. Wall41 remains excluded.
- Full backlog and report status: [WORK.md](WORK.md), [TESTLOG.md](TESTLOG.md).

## Current verification and boundaries

Current non-entity development **2174** has a fresh MSVC Release build and25
focused runners with86 positive executable invocations and112 standalone source
compiles. Native asset contracts,223-kind routing/all18 rejection controls,201
compiled mutation controls and DLL ABI/forbidden-import checks pass. Control
manifests verify that the compiled generated sources/fixtures match their recorded
hashes. All1902 production/config hashes are stable through the final build.
Current DLL and source hashes are in build/final-nonentity-verification.json.
Two-peer gameplay remains unverified. The101-file selected prose sweep passes with
zero findings after correcting include context. Four include tests and the329-arm
existing prose drill pass. Repository prose and fact-row gates retain findings,
including missing captured native-hook references. No baseline was relaxed.
Forty-six corpus files match the installed-pak extracts exactly.
These checks do not qualify the unmodified entity implementation.

The package results below are historical **b2153** evidence, not validation of
the current development source or DLL.

package/check_2152.ps1 passes all 9 runners, including expanded entity/boar tests.
Full MSVC Release DLL resources identify **0.9.0n b2153**. Hash and evidence:
build/release-2152-logs/verification.json and entity-validation.json.
Release scripts explicitly distinguish package 2152 from DLL/protocol 2153;
workspace dist/ contains the 2152 ZIP and SHA256SUMS-2152.txt.
Offline regressions do not establish two-player gameplay correctness.
Live grayboar cues use host-only reliable kind 200, cosmetic turn state uses 201
within development 2153. Install
matching current DLLs on both peers; the existing delivery ZIP predates this work.

The historical merge is already in main. Current non-entity source/test changes
remain uncommitted. Preserve all work. Commit, push, game
installation and publication require an explicit request. No other-agent workflow.
Update TODO/WORK/TESTLOG in place; detailed prior evidence remains in WORK/TESTLOG.

Fork review: see WORK.md, "Fork and upstream review". Development 2153 now includes exact disk text, rack canonical ownership/join wait and retryable trash classification. Rebuild is local; delivery ZIP predates these fixes. Test client hashcodes and filled racks after save/rejoin.

Cleanup completed: run_2141 uses the current local toolchain and current cord/session
fixture declarations. Both performance tests and all five deliberate regression
controls pass against the current sources; no archived implementation is required.

## Current item batch follow-up

The eight requested item areas have source fixes and offline regressions in continued
2153; see WORK.md, Item synchronization batch. Validate all eight on matching current
DLLs: grabs/collision push, both gun effects and contested control, freeze/root scale,
struck panels, crowbar replacement/parents, crate/MRE/foodBox loot, food final bite and
local poison, battery hand/stow/drop/rejoin. Skeletal bone scaling and arbitrary
creature physics remain outside the root-prop lane. Reliable kind 202 is added within
development 2153; existing delivery ZIP does not contain this work.

## Remaining implementation and investigations from WORK

Reviewed again against all sections of WORK.md on 20 October 2026. Newer implementation reports supersede
older "needs implementation" notes for sleeping bodies, shared UI, ragdolls and
food final-bite delivery. Those features still require gameplay verification.
Original report IDs below preserve links to the complete tracker.

- [x] Rebuild current main and the non-entity batch; compile production sources,
  verify ABI, reliable-kind routing and Release identity. New non-entity and
  seven garden/hose suites pass. Historical nine-runner release qualification
  and gameplay smoke remain open; existing packages have not been replaced.
- [x] Restore the merged network admission table: 27 existing client request
  kinds now reach their validating handlers; canonical host-only kinds stay
  refused at the client edge. All 214 current kinds are covered; 18 negative controls pass.
- [x] Placement: numbered request, host acceptance/refusal/retry, retained
  backpressure replies and replay protection. A refused local item stays
  recoverable by pickup and cannot enter a shared fallback birth. Test in game.
- [x] Coins: E/overlap/request share a host reservation before native execution;
  failed completion cannot double-credit and accepted cleanup is retried.
  No client mirror is removed early; map-placed coins stay native. Test in game.
- [x] Material save/sync: wet walls retain dryTime/timeToDry; concrete buckets retain
  dryTimer; sponges retain soapAmount. Native getData/loadData, hand-profile copies
  and existing network carriers preserve the added fields. Wet walls are polled
  by the host; timer-only and soap-only changes publish. Test handoff/save/rejoin in game.
- [x] Black-hole event: a client refuses tp and all six native Delay continuations,
  including chains started before joining. Audio, delayed emails and event state
  run on the host; normal sky/moon/initialization paths remain native. Test in game.
- [ ] Console/events (12–15, 26, 42), non-entity scope: simultaneous submitted
  output, periodic alerts and clr now use one host-ordered display stream, including
  the origin and late-join history. Focused offline checks pass; verify in game.
  Madness now compensates host queue age plus estimated half-RTT for new playback,
  and retries stopped/destroyed audio and HUD components. Exact music alignment
  remains unverified. Remaining: other funny-rule HUD/audio. Audit other
  toy/squeeze sounds. Watoy world activation, host motion and sound/particle state,
  plus owner hand presentation and late join, now have code and focused offline
  checks; gameplay, active pickup/stow/drop and native save behavior remain checks.
  Rufus/thiccfus, llama and creature variants/AI/damage belong
  to the other owner's entity work.
- [ ] Cement/walls (21/22): complete atomic material accounting, creative
  construction and exact impact/effects. Wet-wall timer persistence is implemented.
  The native wallbuilder instant-completion path now has host validation and
  request/reply retry in 2162. Only a new contour spawned for that client/person
  qualifies; the host checks Custom Content, source hand and reach. Repeated requests
  cannot create a second successor. Offline controller/wrapper/placement checks pass;
  two-peer completion/effects still need verification. Atomic paid repairs remain open.
  Verify held
  trowel key continuity: a mismatch currently defers the entire profile poll;
  coordinate native hand reconstruction, unequip/drop and persistent cement stock.
- [ ] Water/cement/coal (20/21/44/45): arbitrate concurrent writers and stale
  in-flight records, dryTimer ownership and personal-hand transitions. Complete
  sponge foam/surface effects and grill lid/final cooked-food behavior.
  Cement timer and sponge soap stock now survive native records and held profiles;
  this does not establish atomic authority handoff or concurrent-use correctness.
  Sponge surface foam and attached bursts now have a host-validated cosmetic
  channel, with no cleaning or soap replay on recipients. Offline checks pass;
  real appearance/timing, other shared spill sources and atomic stock remain open.
  Current2170 covers exact beer/wine/coffee launch, host-native flight/electrical
  outcomes, static decals, saved-actor adoption and native pour audio. Coffee in
  the inventory follows its owner; world coffee remains host-owned even in a
  remote physical hold. Other grime factories, atomic stock and gameplay stay open.
  Bucket pouring now has its own exact owned-bucket birth route in2164, with
  post-native velocity/soap capture, host collision/latent gameplay and separate
  native wade/splash presentation. Offline controller/wrapper/coasting/native
  checks pass. Two-peer delay/impact appearance and launch cues remain unverified;
  washer projectile authority/presentation is implemented in2166; atomic water
  accounting/refund and gameplay acceptance remain open.
  Held tool display packets now include player identity. Lost extinguisher stop
  commands expire visually, old-tool effects retire at replacement/stow/departure,
  missing reflection fields retry and actual stopped audio can restart. Focused
  offline checks pass in development 2161; two-peer foam and stock durability remain.
  Grill lid and actual cooking-loop state/volume now have a canonical presentation
  path and retained join delivery in development 2161. The native lid-output graph
  has exact local-action provenance. Current2163 also has held grill presentation
  and burn persistence; ordinary client records preserve host burn/active. Concurrent
  coal/lid transactions, full stock-record conflicts and personal item durability remain open.
- [ ] Garden (46): audit merged care/hose routing against pre-session plant/faucet/
  hose enrollment, watering/fertilizing/harvesting, pot changes, accepted rerouting
  and simultaneous use. Current main already enrolls saved plants/faucets/hoses;
  seven offline suites passed on 20 October. Care acceptance/refund, personal
  stock durability and real save/rejoin/concurrent use still need work.
  Do not assume missing historical save actors are migrated.
  Faucet/detach requests now wait for host readiness and carry player identity,
  preserving per-key order and cancelling stale/reentrant work. Offline tests pass.
  Paid care now binds unique invocations and original player/slot/connection through
  native read, final cost and queued send; untracked inner calls and skipped POST
  cannot steal reservations. Host rejection/refund is still open.
- [ ] Sky event (47): verify the full host phase/audio/email/bookkeeping sequence,
  permanent-black-hole initialization and mid-chain join/rejoin. Source now refuses
  a client's pre-session latent continuations too. Other non-entity event effects
  need host phases/cues; creature behavior belongs to the other owner.
- [ ] Packages/food (34/39): audit remaining package subclasses and eaten() output;
  require explicit client-local loot provenance. Preserve implemented final-bite
  delivery and eater-local hunger/poison; avoid global replaceProp admission.
  Cooking progress now crosses world/hand/placement/profile/native-save carriers;
  client world and remote-hand cooking factories are refused, personal hand cookItem
  births are narrowly admitted. Late actor/field readiness retains the latest state.
  Six cooking suites, profile regressions and native contracts pass; game checks remain.
  The full ancestry census now covers 78 cooking members and 14 cookItem declarations,
  including eggs/pizzas/potatoes/steak omitted by the earlier filename-prefix audit.
  Early hand conditions now wait for the matching person/generation; failed join
  snapshots and hand sends retry. Banana husks and empty coffee cups use canonical
  host world output and exact personal-hand factory admission. Personal effects
  remain local; conditioned French recipes and other unexamined outputs stay open.
- [ ] Stow performance (16): capture frame times if stutters persist, including
  unresolved-class lookup paths; preserve pending item-state restoration.
- [ ] Models (38): fill remaining preview gaps and verify custom skin compatibility
  without removing rig exclusions. Root prop scaling does not implement skeletal
  bone scaling or arbitrary creature physics; assess those separately if required.

One-time progress audit,21 October: **6/31 non-Entity checkboxes closed (19.35%)**,
including gameplay acceptance and remaining fork reviews. Within the main
implementation/investigation section: **6/14 (42.86%)**. Exclude the15 Entity rows
below and the NPC-only eb55ae97d fork candidate. Partially implemented composite
rows remain open. Inventory/method: build/current-todo-progress.json.

## Entity coverage checklist

For every group, resolve native class/name mapping and missing behavior transport,
then verify host/client spawn, target, motion, actions, damage/death, local effects,
disconnect/reconnect and mid-event join. A transform route alone is insufficient.
Keep personal Insomniac/nightmare and other native personal spawn conditions local.

- [ ] Grayboar: complete remaining join/audio/network-time coordination; verify
  locomotion and movement-loop audio, host target detection, visible turn/pitch,
  helper task retirement/delegates and adoption/held-drive handoffs. Test repeated
  and simultaneous hits, playback failures, death timing, client weapon damage,
  single corpse/loot and host retirement. Verify collision damage, lightning
  replacement propagation and charged explosion/timer/punch/spin effects.
- [ ] Rozitals: Great Pyramid, Kavotia, Pagonioplo-Igetis, Scouts, Soltomia —
  resolve Kavotia/scouts and localized names; verify phases/actions/destruction/join.
- [ ] Antibreather / Fossilhound: player/cave-dependent Antibreather spawning
  and cross-peer attacks; verify the corrected Fossilhound ticker spawn in game.
- [ ] Arirals: verify follower/shooter/pigBeater and implemented gun-stealer paths
  in multiplayer, including stolen/held-item ownership and event phases.
- [ ] Greys / Flamethrower Tank: verify grayTest behavior and firetank turret, fire,
  fuel and damage independently of its Pawn transform feed.
- [ ] Geoms: map Ramiel and Egg-shaped Geom to native classes; verify special actions.
- [ ] UFOs: map Boomerang, Joil, Grey Ship, Midas, Pill, Trifo, Orbs;
  implement/verify child spawns, beam/material/audio phases and late join.
- [ ] Deer / Fish / Cockroach: verify deer/deer_ded; map fish; test roach movement,
  eating, crushing and late join.
- [ ] Blue Brain Cheese, Restocker, Eye, Wolfgang, Tree with Eyes, Living Mushroom,
  Space Jellyfish: establish exact behavior routes, especially actor_save-derived
  Wolfgang; do not identify every eyes entity as eyer.
- [ ] Bone Man, Bloody Skeleton, Curious Eyes, Mimic, Flesh Slug,
  Zombie Deer: finish exact mapping and attack/effect checks; verify mimic shared
  rendering/disguise, damage and lifetime, plus centipede visibility (35/48).
- [ ] Ghost, Ghost Log, Fireflies, Walking Mannequin, Black Spot:
  audit remaining names/effects/phases and test implemented mannequin lifecycle.
  Verify wisp animation readiness and delayed grab/release cancellation through
  pawn replacement, death, world changes and reentrant cleanup.
- [ ] Demons: SKerfur-O, Eyeless Insomniac, Insomnia Demon, Abandoned Kerfur,
  Flesh Erie, Goat Statue, Stolas Statue, Furfur — review individual targeting,
  rituals/run-end behavior; npc_angryErieFlesh is a separate Character (40).
- [ ] Forbidden: Black Friend, CorpseWalker, Crasher, Fast-shadow Figure, Fridge,
  Interloper, The Exit Door, The Head, Pool Entity, Mountain People,
  Cursed Spider, Rune Sphere, Shadow Figure, Fake Kel, Black Sphere —
  map native classes, spawn availability and local/global effects. Do not enable
  debug/test actors in normal play merely to populate this list.
- [ ] Living Erie / Girufus / Rufus: map distinct classes and interaction/animation/
  ownership; do not conflate live, zombie and angry flesh Erie.
- [ ] Gnomes / Krampus: map gnome spawn/interaction/shared lifetime and test Krampus
  attacks/events beyond its existing NPC route.

## Gameplay acceptance checklist

Use matching current 2175 builds. Record role, item/model, reproduction and logs;
offline tests do not close these reports. Existing item-batch and entity checks
above also apply. Issues 6 and the previously working nightmare behavior are not
reopened; verify the newer personal-sleep changes. Wall41 stays excluded.

- [ ] Props (1/4/7/8): client breaking, R+LMB duplication, extinguisher foam and
  loose-outlet floating. Keep outlet floating distinct from appliance connection.
- [ ] Chairs (11/36): rotation and transitions to laptop/sleep/ragdoll.
- [ ] Cremator: verify host-authoritative door and paint-only lever on both peers;
  measure door FPS to check the reported performance problem.
- [ ] Personal sleep (10/49): independent nightmare settings, different beds,
  chance/shared night interruption, simultaneous low sleep and local demon spawn,
  visibility, targeting, grab/attack and wake/release behavior.
- [ ] Bodies (5/31/38/50): every selected/common/custom rig, authored limbs,
  local body, entry/recovery, drift, retry/fallback and streamed remote pose;
  bed mattress/exhaustion anchors, moving beds and sleep/sit/ragdoll precedence.
  Verify sleeping footsteps and hand/head effects are suppressed.
- [ ] Electrical/light/computer (51/53–55): short/long cables, extensions,
  occupied/unpowered outlets; actual light keys, overridden setters, component
  readiness, flicker/load/join timing; shared laptop/portable-PC tabs, text,
  scroll, visibility, cursor/input/activity and operator body. Test ON/OFF during
  animation, power loss/restoration and reconnect.
- [ ] ZIP/cans (56/57): actual inserted/ejected ZIP item class/model/scale on both
  peers; cans open/eat/final portion in hand/world, stow/re-equip, drop/pickup,
  simultaneous use and reconnect.
- [ ] Safe (43): dial/hacked completion, one loot result, persistent lock state,
  retry/stale-record handling and save/rejoin.
- [ ] Garden/sky (46/47): real save/reload and late join after merged changes;
  verify shared schedule and effects for the black-disk/yellow-ring event.
- [ ] ATV: existing saved second-ATV positions, simultaneous carry/mount and stale menus.
- [ ] Disks/racks: client sv.request/hashcodes, floppy/ZIP transfers preserving
  exact text, filled rack save/rejoin and a slow joining peer.
- [ ] Whole-package smoke: swap host/client roles; two clients contest objects;
  late join/rejoin/disconnect/save/reload; partial/empty consumables, full inventory,
  equipped items/container rows, loose cables, flashlight crank/low battery/previews.

## Fork adaptations still requiring review

Audit these candidates against the merged 2154 source before adapting them;
existing shared-screen and garden implementations may already cover parts.

- [ ] Shinobu2 `9863f6ba0`, `291476241`, `56d7ec566`, `9eec3cf37`: review parked
  save ordering/reentrancy, deferred host destruction, held-mirror identity and
  post-birth publication together; retain rollback/duplicate negative controls.
- [ ] Upstream `eb55ae97d`: exclude joining slot from connect-edge NPC fan-out
  with coordinated Session/enrollment changes; preserve late-class/personal/boar paths.
- [ ] Upstream `3a138d025`, `c930d6aa5`: compare active-tab handling with current
  shared UI; tab propagation alone does not establish screen parity.
- [ ] Upstream `53b851c50`, `625187aaa`, `82ad7ef1d`: assess complete camera-bit
  authority/laptop-intent wrapper/network changes and tests against merged code.
- [ ] Upstream `001193570` through `93dd26cc2`: assess the dependent explosion
  capture/client damage/driven-prop coast series; review current wire-kind
  collisions and validate blast ownership before adoption.
- [ ] Upstream `d248fc88d`: assess lobby announce cancellation together with its
  required generation/proof API; do not copy as an isolated patch.
