# TestLog — current status and source reports

## Continued 2153: PC/laptop pages/cart and garden controls (19 October 2026)

PASS run_laptop_ui.ps1 -CompileSources: two executables, five production units.
The actual controller/chunker/parser tests cover holder/lease authorization,
same-slot reclaims, stale/malformed input, authoritative join seeds, local echo
protection, full-size multipart retry, independent recipient backpressure,
final-exit ordering/retry, unready widget, reentrant receipt and world cleanup.
The reflection/ParamFrame wrapper tests cover duplicate/order-sensitive cart
rows, prefix retention, live catalog key stamping, validation before mutation,
native slot reindexing, unnamed legacy cart recovery, effect readback, world
changes and refusal to invoke purchase/camera-possession functions.

PASS run_garden_sync.ps1 -CompileSources: seven executables/four production units.
New action tests execute actual faucet/detach parser and script hooks: duplicate
and rejected sequences, wrong peer/family/reach/endpoint, far-end reach, native
host detach cleanup, queue retention/shared blob sequence and session/world/slot
reset. Wrapper tests cover native action 5 and saved-key clearing before detach.
PASS run_world_sound.ps1 -CompileSources: one executable/three production units,
including the native faucet sound and zero-coordinate checks for 2D audio.

Installed cooked-asset contracts now include the faucet's action/sound and
PC/laptop cart mutators, native reindexing and camera side effects. PASS source
router gate with seven injected controls, now 191 ReliableKinds. Full Release
DLL builds at build/release-2153/main.dll (b2153), using restored local pinned
build inputs. Current logs: build/laptop-ui-validation.log,
build/garden-final-validation.log, build/sound-final-validation.log,
build/build-2153-final.log, build/reliablekind-ui.log and
build/sync-native-contract.json. No two-player gameplay or game installation.

Final artifact PASS: complete AMD64 PE sections, start_mod/uninstall_mod exports,
file/product version 0.9.0n b2153, Release (not Debug), ABI gate with injected
violator and no UE4SS imports. Completed link: build/build-2153-incremental.log;
Ninja dry-run has no pending work. DLL SHA-256:
f32c974fdd2ec0ea9e2c9f6f9648134b49d8600b547ccc816865f97fc513d615.
Manifest: build/sync-verification.json (14 executable checks plus asset/router/
ABI gates). The installed game was not modified.

Manual checks still needed on matching DLLs: base PC/portable-laptop handover,
duplicate cart rows and purchase refusal restore, exit immediately after a cart
edit, late join into a changed page/cart, faucet clicks and long-hose sprinkler
detachment followed by shared pickup/save/reload. The rest of TODO stays open.

## Continued 2153: guest hose routes and endpoint links (19 October 2026)

PASS run_garden_sync.ps1 -CompileSources: six executables (garden_sync,
garden_record, garden_wrapper, hose_route, hose_link, wall_enrollment_2167) and
three production units. The new route test uses production controller and
record/chunk transport: gesture deferral, explicit targeted acknowledgement,
canonical-root retention across quaternion noise, stale State/Reply completion,
conflicting edits, immutable retries, failed endpoint reach/ownership, two hoses
reserving one sprinkler, blocked host gesture/link, partial sends, unknown and
wrong-family identities, retirement, cancelled resume, slot/world callbacks.
The native wrapper test exercises actual reflection/ParamFrame code: pointer
and saved-key directions, nullable endpoints, former-owner cleanup, occupied
endpoint refusal, repeated geometry dispatch, missing dependencies/metadata,
world/serial guards and stale optional-key capture.

Native contracts verify installed 0.9.0n resume/finish moving/tick flags,
loadData versus processKeys/finish/gen, old component destruction and spline
clear, stale sprinkler_key on detach, and faucet/sprinkler saved-key fields.
PASS existing run_2166.ps1 -CompileSources. Logs: build/garden-hose-validation.log,
build/wall-hose-regression.log; asset hashes: build/sync-native-contract.json.
No DLL/package or two-player gameplay check. Completed routes are covered;
placement motion, standalone sprinkler manipulation and watering remain open.

## Continued 2153: gardening and native event sounds (19 October 2026)

Follow-up PASS in the same garden suite: client bucket LMB/fertilizer result
reserved before native spend, no result after native refusal, ordered transport
retry without expiry for completed care, cancelled-body reservation cleanup,
exact fertilizer weak identity, duplicate sequence and consumed source-key
refusal, reach/tool/amount bounds and canonical host updates. Wrapper tests check
native bucket ratio/minimum/capacity, empty proof, fertilizer stock=600 and stale
no-reset. Native contract JSON now includes bucket/fertilizer asset hashes.
This is owner-paid result publication, not durable acceptance/refunds across
host rejection, disconnect or rapid tool switches; those remain OPEN.

PASS run_garden_sync.ps1 -CompileSources: garden_sync, garden_record,
garden_wrapper and existing wall_enrollment_2167; three production units.
Coverage: native-key saved plants/taps/hoses, pre-session/solo host state,
growth/water/fertilizer/ripeness/death, initial creator ack and later updates,
missing-twin Refresh, malformed/foreign schemas, no identity mint on unkeyed
vegetation, fair capture budget, send retry, travel/disconnect. Guest harvest
checks reach/identity/generation/nonce/rate and retries requests without local
loot. Production ParamFrame checks verify the collector argument, packed discard
flag and ripe-to-harvested amount reset. Pocket refusal reports false while the
native harvest runs, including a disconnect inside it.

PASS run_world_sound.ps1 -CompileSources: one production-source executable and
world_sound_sync/event_cue_sync/subsystems compiles. Covers actual black-hole
continuation IDs, native audio parameters, alien/harvest positions, independent
peer FIFO/backpressure, missing assets/dispatch retry, accepted-send dedup,
recipient readiness, unknown/malformed cues, expiry/world/disconnect cleanup.

PASS sync_native_contract_test.py against extracted installed 0.9.0n assets;
native metadata/bytecode and SHA-256 evidence in build/sync-native-contract.json.
PASS existing run_2166, run_2168 and run_2164 suites/production compiles.
Evidence: build/garden-validation.log, build/world-sound-validation.log,
build/wall-placement-garden-regression.log, build/wall-garden-regression.log and
build/sky-sound-regression.log. MSVC 14.50.35717 / SDK 10.0.26100.0.
These are offline checks, not two-player proof. No full DLL/package was built.
Remaining synchronization scope and gameplay limitations are in TODO/WORK.

## Continued 2153: host wall material replay (19 October 2026)

wall_authority_2166_test.cpp exercises actual controller code: duplicate active
and unprocessed pairs, recursive drains, failed-feed dedup, exact material replay
permission, callback additions deferred to another tick and the eight-feed limit.
Collision callbacks stop/reinstall the session, change role/running state or
travel: no stale native feed/mutation resumes, no stale state is published, and
repair refusal writes isFine=true to both local/caller storage. Offline and
last-peer-departure host behavior remain native.

wall_wrapper_2166_test.cpp uses production ParamFrame/reflection calls to verify
post-query wall/material/world/class changes and overlap-component recycling or
replacement. Obsolete collision results do not consume material; restored live
components retry normally. FName conversion during travel prevents material lookup.

PASS run_2168.ps1 -CompileSources: five executables, five production units,
including newly covered wall_build_sync. PASS run_2166.ps1 -CompileSources:
four executables and five production units covering placement/save records.
Logs: build/wall-validation.log and build/wall-placement-validation.log.
MSVC 14.50.35717 / SDK 10.0.26100.0. No full DLL or gameplay validation.
Atomic client wall repair/material persistence is not implemented by these
queue fixes and remains OPEN.

## Continued 2153: playback progress and turn replacement (19 October 2026)

grayboar_actions_test.cpp verifies actual production Start/Verify/Stop behavior:
live/join playing and section getter failures resume verification without another
play/seek; stopped montages emit no sound; failed/no-op seeks and crossed snapshots
stop the stock montage; failed stops retry without replay and expire boundedly.
Snapshot playback succeeds without JumpToSection; live playback waits for it.
Missing stop/query/return metadata prevents start; native zero/refused playback
retries; unreadable/nonfinite/negative post-call results enter cleanup.

The callback matrix changes actor, AnimInstance, montage, world, session, registry
authority/mapping and mesh during start/seek/queries and death/damage sound setup.
No later audio/gameplay call escapes those changes. Replacement animation/mesh
instances do not inherit pending progress. A reentrant Receive plus nested Tick
preserves the in-flight phase and the following cue's order.

grayboar_turn_test.cpp verifies neutral steering and pitch after same-address actor
recycling or remapping, interpolation reset before another animation callback,
late-replica binding, authority/retirement cleanup and expired-capacity reuse at
receipt. Existing action/queue/turn tests still pass.

PASS run_entity_sync.ps1: five executables and twelve production-unit compiles,
MSVC 14.50.35717 / SDK 10.0.26100.0. Evidence:
build/entity-sync-check/validation.log and verification.json. No full DLL/package
or two-player run. A permanently refused engine stop remains a gameplay risk;
expiry prevents an indefinite queue/replay loop but cannot force engine cleanup.

## Continued 2153: queue isolation, fairness and identity (19 October 2026)

Expanded grayboar_actions_test.cpp runs the actual production module with multiple
recipients and actors. PASS: independent recipient FIFO across sequence wrap;
failed-send retry without duplicate delivery; 32 cues per peer / 96 total; healthy
peer delivery past another's capacity; expired/readiness/departed-slot cleanup;
join snapshots behind a saturated peer and retry-age advancement.

PASS: per-entity client FIFO with missing/late meshes; one action per entity and
four attempts per tick, including refused engine playback; round-robin progress
under continuous new arrivals; full queues/one-entity floods/expiry; weak identity
at receipt or late binding; recycled/remapped/retired mirrors; disconnect during
playback and reconnect. Join getters also exercise actor, session, world,
authority and recipient departure changes before publication.

PASS run_entity_sync.ps1: all five executables and twelve production-unit compiles
with MSVC 14.50.35717 / SDK 10.0.26100.0. Current log:
build/entity-sync-check/validation.log. Protocol and payload size are unchanged.
Full DLL/native-asset/gameplay validation remains unavailable in this checkout;
these results do not establish timing or creature behavior in a two-player game.

## Continued 2153: verified host montage before live cues (19 October 2026)

Current checkout is based on 0dce3afed (watchers-of-the-void-2150). New action
regressions exercise the actual grayboar_actions production code: attack/roar/
death require the expected active montage section and use its native rate.
Stopped/wrong sections, missing mesh/metadata, all four failed montage queries,
nonfinite/out-of-range values and actor/session/world/authority teardown refuse
publication. Existing mid-action join, damage/death/audio and retry tests pass.

PASS run_entity_sync.ps1: five regression executables, twelve production-unit
compilations, MSVC 14.50.35717 and Windows SDK 10.0.26100.0. Evidence:
build/entity-sync-check/validation.log. Run from the checkout after initializing
the local environment with `. ./build/local-msvc.ps1`. The runner now accepts
an initialized MSVC shell when the sibling private toolchain is unavailable.
PASS git diff --check. No full DLL build, native asset tests, two-player test or
package in this checkout; vcpkg and _research assets are absent. Earlier full
build/package results below are retained historical evidence, not current checks.

## Continued 2153: nonlethal hit animation/audio (19 October 2026)

Expanded grayboar_actions_test.cpp verifies four native-selected damage sections,
same-section repeat hits, finite health decrease, skipped/malformed arguments,
stopped/wrong montage, mirror/recycled identity, invalid rate and lethal precedence.
Client checks cover direct damage cue plus attack-audio stop, unchanged health,
four silent join snapshots and disconnect/recycle during sound setup. Native
asset tests pin damage1..4 RNG, skipAnim and the SoundCue/idle/attack audio chain.
Targeted C++/native tests PASS. New action values 4..7 reuse the existing 17-byte
payload; version stays 2153. Gameplay, simultaneous hits, native montage failure
and exact audio/network timing still require validation on matching peer DLLs.
PASS all nine focused runners and full MSVC Release b2153 build. git diff --check
is clean. Current DLL hash/scope: build/release-2152-logs/entity-validation.json.
Existing delivery ZIP untouched; no new version/package created.

## Continued 2153: AI continuations, collision and charged form (19 October 2026)

Expanded actual grayboar authority regressions to 24 owner/name registrations,
including separately declared charged overrides and their failed-watch retry.
All six AI latent destinations refuse client replicas; target/delegate cleanup,
construction, cosmetics and teardown retain native entry behavior. Existing
host/personal/offline, adopted/held identity and malformed-frame checks pass.

Native evidence pins collision impulse plus addDamage(10), lightning replacement,
charged explosion and death timer, player punch, random spin, senseUpdate/unhook,
setWalking mode changes and old dest delegate cleanup. Every watched function
must be exported by its declared base/charged package. Targeted C++ and native
tests PASS. Live replacement/effects, helper task retirement and two-player
results remain unverified. DLL/protocol stay 2153; no new package/version.
PASS all nine focused runners (five entity executables, twelve production units)
and full MSVC Release b2153 build. git diff --check is clean. Current hash and
scope: build/release-2152-logs/entity-validation.json. Existing ZIP untouched.

## Continued 2153: cosmetic boar turn and motor pitch (19 October 2026)

New grayboar_turn_test.cpp compiles the actual production module and verifies
host native sampling, 10 Hz limit, latest-state retries, constant-state join
heartbeat, world readiness, live host identity, malformed/range refusal,
host-only client acceptance, ordered/wrapping sequences, signed interpolation,
native turn overwrite, pitch=0.5+1.5*abs(turn), stale neutral fallback, queue cap,
missing audio metadata, recycled objects and reentrant/world/disconnect cleanup.
The entity runner now has five executables and twelve production-unit compiles.
Native asset tests pin BlueprintUpdateAnimation entry 3948, body/AI vector inputs,
ModifyBone consumers and the audio_loop_turn pitch formula. Targeted tests PASS.
Gameplay-visible steering and sound remain unverified on two peers; host/client
clock alignment and old helper delegates remain open. Development stays 2153.
PASS all nine runners in package/check_2152.ps1 and the full MSVC Release build
(454 rebuilt targets, b2153). git diff --check is clean. Current hash/evidence:
build/release-2152-logs/entity-validation.json. Existing delivery ZIP untouched.

## Continued 2153: adopted boar helper path abort (19 October 2026)

Added an active-path StopMovement call on a client replica's verified owned
grayboarPawn AIController. Idle paths and host/personal/unrelated controllers
are untouched. Actual production-source regressions cover late possession,
bad ownership, metadata/call retries, active/idle/invalid statuses, recycled
objects, world/session/authority changes, reentrant task completion and held
handoffs. Native bytecode confirms owner=self and the helper's AIController use.
Targeted boar executables and --boar-only native evidence PASS. Native engine
task/delegate lifecycle and visible turning still need two-player validation.
PASS all nine runners in package/check_2152.ps1 and full MSVC Release b2153 build
in the existing build directory. git diff --check is clean. Current DLL hash and
scope are recorded in build/release-2152-logs/entity-validation.json. No new ZIP.

## Continued 2153: boar navigation guards and build integration (19 October 2026)

Added client host-replica refusal for move/updMov/path checks, navigation retries
and completion, plus the native delayed move continuation at ubergraph entry 15.
Other graph entries remain native. Expanded actual-production tests and bytecode
evidence cover this bypass, identity/role gates and malformed EntryPoint frames.
Active pre-existing AI tasks and visible turning still require follow-up.

Fixed duplicate test/source/runner fragments and namespace closures found during
validation. Added missing includes/source-list entries for four already-called
creature modules so the existing full build links. Their gameplay is not verified
by this integration repair. Release DLL remains b2153; no new version/ZIP.
PASS all nine focused runners and full MSVC Release DLL build. Current evidence
and hash: build/release-2152-logs/entity-validation.json. No two-player validation.

## Continued 2153: death animation/audio and loot protection (19 October 2026)

Death is cosmetic action 3 on the existing 17-byte channel. A new host dying edge
plays the native death section at animationSpeed / 2; already-dying/skip-animation
paths do not emit another cue. Client playback silences idle/attack/roar, spawns
the native attached death sound, and never binds the native death delegates.
Joined death resumes the current montage position/rate without restarting sound.
Client host-replica damaged, death loot-notify and completion bodies are refused;
host/personal behavior remains native. This does not prove corpse/loot gameplay.

PASS all nine focused runners (four entity executables, eleven production-unit
compilations), expanded targeted boar tests and native asset evidence. Full MSVC
Release b2153 linked successfully in the existing build directory. Current hash
and scope: build/release-2152-logs/entity-validation.json. No ZIP/new version.
Still OPEN: two-player lethal client/host hits, death timing, single loot outcome,
retirement/reconnect, helper/turning, exact time alignment and joined audio.

## Continued 2153: joining during a boar action (19 October 2026)

Added a world-ready native montage snapshot for current attack/roar animation.
Registered host-boar candidates are validated by weak identity and sampled with
a per-tick budget. The 17-byte action payload carries current position/rate;
host/client queue time advances the resume position. The receiver verifies its
section and stops only the stock boar montage when the sampled action has ended.
One-shot sounds are not restarted for a join snapshot.

Production-module regressions cover active/stopped/dead/recycled candidates,
duplicate and departed slots, native position/rate, local waiting time, stale
section consumption and malformed positions. Exact network-time alignment, joined
audio, death and helper/turning remain OPEN; two-player validation is still needed.
Current build evidence/hash: build/release-2152-logs/entity-validation.json.
Development version remains 2153; no new package/version is created.
PASS all nine focused runners, including four entity executables and eleven
production-unit compilations. Full MSVC Release DLL builds as 0.9.0n b2153 in
the existing directory. The delivery ZIP is unchanged and predates this work.

## Continued 2153: live grayboar attack/roar cosmetics (19 October 2026)

Implemented host transition observation -> reliable kind 200 -> client direct
montage/section/audio playback, with no native damage/loot callback proxy replay.
Sequences, payload validation, bounded actor/animation retry and expiry, send
retry, slot departure and world/session reset are covered by production-module
regressions in grayboar_actions_test.cpp. Native evidence confirms the exact
`atttack`/`roar` sections, host animationSpeed and audio components.

PASS all nine focused runners. Entity runner now executes four regressions and
compiles eleven production units, including reliable dispatch. Full Release DLL
build stays 0.9.0n b2153; existing build directory reused, no ZIP/version created.
Hash and scope: build/release-2152-logs/entity-validation.json. Existing packaged
DLL/ZIP predates these changes. Matching current peer DLLs still need live tests.
Death cues, mid-action join, playback time alignment and helper/turning remain OPEN.

## Continued 2153: client target and roar authority (19 October 2026)

Extended the host-replica authority guard to grayboar sensePawn and detected.
Clients cannot independently assign a target/start target-driven movement through
sensePawn, or start the roar and bind target.OnDestroyed through detected.
The host, personal/local boars and inactive sessions keep native behavior.
Already-running helper tasks and pre-adoption target delegates remain follow-ups.

PASS run_entity_sync.ps1: three regression executables and nine production-unit
compilations. PASS full entity_sync_native_test.py: spawn catches, ancestry and
native target/move/roar side effects. Tests include an individual failed watch
registration recovering while installed neighbours remain intact. No two-player
test, shared action replay, new version, delivery DLL or ZIP produced.

## Continued 2153: client boar bite authority (19 October 2026)

Added production grayboar guards on the bite-zone overlap and attack montage's
OnNotifyBegin. Connected clients refuse these bodies only for live host replicas,
preventing independently started attack impulses and bite damage to int_objects.
Regression covers adopted/held identity, dead/recycled actors, descendants,
personal boars, host/disconnect fallthrough and watch-registration retry.
Extracted 0.9.0n bytecode confirms both guarded paths and their side effects.

Validation: package/check_2152.ps1 PASS all 9 runners, including the expanded
grayboar executable and nine production-unit compilations. Historical runner/log
names remain unchanged; current source/DLL/protocol stays 2153. No ZIP or new
version created. Two-player gameplay, shared montage/audio and mid-action join
remain unverified/open. Existing packaged DLL/ZIP does not contain this change.

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
[WORK.md](WORK.md); packaging does not close any unverified gameplay report.

Read [TODO.md](TODO.md) for current priorities and release restrictions. This file
combines current issue statuses with the original user report snapshot. Implemented
or locally checked does not mean multiplayer verified. Historical summaries are
preserved in agent/archive/.

| # | TestLog item | Current status |
|---|---|---|
|1|Client cannot break props|Код готов2143, дополнительная защита2159: FIFO повторов ударов и полная проверка границ параметров; needs new-build gameplay result|
|2|Prop jitter/rollback|Частично2151: one receiver drive per prop and late losing-release guard; native local grip contention/general jitter remain|
|3|Pushing one prop with another|Частично2157/2159: local/remote grips and deferred collision chains;2159 corrected empty legacy hit watch to actual hitCpp and checked component owner integration. Client-grab host contacts/convergence need two-player testing|
|4|Client R+LMB duplication|Код готов2143; needs gameplay result|
|5|Selected-model ragdoll|Reopened: owner still reports incorrect ragdoll on2168 (report50). Preserve2144 code, but do not treat this behavior as fixed.|
|6|Mining farm|Reported working|
|7|Extinguisher foam|Код готов2145|
|8|Loose power outlet floating|Код готов2145|
|9|Physics gun / both gun effects|Частично2158: native physical-gun target now feeds ordinary prop movement/release and local ownership, including soft/super descendants. Both guns' beam effects, scale/freeze outcomes, non-prop targets and multiplayer contention remain; gameplay needed|
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
|25|Battery levels|Частично2146/2147/2151; pending received record applied before pocket capture; direct hand/in-transit state still needs review|
|26|Airy event|Осталось|
|27|Main-room computer walls becoming physics props|Код общего пути2158/2159: независимая регистрация новых деталей и движение до остановки; подтверждены7 cubicleP-частей через обычную фабрику, проверены независимые появления. Нужна игра вдвоём на точных панелях. Снятие УДАРОМ, исчезновение У КЛИЕНТА; кто бьёт, не установлено. Пользователь предполагает все снимаемые ударами предметы; custom-пути пока не доказаны. Это не исключённые стены подвала|
|28|Drone sound difference|Код готов2143|
|29|Crowbar static state / dragging|Частично2148; unusual wall/parent/replacement classes unconfirmed|
|30|Second ATV in corridor|Код готов2149 for new companion creation; old saved positions unchanged|
|31|Sleeping body pose|Осталось; native sleepingPawn is camera/input, not a ready-made body. Anchor/ragdoll/selected-model integration needs implementation.|
|32|Not enough space on equipment|Код готов2146 for ALL equippable items|
|33|Open container retains removed row|Код готов2146|
|34|Client crate/package loot invisible|Частично2148/2150/2158: MRE floor/hand, host finished-birth retry; keyed spawn hook no longer waits for all optional ambient assets/observer registration. Other packages/client-local routes remain, gameplay pending|
|35|Centipede / other living creatures invisible|Осталось|
|36|Computer chair rotation (duplicate report)|Same as11,2144|
|37|Riding a carried ATV / flight|Код готов2148/2149; actual menu and race convergence included|
|38|Skin model list / previews|Частично2151/2152: original sixteen preview tiles plus supplied Mynet/skeleton images; both requested meshes were already builtins. Catalog regression passed; runtime appearance/animation/ragdoll still needs gameplay, other preview gaps remain|
|39|Consumables refill on pickup/drop|Частично2146/2147/2151; food earlier fix retained, pending canonical record drained before pocket capture; other transitions remain|
|40|Flesh Eerie summon / visibility / aggro|Осталось|
|41|Secret basement wall|Исключено по прямой просьбе пользователя; не возвращаться к исправлению|
|42|Event teleport to the triggering/base-entering player|Код2161: actual agrav occupancy and runtime-verified shared armed boxes; client pose fallback, initiator gather, repeat/reentry guards and send retries. Two focused checks/five TUs passed; multiplayer pending. Other custom paths and full event effects remain separate|
|43|Safe cracking|Код2162: both dial success and hacked converge on opened; client completion to host, single loot, saved lock publication/retries and host-lock protection against early/stale records. Initial unlocked drop record preserved. Gameplay pending; dial UI and generic child hinge identity unchanged|
|44|Sponge wetting|Частично2160/2169: native saved power/wetness now publishes after enterWater/wash/clamp/LMB; load no echo and final-use tail checked. 2169 adds current held wetness to outgoing profiles. Soap stock/foam/surface effects and personal-hand transitions remain unverified; two-player test needed|
|45|Grill ignition|Код2162: host ignition/off, active/coals/burn snapshot and late join, passive client heating/fuel tick, immediate coal record and request retries. Gameplay pending. Concurrent coal transactions/stale records, lid and personal item transitions remain.|
|46|Client-placed crops/faucets/hoses disappear after save reload|Частично2163: exact native placement -> host save actors, completed hose points/links, plant/pot identity, retry/dedup/retirement/late join. Two tests/five TUs passed; game save/reload not tested. Later gardening, accepted hose rerouting, concurrency and pre-session loaded-actor enrollment remain.|

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
|52|Client token display remains desynchronized|Open. Identify the token counter/UI and its authoritative value; do not assume credits or a particular token class without evidence.|
|53|Base light flickering is not synchronized|Partial2177–2180: host-authored ceiling/ambient phases; indexed clients suppress native writes; local ambient settings remain local. Failed applies retain latest state;2180 retries failed/unreadable join snapshots without advancing the all-peer baseline, with peer/world cleanup. Channel/wrapper regressions, prior native checks and five production TUs pass. Actual keys, loading/late join and two-player timing need gameplay verification.|
|54|Computer display does not show the same activity to host/client|Open. Other players cannot see who is using the computer or what they do. Trace shared screen/UI state, cursor/input and operator visibility; console text12 alone is insufficient.|
|55|Computer power-on and power-off are not synchronized|Partial2173: targets survive actor loading and native animation; failed sends and deferred join snapshots retry with latest state. Pending sends clear on peer departure/disconnect; 2171 echo suppression is preserved. Existing animation regression, new delivery/lifecycle regression and two production TUs pass. Still requires two-player checks, power loss/restoration and late join; no release built. Separate from water short-circuit/damage18/19.|
|56|ZIP disk model sometimes becomes a huge owo when ejected|Partial2172: laptop `zip` is now resolved as a masked Unreal bool bitfield, written without disturbing neighboring flags, and hashed as its logical value. Focused wrapper regression and production TU compile pass. This is a concrete wrapper defect but is not proven to cause the reported model/eject symptom; still needs in-game ZIP eject and multiplayer checks.|
|57|Opening canned food and eating it are not synchronized|Partial2176: world open/eat publish records (2174), held can state travels with HandItem (2175), and client profiles refresh live opened/type/foods before hashing (2176). Identity-checked copied records preserve unrelated data. Three profile/record regressions, native evidence and two production TUs pass. Protocol2175. World/held use, reconnect, final portion, stow/re-equip, drop/pickup and simultaneous consumption still need multiplayer verification.|
|58|Some items disappear when placed on the workbench|Open symptom; partial code2181–2186. Owner clarified17 October: simple placement, visible disappearance after1–2 seconds or later; no crafting reported. Items and host/client unknown. Transform retries, local-birth protection and deletion identity rechecks implemented;2186 invalidates old sweep passes on bracket/world/reset changes and blocks recursive reconcile ticks. Focused tests/production compilation pass. Full sweep/gameplay symptom remains unverified.|

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
