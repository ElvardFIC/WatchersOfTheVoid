# TODO — current priorities

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


Follow-up: the native Montage_Play wrapper now checks a readable finite positive
playback result instead of treating dispatch success as animation success. Refused
parameter writes/dispatch/results keep the receiver retry available. Cached actor and
AnimInstance identities guard the subsequent section jump against callback recycling.
Optional section failure preserves successful playback without restarting it.
run_wisp_animation.ps1 passes the receiver and actual wrapper regressions; all three
production units compile. Live multiplayer verification remains pending.

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


Current branding: Watchers of the Void, authored/maintained by ElvardFIC;
original Multivoid attribution remains Pelmentor. Delivery: dist/WatchersOfTheVoid-0.9.2151.zip.
New supplied cover and package page are prepared; Thunderstore upload is not performed.

Updated 18 October 2026. Use only the three active work documents:
[TESTLOG.md](TESTLOG.md) records user reports and their verification status,
[WORK.md](WORK.md) is the complete task backlog and implementation record, and this
file is the short priority handoff. Do not create numbered TODO, WORK, or TestLog
files; update these documents in place. Historical work/release numbers in their
contents identify prior evidence only.

- Packaged release/protocol: 2151; development protocol: 2152; includes all source work through development batch 2192.
- Both peers must use the current package, not an older development build.
- Full backlog: [WORK.md](WORK.md). Issue reports and their current verification status:
  [TESTLOG.md](TESTLOG.md). Do not close gameplay reports from offline checks alone.
  Reports49–58 were observed by the owner on2168.
- Release evidence: [RELEASE_2151.md](../package/RELEASE_2151.md).
  Runtime multiplayer verification is still pending.

## Next work

Current development target: **2152** (owner, 18 October). Personal horrors (10/49):
Insomniac spawn/attack stays peer-local; sleeping nightmares now use each player's
native enabled/bed/chance conditions during shared acceleration. A nightmare ends
the shared timelapse but its scene is not broadcast. Native checks, sleep-state
regressions and focused compilation pass; host/client gameplay remains pending.
Verify differing sleep levels, differing bed chances/disabled nightmares, simultaneous
encounters and reconnect. Packaged release/protocol is 2151; current development protocol is 2152.

2151 ragdoll (5/50): failed spawn/attachment and body loss retry with 500ms backoff.
Received pelvis coordinates now correct whole-body drift: initial/teleport alignment,
gradual normal correction and a 2cm resting dead zone. Expanded regression and both
production units compile. Models with their own valid articulated physics asset
can now use it, including a different skeleton; common-rig fallback is retained.
Bone/pelvis/joint validation and stale-asset regressions pass. T-pose in gameplay,
the local native body and exact limb pose remain open.

1. Run the ten priority host/client checks in WORK.md on the new package.
2. Bench58: simple placement disappears after1–2 seconds or later. Exact item and
   peer remain unknown; birth/save retry fixes are not a proven symptom resolution.
3. Identify token52's exact counter/UI before changing balance code.
4. Wall repair21/22 requires atomic cement request/result accounting across
   hand/slot/drop/retry/disconnect; preserve the existing no-spend refusal.
5. Continue remaining fixes from WORK.md; preserve relevant historical native evidence.

The user leads the project. No other agent workflow. Commit, push, game installation
and publication still need an explicit request. Preserve all uncommitted work.
Edit the canonical checkout's agent/ directly; there is no external mirror to sync.
Keep this handoff short; update TESTLOG.md, TODO.md, and WORK.md in place.
