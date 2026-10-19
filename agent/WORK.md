# Unified Work — development protocol 2153 (continued 2152); packaged release 2152

Current owner priority: expand synchronized events/interactions; bug fixes are
secondary unless required for the chosen synchronization path. Grayboar work is
deferred. The current candidate list and tracker IDs are in TODO.md. The coverage
table below preserves unmapped entities and partial behaviors as open work.

## Continued 2153: shared PC/laptop pages and cart, faucet and detached sprinkler (19 October 2026)

Owner clarification: Airy means the Live Erie resurrection ritual; Meat Erie is
the separate npc_angryErieFlesh boss encounter. Both remain behavior work, with
their actual cooked classes located. Screen 54 means the base PC and portable
laptop interface, including tabs and the shop cart. Token 52 remains unmapped.

New laptop_ui_sync and engine-only laptop_ui wrapper share the main/media/viewer
page indices and ordered cart row names. Only the exclusive laptop holder may
submit. Host grant/release serials become lease tokens, including same-slot
release/reclaim between UI polls. Host validates the live catalog, item count
and native capacity; prices never travel. New operators seed from the canonical
state. Echoes for an existing owner lease cannot erase newer local changes.
Per-peer latest-state queues retry independently; the final local edit enters
Normal before DeviceClaim release (release waits through queue backpressure).
Loading widget applies remain pending without blocking delivery to other peers.
The reliable channel is kind 202, no blind relay, protocol still 2153.

Native application changes only UWidgetSwitcher pages. It never invokes the
camera-possession createRC/leaveRC path or makeAnOrder. Cart changes remove/add
only the differing suffix, prevalidate all dependencies and row frames before
mutation, stamp catalog keys and rebuild native indices. State readback confirms
the dispatch had its effect. An older unnamed refused-order cart can be replaced;
future refused-order restores now stamp their catalog keys as well. Search,
category, detail windows, cursor, media playback and visible operator remain open.

New garden_actions adds ordered PlacedSave opcode 9: faucet action 5 and native
hose detachSprinkler. Host validates source/peer/sequence, family, live identity,
reach and expected endpoint. Detach uses the far end of the native path for
reach, so long hoses do not require standing at their root. Pointer directions
and saved keys clear together. A stale request cannot detach a newer sprinkler.
Client native faucet body is suppressed; host sound publishes EventCue ID 5
(door0_open, volume .25, pitch 1), observed from the actual completed native
call. Native host detach also uses the saved-key cleanup wrapper. Moving-hose
detach remains part of the completed-route gesture. Actual sprinkler pickup,
poured watering, pot transitions and durable care/refunds remain separate work.

The private hose-edit implementation moved to placed_save_hose.inc, keeping
placed_save_sync.cpp under 800 lines. No grayboar changes were added in this step.

Build environment restored locally from the owner's existing E:/MyMods/MultiVoid+.zip:
CMake/Ninja, vcpkg scripts and pinned installed packages, with native MSVC/SDK.
build/restore_build_inputs.py reproduces extraction without touching source or
the installed game. Full Release target votv-coop builds to
build/release-2153/main.dll, file/product version 0.9.0n b2153. CMake configuration
uses Release try-compile and /Z7 for Debug checks to avoid the installed PDB
manager mismatch. Configuration: build/configure-2153.log; final build log:
build/build-2153-final.log; completed link: build/build-2153-incremental.log.
The sandboxed link stalled with the output open; final linking outside the
sandbox completed successfully. Ninja dry-run reports no remaining work.
The current AMD64 PE is complete, exports start_mod/uninstall_mod and passes
abi_gate --drill with no UE4SS imports. DLL SHA-256:
f32c974fdd2ec0ea9e2c9f6f9648134b49d8600b547ccc816865f97fc513d615.
build/sync-verification.json records source/log hashes, 14 executable checks,
native contracts and the DLL. Two-player gameplay remains untested.

PASS run_laptop_ui.ps1 -CompileSources (two executables, five production units),
run_garden_sync.ps1 -CompileSources (seven/four), run_world_sound.ps1 -CompileSources
(one/three), native cooked-asset contracts and reliablekind_gate --drill (191
kinds, all seven injected controls fire). Logs: build/laptop-ui-validation.log,
build/garden-final-validation.log, build/sound-final-validation.log,
build/sync-native-contract.json, build/reliablekind-ui.log. Source regression
coverage is not proof of the remaining candidate table or live gameplay.

## Continued 2153: completed guest hose rerouting (19 October 2026)

An accepted guest hose now keeps an edit reservation from native resume until
finish and explicit host acknowledgement. Canonical State received during the
gesture is retained without interrupting native placement. The completed route
and its original canonical fingerprint use PlacedSave HoseRoute; the nonce and
proposal remain unchanged across partial sends and transport backpressure.
Only world points and the sprinkler endpoint are editable; the canonical root,
faucet key and first point are retained. Pinning the root also avoids rejecting
native quaternion round-trip noise as an attempted actor move.

The host requires a known live hose, matching baseline/root, a completed native
state, the current peer, endpoint reach and available native endpoint owners.
Pending route endpoints are reserved before native linkage, so two hoses cannot
claim the same sprinkler between ticks. Rejected operation IDs stay rejected;
duplicates receive the actual canonical state without replaying loadData.
HoseReply is targeted to its requester and waits for native geometry/link work.
Each peer has an independent bounded reply queue; blocked heads rotate. Mid-edit
host changes win a conflict. Host action/resume cannot interrupt a pending link.

Native processKeys/finish rebuild the spline by destroying old components.
The wrapper restores both pointer directions and saved endpoint keys, clears a
former sprinkler only if this hose still owns it, handles a nullable sprinkler,
and refreshes its effects. Capture clears stale optional hose/faucet saved keys
when their actual pointer is absent. Out-of-order State/Reply completion cannot
roll back a newer canonical hose snapshot. Retirements, cancelled resume bodies,
peer departure, recycled identities and world changes clear pending work.

PASS: six garden executables and three production compiles, native asset
contracts, existing wall placement/authority/wrapper regression. Evidence:
build/garden-hose-validation.log, build/wall-hose-regression.log and
build/sync-native-contract.json. Protocol remains 2153. No full DLL or two-player
test was produced. This lane publishes completed routes; motion during placement,
standalone sprinkler pickup/detach, poured/held-hose watering, pot transitions and
durable resource transactions remain OPEN. Missing native dependencies keep a
route/link queued until they become available or the world/session ends.

## Continued 2153: garden state, guest harvest and shared event audio (19 October 2026)

The owner supplied D:/Games/Voices of the Void/Gamee/WindowsNoEditor. Native
assets were extracted read-only into the ignored _research/current-assets.
sync_native_contract_test.py verifies the actual cooked plant fields, negative
water/fertilizer depletion, dead dryness=-1, Stolas grown lightning/destruction,
table-selected plant subclasses, hose geometry rebuild and sound entry points.
Asset hashes and results: build/sync-native-contract.json.

The placed-save census now enrolls saved plants of all measured subclasses,
faucets and completed hoses while preserving their native keys. Unkeyed
vegetation is rejected before getData, so scanning cannot assign it keys or fill
the enrollment queue. Host state is polled with a persistent cursor: at most two
captures and four candidates per tick, normally no more than once per two seconds
per actor. Changed normalized state publishes through the existing reliable
record lane, including the planting peer after its initial acknowledgement.
Guest copies refuse native growth/automatic fruit production. Reapplying a
Stolas plant cannot create another lightning strike. Plant loading clears the
cached ripe amount before native upd, which otherwise preserves stale berries.

Client harvest action 4 now queues an ordered request. The host checks known
identity, current peer generation, a 450-unit reach, sequence deduplication,
250-ms rate and a live pocket-refusal watch before running native harvest once.
The caller's valid puppet is passed as collector; its putObjectInventory2 is
refused only during that call, explicitly returning false even through session
teardown. Native random yield stays on the ground for normal shared pickup;
neither the host's inventory nor the puppet's personal store receives it.
Discard-only plants keep native Alt confirmation and host-validated retirement.
Legacy Refresh requests recover a missing save twin without authoring host data.

world_sound_sync observes completed native sound calls. newsky's actual 5062 and
437 continuations publish earthTP_in/out with native volume/pitch. noiser's sole
SpawnSoundAttached publishes outsideAlien_Cue at its actual host position.
growingPlant.harvestSound publishes plantGather_Cue with the native +25 Z offset.
EventCue IDs 1..4 supplement unchanged meteor ID 0. FIFO/retry is independent per
recipient; asset/dispatch failures retry locally, and expiry/world/disconnect
discard stale audio. One-shot audio is not restarted on a mid-event join.
Alien audio currently uses its starting position rather than following the
moving noiser; SoundCue-internal variant randomness remains local.

PASS: four garden executables/three production compiles; one sound executable/
three production compiles; native contracts. Existing wall and sky regressions
also pass. Logs: build/garden-validation.log, build/world-sound-validation.log,
build/wall-placement-garden-regression.log, build/wall-garden-regression.log,
build/sky-sound-regression.log. No full DLL or multiplayer validation.

OPEN: durable guest care consumption/acceptance, pot changes, standalone sprinkler
pickup/detach, hose placement motion and gameplay verification, keyless pre-session crops; exact audio timing/mid-event
join, noiser movement/footsteps, delayed sky mail/bookkeeping and latent state.
Airy native candidates are prop_eriePlush (prop_food) and eriePlushLive (Character);
Flesh Eerie is npc_angryErieFlesh (Character). Their native resources are available,
but adding names to an allowlist would not synchronize their behavior. All other
candidate areas remain open. Source/protocol stay 2153; peers need the same DLL.

### Follow-up: guest bucket LMB and fertilizer care

The exact native paths are now mapped from the installed assets: bucket LMB
requires height>5, writes water=height/maxHeight*600 and empties height; fertilizer
LMB requires nonpositive crop fertilizer, writes 600 and destroys its hand actor.
The client reserves a bounded result slot before native use, measures the source
key/amount/target, then queues Care only after the bucket is empty or the exact
fertilizer weak identity is gone. Native refusals produce no Care. A cancelled
script body with no post callback releases its reservation on the next tick.
Completed care results do not expire while waiting for transport queue space.

The host accepts only known live crop identities within 450 units, with a valid
peer, ordered sequence, last-announced bucket/fertilizer class and finite native
amount. A fertilizer source key cannot be spent twice by that player during the
world; existing host fertilizer is not reset by a stale guest view. Only the
canonical plant field is written, then its record is published through the usual
lane. Personal consumables remain owner-authored and persist through the existing
profile stream; this is not an atomic transaction between profile and world.
A request rejected by the host or interrupted by disconnect/world travel has no
durable acceptance/refund. Rapid tool switches can invalidate the announced-tool
check. Keep these limitations OPEN; do not claim complete gardening transactions.
Bucket getFromWater/poured water and moving hoses use separate paths and remain
open. Expanded controller/wrapper and native contract checks pass.

## Continued 2153: wall material replay and teardown safety (19 October 2026)

Host wall construction keeps unprocessed pairs visible during the drain and
also deduplicates the currently executing wall/material pair. Native callbacks
cannot schedule a second consumption of that pair or recursively drain it.
Each tick attempts at most eight feeds; failed attempts retain one retry and
new callback additions wait for the next tick. The one-use native replay permit
is scoped to the exact wall and weak material identity.

Deferred feeds stamp session/epoch/world context. Stopping or replacing the
session, changing host role or travelling during collision/enrollment callbacks
cancels the replay and nested build/damage/repair mutations. Refused repairs
still set isFine in both the frame and caller output, preserving cement.
Ordinary offline use and a host remaining after the last peer leaves stay native.

The engine wrapper rechecks world generation, wall/material identities and wall
class after IsOverlappingActor. A replaced/recycled overlap component retries
against its successor instead of spending from an obsolete collision result.
Material lookup also refuses travel during FName conversion/native restoration.

Expanded production-source regressions cover duplicate active/unprocessed pairs,
recursive Tick, exact replay permits, retry count/budget, callback additions,
disconnect/reconnect/role/world teardown, no-spend repair outputs and wrapper
identity/component changes. run_2168.ps1 passes five executables/five production
compiles, including the controller added to that runner. run_2166.ps1 passes
four placement/record/authority/wrapper executables and five production compiles.
Logs: build/wall-validation.log and build/wall-placement-validation.log.
No new protocol or package; two-player construction and atomic client repair
across personal inventories remain OPEN.

## Continued 2153: staged cosmetic playback and turn identity (19 October 2026)

Client action cues now retain Start/Verify/Stop progress and weak mesh, animation
and montage references. A successful Montage_Play is not repeated after a failed
section/playing query or failed stop. Missing prerequisites and native zero-result
refusals still retry before acceptance. Return metadata is checked before starting;
an unreadable, negative or nonfinite result after the call enters cleanup.

Live JumpToSection is followed by actual playing/section queries before sound.
A failed/no-op jump or crossed joined section stops the stock montage, with
stop-only retries. Joined playback needs no jump function and never restarts
one-shot audio. Expiry gives pending playback a cleanup attempt on its scheduled
turn and retires it even if the engine refuses; permanent native cleanup failure
cannot be proven safe by these offline checks. Playback ownership is revalidated
after reflected calls, including death-loop silence and damage-audio setup.
Actor/mesh/AnimInstance/montage/world/session/registry replacement ends further
calls. Queue progress is stored by sequence after callbacks, avoiding stale
iterators when reflection appends another packet or recursively ticks.

Turn samples now capture a weak replica identity at receipt or first animation
application. Replacement/retirement/authority changes discard old steering and
pitch; subsequent samples interpolate from neutral. Missing replicas may still
bind later. Stale turn rows are removed before admitting packets, so expired
capacity cannot reject the next valid state before the next tick.

Expanded production-source regressions cover live/join query recovery without
replay, stop failure/expiry, no-op seek, missing metadata, malformed results,
mid-call identity changes, audio guards, reentrant receipt and turn replacement,
late binding and capacity reuse. All five entity executables and twelve production
units PASS; evidence: build/entity-sync-check/validation.log and verification.json.
Protocol remains 2153; no full DLL/package/gameplay validation in this checkout.
Cross-peer timing, joined audio and native cleanup behavior remain OPEN.

## Continued 2153: independent action queues and stale identity cleanup (19 October 2026)

Grayboar outgoing retries preserve sequence order separately for each recipient.
A failed send blocks only that peer's later cues; successful peers receive later
events without duplicates. Capacity is 32 cues per peer (at most 96 shared rows),
so one saturated recipient cannot exhaust other peers' capacity. Full join queues
rotate past blocked slots while preserving their pending snapshot candidates.
Expiry/readiness/slot departure remove obsolete delivery obligations.

Clients retain FIFO per entity, rotating a bounded list of entity heads. Each
tick attempts at most four entities and one action per entity. New arrivals join
the tail; missing/late meshes and continuous traffic cannot starve older retries.
The scheduler reserves its 32-entry storage on installation. Expired packets and
empty entity entries are retired together, and disconnect clears both queues.

Host cues capture the originating actor's weak identity. Clients bind an existing
replica at receipt, or a missing replica on its first available attempt. Recycled,
remapped or retired bound entities discard stale events. Join sampling rechecks
actor/registry authority and a per-slot departure generation after reflected calls,
preventing a callback from resurrecting a departed peer's snapshot.

Production-source regressions cover independent recipients, sequence wrap, retries,
duplicates, saturation, expiry/readiness changes, full-queue join fairness, snapshot
queue-age compensation, stale actors, late binding, callback teardown, four-attempt
limits and fairness under ongoing arrivals. All five entity regression executables
and twelve production units PASS; log: build/entity-sync-check/validation.log.
Protocol/payload remain 2153/17 bytes. No full DLL/package build or two-player test;
network transit alignment, joined audio and engine task lifecycle remain OPEN.

## Continued 2153: live montage validation in restored checkout (19 October 2026)

Restored the source from origin/watchers-of-the-void-2150 at 0dce3afed and all
recursive build submodules into E:/MyMods/MultiVoid+. Development branch:
codex/continue-2153. The previous .git history is preserved; no merge is pending.
Historical DLL/ZIP/test evidence below belongs to the previous workspace.

Attack, roar and death flag transitions alone no longer emit cosmetic cues.
After the native body, grayboar_actions samples the active stock montage and
requires its section to match the observed action. The cue uses the actual
montage rate, including native death slowdown, rather than deriving it from
animationSpeed. Missing/stopped/wrong-section playback, invalid sampled values
and failed reflected getters produce no cue. Actor/world/session/host authority
is checked again after the queries. Live audio and join snapshot formats stay
unchanged; source/DLL/protocol remain 2153.

Expanded production-source tests cover all three actions, native-rate mismatch,
metadata/mesh/getter refusal, malformed position/rate, recycled actors, disconnect,
world replacement and authority loss. run_entity_sync.ps1 accepts an initialized
MSVC shell without the unavailable sibling _toolchain script. All five regression
executables and twelve production-unit compilations PASS with MSVC 14.50.35717
and Windows SDK 10.0.26100.0. Log: build/entity-sync-check/validation.log;
environment: build/local-msvc.ps1. git diff --check passes.

Full DLL/package builds and native asset tests were not run: vcpkg and the private
_research extraction are absent here. No gameplay test, installation, commit,
push or publication. Exact clocks, joined audio and two-player behavior stay OPEN.

## Package 0.9.2152 prepared (19 October 2026)

Owner requested release 2152 and an updated Thunderstore changelog. Built the current
MSVC x64 Release target, reran all nine focused runners successfully, and packaged
workspace dist/WatchersOfTheVoid-0.9.2152.zip. CHANGELOG_PLUS.md, package README,
RELEASE_2152.md and publication instructions now describe the current wisp, NPC
orientation/spawn and possessed-boar fixes, plus the unfinished behavior.

Package version 0.9.2152 intentionally contains DLL 0.9.0n b2153 / protocol 2153;
the internal identity preserves the incompatible 52-byte NPC pose layout. Packaging
and verification scripts explicitly check that identity. Archive structure, manifest,
changelog heading, 256x256 PNG icon, embedded DLL hash, loader imports/exports,
Release configuration, all 752 production source entries and delivery copy passed.
Evidence: build/release-2152-logs/verification.json and tests.json.

ZIP SHA256: 11632e4c18ee0c3aedfefd73ba9ed0cc85839b2ba923601f2d3a3a282794df02
DLL SHA256: 26e0c27442c650b0c5dd30869194584a90d78608913045497d7b8b756ecc041d
SHA256SUMS-2152.txt accompanies the ZIP. No gameplay verification, game installation,
commit, push or online publication performed; all existing open reports remain open.

## Development 2152 - entity category synchronization (19 October 2026)

### Grayboar nonlethal hit reactions (19 October 2026, continued 2153)

The client damaged-body authority guard also suppressed native damage1..4
animation and damage audio. Extended the existing paired host damaged observer:
capture finite pre-hit health and the bounds-checked skipAnim argument, then
require a nonlethal health decrease and sample the actual active native damage
section/rate after the body. No new cue for unchanged health, healing, skipped
animation, malformed arguments, a stopped/unrecognized montage or stale identity.
Lethal hits continue through the existing death edge. Repeated nonlethal hits can
send the same section again; clients do not roll another random variant.

Wire actions 4..7 map to damage1..4 inside the existing 17-byte GrayboarAction
payload. The receiver plays the stock montage section, sets audio_idle to the
native grayboar_damage_Cue, plays it and stops attack audio. It does not call
damaged/addDamage or modify health/AI flags. Sound setup failure does not restart
the already-playing montage; weak identities/session checks cover audio teardown.
Mid-hit joins sample the actual native montage and resume its position/rate with
the existing queue-age compensation, without restarting one-shot damage audio.

Production-source tests cover all four choices, repeat hits, health/skip/frame
gates, active montage/rate, mirrors/recycled actors, lethal precedence, live
damage audio, unchanged client health, audio teardown and silent joined hits.
Native asset assertions pin the random section selection, skipAnim branch,
damage cue class and idle-play/attack-stop chain. Two-player visible reactions,
simultaneous hits, native playback-failure behavior and exact audio/transit timing
remain OPEN. Source/DLL/protocol stay 2153; matching current DLLs are required.
PASS all nine focused runners and the full MSVC Release b2153 build in the
existing directory. Current DLL hash/scope: build/release-2152-logs/entity-validation.json.
git diff --check is clean; no new version or ZIP created.

### Grayboar remaining AI continuations and charged authority (19 October 2026, 2153)

Audited native dest, the callback previously bound to target.OnDestroyed. It
only removes its own binding and clears target, so it remains native along with
target expiry, construction and ReceiveDestroyed helper cleanup. Added guards
for senseUpdate (sensePawn plus unhook), saw (target timeout scheduler), and
setWalking (repeated helper SetMovementMode). Already-scheduled navigation can
resume directly in the ubergraph: entry 15 was covered; 543/558 retries, 966 roar,
1016 stuck check and 1381 delayed path check now also refuse client replicas.
Cleanup entries 531/896/981/1004 and the local idle-audio chain remain native.

The collision event has an independent AddImpulse and int_objects.addDamage(10)
path outside bite guards. reachedByLightning independently rolls a replacement
grayboar_overcharged spawn and destroys the original. Both bodies now refuse
connected clients' live host replicas. Base explosion and charged explosion,
killed/deathplosion, objectDamaged and zapSfx are also guarded: charged death
starts a repeating explosion timer; successful damage can punch a player, and
zapSfx periodically sets random angular velocity. A timer already started before
adoption cannot execute its guarded callback's side effects.

The script gate matches the class declaring the function body, not the actor's
ancestry. Authority registrations now carry explicit owner/name pairs so charged
overrides are watched on grayboar_overcharged_C. Inherited bodies remain scoped
to grayboar_C. Expanded the internal tag range to keep registrations distinct
from the existing body/damage/navigation tags. Host/personal/offline behavior
and existing damage-intent observation remain native. No packet/version change.

Production-source regressions cover all 24 authority registrations, role and
identity boundaries, inherited instances, separate charged-watch retries and all
six AI continuation entries while preserving cleanup. Native asset tests verify
the latent destinations, collision damage, replacement spawn, charged timers/
explosions/punch/spin and that each watch names a function actually exported by
its declaring package. Native engine task retirement, replacement propagation,
charged audiovisual/owner-punch behavior and two-player gameplay remain OPEN.
PASS all nine focused runners and full MSVC Release b2153 build in the existing
directory. Current hash/scope: build/release-2152-logs/entity-validation.json.
git diff --check is clean; no new version or ZIP was created.

### Grayboar cosmetic turn and motor pitch (19 October 2026, continued 2153)

Native evidence shows BlueprintUpdateAnimation entry 3948 computes turn from
the body's StaticMesh forward vector and its AI-authored forward component's
right vector. The signed, smoothed scalar drives several ModifyBone rotations
and audio_loop_turn pitch via Lerp(0.5, 2, abs(turn)). Body pose replication alone
therefore leaves client bone steering and motor pitch dependent on local AI.

grayboar_turn observes the host's native animation-update result and publishes
the actual finite [-1,1] turn scalar in the 12-byte host-only GrayboarTurn kind
201. Publication is capped at 10 Hz per body with four bodies per session tick,
fair rotation, latest-state retry, a small-change threshold and a 500 ms heartbeat.
Only world-ready slots receive it; the heartbeat also supplies stationary joins.
Rows are bounded to 128 and stale/dead host samples expire. No new protocol or
version directory; matching current 2153 DLLs are required on both peers.

Clients validate host sender, host-range eid, finite value and wrapping sequence.
After native BlueprintUpdateAnimation, live mirrored bodies replace only the
cosmetic turn scalar and its native pitch formula. Successive states interpolate
over 100 ms; missing/stale state uses neutral steering after 1.5 seconds. The
native speed path and montages continue. No helper transform, AI target, gameplay
verb or physical rotation is authored here. Host/personal copies stay native.
World/session reset clears state; weak identities guard reflected sound calls
and a recursion guard covers reentrant animation updates.

Actual production-source tests cover native sampling, send throttling/retry,
stationary heartbeat/readiness, host identity, invalid values, client ordering
and sequence wrap, signed interpolation, native-turn overwrite, exact pitch,
stale fallback, bounded queues and world/disconnect/reentrant teardown. Native
asset assertions pin the animation entry, turn inputs, bone consumers and pitch.
In-game visible steering, audio timing, network transit alignment and helper
task/delegate lifecycle remain OPEN; this is not two-player gameplay validation.
PASS all nine runners, including five entity executables and twelve production
unit compilations; full MSVC Release DLL identifies 0.9.0n b2153. Current hash and
scope: build/release-2152-logs/entity-validation.json. No new version or ZIP.

### Grayboar adopted helper path abort (19 October 2026, 2153)

The client body tick now queries GetMoveStatus on the live AIController of its
grayboarPawn helper and invokes StopMovement for Waiting/Paused/Moving paths.
This covers navigation started before save adoption; the existing body/latent
entry guards prevent the completion callback from issuing another native move.
The helper must be the body's pawn, have the body as Owner (confirmed in native
spawn bytecode), and be possessed by that AIController in both directions. Host,
personal boars, unpossessed helpers and unrelated/player controllers are excluded.

No permanent stopped flag or controller configuration is written. Missing
metadata, late/replaced possession and failed calls retry on a later body tick;
idle paths issue no stop. Weak identities, world generation, session epoch,
ownership and role are revalidated after reflected calls. A scoped recursion
guard prevents task completion from recursively issuing another abort.

Production-source tests cover active/idle/invalid statuses, ownership and class
refusal, missing metadata, failed calls, object recycling, world/session teardown,
authority loss, recursive callbacks and held-drive handoffs. Native evidence
confirms the helper's owner and its use in GetAIController -> AIMoveTo. In-game
native task completion, old target delegates, visible turning and two-player
behavior remain OPEN. No full helper synchronization claim or new protocol/version.
All nine focused runners and the full Release b2153 build PASS; current hash and
evidence are in build/release-2152-logs/entity-validation.json. Existing ZIP untouched.

### Grayboar navigation entry authority and build repair (19 October 2026, 2153)

Client host-replica guards now cover move, updMov, checkIfOnPath, nomoveMove and
the AI request-failed/move-finished callbacks. Native move schedules a delay whose
continuation goes directly to ExecuteUbergraph_grayboar entry 15; that continuation
creates AIMoveTo and binds OnCharacterMovementUpdated, bypassing move's body gate.
Added a separately scoped graph guard that reads the validated EntryPoint frame
and refuses only 15 for connected clients' live authoritative replicas. Host,
personal boars, malformed frames and other graph entries retain native behavior.
No new gameplay replay, packet or version. Already-running AI tasks and host-driven
visible turning remain OPEN; existing physical park is still the body authority.

Validation also exposed duplicated source/test/runner fragments and missing
namespace closures in the current checkout. Removed those duplicate fragments,
restored the test's actual velocity-hook doubles and fixed the runner's duplicated
param/branch. The session already called ariral/deer/centipede/mannequin Install,
but their includes and production source-list entries were missing; connected
those existing modules so the full DLL links. This is build integration, not a
claim of verified behavior for those families.

Targeted regressions cover all navigation body guards plus entry-15-only refusal,
native graph fallthrough, malformed frames, personal/host/disconnect behavior and
native bytecode evidence for delay -> AIMoveTo -> movement-update binding.
Full Release b2153 builds in the existing directory. Evidence and current hash:
build/release-2152-logs/entity-validation.json. No new version or ZIP.

### Grayboar death cosmetics and native death authority (19 October 2026, continued 2153)

Native damaged subtracts health, calls killed on lethal damage and sets dying;
the stock death montage plays at animationSpeed / 2. Its death-name notify enters
loot spawning and its completion destroys the body. Extended class-scoped client
host-replica refusal to damaged, that death notify and completion. This also blocks
callbacks from a native proxy started before adoption. Host and personal boars
retain those native bodies. Existing client weapon intents are observed earlier
at addDamage; host processing still reaches the native damaged body.

The existing action channel now accepts death action 3 without growing its 17-byte
payload or changing version 2153. Paired damaged observation emits only the first
dying transition; skipDeathAnimation paths produce no cosmetic montage cue. The
sender halves the native animation rate (including the valid 0.025 lower bound).
Client playback uses only the death section, silences idle/attack/roar components
and directly spawns the native grayboar_idle4 SoundWave on StaticMesh with native
volume 1.5, pitch 1.35 and att_default attenuation. It never creates the native
callback proxy, subtracts health, calls killed, spawns loot or destroys the actor.
Sound failure does not restart successful montage playback. Mid-death join samples
the actual playing death section/rate/position and resumes it without a new scream.

Validation: all nine focused runners pass; the entity runner covers four executable
regressions and eleven production-unit compilations. Full MSVC Release DLL builds
as 0.9.0n b2153 in the existing directory. Additional targeted/native evidence tests
confirm half-rate death/skip/duplicate behavior, joined death, loop silence, direct
audio, actual SoundWave/attenuation types and the native health/loot/destroy seams.
Current hash/scope: build/release-2152-logs/entity-validation.json. No new version,
delivery ZIP, installation, commit or publication. Matching-peer gameplay remains
OPEN for death timing, corpse/loot counts, client weapon damage and retirement;
helper/turning, exact clocks and joined audio remain follow-ups.

### Grayboar mid-action join animation (19 October 2026, continued 2153)

World-ready replay now snapshots registered host prop candidates, filters boars,
captures weak actor identities and samples at most four candidates per tick. It
queries the native AnimInstance's currently playing stock montage, section,
position and actual play rate, gated by the corresponding native biting/roaring
flag. This also finds an action whose start was not observed by our live-cue hooks.
Stopped/dead boars, other sections, mirrors and recycled candidates are skipped.
Join requests expire, deduplicate per slot and clear on slot departure/teardown.

GrayboarAction payload is now 17 bytes: its added position is -1 for the existing
live edge, or a nonnegative sampled absolute montage position for a join snapshot.
Host outbound retry age and client actor/animation queue age advance that position
at the sampled rate. The receiver uses InTimeToStartMontageAt and verifies the
resulting native section; a snapshot that crossed out of attack/roar stops only
the stock boar montage and is consumed. Required stop parameters resolve before
playback. Snapshots neither jump back to section start nor restart one-shot sounds.
All host-only, finite-value, sequence, identity and bounded-retry guards remain.

This compensates local queue delay, not network transit: existing state timestamps
are per-process and are not a shared clock. Live cues retain their arrival-time
start. Exact cross-peer timing, joined audio position, death/corpse/loot and helper
AI/turning remain OPEN, along with two-player verification. Version stays 2153;
both peers need matching current DLLs. No package/version was created.

Expanded production-module tests pass for native montage truth, stopped/dying and
recycled actors, duplicate/departed join requests, seek position/rate/queue age,
no sound restart, expired sections and malformed positions. Full validation and
current DLL hash are recorded in build/release-2152-logs/entity-validation.json.
All nine focused runners and full MSVC Release b2153 build pass; no delivery ZIP
was rebuilt. Existing historical build directory names remain unchanged.

### Grayboar live attack/roar cosmetic relay (19 October 2026, continued 2153)

Added grayboar_actions and host-only reliable kind 200 (13-byte eid/sequence/rate/
action payload). Paired pre/post observations of native bite-overlap and detected
emit a cue only when biting/roaring changes from inactive to active on a live,
registered host-owned boar. Dead, mirror, refused and already-active calls do not
emit. animationSpeed is read from the host. Observations survive nested calls,
are bounded, and recover a frame abandoned when another gate cancels its body.

Client receipt validates host slot, host eid range, sequence, action, finite rate
and queue bound. It waits up to 1.5 seconds for a mirror/mesh/AnimInstance. Playback
calls GetAnimInstance, Montage_Play, Montage_JumpToSection using the native
grayboar_Skeleton_Montage sections `atttack` and `roar`, then the matching native
audio component's Play. No callback proxy, actor AI verb, bite notify delegate,
damage or loot is replayed. Missing required reflection/parameters or refused
Montage_Play retain the bounded retry; audio failure cannot restart an already
played montage. Weak object identity and world/session epoch protect reflected
call reentry. Host send retries preserve FIFO, expire, and clear departed-slot
bits so a replacement peer does not inherit the queued cue. Late join does not
replay historical pulses. No protocol/version increment or new package.

Validation: all nine package/check_2152.ps1 runners pass; expanded entity runner
has four executables and eleven production-unit compilations, including dispatch.
Full MSVC Release DLL builds as 0.9.0n b2153 in the existing build directory.
Production-module tests cover edges, dying/active/native refusal, local-vs-host
identity, malformed packets, queue bounds/expiry, duplicates, missing actor/anim,
parameter/play refusal, send retry, departed slots, recycling and disconnect/world
reentry. Native asset evidence confirms section spelling, rate and audio choices;
the AnimBP has no AnimNotify_* gameplay handlers. Evidence/hash is updated in
build/release-2152-logs/entity-validation.json. Existing delivery ZIP is unchanged.

OPEN: death/corpse/loot authority, current action for mid-event join, host-time
alignment (live pulses currently start on arrival), helper AI/turning and actual
two-player verification. This does not close the complete creature behavior task.

### Grayboar client target/roar authority (19 October 2026, continued 2153)

Native sensePawn does more than scan: it assigns target, starts move and invokes
detected. detected starts the roar montage/audio, changes roaring/biting and binds
target.OnDestroyed. A client host replica must not author these actions from its
own overlap/visibility results. Extended the existing native-authority guards to
both functions, using the same connected-client/live-host-replica predicate as
the bite protection. Host and personal boars retain native targeting and roar.
Each watch retries independently; a failed registration cannot drop or re-register
its successful neighbours. The guard leaves native damage/death/loot paths and
the body tick untouched; it does not cancel already-running helper tasks or
already-bound target delegates from before save adoption.

Validation: run_entity_sync.ps1 passes three regression executables and nine
production-unit compilations. Native evidence passes the full spawn/class audit
and confirms sensePawn -> target/move/detected -> roar montage/audio/delegate.
Expanded production-body tests apply all role/lifetime/identity negative controls
to all four authority seams and cover one independently failing registration.
No network format or version change, DLL/ZIP delivery build, or gameplay test.
Shared cosmetic playback, late-join state, helper task cancellation and turning
remain OPEN. Cosmetic replay must bypass these native gameplay/delegate bodies.

### Grayboar client bite authority (19 October 2026, continued 2153)

Native bite-zone overlap starts the attack montage/audio and adds a body impulse.
The attack montage's OnNotifyBegin enters the bite branch, which calls addDamage
on arbitrary int_objects. The existing player-health guard did not cover those
object writes or the impulse. Added class-scoped guards on both Blueprint bodies
for connected clients' live host replicas. Stable registry identity protects
save-adopted and hand-held replicas; fresh-spawn/parked identity also applies.
Guarding the notify separately covers an attack begun before replica adoption.
Host, personal/local boars, unrelated actors and inactive sessions retain native
behavior. Watch installation retries failed registrations and preserves successes.

Production-body regressions cover both entry points, failed hook registration,
adoption/hand transitions, recycled/dead identity, subclasses and session teardown.
Native bytecode evidence verifies overlap -> attack montage/audio/impulse and
notify -> bite branch -> arbitrary-object damage. This is authority protection;
shared cosmetic actions, late-join action state, helper AI and turning are OPEN.
Keep version 2153; no separate build versions without an explicit owner request.
Validation: package/check_2152.ps1 passes all nine runners, including the expanded
boar regression and nine production-unit compilations. No DLL/ZIP delivery build
was produced for this change; existing delivery artifacts remain the prior build.

### Grayboar native locomotion and movement audio (19 October 2026)

Native AnimBP reads grayBoar.GetVelocity, projects it onto the actor's forward axis,
normalizes/smooths speed, and uses it both for locomotion and audio_loop_velocity
pitch. A kinematically parked prop can report zero native velocity while its replica
moves through the host's transform stream, leaving these cosmetics idle.

Added a native GetVelocity post hook scoped to the stock grayboar AnimBP reading a
live grayboar on an active connected client. The prop-drive receiver supplies the
velocity of its current delayed interpolation segment. The same delay helper now
serves position interpolation and the presentation derivative. It returns zero for
first sample, teleport, pre-roll, stopped/arrived segments and malformed timestamps
or non-finite values. Ended/dead drives leave the native getter untouched. No physics
velocity is assigned, no damage/gameplay verb replayed, no new packet or protocol.
Other getter callers, other AnimBPs, host, disconnected sessions and props outside
this stream retain their native result. Hook resolution/installation retries at
1-second intervals until ready; native animation continues to own smoothing/audio.

Validation: production sender/receiver regression compares rendered positions with
the cosmetic derivative and covers short/long intervals, first pose, teleport,
freeze on loss, invalid samples, ended/recycled actors. Actual grayboar hook tests
cover caller/class/role scope, native fallthrough, zero result, null output, retry
and teardown. Native bytecode confirms GetVelocity -> speed -> movement-loop pitch.
run_entity_sync.ps1 passes three executables and nine production-unit compilations;
package/check_2152.ps1 passes all 9 runners. Full MSVC Release b2153 builds. Current
hash/scope: build/release-2152-logs/entity-validation.json. No two-player validation.

This addresses locomotion input and movement-loop pitch, not shared attack/roar/
death montage cues or independent client turning/AI. Those, corpse/loot, hand-held
presentation and mid-action join remain OPEN. No ZIP, installation or publication.

### Grayboar adopted-replica authority across drive handoffs (19 October 2026)

A save-adopted host boar is not marked as a fresh mirror spawn. When a hand wins
its physical drive, prop_drive_stream removes its parked row. The bite guard's
old fresh-spawn/active-drive checks could then permit native client damage while
the host still relayed the real bite. This was a duplicate-authority gap.

The client damage guard now also checks the Registry's O(1) actor-to-eid binding:
a valid host-range id, a mirror Element, and that Element's live actor matching
the attacker. That identity survives save adoption and drive/hand transitions.
An absent/stale/other actor binding, local Element or peer-range id does not classify
a personal boar as a host replica. No world scan, new packet or protocol bump.

Native sensePawn evidence confirms overlap scanning for Pawn, removal of the boar's
own helper, Pawn cast and assignment as target; it does not hardcode player 0.
This supports host targeting of peer puppets without replacing the native selector.
Actual detection, collision channels and behavior still need two-player testing.

Validation: run_entity_sync.ps1 passes NPC orientation, the actual boar feeder/damage
regression, existing prop-drive sender/receiver tests and eight production-unit
compilations. entity_sync_native_test.py passes the native class/spawn checks;
expanded --boar-only evidence and -GrayboarOnly regression also pass. Added cases
cover adopted replicas, parked-to-hand transition, stale/missing/mismatched binding,
local/peer-range identity and preservation of native local damage. Full MSVC Release
b2153 build passes. Current DLL hash and focused validation scope are recorded in
build/release-2152-logs/entity-validation.json. The later merge-conflict
resolution pass then repeated the full suite (see below).

Shared montage/audio, client helper/AI coordination beyond existing physical park,
corpse/loot behavior and gameplay remain OPEN. No ZIP or installation performed.

### Merge conflict resolution and current validation (19 October 2026)

At the owner's explicit request, resolved all 21 conflicted files. Inspected both
sides: incoming 2150 alternatives would undo current protocol 2153, full NPC rotation,
personal nightmares, ragdoll retries/drift correction, tool effects, recursive skin
discovery and grayboar subsystem registration. Preserved the current implementations
and the latest uncommitted boar damage routing. Conflict choices were applied within
each hunk; candidate results matched the current-side file contents. Original BOM
and line endings were retained where applicable. Before resolution, copied working
files and all three index stages into build/merge-resolution-20261019-fa5592fa/.
Ten pre-existing modified files were verified byte-for-byte after resolution.

Git has zero unmerged index entries and no active conflict markers. Resolutions are
marked in the index. The merge still awaits its commit; no commit/push was performed.
The latest entity/boar code and test changes remain uncommitted in the working tree.

Current validation: package/check_2152.ps1 passes all 9 runners using the ordinary
working tree, including the expanded boar damage/native tests. Full MSVC Release
build passes; main.dll resources identify 0.9.0n b2153. Updated hash and results are
in build/release-2152-logs/entity-validation.json. No ZIP, installation,
publication or host/client gameplay verification. Creature AI/target selection,
montage/audio and corpse/loot follow-ups remain open.

### Grayboar bite ownership follow-up (19 October 2026)

Native bite checks overlap and line of sight, then calls mainPlayer.addDamage with
the boar as attacker. The player's own graph forwards the amount and source into
Add Player Damage, which writes the per-machine saveSlot.health. The script-body
route can bypass the existing ProcessEvent-only addDamage interceptor.

grayboar_sync now watches that final player damage body, filtered by the source's
native grayboar ancestry. A host bite on a registered puppet relays validated damage
to its owner through existing PlayerDamage and cancels the foreign singleton write.
A host bite on the host's own pawn remains native. Other foreign bodies are refused
without a send. On clients a mirror-spawned or host-drive-parked boar cannot deal a
duplicate local hit, and no boar can damage this machine via a foreign player body.
Unbound/local boar encounters retain native local damage. Source-less wire replay
is unaffected. No generic AI, animation or tick cancellation was added.

Validation: run_entity_sync.ps1 -GrayboarOnly passes the production feeder/damage
harness for owner routing, local host damage, foreign/unknown bodies, subclasses,
cyclic ancestry, invalid amounts/frames, mirror/parked duplicates and disconnect.
entity_sync_native_test.py --boar-only confirms the actual overlap/LOS -> bite ->
player source/amount -> health-write path. This pass was initially compiled against
isolated pre-merge headers because shared files carried conflict markers; those
markers were subsequently resolved (see "Merge conflict resolution and current
validation"), and the production code and all 9 runners now pass against the
ordinary working tree.

Remaining: shared target/AI coordination, montage/audio replay, corpse/loot behavior
and host/client gameplay. A native hit uses the existing send-and-owner-apply channel;
this does not verify transport saturation, directional hit presentation or every
possible independently spawned client twin.

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

### Complete requested entity coverage and remaining work

| Requested group and entities | Verified source coverage / remaining work |
|---|---|
| Розітали: Велика Піраміда; Кавотія; Пагоніопьло-Ігетіс; Скаути; Солтомія | piramid2 + piramid_sync, igetis and soltomiaCleaning have existing routes. Exact localized-name associations, phase/actions, destruction and late join need native and multiplayer verification; Kavotia/scouts not mapped in this pass. |
| Творіння Розіталів: Антибризер; Викопна Гончача | antibreather/fossilhound are Character families. Fossilhound ticker catch fixed. Antibreather's player/cave-dependent spawner and cross-peer attack routing remain open. |
| Арірали | arirFollower/ariral_shooter/ariral_pigBeater existing NPC routes. arirGunStealer implemented via ariral_sync; stealing/held-item ownership and event phases verified. |
| Сірі; Вогнеметний танк; Одержимий кабан | grayTest NPC and firetank world-actor routes exist. Firetank is Pawn; turret/fire/fuel/damage are not established by its transform stream. grayboar prop vs grayboarPawn Character split confirmed; wrong body NPC registration removed; host body motion now feeds prop drive. Helper AI, attacks and animation remain open. |
| Геоми: Раміїл; Яйцеподібний геом | eg and superEgger routes exist; mapping these to user-visible names and their special actions is not yet verified. Ramiel remains unmapped. |
| НЛО: Бумеранг; Джоїл; Корабель Сірих; Мідас; Пілюля; Трифо; Кулі | Existing routes include ufoDropper variants, arirShip, skyUfo, morningUfo, rozitBorg, kocker and tentacleBallsFollower. Exact seven-name mapping, child spawns, beam/material/audio phases and late join remain open. |
| Флора/фауна: Олень; Риба; Таракан | deer and deer_ded implemented via deer_sync (NPC mirror route established). Fish class/behavior not yet mapped. cockroachMaster has a dedicated existing roach lane; movement, eating, crushing and late-join gameplay remain to verify. |
| Blue Brain Cheese; Restocker; Eye; Вольфганг; Дерево з очима; Живий Гриб; Космічна медуза | cheese-slither is a goreSlither descendant; funguy and jellyfish have routes. eggyWolfgang derives from actor_save (not Character). Restocker/Eye/Wolfgang/tree exact behavior routes remain open; do not assume eyer identifies every eyes entity. |
| Кістяна людина; Кривавий скелет; Цікаві очі; Мімік; М'ясний слизень; Олень-зомбі | Zombie skeleton/bloody/deer lineage confirmed; goreSlither and owner-entity eyer paths exist. prop_impostor/impostorPropsMaster has prior mimic work. Exact bone-man/curious-eyes mapping and attacks/effects need verification. |
| Привид; Примарна колода; Світлячки; Ходячий манекен; Чорна пляма | Keep personal spawn conditions where native behavior requires them. Firefly has an existing personal/ambient path; mannequin lifecycle implemented via mannequin_sync. Remaining names, effects and phase synchronization need native audit. |
| Демони: SKerfur-O; Безокий Інсомник; Демон Безсоння; Покинутий Керфур; М'ясна Ейрі; Статуя цапа; Статуя Столаса; Фурфур | SKerfur-O zombie lineage confirmed. Insomniac and Child remain peer-local under previously agreed personal sleep behavior. npc_angryErieFlesh is a distinct Character not covered by the zombie-Erie descendant. Other demons/statues/rituals require individual behavior and run-end/target review. |
| Заборонені: Black Friend; CorpseWalker; Crasher; Fast-shadow Figure; Fridge; Interloper; The Exit Door; The Head; Басейнова сутність; Гірські Люди; Проклятий павук; Рунна сфера; Тіньова фігура; Фальшивий Кел; Чорна сфера | Exact native classes, spawn availability and local/global effects are not yet mapped. None is declared synchronized by this pass. Do not force debug/test actors into normal gameplay merely to populate this list. |
| Жартівні: Жива Ейрі; Жируфус; Руфус | Exact live-Erie/Girufus mapping and Rufus interactions/animation/ownership remain open. Do not confuse live Erie with zombie-Erie or angryErieFlesh. |
| Новорічні: Гноми; Крампус | npc_krampus is already in NPC lane; attacks/events need testing. Gnomes' classes, spawn conditions, interaction and shared lifetime remain open. |

Next implementation follows the synchronization candidates in TODO.md; grayboar
body/helper follow-ups are deferred. Add native per-family action/state transport
and missing creature paths. Do not add unknown names
blindly to an allowlist. Run host/client spawn, target selection, action, death,
disconnect and mid-event join for each row before closing it.

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


Follow-up: PlayFatalityMontage previously returned true after a successful ProcessEvent
dispatch even if Montage_Play itself returned zero (playback refused). It now requires
successful parameter writes, dispatch and a readable finite positive ReturnValue.
Failures propagate to the bounded receiver retry. CachedObjRef guards both actor and
AnimInstance across native playback and section-name conversion; a recycled object
cannot receive the later jump. An unavailable/malformed optional section jump does
not turn already successful playback into a repeated restart.

run_wisp_animation.ps1 now also runs wisp_montage_test.cpp against actual production
wisp.cpp, covering zero/negative/NaN/infinite or unreadable results, parameter/dispatch
failure, actor/AnimInstance recycling, recovery and absent optional section. Receiver
regression and compilation of wisp.cpp, wisp_attack_sync.cpp and wisp_tear_mirror.cpp
pass. No 2152 package or two-player gameplay verification yet.

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


Consolidated 18 October 2026 at the owner's request. Release 2150 includes all
accumulated source work through development batch 2192, including later work IDs.
Those numbers are historical work IDs, not newer packaged releases.
Every row below is still open for implementation or actual two-player verification.
"Code ready" and focused offline checks do not close a gameplay report.
This is the single active work list; completed per-change WORK reports have been
removed after their unresolved follow-ups were captured here.
Keep implementation notes and follow-ups here; update existing rows instead of
creating a new numbered WORK report or a parallel backlog.

## First checks on both peers using the new 2150 package

### 2151 — model-authored ragdoll physics (5/38/50), 18 October 2026

ConfigurePlayerRagdollMesh previously rejected every skeleton other than the
hardcoded Kerfur skeleton, even when the selected mesh had its own valid authored
PhysicsAsset. Such a remote body never started and the upright puppet remained.
It now prefers the selected mesh's PhysicsAsset if it has at least three bodies,
enough live constraint templates, unique named bodies including pelvis, and all
physical bones resolve on the selected component. The standard 17-body Kerfur
asset remains the fallback only for its matching common skeleton. A two-body
default still falls back; an unsupported foreign rig is never force-fitted.
The selected physics resource is guarded with CachedObjRef through appearance
capture and target setup, including callback-induced recycling. Pelvis is required
because attachment, velocity synchronization and positional correction use it.

Native asset inspection confirmed SkeletalBodySetups (17), ConstraintSetup (16),
and unique physical bone names including pelvis on kerfurOmegaV1_Physics. This
checks compatibility prerequisites, not the complete constraint graph topology or
actual physics behavior. run_ragdoll_skin.ps1 passes own-rig selection without
loading the fallback, common-rig fallback for an incomplete default, foreign-rig
rejection with missing bones/pelvis/joints, duplicate bones, stale physics resources,
appearance bounds and setup failure propagation. Production engine_ragdoll_skin.cpp
compiles; git diff --check passes. The existing 2144 runner also passed.
No DLL/ZIP or gameplay test. T-pose reproduction, local native ragdoll and exact
limb replication remain open. Verify a common skin and a custom authored rig on
host/client, entry/get-up, body-loss retry and positional convergence in gameplay.

### 2151 — ragdoll positional drift (5/50), 18 October 2026

The sender already publishes pelvis world coordinates, but the receiver discarded
them and wrote only velocities. Different collisions accumulated permanent position
error; a stationary sender could never correct the mirror's resting location.
RagdollDisplay now measures its pelvis and translates the entire articulated mesh
toward the received position. First successful sample and displacement over 10m align
immediately; ordinary drift corrects by 25% per fresh packet, with a 2cm dead zone.
Moving the mesh with K2_AddWorldOffset, bTeleport=true and bSweep=false preserves
velocities and the relative joint pose; existing pelvis velocity feed remains.
An unavailable getter/translation can retry on the next packet. Coordinates and
velocities are checked for finiteness; target coordinates must fit kMaxCoord.
New bodies clear alignment state. No packet layout or release metadata change.

Expanded run_ragdoll_retry.ps1 passes initial alignment, resting drift convergence,
dead zone, large displacement, invalid coordinates, preserved velocity values,
failed translation retry and recreated-body alignment, plus earlier lifecycle cases.
Production remote_player_ragdoll.cpp and engine_playerragdoll.cpp compile with MSVC;
git diff --check passes. No live physics/host-client verification and no DLL/ZIP.
This addresses displacement, not T-pose or exact limb replication. Check slopes,
moving props, walls, high latency, get-up, selected skins and teleport in gameplay.
Engine API evidence: Epic USceneComponent K2_AddWorldOffset (world-space delta) and
K2_SetWorldLocation/K2_AddWorldTransform teleport parameter (keeps physics velocity).
The reflected parameter writes remain checked for the game's UE4 runtime.

### 2151 — remote ragdoll retry (5/50), 18 October 2026

Found a state-machine failure: OnWireBit latched ragdoll=true before Start, then
ignored every subsequent true packet even if spawn/configuration/attachment failed.
DriveAttached also restored the standing puppet after a body died, but the true
wire latch prevented recreation until the player exited and entered ragdoll again.
OnWireBit now retries an inactive display after 500ms while the sender remains
ragdolled. A live display is never respawned; a new rising edge starts immediately;
falling edges and teardown clear the retry deadline. No placeholder body is added.

run_ragdoll_retry.ps1 passed regression cases for transient spawn failure, packet
burst throttling, no duplicate live body, lost body, failed attachment/orphan
destruction, get-up and teardown. Production remote_player_ragdoll.cpp compiles.
git diff --check passed. This fixes failure recovery, not the entire T-pose report:
selected mesh/physics compatibility, local native body, remote drift and exact
limb pose still need investigation and live host/client verification. No DLL/ZIP.

### 2151 — insomnia demon (49), 18 October 2026

Native evidence: `mainGamemode.trySpawnInsomniac` rolls from the local saveSlot.sleep
below 20, refuses isSleep, and locates the spawn around camera 0. The ticker spawns
insomniac_C (or its rare Child variant), stores the normal actor in gamemode.insomniac
and clears it on destruction. The demon's own graph moves toward GetPlayerPawn(0),
checks local sleep/distance, drives the local camera/stopMovement during grab, punches
the local mainPlayer for spooker, and calls gamemode.createDream for the normal attack.
It has no general remote-pawn target path. The existing host-only ticker gate and
parked NPC mirror prevented the client's personal encounter from running.

Removed the ticker refusal, the insomniac NPC allowlist entry (also excludes derived
Child), and the obsolete EX-spawn source rows. Each peer now runs its native personal
encounter against its own sleep state; the host's demon is not a shared world NPC.
No new packet or version metadata change is needed for this source fix. Last package
remains 2150; current work targets 2151. The owner's follow-up also changes sleeping
nightmare handling as described below.

Validation: `tests/insomnia_native_test.py` passed against the locally extracted game
bytecode; `tests/run_insomnia.ps1` compiled both changed C++ translation units with
MSVC; git diff --check passed. No DLL/ZIP, installation or live two-player test.
Gameplay: low-sleep client sees pursuit and grab/dream; rested client does not inherit
the host's encounter; host behavior remains native; test simultaneous low sleep,
sleep recovery/despawn, disconnect/reconnect and rare Child behavior if reproducible.

### 2151 — personal sleeping nightmares (10), 18 October 2026

Owner requested both insomnia encounters and sleeping nightmares only for players
whose own conditions match. Native mainGamemode's sleep loop gates on isSleep and
nightmaresEnabled, rolls bedSleepProb, then calls wakeup before createDream. The bed
probability uses the local bed.dreamProb, or 0.15 without a bed; dreamProbability=-1
preserves this native path. This is a separate condition set from Insomniac's sleep<20.

Both roles now restore the native probability during shared acceleration; waiting
sleepers keep the existing suppression. Removed host dream capture/broadcast and
client forced replay/retry, which bypassed the recipient's conditions and could
overwrite a personal scene. DreamScene's numeric wire ID remains reserved; its
receiver ignores legacy broadcasts. Local nightmare wake edges still use SleepState
to end acceleration for everyone. Only the affected player enters that dream; other
players wake without being forced into it. Insomniac's own createDream path remains
native and personal. Existing personal scare triggers retain their native checks.

Validation: production sleep_sync.cpp compiles with MSVC. run_sleep_personal.ps1
passes client/host waiting policy, client native probability, sender authority,
client clamp, client nightmare falling-edge report, host interrupted End without
full rest, legacy packet rejection, wake-before-refill and disconnect restoration.
insomnia_native_test.py also verifies actual sleeping/enabled/bed/random gates and
wake-before-dream ordering. git diff --check passes. No DLL/ZIP or gameplay test.
Gameplay remains required with nightmares disabled on one peer, different beds,
simultaneous wins, one awake/rested peer, Insomniac attacks and reconnect.

- [ ] Insomnia demon spawn, visibility, client targeting and attacks (49).
- [ ] Selected-model ragdoll pose/entry/exit on each peer (5/50); do not restore the plushie fallback.
- [ ] Client appliance plug/unplug, short/long cables, extensions and unpowered outlets (51).
- [ ] Identify which token counter/UI is desynchronized; do not assume money (52).
- [ ] Shared base flicker, late join and load recovery (53).
- [ ] Computer shared screen/operator/activity (54), ON/OFF during animation, power loss and reconnect (55).
- [ ] ZIP insertion/ejection: actual returned item, class, model and scale on both peers (56).
- [ ] Cans: open/eat/final portion, held/world, stow/re-equip, simultaneous use and reconnect (57).
- [ ] Workbench: simple placement and disappearance after 1–2 seconds or later (58).
  Record the exact item, authoring peer and logs; crafting is not required by the report.

## Complete open tracker carried into 2150, with 2151 follow-ups

The statuses retain their original work IDs and evidence. Duplicate reports stay linked
rather than being silently dropped. Issues 6 (mining) and 10 (nightmares) were reported
working; issue 41 (old basement wall) remains explicitly excluded by the owner.

| ID | Problem | Remaining work / evidence |
|---|---|---|
|1|Client cannot break props|Needs new-build gameplay result|
|2|Prop jitter/rollback|Частично2151: one receiver drive per prop and late losing-release guard; native local grip contention/general jitter remain|
|3|Pushing one prop with another|Частично2157/2159: local/remote grips and deferred collision chains;2159 corrected empty legacy hit watch to actual hitCpp and checked component owner integration. Client-grab host contacts/convergence need two-player testing|
|4|Client R+LMB duplication|Needs gameplay result|
|5|Selected-model ragdoll|Reopened: owner still reports incorrect ragdoll on2168 (report50). Preserve2144 code, but do not treat this behavior as fixed.|
|7|Extinguisher foam|Needs gameplay result|
|8|Loose power outlet floating|Needs gameplay result|
|9|Physics gun / both gun effects|Частично2158: native physical-gun target now feeds ordinary prop movement/release and local ownership, including soft/super descendants. Both guns' beam effects, scale/freeze outcomes, non-prop targets and multiplayer contention remain; gameplay needed|
|10|Nightmares|Previously reported working. 2151 requested personal enabled/bed/chance behavior implemented; shared night interrupt retained. Native checks, state regressions and compile pass; new behavior needs multiplayer gameplay.|
|11|Rotating chair|Needs gameplay result|
|12|Console commands|Частично2154/2155: общий ввод/ответы/ошибки/clr и недавняя история при позднем входе; полные события, местные эффекты и единый порядок одновременного ввода остаются|
|13|Funny rule music|Частично2154/2155/2157: Madness music/HUD и музыка семейства Maxwell в мире/выбранной руке; точная фаза трека и прочие события остаются|
|14|Funny rule display|Частично2154: Madness HUD; прочие события остаются|
|15|Toy sounds|Частично2156/2157: Maxwell squeak/pitch world and selected hand; abplush Alt-use world/hand; ordered retries. Other toys and squeeze gameplay remain; needs gameplay test|
|16|Stutters when stowing items|Частично2157: removed full object-array lookups of unloaded claimed classes from save-coverage/pocket path, serial-safe cache. No frame profile or in-game confirmation; residual stutters remain unconfirmed.2165 follow-up removes recurring unresolved-class full walks in sleep resolver; production compile passed, no runtime profiling|
|20|Bucket water disappearing|Частично2153/2159/2169: world changed-state publication/empty repair/passive simulation;2159 adds remote selected-hand level/soap-presence/alcohol display, preserving owner's actual contents.2169 adds current held water/actual soap count/alcohol to the personal profile, with validated ordered receipt. Simultaneous writers/in-flight record arbitration and pouring gameplay remain|
|21|Cement / trowel|Частично2153/2160/2166/2168/2169: trowel hand display; world units/dry and two-bag transfer publication, passive tick, exhausted-bucket guard;2166 adds concrete-bucket hand fill/dry/empty display and no-spend client wall-repair refusal. 2168 refreshes outgoing held-trowel profile concrete from the matching local hand, including zero; identity mismatches defer the poll. 2169 adds live held concrete-bucket/cement-bag profile amounts/dry and validated ordered profile receipt. Concurrent host transactions, dryTimer handoff and actual client wall repair remain|
|22|Brick-wall outline and build|Partial2166/2167/2168: placement/progress, existing-wall enrollment, solo-host tracking and native-key save twins;2168 adds host-validated client melee damage to completed walls. Five focused2168 tests/four production compilations pass. Client repair, atomic material transactions, creative build and effects remain; no multiplayer test.|
|25|Battery levels|Частично2146/2147/2151; pending received record applied before pocket capture; direct hand/in-transit state still needs review|
|26|Airy event|Осталось|
|27|Main-room computer walls becoming physics props|Код общего пути2158/2159: независимая регистрация новых деталей и движение до остановки; подтверждены7 cubicleP-частей через обычную фабрику, проверены независимые появления. Нужна игра вдвоём на точных панелях. Снятие УДАРОМ, исчезновение У КЛИЕНТА; кто бьёт, не установлено. Пользователь предполагает все снимаемые ударами предметы; custom-пути пока не доказаны. Это не исключённые стены подвала|
|29|Crowbar static state / dragging|Частично2148; unusual wall/parent/replacement classes unconfirmed|
|31|Sleeping body pose|Осталось; native sleepingPawn is camera/input, not a ready-made body. Anchor/ragdoll/selected-model integration needs implementation.|
|34|Client crate/package loot invisible|Частично2148/2150/2158: MRE floor/hand, host finished-birth retry; keyed spawn hook no longer waits for all optional ambient assets/observer registration. Other packages/client-local routes remain|
|35|Centipede / other living creatures invisible|Осталось|
|36|Computer chair rotation (duplicate report)|Same as11,2144|
|38|Skin model list / previews|Частично2151/2152: original sixteen preview tiles plus supplied Mynet/skeleton images; both requested meshes were already builtins. Catalog regression passed; runtime appearance/animation/ragdoll still needs gameplay, other preview gaps remain|
|39|Consumables refill on pickup/drop|Частично2146/2147/2151; food earlier fix retained, pending canonical record drained before pocket capture; other transitions remain|
|40|Flesh Eerie summon / visibility / aggro|Осталось|
|42|Event teleport to the triggering/base-entering player|Код2161: actual agrav occupancy and runtime-verified shared armed boxes; client pose fallback, initiator gather, repeat/reentry guards and send retries. Two focused checks/five TUs passed; Other custom paths and full event effects remain separate|
|43|Safe cracking|Код2162: both dial success and hacked converge on opened; client completion to host, single loot, saved lock publication/retries and host-lock protection against early/stale records. Initial unlocked drop record preserved. dial UI and generic child hinge identity unchanged|
|44|Sponge wetting|Частично2160/2169: native saved power/wetness now publishes after enterWater/wash/clamp/LMB; load no echo and final-use tail checked. 2169 adds current held wetness to outgoing profiles. Soap stock/foam/surface effects and personal-hand transitions remain unverified; two-player test needed|
|45|Grill ignition|Код2162: host ignition/off, active/coals/burn snapshot and late join, passive client heating/fuel tick, immediate coal record and request retries. Concurrent coal transactions/stale records, lid and personal item transitions remain.|
|46|Client-placed crops/faucets/hoses disappear after save reload|Continued2153: saved-actor enrollment, host growth/water/fertilizer/ripeness/death, creator updates, guest harvest to world loot, owner-paid LMB water/fertilizer results, harvest audio and missing-save-twin refresh added. Four focused executables/three production compiles pass; native contracts verified. Durable care acceptance/refunds, other watering paths, hose rerouting, pot changes and two-player save/reload remain open.|
|47|Black disk/yellow-ring sky event starts on client independently|Visual2164 and schedule authority2165 retained. Continued2153 adds native black-hole in/out sounds from actual start/latent continuations; alienSounds audio also added. Offline/native checks pass. Delayed mail/bookkeeping, other latent phases and two-player timing remain open. This is NOT Bad Sun or black fog|
|48|Mimic (user identified; trees/white pillars/radar report)|Code2165: prop_impostor_C and impostorPropsMaster_C identified; client random generation and local-camera culling disabled. Reuses keyed prop class/disguise/birth/death/join; host controls lifetime. Focused checks/compilation pass; multiplayer/rendering/damage verification pending. Related to35; other creatures remain open|
|49|Insomnia demon neither appears for nor attacks the client|2151 code ready: restored native peer-local ticker and excluded insomniac_C and its subclasses from the NPC mirror lane. Native spawn/target/grab paths verified; focused compile passed. Host/client gameplay remains pending.|
|50|Ragdoll display is still incorrect|2151 partial: spawn/lost-body retry, pelvis correction and model-authored physics selection added. Valid own rigs may use a different skeleton; common fallback retained, bone/pelvis/joint and stale-asset guards tested. Focused regressions and compile pass. T-pose in gameplay, local native body and exact limbs remain open; revisit5/38.|
|51|Client cannot plug electrical appliances into outlets|Partial2170: socket-endpoint authority fixes refusal at the far end of long cables; native length/occupancy and callback lifetime guarded. Client connect/graph parameter-frame bounds now fail closed. Two focused executables, native-bytecode check and two TUs pass. Report remains open for actual2168 symptom/gameplay verification; nearby failures may have another cause. Distinct from loose-outlet floating8.|
|52|Client token display remains desynchronized|Open. Identify the token counter/UI and its authoritative value; do not assume credits or a particular token class without evidence.|
|53|Base light flickering is not synchronized|Partial2177–2180: host-authored ceiling/ambient phases; indexed clients suppress native writes; local ambient settings remain local. Failed applies retain latest state;2180 retries failed/unreadable join snapshots without advancing the all-peer baseline, with peer/world cleanup. Channel/wrapper regressions, prior native checks and five production TUs pass. Actual keys, loading/late join and two-player timing need gameplay verification.|
|54|Computer display does not show the same activity to host/client|Open. Other players cannot see who is using the computer or what they do. Trace shared screen/UI state, cursor/input and operator visibility; console text12 alone is insufficient.|
|55|Computer power-on and power-off are not synchronized|Partial2173: targets survive actor loading and native animation; failed sends and deferred join snapshots retry with latest state. Pending sends clear on peer departure/disconnect; 2171 echo suppression is preserved. Existing animation regression, new delivery/lifecycle regression and two production TUs pass. Still requires two-player checks, power loss/restoration and late join; no release built. Separate from water short-circuit/damage18/19.|
|56|ZIP disk model sometimes becomes a huge owo when ejected|Partial2172: laptop `zip` is now resolved as a masked Unreal bool bitfield, written without disturbing neighboring flags, and hashed as its logical value. Focused wrapper regression and production TU compile pass. This is a concrete wrapper defect but is not proven to cause the reported model/eject symptom; still needs in-game ZIP eject and multiplayer checks.|
|57|Opening canned food and eating it are not synchronized|Partial2176: world open/eat publish records (2174), held can state travels with HandItem (2175), and client profiles refresh live opened/type/foods before hashing (2176). Identity-checked copied records preserve unrelated data. Three profile/record regressions, native evidence and two production TUs pass. Protocol2175. World/held use, reconnect, final portion, stow/re-equip, drop/pickup and simultaneous consumption still need multiplayer verification.|
|58|Some items disappear when placed on the workbench|Open symptom; partial code2181–2186. Owner clarified17 October: simple placement, visible disappearance after1–2 seconds or later; no crafting reported. Items and host/client unknown. Transform retries, local-birth protection and deletion identity rechecks implemented;2186 invalidates old sweep passes on bracket/world/reset changes and blocks recursive reconcile ticks. Focused tests/production compilation pass. Full sweep/gameplay symptom remains unverified.|

## Additional unfinished details retained from earlier TODOs

Mirror-backup audit, 18 October 2026: the retired mirror contains no separate
production implementation to merge. Archived test variants have canonical
counterparts; differences include corrected checkout paths and later regression
coverage. The one-time RBE2143 import was already applied; do not rerun it.
Record retries through work2192 and sweep protection through work2187 are present
in production. run_2188.ps1 and run_2182.ps1 passed again during this audit, including
production compilation. These checks do not close the workbench gameplay report.

Keep these investigation details after removing mirror-backups:

- Sleeping body31: native playerSleepingPawn_C supplies camera/input, not a visible
  sleeping body or a verified mattress anchor. Use the selected player model with
  a verified bed/body anchor and sleep/ragdoll/sitting entry/exit precedence; suppress
  footsteps and inappropriate hand/head effects. Native evidence remains in the
  workspace _research/sleep_2166.txt, wakeup_2166.txt, sleep_graph_2166.txt,
  sleep_full_2166.txt and objects__misc__playerSleepingPawn_2164.txt.
- Cement21/22: verify native held-trowel key continuity in game. A mismatch defers
  the entire profile poll. mainPlayer.updateHold rebuilds the held actor from
  saveSlot.hold[0]; checkEquip reads the live actor but does not update that array.
  Any future repair transaction must coordinate unequip/drop and stock persistence.
- Light53: still verify overridden native setters, actual component readiness,
  late join and visible timing. Later channel retries supersede the old2178 note
  about a missing apply retry; separate reliable messages do not ensure frame-exact
  flicker alignment.

- [ ] Food/package final-bite destruction and subclass eaten() output; preserve local poison/player effects.
  Other package families and client-local loot need explicit provenance, not global replaceProp admission.
- [ ] Rejected client placement may remain awaiting binding: establish acknowledgement/refusal behavior.
- [ ] Coin pickup by E/overlap: one host credit, disappearance only after acceptance; map-placed coins remain local.
- [ ] Console event bodies/AI/damage for Rufus/thiccfus, llama, Madness grunts and variants;
  simultaneous typists' canonical order, periodic/local output and exact music phase remain.
- [ ] Wall repair requires an atomic request/result and exact personal cement stock across
  hand/slot/drop, retry/rejection/disconnect; keep the current no-spend refusal until complete.
  Creative construction, wet-wall timer persistence and exact impact/effects remain.
- [ ] Water/cement/coal concurrent writers and stale in-flight records; dryTimer ownership;
  sponge soapAmount/foam/surfaces, grill lid/final cooked food and personal-hand transitions.
- [ ] Enroll pre-session plants/faucets/hoses; later watering/fertilizing/harvest, pot transitions,
  accepted hose rerouting and concurrent use. Missing historical save objects are not migrated.
- [ ] Sky-event audio, delayed email/bookkeeping and pre-session latent chains; other custom
  event effects/creatures need host phase/cues rather than replaying whole commands on every peer.
- [ ] Stow frame-time capture if residual stutters remain; preserve pending item-state restoration.
- [ ] Skin preview gaps, custom skins and real animation/physics; do not remove compatibility exclusions.
- [ ] Existing saved second-ATV positions are unchanged; test near-simultaneous carry/mount and stale menus.
- [ ] Whole-package multiplayer smoke: host/client role reversal, two clients contesting objects,
  late join/rejoin, disconnect, save/reload, partial/empty consumables, full inventory,
  equipped items, container rows, loose cables, flashlight crank/low battery and model previews.

Exact prior TODO text is preserved in [archive/pre-release-2150](archive/pre-release-2150/).
Tests, native evidence and the issue tracker remain available. Completed per-change
WORK reports have been removed from the active folders; their remaining follow-ups
are captured above. Update this file rather than creating another parallel work list.
