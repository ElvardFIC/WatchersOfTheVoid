# TODO — current priorities

Continuation size: **10 concrete fixes per pass**. Checked items mean implemented
code with recorded verification; unchecked items mean remaining implementation
or a named acceptance test that has not passed. Completed code does not close an
unreproduced report or an unperformed rendered multiplayer test.

## Priority acceptance — 0.9.2159

- [x] Implement blast-driven prop discovery/motion and final rest publication,
  including zero-damage force-only blasts (b2189–b2190). Targeted native LAN and
  production regressions pass; rendered/input acceptance remains below.
- [ ] Accept host/client blast-driven prop movement and final resting poses,
  including both origins, simultaneous/force-only blasts, native object-type filters, pickup/handoff
  and late join. Targeted native evidence does not close all gameplay cases.
- [x] Replicate ATV handle/wheel steering and retain pending honks for late join
  (b2188); production controls and native LAN checks pass.
- [ ] Accept ATV steering on host/client, left/right/center, driving and stopped,
  late join and driver/carrier handoff using identical current DLLs.
- [x] Validate and forward owner blood-loss drops without echoing to the injured
  player; damage/viewport effects remain personal (b2187). Native/production checks pass.
- [ ] Accept visible bleeding from host and client injuries in rendered multiplayer
  with identical current DLLs, including stopping blood loss and joining mid-bleed.
  Native drop collisions/stains run independently on each peer; exact stain layout,
  historical-stain joins and persistence are not established by this change.

## Current baseline and pending acceptance

Current development: **DLL b2215 / protocol 2215**, game **0.9.0n**, targeting
package **0.9.2159**. Upstream `ad5315611` is merged with all local history,
including native-action/hit-result fixes from `ddc711220`.
**94/94 full regression groups pass**; Release/ABI qualification is recorded in
`build/upstream-qualified-2215/verification.json`. See [TESTLOG.md](TESTLOG.md).
Native blast/ATV evidence was recorded on earlier DLLs. Rendered gameplay and
real keyboard input still need acceptance.

Last prepared manager package: **0.9.2158 / DLL b2186 / protocol 2186**.
Its Release, **68/68 offline groups**, package label and archive checks passed.
It does not include the b2187–b2215 fixes; no 2159 archive is prepared.
Verification history remains in [TESTLOG.md](TESTLOG.md); completed release reports
and superseded build instructions have been removed from this backlog.

- [ ] Verify startup from the full manager package, including the displayed public
  version. Earlier DLLs passed native startup; current-DLL/full-package acceptance remains open.
- [ ] Accept ordinary rendered host/client gameplay with identical current DLLs.
- [x] Implement watch-capacity recovery, player-flashlight state retries and
  exact-recipient container seeds for immediate joins; implementation boundaries:
  [SYNC_2157_FIXES.md](SYNC_2157_FIXES.md).
- [ ] Accept the 2157 immediate-join reports: player flashlights, light switches,
  garage/base doors and client drone contents. Watch capacity, flashlight retries
  and exact-recipient container seeds are implemented; details:
  [SYNC_2157_FIXES.md](SYNC_2157_FIXES.md).
- [x] Finish the 2–32-player chooser and carry its remembered value through both
  menus, all hosting paths and lobby counts (b2207). Native controllers and all
  118 local master tests pass; details below.
- [ ] Accept 2–32-player hosting/admission in game and confirm the deployed
  master's matching capacity. Its local Release build is ready; no remote deployment is recorded.
- [x] Integrate upstream through `ad5315611`, preserving local history, 32-player
  hosting, published wire IDs, prop lifetimes, ATV steering and blast motion.
- [ ] Accept the combined upstream behavior in game: host/client damage and
  creature hits, ATV ownership/conditions/joins, explosions, shared prop actions,
  notebook/paper edits, drone controls and parked-creature presentation.
- [ ] Optional future rigging: authored/cooked player PhysicsAsset or a separately
  validated stock-body display retargeter, after choosing its tradeoffs.

Use identical current DLLs on every peer. Prioritize reproducible failures,
resource identity/transactions and the PC/ragdoll reports below. Unchecked gameplay
items can already have implemented code; they require acceptance, not reimplementation.
Mining (6) stays closed; Wall41 stays excluded; chair reports 11/36 are duplicates.
The owner resumed the fresh game-pak/Blueprint audit on 24 October; ordinary
Blueprint extraction and the imported native-contract suite now pass.
Details: [WORK.md](WORK.md), [ENTITY_COVERAGE.md](ENTITY_COVERAGE.md).

## Report batch of 24 October (numbers local to this batch)

### Implemented — ten prop-action/MRE delivery fixes (b2214)

- [x] Retire action queues, rate buckets, safe publication and MRE scopes across
  session pointer/serial, world, role, running and connection transitions.
- [x] Capture the target and its class before native key lookup; callbacks that
  retire either identity cannot enqueue an action against a replacement.
- [x] Reserve an owned, numbered queue entry before the first send; a nested
  action cannot overtake the request whose transport callback produced it.
- [x] Drain owned payload snapshots with a reentry guard, preserving exact bytes
  across callback queue resets and preventing nested drains from repeating a hit.
- [x] Match send completion to its source and queue revision, preserving fresh
  queues installed by callbacks; only retained or delivered work is accepted.
- [x] Bound each drain to eight rows and retain FIFO/backpressure, capacity,
  expiry/clock checks, exact key/class and world-to-hand MRE refusal.
- [x] Refuse malformed, foreign-source and unscoped reflected client world-MRE
  openings; coalesce queued opens and guard their request cue through callbacks.
- [x] Require an active, bounded native personal-MRE spawn scope for loot birth,
  with actor/class/function/owner/player/invocation guards, exact POST/pruning and
  captured hand-open provenance when the native body clears the hand.
- [x] Fence received MRE opens by ready recipient/person/body, target/key and
  native function/owner/mesh currency through preparation. Reserve before native
  callbacks; allow at most one admitted dispatch per current actor, while failed
  preparation remains retryable. An uncertain admitted result is not replayed.
- [x] Capture and retry safe records using owned actor/class/revision snapshots;
  stale key reads and old publication completions cannot discard newer work.

Local controller/native checks, nine fault controls and stable Release/ABI pass;
6/6 selected groups are recorded in TESTLOG. Rendered host/client opening, package debit/output failure
recovery, native ancillary effects/rewards and remaining package subclasses stay open.

### Implemented — ten food callback/output fixes (b2213)

- [x] Refuse client eats that cannot reserve an active invocation or fit the
  bounded observation table; unrelated food actions keep their native path.
- [x] Require an addressable item, a successful condition read and valid positive
  portions before allowing a client to consume optimistically.
- [x] Reserve the observation before callback-capable reads and reject nested
  eats of the same actor, preventing an untracked second consumption.
- [x] Capture one request address and revalidate its key, element and actor/class
  before admission and live POST; retagged items cannot submit an old bite.
- [x] Retire food caches and active observations across session pointer/serial,
  world, role, running and connection transitions; callbacks guard source currency.
- [x] Detach only the exact eat POST record without rereading expired parameters;
  retain captured final-bite delivery after native destruction and send refusal.
- [x] Admit box/banana/cup births only inside their captured native eaten scope
  and exact declaring graph, replacing hand-pointer-only output permission.
- [x] Preserve original local-hand provenance while a live final bite clears
  the hand before eaten runs; world and mirror outputs remain host-owned.
- [x] Bound output scopes and guard their actor/class, function/owner, player and
  invocation incarnations; exact POST or skipped-POST pruning retires permission.
- [x] Validate host bite condition/key/class and recheck zero portions after
  publication and native metadata preparation, preventing stale empty output or
  destruction of refilled/replaced food. Replay only the three wrapper overrides;
  hunger, poison, hints and achievements remain native on the eater.

Local controller/native checks, eight fault controls and stable Release/ABI pass;
8/8 selected groups are recorded in TESTLOG. Cross-peer concurrent consumption and
rendered acceptance remain open; other package subclasses still need an audit.

### Implemented — ten wall material delivery fixes (b2212)

- [x] Retire queued work on world, session serial, role and running-state
  changes, including reuse of the same session pointer.
- [x] Capture the wall/material and their class incarnations before enrollment;
  reject malformed overlaps and callback-retired pairs before admission.
- [x] Keep the active pair visible during preparation and admit its replay once,
  preventing nested enrollment/overlap callbacks from stealing or duplicating it.
- [x] Guard native metadata preparation, collision query and overlap dispatch
  against caller, actor/class, component, field and function replacement.
- [x] Pace pre-admission retries at 250 ms with fair rotation and eight actual
  attempts per tick; preserve the existing bounded queue and expiry.
- [x] Observe native replay admission and never automatically retry that queued
  attempt afterward, including failure after material may have been consumed.
- [x] Match native POST to its invocation/source/actor, detach before callbacks
  and prune skipped POST records; bounded refusal keeps repair stock untouched.
- [x] Preserve fresh queued work and nested session installation while retiring
  old callbacks; old completions cannot resume offline spending.
- [x] Attempt all six guards independently and retry only missing registrations,
  so an earlier registration failure does not skip the repair protection.
- [x] Publish current partial wall state when an admitted call loses POST or
  fails, without replaying the material-spending graph.

Local controller/wrapper checks preserve solo-host ownership, last-peer departure
and client no-spend repair refusal. Paid client repair still needs its complete
request/result and personal-stock transaction; rendered acceptance remains open.

### Implemented — ten local container recovery fixes (b2211)

- [x] Reserve bounded recovery ownership before the base world-container
  extract body can remove its selected saved row; refuse unsupported admission.
- [x] Own the complete saved record across caller-frame expiry, preserving its
  key, zero/partial stock, typed groups and same-save nested inventory index.
- [x] Match debit and pickup receipts to exact native invocations; initialize
  result flags and refuse a pocket path whose frame cannot be tracked.
- [x] Restore full records through checked encoding with owned input and native
  frame/source guards; partial scratch never reaches loadData.
- [x] Retire accepted personal credit from the pocket POST or the enclosing
  native result, preventing a second saved-row credit.
- [x] Recover failed/null spawning, including a begun but unfinished carrier;
  retire the exact live carrier before returning its record.
- [x] Retain refused destruction/refund debt and retry with a bounded rotating
  budget; nested callbacks cannot settle the same record twice.
- [x] Append refunds atomically to current contents, preserving other edits;
  same-key/class retries keep newer data and conflicting classes refuse.
- [x] Guard world/session/role/player/container/component/save/class ownership
  and nested replacement; best-effort teardown settles only proven current debt.
- [x] Suppress premature carrier publication, mark committed refunds for
  synchronization, and rebuild names before source-guarded volume/mass updates.

This is local base prop_container_C::extract recovery. Cross-peer debit/output
arbitration, personal extraction overrides and raw takeObj paths remain separate.
If a fault skips the receipt needed to prove debit/output, keep bounded unresolved
debt rather than risk duplication. Debt does not persist across world/session
replacement; rendered multiplayer and disconnect failure acceptance remain open.

### Implemented — ten console input and playback fixes (b2210)

- [x] Consume native client input successfully before submitting its world
  command; consumption failure cancels the body without sending that command.
- [x] Prepare reflected textbox work before changing input, then validate the
  terminal, class, function and textbox ownership around each native boundary.
- [x] Refuse a nested commit of the same input while its original Enter is
  preparing or consuming it; an already full command outbox retains the input.
- [x] Bind client preparation, submission and counters to the captured source,
  widget incarnation and host binding across reads and transport callbacks.
- [x] Track each local command by invocation so nested commands preserve the
  outer command's clear/output ownership.
- [x] Prune canceled/faulted invocations without waiting for a POST that may
  never arrive; bound the retained records.
- [x] Match POST to its own captured invocation and detach it before native
  reads; preserve latent output ownership and pair graph-watch rollback exactly.
- [x] Stop a display drain after source/widget retirement; retire queued text
  and the old mirror busy level when the client's host binding changes.
- [x] Retry a failed row only while its stream revision survives; preserve
  appended rows and let a newer clear supersede old failed output.
- [x] Limit replay bypass to the owned row's arguments and one direct native
  invocation, allowing nested commands/alerts and replacement-session output
  through normal synchronization.

Local controller and real-wrapper regressions cover these boundaries. Rendered
command input, console layout and simultaneous multiplayer use remain open.

### Implemented — ten console synchronization fixes (b2209)

- [x] Support 31 private remote terminals, covering all clients in a 32-player session.
- [x] Stop stale queued sends and host command preparation after world, session
  serial, role or link changes, including context-cache commits.
- [x] Bind queued output, partial input and terminal attachment to the current
  recipient generation/GUID and explicit leave/rebind incarnation.
- [x] Own each outgoing blob across transport callbacks that can replace its queue.
- [x] Identify queue entries so an old send cannot remove a replacement head;
  keep those identities aligned when the bounded queue drops an old entry.
- [x] Serialize each recipient's flush across nested sends, appends and resets.
- [x] Revalidate source, recipient and queue head between real wire chunks;
  retain refused output for a paced retry.
- [x] Retry revision-owned history seeds with current busy state; enqueue each
  shared row for every observer before sending, preserving nested clear order.
- [x] Retain both busy levels until the local widget exists and bind local holds
  to that widget's current incarnation.
- [x] Detach retired terminals and session state before callbacks, preserving
  nested replacement sessions and stopping stale terminal construction.

Local production-controller tests cover these boundaries. Rendered simultaneous
console use, command effects and real keyboard input remain acceptance work.

### Implemented — main-menu and branding cleanup (b2208)

- [x] Stop adding and animating the mod's tester/reviewer thanks block under
  the main-menu version labels.
- [x] Remove the unused thanks renderer and its widget-building implementation.
- [x] Remove the embedded thanks text and resource ID/resource script.
- [x] Remove thanks-list startup initialization and the obsolete cache/parser module.
- [x] Remove browser-triggered thanks refresh and the client FetchThanks API.
- [x] Remove retired CMake inputs and update HTTP/prose censuses and UI/release docs.
- [x] Remove the original-author promotion from the startup banner.
- [x] Remove voluntary original-author credits from current README/package copy
  and the old branding sentence in the distributed changelog.
- [x] Remove the original-author phrase from future package manifest descriptions.
- [x] Remove the old author's Sponsor link from repository funding metadata.

MIT copyright/permission notices, third-party notices and historical source
provenance remain. Current maintainers and the game's own menu remain intact.

- [ ] Accept the cleaned main menu visually after loading the current DLL in game.

### Implemented — ten host-player selection fixes (b2207)

- [x] Give player capacity its own row in session settings, with enough window
  height and an explicit label saying that the host counts toward the total.
- [x] Add quick choices 2/4/8/16/32 alongside single-step controls for all 31 limits.
- [x] Remember the choice in the typed net.player_limit row (default four,
  range 2–32), including recovery when persistence is temporarily unavailable.
- [x] Share the selection between native and overlay menus; slider dragging
  updates memory and writes the final choice once instead of writing each frame.
- [x] Clear controls after partial construction/menu loss and show endpoint
  button state; clamped or ambiguous clicks cannot select outside the range.
- [x] Read the selected limit into scripted/environment host configuration,
  using the same registered default/range as interactive hosting.
- [x] Carry the chosen limit into the legacy HostLobby network configuration;
  its advertised value no longer differs from its default four-seat admission.
- [x] Announce the environment host's actual configured limit instead of four.
- [x] Normalize the initial and retained lobby announcement limit, so retries
  and re-announcements preserve the same 2–32 capacity as the host configuration.
- [x] Keep browser/master counts within total capacity, including the host;
  wide JSON integers cannot wrap counts, and the master returns its accepted limit.
- [ ] Accept the selector's rendered layout/input, full-server refusal and seat
  reuse in real play at limits two and 32; confirm the matching remote master.
  Offline controller/API tests do not represent a 32-player rendered session.

### Implemented — ten sleep synchronization fixes (b2206)

- [x] Retain refused host-asleep flags per recipient and retry current truth;
  rapid sleep/wake changes supersede obsolete levels with bounded retry pacing.
- [x] Retain refused peer-rest relays, including departure-to-awake updates,
  instead of losing the only notification of a changed state.
- [x] Seed every peer row, including awake, for a joining or newly ready client.
  Partial/refused seeds retry and clear stale sleeping mirrors.
- [x] Bind rest votes and delivery to transport occupancy generations; missed
  departures, seat replacement and lost readiness clear old votes and pending reports.
- [x] Pay paced reports only in their installed live host session; unready,
  departed and disconnected reports cannot consume debt or reappear afterward.
- [x] Veto shared acceleration immediately for an accepted wake/dream report
  waiting in the pacing queue, while its presentation relay remains paced.
- [x] Require the host's native state to be resting, with readable finite flags;
  an overlapping host dream cannot qualify as unanimous sleep.
- [x] Refuse stopped/disconnected sleep input, null payloads, nonzero padding and
  awake-with-bed rows on either receive role.
- [x] Clear sleep snapshots, pending reports and delivery debt across world,
  session serial, session object and role changes; interrupted sends cannot clear newer debt.
- [x] Publish the client's initial awake state and latch the exact accepted
  payload; a nested newer send or disconnect cannot be overwritten by an old completion.
- [ ] Accept b2206 host/client sleep/wake/dream, refused-send recovery, late join,
  reused seats and unanimous clock transitions in rendered multiplayer.
  Native nightmare probability remains personal; per-bed capacity is still deferred.

### Implemented — ten inventory/profile fixes (b2205)

- [x] Refuse malformed personal-store array headers instead of saving a false
  empty inventory over a player's profile. Genuine reserved empty arrays remain valid.
- [x] Publish a personal snapshot only after equipment and hand headers validate;
  failed reads leave no partial inventory available for profile publication.
- [x] Treat malformed world-container arrays as unreadable, preserving shared
  contents instead of publishing a false empty slice.
- [x] Refuse inventory restoration against malformed carried/equipment/hand
  target arrays, preserving the existing fixed-slot shape and live headers.
- [x] Stage the complete held record with checked encoding before replacing it.
  Construction failure or stale save/world/header/slot leaves the old item intact.
- [x] Recheck container, component, class, world, ownership and function lifetime
  between native volume and name updates; stale callbacks cannot continue.
- [x] Bind join restoration to the live client session, world, save and request
  across native calls; partial effect reset keeps profile reporting closed.
- [x] Own one revision of the complete pending profile and consume an exact join
  request, preventing mixed items/vitals/pose and stale completion after nested joins.
- [x] Own the recipient GUID and guard host profile preparation and every chunk
  against session/world/recipient/sequence changes, including disconnect callbacks.
- [x] Bind the host's profile-sent latch to recipient generation and GUID;
  replacement occupants rearm delivery and interrupted sends retain retry debt.
- [ ] Accept b2205 join/rejoin, malformed-state recovery, held-item changes,
  container UI updates and replacement recipients in rendered multiplayer.
  Outer-header checks do not validate every nested record or establish the
  original item-copy trigger; removed-row/carrier refunds remain separate work.

### Implemented — five world-container fixes (b2204)

- [x] Remove the lane's duplicate raw Player/offset readers. Publication and apply
  share the masked, class-incarnation wrapper; a world slot can never be personal
  index zero, even when a broken component has Player=false.
- [x] Use checked record staging for container slices. Failed nested allocation
  or construction preserves live contents and remains parked for retry; unresolved
  classes wait before allocating. Failed writes do not advance publication hashes.
- [x] Bind apply/relay to actor, inventory/store/header, sender occupancy and
  session/world scope. Accepted newer input/local edits cancel older work;
  rejected input does not. Birth completion follows commit before native callbacks,
  and duplicate hashes require the same live component and store binding.
- [x] Replay owned park snapshots with revision/epoch checks, protecting newer
  admission/reset and nested sweeps. Retire only the observed older row after
  newer input; enforce author caps on ownership replacement and avoid TTL underflow.
- [x] Retain a refused relay for that exact recipient generation. Retry sends
  current container truth through the existing targeted-seed ledger.
- [ ] Accept b2204 simultaneous container use, failed construction/recovery,
  late join/rejoin, recipient replacement and native UI/volume updates in game.
  Original item-copy reproduction and removed-row/carrier refunds remain open.

### Implemented — four inventory restore fixes (b2203)

- [x] Stage all carried/equipment/hand arrays before replacing any live header.
  A checked codec reports outer/nested allocation, class, name, string and signal
  construction failures; an incomplete profile preserves the previous arrays.
- [x] Own the input snapshot and cancel stale staging after world, save/gamemode
  identity, save binding or array-header changes. A nested apply supersedes an
  older apply; native callbacks cannot publish its stale scratch afterward.
- [x] Share the real masked Player flag between personal and world readers.
  Both personal readers require the baked index zero; another slot is refused
  rather than logged and read as this player's items.
- [x] Scope reflected save/container/index/owner/volume fields to each class
  incarnation, refreshing after replacement and after an old failed lookup.
- [ ] Accept b2203 join/rejoin, empty/full profiles, worn/held fixed slots,
  world reload and changed-class recovery in game. The original item-copy
  trigger is still unknown; this pass does not close it or the separate
  removed-container-row/carrier restoration/refund transaction.

### Implemented — three arirShipAppear fixes (b2202)

- [x] Publish the host's visible/alpha phase, material, native light curve and
  radar activity. Reuse the exact map actor or a unique nearby match; stop a
  client's pre-session timeline before painting the host's alpha.
- [x] Refuse client-local appearance, camera/day disappearance and blank-email
  continuations. Admit only the adapter's exact native photo-listener binding;
  local photography remains native while the shared actor is visible.
- [x] Retain current state through actor/component readiness, refused sends and
  partial application. Quiet late-join seeds, ordered incarnations/retirement,
  bounded retries and recipient/world/session guards prevent stale effect replay.
- [ ] Accept this exact blank-email event on both roles, appearance/fade/radar,
  looking away/daylight, local photography, repeated mail, joining during a fade,
  reconnect and teardown. `arirShipVanishing_C`, other warp events and shared
  reputation consequences remain separate work; report (6.1) is still open.

### Implemented — three container-birth fixes (b2201)

- [x] Replace the global extraction flag with the gate's native-body scope:
  nested extraction, cancellation/fault unwinding and unrelated threads cannot
  expose or suppress another call's provisional item key. Blueprint calls are watched.
- [x] Bind the successful object result to the live function incarnation, with
  frame bounds and the actual caller's out parameters. Refuse failed/stale results.
- [x] Retain refused birth/state publication separately, with final identity,
  fair bounded retries and actor/world/session guards. The generic host finish
  feeder waits for this publisher, so it cannot send records ahead of the birth.
- [ ] Accept b2201 container ejection/extraction on both roles, rapid nested uses,
  mutable/empty contents, saturated queues, reload/rejoin and destruction during
  delivery. These code fixes do not establish the cause of the original item-copy report.

### Implemented — six original-report paths (b2200)

- [x] (1) Synchronize transient shovel holes through bounded identity, soil and
  geometry records, acknowledgement/retry, retirement and late join. Native
  BeginPlay is suppressed on mirrors to avoid repeating buried-loot creation.
- [x] (3) Drain canonical can state before mutation and refuse ordinary peer
  records that refill portions, reclose an opened can or change its type.
  Initial saved births keep their original state; other food families remain open.
- [x] (4) Key satellite console claims by physical panel and native widget owner,
  allowing different consoles to be operated independently.
- [x] (5) Release the unpublished companion-ATV retry latch after failed
  creation/publication, so a missing second body does not permanently block retry.
- [x] (6.2) Keep remote location-event activation while removing forced player
  gathering/teleport to the base from that route.
- [x] (7) Gate shared host sleep acceleration on unanimous connected-player rest;
  awake, joining and dreaming participants prevent global acceleration.

Verified code: `build/session-reports-2200/verification.json` and the batch's native
contracts, as recorded in TESTLOG. The following rendered acceptance is separate.

- [ ] Accept b2200 fixes in rendered host/client play: (1) shovel-hole creation,
  soil material, filling/removal and late join; (3) cans open/eat/empty with stale
  peer records; (4) different satellite consoles at the same time and contention
  for the same console; (5) companion ATV retry after failed creation/publication;
  (6.2) event activation while distant players remain in place; (7) shared clock
  acceleration only when every connected player is resting, including wake,
  dream, joining and disconnect transitions. Offline tests do not close these.
- [ ] (2) Client-visible item copy: build and immediate trigger remain unknown
  (owner could not identify either). Capture both peers' class/key/EID and logs
  around the first appearance. b2201 fixes specific container-birth defects;
  the original reproduction remains open, without speculative duplicate retirement.
- [ ] (6.1) Accept the dedicated b2202 `arirShipAppear_C` phase adapter against
  the original warp-arrow report (its screenshot's blank-email loop matches this
  native graph). `arirShipVanishing_C` and other variants remain unmapped;
  original reproduction and shared reputation still need verification.
- [ ] Optional suggestion, deferred: for at most four participants (host included),
  allow two sleepers per floor bed; above four, allow all. Current sleep packets
  carry a bed class, which cannot arbitrate individual mattress occupancy.

## Event synchronization request - OPEN

- [ ] Complete verified native routes for Dreams, Ariral Reputation,
  Player-Activated, Time-Related, Endings, Story Mode, Random and Signal events.
  Every event needs its real trigger, chosen outcome, effects and join/lifetime
  checks. WORK's event coverage table includes dream wake/rewards, reputation
  tiers/relocation, simultaneous activations, calendar/3AM branches, per-player
  ending consequences, active story phases and signal processing/playback.
- [ ] Extend frequency coverage beyond Fossilhound, killer-wisp, sky-wisp,
  night-egg, owner-local night-eyes, peer-symmetric fireflies and host-owned
  flickerer to the complete repeatable random-event list. Night eyes now
  preserve native night/day and 5% chance checks; preserve personal
  dreams/Insomniac and story dates.
  `treeSpawner`, `bp7Spawner` and `serverBreaker` still need complete authority/effect
  routes before admission, including protected-server floppy consumption.
  Change eligible-roll waits, preserving eligibility,
  probabilities, AI/animation, effect durations and sleep needs.
- [ ] Complete level/map-script coverage once their asset headers are available.
  The fresh ordinary-Blueprint inventory covers 1,621 packages / 22,691 functions
  and 69 scheduled-event rows. Raw rechecking resolved two false decoder errors;
  this inventory does not establish complete event synchronization.
- [ ] Real-peer event, frequency, late-join, reconnect and simultaneous-trigger
  acceptance on identical current DLLs, including repeated/chosen events,
  live/persisted frequency settings, load windows and teardown.
  Include serverbox refused-send/readiness retries and dream-burger collection
  by local players versus remote puppets/ragdolls. The game disk is currently
  unavailable; the owner confirmed this on 24 October.

## Investigations requiring runtime evidence

- [ ] Bench58: the owner confirms **the host's old rifle** disappeared after
  simple placement (21 October). Earlier components remain unidentified.
  Native candidate: `prop_rifle_C`; its leaf Blueprint has destructive LMB use,
  without a placement entry point. Inherited behavior/cause remains unverified.
  Reproduce with `VOTVCOOP_PROP_BIRTH_KEY_PROBE=1` and capture class/key, destroy
  caller/timing, `[PROP-DROP]`, `remote_prop::OnDestroy` and sweep logs.
  Client placement exhaustion recovery is implemented separately; it does not
  establish a fix for the host report.
  Development b2178 also prevents a received destroy from using an actor replaced
  during listeners/native grab release and adds role/sender/class/key diagnostics.
  This reproduces a code defect offline; the owner's disappearance still needs runtime evidence.
- [ ] Token52: clarified on 22 October as probably the general balance. Accept
  shared `saveSlot.Points` after join/rejoin, credit, spending and refused orders
  on both roles. The balance-delivery defects now have production regressions;
  b2198 also cancels queued credits after world replacement.
  The exact original UI/reproduction is still not confirmed.
- [ ] Trowel key continuity and stock across reconstruction/unequip/drop:
  identity mismatch defers the profile. Capture the actual divergent keys around
  native `mainPlayer.updateHold` reconstruction from `saveSlot.hold[0]`.
  Exact-identity concrete changes now persist into the existing `hold[0]` scalar
  before reconstruction, including zero. The fresh native audit preserves an
  existing non-None saved key; a missing-key cause has not been reproduced.
  Mismatch repair and runtime acceptance remain.

## Remaining implementation

- [x] Records (b2194–b2197): initial-author waits, capture readiness, guarded
  native dispatch and full-record carrier restoration; failed/invalidation paths
  refuse pocket capture. Verification and limits are recorded in TESTLOG.
- [x] Materials (b2193 and held-stock fixes): persist exact held stock, including
  zero, and reject stale material/hand snapshots through capture and publication.
- [x] Grill edits (b2191–b2192): guard ordered coal/lid work and hand rows across
  session restart, replacement, native-read retirement and interrupted delivery.

- [ ] Console/events (12–15, 26, 42): complete Rufus/thiccfus, llama, Madness grunts
  and variant AI/damage/effects; accept the host-ordered submitted/periodic/clear
  console output in game. b2209 implements recipient/source-safe queues, history,
  busy state and all 31 remote terminal seats with local regression coverage.
  Verify Madness playback recovery and Maxwell phase. b2210 implements client
  input-consumption and per-row native display lifetime with invocation-owned
  attribution, scoped replay and local controller/wrapper regression coverage.
  Other funny-rule HUD/events and remaining toy/squeeze sounds still need coverage.
- [ ] Cement/walls (21/22): atomic repair request/result with exact personal stock
  across hand/slot/drop, retries, rejection and disconnect; creative construction and persisted
  wet-wall timers now have imported controllers; verify native impact/effects. Keep no-spend refusal until
  the transaction is complete. b2212 implements guarded host material delivery,
  paced pre-admission retries and no automatic repeat after native admission.
- [ ] Water/cement/coal (20/21/44/45): arbitrate concurrent writers and stale
  in-flight records, dryTimer ownership and personal-hand transitions. Imported
  stock persistence, sponge foam, ordered grill coal/lid edits and cooking-progress
  carriers need gameplay acceptance; final output/accounting gaps remain.
  Held-stock persistence, grill lifetime/delivery guards and exact material
  snapshot validation are implemented; see WORK and TESTLOG for their boundaries.
- [ ] Personal resources: finish debit, output acceptance and refund as one
  transaction; cover rejection, retry, disconnect and hand/slot/drop transitions.
  Imported stock/profile persistence alone does not complete these transactions.
  Cross-peer writer arbitration and accounting remain open.
  Local base world-container failure escrow/refund is implemented in b2211;
  b2197's capture refusal alone did not return the removed record. Complete
  cross-peer debit/output authority, personal overrides/raw takeObj paths and
  receipt-less fault recovery/persistence before closing the category.
- [ ] Grime/scorch: cover remaining dynamic grime sources and preserve explosion/
  salt-rocket scorch provenance through a full save/world reload. Same-world
  reconnect coverage does not establish persistence across world reconstruction.
- [ ] Sky event (47): verify the imported host ownership of audio, delayed
  email/bookkeeping and all six pre-session latent continuations. Transport
  presentation phases/cues for clients and other custom effects/creatures.
- [ ] Packages/food (34/39): audit remaining package subclasses and their output
  paths; complete concurrent cross-peer portion/output arbitration, including
  optimistic personal effects before host acceptance. b2213 guards food input,
  captured final-bite delivery and exact box/banana/cup eaten output provenance;
  six personal eaten overrides and the empty base remain excluded from host replay.
  b2214 guards action delivery, received world-MRE opening and exact personal-MRE
  spawn provenance. Native poison-pour/roach effects and reward routing still need
  acceptance; uncertain admitted opening failures do not implement output/refund recovery.
  Rendered acceptance and full-save behavior remain open; avoid global replaceProp admission.
- [ ] Stow performance (16): capture frame times if stutters persist, including
  unresolved-class lookup paths; preserve pending item-state restoration.
- [ ] Models (38): fill remaining preview gaps and verify custom skin compatibility
  without removing rig exclusions. Assess skeletal bone scaling and arbitrary
  creature physics separately if required.

## Entity coverage checklist

Use the exact native mapping and per-class gaps in ENTITY_COVERAGE. For every
group, finish missing behavior transport and verify spawn, target, motion,
actions, damage/death, local effects, disconnect/reconnect and mid-event join.
Keep personal Insomniac/nightmare and native personal spawn conditions local.

- [ ] Grayboar: remaining join/audio/network-time coordination; verify locomotion
  and movement-loop audio, host target detection, visible turn/pitch, helper task
  retirement/delegates and adoption/held-drive handoffs. Test repeated/simultaneous
  hits, playback failures, death timing, client weapon damage, single corpse/loot
  and host retirement. Verify collision damage, lightning replacement propagation
  and charged explosion/timer/punch/spin effects.
  Check damage under saturation and directional hits; investigate independently
  spawned client twins if reproduced.
- [ ] Rozitals: Great Pyramid, Kavotia, Pagonioplo-Igetis, Scouts, Soltomia —
  finish action/effect adapters and verify phases/actions/destruction/join.
  Kavotia still needs fire/laying montage, speech, canonical beam/impact/damage
  and exact phase-time alignment.
- [ ] Antibreather / Fossilhound: player/cave-dependent Antibreather spawning
  and cross-peer attacks; verify Fossilhound ticker spawn and attacks in game.
- [ ] Arirals: follower/shooter/pigBeater and gun-stealer gameplay, including
  stolen/held/thrown-item ownership, attachment, shots/hits and event phases.
- [ ] Greys / Flamethrower Tank: grayTest targets/actions/effects; firetank turret,
  fire, fuel, damage and destruction.
- [ ] Geoms: Ramiel (`geomOcta_C`) movement/charge/beam/audio/damage and
  Egg-shaped Geom (`eg_C`) retrieval/landing/special actions.
- [ ] UFOs: child spawns, beam/material/audio phases and late join for Boomerang,
  Joil, Grey Ship, Midas, Pill, Trifo and Orbs. Grey Ship display-name split needs
  a live probe.
- [ ] Deer / Fish / Cockroach: deer/deer_ded spawn/death/rejoin, caught
  `prop_fish_0..21_C` variants/inventory, roach movement/eating/crushing/late join.
- [ ] Blue Brain Cheese, Restocker, Eye, Wolfgang, Tree with Eyes, Living Mushroom,
  Space Jellyfish: finish exact behavior routes and effects. Wolfgang is
  actor_save-derived; do not identify every eyes entity as eyer.
- [ ] Bone Man, Bloody Skeleton, Curious Eyes, Mimic, Flesh Slug, Zombie Deer:
  attack/effect/lifetime checks, unresolved display-name associations, mimic shared
  rendering/disguise/damage and centipede visibility (35/48).
- [ ] Ghost, Ghost Log, Fireflies, Walking Mannequin, Black Spot: unresolved
  names/effects/phases and lifecycle gameplay. Verify wisp animation readiness and
  delayed grab/release cancellation through pawn replacement, death, world changes
  and reentrant cleanup.
- [ ] Demons: SKerfur-O, Eyeless Insomniac, Insomnia Demon, Abandoned Kerfur,
  Flesh Erie, Goat Statue, Stolas Statue, Furfur — individual targeting,
  rituals/run-end behavior. Keep distinct live/zombie/angry flesh Erie classes.
- [ ] Forbidden: Black Friend, CorpseWalker, Crasher, Fast-shadow Figure, Fridge,
  Interloper, The Exit Door, The Head, Pool Entity, Mountain People,
  Cursed Spider, Rune Sphere, Shadow Figure, Fake Kel, Black Sphere —
  finish missing mappings, spawn availability and local/global effects.
  Do not enable debug/test actors in normal play to populate this list.
- [ ] Living Erie / Girufus / Rufus: unresolved Girufus mapping and distinct
  interaction/animation/damage/ownership/lifetime behavior.
- [ ] Gnomes / Krampus: group spawn, presents, revenge, interaction, damage/death
  and shared lifetime; Krampus attack/fireball/jump/target/despawn events.

## Gameplay acceptance checklist

Use matching development **b2214** DLLs for new checks; the prepared 0.9.2158
package remains **b2186**. Record role, item/model, reproduction and logs.
Offline tests do not close gameplay reports. Mining (6) stays closed; previously
working nightmares (10) only need acceptance of the newer personal-sleep behavior.
Wall41 remains excluded.

- [ ] Item batch (2/3/9/25/27/29/34/39): grab jitter and contested control;
  actual collision push/convergence; both gun effects and freeze/root scale;
  struck panels; crowbar replacement/parents; crate/MRE/foodBox loot; final bite
  and local poison; battery hand/stow/drop/rejoin.
- [ ] Placement: refusal/recovery and late PropSpawn adoption with two peers,
  device ejects, containers and live food/drive state.
- [ ] Coins: contest one coin via E/overlap on both peers; one host credit,
  accepted mirror removal, refusal retention and client-local map coins.
- [ ] Props (1/4/7/8): client breaking, R+LMB duplication, extinguisher foam and
  loose-outlet floating. Keep outlet floating distinct from appliance connection.
- [ ] Water/surfaces (17–20/44): pressure-washer stream/audio and host impacts,
  pouring/drinks/fuel, computer water short-circuit sparks/damage/shutdown,
  canonical grime/cleaning, quiet joins and final-state retries. Verify scorch
  lifetime/reconnect separately from the unresolved full-reload provenance.
- [ ] Inventory/containers (23/24/28/32/33): exhausted and cranked flashlight on
  both peers; anchored-hook retirement, one ordinary pickup, retry and late join;
  drone audio; all equippable items with full inventory; simultaneous container
  removal and row refresh on the other peer.
- [ ] Chairs (11/36): rotation and transitions to laptop/sleep/ragdoll.
- [ ] Cremator: host-authoritative door and paint-only lever on both peers;
  measure door FPS against the reported performance problem.
- [ ] Personal sleep (10/49): independent nightmare settings, different beds,
  chance/shared night interruption, simultaneous low sleep and local demon spawn,
  visibility, targeting, grab/attack, wake/release, reconnect and rare Child variant.
- [ ] Bodies (5/31/38/50): selected/common/custom rigs, authored limbs, local body,
  entry/recovery, T-pose, drift, retry/fallback and streamed remote pose; slopes,
  running momentum, camera/recovery heading and incomplete remote limbs;
  moving props/walls, high latency and teleport. Bed mattress/exhaustion anchors,
  moving beds, sleep/sit/ragdoll precedence and sleeping effect suppression.
  First verify the 21 October C-key launch report on host and client: standing,
  crouched and repeated entry/recovery, floor/slopes and each selected rig.
- [ ] Electrical/light/computer (51/53–55): short/long cables, extensions,
  occupied/unpowered outlets; actual light keys, overridden setters, component
  readiness, flicker/load/join timing; shared laptop/portable-PC tabs, text,
  scroll, visibility, cursor/input/activity and operator body. ON/OFF during
  animation, power loss/restoration and reconnect.
  First verify the 21 October stationary base-PC infinite-loading report on
  both roles, including entering/leaving during boot and repeated power cycles.
- [ ] ZIP/cans (56/57): actual inserted/ejected ZIP class/model/scale on both
  peers; cans open/eat/final portion in hand/world, stow/re-equip, drop/pickup,
  simultaneous use and reconnect.
- [ ] Safe (43): dial/hacked completion, one loot result, persistent lock state,
  retry/stale-record handling and save/rejoin.
- [ ] Garden/sky (46/47): pre-session save/reload, care/harvest, simultaneous use
  and late join, pot state and accepted faucet/hose rerouting; shared schedule/
  effects for the black-disk/yellow-ring event.
  Missing historical garden save actors are not migrated.
- [ ] ATV (30/37): new companion spawn versus saved second-ATV positions,
  simultaneous carry/mount, stale menus and race convergence; steering visibility
  on remote copies and retained honk when another player joins.
- [ ] Disks/racks: client sv.request/hashcodes, floppy/ZIP transfers preserving
  exact text, filled rack save/rejoin and a slow joining peer.
- [ ] Whole-package smoke: swap host/client roles; two clients contest objects;
  late join/rejoin/disconnect/save/reload; partial/empty consumables, full inventory,
  equipped items/container rows, loose cables, flashlight crank/low battery/previews.

## Validation follow-ups

- [ ] Resolve the remaining public-prose counters recorded in TESTLOG:
  document count 85/82, src.comment_doc_row 155/0 and
  src.files_not_swept 84/0 with the reading-room checkouts absent.
  The latest fact-row gate passes (517 rows/166 files);
  older missing-reference/unused-row findings are superseded.

## Release preparation

Package 0.9.2158 is prepared locally with offline-test qualification.
Requalify changed source and build the next package through package/release.json,
package/check_current.ps1 and package/package_current.ps1. Native startup and
rendered multiplayer acceptance remain open; see TESTLOG for verification limits.

Commit, push, game installation and publication require an explicit request.
No other-agent workflow. Update the active tracking documents in place.
