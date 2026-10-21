# Watchers of the Void 0.9.2155

## 0.9.2155

- Revalidate a prop's original object identity, world and session-start serial
  throughout received destruction, local consumption and conversion cleanup. Stop
  after native metadata, drive release, grab release or unpin callbacks replace the
  actor. Reject recursive destruction of the same incarnation and snapshot destroy
  listeners so registration during a callback cannot invalidate iteration.
  Mark incoming destruction only after the final cancellation check, preserving
  future genuine destruction when an attempt is aborted.
- Add received-destroy diagnostics with role, sender, actor, class, key, element
  ID and world to correlate the host's disappearing old-rifle report. The report
  remains open; the new regression reproduces a receiver lifetime defect, not the
  owner's in-game disappearance.
- Reset location-event state when a session restarts at the same address in the
  same world. Revalidate native player-pose reads and event gathers after teardown
  or world replacement, and prevent recursive location-event ticks.

- Integrate all production improvements from `watchers-of-the-void-fish`, adapted
  to the current main branch's Entity, Kavotia, event and personal-sleep controllers.
- Deliver the host's chosen scheduled and repeatable special events, including
  nested outcomes, with recipient-specific retries. Harden client replay against
  recursive callbacks, disconnects, world/session replacement and queue invalidation.
  Retry incomplete native-call preparation; consume attempted calls whose completion
  is unknown to avoid repeating partial effects. Bound replay work per pump.
- Retain active-event join snapshots while the native registry or transport is
  unavailable. Copy registry membership before sending, validate sender identities
  and recipient generations, and stop delivery after teardown or world changes.
- Add a live, replicated event-frequency setting, default 2x and adjustable from
  0.5x to 5x. Scale the native waits of Fossilhound, killer-wisp, sky-wisp and
  night-egg generators on the host, plus each owner's night-eyes generator.
  Preserve native eligibility, night/day checks, chance rolls, story dates and
  personal dreams/Insomniac. Other repeatable generators remain outside this setting.
- Extend Kavotia stance, aim and charge snapshots, Grayboar reliable actions/turns,
  Ariral and wisp lifetimes, and personal-sleep handling. These routes still need
  full effect/phase and multiplayer acceptance.
- Add shared computer display/operator handoff and readiness recovery. Improve the
  selected-model sleeping body, moving-bed/mattress alignment and sleep/laptop/
  ragdoll transitions, with animation warmup, articulated-rig checks and recovery.
- Use numbered immutable placement requests, explicit accepted/refused results,
  retained replies and bounded backpressure. Refuse existing or reserved keys;
  publish saved state only after acceptance. Preserve food condition and cooking
  progress through placement. If admitted client placement exhausts readiness or
  send retries, retain its local item for pickup and protect it from join cleanup
  and competing spawn admission. Add request/class/key/world and cleanup diagnostics.
- Reserve coin pickups before native credit across E, overlap and remote requests,
  including actors without an element ID. Prevent duplicate credit after uncertain
  native completion and retry removal of credited coins. Keep client map coins personal.
- Carry cooking progress through world, hand, placement, profiles and saved records;
  retry actor readiness and scope personal cooking births to their actual factory.
  Route banana-peel and empty-cup outputs through the host.
- Persist wet cement, wall drying and sponge soap state; replicate cosmetic sponge
  foam without spending twice. Validate creative construction. Persist held-trowel
  concrete changes into the matching native `hold[0]` record before reconstruction,
  including zero stock, with exact class/key/slot/world checks. Identity mismatches
  still defer persistence; ordinary paid wall repair remains disabled pending an
  atomic material transaction.
- Give the host native bucket/washer water shots, drink spills and gasoline flight,
  collision/fire and canonical decals. Deliver source audio and replica effects with
  exact personal-hand provenance and lifetime checks. Keep private computer-game
  explosions local.
- Synchronize grime/window cleaning, final wipes, quiet join snapshots and retained
  delivery. Track explosion and salt-rocket scorch decals and their retirement.
- Order grill lid and coal edits independently; synchronize native burning, audio
  and held-grill state. Preserve host burn/active state against stale client records.
- Order console submitted, periodic and clear output/history on the host. Recover
  Madness audio/HUD playback, Maxwell phase/speed/audio and Watoy world/hand state.
- Bind garden care, faucet and hose/detach actions to their invocation, player and
  generation. Capture current held state before profile hashing; cancel stale
  chunked profile sends. Strengthen held-item identity and laptop world-ready handling.
- Reconcile message IDs, layouts, authority, dispatch and lifecycle registration
  across both branches. Keep Kavotia=223 and placement-result=224; assign Watoy=235.
  Placement requests/replies are 188/101 bytes. All peers require this exact DLL.
- Expand offline qualification to 65 groups, including event replay/lifetime,
  owner-local frequency, native night-eyes bytecode, trowel persistence and placement
  exhaustion, received-destroy identity and location-event reentry. Verify the stable production snapshot, Release identity, loader ABI,
  224 reliable message kinds and 18 routing fault controls.

Package **0.9.2155** targets **Voices of the Void 0.9.0n** and contains
**DLL b2178 / network protocol 2178**. All players must update together.
Release qualification and scope are recorded in RELEASE_2155.md.

The complete eight-category event request, the full repeatable random-event list,
and real-peer event/frequency/join/reconnect acceptance remain open. The fresh
game-pak/Blueprint audit remains deferred. Bench58 is now confirmed as an old rifle
disappearing after placement by the host; its cause remains unverified, and the
client placement recovery fix does not establish a host-rifle fix. Token52's UI is
still unidentified; no token or money balance was changed for that report.
Paid cement repair, exact stock/key continuity in gameplay, general resource
debit/refund, remaining grime sources and scorch provenance through full world
reload remain unfinished. Full public-prose and fact-row CI gates retain known
failures; 65 passing offline groups do not mean all repository CI or gameplay passes.

**AquariumFishTeam**

HELLO FELLOW astronomicans with thousands of hour of studying astronomics. so straight to the point, we synced MANY STUFF lie computer and laptops, gardening, FKAHH BOARS, gardening again, sounds from different events that wasnt working properly before, building and stuff connected to it. stuff goes well and we have not finished the whole list of bugs yet... BUT WE ARE ON IT, tuned guys, have fun
