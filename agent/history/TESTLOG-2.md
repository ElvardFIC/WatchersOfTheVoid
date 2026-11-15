# TestLog — current status and source reports - historical reports 2

[Current status](../TESTLOG.md).
## Development 2152 - entity category synchronization (19 October 2026)

### Grayboar body movement follow-up (19 October 2026)

Native grayboar_C is a prop whose ReceiveTick applies force to StaticMesh; its
spawned grayboarPawn_C is attached to the body and supplies AI/pathfinding. A
Character pose for that helper cannot replace the body's physical motion.

Added grayboar_sync: a class-scoped pre-body ReceiveTick watch claims the registered
host prop in prop_drive_host. The existing stream supplies transform interpolation,
stationary repair, join resend, hand arbitration and liveness cleanup. No second
entity identity or per-frame world scan is created. Missing authoring retries on
the next native tick. Client, stopped-session, mirror/peer-range and unregistered
bodies cannot author this host stream. Disconnect clears the feeder's session.
The inherited grayboar ReceiveTick also covers variants using the same body.

run_entity_sync.ps1 passes three actual production regressions (NPC orientation,
boar feeder, prop-drive sender/receiver) and eight production-unit compilations.
The old prop-drive harness received no-op collision-feeder doubles for its new
production dependency; collision behavior is outside this movement test. Native
checks confirm the tick's physical-force call and helper spawn/attachment. Full
Release DLL b2153 builds successfully. All 9 package/check_2152.ps1 runners pass;
updated DLL hash is in build/release-2152-logs/entity-validation.json.
No new wire layout change in this follow-up.

This completes the host body movement feed only. Client AI/helper coordination,
target choice, attacks, montages/audio, corpse/loot behavior and multiplayer physics
remain OPEN. Do not equate the new transform feed with a completed boar family.
Restored all 14 Ukrainian taxonomy labels previously damaged by shell-pipe encoding.

Owner requested the complete entity taxonomy below, including behavior/functions,
not just visible transforms. This request is OPEN. Existing allowlist membership
only establishes a transport path; it does not prove attack, interaction, audio,
local-player effects, late-join phase, or multiplayer gameplay correctness.

Implemented in this pass:
- Added the native EX_CallMath spawn sources ticker_fossilhoundSpawner_C and
  ticker_yellowWispSpawner_C. Clients already suppress those tickers; previously
  the host catch omitted their output, so their creatures could be missing remotely.
- NPC poses now carry/interpolate pitch and roll as well as yaw, with shortest arcs,
  first-pose/teleport snap and rotation-only updates. This fixes lost actor tilt,
  not creature-specific montage or child-component animation.
- Removed grayboar_C from the Character NPC list: the native class derives from
  prop_C and must never receive ACharacter mesh/movement offsets. Its separate
  grayboarPawn_C derives from Character. Body/helper AI synchronization remains OPEN.
- Native evidence confirms the zombie-deer, skeleton, bloody skeleton, SKerfur-O
  and zombie-Erie classes inherit the existing zombie family; cheese-slither inherits
  goreSlither. Their presence in a family does not verify their special functions.

EntityPose is now 52 bytes, capped at 26 entries (1376-byte datagram). The wire protocol is
2153 because old 2152 peers parse a different row size; both peers must update.
The task continues the 2152 source work; this is not a published release.
The published/prepared 2151 ZIP is not upgraded by these source changes.

Validation: run_entity_sync.ps1 passes the actual production NPC interpolation
regression (orientation, teleport, shortest arcs, stationary turns, MTU) and six
production-unit compilations. entity_sync_native_test.py passes native spawn-path,
class-layout and descendant evidence, including Character ancestry for all 25
current NPC allowlist roots. package/check_2152.ps1 passes all 9 runners.
Full MSVC Release build passed and DLL resources verify 0.9.0n b2153.
All 9 runners passed again with protocol 2153. Hash/validation evidence:
build/release-2152-logs/entity-validation.json. No ZIP or gameplay verification.
The existing build script/directory retain their historical 2152 names. Packaging
scripts still target release 2152 and must be updated before preparing a 2153 ZIP. Two-player gameplay
and all entity-specific effects remain unverified.

Full requested taxonomy and unresolved behavior coverage: WORK.md, "Complete requested entity coverage and remaining work".

## Development 2152 - wisp animation readiness

### 2152 follow-up: puppet hold iteration across native callbacks (19 October 2026)

Puppet hold Tick previously retained a vector iterator across socket/location calls.
Native callbacks could clear, erase or grow that vector, invalidating the iterator
and allowing a stale post-disconnect movement write. Tick now iterates a snapshot,
checks a hold revision after socket lookup and before continuing after movement,
and validates both actors with CachedObjRef. It rechecks the current puppet binding
before moving it. Peer leave/re-engage invalidates the old iteration even for the
same slot and wisp. Invalid peer slots are refused. Disconnect clears puppet holds
before native self-release callbacks so callback-installed state is not erased later.

run_wisp_animation.ps1 passes the three regression executables and compilation of
all four production units. Actual hold tests cover socket-time disconnect, peer
leave/re-engage, puppet/wisp recycling, puppet replacement, movement-time teardown
and vector growth, normal multi-hold drive, invalid slots and stale hold removal.
Gameplay remains unverified; no DLL/ZIP, game installation or publication performed.

### 2152 follow-up: bind delayed grab actions to the original pawn

The death arm previously captured Local only at its deadline, so replacing the
pawn earlier in the grab window could kill the successor. OnWispGrab now stamps
the victim's CachedObjRef at receipt, before native attachment callbacks. Tick
consumes that reference with the deadline and revalidates identity after release.
Disconnect clears the reference. Invalid wisp/victim IDs and absent local pawns
cannot arm a death. Hold retries likewise retain their initial victim rather than
attaching whichever pawn Local names on a later frame.

run_wisp_animation.ps1 passes all three regression executables and compilation of
four production units. Added cases exercise replacement and slot recycling before
the death deadline, invalid/absent-victim grab rejection, disconnect reference
cleanup, pending-attach replacement/recycling rejection, and normal delayed attach.
Host/client gameplay remains pending. No new DLL/ZIP, installation or publication.

### 2152 follow-up: hold victim identity and attachment cancellation

ReleaseSelf now retains the actual attached pawn through CachedObjRef instead of
resolving Local again at cleanup time. A replacement pawn receives no detach or
movement reset; the original pawn is released while its slot/world remain valid.
A recycled original slot is rejected. Attachment attempts also compare their hold
generation and victim identity after native callbacks, so disconnect or a newly
installed pending hold cannot be overwritten by the old attempt's success.

run_wisp_animation.ps1 passes all three regressions and compiles all four production
units. The actual hold regression covers original-pawn cleanup after Local changes
or disappears, recycled slots, disconnect during initial/retry attachment, and
reentrant replacement with both attached and pending holds. Gameplay remains
unverified; no 2152 DLL/ZIP was prepared and the 2151 package is unchanged.

### 2152 follow-up: release-time death cancellation and reentrant cleanup

The due-death Tick previously continued after ReleaseSelf even if native release
callbacks disconnected the session or replaced the local pawn. It now consumes the
old death arm before release, captures the intended victim's CachedObjRef, and checks
the session epoch, pawn identity and slot serial after release. A successor pawn
cannot receive that pending kill. An absent/dead victim still gets hold cleanup.
OnDisconnect clears its death arm before invoking release callbacks.

ReleaseSelf now clears hold state before calling native detach/movement cleanup.
Nested teardown is a no-op for the consumed hold, and a callback's newly installed
hold is not wiped out when the outer release returns.

run_wisp_animation.ps1 passes three regression executables: receiver lifecycle,
actual native montage wrapper, and actual hold-release implementation. New cases
cover release-triggered disconnect, pawn replacement and recycling, one-shot normal
kill, absent victim cleanup, recursive teardown and new-hold preservation. Four
production units compile (tear mirror, grab hold, attack sync, native wisp wrapper).
No live gameplay verification or 2152 package; prepared 2151 ZIP remains unchanged.


Additional offline result: actual wisp montage wrapper regression passes refused,
invalid or unreadable playback results, failed parameters/dispatch, callback recycling
of actor/AnimInstance, recovery and optional-section absence. Receiver regression also
passes; all three related production units compile via run_wisp_animation.ps1.
The native call's dispatch status is no longer confused with accepted playback.
The player-reported wisp animation still needs host/client gameplay confirmation.

The owner continued bug fixing in 2152 after packaging 2151. Current source protocol
is 2152; the prepared delivery ZIP remains 0.9.2151 and is not replaced.

A live wisp actor did not guarantee a ready AnimInstance or montage. The receiver
previously consumed WispTear even when PlayFatalityMontage returned false, permanently
losing capture animation. PlayTearOnWisp now reports failure. Pending attacks retry
at 100ms intervals within the original 3.5s window; duplicate pending messages neither
bypass backoff nor extend the deadline. Puppet socket holding begins as soon as the
actor exists, independently of animation readiness. Host-local attacks now use the
same receiver/retry path. Actor lifetime and disconnect epoch are checked after calls.

run_wisp_animation.ps1 passes animation-not-ready then recovery, throttling, unchanged
deadline, expiry, no replay after success, delayed actor, sender/slot rejection and
reentrant disconnect both on queued and immediate failed playback. Both production
units (wisp_tear_mirror.cpp, wisp_attack_sync.cpp) compile with MSVC. This is a focused
offline check; live host/client animation remains unverified. No 2152 DLL/ZIP, game
installation, commit/push or publication has been performed.


## Release 2151 preparation - 18 October 2026

Current release/protocol: 2151; both peers must update. User reports were from build
2050; missing model names are unknown. The current changes cover personal horrors,
ragdoll retry/rig/pelvis/get-up handling, partial NPC class-cache installation and
first-spawn matching, deferred wisp capture and non-chair capture pose, host-authoritative
Cremator door/paint-only lever with cached searches, held physgun/gravgun cosmetic
replication, recursive custom-skin discovery and failed-preview retry/hiding.
No claim that unknown missing assets or static preview mappings are fixed.
Seven player-report regression cases, ragdoll retry, ragdoll skin, personal sleep and
two native-evidence checks pass (12 focused checks). Clean Release build and ZIP
verification evidence: package/RELEASE_2151.md and build/release-2151-logs/.
Gameplay reports remain open pending two-player testing, including measured door FPS.
No game installation or online publication is performed during preparation.


Updated 18 October 2026. All accumulated source work through development batch 2192
is retained in release/protocol 2151 at the owner's request. Historical numbers
below identify work batches and prior observations. The complete open backlog is
[WORK.md](../WORK.md); packaging does not close any unverified gameplay report.

Read [TODO.md](../TODO.md) for current priorities and release restrictions. This file
combines current issue statuses with the original user report snapshot. Implemented
or locally checked does not mean multiplayer verified. Historical summaries are
removed with historical archives during the owner-approved cleanup.

| # | TestLog item | Current status |
|---|---|---|
|1|Client cannot break props|Код готов2143, дополнительная защита2159: FIFO повторов ударов и полная проверка границ параметров; needs new-build gameplay result|
|2|Prop jitter/rollback|2153 item batch: lowest-slot hold arbitration includes the local handle/physgun/gravity target; refused holds cannot apply a late release, and cached drives validate actor slot serials. Native yield and motion regressions pass. Initial grab remains optimistic until peer poses arrive; multiplayer jitter verification remains open.|
|3|Pushing one prop with another|2153 item batch: verified hitCpp feeder resolves nested component owners (bounded 64 hops), preserves deferred chained push/coast and excludes static/frozen/held/trash targets. Contact and host/receiver regressions pass; real PhysX contacts and two-player convergence remain unverified.|
|4|Client R+LMB duplication|Код готов2143; needs gameplay result|
|5|Selected-model ragdoll|Reopened: owner still reports incorrect ragdoll on2168 (report50). Preserve2144 code, but do not treat this behavior as fixed.|
|6|Mining farm|Reported working|
|7|Extinguisher foam|Код готов2145|
|8|Loose power outlet floating|Код готов2145|
|9|Physics gun / both gun effects|2153 item batch: physgun target/component identity checked; dedicated ATV/hinge channels can see non-prop targets without casting them as props. Native weapon-release cleanup, remote beam/audio effects and host-validated live root scaling implemented/tested. New kind 202 requires matching current DLLs. Skeletal bone scaling/arbitrary creature physics are not implemented by the prop lane; actual freeze/scale/contested-control gameplay remains open.|
|10|Nightmares|Previously reported working. 2151 requested behavior: only the player whose native sleep/enabled/bed/chance conditions win enters a nightmare. Shared acceleration ends via SleepState; host scene is no longer forced on other peers. Native checks/regressions/compile pass; gameplay pending.|
|11|Rotating chair|Код готов2144|
|12|Console commands|Частично2154/2155: общий ввод/ответы/ошибки/clr и недавняя история при позднем входе; полные события, местные эффекты и единый порядок одновременного ввода остаются|
|13|Funny rule music|Частично2154/2155/2157: Madness music/HUD и музыка семейства Maxwell в мире/выбранной руке; точная фаза трека и прочие события остаются|
|14|Funny rule display|Частично2154: Madness HUD; прочие события остаются|
|15|Toy sounds|Частично2156/2157: Maxwell squeak/pitch world and selected hand; abplush Alt-use world/hand; ordered retries. Other toys and squeeze gameplay remain; needs gameplay test|
|16|Stutters when stowing items|Частично2157: removed full object-array lookups of unloaded claimed classes from save-coverage/pocket path, serial-safe cache. No frame profile or in-game confirmation; residual stutters remain unconfirmed.2165 follow-up removes recurring unresolved-class full walks in sleep resolver; production compile passed, no runtime profiling|
|17|Pressure-washer stream|Код готов2149; visual/audio only, impacts remain with water work|
|18|Computer sparks/damage presentation|Код готов2150 for native water shortCircuit; needs gameplay result|
|19|Computer shutdown from client damage|Код готов2150 for native water shortCircuit; ordinary addDamage is empty in this cooked class|
|20|Bucket water disappearing|Частично2153/2159/2169: world changed-state publication/empty repair/passive simulation;2159 adds remote selected-hand level/soap-presence/alcohol display, preserving owner's actual contents.2169 adds current held water/actual soap count/alcohol to the personal profile, with validated ordered receipt. Simultaneous writers/in-flight record arbitration and pouring gameplay remain; gameplay needed|
|21|Cement / trowel|Частично2153/2160/2166/2168/2169: trowel hand display; world units/dry and two-bag transfer publication, passive tick, exhausted-bucket guard;2166 adds concrete-bucket hand fill/dry/empty display and no-spend client wall-repair refusal. 2168 refreshes outgoing held-trowel profile concrete from the matching local hand, including zero; identity mismatches defer the poll. 2169 adds live held concrete-bucket/cement-bag profile amounts/dry and validated ordered profile receipt. Concurrent host transactions, dryTimer handoff and actual client wall repair remain; gameplay needed|
|22|Brick-wall outline and build|Partial2166/2167/2168: placement/progress, existing-wall enrollment, solo-host tracking and native-key save twins;2168 adds host-validated client melee damage to completed walls. Five focused2168 tests/four production compilations pass. Client repair, atomic material transactions, creative build and effects remain; no multiplayer test.|
|23|Depleted flashlight still lit remotely|Код готов2145; compared with archive2151, retained and regression passed|
|24|Client cannot pick up anchored hook|Код готов2152: host retires keyed anchor, creates one ordinary pickup near requesting client; client then picks it up normally (not an automatic inventory grant). Includes saved wall anchors, retries and late-join retirement. Gameplay pending|
|25|Battery levels|2153 item batch: personal profiles capture the live held record before hashing (including zero charge) with class/key/serial/world validation. Supported mutable world pickup waits for reliable record acceptance before destroying its source. Capture/transition regressions and Release compile pass; battery hand/stow/drop/rejoin gameplay remains open.|
|26|Airy event|Осталось|
|27|Main-room computer walls becoming physics props|2153 item batch: seven factory-born cubicle parts and their post-finish state/coast are covered by the production birth harness; source registration remains independent of optional ambient assets. Actual struck-panel disappearance on host/client remains a gameplay check, not a closed report.|
|28|Drone sound difference|Код готов2143|
|29|Crowbar static state / dragging|2153 item batch: unstick preserves live local handle/gravity/hotbar ownership even without a localPlayer argument. Native refusal, non-component static flags, retry, recycle/world and callback tests pass. Unknown parent/replacement subclasses still require individual native/gameplay evidence.|
|30|Second ATV in corridor|Код готов2149 for new companion creation; old saved positions unchanged|
|31|Sleeping body pose|Source implemented in continued 2153: selected-model poseable body, mattress/moving-anchor and sleep/ragdoll/sit precedence; regression checks pass. Verify both peers, effects suppression and each rig in game.|
|32|Not enough space on equipment|Код готов2146 for ALL equippable items|
|33|Open container retains removed row|Код готов2146|
|34|Client crate/package loot invisible|2153 item batch: inherited MRE/AB route audited; exact personal foodBox final-bite wrapper birth admitted, while world wrapper output is host-owned. Final food destruction cannot overtake its bite request; post-finish host birth/state tests pass. Other unexamined package families and two-player loot appearance remain open.|
|35|Centipede / other living creatures invisible|Осталось|
|36|Computer chair rotation (duplicate report)|Same as11,2144|
|37|Riding a carried ATV / flight|Код готов2148/2149; actual menu and race convergence included|
|38|Skin model list / previews|Частично2151/2152: original sixteen preview tiles plus supplied Mynet/skeleton images; both requested meshes were already builtins. Catalog regression passed; runtime appearance/animation/ragdoll still needs gameplay, other preview gaps remain|
|39|Consumables refill on pickup/drop|2153 item batch: current held native record is captured for profiles, supported world pickup waits for record enqueue, and host food samples survive a missing world actor. Final-bite identity is captured before destruction with durable send retry; verified box output is host-owned and eater effects stay native/local. Food/consumable transition regressions pass; gameplay remains open.|
|40|Flesh Eerie summon / visibility / aggro|Осталось|
|41|Secret basement wall|Исключено по прямой просьбе пользователя; не возвращаться к исправлению|
|42|Event teleport to the triggering/base-entering player|Код2161: actual agrav occupancy and runtime-verified shared armed boxes; client pose fallback, initiator gather, repeat/reentry guards and send retries. Two focused checks/five TUs passed; multiplayer pending. Other custom paths and full event effects remain separate|
|43|Safe cracking|Код2162: both dial success and hacked converge on opened; client completion to host, single loot, saved lock publication/retries and host-lock protection against early/stale records. Initial unlocked drop record preserved. Gameplay pending; dial UI and generic child hinge identity unchanged|
|44|Sponge wetting|Частично2160/2169: native saved power/wetness now publishes after enterWater/wash/clamp/LMB; load no echo and final-use tail checked. 2169 adds current held wetness to outgoing profiles. Soap stock/foam/surface effects and personal-hand transitions remain unverified; two-player test needed|
|45|Grill ignition|Код2162: host ignition/off, active/coals/burn snapshot and late join, passive client heating/fuel tick, immediate coal record and request retries. Gameplay pending. Concurrent coal transactions/stale records, lid and personal item transitions remain.|
|46|Client-placed crops/faucets/hoses disappear after save reload|Merged source now includes pre-session saved-actor enrollment, care/harvest/pot state, accepted hose rerouting and canonical updates; garden/hose regressions pass. Verify actual save/reload, late join and simultaneous use. Missing historical actors are not migrated.|

## Additional user reports (15 October2026; OLD released versions)

|ID|Report|Status|
|---|---|---|
|47|Black disk/yellow-ring sky event starts on client independently|Visual code2164; common schedule-entry authority2165 covers all scheduled rows, boot/reflected calls and passEvents protection. Local checks pass; gameplay pending. Audio/emails/pre-session latent effects remain separate. This is NOT Bad Sun or black fog|
|48|Mimic (user identified; trees/white pillars/radar report)|Code2165: prop_impostor_C and impostorPropsMaster_C identified; client random generation and local-camera culling disabled. Reuses keyed prop class/disguise/birth/death/join; host controls lifetime. Focused checks/compilation pass; multiplayer/rendering/damage verification pending. Related to35; other creatures remain open|

## Additional user reports — 16 October 2026

Owner confirmed all ten reports were observed on build2168. They remain open current observations against2168; earlier related code is not gameplay confirmation. This workspace has not produced or inspected that2168 package, so its last locally verified package remains2146. Do not dismiss the reports as pre-fix behavior.

|ID|Report|Status|
|---|---|---|
|49|Insomnia demon neither appears for nor attacks the client|2151 code ready: native peer-local sleep-dependent spawn/pursuit/grab restored; removed ticker refusal and pose-only NPC mirroring. Native bytecode check and focused C++ compilation pass. Requires host/client gameplay: low/rested sleep, grab/dream and reconnect. Distinct from nightmares10 and mimic48.|
|50|Ragdoll display is still incorrect|2151 partial: spawn/lost-body retry, pelvis correction and model-authored physics selection added. Valid own rigs may use a different skeleton; common fallback retained, bone/pelvis/joint and stale-asset guards tested. Focused regressions and compile pass. T-pose in gameplay, local native body and exact limbs remain open; revisit5/38.|
|51|Client cannot plug electrical appliances into outlets|Partial2170: socket-endpoint authority fixes refusal at the far end of long cables; native length/occupancy and callback lifetime guarded. Client connect/graph parameter-frame bounds now fail closed. Two focused executables, native-bytecode check and two TUs pass. Report remains open for actual2168 symptom/gameplay verification; nearby failures may have another cause. Distinct from loose-outlet floating8.|
|52|Client token display remains desynchronized|Open: owner still cannot identify the token counter/UI (21 October). Do not assume credits or a specific token class.|
|53|Base light flickering is not synchronized|Partial2177–2180: host-authored ceiling/ambient phases; indexed clients suppress native writes; local ambient settings remain local. Failed applies retain latest state;2180 retries failed/unreadable join snapshots without advancing the all-peer baseline, with peer/world cleanup. Channel/wrapper regressions, prior native checks and five production TUs pass. Actual keys, loading/late join and two-player timing need gameplay verification.|
|54|Computer display does not show the same activity to host/client|SharedScreen and merged LaptopUI/operator code is present; wire/lifecycle/holder/widget regressions pass. Verify actual laptop/portable-PC tabs, text, scrolling, cursor, activity and operator visibility in game. Console text12 alone is insufficient.|
|55|Computer power-on and power-off are not synchronized|Partial2173: targets survive actor loading and native animation; failed sends and deferred join snapshots retry with latest state. Pending sends clear on peer departure/disconnect; 2171 echo suppression is preserved. Existing animation regression, new delivery/lifecycle regression and two production TUs pass. Still requires two-player checks, power loss/restoration and late join; no release built. Separate from water short-circuit/damage18/19.|
|56|ZIP disk model sometimes becomes a huge owo when ejected|Partial2172: laptop `zip` is now resolved as a masked Unreal bool bitfield, written without disturbing neighboring flags, and hashed as its logical value. Focused wrapper regression and production TU compile pass. This is a concrete wrapper defect but is not proven to cause the reported model/eject symptom; still needs in-game ZIP eject and multiplayer checks.|
|57|Opening canned food and eating it are not synchronized|Partial2176: world open/eat publish records (2174), held can state travels with HandItem (2175), and client profiles refresh live opened/type/foods before hashing (2176). Identity-checked copied records preserve unrelated data. Three profile/record regressions, native evidence and two production TUs pass. Protocol2175. World/held use, reconnect, final portion, stow/re-equip, drop/pickup and simultaneous consumption still need multiplayer verification.|
|58|Some items disappear when placed on the workbench|Open symptom; partial code2181–2186. Owner clarified17 October: simple placement, visible disappearance after1–2 seconds or later; no crafting reported. Owner identified an old rifle and components in an isolated case on21 October; exact component and authoring role remain unknown. Native rifle candidate is prop_rifle_C. Transform retries, local-birth protection and deletion identity rechecks implemented;2186 invalidates old sweep passes on bracket/world/reset changes and blocks recursive reconcile ticks. Focused tests/production compilation pass. Full sweep/gameplay symptom remains unverified.|

Related deletion-path fix2187: pile watches/pending state are now cleared only at final deletion, not provisional sweep selection. This prevents losing state for candidates later kept by completeness checks. Production compile and existing candidate regressions pass; no full pile/sweep gameplay test. This does not close report58.
Related record-delivery fixes2188–2192: failed captures retry; unindexed newborns retain validated source identity; retries are per recipient/broadcast; loading waits preserve attempts; successful ordinary sends now cancel only their satisfied older retry. Real record-queue regressions and production compile pass; not a proven cause/fix of report58.

Preserve this TestLog with the source changes. Do not mark remaining rows done solely
because a related older patch exists or a standalone compile passes.

## Original user report snapshot

Source: `E:/TestLog(eng).md`, read on 14 October 2026. Original report wording is
preserved below as evidence; it is not proof that an issue remains unfixed today.
The first 40 reports map to tracker IDs 1–40 above, in order.

The client cannot break props; hits register endlessly without the objects breaking on client side, on host's side hits do not register. (appeared in 2142RBE)

Props move erratically for the client; they seem to teleport back briefly, resembling a high-ping issue, and movement is delayed.

Inconsistencies occur when moving one object using another.

When a player throws an object using R+LMB, the object may duplicate (e.g., the signal box lid duplicates). (appeared in 2142RBE)

Ragdoll physics interact poorly with the model, though the model itself is visible; the lack of model "bones" causes the T-pose to drag along the floor during ragdoll states.

The mining farm functions correctly.

The fire extinguisher emits foam only for the user, yet the supply depletes for everyone.

If another player moves an unconnected power outlet, it remains floating in mid-air for everyone else.

The physics gun is unsynchronized while the gravity gun is synchronized; neither displays visual effects when viewed from another player's perspective.

Nightmares function correctly.

The chair rotates only for the player sitting in it; for others, the player spins in a seated pose while the chair itself remains stationary.

Console commands entered by one player do not synchronize across other players' consoles.

When the client entered a "funny rule" command, the music played only for the host; nothing happened for the client.

The "funny rule" display appears only for the host.

When using sound-emitting toys, the sound plays only for the user.

Micro-stutters still occur when putting an item away.

The stream from the pressure washer is visible only to the person using it.

Sparks and signs of the main computer breaking appear only for the person damaging it (sparks were seen only by the person spraying water at the computer).

The main computer doesn't shut down until the host breaks it.

Buckets are buggy; water disappears from the bucket if someone other than the host pours it in.

Cement is consumed from the bucket, but nothing appears on the trowel; it only appears for the person applying the cement to the trowel.

The outline for a brick wall is visible only to the person who placed it, and the wall itself is visible only to the person who built it.

When a friend's flashlight runs out of power, the host still sees it working.

When a grappling hook is attached to a wall, the friend cannot pick it up, but the host can.

Battery levels aren't synchronized; for example, the host might see 0.0% while the friend sees 100%, but once the friend touches the battery, it jumps to 100% for the host as well.

The Airy toy event isn't synchronized for the friend; while the host sees the animation and hears the sounds, the friend sees nothing—the only thing that appeared during the event was a trash bag, and the Airy toy spawned for the friend only after some time had passed.

computer walls in the main room after changing into the physic objects are disapearing from on client side.

sounds of the drone are different on client side. (appeared in 2142RBE)

Static objects pulled out with a crowbar do not synchronize; their static state changes only for the player who pulled them out, while their position updates via teleportation rather than smooth movement when dragged.

The second ATV spawns not in the garage, but in the room with the portrait of Stolas (the main corridor).

What if we tried adding a sleeping animation? I mean, flipping that white model over and placing it on the mattress.

There’s a fanny pack that increases inventory space. I bought two of them, but when my friend tries to equip one, he gets an "not enough space" message, same with backpacks, might be with other wearables.

Basically, when we Both open the same container and one of us takes an item out, the item remains visible to the other person—but if they try to take it, the game says there isn't enough space.

If my friend (not the host) breaks a wooden box / opens a package with food, I won’t be able to see the items falling out of them until he touches them.

When a centipede approached us, I could see it, but my friend couldn’t., its also a problem between all kinds of "alive" creatures.

If I or my friend sits on a chair near the computer (black, on which you can spin) That is, if it starts spinning, it won't be visible.

If I get on the ATV that my friend is holding, I’ll start moving through the air, and as soon as I get off, the ATV will be in his hands.

Skin menu has not every creature models init and has no preview image on it (i have images so when we start fixing it, ill send you them).

As far as we can tell, all interactable items—such as batteries, lighters, food (which you’ve already fixed), and fire extinguishers—aren't synchronizing correctly. If a friend picks one up and then puts it back down, it becomes full again, just like what used to happen with the food.

Regarding the Flesh Eerie: if a friend lights the summoning item, the boss doesn't appear for them—there are no sounds and no boss at all, except for the rockets hitting the Eerie; plus, the boss only aggros onto the host.

## Fork review ? 19 October 2026

Release 2153 build PASS; check_2152.ps1 PASS (9 runners); disk_utf8_test.py PASS (production Windows conversion and 16/32-bit string framing). git diff --check PASS. No gameplay run: client hashcodes, disk transfer, rack persistence and slow join remain unverified. Distribution ZIP unchanged. Sources/selection rationale: WORK.md, Fork and upstream review.

## Artifact cleanup verification - 19 October 2026

Approved inventory removed: 10,030 files / 885,751,603 bytes (844.7 MiB gross).
Current package SHA-256 unchanged; current DLL and dependency cache present.
Post-cleanup check_2152.ps1 PASS, all nine runners. Running them regenerates their
current test outputs. git diff --check PASS. No game runtime changes in cleanup.
Extra historical run_2141.ps1 check cannot compile: its fire_stubs Session lacks
SetLocalPropPose used by current cord_sync.cpp. This is an existing stale test API,
not a deleted input. Historical source-baseline comparison was removed; current
performance cases and mutation controls remain pending stub maintenance.

## Item batch validation - continued 2153

The eight selected reports have source fixes/regression coverage recorded in WORK.md,
Item synchronization batch. run_item_batch.ps1 PASS; all nine check_2152 runners PASS;
Release b2153, zero-UE4SS-import ABI and reliable-kind routing PASS. New harnesses
exercise live held records, native yield boundaries, scale validation/retry/reentry
and existing host/receiver motion. Final-bite tests cover native destruction before
POST, suppression of generic destroy, refused-send retry and no duplicate request.
Native evidence verifies the foodBox output has no player effects and the physgun
release input reaches real cleanup (its release() event alone is empty).
No two-player gameplay result or new distribution ZIP. New kind 202 requires identical
current DLLs despite retaining development protocol 2153. See build/item-batch-check/verification.json.

## Shared computer, ragdoll and sleeping body - 19 October 2026

Release `main.dll` b2153 built successfully after the changes. Focused regressions
passed: `run_shared_screen.ps1` (wire bounds/Unicode, holder trust, handoff, late join
and display retry), `run_ragdoll_retry.ps1`, `run_ragdoll_skin.ps1` and
`run_sleep_personal.ps1`. `git diff --check` passed for the changed paths. These tests
do not replace a two-peer in-game check of laptop/portable-PC UI, selected-rig ragdoll
entry/recovery, and bed/ragdoll/chair transitions. Distribution ZIP unchanged.

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
