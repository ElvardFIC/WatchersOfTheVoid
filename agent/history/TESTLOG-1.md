# TestLog — current status and source reports - historical reports 1

[Current status](../TESTLOG.md).
## Development b2178 continuation (21 October 2026)

Focused PASS `run_placement.ps1` and `run_2161.ps1 -CompileSources`.
New `remote_destroy_lifetime_test.cpp` includes the actual production receiver with
native/reflection boundaries doubled. Before the fix it failed when a destroy
listener recycled the selected object's slot; after the fix it covers listener,
metadata lookup, drive/grab release and unpin replacement, death, world/session
changes, recursive destroy, listener registration, ordinary success, local consume,
conversion cleanup, deferred application and preserved load-tail behavior.

The location harness first failed because a new session serial at the same pointer
and world still counted as current. After the fix, session reuse, pose teardown,
pawn serial replacement, native bounds-query world changes, recursive ticks and
actual-overlap cancellation pass along with existing geometry/authority cases.
All five location-related production TUs compile separately.

Full qualification **PASS, 65/65 groups**, via
`package/check_current.ps1 -EvidenceDirectory build/continuation-check`:
unchanged full production snapshot, 781 build entries, final incremental Release
build up to date, loader ABI/imports/exports and 224 reliable kinds/18 routing
fault controls. Evidence: `build/continuation-check/{verification.json,identity.json,results.json,build.log,logs/}`;
initial Release compilation: `build/continuation-build.log`.
DLL SHA256: `9e5f4fd23b272efa6fd807df45cba466664acfe8639c5d4d639a9f23c9c4d5f8`.
Production snapshot: `5abdd52bf142d0a452b16f904163a7496622fe11be0bcc382975f0f965eeb1e7`.
An earlier changing-tree attempt was refused by the source-stamp check after the
final echo-mark ordering fix; this stable-tree result supersedes it. The final
regression also verifies no incoming-destroy mark survives a cancelled attempt.
The source protocol is b2178; the previously prepared package 0.9.2155 remains
b2177. Its outer ZIP SHA256 remains
`37430bd3701e56d72de7960d9f6b4afaf87311ef57e63d70d225eb7ac982048f`.
These later changes are recorded under Unreleased in the changelog.
No new ZIP, commit, push, game installation or publication was performed.

Bench58 remains open: no host old-rifle disappearance was reproduced in the game.
Existing-corpus inspection also checked inherited prop lifespan serialization,
native inventory cleanup and the gun-stealer's literal `arirGun` target; no broad
rifle-destruction or lifespan behavior was disabled. Walking-tree bytecode has
viewer-dependent behavior and latent timers, so its rate/authority route remains
deferred rather than being admitted solely by class. Token52 is unidentified;
fresh pak audit and real-peer acceptance remain deferred/unverified as before.

## Package 0.9.2155, stable Release b2177 (21 October 2026)

PASS `package/build_2155.ps1`: Windows x64 Release, **65/65 offline validation
groups**, unchanged full production snapshot across tests and final incremental
build, 781 build source entries, identity/imports/exports, 224 reliable message
kinds and 18 rejected routing fault controls.
DLL SHA256: `4ab599f644354070a272f79037c023dd65bcadc5ac55a16ab38f7017e02fa304`.
Evidence: `build/release-2155-logs/{verification.json,identity.json,results.json,initial-build.log,build.log,logs/}`.
The historical build directory remains `build/votv-coop-2154`; its DLL is b2177.

New production regressions cover:

- Owned/bounded event replay across recursive native callbacks, queued replacement,
  world/disconnect/session changes, metadata-readiness retries and consumed ambiguous
  native attempts. Preparation failures remain retryable; actual attempted native
  calls are not repeated after uncertain completion.
- Active-event begin/end sender serials, null/bounded frames, copied registry
  membership, failed/readiness join debt, recipient replacement and teardown.
- Owner-local night-eyes frequency, native interval restoration and context changes
  during getters. `event_rate_native_test.py` checks the existing cooked ticker's
  1800–2700s wait, 5% roll, day/night gates and owner-entity route. The complete
  random-event list is not covered; no fresh pak audit was performed.
- Exact-identity native held-trowel stock persistence, zero/refill, malformed schemas,
  changed slot bytes/actor/save/world/stock and callback rejection. `run_trowel.ps1`
  uses the production inventory function and extracts the actual saved-int helper
  and canonical offset from production for its array-schema harness.
- Admitted client placement recovery after exhausted send/transform/food-capture
  retries, bounded/full recovery pools, sweep immunity and no fallback shared birth.
  These checks include `prop_rifle_C` as a fixture, not a host disappearance repro.

Latest Bench58 owner answer: **host**, item **old rifle**. Components remain
unidentified. Existing `prop_rifle` leaf Blueprint has destructive LMB use without
a placement entry point; inherited behavior and actual disappearance cause remain
unverified. Decoding evidence: `build/release-2155-logs/prop_rifle-existing-corpus.txt`.
Use the existing `VOTVCOOP_PROP_BIRTH_KEY_PROBE=1` diagnostic before a reproduction;
capture both peers' birth/destroy caller/class/key and cleanup/drop logs.
Token52's counter/UI is still unidentified; no balances were changed for it.

Full native-contract audit remains unavailable against the incomplete corpus and
fresh extraction remains deferred. Full public-prose/fact-row CI retains the known
failures documented below. No real-peer event/frequency/join/reconnect, native
physics, trowel reconstruction or host-rifle acceptance is claimed.

Package/delivery validation is recorded separately in
`build/release-2155-logs/release-verification.json`: PASS, 32 archive members,
qualified embedded DLL and current documents/manifest/paks, matching outer copy.
All changes since package 2154,
including the fish integration, are listed in `package/CHANGELOG_PLUS.md`.
No commit, push, game installation or online publication is performed.

## Fish integration into main, protocol b2176 (21 October 2026)

Source: `watchers-of-the-void-fish` at
`e48f19b8717c8b695c6d235499d0baf31fccbf6b`, relative to `b6651cb91`;
main integration baseline: `74e1729e5`. See WORK.md for the behavior inventory
and HANDOFF_FISH.md for the original branch's qualification and open work.

The merged protocol keeps Kavotia=223 and placement-result=224; Watoy moves to
235, with new fork kinds 225..234. Placement requests/replies are 188/101 bytes;
cooking progress, tool/player identity and other new layouts require matching
b2176 peers. The authority table and all dispatch/lane/build/lifecycle entries
preserve main's Entity, event-frequency/delivery and personal-sleep changes.

Conflict-specific checks exercise the actual placement and coin controllers:
acceptance-only record publication, same-key refusal, active/live key reservations
before index enrollment, synchronous replies, immutable retries, retained replies,
actor/world/occupant changes, readiness, malformed strings/padding/cooking values,
pre-eid coin reservation and retirement after faulted native destruction.
Existing fault controls now expand the production .inc fragments before mutation;
the queue-cap control exercises simultaneous retained captures and callback arrivals.

Final stable-tree qualification: **PASS, 63/63 groups**, production snapshot
unchanged through tests and final incremental Release build; **781 source entries**.
Command: `package/check_current.ps1 -EvidenceDirectory build/fish-integration-check`.
DLL: `build/votv-coop-2154/main.dll`, **0.9.0n b2176**.
SHA256: `b069a8cfb542c86a7569c495302a882003ef5a245feb1f126210ce26b02355c1`.
Evidence: `build/fish-integration-check/{verification.json,identity.json,results.json,build.log,logs/}`.
Identity, imports/exports and all 224 reliable kinds pass; all 18 routing fault
controls are rejected. The full source/config hash is in verification.json.
The earlier changing-tree pass is superseded by this final run. Its failures
identified merged duplicate test doubles, moved production-fragment includes and
an outdated queue-cap fault fixture; focused reruns passed after adaptation.
No passing result is claimed for the changing-tree snapshot.

Additional validation limits:

- `native_physics_fingerprints_test.py`: three captured encoding controls and two
  documented derived signatures pass. This is not a fresh installed-game census.
- `public_prose_include_test.py`: all four include-context cases pass.
- `sync_native_contract_test.py` was not rerun: this workstation lacks 18 of its
  45 literal package inputs, and additional dynamic inputs are required. The
  owner's fresh pak/Blueprint audit remains deferred. The branch's original
  58-file installed-pak contract qualification is provenance, not a local result.
- The complete working-tree public-prose census, including new unstaged source
  files, remains above baseline on 14 counters (1905 source files;
  `build/fish-public-prose-working-tree.json`). The normal tracked-only command
  reports 15 counters because it cannot see new callers in untracked files.
- `fact_rows_gate.py` remains red across 525 rows/157 files, including missing
  native-hook references and unused existing rows; details are in
  `build/fish-fact-rows.log`. Full repository CI is not claimed green.

Two-peer gameplay, physical projectile/collision behavior, atomic resource
spending/refund, ordinary paid cement repairs, remaining dynamic grime sources
and scorch provenance through full world/save reload remain open. No package,
game installation, commit, push or publication was performed.

## Event synchronization: stable Release b2159, 40/40 groups (21 October 2026)

PASS `package/check_current.ps1 -EvidenceDirectory build/event-sync-check/validation-2159`:
all **40/40** groups, production-source snapshot unchanged across tests/final
incremental Release build, DLL identity/imports/exports and 762 source entries.
This result supersedes the earlier changing-tree attempt below.

DLL: `build/votv-coop-2154/main.dll`, **0.9.0n b2159**.
SHA256: `87a0ba2a4190fd383eb8abb31a13f1982e958f1964a87283fd8eefa80902f49c`.
Evidence: `build/event-sync-check/validation-2159/{verification.json,results.json,identity.json,build.log,logs/}`.

New `run_events.ps1` checks chosen/repeated native specials, direct F1 emission,
reflected scheduler calls, refused-send ordering and healthy-recipient dedupe,
peer-slot reuse, world/session replacement, reentrant send teardown, client-only
replay/admission, malformed dispatch and install-pump load-window retry. The rate
harness covers four exact spawner classes, default2x/live0.5..5x changes, native
re-arming, no cumulative scaling, failed setter retry, pre-session/late actors,
CDO/foreign-world/personal ticker exclusions, restoration/recycled identities and
reentrant engine callbacks. All four event production TUs compile separately.
Related location/gather, schedule/spawn authority, personal-sleep and world-sound
regressions pass as part of the 40-group run.

The complete eight-category event request is still OPEN. The owner requested
skipping the fresh .pak/Blueprint audit for now; the four rate-controlled
generators do not establish coverage of every repeatable random event. Full
two-peer effects, late join, simultaneous activation and gameplay remain untested.
No package, game installation, commit or publication was performed.

Additional source gates are outside that 40-runner suite: fact_rows_gate still
reports existing unused rows/missing native fingerprints in other lanes, and
l10n_pot --check reports stale catalogues/obsolete existing translation entries.
The new native interval row has a reference entry; no fingerprint was fabricated.

## Earlier source 2159 changing-tree attempt (21 October 2026)

The owner confirms parallel event work remains active. A later Release b2159 build
succeeded, but it is not the completed b2158 audit snapshot below. All39 runners
passed in the later attempt; identity verification refused the old DLL when source
protocol advanced during the run. Preserve that distinction rather than counting
the changing-tree attempt as a matching full validation.

The current checker now checks production-input hashes across tests and a final
incremental Release build. Syntax and source-stamp generation pass. Its stable full
run must follow completion of the other task. Current build metadata and the last
fully verified snapshot are separately recorded in build/todo-audit/verification.json.

## Full TODO audit: 39 groups and Release b2158 (21 October 2026)

PASS package/check_current.ps1: **39/39** groups, current Release identity/exports/
imports/source list, retained wire IDs202/203/204, and the reliable router's18 failure
controls. Evidence: build/todo-audit/verification.json, results.json and logs/.
Verified audit snapshot: 0.9.0n b2158; source count760. The mutable
build/votv-coop-2154/main.dll path has since been rebuilt by the parallel continuation.
SHA256: d8cd0c113cc0429160a70ec4e3fa042908e2c6b447e7208ae3a5e7a806300d23.

The audit fixed stale held-mirror publication/duplicate birth and the missing laptop
ON/OFF retry after loading/animation/power recovery. Both production regressions pass.
Restored the older API-drifted runners rather than relying on historical pass reports:
record/placement mutation controls, item/codec/pry/birth paths, native explosion suites,
cord performance, console/toys, resource wrappers, teleport, lamp and laptop recovery.
The expected assertion failures in mutation logs are rejected mutants, not failed groups.

Reviewed every listed fork candidate; upstream tab/camera/NPC-join/lobby fixes are
already merged. Local explosion lanes are retained because upstream's177 conflicts
with KitchenState177; affected-prop coasting remains unimplemented.

Bench58: owner now identifies an old rifle and components, in one isolated placement
case. Exact component and authoring role are unknown; the prop_rifle_C candidate's
placement refusal/sweep protection passes offline. The disappearance itself has no
new gameplay result. Token52 remains unidentified by the owner.

TODO now checks confirmed source work separately from gameplay. Cement/resource
transactions, remaining creature/event adapters and other explicitly open code work
are not marked complete. No two-peer gameplay, package, installation or publication.

## Continued 2158: placement acknowledgements and coin collection (21 October 2026)

PASS all 16 validation groups on the merged tree: the nine prior focused groups,
garden, laptop UI, world sound, placement/merged ABI, coin handoff/collect and its
production TU, native coin bytecode and reliablekind_gate --drill. The router
reports 213 kinds, no duplicate claims and all 18 injected failures rejected.
New compiled ABI checks pin retained PropScaleState 202, SharedScreen 203 and
LaptopUI 204, placement request/result sizes 184/8, host-only result authority,
Bulk ordering with PropSpawn/PropDestroy and incompatible-version refusal.

Placement regressions exercise invalid lengths/NaN, existing-key conflict,
forbidden fresh birth, failed spawn, send retry, repeated and reentrant requests,
same-key concurrent requests, eviction replay refusal, peer departure and reset
during send. Client checks cover refusal/timeout protection and recovery, stale
sender exclusion, duplicate ACKs, late adoption, both ACK/bind orders and late
acceptance. Save records are absent before acceptance and on refusal; capture/send
callbacks cannot revive reset work. Existing food/transform/birth tests also pass.

Coin regressions exercise host E/overlap and forwarded requests racing inside
native dispatch, including nested calls marked fromOurCode, pre-eid overlap,
native no-op retries, unreadable balance, partial dispatch failure, recycled
identity, world change, teardown, malformed/unauthorized requests and transport
retry reentry/reset. Personal map coins stay native on clients. Native asset
test pins E -> graph 441, overlap -> mainPlayer cast -> graph 441, then points,
hint and native destroy; the shared credit block ignores player parameters.

Full MSVC x64 Release build and final incremental link PASS. DLL file/product
version **0.9.0n b2158**, SHA256:
`a04891ee860b33033091bcd69c73662176f2c8162cde53275ac6fe461dae7252`.
package/verify_current_build.ps1 derives the expected identity from source and
passes Release configuration, imports/loader exports and all 760 listed source
paths. Evidence: `build/entity-sync-check/validation-2158/{tests.json,identity.json,
verification.json,logs}`. Previous b2157 evidence remains historical; the build
directory now holds b2158. Reconfirmed origin/multivoid-sync-2153 ancestry at
HEAD 023f256168f0940c74d1eea7a3bf34e02f2dd8fd with continuation edits present.

No two-peer gameplay tests or new release archive. Existing C4715 in
npc_pose_drive.cpp and EH option warnings remain. Deployment/packaging remains
separate; archived 2154 package identity checks describe b2156, while current
development checks use verify_current_build.ps1.

## Continued 2157: Kavotia state, authorship merge repair and personal effects (20 October 2026)

Implemented Kavotia's native aim/stance and charge-loop/pitch/windup snapshot on
host-only reliable kind 223 (25 bytes), sharing Grayboar's bounded queue helpers.
The adapter preserves native mesh initialization, refuses client latent AI/shot
entries, samples host actor ticks even for culled meshes, repairs a join through
heartbeat and stops stale loops independently of particle/pitch availability.
Actual-source tests cover authority/pre-bind/malformed frames, bounds, sequence
wrap, delayed binding, metadata/parameter retries, completed-operation retention,
component/actor replacement, reentrant callbacks/receive/teardown, four rows/tick,
32 pending per peer and full-pack join repair. Native bytecode assertions pin the
setup entry, native movement rates, charge graph and exact burst/montage names.
The shared fanout also has a Grayboar regression for peer departure erasing a cue
inside its send callback; a delivery revision aborts that pass before indexing the
changed queue, and remaining peers resume in FIFO order on the next tick.

CI exposed 38 pre-existing authorship omissions after the merge. Repaired 27
client-intent/owner-sample rows and 11 host-only rows by their real paths. The
reliable-kind gate passes all 212 kinds and 18 injected failure controls after
fixing its false-return parsing and stale family-dispatch mutation.

Wisp tests found an actual missing sender check on WispGrab; fixed it and added
immediate death/hold/tear cancellation on original-pawn loss/recycling/replacement,
native death and world changes. A grab rearmed during release survives without
receiving the old kill. Removed blanket client nightmare suppression; the updated
sleep test checks preserved native probability/entry, unchanged wake/rest state,
real rest-report layout and sender authority. Updated the stale preview-spawn stub.

Release `0.9.0n b2157` builds. All 13 groups pass: player reports, wisp animation/
release, ragdoll retry/skins, personal sleep, entity sync, garden, laptop UI, world
sound, three native asset audits and reliable-kind CI/drill. Evidence and DLL hash:
`build/entity-sync-check/validation-2157/verification.json`. Existing C4715 in
`npc_pose_drive.cpp` and MSVC EH-option warnings remain. No two-player gameplay or
new package; Kavotia fire/laying montage, voice and beam/impact/damage remain open.

## Sync-2153 ancestry audit and entity-route repair (20 October 2026)

Verified `multivoid-sync-2153` tip `bf276d14c4d30dc1ba95ea9ea184f7d9e89dedef`
is parent 2 of merge `7fced281e92c4cb2df0e37797bb533041022b087` and an ancestor
of current `main`. The merge's ten manually resolved paths retain local reliable
kinds 202/203, assign Laptop UI 204 and combine both Grayboar implementations.
The sync branch's laptop, garden, world-sound and placed-hose source files remain
connected to the build/session. Current source is protocol 2156.

The entity audit found that `npc_world_enum.cpp` logged and the native test read a
dead EX-spawn source table, while the executed table omitted the Fossilhound and
yellow-wisp tickers plus boar-war/tank controllers. There is now one executed table;
the runner asserts its required sources and rejects a second table. The existing
`ariral_sync` watched nonexistent `StealGun`/`TakeWeapon` exports. Native 0.9.0n
shows a timer export named `punching`; the gun stealer is now a late NPC class and
only a confirmed client host-mirror refuses that timer, preventing duplicate player,
prop and door effects. Gun attachment/held-item state remains open rather than being
claimed from pose replication.

The reusable part of the Grayboar action path now lives in
`coop/creatures/reliable_action_queue.h`: non-zero wrapping sequences, ordered
receive admission, bounded per-peer fanout, expiry, independent FIFO retry and
departed-peer cleanup. `grayboar_actions.cpp` consumes that layer without a wire
change. Its existing saturation, wrap, retry, expiry, reconnect, late-join and
reentrant playback tests all pass. Entity-specific native sampling and cosmetic
playback remain in adapters; gameplay verbs are not replayed by the shared queue.

`agent/tests/run_entity_sync.ps1` passes all entity/Grayboar/prop-drive regressions
and production compilation, including `npc_world_enum.cpp` and `ariral_sync.cpp`.
The existing `npc_pose_drive.cpp` C4715 warning remains. Native Python evidence was
updated but could not run in this environment because only the Windows Store Python
alias is installed. No two-player gameplay claim, DLL rebuild or package was made.

## Merge resolution - local branch with multivoid-sync-2153 (19 October 2026)

Resolved the Grayboar source/test conflicts by retaining local authority and
identity checks and adding the remote stale-replica and per-peer retry coverage.
Kept local wire kinds 202/203 and assigned LaptopUI kind 204. Source protocol is
2154. Merge whitespace check passed; no build or gameplay tests were run for the
combined tree, and no delivery artifact was created.

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
