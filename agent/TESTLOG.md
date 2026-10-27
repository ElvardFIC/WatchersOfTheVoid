# 2162 hosting game-mode selection (27 October 2026)

Owner explicitly requests Tutorial, Story Mode, Infinite Mode, Ambience Mode,
Sandbox Mode and Solar Mode. Implement named G1-G3, then remove the completed
implementation entries from TODO; validation remains in this document and WORK.

PASS run_game_modes: actual new-save map and mode-application helpers plus the
shared six-choice UI model, with native/reflection boundaries doubled. Check
ordinals 2/0/1/6/4/7, default Story, invalid hidden/unknown selection refusal,
per-mode mainMap, write/readback retry, existing-save mode ownership and retired
save/class identity around FName construction. Source wiring covers both menus,
session recap, captured new-game boot mode, Tutorial map travel/world recognition
and the existing client mode-transfer byte. Cooked primary assets confirm the
mode ordinals, tutorial3 first-level call, native mainMap creation and prefixes.

PASS expanded run_host_players: actual hosting controllers for all six modes,
all 31 player limits, new/existing slots, direct/brokered, listed/hidden and master
failure branches. Invalid new-game mode refuses before queue/announcement. Its
local server library suite also passes 118 tests. PASS run_upstream_integration,
run_save_record, sync_native_contract and reliablekind_gate. These six registered
groups pass in build/game-mode-feature-final/verification.json, with stable
production source and Release b2217 / 854 production C++ entries.
FullRegression=false: this is a focused feature qualification, not all 123 groups.

PASS separate run_2164 sky regression (build/game-modes-check/sky.log),
l10n_pot --check (443 entries) and abi_gate with its injected rejection control
(build/game-modes-check/abi.log). Fresh DLL SHA256:
efadfe9a41ff076c77378e2acba8f2e4e6c7161fa097dc900830e901e8d2c7c6.

An earlier attempted focused run named the unregistered run_2164 group and
stopped before building; its partial evidence remains in build/game-mode-feature.
The final registered run completed successfully, and run_2164 was executed
separately. No ignored failure is counted as a passing group.

Native and fallback hosting select the same mode, settings show the captured
choice, existing saves retain their own mode, and native widget pointers retire
with the menu. Tutorial and its chapter worlds count as gameplay; new Tutorial
starts at /Game/maps/tutorial3. Sandbox uses the main map. No additional Sandbox
map picker, wire format change, protocol/public-version bump, archive, install,
commit or publication. Rendered menus and real-peer gameplay for these modes
remain unperformed acceptance; no fix to the prior flashlight/T-pose reports is
claimed by this feature.

# 2162 native flashlight audit repairs (27 October 2026)

Owner removes all validation/acceptance tasks from TODO while the requested
75-task work is ongoing. Replace the superseded B01-B75 selection with an
implementation-only backlog: 38 entries before these three repairs, 35 remaining.
Keep validation here/in WORK; do not claim 75 tasks completed.

Implemented A10: refuse each native light setter when ParamFrame writes fail.
Implemented A11: apply ItemActivate inside the existing admitted game-thread
inbox drain, removing the extra task that could survive session/occupant turnover.
Implemented A12: reject resolved Player id/sender-slot mismatch and malformed
null/length/slot boundaries before reading or applying ItemActivate.

PASS new run_flashlight_native: extracted actual setter/dispatcher bodies, real
ParamFrame/call.cpp, reflection/native/registry boundaries doubled. Exact setter
bytes, missing/overflowing fields, negative/zero frames, dead/null targets and
native refusal; immediate apply/no posted work, correct host/relayed origins,
Player spoofing, self echo, invalid slots/lengths and pre-pose retention.
PASS related run_flashlight_join, run_flashlight_click, run_2145 and
reliablekind_gate. PASS check_current's focused five-group run, stable production
source and Release/loader import/export verification. Evidence:
build/flashlight-audit-fixes/verification.json (FullRegression=false), logs/,
build.log. Register the new native group for future full qualification.
DLL SHA256: 35924fa834fb0e5af38e153dc36aa62c13fe22fdcf3393497f30972314aea056.

No full 122-group rerun, archive, game install, publication or version/protocol
change. Remote-light visibility and the reported T-pose remain unclosed gameplay
defects; these three code repairs do not prove their original cause fixed.

# 2162 offline audit (27 October 2026)

Owner reports: flashlight visible only locally on every peer; falling produces a
T-pose on all players. These reports remain OPEN: no rendered host/client
reproduction or verified gameplay repair was performed.

Baseline build/audit-current: run_sat_console_callbacks and run_flashlight_join
fail to compile. Review b9a087de9's entire delta: it adds duplicate SAT-console
blocks (60 lines) and duplicate flashlight fixture definitions (one line).
Remove exactly those additions. Compare pre-merge contents: identical. The only
remaining difference from that TU's last-qualified file hash is LF/CRLF.

PASS package/check_current.ps1 -EvidenceDirectory build/audit-current-fixed:
121/121 registered groups, stable production source SHA256
caeeb4982b06fcfd1205ee9fc50f800791042e820761a16fd128af92c4d5c200,
Release b2217, 854 production entries, loader exports and static runtime imports.
PASS abi_gate.py main.dll --drill (including the injected rejection control).
PASS verify_package_label.py: production formatter still 0.9.2161.
DLL SHA256: 03fe830ac55f758c65b8554e9840ae9c6113b19590f66567157d06983a3b8062.

PASS 132 server tests via portable Rust/MSVC, cargo test --locked --offline
--manifest-path server/Cargo.toml (118 library + 6 identity + 7 lobby + 1 cost).
PASS master-contact, workflow-expression, reliable-kind, fact-row, data-path,
plural-vector, peer-connection, registry, GC-pin, MinHook, atlas and captured
fingerprint checks. Initial l10n_pot check FAIL: stale template/pseudolocale.
Regenerate from current call sites; recheck PASS (443 entries).

Run all 37 scripts absent from the main runner's direct list as well. Initial
supplemental result: 18 pass / 19 fail. Repair legacy toolchain, Python and
research paths and run with the portable build environment: final 23 pass /
14 fail. Details and per-runner causes are in WORK and
build/audit-supplemental-final/results.json; preserve failures without weakening
assertions. Some supplemental scripts are already transitively covered by the
main suite, so these counts are script runs, not independent unique test groups.

Inventory: 140 regression scripts, all 1089 production/development source units
listed and present, no unlisted .cpp. Compare all 2142 last-qualified snapshot
inputs: exact matches except the repaired SAT-console's line endings.
Evidence: build/audit-gates/inventory.json, abi.log, package-label.log,
server-tests.log, results.json and logs. The original results.json retains the
initial failing l10n gate; the regenerated template subsequently passed.

Static review also finds unchecked native flashlight frame setters, unbound
deferred ItemActivate delivery and missing payload-EID versus sender-slot
validation. These are OPEN code findings, not verified fixes for the owner's
constant missing-light report. Ragdoll configuration resets to reference pose
before physics takeover; rendered transfer remains OPEN. TODO A8–A13 track the
remaining work. No new archive, install, commit, push or publication.

# 2161 package preparation (27 October 2026)

Owner explicitly requested the 2161 build. PASS package/build_current.ps1:
complete fresh offline suite **121/121 groups**, Release/ABI with 854 source entries,
all 2142 source-snapshot inputs, source stability and the production 0.9.2161 label.
Public/internal/protocol identities remain 0.9.2161 / b2217 / 2217.

First full run: 120/121 passed; run_upstream_integration had a stale assertion for
15 protocol additions. Replace it with the exact 16 name/ID mapping, including
already-implemented LlamaState 253, while retaining all 226 published 2214 values.
The corrected test passes and the entire fresh 121-group suite was rerun before
packaging. No production code or DLL changed during this test maintenance.
Audit: build/package-current/full-suite-retry.json.

PASS manager archive validation: **32 members**, manifest/dependency/loader layout,
activation marker, 256x256 icon, bundled models, current docs and byte-identical
qualified DLL. PASS all five rejection controls: failed group, stale label,
wrong DLL, incomplete suite and focused-only suite. The qualification file is
restored after controls and the finished archive passes VerifyOnly again.

Archive: dist/ElvardFIC-WatchersOfTheVoid-0.9.2161.zip, **15,384,506 bytes**.
ZIP SHA256:
`8e176d4970adb1056d6c3b7f09758eb8a26ab3b05f3677d7fd2b4d38835d0d0f`.
DLL SHA256:
`67f3e3c4d5a22a23b628aebcc277c7b9d005ad66fbbc35306a0f2b88773fb9a4`.
Production snapshot SHA256 remains:
`cec5a938df70e3246765de9d9b235114bf3cb8a273613b14bdb6927ce3128413`.
Checksums: dist/SHA256SUMS-current.txt. Build evidence:
build/qualification-current/verification.json and source-before.json. Archive
evidence: build/package-current/verification.json and rejection-controls.json.

FullRegression=true, fullOfflineGroupCount=121, offlineTestPackage=true.
archiveStartupVerified=false, nativeGameplayVerified=false,
TwoPlayerGameplayVerified=false. This is a packaged offline test build; existing
rendered multiplayer, event/save/physics and other TODO acceptance remain open.
No game launch/installation, version bump, commit, push or publication.

# Earlier 2161 TODO Console/events — 75 llama marker/event/Begin fixes (27 October 2026)

Keep public 0.9.2161 / b2217 / protocol 2217. All 75 new implementation items are
listed individually in TODO. Extend existing PlacedSave instead of adding a
parallel marker wire or new reliable kind. Canonical client visuals now remain
separate from native h/event ownership; recover suppressed Begin initialization
through the isolated native timer entry. Whole gameplay acceptance remains open.

PASS run_llama_marker: 25 numbered groups, including 19 actual marker wrapper/
record/real-ParamFrame cases and six actual existing PlacedSave/chunk/wire cases.
Engine, save codec and native key-registry boundaries are doubled. Verify exact
llamaSoul/actor_save ancestry, source/class/world/slot ownership, full native keys,
native getKey generation/registration, occupied-key refusal, getData/loadData
function/record spans, strict empty extra groups, finite quaternion/position/scale,
failed output preservation, callback retirement, exact output class and returned
actor, explicit transform application/readback, native key readback and keyed
saved-body adoption. Reject client marker Create/Remove/resource verbs while
keeping host publication/refresh, late census and native actor preservation at
disconnect/rejoin. Cleanup owns only failed bodies created by the wrapper.
Four marker/PlacedSave production TUs compile. PASS old garden/2163 regression
fixtures after adding narrow marker boundary doubles; previous families remain.

PASS run_llama_events: 25 numbered actual native-wrapper/controller/ParamFrame
cases. Verify canonical Apply leaves native h unchanged while retaining light/
audio, client-only census and Prepare gating before presentation, current GM/
class/world/source identity, typed counter/array/bool fields, bounded sender
storage, multiplicity and count consistency, unchanged unrelated order/storage,
matching-contribution subtraction and masked h clear. Revalidate the complete
snapshot after the last callback; the final ledger commit has no native/reflection
callbacks. Refuse unavailable metadata without partial mutation and retry; never
debit an absent sender or replay lib.setEvent to compensate. Weak exact-source
guards retain their own script-gate hold after same-world disconnect, cancel a
measured false call only when absence is confirmed, allow new true/registered
false calls, and retire after source/world replacement. The lane-owned event
snapshot list now includes prop_llama_C. Four relevant production TUs compile;
the existing native llama fixture adopts the new presentation/native-h contract.

PASS run_llama_begin_restore: 25 numbered actual native-wrapper/controller/
ParamFrame cases. Capture original-true Begin, zero timer/typed TimerHandle,
current source/Sphere/class/world and actual byte collision. Reserve invocation
ownership before queries, commit only admitted initialization, cancel refused
PRE and confirm only exact POST. Preserve the oldest Sphere baseline; wait for
active stacks and defer restoration while any live client authority remains.
Restore only the observed disabled collision, respect external nonzero changes
and replacement timers, and invoke graph108 once without base1146. Native h must
be false and yumpskare true before rearm. Read back the actual handle/activity;
consume uncertain creation without replay. Bound records, retry safe preparation
refusals and retire source/world replacement. A host-admitted jump explicitly
consumes delayed Begin recovery, including source-matched in-flight cancellation.
Both new production TUs compile separately.

Root integration checks refused Prepare restores the parked flag without base
init, event-accounting refusal retains canonical state for retry, current role
installation around native callbacks, exact PRE/POST/cancel association and host
jump consumption. PASS all 75 new `PASS llama lifecycle N` IDs exactly once and
the previous 175 numbered llama groups. Engine/registry/transport/clock boundaries
remain doubled; the production wrappers/controllers and ParamFrame are real.

PASS expanded cooked llama contract: isolated108 delegate/timer/store/pop-flow,
no base/effect replay, marker actor_save inheritance, native getData shape,
key-only loadData/getKey/skipSave, int_save save enumeration and additive native
setEvent counter/sender operations. Contracts consume seven current cooked asset
pairs. Reliablekind gate still passes 242 kinds and all 18 rejection controls.

PASS current qualification: **50 selected groups**, Release/ABI with **854 source
entries**, and production menu/browser label Watchers of the Void 0.9.2161.
All runner exit codes are zero. Evidence: build/qualification-current/
verification.json, source-before.json and llama-lifecycle-verification.json.
Current DLL SHA256:
`67f3e3c4d5a22a23b628aebcc277c7b9d005ad66fbbc35306a0f2b88773fb9a4`.
Production snapshot SHA256:
`cec5a938df70e3246765de9d9b235114bf3cb8a273613b14bdb6927ce3128413`.
The source-before snapshot contains 2142 inputs. Supplemental evidence records
the numbered cases and current source/test/cooked input hashes.
FullRegression=false, TwoPlayerGameplayVerified=false,
LiveNativeSchemasVerified=false, EnemyAiVerified=false, Packaged=false.

Open: rendered native marker save/reload/adoption, same-world teardown, event/
audio/physics timing and rearm. No host activeEvents contribution is synthesized
on clients. The absent-sender guard does not distinguish the causal age of an
old/new latent continuation on the same actor; unknown metadata retains original
native execution. Missing POST permits collision cleanup only; uncertain graph
admission is consumed once and a new native timer starts its native 0.5-second
interval. Collision-only cleanup may also await graph/KSL metadata; callback-side
collision-only changes can outlive the initial captured value. Refused cleanup
needs later Install/Tick and has no independent idle SP retry pump. Previously
admitted native effects cannot be rolled back. Full regression/package and the
wider Console/events category remain open. No game launch, archive, installation,
version bump, commit, push or publication.

# Earlier 2161 TODO Console/events — 75 llama timer/delivery/completion fixes (27 October 2026)

Keep public 0.9.2161 / b2217 / protocol 2217. All 75 new implementation items are
individually recorded in TODO. Continue the remaining llama follow-ups without
a new network kind, package or release identity. Implementation remains separate
from rendered two-player acceptance and the wider Console/events category.

PASS run_llama_timers: 25 new numbered actual native-wrapper/controller/ParamFrame
groups. Check pre-existing and late client timers, original paused-state refusal,
current llama/world/hand eligibility, actor/class/field/inner-type/handle identity,
existence/active/paused queries, exact nonoverlapping native parameter spans,
pause without rearming or clearing, native phase preservation, post-pause
confirmation, uncertain dispatch ownership and retained cleanup. Exercise nested
acquisition/cleanup, bounded capacity, session/role/start/world replacement,
source-only restoration, replacement/stopped handle refusal and immediate cleanup
after a disconnect interrupts Pause. Inject current-predicate retirement before
pause/resume/publication; recheck exact handles after callbacks. Temporary
metadata unavailability retains ownership rather than becoming false retirement.
The typed KSL CDO cache avoids repeated global object scans and refreshes after
its class incarnation changes. Both new production TUs compile separately.

PASS run_llama: its previous 27 groups and 25 new delivery/Begin groups. Preserve
live cue and original timestamp through heartbeat, quiet recipient intent and
delivered-pulse history, including nested same-pulse heartbeat during transport
admission without acknowledging newer debt. Clamp same-pulse local expiry;
consume quiet snapshots; keep sequence/heard tombstones after state expiry.
Check rollback before receipt, saturated expiry and elapsed client polling,
canonical tracker rebinding and callback-side canonical/announced ownership,
recipient person/connection/readiness/debt rechecks immediately before send,
rotating source/recipient scheduling and refused-send fanout limits. Restore
temporary Begin flags after lost invocation/POST, teardown, canceled PRE,
function-token retirement and native refusal. Nested owners retain the oldest
baseline until every active stack finishes. Refuse new Begin admission at 128
restore records; at most 32 existing captures can subsequently retire into debt.
Integrate native timer Install/Tick/OnDisconnect and local jump motion queue;
newer session installation survives callback-side teardown reentrancy.

PASS run_llama_trigger: its previous 73 groups plus 25 launch/motion groups.
Separate target admission from source-owned completion after timer consumption;
target disconnect, movement or body replacement no longer strands that launch.
Refuse retired sources, recheck source position after rotation and execute the
native tail once after an uncertain velocity attempt while source ownership is
valid. Tail ownership no longer depends on the old velocity-function binding.
Queue remote and local motion, confirm Coast with IsDriven, retry late element
enrollment every 100 ms with a 10-second local lifetime, bound pending rows, retire
source/key/class/session/world debt, and yield to player hand/Kerfur ownership.
Reserve before Coast callbacks and preserve newer requeued debt by revision.
Unarmed/failed capture rows release capacity; only unspent idle records may be
evicted under pressure. Both trigger production TUs compile independently.

The three logs contain each new `PASS llama recovery N` ID 1..75 exactly once,
along with the previous 100 llama groups. Native/engine/reflection/registry/
transport/object-slot and clock boundaries are doubled; selectors, wrappers,
controllers and ParamFrame are actual production code. Existing cooked llama,
Rufus, chair, Madness, entity, console and synchronization contracts pass, as
does reliablekind_gate with 242 kinds and all 18 rejection controls. No rendered
schema/physics/audio/save/transport-pipeline proof is implied.

PASS current qualification: **42 selected groups**, Release/ABI with **849 source
entries**, and production menu/browser label Watchers of the Void 0.9.2161.
All runner exit codes are zero. Evidence: build/qualification-current/
verification.json, source-before.json and llama-recovery-verification.json.
Current DLL SHA256:
`2cf4a93765e8a6bb654adad8b5480de2b91b112860e8d55362066fc4586ba7b9`.
Production snapshot SHA256:
`189fe323d43c2f108bee3090d67a610bb22b9e7660fbe04b8760c5a1c649c601`.
The source-before snapshot contains 2132 inputs; supplemental evidence records
new numbered cases, current source/test and directly consumed cooked input hashes.
FullRegression=false, TwoPlayerGameplayVerified=false,
LiveNativeSchemasVerified=false, EnemyAiVerified=false, Packaged=false.

Open acceptance: rendered same-world disconnect/rejoin and timer phase, heartbeat
audio/quiet joins, physical motion after late enrollment and source-owned tail
completion. Marker/save/event-count lifecycle and restoration of never-created
Begin timers/Sphere collision remain separate gaps. An external second pause on
the identical already-paused handle has no distinguishable causal token. Refused
cleanup remains for later Install/Tick; idle single-player has no independent
retry pump. Ordering/heard tombstones hold 256 keys and may evict oldest expired
records; this is not a universal temporal key-incarnation solution. Already
admitted native effects cannot be rolled back, and an uncertain one-shot impulse
is not repeated. No game launch, archive, install, version bump, commit, push or
publication. Full regression/package and the wider Console/events category remain open.

# Earlier 2161 TODO Console/events — 75 remote-player llama trigger fixes (27 October 2026)

Keep public 0.9.2161 / b2217 / protocol 2217. Owner now requests 75 concrete fixes
per continuation; AGENTS, agent/README and TODO match. All 75 items are individually
recorded in TODO. This continues remote-player selection in the existing llama
subtask; no new wire kind or release identity.

PASS run_llama_trigger: 39 actual native-wrapper/real-ParamFrame groups plus 34
actual selector/lifetime groups at 32 slots. PASS run_llama: its 25 previous
controller/native groups plus two new actual-state-controller integration groups.
The combined logs contain every new trigger item ID 1 through 75 exactly once.
Engine/reflection/registry/transport/object-slot and clock boundaries are doubled;
the selectors and wrappers are actual production code, not a model of their logic.

Exercise host/session/slot world and transport readiness, registered alive/posed
non-ragdoll targets, exact overlap membership, unrelated Pawns/local slot, finite
body/eye positions, 600 cm eye/body bound, nearest visible target and stable ties,
unknown/blocked LOS, target movement/replacement, person/connection/body/class
retirement and hand ownership. Confirm native camera-zero fallback is allowed
only when the host's local body is actually in the overlap. Check typed current
components/fields, eight-byte active timer, float radius, actual centers and Audio
origin, simulation, native function identity and exact parameter spans. Preserve
outputs after failed reads and free engine overlap arrays after success/refusal;
validate shape and deduplicate without discarding players behind unrelated Pawns.

Check native cross axes, rotator/velocity finiteness and degeneracy, multiplier
three, vertical +100, rotation refusal, full preflight, moved source points,
callback function retirement, timer/physics rechecks, captured handle clear and
replacement protection, replacement velocity/None and tail 212. Check admitted
uncertain clear/push completion, no repeated impulse, preparation retry/new timer,
nested entry suppression, spent native delegates, source record cap/pruning,
session/start/world retirement and 500 ms/rollback timing. Integration cases
exercise existing LlamaState to once-only client effects, local jump coasting and
source revalidation after the coast callback. Six llama-related production TUs
compile in the two runners; full Release linking also passes.

PASS expanded llama_native_contract_test against current cooked assets: native
timer clear, cross-product axes/rotation, zero camera offset, velocity *3/+100,
None/add=false, graph tail 212, SphereRadius overlap and Pawn query byte 2, plus
the existing base/latent/light/audio/event/console/marker contracts. These cooked
checks do not verify live native parameter schemas or rendered gameplay.
PASS reliablekind_gate: 242 kinds, 214 family-routed, 21 event_feed cases, seven
net-thread terminals, zero duplicate claims; all 18 rejection controls fired.

PASS current qualification: **41 selected groups**, Release/ABI with **847 source
entries** and the production menu/browser label Watchers of the Void 0.9.2161.
All runner exit codes are zero. Evidence: build/qualification-current/
verification.json, source-before.json and llama-trigger-verification.json.
Current DLL SHA256:
`4b699ceaf5d64082cc5acff7db8665ce50fc97042d2d79b65c999ed1d5d29613`.
Production snapshot SHA256:
`eafb0c480a8caf750a60dbf881b2752d78e6277d30b350eba1cea97acc20b1dc`.
The source-before snapshot contains 2128 inputs. Supplemental evidence records
the actual new item IDs, source/test hashes and eight directly consumed cooked
input hashes. FullRegression=false, TwoPlayerGameplayVerified=false,
LiveNativeSchemasVerified=false, EnemyAiVerified=false, Packaged=false.

Rendered local/remote trigger choice, direction, physical coasting transport,
audio/light timing, held props, join/reconnect and save/reload remain open. The
existing prop lane still requires enrolled/expressed actors; the fixtures double
its Coast boundary and do not prove full enrollment/transport/physics behavior.
Marker/save/event-count lifecycle and restoration of pre-session timers remain
open. Native effects already admitted cannot be rolled back: retirement or an
uncertain result can leave partial effects, and that timer's one-shot is consumed.
The wider Console/events category and full regression/package remain open. No
game launch, archive, install, version bump, commit, push or publication.

# Earlier 2161 TODO Console/events — llama jump/light/audio (26 October 2026)

Keep public 0.9.2161 / b2217 / protocol 2217. Follow llama in the open Console/
events category with the 25 named TODO/WORK items. Add LlamaState kind 253 within
the current development format; require matching DLLs.

PASS run_llama.ps1: 17 actual-controller groups at 32 peer capacity and eight
actual-native-wrapper/real-ParamFrame groups cover all 25 numbered items. Exercise
host state, client graph refusal, preserved base init/temporary flag restoration,
key/hand/world eligibility, exact invocation/frame/params, nested PRE reservation,
bounded rows, restart/running/role/link/host-connection retirement, independent
world pointer/generation/readiness, nested newer reads and failed-read cleanup,
malformed/unowned packets, recipient identity/connection/departure, slot 31/invalid
32, world/lane retry, owned send bytes/same-person requeue, actor/component/world
readiness, sequence/pulse wrap, same-pulse nested receipt and once-only cues.
Check expired/quiet join/rollback debt, expiry during native preparation and
per-recipient remaining lifetime after a stalled earlier send. Native cases cover
EntryPoint spans/types, typed light/audio, h/yumpskare masks, callback retirement,
visibility/Activate parameters and missing/replaced function bindings, failed
output preservation, bounded/deduplicated collection, quiet light-only replay and
admitted sound consumption after uncertain completion. Engine/reflection/transport/
clock/object-slot boundaries are doubled. Four production TUs compile separately.

PASS llama_native_contract_test against current assets: prop ancestry, native
base BeginPlay and conditional timer, exact jump/check/destroy entries, local
camera/physics path, Activate(false), h/light transitions, 3-second continuation,
event bookkeeping, typed component templates and console/llamaSoul dependencies.
Confirm explicit Audio.bAutoActivate=false and PointLight.bVisible=false defaults;
fresh native components do not defeat a quiet join seed before the first state.
PASS reliablekind_gate after the new route: 242 kinds, 214 family-routed, 21
event_feed cases, seven net-thread terminal, no duplicate receiver claims.

PASS current qualification: all 40 selected groups and Release/ABI with 845 source
entries. Production menu/browser formatter remains Watchers of the Void 0.9.2161.
Evidence: build/qualification-current/verification.json, source-before.json and
llama-verification.json. Current runner output is logs/run_llama.log within that
directory; final extra cooked-default checks are build/llama-native-contract-latest.log.
Current DLL SHA256:
`ccfecb8016c7ce24052e29658b5fc56f51454459be4fe29d1cb44d34981304f4`.
Production snapshot SHA256:
`0360a8ae0a30f561fcd294f0ea10f9f9d6c755388cd493b9276748af092b7d3a`.
Refresh the 2124-entry source-before snapshot; record sixteen source/test hashes
and six directly consumed cooked-asset hashes. fact_rows_gate also passes with
565 rows in 181 files. FullRegression=false, TwoPlayerGameplayVerified=false,
EnemyAiVerified=false, Packaged=false.
Rendered host/client prop motion, sound/light, join/reconnect, hand and save/reload
acceptance remain open. The marker/save/event-count lifecycle, remote-player trigger
selection (native host camera/player zero remains) and restoration of pre-session
timers are not completed here. Cue bounds are local send/receive/native
admission bounds; they do not prove synchronized audio timing or maximum reliable
transport delay. FullRegression=false, TwoPlayerGameplayVerified=false,
EnemyAiVerified=false, Packaged=false. No game launch, archive, installation,
version bump, commit or push.

# Earlier 2161 TODO Console/events — Rufus/thiccfus birth/pose (26 October 2026)

Keep public 0.9.2161 / b2217 / protocol 2217. The 25 named items in TODO/WORK
cover the first unresolved Console/events family, scoped to birth/pose and mirror
authority; gameplay/effects and rendered acceptance remain separate.

PASS run_rufus.ps1: 25 numbered groups include the actual Rufus wrapper and actual
core/call.cpp, private birth/lifetime helpers and extracted production deferred
catch/drain/enroll/adoption bodies. Exercise first-load root/derived ancestry and
plush exclusion; exact console source/outputs; slot/class reuse; session serial,
generation/pointer/running/role changes; host-alone admission; duplicate debt;
all nine non-finite pose/scale fields and world retirement during native reads;
actual mesh yaw; marked BeginPlay/Tick/AI/latent/damage/child refusal; untouched
unmarked behavior; unavailable registration/watch/mark/world; failed adoption;
checked KeepRelative attachment, all seven malformed/missing parameters/returns,
retired function/actor/mesh/arm/anim dependencies; native refusal/uncertain completion;
typed pawn binding and AnimBP recovery; unchanged failed yaw reads; pitch/roll and
rotation-write failure. Seven changed/new production TUs compile independently.
Registry/transport/object slots/script-gate dispatch and engine boundaries are fake.

PASS existing run_entity_sync, including actual NPC orientation and new stationary
Rufus presentation retry, Grayboar/Kavotia/prop fixtures and fifteen production TUs.
PASS rufus_native_contract_test against current assets: Character inheritance,
console BeginDeferred(self) routes, exact native BeginPlay/Tick/Destroyed/damage
stubs, unsafe AI/HUD/player movement/explosion/children, Mesh/SpringArm attachment
and animation owner/velocity dependencies. Source checks also confirm guard
preflight before BeginDeferred/Finish and adoption binding. These source-order
checks do not run the full engine spawn/transport pipeline.
PASS fact_rows_gate: 565 registered rows in 181 files. Rufus uses the existing
class-scoped name-watch API, readiness checked before birth; no live census claim.

PASS current combined qualification: 37 selected groups, Release/ABI with 843
source entries, and production menu/browser formatter Watchers of the Void 0.9.2161.
Evidence: build/qualification-current/verification.json, source-before.json and
rufus-verification.json. Final runner output is logs/run_rufus.log in that evidence
directory; combined output is build/qualification-current-latest.log.
Current DLL SHA256:
`df40742ddcde7ba231b95130725b2220c164409c300f5b134caf6102eaf5ebe9`.
Production snapshot SHA256:
`563eaafff7af527e1f755f601be90671e59404d51556d90daf71aa92deeabd40`.
Refresh the 2120-entry source-before snapshot and record twenty source/test hashes
and ten directly consumed cooked-asset hashes. FullRegression=false,
TwoPlayerGameplayVerified=false, EnemyAiVerified=false, Packaged=false.
Rendered multiplayer birth/pose/animation/yaw, join/rejoin/destruction, cross-player
targeting/grab outcomes, sound/HUD/event state, corpse mesh/loot and damage/explosion
feedback remain unverified/open. Marking cannot reverse earlier native effects.
Native scale reads still use the helper's unit-scale fallback on a failed dispatch.
EnemyAiVerified=false, FullRegression=false, TwoPlayerGameplayVerified=false,
Packaged=false. No game launch, archive, installation, version bump, commit or push.

# Earlier 2161 TODO Console/events — Maxwell native playback (26 October 2026)

Keep public 0.9.2161 / internal b2217 / protocol 2217. Follow the open Maxwell
phase/recovery subtask, with the 25 exact implementation items in TODO and WORK.

PASS run_maxwell_native.ps1: 25 groups include the actual native wrapper and
actual core/call.cpp. Fake typed object slots/classes/dispatch functions, native
queries and callbacks cover bounded/null collection; pointer/generation/readiness
retirement; indexed dedup; actor/audio class identity; component binding; packed
isUsed layout; coherent state; invalid pitch and unchanged failed output; Sound/
SoundWave identity; coherent duration/loop/rate; query function/return replacement;
owned input and malformed state; volume/pitch/Play exact parameters and retired
function preparation; seek clock changes; Stop/touch/ToggleActive preparation and
completion; squeak admission; nested newer Playback state. Include positive retry,
unchanged heartbeat and real ParamFrame metadata-cache incarnation behavior.
The changed production TU also compiles independently. Existing run_toy_music
phase/lifetime/controller/wrapper regression and four production TUs passed before
the new suite; the combined current run below rechecks the final sources.

An initial combined run refused qualification when final EOF/BOM formatting
changed the production hash; rerun the aggregate against stable final sources.
PASS final stable aggregate: all 33 selected groups, Release/ABI with 842 source
entries, and production menu/browser formatter Watchers of the Void 0.9.2161.
Evidence: build/qualification-current/verification.json, source-before.json and
maxwell-native-verification.json. Current DLL SHA256:
`3cae262f4d872e5fd2eeb12e393466930c052d5ea935f1918e3d15f12dbb45e3`.
Production snapshot SHA256:
`3a33d208a2049b3adc2895c03c13a1a13f4fa78851a1e609150737f12bde5954`.
Refresh the 2116-entry source-before snapshot; record hashes for eight source/test
inputs and 24 relevant cooked asset files. Final focused output, including the
additional qwak/plush positive case, is build/maxwell-native-latest.log; combined
output is build/qualification-current-latest.log and the qualification logs folder.
FullRegression=false, TwoPlayerGameplayVerified=false, EnemyAiVerified=false,
Packaged=false. The formatter required loading the local MSVC environment before
its successful run; no game UI was rendered.
The cooked sync-native contract already covers touch -> ToggleActive order,
native isUsed/deactivation, plush Alt/qwak and five wave duration/loop overrides;
recheck it against current local assets. These fixtures double engine/reflection/
transport/clock boundaries. Rendered host/client music phase, sample-exact timing,
live schemas, late joins, restarts and world reload remain unverified. Optional
phase uses only a known looping SoundWave, preserving ordinary playback otherwise.
Native effects already admitted cannot be rolled back. Rufus/thiccfus, llama,
grunts/variant AI/damage/effects, the broader Console/events category, original
garage acceptance, full regression and packaging remain open. No game launch,
archive, install, version change, commit or push.

# Earlier 2161 TODO Console/events — Madness recovery (26 October 2026)

Follow Remaining implementation's first open category, specifically the named
Madness playback-recovery subtask. Keep 0.9.2161 / internal b2217 / protocol 2217.
The 25 implemented boundaries are listed at the top of TODO/current WORK; the
broader Rufus/thiccfus/llama/grunts/variant-effects category remains open.

PASS run_madness_recovery.ps1: twelve real-controller groups at 32-slot capacity
and thirteen real-wrapper plus actual ParamFrame groups. Cover same-pointer start
serial, running/role/world retirement, clock rollback, malformed host snapshots,
source/class replacement, paused age, person/connection retirement, retained
world/lane readiness, departure, per-recipient owned bytes, same-person requeue,
old-event cleanup refusal, current master lineage/field retirement, HUD binding
replacement, known-null music fields, query-function/return failure, typed widget/
audio returns, viewport retry/no repeated show, stopped/dead audio recovery,
no unchanged music restart, query/native pre-allocation failure, original seek,
world/context retirement during preparation or completion, missing factory return
slots without allocation, typed uncertain completion adoption, matching text/count
and packet ownership, retired/foreign cleanup, nested Apply/Clear and refused
Stop/Remove retry. Reflection/object slots, engine, transport and clocks are doubled.
Both changed production TUs compile.

PASS legacy run_2154 including canonical console output/clear/history/execute-once,
old Madness controller cases and the new actual wrapper fixture. Replace its
old raw-function/string boundary fake with typed UObject/function slots and real
core/call.cpp; preserve seek/no-restart/stopped/dead/query/viewport behavior.
PASS madness_native_contract_test.py against current local assets: native HUD
creation/removal, count/text, addCount -> master.killed and spawnGrunt's deferred
prop_grunt creation plus killcount linkage. No game enemy route is replayed.

PASS current Release/ABI (842 source entries), 28 selected validation groups and
the production public-label formatter (Watchers of the Void 0.9.2161). Evidence:
build/qualification-current/verification.json, source-before.json and
madness-recovery-verification.json. Focused output: build/madness-recovery-latest.log;
legacy console/Madness: build/madness-legacy-latest.log; cooked contract is also a
selected current group. Current DLL SHA256:
`06af722a2b61a644b0bb2593650b7bbafa4c4fa81360064a2c69091fc87baf6b`.
Production source snapshot SHA256:
`861ea9c5685dd985de8235ab3f12b837914ff9c58cf60b16f58a5f40bfbb13b5`.
Refresh the 2116-entry source-before snapshot and independently match final
production/DLL hashes. FullRegression=false, TwoPlayerGameplayVerified=false,
EnemyAiVerified=false, Packaged=false.

Rendered host/client music/HUD/score, late join/reconnect, pause/resume, new-event readiness,
exact sample timing and live reflection schemas remain unverified. Native effects
with unknown results cannot be rolled back; final controller teardown cannot retry
refused cleanup after its owner is gone. Enemy AI/damage/effects, the broader TODO
category, original garage report, full regression and package remain open. No game
launch, archive, install, version bump, commit or push.

# Earlier 2161 host companion lifecycle: 25 fixes (26 October 2026)

Keep public 0.9.2161 / internal b2217 / protocol 2217. The next 25 named items
are listed in TODO/current WORK. The original owner's one-ATV-for-everyone report
is still unreproduced; these are verified code fixes, not a proven live root cause.

PASS run_atv_companion.ps1: 25 groups include the actual private production
atv_companion.h with real CachedObjRef and AtvEntry, doubling engine/count/save/
transport and clock inputs. Cover world/session/start/role/running/connection
retirement, reused minted/source slots, all six non-finite source fields, small
motion and three-axis rotation stillness, zero/rollback clocks, unknown/changed
counts, drive/carry release, source movement during parking or the final recount,
scoped search reset, a persisted coop-key source, bounded saved-body waiting,
nested minting, foreign class/world returns, third-body retry recovery, repeated
destroy refusal, protected foreign/missing index rows, malformed/mismatched save
keys and actor retirement, existing seat/syncer/clock preservation, protected key
targets with newest callback-updated entry preservation, and refused spawn-send
retry. Compile the production atv_sync.cpp against the extracted lifecycle.

PASS current Release/ABI (842 source entries), 25 selected validation groups and
the production 0.9.2161 public-label formatter. Current combined evidence:
build/qualification-current/verification.json and source-before.json; focused
lifecycle evidence: atv-companion-verification.json and build/atv-companion-latest.log.
Current DLL SHA256:
`a5a5539cafded51e174df687db5d02772db318c56999c2519989b314e3079bee`.
Production source snapshot SHA256:
`4fd7600975f5d1b8675aaa34929ec777f4bebc6a0a57b0140fcce167226f781e`.
Refresh the 2116-entry source-before snapshot and independently match source/DLL.

The first combined run passed 24 groups and found run_session_reports's obsolete
extractor for TrySpawnCompanion inside atv_sync.cpp. Replace its duplicated old
ATV model with delegation to the actual lifecycle runner; retain its real console
classifier, personal-rest and cooked native-contract checks. Rerun only this
failed group through check_current into build/session-report-recheck. Require
identical production snapshot, DLL SHA256 and protocol before substituting its
passing result in the combined 25-group report. Keep the original failure log;
verification.json.FocusedRecheck records it and pins the successful recheck.

This fixture does not execute the outer host hit-guard readiness gate, real garage
traces, live UE spawning or native save files. Those dependencies retain their
existing focused checks. Rendered host/client garage creation, late save replay,
two complete rigs, no third body, reconnect and save/reload remain open. Native
occupied save-key reservation remains unchanged; stale records are not overwritten.
Failed disconnect cleanup is not retried after teardown. Accepted native effects
cannot be rolled back. Full regression/package remain open. No game launch,
archive, install, public/internal version change, commit or push.

# Earlier 2161 seat presentation: 25 fixes (26 October 2026)

Owner explicitly changes continuation size to 25; update AGENTS/README/TODO.
Public version remains 0.9.2161, internal build/protocol 2217. The complete 25-item
implementation list is at the top of TODO and is explained in current WORK.

PASS 25 new groups in run_seat_presentation: twelve controller groups and thirteen
native/view groups. Include the actual seat controller and ue_wrap sit wrapper;
include real core/call.cpp; extract production RemotePlayer RefreshSeatView/
RefreshView, SetComponentWorldRotation and IsSitting bodies unchanged. Exercise
the real body-yaw state helper. Reflection/object slots, engine, transport and
clock boundaries are doubled. Four changed production TUs compile.

Cases cover deferred first-pose/target reads, rejected far/huge anchors, changed
chair world during key lookup, local seat/pawn replacement during occupancy reads,
scope restore and nested session install, AnimBP replacement, adjacent bool-bit
preservation, missing setter parameters/function replacement/preparation retirement,
non-finite and oversized model dimensions, source/base/parent replacement, full
roll/yaw/pitch parent transforms including vertical forward motion, refused clear
retry, nested fresh shifts, partial two-mesh writes without cumulative drift,
32 bodies and restoration, small/wrapped seated yaw, final head/probe placement,
world-space flashlight aim after tilt, missing rotation metadata without raw
writes, all rig/seat/class/world/person/link/slot/pawn retirement cases, six invalid
driver-pose fields, failed component/ATV rotation reads, bad fallback forward
vectors, refused first/second writes and chair-to-ATV placement priority.

The 25 compiled removal controls each exercise their numbered group and require
its runtime assertion. Prior chair groups/controls are rerun, retaining rotation,
heartbeat, OFF, late lookup, transitions and scope/packet/reentry coverage.

PASS current Release/ABI (842 source entries), 24 selected validation groups and
the production public-label formatter (Watchers of the Void 0.9.2161). Combined
evidence: build/qualification-current/verification.json and source-before.json.
Focused evidence: seat-presentation-verification.json,
build/seat-presentation-latest.log and build/seat-presentation-check/fault-*.log.
Current DLL SHA256:
`9da3365bb15934b5c61c7cd1c05fc9357c6026f123ac488dd98e4c72edbc2217`.
Production source snapshot SHA256:
`143ab482e8214466aca20e92855cf5745ac40cc9600eab0a6acd0a3b24c63ba1`.
Refresh the 2115-entry source-before snapshot and independently match final
source/DLL hashes to qualification. Selected related groups cover previous chair,
player/session reports, laptop, ragdoll retry/skin, flashlight, lighting, console
and ATV checks. FullRegression=false, TwoPlayerGameplayVerified=false, Packaged=false.

Rendered host/client/custom-model seat alignment, head/flashlight, banked ATV,
standing/IK restoration, joins/reconnect/moving chairs and 32-player load remain
open. Live schemas/native completion are not established by doubles. Relative
location's zero-on-failure API cannot distinguish failure from authored zero.
Accepted native effects cannot be rolled back. Original one-ATV-for-everyone,
full regression and package qualification remain open. No game launch, archive,
install, commit, push or public/internal version change.

# Earlier 2161 seated player/chair rotation continuation (26 October 2026)

Keep public 0.9.2161 / internal b2217 / protocol 2217. Ten named TODO/WORK fixes:
small-motion updates, all chair axes, shared shortest-arc interpolation, OFF
cancellation, admission before body placement, pending chair lookup, scope/peer
retirement, coherent owned samples, state transitions and send-completion ownership.

PASS run_chair_sync.ps1 -FaultControls: ten groups and ten compiled removal
controls reaching their named runtime assertions. Include the actual sit_pose.cpp
unchanged for positive runs and extract the real RemotePlayer::IsSitting body
into the fixture's class. Test at 32 slots, including slot 31 and rejected slot 32.
Cases cover 1 cm/0.5 degree thresholds, 50 ms pacing, fresh refused-send retry,
pitch/roll-only changes, yaw 179 -> -179, heartbeat, shared halfway/rebased
body/chair interpolation, OFF without chair writes, denied reach/local occupancy/
foreign class, packet mutation during authorization, nested departure, missing
chair/puppet recovery, world pointer/generation/session serial/role/running/link
changes, roster/transport replacement, reused chair serial, three-second expiry,
failed reads, NaN/Inf, overlong/non-ASCII/None keys, source/key retirement during
capture, stand/laptop/sleep/ragdoll/death release, departure during native apply,
and disconnect/session/pawn/seat/class changes during send. Nested Tick sends once.
Production sit_pose.cpp, remote_player.cpp and net_pump.cpp compile.

PASS chair_native_contract_test.py against current-assets: sitBox passes its
parent chair to mainPlayer::sit; the player retains sittingOn, attaches to it and
clears sittingOn on unsit. This establishes cooked dependencies, not measured
live reflection offsets or rendered pose quality. The legacy chair fixture now
shares the current fixture, and run_2144 delegates chair verification to the new
runner. Both chair runtime/native runners are in check_current.

PASS current Release/ABI (842 source entries), 23 selected validation groups and
the production public-label formatter (Watchers of the Void 0.9.2161). Combined
qualification: build/qualification-current/verification.json and source-before.json.
Focused source/asset/control evidence: chair-sync-verification.json,
build/chair-sync-latest.log and build/chair-sync-check/*.log. Current DLL SHA256:
`bdcfe990da48b15286a640c90aa8671c8ab076e891ac20b972489ed4291b55a8`.
Production source snapshot SHA256:
`0c5807c32029061bfba5fea148cd20f86ef15b16c574303980a53f8b38ee62ee`.
Refresh the 2115-entry source-before snapshot and independently match final
source/DLL hashes to qualification. The selected groups include player/session
reports, ragdoll retry/skin, laptop UI and previous console/flashlight/lighting/ATV
regressions; this is not a full regression or packaged release.

Controller tests double
reflection, object slots, engine, transport and clock boundaries. Rendered
host/client spins, chair/body/mesh/head/flashlight alignment, moving chairs,
late join/reconnect and stand/laptop/sleep/ragdoll/ATV-seat acceptance remain open.
Host reach before the sender has a posed puppet can wait for the one-second
heartbeat. Native effects or sends already accepted cannot be rolled back.
The original one-ATV-for-everyone report, full regression and package qualification
remain open. No game launch, archive, install, version change, commit or push.

# Earlier 2161 ATV spawn/placement continuation (26 October 2026)

Keep public 0.9.2161 / internal b2217 / protocol 2217. Ten named TODO/WORK fixes:
finite spawn pose, refreshed native service tuple, frozen class/key/pose and class
offset, original world/context fence, validated begin-return type, deferred actor
finish lifetime, incarnation-scoped cleanup, teleport lifetime, failed-placement
refusal and source identity/pose through parking queries.

PASS run_atv_spawn_lifetime.ps1 -FaultControls: nine spawn groups plus the current
parking fixture (ten groups) and ten compiled source-removal controls, each reaching
its named runtime assertion. Extract the actual TeleportRig, keyed SpawnMirror and
FindCompanionLocation bodies unchanged for positive runs. Reflection, object slots,
world/context, ParamFrame and native dispatch are doubled; the production ATV TU
also compiles. Cases cover NaN/Inf on all six pose fields, replaced native CDO/class/
function bindings, class serial/key-offset changes during name conversion, copied
caller inputs, original world/context retirement, foreign begin returns without
key writes/destruction, deferred body serial/class retirement before/after finish,
safe cleanup versus replacement preservation, teleport preparation/completion
retirement, failed teleport publication, trace-source retirement and six changed
source-pose axes. Failed parking leaves the caller's output unchanged.

PASS run_atv_recovery and run_2149 during focused verification: begin/finish
argument failure and return/cleanup coverage, install/impulse recovery, current
mount gates, supported opposite bay, walls, pits, unknown traces, tilt and bad yaw.
The spawn fixture now models actual object slot/class/world identities; placement
adds callback boundaries and configurable extracted-source includes for controls.

PASS current Release/ABI (842 source entries), 17 selected validation groups and
the production 0.9.2161 public-label formatter. Current combined evidence:
build/qualification-current/verification.json, source-before.json and
atv-spawn-lifetime-verification.json; focused control output:
build/atv-spawn-lifetime-latest.log and build/atv-spawn-lifetime-check/*.log.
DLL SHA256:
`1ca482af4d7581011e6dfee47ce522e7f5dcfdfdce6d8f7b60609dd66bba4032`.
Production source snapshot SHA256:
`a1a4eea36c6ff562b3418e63150915baf0719a396a8812829172745758b9e801`.
Refresh the 2115-entry source-before snapshot and independently match the final
DLL/source hashes to qualification. World tests separate generation retirement
from pointer replacement, including the same world pointer with a new generation.

The original owner's garage failure (one ATV for everyone) remains unreproduced.
Live reflection schemas, real garage geometry, complete wheel placement and
host/client join/reconnect/reload/riding/save acceptance remain open. An unreadable
deferred return cannot be safely identified for cleanup. Already dispatched native
effects cannot be undone. Full regression/package remain open. No game launch,
archive, installation, commit, push or version change.

# Earlier 2161 lamp/ambient/switch-audio continuation (26 October 2026)

Public version stays 0.9.2161; internal build/protocol stay 2217. Ten individually
named fixes are in TODO/WORK: lamp preparation ownership, ambient preparation
ownership, exact ambient component binding, sound metadata/callee currency,
bounded sound parameters, owned audio packet, attenuation recovery, finite audio
and location values, source/resource currency, and construction/replay reentry.

PASS run_light_presentation.ps1 -FaultControls: ten new real-wrapper/ParamFrame
groups and ten compiled controls that reach their named assertions. Cases include
actor/class/function/world replacement during lamp/ambient frame preparation,
component replacement during read/preparation/completion, unchanged failed-read
output, sound function serial and binding replacement, old callee exclusion,
metadata/hook retry, negative/truncated cue/volume/pitch frames, null callback
parameters, packet mutation during location lookup, attenuation serial/class/
readiness recovery, invalid multipliers and all three position axes, source/cue/
function/world retirement, recursive construction and nested switch-scope restore.
The native-hook layer exposes the exact callee only while its callback runs;
the real native_result_2126 fixture verifies PRE/POST identity and cleanup, plus
its existing one-byte guard-page, void/pointer return and override checks.

PASS previous run_lightswitch_native.ps1 -FaultControls: ten earlier lighting
groups and ten controls. Adapt its use-lifetime control anchor to the new sound
readiness check. The shared fixture now models lamp/ambient metadata, sound
resources/callee, engine reentry and packet mutation; actual wrapper/ParamFrame
remain included. PASS lamp/ambient and cooking regressions, retaining existing
provenance, local sound filtering and nested replay coverage. Active legacy
fixtures now expose sound frame size and the native callee; metadata-unavailable
visual replay waits, while attenuation-unavailable visual replay runs muted.

PASS current Release/ABI, 16 selected validation groups and the production 2161
public-label formatter. Combined qualification is recorded in
build/qualification-current/verification.json and source-before.json. DLL SHA256:
`85f9b30948db251f8790c5be177039b9ca121fb9bc74f5b988cba246086d0b62`.
Additional source/control evidence:
light-presentation-verification.json, build/light-presentation-check/verification.log
and per-control logs. This is code-boundary evidence with reflection/engine
fakes, plus the real native thunk; rendered audio/light completion, live game
reflection schemas and inner engine audio-wrapper preparation are not established.
Already dispatched native effects cannot be rolled back. Full regression/package
and the original garage-companion reproduction remain open. No game launch,
archive, installation, commit, push or version change.

# Earlier 2161 native lighting continuation (26 October 2026)

Keep public 0.9.2161 / internal b2217 / protocol 2217. Ten implemented TODO/WORK
items: root metadata recovery, switch metadata recovery, removal of root and
switch fallback offsets, root/switch/breaker bool masks, bounded linked-root
lookup, checked group verbs and switch replay lifetime.

PASS run_lightswitch_native.ps1 -FaultControls: ten runtime groups, ten compiled
removal controls reaching their named assertions, and production-header wrapper
compilation. The fixture includes the actual lightswitch.cpp and core/call.cpp;
only reflection, world, object slots and native dispatch boundaries are doubled.
Cases cover missing runTrigger/use and late fields, live old class/function
bindings replaced or serial-reused, field failure/recovery, unrelated bool bits,
dead/foreign objects, malformed array counts/capacities, linked actor/header
replacement, missing owner/index/active parameters, actor/class/function/world
retirement during frame preparation, layout replacement with old functions still
alive, sound-preparation retirement and native completion after a world change.
The first linked group and absolute runTrigger indices 1/2 retain their behavior.

PASS run_2177: lamp/ambient phases, masks, lifetime, native presentation, initial
watch recovery and stopped-session guards. PASS run_cooking includes active
wrappers_2129 sound/nested replay tests; update that fixture to resolve switch
metadata through EnsureSwitchResolved rather than inject the removed raw cache.
Its cooking provenance coverage is retained. Current cooked local assets confirm
runTrigger(owner,index), setActive(active), switch use(), separate isActive/active/A
names and inherited Key/objects on triggerBase. Name/bytecode checks establish
dependencies, not a live reflected-offset or rendered-game measurement.

PASS combined Release/ABI, 15 selected current groups and the production 2161
public-label formatter. Qualification: build/qualification-current/verification.json
and source-before.json. Current DLL SHA256:
`30c5a6c1ed0b3cf07e68ca5bcadd320e69e9c8a8b0367d121280c603fcb18e86`.
Additional source/control/asset evidence: lightswitch-native-verification.json.
Rendered host/client switches, breaker/live-state separation, flicker, joins and
world reload remain open, as do the original garage companion reproduction, full
regression and package qualification. Native call completion cannot roll back an
already dispatched side effect. Replay sound hook parameter/resource lifetime
and remaining lamp/ambient preparation boundaries are outside these ten fixes.
No game launch, archive, installation, commit, push or version bump.

# Earlier 2161 flashlight join-delivery continuation (26 October 2026)

Ten named fixes in TODO/WORK close the previous targeted replay retry gap. Public
version remains 0.9.2161; internal build/protocol remain 2217. Local seeds and host
cached relays now retain refusal debt, retry fresh local state, carry OFF, bind
recipient/author identity, own packet bytes, preserve newer work through older
completion, share a rotating 64-send-attempt budget and validate active session,
role and recipient readiness. Connect hooks enqueue; one Tick drain sends.

PASS run_flashlight_join.ps1 -FaultControls: ten groups and ten compiled removal
controls, each reaching its named assertion. The real production item_activate.cpp
is included unchanged with engine/transport/registry boundaries doubled; compile
the fixture at 32-slot capacity. Cases cover local and host-relay refusal retry,
missing component recovery, fresh intensity/mode/battery-empty samples, local and
relay OFF, recipient person/connection replacement including during sampling,
author replacement/departure/link loss before generation clearing, owned cache
bytes during nested mutation, nested queue/Tick and ordinary publication, one
64-attempt budget, link/lane/world readiness, host/client allowed targets, zero
client-host generation, and session/world/serial/role/running/link retirement.
The stress test retains 961 debts through refused sends and drains every distinct
recipient/origin pair exactly once after acceptance resumes, at most 64 per tick.

PASS prior run_2145.ps1 -FaultControls: flashlight state/lifetime scenarios and six
compiled controls, plus existing cord and hand-tool fixtures. Its queue check now
requires targeted replay to leave normal publication dedup unchanged. Production
item-activation TU compiles. Register run_flashlight_join in check_current.
PASS combined Release/ABI, production public-label formatter and 12 selected
current groups, including the earlier console/light/ATV audit groups. Recorded
in build/qualification-current/verification.json with a refreshed source snapshot.
Current DLL SHA256:
`6f332fffd1a7b2c1d0f2f71077092f49fe39f108e71d6a953e038edefc1879c3`.
Extra evidence: flashlight-join-verification.json, run_flashlight_join.log, the ten
per-control logs in build/flashlight-join-check and the prior-state control log.

This establishes the named controller boundaries with fakes, not rendered light
or real GNS throughput. Unknown recipient identities are refused; they are not
adopted later without a new valid enqueue. Pre/post identity checks are not an
atomic net-thread admission lock, and already accepted transport cannot be
retracted. FullRegression=false; TwoPlayerGameplayVerified=false. No game launch,
archive, installation, commit, push or version bump. OPEN: rendered host/client
late join, reconnect, mode/battery/OFF, slot reuse and backpressure; earlier garage
companion reproduction, full regression and package qualification remain open.

# Earlier 2161 ATV/light/console audit (26 October 2026)

Owner report: only one ATV spawns for everyone; the second previously appeared
beside the original in the garage. Also audit flashlight, console and light sync.
Public version remains 0.9.2161; internal build/protocol remain 2217.

Implemented ten fixes listed in TODO/WORK: retryable hit-guard installation and
its per-pump controller call, bounded impulse writes, independent verb retry,
initial light-group watch recovery, running-session light gating, checked begin
spawn, checked finish/result cleanup, checked teleport parameters and finite yaw.
The original permanent hit-guard refusal prevents Tick from reaching companion
creation. This is a demonstrated code blocker, not a reproduced diagnosis of the
owner's particular game/save. Current cooked ATV contains all seven exact hit
delegate names used by the guard.

Additional confirmed audit repair: the merged console native wrapper contained
duplicate definitions/statements and an unmatched duplicate panel loop, causing
compilation failure. Remove those duplicates and retain its lifetime guards.
The flashlight fixture also had duplicate engine definitions and a duplicate
lifetime run. The 2149 runner referred to deleted SenderMaySpeakFor and
AcceptIncomingOccupancy; update it to current mount/carrier/stamped-seat queries
and correct grabbing_actor sampling. It no longer claims the removed model's
authority/seat-race coverage. Repair two console mutation anchors so both
preparation and completion checks are removed by their named control.

PASS `package/check_current.ps1` with 11 selected groups:
run_session_reports, run_sat_console_delivery, run_sat_console_callbacks,
run_flashlight_click, run_2145, run_2149, run_2177, run_atv_recovery,
run_atv_steering, sync_native_contract, sat_console_native_contract_test.
PASS combined current Release/ABI (842 source entries) and production public-label
formatter. DLL SHA256:
`ddfe4fb119c8988ba081fe8f26fdee4ab9bb9ead4a0e0011f84e516f79ac1664`.

Additional PASS: console 23 compiled removal controls plus 21 controller callbacks,
14 delivery cases, actual input/metadata wrapper and ten native-operation groups;
flashlight state six compiled controls; remote click ten controls and both TUs.
The ATV recovery fixture includes production hit_guard.cpp and unchanged Install/
EnsureHonkWatch bodies. It exercises delayed metadata, temporary interceptor
capacity, invalid/truncated/exact-fit frames, throttle/idempotence, independent
verb retry and owned/mirror/offline impulse writes. Spawn tests use unchanged
TeleportRig/SpawnMirror with doubled reflected-call preparation: all five begin
argument failures, finish arguments/missing/null/wrong return, orphan cleanup,
teleport argument failures and successful creation. Placement tests cover opposite
bay selection, walls, pits, unavailable traces, tilt and NaN/infinite yaw. These
boundary fakes do not establish real garage geometry or complete UE metadata
ownership. Lamp/ambient tests cover recovery and stopped-session guards; native
asset checks distinguish lamp flicker presentation from the group's active flag.

Evidence: `build/qualification-current/verification.json`, `source-before.json`,
`audit-extra.json` and logs. FullRegression=false; TwoPlayerGameplayVerified=false.
Native-wrapper source and fixture hashes and extra control logs are retained.
No archive, installation, game launch, commit, push or version bump.
OPEN: rendered two-ATV garage spawn on host/clients, reconnect/no-third and saved
second-ATV positions; flashlight modes/battery/late joins, group/lamp/ambient
flicker and console concurrent authors/clear/history/reconnect. Refused targeted
flashlight join replay still lacks retry debt. Full regression and package
qualification remain unperformed.

# Earlier 2161 console-only continuation — ten native-operation fixes (26 October 2026)

The user assigned console work to this session and left flashlight work to the
other session. This continuation does not edit item_activate.cpp. Public/internal
identities remain 0.9.2161 / b2217 / protocol 2217.

Ten individually named TODO/WORK fixes cover live layout bindings, atlas identity,
factory/context/result ownership, init/dish lifetime, superseded command fields,
writeToLog preparation, text-box class, two-field clear preparation, bounded and
transactional argument reads, and bounded panel enumeration. Old classes/functions/
CDOs that remain alive no longer keep an obsolete layout binding latched forever.

PASS run_sat_console_callbacks.ps1 -FaultControls: 21 controller scenarios, the
existing actual ParamFrame/FText/FString input/metadata tests, ten new native-operation
groups and 23 compiled removal controls (nine controller, four earlier wrapper,
ten new native controls). PASS run_sat_console_delivery.ps1: 14 scenarios.
PASS production-header compilation of both controller and native wrapper.
PASS sat_console_native_contract_test.py against current cooked 0.9.0n assets:
enterCommand's clr resets consoleLine and logText; writeToLog uses consoleLine.
The cooked proof includes both asset hashes.

Console-only evidence and tested source hashes:
`build/sat-console-callbacks-check/console-verification.json`.
Logs: console-focused.log, console-delivery.log, console-native-contract.log and
console-native-compile.log in that directory. Native wrapper source SHA256:
`60bba74cca4a66799153b60976d8701da039a5a5dd0c8b103c6b3ba0070c4006`.
No shared DLL/qualification-current report was overwritten by this continuation.
No combined Release build, full regression/package qualification, native game
startup/rendered acceptance, installation, commit or publication was performed.
Complete reflected schemas, caller session predicates inside every helper,
atomic rollback and uncertain-native-call retry/admission remain open boundaries.
Earlier shared Release evidence below describes its earlier source snapshot.

# Earlier 2161 continuation — console metadata and flashlight lifetime fixes (26 October 2026)

Four implemented changes: delayed-resolution retry, clean partial layouts, live
metadata dependencies and bounded/validated FString reads. Versions stay 2161/b2217.

PASS run_sat_console_callbacks.ps1 -FaultControls: 21 controller scenarios, the actual
ParamFrame/FText/FString input wrapper and four new wrapper regression groups:
recovery after eight incomplete attempts; missing enter/write metadata cannot reuse
old offsets; class/function serial replacement invalidates and re-resolves the cache;
and malformed capacity/count/null storage/termination/oversized output is rejected.
All thirteen compiled removal controls fail their expected runtime assertions: nine
existing controller controls plus metadata-retry, partial-layout, metadata-currency
and string-capacity. Control logs: build/sat-console-callbacks-check/mutants/.
PASS production-header compile of ue_wrap/desk/sat_console.cpp to
build/sat-console-callbacks-check/native_wrapper_production.obj.

The earlier interrupted run_2145 compile exposed conflicting Seat/Scope edits. The
owner clarified no other writer is active and authorised reconciliation. That temporary
state is superseded by the final single SessionScope/PublishState implementation.

PASS six flashlight scenarios in run_2145: session/world/role/link/pointer retirement;
roster person and transport replacement retiring pending/cache state; rejection of local,
foreign and wrong-slot actors; owned packet across a mutating native getter; source
retirement before visibility and within the final binding getter; and stale publication
completion, including nested different state, return to the previously admitted state
and identical-state recursion. Late old transport completion invalidates the signature;
the next publication sends the current intensity again. Six separately compiled removal
controls each fail the selected scenario's assertion. Logs: build/flashlight-state-check/.
The debug toggle now samples/publishes entirely on the game thread, using PublishState.

PASS five selected current groups: run_2145, run_flashlight_click,
run_sat_console_callbacks, run_sat_console_delivery and sync_native_contract. Final
Release/ABI and production version formatter pass, 0.9.2161 / b2217 / protocol 2217.
Evidence: build/qualification-current/verification.json, source-start.sha256 and logs/.
The earlier source-before.json is a historical packaging snapshot; it was not refreshed
by this focused check and is not the current source manifest.
DLL SHA256: `408037f478d3939dce20642b3398d6356b17941a2bd8f2d0cbc91e695ebd4fe8`.
FullRegression=false, TwoPlayerGameplayVerified=false, Packaged=false. This evidence
supersedes the preceding snapshot's hash. Rendered gameplay, archive and installation
remain unperformed. Refused targeted host late-join cache relays still have no retry queue.

# Earlier 2161 continuation — flashlight and console priorities (26 October 2026)

Public 0.9.2161, internal b2217/protocol 2217 unchanged. Ten individually named
changes and remaining gameplay scope are in TODO/WORK. The owner report has no
reproduced rendered stimulus yet; do not equate offline guards with full acceptance.

PASS run_2145: existing battery-empty/unequip/send retry/deferred setter tests plus
exact local publisher, pawn serial replacement during sampling, non-finite sample,
invalid incoming cone/intensity, superseding state before cone dispatch, nested
echo restoration and unchanged-state publication on pawn reincarnation.
PASS run_sat_console_callbacks: 21 scenarios, including new exact POST mismatch,
owned body across vector reallocation/erasure, oversized shared command preserving
input, unreadable/oversized/invalid-type output on both roles. Nine existing compiled
removal controls still fail their intended runtime assertions. Production controller
and actual ParamFrame/FText/FString input wrapper compile/pass.
PASS run_sat_console_delivery: 14 existing queue/history/busy/source/recipient
scenarios. PASS ten flashlight-click groups and both production TUs.

PASS current check_current.ps1 with five selected groups: run_2145,
run_flashlight_click, run_sat_console_callbacks, run_sat_console_delivery and
sync_native_contract. Release/ABI and version formatter pass. Current evidence:
`build/qualification-current/verification.json`, source-before.json and logs/.
DLL SHA256: `18cea268033e48548bad5a2707458e5a1d2260e1383c5714c30f1a3de670d301`.
FullRegression=false. No native game startup/rendered acceptance, new archive,
profile installation, commit/push or publication. Previous snapshot statements below
are historical; the current focused evidence supersedes their DLL hashes.

# Earlier 2161 continuation — remote flashlight clicks (26 October 2026)

Public **0.9.2161**, internal b2217/protocol 2217 unchanged. Ten TODO/WORK fixes:
slot bounds; exact remote-body routing; puppet/class/world lifetime; scoped
deduplication; retryable preparation; sound and attenuation incarnation caches;
per-slot reentry ownership; finite position; and refusal of unattenuated fallback.
The one-third attenuation configuration and the local player's native click remain.
Preparation failure leaves the transition available on a subsequent apply. A native
attempt commits once before dispatch; the void wrapper does not prove audibility.

PASS ten new 32-slot groups, ten separately compiled removal controls and two
production-TU compiles (click controller and item activation). Tests use the real
roster PerSlotState and CachedObjRef with native resource/location/playback fakes.
They cover same-state recursion, nested different states, cancellation back to
the previously admitted state, roster replacement and recursive attenuation creation.
Evidence: `build/flashlight-click-check/final-focused.log` and `fault_*.log`.
The new runner is included in `package/check_current.ps1` for future full runs.

PASS **3/3 selected related groups**: run_flashlight_click, run_2145 and
sync_native_contract. Release/ABI and the production formatter pass:
**Watchers of the Void 0.9.2161**. Current evidence:
`build/qualification-current/verification.json`, `source-before.json` and
`build/qualification-current-run.log`. FullRegression remains false.
DLL SHA256:
`d663f3b9f752711a6adca219c31fcddf47c78a6d740a2317cfba9c832f04c110`.
No game startup, rendered multiplayer, full regression, new archive, commit or
publication was performed. Rendered toggle/mode-change/join acceptance, full
regression/package qualification and the native audio wrapper's internal reflected
metadata/admission lifetime remain open. Current evidence supersedes the preceding
focused snapshot contents; prior group counts describe their earlier snapshots.

# Earlier 2161 continuation — held-toy audio (26 October 2026)

Public **0.9.2161**, internal b2217/protocol 2217 unchanged. Ten TODO/WORK fixes:
full session scope; sender and recipient occupancy; frozen pre-native hand/action
identity; exact bounded PRE/POST; queue admission before dedup/rate charge; owned
incoming packet; shared 64-send budget; strict callback expiry/clock retirement;
and exact recipient completion preserving fresh join debt.

PASS legacy held-toy/world-toy/contact fixtures, ten new 32-seat lifetime groups,
ten compiled removal controls and seven production-TU compiles. Evidence:
`build/hand-toy-check/final-focused.log` and individual `fault_*.log` files.
The legacy fixture now runs real PRE/POST pairs. Unknown initial hand generation
does not create a network event that can later adopt another generation; a fresh
use after announcement sends normally. Native local use is not cancelled. The
squeak fake models SetPitch preparation and checks currency before Activate, as
the real wrapper does, rather than counting a sound before the preparation callback.

PASS **8/8 selected groups**: run_2155, run_2156, run_2157, run_toy_music,
run_watoy, run_sponge_effects, run_window_stroke and sync_native_contract.
Release/ABI and the production formatter pass: **Watchers of the Void 0.9.2161**.
Current evidence: `build/qualification-current/verification.json`,
`source-before.json` and `build/qualification-current-run.log`.
FullRegression remains false. DLL SHA256:
`1990c650e1e57b9c83d14e341d0099c86a8ad31d8fd053452dc4c95e7939f751`.
No game startup, rendered multiplayer, full regression, new archive, commit or
publication was performed. Full regression/package qualification, initial-hand
readiness and rendered music/squeak acceptance remain open. Hook-function/schema
lifetime and native presentation/authorization budgets are separate implementation
limits. Current evidence supersedes the preceding focused snapshot contents.

# Earlier 2161 continuation — watoy native wrapper (26 October 2026)

Public **0.9.2161**, internal b2217/protocol 2217 unchanged. Ten TODO/WORK fixes
cover field/class identity, unknown zero-component metadata, complete read
bindings, requester predicate at activation, audio currency, service CDOs,
mesh/template currency, checked/finite outputs, spawn candidate ownership and
direct same-View Apply/Clear reentry. The controller passes its requester predicate
into activation; reflected metadata and native query callbacks cannot silently
continue an explicitly retired request in the tested preparation boundaries.

PASS the legacy controller/native wrapper, ten controller lifetime groups and
ten new wrapper lifetime groups. PASS ten existing controller and ten new wrapper
compiled removal controls (**20 total**) and four production-TU compiles. Each
wrapper control runs its named scenario and must reach a failing assertion.
Evidence: `build/watoy-check/final-focused.log` and `wrapper_fault_*.log`.

PASS **7/7 selected groups**: run_2155, run_2156, run_toy_music, run_watoy,
run_sponge_effects, run_window_stroke and sync_native_contract. Release/ABI and
the production formatter pass: **Watchers of the Void 0.9.2161**.
Current evidence: `build/qualification-current/verification.json`,
`source-before.json` and `build/qualification-current-run.log`.
FullRegression remains false. DLL SHA256:
`225c5a080fc29194f80bfc27d25a98f0b24688c98b92043bcadabb50eb3705a7`.
No game startup, rendered multiplayer, full regression, new archive, commit or
publication was performed. Full regression/package qualification, rendered
acceptance, complete reflected schemas/function ownership, cleanup across
independently copied View owners, original-hand provenance and native-work budgets
remain open. Current evidence supersedes the preceding focused snapshot contents.

# Earlier 2161 continuation — watoy controller (26 October 2026)

Public **0.9.2161**, internal b2217/protocol 2217 unchanged. Ten individually named
TODO/WORK fixes cover full session scope, exact activation POST, bounded PRE
reservation/pruning, requester and recipient transport identity, snapshot state
flush, retired drain ownership, revision-safe pruning, admission before cue dedup
and owned incoming payload. Drains also handle a departure removing the remaining
queue during native playback. Teardown preserves a freshly installed session.

PASS legacy controller/native wrapper, ten new 32-seat lifetime groups, ten
compiled protection-removal controls and four production-TU compiles. Evidence:
`build/watoy-check/final-focused.log` and individual `fault_*.log` files. Controls
run their selected scenario and must reach a failing assertion. The legacy fixture
now supplies real invocation/function identity and expects automatic world-scope
retirement; it retains existing activation, hand-readiness and reliable FIFO cases.

PASS **7/7 selected groups**: run_2155, run_2156, run_toy_music, run_watoy,
run_sponge_effects, run_window_stroke and sync_native_contract. Release/ABI and
the production menu/browser formatter pass: **Watchers of the Void 0.9.2161**.
Current evidence: `build/qualification-current/verification.json`,
`source-before.json` and `build/qualification-current-run.log`.
FullRegression remains false. DLL SHA256:
`aa1f5388a39da63184fb669a4653d9e4498268ca3c16bfea6fb4533649fe2af0`.
No game startup, rendered multiplayer, full regression, new archive, commit or
publication was performed. Full regression/package qualification and game
acceptance remain open. Complete native-wrapper preparation lifetime, original
hand provenance and bounded native work are separate open implementation limits.
The current evidence directory supersedes the preceding focused snapshot contents.

# Earlier 2161 continuation — world toy music/squeak (26 October 2026)

Public version **0.9.2161**, internal b2217/protocol 2217 unchanged. Ten fixes
are individually tracked in TODO/WORK: session scope, requester transport,
recipient transport, exact POST, reserved PRE, admission before pulse dedup,
owned receive payload, 64-send budget, callback expiry/rollback and key domain.

PASS the wrapper/phase/clock fixtures, ten new 32-seat lifetime groups, ten compiled
protection-removal controls and four production-TU compiles. Evidence:
`build/toy-music-check/final-focused.log` and individual `fault_*.log` files.
Overflow is tested both before and during authorization, including nested newer
receipt. The old fixture now uses distinct invocation IDs for distinct native
calls and expects automatic scope retirement/rebinding on world change.
The first seven-group run failed those two obsolete fixture assumptions; they
were corrected before the complete seven-group rerun against stable source.

PASS **7/7 selected groups**: run_2155, run_2156, run_toy_music, run_watoy,
run_sponge_effects, run_window_stroke and sync_native_contract. Release/ABI and
the production menu/browser formatter pass: **Watchers of the Void 0.9.2161**.
Current evidence is `build/qualification-current/verification.json`,
`source-before.json` and `build/qualification-current-run.log`.
FullRegression is false. DLL SHA256:
`55df3183b7befd6ce4bcc4f0cb5678614ccaa17726a6894d11f17c100202b42e`.
No game startup, rendered multiplayer, full regression, new archive or publication
was performed. Full regression/package qualification and game acceptance remain open.
The following sections describe earlier snapshots; the current evidence directory
tracks the latest continuation and supersedes its prior focused-run contents.

# Earlier 2161 continuation — sponge foam and version correction (26 October 2026)

Owner correction: remain on public version **0.9.2161**. The assistant's previous
2162 label was erroneous. Active metadata/documentation is corrected to 2161;
internal b2217/protocol 2217 and the 68-byte SpongeEffect wire shape are unchanged.
Earlier immutable reports retain their original labels as historical evidence.
No new public version, release archive, publication or additional cleanup is part
of this continuation. New evidence uses `build/qualification-current/` to avoid
overwriting historical release qualification.

Implemented the ten individually named TODO items: session retirement; exact
invocation and POST pruning; PRE reservation; original item/hand address;
actor/class/key authorization; source occupancy; recipient occupancy; queue
admission before dedup; bounded native/send work with expiry/clock recovery; and
current actor/player/class/player-link/hitResult schema through trace capture.

PASS existing controller/wrapper, ten new lifetime groups at 32-seat capacity,
trace-retirement/recovery cases, eleven compiled fault controls and four production
TU compiles. Evidence: `build/sponge-effects-check/final-focused.log` and individual
fault logs. The budget regression also checks ready effects behind eight unready
mirrors; presentation rotates independently of wire FIFO. PASS **5/5 selected
groups** on the final stable snapshot: sponge effects, window stroke, cooking,
grime join and the cooked synchronization contract. Release/ABI and the actual
production formatter also pass; displayed label: **Watchers of the Void 0.9.2161**.
Evidence: `build/qualification-current/verification.json`, `source-before.json`
and `build/qualification-current-run.log`; FullRegression remains false.
DLL SHA256: `d09f1756759245a3a893b6116e12f460d2e679daf02033a608871761ff20a943`.
Earlier 95-group/archive results do not qualify this changed implementation.
Full regression/package qualification and rendered multiplayer remain open.

# Historical native canvas continuation b2217 (26 October 2026; mistaken 2162 label)

Started 0.9.2162 after previewing/running cleanup against the qualified 2161
metadata: 752 files / 535.9 MiB removed, retaining the exact prior archive/DLL
hashes. Removed obsolete 2160 delivery and generated test outputs/staging/caches;
source, dependencies and unique research/verification/crash evidence remain.
Current inventory: `package/cleanup-inventory.json`.

Implemented ten native-window-canvas fixes documented in TODO/WORK: retriable
incarnation-owned metadata, live raw observers, guarded/whole brush reads and
known stock, checked factory/type/pin publication, detached teardown, current
packed signal gate, source/target/component/function fences, checked argument
sizes/offsets/Vector2D and retained pre-draw retries with at-most-once admission.

PASS 14 controller + 14 actual-wrapper/ParamFrame scenario groups and 17 compiled
fault controls (eight controller, nine wrapper), plus both production-TU compiles.
The cooked 0.9.0n graph confirms canvas only opens/joins and rearms its 0.25-second
close; K2_DrawMaterial is a separate painting admission. The wrapper keeps native
ownership and never calls setDraw/endDraw directly. Native file hashes and scope:
`build/window-stroke-check/native-contract.json`; focused evidence:
`build/window-stroke-check/final-focused.log`.

The complete final b2217 run passes **95/95 current-source groups** without
prior-version result reuse. Release/ABI, the production formatter's 2162 label,
32-member archive and five package rejection controls pass. Qualification:
`build/release-2162/verification.json`; archive/control evidence:
`build/package-current/verification.json` and `rejection-controls.json`.
Prepared locally; no commit, push, publication or game installation.
DLL SHA256: `0f34cdf7f1d276468f9c86d4a7de112aaf539b14c810bb70ff8b78c8385fa769`.
ZIP SHA256: `c80bfe4c677fbf6ba0b4904b489128707faeb69bd1bbb645920b57956cfa1601`.
Partial full runs were stopped for review corrections: native
cleanBubbles can leave finite negative soap (subtract five without clamping), and
Vector2D schema incarnations must remain owned through the final signal check
and actual draw admission. Added late-schema replacement scenarios
and a fresh cooked final-soap contract; all focused checks/17 controls pass again.
Preparation also requires the audited base canvas entry: inherited subclasses
are allowed; an unverified override cannot enter a supposedly repeatable path.
The complete qualification restarts on that corrected production snapshot.
Rendered/game startup and master deployment remain unverified. Original-dab
brush provenance, atomic material-scalar sampling, thrown sponge transport,
independent periodic grime, dynamic-source completeness and full-save parity remain open.

# Bay-window stroke continuation b2216 (26 October 2026)

Implemented ten window-stroke delivery/lifetime fixes for package 0.9.2161:
source retirement across world/start serial/role/running/link changes; retained
FIFO refusal and sealed brush payloads; eight-row send budget and 20-second expiry;
owned completion across nested callbacks; validated ranges and ready host origins
with host-relayed client slots; no oldest-row eviction at capacity; serial/world
scan identity; guarded bounded at-most-once replay and hook recovery.

PASS `agent/tests/run_window_stroke.ps1 -FaultControls`: 13 scenario groups and
seven compiled controls reach their intended failing assertion when the named
protection is removed. The actual production translation unit compiles separately.
Coverage includes same-world reconnect without waiting for a scan backstop,
32-seat origin bounds, nested replacement queues and old callback completion.
The runner is registered in the complete current qualification.

The first full-suite attempt was intentionally stopped after review identified
the same-world scan-cache issue. Focused checks/controls passed again after that
fix, and the complete suite restarted from the stable b2216 source snapshot.
All 95 groups then ran on that stable snapshot. The only failure was the upstream
integration fixture's hardcoded protocol 2215 assertion. Changed it to require the
release metadata's protocol and at least 2215, retaining all wire-ID/32-seat checks.
The corrected group reran after Release/ABI completed; production and DLL hashes
matched the full run. PASS **95/95 groups** after that rerun, with no 2160 results
reused. Initial qualification/failure remains in
`build/release-2161/initial-verification.json`; the final report's
`CorrectedTestRerun` records the original hash, corrected test hash, retry log and
matching production stamps. Current report: `build/release-2161/verification.json`.

PASS production menu/browser formatter: Watchers of the Void 0.9.2161.
PASS 32-member manager archive, loader layout, branding, models, matching qualified
DLL and five failed/incomplete/focused/stale-label/wrong-DLL package controls.
Evidence: `build/package-current/verification.json`, `rejection-controls.json`
and `dist/SHA256SUMS-current.txt`. Prepared locally; no commit/push/publication.
DLL SHA256: `45b83eece370901457248db4012ab9b01f6c851af06318458a75462ec5888566`.
ZIP SHA256: `ab201777ed50b2a67984a8f1a38814f3bb6ab68147b771fe93b27ad22878eaad`.

No rendered/game startup or master deployment was performed. Uncertain admitted
native draws are not retried. The brush is first sampled at the outgoing drain;
exact original-dab hand/material provenance, thrown sponge transport, independent
periodic grime, full-save parity and complete native-wrapper lifetime remain open.

# Version-start cleanup for 2161 (25 October 2026)

Next development target is 0.9.2161; the last qualified release remains
0.9.2160 / DLL b2215 / protocol 2215. No 2161 build or gameplay pass is claimed.
Updated the recurring cleanup workflow and reconciled TODO/WORK before fixes.

Previewed and ran `package/cleanup_current.ps1`: 1,038 files / 849.4 MiB removed,
including obsolete 2158 archives, superseded publication drafts, generated test
binaries, copied test outputs, package staging and Python caches. Active source,
test runners, CMake/dependencies, unique research and verification/crash evidence
remain. Detailed targets and retained hashes: `package/cleanup-inventory.json`.

PASS `package/package_current.ps1 -VerifyOnly` after cleanup: the retained 2160
archive still has 32 members and matching loader layout, branding, models,
identity and qualified DLL. Retained archive/DLL SHA256 values are unchanged.
This verifies package preservation only; it does not rerun the 94 regression
groups or close native startup/rendered multiplayer acceptance.

# Package 0.9.2160 / b2215 (25 October 2026)

Prepared the requested manager package with the combined upstream/fork source
and ten native damage/slice/hit fixes now checked in TODO. Those fixes have 23
scenario groups and ten compiled source-copy controls; actual getter dispatch,
hit construction, parameter bindings, owner/class/source lifetime and wall
destruction run through controlled native boundaries. Cutting/fragment gameplay
is a separate path and remains unaccepted.

The canonical production snapshot exactly matches the already qualified 94-group
integration snapshot `67fc6c1e8c87ae12efbaf938d60248bf0d4d84d42e7fe5fad71fc0ced348ff88`.
The new metadata-driven `package/build_current.ps1` reuses that complete evidence
only after matching protocol and production hashes. `FreshFullRegression=false`
and `RegressionReuse` retain the original report/hash/scope; this is not a new
execution of all 94 groups on the changed public-version label.

PASS fresh Release/ABI **0.9.0n b2215**, 842 build source entries, and the actual
menu/browser formatter using its generated header: **Watchers of the Void 0.9.2160**.
PASS complete manager archive: 32 members, exact DLL/content hashes, identity,
enabled loader, shimloader dependency, 256x256 icon and starter-model paks.
PASS five package controls rejecting failed, incomplete or focused qualification,
stale labels and wrong DLLs. Public prose and final diff checks pass.
PASS current fact-row gate: **565 rows / 181 files**, reference 0.9.0n.

DLL SHA256: `e859eada7c2da04d7e60fd654d418678621158385ec6eab6d495b5b6826bc534`.
ZIP SHA256: `5522ba389cc46535336e8a967c525f015a28c7931b47c059f306cb77725be59a`.
Archive: `dist/ElvardFIC-WatchersOfTheVoid-0.9.2160.zip` (15,185,210 bytes).
Evidence: `build/release-2160/verification.json`, retained reference qualification
and source snapshot, `build/package-current/verification.json`,
`build/package-current/rejection-controls.json`, `dist/SHA256SUMS-current.txt`.

The requested GitHub README/changelog/release descriptions are published in
`157fefd70743e93392fa1b0c2affc41dd9e01aa5`; the shared 201-character short About
description is updated through the official API and read back. The connector
publishes the documentation commit with a checked branch lease. Local main is
fast-forwarded only after every published file matches its local content.
Evidence: `build/release-2160/github-publication.json`. No GitHub binary release
is created. The new Thunderstore README/manifest/changelog are inside the ZIP,
but the live page has not been updated here: its upload API access is missing.
No game run, game installation or master deployment was performed. Existing
rendered, event/effect and debit/output/refund acceptance remains open.

# Upstream integration b2215 (25 October 2026)

Merged VOTV-MP/Multivoid through `ad531561142ff5685153dc799f9ff2de4af07056`
with all fork history and the latest local native-action/hit-result fixes
`ddc711220`. The fetched graph originally had 607 commits ahead / 923 behind;
the integrated graph has **0 upstream commits missing**. Own commits remain.
Origin `68f0c2a7f` is also an ancestor; no reset, rebase or force push is needed.

Upstream rewrote its old `93dd26cc2` history; the corresponding rewritten
`df76e6f66` differs in 15 cleanup files. Ordinary merge chose the ancient
`1afb8c2ef` and conflicted over already-shared changes. Three-way content
reconciliation uses preserved shared commit `2276a14c8` and resolves 53 paths,
while the merge records the actual branch tips as parents. The subsequent
latest-local merge uses our original `d00610643` content baseline.

Combined behavior:

- Keep 32-player chooser/admission/master capacity and all 226 published fork
  message values. Protocol/build is **2215**, with fifteen additions at 238?252.
  Legacy PlayerDamage=20 and AtvRelease=71 remain reserved and explicitly refused.
- Import host-judged damage, creature-hit intents, parked-creature presentation,
  full ATV ownership/seat/syncer tables and join sets, shared-prop transactions,
  notebook/paper editing, text codec and device/UI/localization fixes. Retain
  PvP, native-state guards, full NPC rotations, Kerfur and sit presentation.
- Retain ATV companion placement/collision checks, save-key recovery,
  steering, cockpit/modules, radio URLs, service/refuel/repair and honk retry.
  Only the host ejects tyres; clients consume its resulting mask. Failed native
  ejection publishes actual native state and the assigned syncer owns sends.
- Keep owned record queues, safe/can/grill guards and exact source/class/revision
  checks. Class-gated or held records wait; successful native application precedes
  publication, including the actually adjusted record and reentrant replacements.
- Use one native explosion pipeline; retain fork radial prop discovery and
  force-only motion through the idempotent coast lane. Old blast controllers
  remain testable but their duplicate runtime Install/Tick paths are disabled.
  PC minigame blasts stay personal, without dissolve, broadcast or parking.
- Keep drone lifetime/retry guards and legacy absolute requests; new timer presses
  toggle current host state. Two queued presses restore the original setting.
  Desk ping progress and pending verdict/reply retries both run.

PASS **94/94 full regression groups** on the stable combined source. Includes
118 Rust master tests, native/cooked contract checks and 18 routing mutation
controls. Independent baseline checks preserve every published message value;
new fixtures execute the actual blast decoder, drone intent controller and ATV
condition controller. The latter reproduced the duplicate-ejection failure before
its fix. Steering fixtures exercise the actual complete-set sender and retained
join honks. Additional fault runs pass all **11 item-record** and **10 native
prop-action/hit-result/wall** mutation controls, which must fail at their intended
assertions. Engine/transport seams in controller tests are controlled doubles.

Full qualification command in the isolated integration checkout:
`package/check_current.ps1 -BuildDirectory build/integration
-EvidenceDirectory build/upstream-qualified-final-2215`.
Final evidence is retained in the canonical checkout at
`build/upstream-qualified-2215/verification.json`, with all 94 runner logs and
fault-control logs. Paths inside the original evidence retain their execution
locations; the qualified DLL is copied byte-for-byte beside the evidence as
`build/upstream-qualified-2215/main.dll`. Production snapshot SHA256:
`67fc6c1e8c87ae12efbaf938d60248bf0d4d84d42e7fe5fad71fc0ced348ff88`.

PASS stable-source **Release/ABI b2215**, 842 production source entries.
DLL SHA256:
`36bf0697655c6ebeaaadf721f20b7da86f87c076acca5b9ed2bceb6c0256b290`.

Rendered multiplayer, actual keyboard input and full-manager startup remain
unverified. Every peer must use b2215. No distribution archive, game installation
or master deployment is part of this integration. Existing acceptance items stay
open in TODO and WORK.

# Development b2214 - prop-action/MRE delivery (25 October 2026)

Ten completed fixes are checked in TODO. Prop action queues now own target/class
identity and a numbered entry before the first send. Native key/send callbacks
cannot borrow invalidated queue storage, repeat a draining head, overtake an older
request or erase a newly installed queue. Exact source/revision completion and
owned payloads preserve FIFO across refusal and nested actions. Each drain handles
at most eight rows; the existing capacity, expiry/clock, key and MRE hand-transition
guards remain. Session pointer/serial, world, role, running and connection changes
retire queued actions, buckets, safe retries and MRE scopes.

Client world-MRE opening now refuses malformed/foreign-source/unscoped reflected
calls, coalesces a pending open and guards request feedback through callbacks.
Personal MRE loot requires the exact active native spawn scope, declaring function
and actor/class/player ownership. A tracked native hand opening can clear the hand
before spawn without losing that provenance. Bounded scopes use exact POST and
active-invocation pruning; a hand pointer alone no longer admits loot.

Received host opens capture readiness/person/body before authorization and retain
target/key/function/owner/mesh currency through ParamFrame construction, argument
binding and hit preparation. A bounded per-actor reservation prevents nested
received opens; failed preparation releases it, while an admitted dispatch remains
latched until actor/class or source retirement. This is conservative at-most-once
admission, not proof that native output committed. Host-local native actions retain
their existing path. Safe capture/retry now owns actor/class/revision snapshots;
native key reads and old publication completions preserve fresh requeues.

PASS **18 new controller scenario groups**, including six source transitions,
four capture retirements, exact payload/order after nested installation, queue caps
and eight-row budget, expiry/clock/key/class retirement, MRE request/cue guards,
cleared-hand output, nine scope retirements, received-opening dedup/preparation
retry, 36 native preparation combinations, six authorization changes and safe
requeue/capture lifetime. Engine/native/transport seams are controlled doubles.
PASS nine compiled source-copy controls at their intended assertions: prequeue
send, recursive drain, budget, session serial, target class, missing spawn scope,
duplicate received open, native function incarnation and safe revision.
Evidence: `build/prop-action-delivery-check/final-2214.log`. Both production TUs
compile against real headers.

PASS fresh cooked MRE contracts: native action-4 entry calls spawn, ripping cue
then destruction; three loot factories and ancillary poison-pour finish directly
inside spawn. Native name/food initialization, weighted choices, jackpot reward,
poison and roach/portion side effects are retained. This audit establishes the
scope boundary; it does not establish multiplayer parity for ancillary effects
or change native reward routing.

PASS **6/6 selected groups**: run_prop_action_delivery, run_2162, run_2168,
run_cooking, public_prose_include and reliablekind_gate (18 refusal controls).
Evidence: `build/prop-action-delivery-2214/verification.json` (`FullRegression=false`).
PASS stable-source Release/ABI **b2214 / protocol 2214**, 793 build source entries.
DLL SHA256: `a4a227726743b7eec1178b289e1495a8eb4920ff64942329c3ba388285b0aac3`.
Production snapshot SHA256:
`51c6e5a78e4f30052c58bdf316ad46dccd7611e868af492da26d6f3a3f6aca05`.
The current 92-group suite was not run in full; the last full aggregate remains
83/83 on b2203. Final diff and new-file whitespace checks pass.

All checks run locally. Rendered opening, package debit/output/refund recovery,
remaining package subclasses, ancillary effects/rewards and full-save behavior
remain open. No game run, package, installation, commit, publication or remote
update was performed.

# Development b2213 - food callback/output ownership (25 October 2026)

Ten implemented fixes are checked in TODO. Client eat admission now requires a
bounded, active observation, an addressable actor and a successful valid read
with positive portions. Reserving before native reads refuses nested eats of
the same item. Key/element and actor/class revalidation prevents a callback from
retagging that observation. Exact POST owns and detaches its record without
rereading caller parameters. Captured last-bite delivery and refused-send retry
survive native destruction. Food caches and callback ownership retire across
world, session pointer/serial, role, running and connection transitions.

Empty box/banana/cup authoring now requires an active native eaten scope and its
exact declaring graph. A captured local-hand eat retains that provenance when
the native final bite clears the hand before eaten. World and mirror outputs
keep host ownership. Bounded scopes validate the actor/class, function/owner,
player and active invocation; exact POST or skipped-POST pruning removes them.
Host bites require valid current food data and preserve source/key/class currency
through authorization and writes. Final output/destruction rechecks zero portions
after publication and native preparation. Refilling/replacing food during those
callbacks cannot leave an empty wrapper or destroy the replacement. Native
function/owner replacement retires the old prepared completion.

PASS **18 new controller scenario groups**, including six invalid-condition
cases, eight PRE retirements, nine output retirements, both capacity refusals,
key/POST ownership, nested eats, cleared hand, dead final delivery, three allowed
factories versus personal overrides, refilled final state and four native
function/owner preparation changes. Engine/native/transport calls are controlled
boundaries; production synchronization code runs unchanged. Both production TUs
compile against real headers.

PASS eight compiled source-copy fault controls: untracked eat, invalid condition,
nested eat, captured address, session serial, missing output scope, refilled final
state and native function incarnation. Each reaches its intended assertion.
Evidence: `build/food-callbacks-check/final-2213.log`. PASS fresh cooked native
contracts: final portion decrement precedes eaten/output/destruction; exactly
three eaten paths spawn wrappers, while six personal overrides and the empty
base are excluded from host replay. Personal hunger, poison, hints and
achievements continue to run on the eater.

PASS **8/8 selected groups**: run_cooking, run_food_callbacks, run_2140,
run_2174, run_grill_hand, run_grill_edits, public_prose_include and reliablekind_gate
(18 refusal controls). Evidence: `build/food-callbacks-2213/verification.json`
(`FullRegression=false`). PASS stable-source Release/ABI **b2213 / protocol 2213**,
793 build source entries.
DLL SHA256: `05ea31f7c207aee8f499a5670681f83b7d83708f4a675964351b5a39357e3e99`.
Production snapshot SHA256:
`63cbd508943f485822a2d8e00a8fd193d5073d6a681c5699230c9812c873df05`.
The current 91-group suite was not run in full; last full aggregate remains
83/83 on b2203. Final diff checks pass.

All checks run locally. Cross-peer concurrent consumption/output transactions,
optimistic personal effects before host acceptance, remaining package subclasses,
full-save behavior and rendered multiplayer acceptance remain open. No game run,
package, installation, commit, publication or remote update was performed.

# Development b2212 - wall material delivery (25 October 2026)

Ten completed changes are checked in TODO. The deferred host wall/material queue
now expires ownership across world, session serial, role and running transitions.
Both actors and their class incarnations are captured before native enrollment;
malformed or callback-retired input never queues a spend. The active pair remains
visible during preparation, and its one replay permit is claimed before another
callback can steal or requeue that pair. Recycled identities cannot mint a new
queued attempt from an old replay.

Native FeedWhile guards the caller through metadata binding, collision query and
overlap dispatch, including actor/class/component/field/function incarnations.
Only failures before native admission retain a paced 250 ms retry. Rotation keeps
later due pairs moving, with eight actual attempts per tick, the existing expiry
and bounded queue. Once the native gate admits that queued attempt, it is retired
even if dispatch fails or POST is skipped: its graph may already have spent
material. Admission is conservative and does not claim every admitted body ran.

Native POST records are invocation/source/actor owned, detached before callbacks,
bounded and pruned when cancellation/fault skips POST. Source-current partial wall
state is marked for publication when admitted work lacks a successful POST;
material-spending code is never rerun to obtain that state. Fresh callback queues
and nested session installations survive old completions. Six guard registrations
are attempted independently; accepted protections remain while only missing ones
retry. Solo hosts and last-peer departure retain native host ownership.

PASS **20 new scenarios** using the actual controller, wrapper and ParamFrame
with controlled engine/native dispatch doubles. Coverage includes admitted failure
versus pre-admission retry, pacing/fair budget, lifecycle retirement, 14 collision
query changes, enrollment invalidation and reentry, nested body/tick callbacks,
20 metadata retirement combinations, exact registration retry, stale POST,
no-spend repair, missing-POST publication, replacement queue/install, solo hosting,
four pre-dispatch identity/class changes, component requery and body-record cap.
Both production translation units compile against real headers.

PASS eight compiled source-copy controller/wrapper controls, each reaching its
intended assertion: admitted retry, ignored session serial, enrollment reentry,
unguarded wrapper source, stale collision component, registration short circuit,
stale POST and wrong replay pair. Evidence: `build/wall-delivery-2212.log`.
PASS existing placement/records/authority/wrapper, wall enrollment/damage and
creative construction regressions: `build/wall-baseline-2212.log`,
`build/wall-damage-2212.log`, `build/wall-creative-2212.log`.

PASS **6/6 selected groups**: run_wall_delivery, run_2166, run_2168,
run_wall_creative, public_prose_include and reliablekind_gate (18 refusal controls).
Evidence: `build/wall-delivery-2212/verification.json` (`FullRegression=false`).
PASS stable-source Release/ABI **b2212 / protocol 2212**, 793 build source entries.
DLL SHA256: `a0288a41fb67ae40b03c1cca236315b489055a49318a29f766adc0a14e771a38`.
Production snapshot SHA256:
`91c0faecf0c358b171a23d2522780b58c60c432e810829e4162f63017b36f51a`.
The current 90-group suite was not run in full; last full aggregate remains
83/83 on b2203. Final diff checks pass.

All verification runs locally. Paid client repair remains a no-spend refusal:
host request/result acceptance and exact personal-stock accounting across
hand/slot/drop, refusal and disconnect remain open. Native rendered material
consumption, wall construction/impact/destruction and ordinary game acceptance
still need current-DLL gameplay checks. No game run, package, installation,
commit, publication or remote update was performed.

# Development b2211 - local container failure recovery (25 October 2026)

Ten completed fixes are listed in TODO. Base world-container extract now reserves
bounded, invocation-owned full-record escrow before native takeObj can debit its
source. Exact debit/pocket receipts distinguish accepted personal credit from
refusal. Checked full-record restore owns its input and rejects incomplete scratch
or stale frame/source state before loadData. No generic native loadData result
bool is used: the base implementation does not reliably assign it.

Failed/null spawning, including an alive deferred actor without a finished actor,
and failed restoration/full-inventory refusal settle locally by retiring the exact
carrier before atomically returning the retained record to its current source.
Destruction/refund refusal retains paced debt. Same-key/class dedup preserves newer
data; conflicting identity classes refuse. The append stages checked engine-owned
data, revalidates save/component/index/headers and copies latest other rows only
after callback-capable preparation. Cleanup reentry cannot turn a committed refund
into another attempted credit.

The contents lane is notified after a current-source commit. Both container
derivation paths rebuild names before volume/mass, as native volume calculation
reads nameData. The recovery caller's source guard spans those native callbacks.
The existing birth suppressors now cover the whole native extraction and its
pending carrier, preventing default-state publication before the transaction ends.
Stopped-session teardown attempts only proven refunds against the original live
source; a nested installation keeps its own state.

PASS 16 production-controller scenarios on controlled engine/native VM seams,
including both roles, owned data after caller locals expire, null/partial spawn,
successful pocket, full inventory, refused destroy/refund retry, skipped receipts,
nested extracts/installation, class/source retirement, teardown settlement and
pre-debit capacity refusal. PASS nine real-wrapper groups using actual
container_inventory/save_record/metadata code: all typed groups and zero values,
same-save nested index, every allocation point, class/name/string/signal failures,
key/class dedup, 13 source/header retirements, owned/nested input, current in-place
edits and late same-key return, cleanup reentry and personal-store exclusion.

PASS seven compiled controller fault controls: ignored refund refusal, carrier
quarantine, partial spawn, ambiguous debit, admission cap, nested install and
accepted credit. Each reaches its intended runtime assertion. Evidence:
`build/container-recovery-check/final-2211.log`. Existing codec regression plus
ten codec fault controls and source compilation also pass:
`build/save-record-check/checked-restore-2211.log`.

PASS fresh cooked extraction/debit/pocket-failure and names/volume contracts.
The new extract reference is generated from saved live 0.9.0n canonical capture
text; its whole-text hash and the existing takeObj signature/body control match.
The 68-line capture is retained in tests. PASS fact-row gate: **517 rows / 166
files**, `build/container-recovery-check/fact-rows-2211.log`.

PASS **9/9 selected groups**: run_container_recovery, run_container_birth,
run_container_apply, run_inventory_apply, run_save_record, run_2147,
inventory_capture, public_prose_include and reliablekind_gate. Evidence:
`build/container-recovery-2211/verification.json` (`FullRegression=false`).
PASS stable-source Release/ABI **b2211 / protocol 2211**, 793 build source entries.
DLL SHA256: `c48f57b607a0f5bbace1e19239b39d654f8a0370c3bbc1669739da2a26ac61fe`.
Production snapshot SHA256:
`980cff1745837906bc6978c04da6dc9935e685a975baaf076c159971e84c1f25`.
The current 89-group suite was not run in full; last full aggregate remains
83/83 on b2203. Final diff checks pass.

All verification runs locally. This completes proven local base-container failure
recovery; it does not close cross-peer debit/output arbitration, personal extract
overrides/raw takeObj, rendered multiplayer acceptance or persistent recovery.
Receipt-less faults keep bounded unresolved debt instead of speculative refunds;
world/session replacement clears that debt. Teardown settlement is best effort,
so guaranteed disconnect failure recovery remains open. No game run, package,
installation, commit, publication or remote update was performed.

# Development b2210 - console input and native playback (25 October 2026)

Ten input/playback changes are checked in TODO. Client shared-world commands
consume native input successfully before submission and refuse nested commits of
the same input. A full command outbox refuses before consumption. The guarded
wrapper prepares FText/SetText before changing the command, validates source,
terminal/class/function/textbox serials and field ownership around reflected
work, and returns failure after invalidation. Failed consumption sends no command
and does not allow the original world-command body to execute on the client.

Client preparation and counters retain their source/widget/host-binding token.
Local command/graph records use invocation IDs, preserving nested clear/output
ownership and pruning canceled/faulted calls whose POST never runs. Exact POST
ownership detaches before native reads; source and revision checks protect busy
state and latent output. The graph watch and its rollback use the same callbacks.

Display rows retain source, target and clear revision across native dispatch.
A failed row may retry ahead of an ordinary append, while a newer clear retires
that old row. Widget replacement stops the drain; client host-binding replacement
retires old text and mirror busy debt, including discovery immediately before a
drain. Each replay scope owns expected arguments and claims one direct write
invocation. Nested writes/commands, pre-claim alerts and replacement-session
output follow normal canonical synchronization.

PASS **17 callbacks scenarios** using the actual controller, wire codecs,
chunker and assembler, with controlled native/transport doubles. Coverage
includes 63 client read/consume retirement transitions, failed input recovery,
duplicate Enter, consume-before-send ordering, stale POST and native arg reads,
nested clear, skipped POST, latent local graph output, failed row clear/append,
display source/widget/host-binding replacement and recovery, replay attribution,
new-session output, pre-claim/probe reentry and exact watch rollback.

PASS the actual input wrapper with real ParamFrame, field_io, FString and FText
implementations: 39 preparation callback invalidations, five post-SetText
invalidations, no textbox, dead textbox, failed mint/frame/parameter/native calls
and ownership/serial replacement. Preparation failures retain original input;
a native failure after mutation is refused, without claiming a complete rollback.

PASS nine source-copy callback fault controls: ignored consume refusal, duplicate
input ownership, stale POST, missing unwind pruning, clear revision, widget stop,
replay invocation/preclaim and mismatched watch rollback. Every control compiles
and reaches its intended runtime assertion; compile failures do not count.
Evidence: `build/console-callbacks-2210/fault-controls/callbacks.json` and adjacent
assertion logs. Existing delivery scenarios and all eight delivery controls pass:
`build/console-callbacks-2210/fault-controls/delivery-controls.log`.

PASS existing console/Madness controllers, Madness presentation and six production
integration TU compiles: `build/console-baseline-2210.log`.
PASS **6/6 selected groups**: run_sat_console_callbacks, run_sat_console_delivery,
run_2154, run_session_reports, public_prose_include and reliablekind_gate
(including its 18 refusal controls). Evidence: `build/console-callbacks-2210/`.
PASS stable-source Release/ABI **b2210 / protocol 2210**, 792 build source entries:
`build/console-callbacks-2210/verification.json`.
DLL SHA256: `4e8687d8cc2657687aa2a76a11044c94bd5ce47d656da23b4b12adc03ecdb1af`.
Production snapshot SHA256:
`b0b8d9573b9afc9fa4f7b2002a26b63f3fc75359480048bbce0a608f7d7d0f1a`.
FullRegression=false; the current 88-group suite was not run in full. Last full
aggregate remains 83/83 on b2203. Final diff checks pass.

All verification runs locally. Rendered console input, multiplayer world-command
effects and ordinary game startup remain open. Other events/entity variants and
personal resource transactions remain separate work. No game run, archive,
installation, commit, publication or remote service update was performed.

# Development b2209 - console delivery and lifetime (25 October 2026)

Ten console changes are checked in TODO: 31 remote terminals; current-source
guards; recipient generation/GUID/leave-incarnation binding; owned outgoing
blobs; exact queue entry completion; nonreentrant per-recipient flush; guarded
real wire chunks and retained refusals; revision-owned history/current-busy
reseed with canonical all-observer clear ordering; both busy levels and
widget-bound local holds; detached terminal/session teardown and guarded native
construction, context initialization and command preparation.

The actual controller retains its bounded text history and paced outbox. A join
replays current text/busy state without executing a previous native command.
Source/recipient changes stop old callbacks before committing a cached context
or running a command. Nested transport callbacks cannot pop a replacement queue
head or make later observers receive old text after a clear. Retired terminals
and session state are detached before callbacks, so a nested replacement session
keeps its own state.

PASS local `run_sat_console_delivery.ps1 -FaultControls`: 14 scenarios using
the production controller, wire codecs, chunk sender and assembler, with
controlled engine/transport doubles. They include all 31 remote seats, slot 31,
generation/GUID replacement, partial assembly retirement, queued append/replace,
world/serial/role/link/session changes, refused/mutated history, busy reseeding,
widget reincarnation, nested teardown/install, detached eviction and a nested
clear during a multi-chunk broadcast to three recipients. Native preparation
covers 27 source/recipient changes across busy read, context init and panel write.
All eight production-source fault controls fail their intended assertions:
capacity, recipient invalidation, nested flush, world identity, false busy,
history revision, broadcast ordering and native command source. Compile failures
do not count. Evidence: `build/sat-console-delivery-check/fault-controls-2209.log`.

PASS existing console/Madness controllers and Madness presentation, plus
production-header compilation of six integration translation units:
`build/console-baseline-2209.log`. The new runner also compiles the console
controller against production headers.

PASS **5/5 selected groups**: run_sat_console_delivery, run_2154,
run_session_reports, public_prose_include and reliablekind_gate (including its
18 refusal controls). Evidence: `build/console-delivery-2209/`.
PASS stable-source Release/ABI **b2209 / protocol 2209**, 792 build source entries:
`build/console-delivery-2209/verification.json`.
DLL SHA256: `febebb9da4e71f84ec5173afc57c7bfa68d52035cd4b8a9ab0d8810e2bc3aab1`.
Production snapshot SHA256:
`05d591b5340d92f56210aa5f1df4bd681ca105163c48a92963e630fc90655bab`.
FullRegression=false; the current 87-group suite was not run in full. The last
full aggregate remains 83/83 on b2203. Final diff checks pass.

All verification runs locally. Rendered simultaneous console use, world command
effects and real keyboard input remain open. Broader Rufus/thiccfus, llama,
Madness/event variants and other entity effects are not closed by these changes.
No game run, release archive, game installation, commit, publication or remote
service update was performed.

# Development b2208 - main-menu and promotional cleanup (24 October 2026)

Removed the mod's tester/reviewer thanks block from the main-menu tick. Its
renderer, widget builder, embedded text/resource, startup initialization, cache/
parser, browser refresh and client FetchThanks API are retired together. CMake
and the HTTP/prose censuses no longer retain those inputs or the /v1/thanks client
operation. The server's legacy endpoint remains available to older clients.

Removed voluntary original-author promotion from the boot banner, current root/
package README copy, distributed branding changelog sentence, future package
manifest description and repository Sponsor metadata. Current maintainers remain
credited. LICENSE, THIRD-PARTY-NOTICES and historical source provenance are
unchanged; no attribution is falsely reassigned. UI/master/release documentation
now describes the current client rather than the removed roll.

PASS **3/3 selected groups**, stable-source Release/ABI b2208 / protocol 2208,
792 build source entries: `build/menu-cleanup-2208/verification.json`.
Groups: run_host_players (including its 118 Rust library tests),
public_prose_include and reliablekind_gate.
DLL SHA256: `4868a0d7b5ae588c8e75575cd533457d718b9a44aa450b5ce62d9a982cc0d7be`.
Production snapshot SHA256:
`82d2bdcf2df6bb3510eea7bb0650cd02e3b4c29dfa1f9a2fe9ee567f4897df92`.
FullRegression=false; the 86-group suite was not run in full. Last full aggregate
remains 83/83 on b2203.

PASS master contact census: 8 endpoints across two files, no failures;
`build/menu-cleanup-master-contact.log`. Scans find no retired roll/cache/client
API/endpoint references in current production code or promotional original-author
references in the edited current copy. The license/third-party files have no diff.
PASS artifact check: LoadLibraryEx loaded the current DLL as data only (no DLL
entry execution), and FindResource confirms retired RCDATA ID 320 is absent:
`build/menu-cleanup-2208/resource-check.json`. Diff checks pass.

All verification ran locally. Visual in-game main-menu acceptance remains open;
no game run, release archive, game installation, commit, publication or remote
service update was performed. TODO records the ten concrete cleanup steps without
claiming ten unrelated gameplay fixes.

# Development b2207 - complete 2–32-player selection (24 October 2026)

Ten completed changes:
1. Capacity has its own row in native session settings, with window height for
   the extra controls and an explicit total-including-host label.
2. Quick 2/4/8/16/32 buttons complement minus/plus for every integer from 2 to 32.
3. A typed net.player_limit row remembers the choice, defaults to four and bounds
   both the UI and environment/config host path. Failed disk writes retain the
   live choice and can retry persistence later.
4. Native and overlay menus share the same selection. Slider dragging changes
   memory; release/host remembers once. Serialized choices and cached reads avoid
   competing defaults, per-frame ini reads/writes and render-thread widget calls.
5. Partial build/menu cleanup clears every selector pointer. Endpoint controls
   dim, ambiguous hover is refused and all external selector inputs clamp.
6. ReadNetConfig carries the registered limit into scripted/environment hosting.
7. The legacy HostLobby path copies the selected capacity into its actual network
   Config, closing its advertised-limit versus default-four admission mismatch.
8. Environment hosts announce their actual configured limit instead of four.
9. Initial and retained/re-announced lobby requests normalize to the same bound.
10. Browser counts stay within host-inclusive capacity, with safe signed/unsigned
    64-bit JSON clamping. Master heartbeat counts cannot erase the hosting seat,
    and create replies echo the accepted players_max without changing old fields.

PASS `run_host_players.ps1 -FaultControls`: actual selector and config registry,
all 31 limits/presets/endpoints, cached reads, concurrent choices, delayed writes,
reopen, failed persistence and every one of eight partial-build points. Extracted
production HostWithSave/HostLobby/environment bodies cover 496 save/mode/listing/
master-availability combinations plus every legacy/environment limit. Real lobby
list parsing covers missing/wrong-typed and extreme 64-bit counts.
Seven controls fail after removing remembered choice, persistence, bounds,
partial-build cleanup, deferred writes, legacy capacity or environment capacity.
Evidence: `build/host-players-controls.log`.

PASS all **118 Rust library tests**, including each 2–32 limit through signed
create, heartbeat, browse and join, legacy/default/bad/out-of-range capacity and
host-inclusive count bounds. Rust 1.99.1 runs from a portable workspace toolchain;
the system PATH was not changed. Optimized local Windows master build passes:
`build/master-player-limit-release.log`, `server/target/release/coop-master.exe`.
Master SHA256: `8d5af3ec8a34bbf4fcce6734998f3a2c754158bf642569d6aff52965646e4ba7`.
PASS config registry gate: 292 rows plus five font-role rows and its fixture
controls, `build/host-players-registry.log`.

PASS **7/7 selected groups**, stable-source Release/ABI b2207 / protocol 2207,
794 build source entries: `build/host-players-2207/verification.json`.
Groups: run_host_players, run_sync_2157, run_sleep_personal, run_profile_join,
run_session_reports, public_prose_include and reliablekind_gate.
DLL SHA256: `6e681dc776f64d68165095e8913a5baf8753c3a34221fdc2069d353cedd25a96`.
Production snapshot SHA256:
`4fc742aeed5572d5f1b373394592266041ba3bcdba67444e3f75f8d915ba5ed9`.
FullRegression=false; the new default has 86 groups and was not run in full.
Last full aggregate remains 83/83 on b2203. All verification ran locally.

The chooser implementation is complete and checked in TODO. Rendered input/layout,
real two/32-player admission/full/reuse and larger-group performance remain
unperformed acceptance. The remote master's matching deployment is not confirmed
or changed by this work. README distinguishes these b2207 development controls
from prepared package 0.9.2158. No game run, release package, game installation, commit or
publication was performed.

# Development b2206 - ten sleep synchronization fixes (24 October 2026)

1. Refused host-asleep flags retain recipient-specific current-state debt rather
   than disappearing after the only edge. New sleep/wake truth supersedes old
   levels; a refused recipient retries after 100 ms instead of on every frame.
2. Refused peer-rest relays retain the same bounded debt, including wake and
   departure notifications. Transport acceptance clears only the observed revision.
3. Join/readiness seeds include every peer row, including explicit awake rows.
   Partial/refused seeds retry, repairing old sleeping mirrors without history replay.
4. Rest state, pending input and recipient delivery follow transport generation
   and readiness. Missed departure edges, reused seats and readiness loss clear
   old votes; a replacement needs its own accepted resting report.
5. Paced reports are paid only by the installed running/connected host session.
   Unready/departed reports are cleared, and full disconnect resets their token
   state and debt so an old sleep cannot reappear in the next session.
6. A valid wake/dream waiting in the pacing queue vetoes shared acceleration
   immediately. Presentation remains paced; the clock no longer trusts an older
   applied Resting value while its author's newest report says otherwise.
7. Unanimity reads the host's native sleep/dream state, refusing unreadable or
   non-finite flags. An overlapping host dream cannot count as ordinary rest.
8. Sleep receive refuses stopped/disconnected input, null payloads and nonzero
   padding. Awake-with-bed is rejected on the client receive path as on the host.
9. World generation, session serial/object and role changes clear all snapshots,
   pending reports and delivery debt. In-flight source/recipient checks stop old
   delivery; a reentrant newer revision cannot be erased by the old completion.
10. Clients publish their initial awake state and latch the exact accepted wire
    payload. A nested newer send owns completion; an interrupted/disconnected
    send cannot falsely mark the new local state as delivered.

PASS `run_sleep_personal.ps1 -FaultControls`: the actual controller, native
personal nightmare behavior and real 36-byte wire payload run with 32-seat stubs.
Delivery refusal/coalescing/backoff, awake-inclusive seed repair, generation and
readiness changes, real paced wake input, host dream, pending teardown, foreign
session payment, malformed/offline input, world/serial/role reset, reentrant
delivery and exact/nested client latches pass. Every connected seat votes, with
the final subject/recipient slot covered. The complete production TU compiles.
Twelve removed-protection controls fail their assertions: lost-host-level,
lost-peer-level, incomplete-join, stale-occupancy, foreign-debt, paced-wake,
host-dream, inactive-input, stale-world, stale-client-payload, stale-client-send
and disconnect-debt. Evidence: `build/sleep-personal-check/fault-controls-2206.log`.

PASS **7/7 selected groups**, stable-source Release/ABI b2206 / protocol 2206,
794 build source entries: `build/sleep-delivery-2206/verification.json`.
Groups: run_sleep_personal, run_session_reports, run_dream_identity,
dream_identity_native_test, insomnia_native_test, public_prose_include and
reliablekind_gate. The session-report sleep-speed controller and fresh native
dream/Insomniac contracts remain green.
DLL SHA256: `f1469bb7f1966bce051ea3ca0c84b82b85835cb1973f08b38fb9327aeb4601d0`.
Production snapshot SHA256:
`f5c08f20cc6ce85c02adaf202feb08c0a5855e0f14358fcd3dd180fde00af0a3`.
FullRegression=false; the 85-group suite was not run in full. Last full aggregate
remains 83/83 on b2203. All verification ran locally.

Rendered multiplayer clock/wake/dream and recipient replacement acceptance remains
open. Native nightmare probability/rewards stay personal, and the optional
per-mattress capacity suggestion remains deferred because the wire identifies
bed classes. No game run, package, installation, commit or publication was
performed. TODO checks the ten implementations separately from game acceptance.

# Development b2205 - ten inventory/profile fixes (24 October 2026)

1. Strict personal-store outer headers refuse negative/inconsistent counts,
   invalid pointers and malformed capacity. An unreadable inventory no longer
   looks empty to profile persistence; genuine/reserved empty arrays stay valid.
2. Personal snapshots validate equipment/hand headers before publishing any
   carried/equipment/hand output. Failed reads cannot expose a partial profile.
3. World-container reads use the same strict headers and return Unreadable on
   corruption, preserving shared contents instead of broadcasting false emptiness.
4. Restore validates existing carried/equipment/hand headers before using their
   fixed-slot shape or staging replacements. Failure preserves the live arrays.
5. Held-item replacement stages the complete record through the checked codec.
   Failed construction and changed save/world/header/slot or nested replacement
   refuse publication; existing key, tag and other saved data are preserved.
6. Native container volume/name derivation checks object/class/world, ownership,
   component binding and function lifetime between dispatches. A stale volume
   callback cannot continue into an unrelated or replaced owner's name update.
7. Save-ready join restoration binds each native step to the live client session,
   world/serial and save incarnation. Failed effects reset also keeps client-to-host
   profile reporting closed, protecting the stored profile after a partial restore.
8. The callback owns one complete pending profile revision and consumes an exact
   atomic request generation. New profiles/requests and nested joins cancel older
   work without mixing items, vitals and pose or overwriting newer completion.
9. Host profile delivery owns the recipient GUID and binds preparation and every
   chunk to source epoch/world/serial, role, connection, occupancy, GUID and send
   sequence. Disconnect callbacks fence an in-flight send immediately.
10. The sent latch records recipient generation and GUID. Silent seat reuse
    rearms delivery; refused/interrupted sends retain retry debt, and a stale
    host tick cannot continue across a world/session change into another seat.

PASS all three fault-control runners: 23 expected-failure controls (9 inventory,
7 container and 7 profile-join). Each removed protection fails its assertion while
the real implementation passes. Evidence:
`build/inventory-apply-check/fault-controls-2205.log`,
`build/container-apply-check/fault-controls-2205.log`,
`build/profile-join-check/fault-controls.log`.
The actual wrappers, record codec, extracted world read/apply/relay, profile
controller/receiver/chunker and private save-ready callback are exercised. Join
coverage changes world/session/save/request at each native stage, admits nested
newer joins, checks partial failure and tests recipient replacement between chunks.
Complete production inventory/container/save-record/profile TUs also compile.
The equipment-view fixture (run_2146) passes separately.

PASS **12/12 selected groups**, stable-source Release/ABI b2205 / protocol 2205,
794 build source entries: `build/profile-safety-2205/verification.json`.
Groups: run_profile_join, run_inventory_apply, run_container_apply,
run_container_birth, run_2169, run_2176, run_trowel, run_2147, run_sync_2157,
run_item_records, public_prose_include and reliablekind_gate.
DLL SHA256: `f03c8e6843f63e24b5d9739d679141c0d60db512e2aa4a9b2363a178869bfa77`.
Production snapshot SHA256:
`f626d68fe29a739feba03fc87847fec2cde594c6d4d72b370ecd5c85ecf7edbe`.
FullRegression=false; the new default has 85 groups and was not run in full.
Last full aggregate remains 83/83 on b2203. All verification ran locally.

Outer-header validation does not validate every nested record or allocation
extent. A newer profile cancels an old restore without automatically restarting
that consumed save-ready hook. Existing unpublished nested scratch retains the
codec pin policy. Rendered join/rejoin, held/container UI and original item-copy
reproduction remain open, as does removed-row/carrier refund accounting. No game
run, package, installation, commit or publication was performed. TODO checks all
ten implementations separately from those unperformed acceptance cases.

# Tracking reconciliation (24 October 2026)

At the owner's request, TODO now explicitly checks the six implemented b2200
report paths, earlier blast/ATV/blood code, immediate-join infrastructure and
record/material/grill safeguards. The 15 b2201–b2204 code items remain checked;
their headings now identify completed implementation. Unreproduced item-copy,
broader warp events, unfinished transactions and rendered acceptance stay open.
Continuation size changes from 3–5 to **10 concrete fixes per pass** in the active
instructions and TODO. This is a documentation reconciliation against existing
evidence; no source, DLL, test result or gameplay status changed.

# Development b2204 - five world-container fixes (24 October 2026)

1. Removed the lane's separate raw Player/offset readers. Dirty publication,
   receive/apply and diagnostics share the masked, class-incarnation wrapper.
   World addressing also refuses reserved personal index zero when Player=false.
2. Container slices stage checked records before swapping the live header. Failed
   outer/nested allocation or construction retains the old contents and parks the
   input without advancing hashes/publication. Missing classes wait before scratch
   allocation; partial nested scratch retains the existing codec pin policy.
3. Stack-owned apply scopes bind accepted work to session/role/world, sender
   occupancy, actor/class/component/save/store/header and accepted newer/local
   changes. Rejected input cannot cancel accepted work. Duplicate hashes require
   the same live component/store binding. Transfer completion follows commit,
   before native volume/name/view callbacks, preventing a committed birth from
   being merged again after an interrupted callback. Interrupted committed host
   state remains dirty for current-state publication.
4. Park sweeps copy identities first and own one callback payload at a time.
   Revisions/epochs preserve replacement/reset, nested sweeps defer, author-cap
   replacement is counted, clock rollback cannot expire a row, and newer handled
   input retires only the observed older park. Old held state cannot reappear
   after newer truth, and large join queues do not duplicate every blob at once.
5. Refused accepted-state relay becomes debt to that exact recipient generation.
   The existing seed retry sends current truth; departed/replaced seats retire
   their old debt. Relay checks source and recipient currency between chunks.

PASS `run_container_apply.ps1 -FaultControls`: extracted production receive/apply/
relay functions, actual record codec and container/inventory wrappers, real park
and both complete production TUs. Allocation/construction refusal, retry, reserved
personal address, callbacks/newer and rejected input, birth completion, binding
replacement, recipient debt and old/new park cases pass. Six controls fail when
checked encoding, current-source proof, relay debt, masked boundary, old-row
retirement or revision protection is removed.
Evidence: `build/container-apply-check/fault-controls.log`.

PASS **8/8 selected groups**, stable-source Release/ABI b2204 / protocol 2204,
794 source entries: `build/container-apply-2204/verification.json`.
Groups: run_container_apply, run_inventory_apply, run_container_birth, run_2147,
run_sync_2157, run_item_records, public_prose_include and reliablekind_gate.
DLL SHA256: `922fec45cc6bbfc2c14949c4d5cb8e7d4e3328036df3f6e36c0353dd97a70d93`.
The earlier same-build qualification was superseded after reducing park-copy
peak memory; only this final stable snapshot qualifies the current DLL.

check_current.ps1 now accepts an explicit CheckGroups array, rejects unknown
groups before building, and records FullRegression/RequestedGroups. The default
still runs all 84 groups. A focused pass is explicitly FullRegression=false;
the last full aggregate remains 83/83 on b2203. The unknown-group control passes:
`build/container-apply-check/selection-control.log`. All work ran locally.
Rendered container concurrency/UI/rejoin and the original item-copy cause remain
open, as does native removed-row/carrier refund accounting. No game run, package,
installation, commit or publication was performed.

# Development b2203 - four inventory restore fixes (24 October 2026)

1. Prepare carried, worn and held arrays before publishing any live header.
   Checked record encoding reports outer/nested allocation and class/name/string/
   signal construction failures. Fixed empty equipment/hand slots preserve the
   native shape and identity transform. Size/overflow refusals cannot empty the
   previous inventory. Legacy record writers retain their existing fallback.
2. Own the input before native dispatch, and bind staging to the target actor/
   class, world, gamemode, save binding and original array headers. Nested apply
   supersedes older staging. Changed/dead contexts return without publication.
   Unwritten scratch is freed; failed partial nested scratch follows the existing
   buffer pin policy rather than introducing a speculative deep destructor.
3. Personal and world readers share the reflected Player bit/mask. Adjacent
   flags no longer imply personal ownership. Both personal readers refuse an
   index other than the baked zero instead of logging and reading foreign data.
4. Save/container/index/owner/volume field offsets now use the actual class's
   slot/serial incarnation. Replacement classes refresh positive and negative
   old metadata; the Player mask cache refreshes with its class too.

PASS `run_inventory_apply.ps1 -FaultControls`: actual inventory/container wrappers,
record codec and extracted production InstanceOffset. Every one of the fixture's
69 allocations is failed separately, preserving all three previous arrays.
Coverage includes construction failures, nested/newer input, target/world/class/
gamemode/save/header changes, fixed slots, masked flags and exact slot isolation.
Four removed-protection controls fail: unchecked records, stale targets, foreign
personal slot and stale class layout. Fresh cooked evidence reads the player
component's index=0/player=true/customVolume=50000 and native fixed-slot lookup.
Evidence: `build/inventory-apply-check/fault-controls.log`.
The existing trowel/material persistence and equipment/view fixtures also pass.
PASS **83/83 groups**, stable Release/ABI b2203 / protocol 2203, 794 source entries:
`build/inventory-apply-2203/verification.json`.
DLL SHA256: `6e665deb20a8f591ee9f8efb7dcffc418cbc5481a3564858a22fd5dadb7e168a`.

All runners and compilation execute locally. Focused tests cover work in progress;
this full aggregate qualifies a shared-codec change across its consumers. Full
regressions are not required after every intermediate edit. Rendered join/rejoin,
the unidentified original item-copy trigger and removed-row/carrier refund remain
open. No game run, archive, installation, commit or publication was performed.

# Development b2202 - three arirShipAppear fixes (24 October 2026)

1. Host visible/alpha phase reaches ready clients on a dedicated host-only lane.
   The adapter paints native material/light/radar and event-sender membership on
   existing scene actors. It stops old client timelines, which write their float
   track even when the Blueprint Update callback is refused. Indexed exact-class
   discovery and name/position corroboration avoid generic event-actor spawning.
2. Clients refuse their own appearance, camera/day disappearance and blank-email
   function/latent entries, including chains started before joining. An exact
   actor/epoch scope admits native photo-delegate bind/unbind while appear stays
   blocked; native local photography runs only against an applied visible phase.
   The host retains native timing, eligibility and mail generation. No host photo
   reward is replayed by the adapter; shared reputation remains separate work.
3. Current-state joins are quiet, with ordered incarnations/retirement, bounded
   fair send retries, recipient generations/replay tokens and retained partial
   application. Live cosmetic air cues have one-second retries from receipt.
   Actor/component/gamemode, world/session/role and callback guards cancel stale
   work. Whole discovery batches are slot/serial stamped before native reads;
   newer incarnations during a read cannot inherit the old actor association.

PASS `run_ariral_ship.ps1 -FaultControls`: production controller and real
wrapper/ParamFrame tests, four removed-protection controls, fresh cooked native
contract and both production TUs. Controls reject local latent continuations,
stale phase overwrite, lost refused sends and historical join cues.
Evidence: `build/ariral-ship-check/fault-controls.log`.
PASS **82/82 groups**, stable Release/ABI b2202 / protocol 2202, 794 build inputs:
`build/ariral-ship-2202/verification.json`.
DLL SHA256: `4b8e918b1783b20fc631038ad85f801a66f515a9560ea6543021f716a2e430f5`.
The first aggregate was stopped before its build to add discovery-batch and
reentrant-incarnation guards. Only the subsequent stable aggregate qualifies b2202.

This three-item pass covers `arirShipAppear_C`, whose native blank-email loop
matches the screenshot clue. Original report (6.1) still needs rendered host/client
acceptance. `arirShipVanishing_C`, other warp events, broader event completeness
and shared reputation remain open. The unidentified item-copy report also stays
open. The game disk remains unavailable; no archive, installation, commit or
publication was performed.

# Development b2201 - three container-birth fixes (24 October 2026)

1. Replaced the process-global takeObj flag/ProcessEvent observers with the script
   gate's native-body scope and a named body watch. Nested native calls cannot
   clear an outer extraction, and cancellation/fault paths need no balancing POST.
   The consumer reads its own thread's scope; Blueprint-internal calls are covered.
2. Successful extraction results require current UFunction metadata, checked frame
   bounds and the true caller out storage. Recreated functions refresh both offsets;
   failed calls, truncated layouts and stale/recycled actors are refused.
3. A bounded rotating queue retains birth and mutable-state delivery separately.
   No state is published before the spawn is accepted, and a refused state does
   not repeat that spawn. Final live key/EID and callback/lifetime guards bind each
   attempt to its actor/world/session/role. Generic host finish work waits for this
   owner rather than publishing a record around the refused birth.

PASS `run_container_birth.ps1 -FaultControls`: production consumer/feeder tests,
four removed-protection controls and the fresh cooked native takeObj/extract
contract. Controls reject leaked scope, stale metadata, lost refused spawns and
repeated spawns during a state retry. Three changed production TUs compile.
Evidence: `build/container-birth-check/fault-controls.log`.
PASS **81/81 groups**, stable Release/ABI b2201 / protocol 2201, 792 build inputs:
`build/container-birth-2201/verification.json`.
DLL SHA256: `38e98a8bbd7981f7a576749d95b83e92555b6a21b273d38dc7765957e7ee67f6`.

This is a three-item continuation, per the owner's new workflow preference,
recorded in agent/README. It does not close the unidentified original client
item-copy reproduction, warp-arrow presentation, or the separate inventory
removed-row/carrier-restore/refund transaction. Real-peer acceptance remains open;
no package archive, game installation, commit or publication was requested.

# Development b2200 - 24 October report batch

Implemented paths for this batch's reports (1), (3), (4), (5), (6.2) and (7):
transient shovel-hole identity/soil/geometry and lifetime, pending canonical can
state before mutation plus no-refill/reclose/type rollback, physical satellite
console claims, unpublished companion-ATV retry recovery, location activation
without forced gathering, and unanimous host sleep acceleration.

Hole mirrors wait for a live BeginPlay suppression watch; native BeginPlay
would search buried objects and spawn treasure again. The adapter does not add
persistent hole saves or close buried-item/loot accounting. Can initial births
retain their saved portions; cross-peer concurrent-use accounting and other
consumable families remain separate work. The bed-capacity suggestion is deferred.

PASS targeted production regressions: `run_session_reports`, `run_sleep_personal`,
`run_2174`, `run_item_records`, `run_garden_sync` and `run_2161`. Coverage includes
actual console classifiers, ATV and sleep controllers, hole authors/ACK/late join/
retirement/duplicate creation/watch readiness, and ordinary stale versus initial
can records. Fresh cooked contracts verify the hole factory/material/loot path,
console `used` owner before entry, and non-refillable native can semantics.
PASS **80/80 groups**, stable Release/ABI b2200 / protocol 2200, 792 build inputs:
`build/session-reports-2200/verification.json`.
DLL SHA256: `54bc6b86735f11c9d236e765c4774b2b5ae7618bb276e2653b1c122f547413b2`.
The first aggregate was intentionally interrupted to add the mirror-watch
readiness guard; only the subsequent stable run may qualify b2200.

The user could not identify the report build or item-copy trigger. Screenshot
retrieval succeeded through stable GitHub attachment URLs after the signed-link
viewer failed. The item images show a shovel and noodle packaging, without
class/key/EID or transition evidence, so report (2) stays open. The warp-event
image shows repeated blank emails; the native `arirShipAppear_C` graph's delayed
blank-email loop makes it a strong candidate. Its alpha/light timeline, local
camera/trace checks, photo reputation and email side effects require a specific
presentation route; report (6.1) is not fixed by this batch.

Rendered host/client acceptance remains open; the owner previously confirmed
that the game disk is absent. No archive, installation, commit or publication
was performed by this work.

# Development b2199 - server delivery and personal dreams (24 October 2026)

Serverbox per-recipient delivery debt survives refused/partial sends and occupant changes.
Clients retain latest state and unfinished re-skin debt through native readiness/failure;
per-box incarnations and callback/lifetime guards cancel stale work. Old-code regressions fail.
Dream-burger overlaps reject remote puppets/ragdolls while preserving local/native collection.
Function offsets bind to UFunction incarnations and checked parameter-frame bounds.
PASS production regressions/compilation and three dream removal controls:
`build/events-2199-final/dream-controls.log`, `build/requested-events-audit/serverbox-*.log`.
Fresh native dream reward and protected-server contracts pass; serverBreaker stays excluded.
Fresh audit: 42,941 pak entries, 37,971 extracted uasset/uexp files, 18,854 assets;
1,621 relevant Blueprint packages / 22,691 functions / 69 scheduled-event rows.
Zero package errors; 190 focused dumps: `build/requested-events-audit/summary.json`.
PASS imported native-contract suite: `build/requested-events-qualified/fresh-native-contract.log`.
The raw inventory's two graph rejections were hint-text false positives. PASS all 22,691
raw decodes, zero actual errors: `build/requested-events-audit/reclassified-graphs.json`.
Level/map scripts and complete semantic event coverage remain outside this result.
PASS **78/78 groups**, stable Release/ABI: `build/events-2199-final/verification.json`.
DLL SHA256: `a376716c7f9fd6bfce562ef983257db9abd95d04a2239644e72e5e94e7adcdd3`.
The earlier `events-2199-qualified` run rejected source changes during the re-skin fix.
Fact rows 515/165 and nine controls pass; prose findings: 85/82 documents,
src.comment_doc_row 155/0, src.files_not_swept 84/0 with reading-room checkouts absent
(previously 1/0 and 1/0 with them present). No new package or installation.
Runtime acceptance remains open: owner confirms the game disk is absent on 24 October.

# Development b2198 - event frequency and queued credit (23–24 October 2026)

Queued credits cancel after world replacement; the old-code assertion reproduces the defect.
Host-owned flickerer scales only its native 30–60s eligible wait, preserving probability/effects.
PASS production/native/captured-reference regressions, **74/74 groups**, stable Release/ABI:
`build/requested-events-qualified/verification.json`; sequential `final-build.log` supersedes
the aggregate's concurrent-Ninja failure in `build.log`. No failed build counted as a pass.
DLL SHA256: `0b9ea938eaa683e18d968d66898af25f687c4bb0d3f359a1c492958c85bc4d42`.
Two isolated native launches overflowed before acceptance. Dumps prove recursive native
lib_C.end; Untitled_1.CustomEvent's borders check is the strongest initial-caller candidate.
Exact caller/failed check still needs live confirmation; no native guard was suppressed.
Evidence: `build/requested-events-native/crash-analysis-20261024.md` and retained crashes.
Bench58 placement, Token52 orders, trowel continuity and real-peer events remain unverified.

# Development b2197 - full-record carriers (23 October 2026)

Pre-fix: full restore ignores native invalidation; failed restoration still allows pickup.
PASS guarded explicit-function capture/restore, saved key/full state and extraction owner/frame binding.
PASS 15 codec/container/pickup controls: `build/container-lifetime-qualified/` control logs.
PASS **74/74 groups**, stable Release/ABI: `verification.json` in that directory.
DLL SHA256: `b1dc33c642e5136c36b1f35f30d823b166a3facd57701cbac0be9214ee0d34a2`.
Fact rows 513/165 and nine controls pass; three inherited prose counters remain.
Existing cooked corpus reused; fresh asset audit remains deferred.
Failed-row retry/refund and current-DLL gameplay remain open; no package/install.

# Development b2196 - guarded record dispatch (23 October 2026)

Pre-fix native capture rekeys an actor but old loadData still dispatches: `pre-fix.log`.
PASS production capture/apply/convention/context guards and receiver revision forwarding.
PASS seven codec/eleven receiver controls: `build/record-dispatch-qualified/` control logs.
Superseded run stopped for lineage ordering; scalar-fixture link mocks updated.
PASS **73/73 groups**, stable Release/ABI: `verification.json` in that directory.
DLL SHA256: `6d800135d85644e6a325549659213befb9a22d456249528d76f95620cda223b6`.
Fact rows 513/165 and nine controls pass; three inherited prose counters remain.
Current-DLL gameplay, writer/accounting and new package/install remain open.

# Development b2194–b2195 - initial author state (23 October 2026)

b2195 pre-fix: an empty park bypasses the author wait. PASS capture/matching drain,
new native wait, pickup/output reset and ten controls; stable Release/ABI, **72/72 groups**.
Evidence: `build/capture-wait-qualified/verification.json`, `pre-fix-pickup.log`, `fault-controls.log`.
b2195 DLL SHA256: `2bd5e745df8e5b638abf0f09cda6ec6d168ceec5bdecd9cf82bb8d1b18a2c0bc`.
b2194 pre-fix: foreign claims clear the wait. PASS author/application/early-state
and repeated-birth refusal/retry, placement and eight controls; stable Release/ABI, **71/71 groups**.
Evidence: `build/initial-record-qualified/verification.json` and `fault-controls.log`.
b2194 DLL SHA256: `d5582405d608be4fa5a6d04b93f3b80a6e011d7b0e7adce895682a04647d89dc`.
Writer/accounting and rendered acceptance stay open; no package/install.

# Development b2193 - material publication revisions (23 October 2026)

Pre-fix stale water publication includes native stock changes that skip observation.
PASS exact-stock/material/hand and host/client capture/send fences, caller retries,
compilation and four controls: `build/material-publish-qualified/fault-controls.log`, `stock-control.log`.
PASS 71/71 stable Release/ABI: `build/material-publish-qualified/verification.json`.
The superseded pre-correction run is not qualification evidence.
DLL SHA256: `c11af21a8ac6868721222b2d93c8b4a1fc29271eba3c271c242e954176b50208`.
Delivered records cannot be recalled; arbitration, debit/refunds and rendered acceptance remain open.

# Development b2191–b2192 - grill lifetime (23 October 2026)

Pre-fix regressions fail on stale coal after session restart and false admission
of a replaced hand row. PASS ordered/hand restart fences, revision/fan-out/debt
guards, native-read retirement and audio cleanup; two/seven fault controls pass.
Each DLL passes 71/71 groups and stable Release/ABI; evidence and fault logs:
b2191: `build/grill-session-qualified/verification.json`.
DLL SHA256: `8fc98843693b4b4332c96d1a7e3f3a4aaea834611ee9f61ee2ed6194bacfe2de`.
b2192: `build/grill-hand-qualified/verification.json`.
DLL SHA256: `a767674fcfa8e989ec4736c23f406d5c2990ec56ad602091f844b7643638ea43`.
Fact rows 513/165 pass; three inherited prose counters remain. Personal debit/refunds
and rendered acceptance stay open; native payloads unchanged, no package/install.

# Development b2190 - zero-damage explosion motion (23 October 2026)

Accepted host blasts are tracked independently of damage; the native impulse's
sphere/object-type query discovers targets before physics integrates positions.
PASS production feeder/wrapper/native contract and five expected-failure controls.
PASS 71/71 groups, stable Release/ABI: `build/force-only-qualified/verification.json`.
DLL SHA256: `60f082e28962f60ab58057c7d0ec86c91b2b86311e6e176fe244970e6f3b899f`.
PASS native LAN damage 0 / force 5000: bucket moves 541.02 cm, reliable rest edge,
live/rejoin agree within 0.1 cm: `build/force-only-qualified/native.json`.
Both compatibility reports HEALTHY; native query MATCH; fact rows 513/165.
Historical b2189 qualification: `build/explosion-motion-qualified/verification.json`;
ordinary-blast evidence: `build/explosion-motion-qualified/native.json`.
Rendered/input/all-blast acceptance stays open; existing corpus reused, broader audit deferred.
Package 2158 stays b2186; no archive, profile installation or publication.

# Development b2188 - ATV steering and join honk (23 October 2026)

ATV handle/wheel steering is replicated; join snapshots retain pending live honks.
PASS 70/70, Release/ABI: `build/atv-steering-qualified/verification.json`.
PASS native LAN 6/6 + nonzero late join: `build/atv-steering-qualified/native.json`.
Two controls, 511/163 fact rows/nine controls and 225 routing kinds/18 controls pass.
Native field stimulus does not close rendered/keyboard acceptance; package stays b2186.

# Development b2187 - player bleeding (23 October 2026)

Owner blood-loss drops are host-validated/forwarded without echo; damage/blur stay local.
PASS 69/69, Release/ABI: `build/bleeding-qualified/verification.json`.
DLL SHA256: `b6cd023baaba1d2807a733ecd8114066c6cb72d2d052c97ecfe87578c1229a56`.
Native contract: [blood capture](tests/blood_capture_0.9.0n.txt), two live functions.
HEALTH PASS/reference MATCH/zero census problems: `build/bleeding-qualified/native.json`.
PASS fact rows 509/161/nine controls and routing 225/18; inherited destroy-watch errors remain.
Rendered bleeding and stain/join/cleaning/save parity stay open; package stays b2186.

# Package 0.9.2158 test build

PASS 68/68 stable Release/ABI b2186 / protocol 2186, 786 entries: `build/release-2158/verification.json`.
DLL SHA256: `d57443e02bf588ec1f3fe812af1acce0f9e3b93f564bf562645ed5539b55535a`.
PASS label 0.9.2158, 32-member archive, source/DLL hashes, manifest/icon/models,
activation/disclaimers and matching export; four rejections and DLL-only routing:
`build/package-current/verification.json` and `rejection-controls.json`.
ZIP SHA256: `05e67388365d16902f283aef7a9c962b942ca844eb9d5fb2bc590702ab9d6929`.
Native gameplay/archive startup stayed unverified; qualification is explicitly offline.
About/README/release/changelog publication: `565edebe2e86b357b24cb26681102967c38bd36a`.
The branch fast-forward left implementation changes uncommitted; no binary/upload/master deployment.

# Selected upstream qualification

PASS stable Release/ABI 0.9.0n b2185 / protocol 2185, 68/68 groups and 786 entries:
`build/upstream-qualified/verification.json`.
DLL SHA256: `321c13ced4a507f862ef2e343d338684e8cca9abc013fad2605fba796d807c51`.
PASS vital encoding, delayed loading, worker isolation, NPC rotations, dedup and
write/landing retries; fact rows 508/160. Prose counters 84/82 documents, 26/24 naming,
155/0 citations, 84/0 unswept; baseline unchanged: `build/upstream-qualified/prose-import-counters.json`,
`prose-head-counters.json`. Rendered gameplay stayed unverified; no package/deployment.

# Development b2185 qualification ? 22 October 2026

Rendered join reports remain open: [SYNC_2157_FIXES.md](SYNC_2157_FIXES.md).
PASS 67/67 stable Release/ABI, 785 entries, protocol 2185 / game 0.9.0n:
`build/sync-2157-qualified/verification.json`; DLL: `build/votv-coop-2154/main.dll`.
PASS 1100 watches/eight reopens, exact-recipient seeds/readiness, occupancy/reentry,
2–32 seats/slot-31 and flashlight setter/connect/world retries. Assertions stay enabled.
CMake exit status is checked; stopped seed-correction run is not final evidence.
DLL SHA256: `5b5b5a4a745f11c1e2dc31840f886b7188569a6faa77a94ef81451aaa8109207`.
Production snapshot SHA256: `9bc95f5f4c6c29e80604ccae7f23916b1ef80671f918513e668ecca558932908`.
Rendered/32-player/Rust/deployment/archive checks stayed open; update the old master
for larger automatic lobbies and use identical b2185 DLLs.
Fork review: [FORK_AUDIT_20261022.md](FORK_AUDIT_20261022.md).

## Historical release 0.9.2157 / b2184 - package-label correction, 22 October 2026

The menu and server browser now show Watchers of the Void 0.9.2157 from
package/release.json. CMake regenerates the label when that metadata changes;
game target 0.9.0n and wire protocol 2184 remain separate compatibility values.
The packer requires matching runtime formatter evidence and verifies the label
again in the freshly unpacked installation's startup log.

PASS Release build, actual shared DisplayVersion formatter at startup, ABI drill,
one impaired LAN run per direction (Scientist and Kerfur, selected articulated
mesh, entry and recovery; 250ms lag,40ms jitter,5percent send loss), and isolated
startup of the full 32-member ZIP. Production/build inputs frozen:2646.
Evidence: build/version-2157-check/verification.json and final logs;
build/package-current/verification.json with archiveStartupVerified=true.
DLL SHA256:fa77a7c20c685840579e452191bc7f5f098152a0aa5f9dc961097392161f2e87
ZIP SHA256:ab9f59c44bf8cc3dc4f2761559c222a1a9e4a4ce6f35cc621d4417bb75e525f8

The gameplay, ragdoll physics and wire implementation are unchanged from the
preceding qualification below. Its complete physics/model/camera matrix was
not rerun on this label-only rebuild. Tests use NullRHI; native widget text
readback and an ordinary rendered/input check are not claimed. The full65 suite,
existing16 fact-row findings and script-name watch capacity remain open.
Entity, original game files and the normal installed profile are untouched.
The same 0.9.2157 filename replaces the previous ZIP; authorship is AquaFishhh.


## Preceding ragdoll qualification 0.9.2157 / b2184 - 22 October 2026

This supersedes the b2183 status below; package number and filename stay unchanged.
Selected-character physics now starts in the local native FinishSpawningActor
post-hook, before camera attachment and native impulses, without the polling pause.
At native recovery attachment, preserve the camera world pose and hand current yaw
to the capsule/controller once, preventing the late return to the pre-fall heading.
Native fallImpulse, physics and get-up animation remain the owners of movement.

PASS final exact-DLL native entry/recovery8/8 (instant selected simulation,
completed wakeup, no delayed yaw drift and subsequent turning), momentum8/8,
camera10/10,blocking dynamic-platform contact24/24,installed models30/30 and
remote sleep/sit/ragdoll transitions36/36. Impaired LAN3/3 per direction passes
with250ms lag,40ms jitter and5percent send loss; production construction hook
fires on all six entries without a manual watcher setup in the peer driver.

PASS focused native/skin/retry/stream, shared-screen C++ and personal-sleep
regressions; four native correction-removal controls, stream old-behavior controls,
ABI drill and18 reliable-routing negative controls. Both new camera dependencies
match their live-captured references; the fact-row gate retains16 baseline
violations and gains none. No new complete65-group pass.
The legacy external toolchain/private laptop corpus is unavailable here.
Production/build inputs frozen:2645; tested DLLs identical.
Evidence: build/ragdoll-transition-check/verification.json and final logs.

Full32-member ElvardFIC-WatchersOfTheVoid-0.9.2157.zip passes original branding,
models, loader layout, byte comparisons and actual isolated unpacked startup.
DLL SHA256:7bf4a68037a384605ae54fd9c8939564ba531f274d9b94fb6fc5f99617f647d2
ZIP SHA256:cf0c122bb787876eb62e13a01f4896bc94468622460f4c559e4a89405f30955a
Package proof: build/package-current/verification.json; archiveStartupVerified=true.

The first impulse observer missed the first spawn's watcher registration; fix the
acceptance setup before the native call. An artificial movable platform also
retained StaticMeshActor's WorldStatic channel, triggering native camera wall
travel on recovery. The final fixture explicitly verifies WorldDynamic and keeps
all physical responses blocking. No native wall-detection suppression was added.
The surface driver now uses ordinary C-key recovery through completed native
animation, rather than ForceWakeup, whose immediate reset could snap a sliding
head across scenery. A world change aborts the matrix instead of posting stale
pawn tasks. Failed intermediate runs remain in build/ragdoll-transition-check/; they are not
release evidence. Static terrain beyond the tested native ground remains open.

Native test logs also expose the existing 768-slot script-name watch capacity
being exhausted during world startup. Keep that registration-capacity issue open;
this change does not enlarge the registry or claim the other refused lanes fixed.

Entity and the normal installed profile are untouched. Owner rendered/input
acceptance, other terrain/vehicles, native bed/chair and uninstalled rigs remain
open. Branch updates retain AquaFishhh authorship and package0.9.2157.

## Historical release 0.9.2157 / b2183 — 22 October 2026

This status supersedes older package/build statements below. Existing open gameplay
and implementation tasks remain open unless explicitly accepted.

Integrated main 2e104004b with the selected-character native ragdoll changes.
Preserved upstream computer leases/desktop edits, balance retries, event cues,
container births and lifetime guards. DLL/protocol2183 includes both wire changes.
Public package version is 0.9.2157 at the owner's request; 2158/2159 below are
historical local iterations, not the current delivery.

PASS Release and focused native/skin/retry/stream, shared-screen C++/sleep,
events, balance, placement, trowel, water/material and personal-sleep groups;
ABI and reliable routing with all18 negative controls. The legacy full runner
cannot complete with the absent external toolchain/private laptop corpus.
No new complete65-group result is claimed.

PASS actual NullRHI game momentum8/8,camera10/10,surfaces24/24,catalog30/30,
remote sleep/sit/ragdoll transitions36/36 and impaired LAN3/3 per direction
(250ms outbound lag,40ms jitter,5percent send loss on both peers).
Frozen production/build inputs:2644; test DLLs identical.
Evidence: build/release-2157-check/verification.json and final logs.

Full32-member ElvardFIC-WatchersOfTheVoid-0.9.2157.zip passes package identity,
original icon/models/authors/loader checks and actual unpacked startup.
DLL SHA256:c4237cc74aed8932873bf1a6f6c7bb35d47a8e4566b1278135641ef9f2c78f9a
ZIP SHA256:dde82f9435b31896cef5fc43b4b385dbca471b62194781b4e115aad1c5c5d9b7
Package proof: build/package-current/verification.json; archiveStartupVerified=true.

The first integration candidate included a superseded pre-mesh collision/all-body
shutdown. Offline setup checks passed, but actual momentum fell to6/8 and camera
entries rose over50m. Keep that rejected run in integration-first-failed.log.
The final build retains the previously qualified staging order: pause component
simulation, place/rebuild the selected mesh, then stop its new per-body simulation
until warmup finishes. Both the ordering regression and all native matrices pass.

Normal installation/profile and Entity remain unchanged by this release work.
Owner handles rendered/input acceptance, especially local bed/chair interaction;
uninstalled rigs, other terrain/vehicles and more extreme outages remain open.
Both requested branches are intended to carry this exact release tree, preserving
prior main/fish history and AquaFishhh commit authorship.

## Ragdoll robustness b2182 / package0.9.2159 — 22 October 2026

Reproduced wall-clock body starvation at 75ms and loss of distinct same-tick bone packets.
PASS actual-send rotation/bounded merge for 1/5/17/35/64 bodies, loss/burst/reorder/wrap/reset;
both old-behavior controls fail: stream-final.log, ragdoll-stream-check/legacy-{time,inbox}.log.
PASS native/skin/retry/stream, sleep/display, shared UI/wire, ABI and routing224/18 controls.
Full65 suite not rerun. Candidate LAN6/6, surfaces24/24 and remote transitions6/6 passed.
Initial raw-snap fixtures left native lastLoc stale, legitimately triggering backrooms travel
(mainGamemode7160/7183). Native teleportWObackrooms934..1075 and Block2/Movable2 correct
the fixture; gameplay rules unchanged. Retained surface-{initial,registered,no-mirror,travel,
backrooms}.log cleanup faults and catalog-fixture-interrupted.log are not passing evidence.
PASS final DLL `b35471ad15d0f417b26439f51c9b64e2f034987de7d2e51e3ba031ea54cbb5a1`:
momentum8/8, camera10/10, surfaces24/24, installed catalog30/30, transitions36/36, LAN6/6.
Frozen2644 inputs/three matching DLLs; GNS250ms lag,40ms jitter,5% loss after world-ready.
Six skins sci_v1sc/walter_v1sc/rvi_scientist_v1sc/luther_v1sc/KerfurH/M use real colliders,
flat/25deg/35deg/moving surfaces, selected body/mesh, finite motion and native recovery.
Moving platform100cm/s traveled175cm; max camera entry jump0.069deg; final logs free of travel.
PASS historical2159 archive32 members, identity/icon/authors/models and unpacked startup:
b2182, HEALTH, skins4/4, fork namespace, archiveStartupVerified=true.
ZIP SHA256: `bb85eaff437b031ed101c88702d8b5c8ea676bb258d77e9a45b845aaea49e42b`.
Evidence: build/ragdoll-robustness-check/{verification.json,*-final.log,source-before.json,
catalog-names.json}, build/package-current/{verification.json,startup.log,UE4SS-startup.log}.
No extra models installed or normal profile/publication changed; private processes stopped.
Ordinary rendered/input and local bed/chair acceptance remains with the owner.

## Native entry momentum b2181 — development, 22 October 2026

Reproduced lost running motion in the actual engine substrate fixture before the
fix. Native fallImpulse resumes after Delay(0), adding saved fallVeloc to every
bone. Preserve its commands in order during physics rebuilding, restore the old
body motion once, and initialize newly added limbs with the original translation.
Late commands run normally; no invented movement kick or replacement solver.

PASS focused attachment/entry/camera/momentum, selected physics/joint templates,
remote lifecycle, articulated display, wire, sleep, shared screen, reliable routing
and ABI checks. Removing the actual velocity hold makes its regression fail;
removing camera correction also fails its control. Routing rejects all 18 controls.
Logs: build/ragdoll-momentum-check/*-final.log. Full 65-group suite was not rerun.

Native DLL 6116e6755237f01f9d227caa9335dfe6167780e756efd1f01d8c8a7868ebd293:
PASS 8/8 controlled movement cases on sci_v1sc and kerfur_omega_h (walk 300,
run 600, diagonal, airborne), with recovery. Native camera PASS 10/10, entry view
change below 0.1 degrees. An optional held-input probe yielded zero movement;
rendered input-path acceptance remains open. Initial recovery failures were a
test setup problem: forceGetUp waits and gates recovery, forceWakeup is immediate.
Retain the failed/intermediate logs; do not count them as successful cases.

LAN on that DLL: entry/recovery observed both ways, selected model correct, but
articulated display absent in the acceptance probe (PARTIAL, streamed=0). A prior
8-second game-thread wait aborted readiness while loading; the harness now respects
its overall bounded readiness deadline. That fix let the drive sequence execute.
Final DLL 3d77b13017ed02d4353ec19a0b7de47035663ab7ca041ea9ed8924c67131d3e2
adds rig/body/batch counts to existing limited emissions and remote rig-change logs.
PASS exact final DLL: momentum8/8, camera10/10 (max view jump0.056deg, max rise60.9cm
over entry centre in crouch), bidirectional LAN selected articulated display and
recovery. Source snapshot2642 entries frozen, all three DLL copies identical.
ABI/native/remote fixtures rerun after the logging changes. Qualification evidence
build/ragdoll-momentum-check/verification.json. The previous partial LAN is not
explained by a behavior change and remains an intermittent acceptance limitation.
PASS complete ElvardFIC-WatchersOfTheVoid-0.9.2158.zip (32 members) and actual
unpacked startup: correct b2181 boot, HEALTH PASS, starter skins4/4 and enabled
fork namespace. build/package-current/verification.json archiveStartupVerified=true.
ZIP SHA256 ba8277a78e51b03ac2cd9dd47707e36f2ed641525b1186a542f7d03d9f14b269.
All private game instances stopped; git diff --check clean. Original installation,
normal profile and Entity code remain unchanged.

## Ragdoll entry camera b2180 / package 0.9.2157 (22 October 2026)

Owner confirms the launch fix and reports the camera looking upward on entry.
Before correction, native game measurements changed from a level view to
89.75 degrees upward at the selected-mesh swap. The inherited camera arm still
had the native plushie's fixed head-relative rotation.

Capture the controller's entry rotation, rebase the head-attached arm after the
swap and through pose warmup/physics initialization, then stop camera writes.
Retain normal physical head following and native recovery. Cached object serials
and actual attach-parent reads reject recycled/reparented camera references.

PASS focused native attachment/entry/camera, selected rig/joint-template, remote
lifecycle/bone batches, articulated display and wire regressions. The camera
regression rejects an actual-substrate mutant with correction disabled. Release
ABI and reliable-kind gates pass with their negative controls (18 routing drills).

PASS ten native 0.9.0n transform/physics cases: sci_v1sc and kerfur_omega_h, each
with level, down/wrapped-yaw, up, crouched and faint entries, with recovery between
entries. Mesh-replacement view differences stay below 0.1 degrees; selected
bodies remain attached and physically move without sky launch. In the NullRHI test,
synthetic pitch resets between frames, so angled stimuli arrange native controller and
camera-arm transforms in the same posted task as ragdollMode. These checks do not
claim rendered-image acceptance or verification of the real input path.

The first LAN attempt completed client-to-host but its old screenshot-framing
teleport moved the reverse observer toward an unconverged puppet position. It
fell/died and disconnected before the host drove. Evidence is retained in
build/ragdoll-camera-check/peer-framing-{host,client}.log. The headless test now
opts out of screenshot teleports/view writes; normal gameplay is unaffected.

PASS final identical b2180 DLLs in both native LAN directions: scientist client
to host and Kerfur host to client, selected articulated display and entry/recovery.
PASS complete ElvardFIC-WatchersOfTheVoid-0.9.2157.zip package verification and
actual startup from the unpacked archive in an isolated manager layout, dev off.
Original fork identity/icon/models and ElvardFIC/AquaFishhh credits are retained.
Normal game/profile files were not modified. No online release was published.

Evidence: build/ragdoll-camera-check/{verification.json,source-before.json,
camera-final.log,peer-host-final.log,peer-client-final.log,native-final.log};
build/package-current/{verification.json,startup.log,UE4SS-startup.log}.
The full 65-group suite remains specific to b2178; it was not repeated for b2180.
Terrain, moving props, high latency, arbitrary imported rigs, sleep/chair transitions
and visual camera acceptance remain in the broader gameplay checklist.

## Corrected full manager package 0.9.2156 / b2179 (22 October 2026)

The previous ragdoll ZIP was a manual DLL replacement, not an installable fork
package. The owner's actual r2modman profile confirms it was inferred as
WatchersOfTheVoid-ragdoll, author WatchersOfTheVoid, version 2179.0.0. Its root
manifest/icon and loader enabled marker were absent; UE4SS did not start it.
The ragdoll code had been tested, but that delivery had not passed manager
installation verification. This packaging failure supersedes the earlier
delivery claim; the player-ragdoll qualification below still applies to its DLL.

Prepared the complete **ElvardFIC-WatchersOfTheVoid-0.9.2156.zip**, retaining the
existing fork name/namespace and original supplied 256x256 icon. README credits
ElvardFIC and co-maintainer AquaFishhh, preserving original Multivoid credits.
The root manifest declares version 0.9.2156 and unreal_shimloader 1.1.7. The
archive contains mod/enabled.txt, mod/dlls/main.dll, starter models/previews and
legal notices, using the original mod/ and pak/ manager routes.

PASS package/package_current.ps1: all 32 entries checked against current inputs,
unchanged qualified b2179 DLL, production source hashes, metadata, icon dimensions
and loader tree. The existing manager-routing predicate rejects the prior broken
archive and accepts the complete one. The author's namespace comes from the
canonical Author-Name-Version filename; the actual r2modman import source was
checked, as were the installed VotV schema's shimloader route rules.

PASS actual game startup from the newly unpacked archive in an isolated manager
layout, with dev mode disabled and default loader settings. UE4SS reports
ElvardFIC-WatchersOfTheVoid's enabled.txt and starts the module. The module reports
Watchers of the Void by ElvardFIC, game 0.9.0n, build b2179, correct namespaced
data path and HEALTH: PASS; the installed starter pak supplies all four skins.
The user's Default profile was inspected without modification.

The DLL is byte-identical to the one which passed the bidirectional ragdoll game
tests. No gameplay source changed for this delivery correction. Evidence:
build/package-current/{verification.json,startup.log,UE4SS-startup.log}.
Package identity has one source in package/release.json. The previous manual-only
delivery is retained under build/package-current/ as a rejected test control.

## Player ragdoll hotfix b2179 (21 October 2026)

Owner report: launch into the sky on entry; standing remote puppet followed by
a recovery teleport; selected character required instead of the native plushie.

Release built with MSVC x64 against the actual 0.9.0n game. The clean dependency
build exposed a WebRTC C++14 / Abseil C++20 link mismatch; the two ICE targets
now use the project's C++20 standard. Final incremental build reports no work.
This is a focused hotfix qualification, not another 65/65 b2178 full-suite run.

Reproduced before repair:

- Original game log repeatedly read an unknown ReturnValue from the void Actor
  attachment call, then destroyed the remote body. The actual-ParamFrame native
  regression failed on this behavior.
- Native entry regression failed because the selected mesh was lowered to the
  plushie's pelvis, putting the full-size feet below the floor.
- Isolated live Kerfur still rose 18–35 metres after the initial geometry fixes.
  Native asset audit found the full rig's mixed-scale chest pivot; the measured
  joint-anchor gap was 1069.2 cm. Entry velocity was zero before simulation.

PASS focused checks:

- run_ragdoll_native.ps1 -CompileSources: actual ParamFrame attachment contract,
  pelvis parent verification, recycled actor/body/mesh, missing parameters,
  selected geometry retained, one staged configuration and recovery; three
  production translation units compile.
- run_ragdoll_skin.ps1: selected mesh/materials, compatible common/custom rig,
  bodies/joints/pelvis, transform before physics initialization, no player AnimBP,
  stale objects and failures. Separated anchors are repaired while instantiating
  bodies and shared NPC templates restored on success, dispatch failure and a
  later joint-read failure. Foreign authored rigs are not retargeted.
- run_ragdoll_retry.ps1: actual remote lifecycle, drift, complete bone batches,
  parent ordering, rig replacement, malformed poses and retry/recovery.
- sleep_display_test.cpp: model/material/scale, articulated pose and sleep
  transitions. shared_screen_wire_test.cpp: actual protocol and bone validation.
- abi_gate.py --drill: no UE4SS imports; injected forbidden import rejected.
  reliablekind_gate.py --drill: 224 kinds and all 18 negative controls pass.
- Fault control with joint-anchor repair deliberately removed fails the native
  skin test. Original attachment and geometry failures are retained as controls.

Actual game evidence uses isolated executables/profiles and native PhysX under
NullRHI. Normal game/profile files were not modified. The loading curtain now
uses a steady clock: its former game-thread ImGui::GetTime call faulted without
a renderer and prevented the test client's join from completing.

PASS Kerfur local entry/fall/rest/recovery: 64 samples, maximum pelvis height
22.5 cm above the pre-entry actor centre; selected model, remote mirror model,
pelvis attachment and recovery all verified. PASS scientist sci_v1sc with the
final physics implementation: 52 samples, the same 22.5 cm maximum and all flags
true. Earlier broken builds rose thousands of centimetres in the same probe.

PASS real LAN in both directions: sci_v1sc client to host, then kerfur_omega_h
host to client. Both observers verify the selected mesh and complete articulated
network display; rising and falling edges observed, no observer death.
Final bidirectional run and artifact hashes are recorded in
build/ragdoll-evidence/verification.json. Its logs distinguish the reverse round.

Evidence: build/ragdoll-evidence/, build/ragdoll-fault-controls/ and the actual
native harnesses in agent/tests/. Remaining gameplay acceptance includes arbitrary
imported rigs, slopes/moving surfaces, high latency and sleep/chair transitions.
Entity implementation and the shared native physics asset defaults are unchanged.

## Recheck: PC boot, C-key ragdoll and resource identity (21 October 2026)

PASS repeated ragdoll_skin/shared_screen/trowel and **65/65 groups**, stable Release/ABI,
781 inputs: `build/continued-20261021/verification.json`; snapshot/DLL match qualified b2179.
PC loading and all-role C-key reports still need real-peer acceptance on matching DLLs.
WindowsApps Python failed, so full prose/fact gates were not rerun: historical findings
remain13 prose counters and45 fact findings/525 rows/157 files. Passing include-context
tests do not replace the full gates; subsequent qualification records supersede these counts.

## Development b2179: PC boot, C-key ragdoll and held materials (21 October 2026)

Owner report: the stationary base PC stays loading on host and client. The
production shared-screen wrapper regression reproduces an old boot snapshot
writing `rootswitch` back to 0 after native startup has set it to 1. The previous
code fails the root-page assertion. Desktop-only traversal passes and preserves
native boot/power pages across repeated paints, while still restoring desktop
text, scroll and tabs with late-child retry. `run_shared_screen.ps1` passes,
including the existing wire, lifecycle and sleep-display cases.

`computer_boot_native_test.py` checks the existing laptop corpus: rootswitch 0
at ubergraph 29, 1 at 329, delayed continuation 315, anim=false at 550 and the
launched notification at 561. No fresh pak/Blueprint extraction was run. Actual
host/client startup, repeated power cycles, entering/leaving during boot, power
loss/restoration and join/reconnect remain gameplay acceptance for reports 54/55.

Owner report: pressing C launches the player on all roles and breaks ragdoll.
Model/runtime logs are not yet recorded. New production regressions fail on
the previous code: the skin setup changes mesh/physics asset before stopping all
old bodies, and local startup aligns the pelvis before animation warmup. The
latter starts simulation at the moved warm pose rather than the entry anchor.
An additional regression exposes the different-rig height error: the native
shell's pelvis cannot determine the selected model's standing/crouched height.
This is an offline reproduction of code defects, not of PhysX launch.

The skin setup now disables collisions and all old bodies before replacement.
Local startup captures the selected model's standing/crouched pelvis, preserves
that entry anchor across failed setup retries and
teleports the warmed pose to it immediately before simulation. Pawn, body, source
mesh and selected asset identities are checked across calls; missing/non-finite
poses and failed teleports refuse startup. Unreadable initial velocity becomes
zero, without reusing a previous entry's velocity.

Focused PASS: `run_ragdoll_skin.ps1` (actual production startup seam and skin
wrapper), `run_ragdoll_retry.ps1` (remote lifecycle and both production TUs).
Tests cover different native/selected pelvis heights, crouched entry, warmed-pose
movement, retry after configuration/read/teleport failure,
single startup, velocity preservation, non-finite positions and replacement or
teardown during reads/configuration/translation/start. Native C-key/faint and
standing/crouched/repeated recovery, slopes and selected/common/custom models
still require matching b2179 host/client gameplay. Reports 5/50 remain open.

The native `mainPlayer.updateHold` corpus reads `saveSlot.hold[0]` before replacing
the held actor. Existing profile snapshots refreshed water/material values only
in a copy; immediate native stock persistence covered only trowels. A new
production-controller regression first failed on the unchanged water-hand use
path because it never invoked persistence.

The controller now persists local held stock after water use/tick, material
changes and both native cement-transfer endpoints. The inventory wrapper accepts
the five exact supported classes, matching inner/outer keys and a current hand/
save/world/slot, then confirms live stock again. The codec validates all existing
bool/float/int groups against the snapshot before writing any changed values,
without native dispatch, engine allocation or replacing array headers.

Material focused PASS: `run_trowel.ps1`, `run_2153.ps1 -CompileSources`,
`run_2169.ps1 -CompileSources`, `run_nonentity.ps1`, `run_sponge_effects.ps1`.
Coverage includes zero/refill, unchanged base groups and array ownership,
malformed/missing shapes, stock changes, nested saved-value changes, actor
recycling, world/save/slot replacement and selected-hand controller hooks.
The scalar-memory test compiles the actual production codec function.

The first full attempt exposed an obsolete scalar-codec mock in
`inventory_view_2146_test.cpp`; it now uses the current API and `run_2146.ps1`
passes. Final qualification via `package/check_current.ps1
-EvidenceDirectory build/material-persistence-check` passes **65/65 groups**,
the stable-source guard and Release identity/ABI inspection (781 build entries).
Production snapshot SHA256:
`828154d98e2208cf82869997293f059d422511e62a8a5b2f801b5d4592ce3dcc`.
Development DLL `build/votv-coop-2154/main.dll` SHA256:
`5a92cbeda18433d3a3dde41494ddf75a2e8f0979c7698136f1fe97af53271332`.
Package 2155 and its delivery archive remain b2178. No package, installation or
publication is part of this continuation. Runtime resource transitions, trowel
key mismatch repair, concurrent writers and paid debit/output/refund transactions
remain open. Bench58 and Token52 are unchanged; no fresh asset audit was run.

Full public-prose and fact-row gates were rerun during this continuation: prose
still has 13 counters above baseline; fact rows reports 45 findings across 525
rows in 157 files (missing reference entries and unused declarations). Logs are
`build/material-persistence-check/public-prose.log` and `fact-rows.log`.
Passing offline groups do not close these full CI findings.

## Final package 0.9.2155 / DLL b2178 (21 October 2026)

Final **0.9.2155 / b2178** supersedes incompatible b2177; labels/metadata agree.
PASS Release `build/release-2155-logs/final-build.log`; unchanged **65/65**, 781 inputs,
identity/ABI: `build/continuation-check/verification.json`. PASS32 members,256x256 icon,
manifest/docs/paks/loader and delivery identity: `build/release-2155-logs/release-verification.json`.
Delivery: outer `dist/WatchersOfTheVoid-0.9.2155.zip`, `GITHUB_RELEASE_2155.md`, `SHA256SUMS-2155.txt`.
DLL SHA256: `9e5f4fd23b272efa6fd807df45cba466664acfe8639c5d4d639a9f23c9c4d5f8`.
ZIP SHA256: `ce2688393150abb294f3c3edb4acc489439042fc661fd09eefa80afb029764b1`.
No publication/commit/push/installation. Real-peer acceptance, host rifle, Token52,
events, paid transactions and then-deferred native audit remained open at this date.
