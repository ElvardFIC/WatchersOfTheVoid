# WORK — current state and open work

Current development: **DLL b2215 / protocol 2215**, game **0.9.0n**, package
**0.9.2160**. **94/94 full regression groups pass**; underlying-source evidence:
`build/upstream-qualified-2215/verification.json`. Upstream `ad5315611` and
local native-action/hit-result fixes `ddc711220` are included with their history.
Prepared manager package: **0.9.2160 / DLL b2215 / protocol 2215**. Matching-source
94-group results are reused for its public-version change; fresh Release/ABI,
menu/browser label, 32-member archive and five package rejection controls pass.
Current package evidence: `build/release-2160/verification.json` and
`build/package-current/verification.json`. Thunderstore upload remains pending
access. Rendered gameplay, manager startup and master deployment remain unverified.

[TODO.md](TODO.md) owns priorities and report checklists.
[TESTLOG.md](TESTLOG.md) owns completed-change reports, hashes and evidence.
[ENTITY_COVERAGE.md](ENTITY_COVERAGE.md) owns native mappings and per-class gaps.
This file keeps implementation boundaries and unresolved questions.

The owner now requests **10 concrete fixes per continuation**. TODO records
completed implementation with [x] separately from remaining work and rendered
acceptance; changing that status does not claim new gameplay evidence.

## Upstream integration awaiting gameplay acceptance

The complete upstream ancestry is merged without resetting or rebasing our
commits. Keep 32-player admission, stable published message values, owned native
lifetimes and record queues, personal PC explosions, ATV steering/cockpit/radio
state and blast-driven prop discovery. Adopt upstream host-judged damage,
creature hits/presentation, ATV authority/join sets, shared-prop transactions,
notebook/paper editing, explosion requests and device/UI/localization changes.
Actual received ATV condition results and post-application prop records are
published only after native application. Two queued drone timer presses toggle
host state twice. Details and verification boundaries: [TESTLOG.md](TESTLOG.md).

Rendered host/client acceptance remains open for these combined paths, including
concurrent actions, driver handoff, force-only blasts and late join. Every peer
requires b2215. The subsequent 0.9.2160 package is recorded above; no game
installation or master deployment was performed. Older acceptance items remain open.

## 24 October report batch

b2200 gives transient `dirthole_C` actors bounded, world-scoped identities and a
validated position/rotation/scale/soil record on the existing placed-object lane.
Both authors, acknowledgement, retry, retirement and late join use that lane.
Mirror construction waits for the BeginPlay suppression watch: replaying native
BeginPlay would search buried treasure and create loot again. This does not add
save-file persistence or complete buried-item/treasure transaction authority.

Can mutations first drain pending canonical state; the host prevents ordinary
client records from increasing portions, closing an opened can or changing its
type. Initial saved births are exempt. Other consumable families still need
their own native contracts and concurrent-use transactions.

Satellite claims use physical-panel position and the shared widget's native
`used` owner. Missing unpublished ATV bodies release the retry latch. Location
events retain remote activation but no longer gather participants. Host sleep
uses personal restoration until every connected player reports rest; awake,
joining and dreaming participants prevent global acceleration.

Report (2)'s item-copy trigger/build are unknown. b2202 implements a dedicated
phase adapter for report (6.1)'s blank-email candidate, `arirShipAppear_C`:
host visible/alpha, native light/radar, current-state joins and short-lived live
air cues reuse scene actors. Clients stop old timelines and refuse their own
camera/day disappearance and delayed mail chains. Exact scoped native trigger
calls bind/unbind only the photo delegate; local photo checks/rewards stay native.
The host still owns eligibility, animation timing and mail generation. The
original rendered reproduction, `arirShipVanishing_C`, other event variants and
shared reputation consequences remain open. These are distinct from actor spawn.
The optional bed-capacity suggestion is deferred; class-only sleep reports do
not identify a particular mattress. Rendered acceptance remains open for all
implemented fixes; the game disk is unavailable. See TODO's batch-specific IDs.

Artifact cleanup completed on 24 October 2026 through
[cleanup_current.ps1](../package/cleanup_current.ps1): 3,833 generated/obsolete
files removed (2,638.6 MiB), including old builds/staging and release archives.
Current DLL/dependencies, package 2158 and verification/crash evidence remain;
current DLL and delivery archive hashes are unchanged.

## Development changes awaiting acceptance

| Area | Implemented boundary | Still open |
|---|---|---|
| Prop actions/MRE delivery (b2214) | Source- and target/class-owned, numbered FIFO entries reserved before send; copied payloads, nonreentrant eight-row drain, exact completion, bounded refusal/expiry and replacement-queue preservation. Client world-MRE refusal/dedup/cue lifetime; active personal spawn scopes with exact source/function/player/invocation proof, including a cleared hand. Received opens guard authorization/body/metadata/mesh and retain an at-most-once admitted-dispatch latch; safe record capture/retry owns its revision. | Rendered host/client opening, actual accepted output/debit/refund recovery after uncertain native failure, remaining package subclasses, poison-pour/roach effects and native reward routing. The latch is conservative admission, not proof that loot committed. Local host-native actions are not converted into a received transaction. |
| Food callbacks/output (b2213) | Bounded reserved eat observations; known valid portions/address, exact source/actor/class/key currency and POST ownership; dead final-bite request retry; active native eaten provenance for box/banana/cup, including hand-cleared final bites; weak function/owner and invocation retirement. Host final output rechecks canonical zero portions after publication and metadata preparation. Six personal overrides and empty base are excluded from host replay. | Cross-peer concurrent portion/output arbitration and optimistic personal effects before host verdict, other package subclasses, full-save behavior and rendered multiplayer acceptance. Existing personal effects remain on the eater; this does not turn local feedback into an acknowledged transaction. |
| Wall material delivery (b2212) | World/serial/role/running source resets; weak pair/class ownership before enrollment; protected preparation and one replay permit; guarded native metadata/query/component/function currency; fair paced retries before admission; no automatic repeat of an admitted spend; invocation-owned POST and partial-state publication; nested queue/install preservation and independent retries for six native guards. Solo-host and last-peer native ownership remain. | Paid client repair request/result and exact personal-stock accounting across hand/slot/drop, host refusal and disconnect. Client repair still refuses without spending; rendered native impact/build/destruction acceptance remains open. |
| Local container recovery (b2211) | Pre-debit bounded owned escrow for base world-container extract; exact native debit/pocket receipts; checked full-record restore; failed/null/partial spawning and full-inventory refusal settle by retiring the exact carrier then atomically returning its saved record. Refund retries preserve current other rows/newer same-key data, notify the contents lane and rebuild names before guarded volume/mass. Source/class/invocation ownership and best-effort stopped-session settlement protect replacements. | Cross-peer debit/output arbitration, personal extraction overrides/raw takeObj paths, rendered acceptance and guaranteed disconnect recovery. Missing debit/output receipts retain unresolved bounded debt; no class-only actor guesses or speculative refunds. Debt is not persisted across world/session replacement. |
| Console input/playback (b2210) | Consume-before-submit with guarded native textbox preparation and same-input reentry refusal; captured source/widget/host binding through reads and counters; bounded invocation records preserve nested clear/output and prune skipped POST; exact POST/native busy ownership with paired graph-watch rollback; source/target-owned display retry, clear revision, host-binding text/busy retirement and single-invocation argument-matched replay. | Rendered client input, simultaneous console use and native command world effects. These local regressions do not close Rufus/thiccfus, llama, Madness/event variants or other entity effects. |
| Console delivery (b2209) | All 31 remote terminal seats; source and recipient incarnation guards through native command preparation and wire chunks; owned, identified queue entries with nonreentrant flush; revision-owned history retry and current busy reseed; consistent nested clear ordering across observers; widget-bound local holds and both busy levels; detached terminal/session teardown preserves nested installs. | Rendered simultaneous console use, real input and world command effects. Rufus/thiccfus, llama, event variants and other entity effects remain separate work. Current-state history is bounded; it does not replay native commands. |
| Menu/branding cleanup (b2208) | Removed the tester/reviewer roll, embedded list, startup/cache/parser and browser HTTP path; retired client API and build inputs. Current startup/README/package descriptions and Sponsor metadata no longer promote the original author. License/third-party notices and historical provenance remain. | Visual main-menu acceptance after restarting with the current DLL. No published archive or remote service was changed. |
| 2–32-player selection (b2207) | Ten completed fixes: separate total-capacity row and presets; remembered typed default; shared native/overlay selection with deferred slider writes; safe build/menu cleanup and endpoint controls; environment config, legacy HostLobby and environment announce propagation; normalized re-announcement; coherent browser/master counts with overflow-safe parsing. All 31 limits pass the real hosting controllers across both save choices, modes, listing and offline fallback. All 118 master tests and its local Release build pass. | Rendered selector input/layout, real two/32-player admission/full/reuse and larger-group performance. Matching remote master deployment remains unrecorded; no package or live service was changed. |
| Sleep synchronization (b2206) | Ten fixes: retained host flags and peer relays; complete awake-inclusive join repair; generation/readiness-bound votes and delivery; installed-session paced payments; pending wake/dream veto; native host-dream veto; active canonical wire input; world/session/role reset; exact client payload and nested-send completion. Current-state debt is bounded by seats and retries refused recipients at 100 ms. | Rendered unanimous clock, wake/dream and join/replacement transitions. Local/native nightmare probability and rewards stay personal. Packets identify bed classes, so the optional per-mattress capacity suggestion remains deferred. |
| Inventory/profile safety (b2205) | Ten fixes: strict personal, equipment/hand, world-slice and target outer headers; checked whole held-record staging; current native volume/name callbacks; live join restoration; coherent revision/request ownership; exact-recipient guarded host preparation/chunks; generation/GUID-bound sent latches and retries. | Rendered join/rejoin, held/container UI recovery and replaced recipients. Outer-header validity does not validate every nested record or allocation extent. New profile arrival cancels an old restore; it does not automatically restart that consumed save-ready hook. Original item-copy cause and removed-row/carrier refund accounting remain open. |
| World containers (b2204) | Five fixes: shared masked/world-slot boundary; checked scratch and retry without false publication; scoped current actor/component/store/sender commits and relays; revision-safe park replay/retirement; exact-recipient relay refusal debt. Birth completion precedes native post-commit callbacks. | Rendered concurrency and UI/volume recovery; original unknown item-copy reproduction; native removed-row/carrier refund accounting. Partial unpublished nested scratch retains the codec's pin policy. |
| Inventory restore (b2203) | Four fixes: checked preparation of all three arrays before publication; owned-input and actor/class/world/save/header/nested-call guards; masked Player plus exact personal slot zero; class-incarnation field caches shared across personal/world readers. | Rendered join/rejoin and profile restoration; unidentified original item-copy trigger; removed-row/carrier refund transaction and broader writer/debit arbitration. Failed unpublished nested scratch follows the existing buffer pin policy. |
| Blank-email Ariral appearance (b2202) | Three fixes: shared phase/material/light/radar; client-local appearance/disappearance/mail suppression with scoped local photo listeners; quiet current-state joins and retained guarded delivery/application. Indexed scene lookup, unique position fallback, native timeline stop and bounded one-second cosmetic cue retry. | Original rendered warp-arrow reproduction; other classes including arirShipVanishing; shared reputation and broader event completeness. No history replay or player movement is needed by this adapter. |
| Container births (b2201) | Three fixes: native-body extraction scope, incarnation/out-frame result validation, and retained ordered birth/state publication. Generic finish publication defers to that owner. | Rendered ejection/extraction, root cause of the original client item copy, and the separate removed-row/failed-carrier refund transaction. |
| Records (b2194–b2197) | Initial-author waits, capture readiness, guarded native dispatch and full-record carrier lifetime checks; failed restoration refuses pocket capture. | Cross-peer writer arbitration and debit/output/refunds. Extraction removes the native row before restoration: retain/retry its saved record or refund it on failure. |
| Materials (b2193) | Exact stock/row/hand eligibility is checked through capture and publication; superseded work stays dirty for a fresh attempt. | Delivered chunks cannot be recalled. Concurrent writers, dryTimer ownership, hand transitions and accounting. |
| Grill (b2191–b2192) | Ordered coal/lid work and held rows cancel across session restart, replacement and recipient retirement; interrupted sends retain delivery debt. | Personal debit, cooking output, refunds, disconnect accounting and gameplay acceptance. |
| Blast motion (b2189–b2190) | Accepted host blasts, including zero damage, discover eligible props at native impulse and use the existing drive/rest publisher. Native LAN covers ordinary and force-only cases. | Broad host/client physics, filters, simultaneous impulses, pickup/handoff and late join. Keep lanes 188/191; upstream ExplosionSpawn=177 conflicts with KitchenState=177. |
| ATV (b2188) | Steering replicates; join snapshots retain pending honks. Native LAN covers both directions and late join. | Rendered steering and driver/carrier handoff on current DLLs. |
| Bleeding (b2187) | Drop parameters reach ready observers, excluding the injured owner; damage and blur remain personal. | Rendered injuries, stopping bleeding, mid-bleed joins and refusal/reconnect. Exact stain layout, historical-stain joins, cleaning parity and full-save persistence remain unverified. |

Native blast/ATV evidence belongs to earlier DLLs. Offline/native checks do not
close rendered gameplay or real keyboard-input acceptance on b2214.

## Runtime investigations and implementation gaps

- **Bench58:** host old rifle vanished after placement. `prop_rifle_C` is a
  candidate, not a confirmed cause. Capture class/key, destruction caller/timing
  and drop/sweep logs with `VOTVCOOP_PROP_BIRTH_KEY_PROBE=1`.
- **Token52:** probably shared `saveSlot.Points`; confirm the original UI and
  accept credit/spend/refusal/join. Delivery guards and queued-credit cancellation
  across world replacement are implemented.
- **Trowel:** stock persists into `hold[0]`, including zero. Native graphs preserve
  non-None saved keys; capture a real mismatch before designing key repair.
- **Personal resources/paid repair:** complete atomic debit, accepted output and
  refund across rejection, retry, disconnect and hand/slot/drop. Persistence and
  capture guards do not complete the transaction. b2211 completes the proven local
  base-container failed-extraction refund path; cross-peer arbitration and the
  receipt-less/persistence cases above remain open.
- **Remaining systems:** console/toy/entity behavior, package subclasses and
  cross-peer food transactions beyond b2213's scoped output provenance,
  dynamic grime and full-reload scorch, sky-event client phases,
  stow performance and model previews remain in TODO. Fish boundaries:
  [HANDOFF_FISH.md](HANDOFF_FISH.md).

## Gameplay, validation and delivery

Immediate-join flashlights/switches/doors/drone contents and rendered 2–32-player
admission still need acceptance. The chooser and local master implementation are
complete; confirm matching capacity on the deployed master. No remote deployment is recorded.
Boundaries: [SYNC_2157_FIXES.md](SYNC_2157_FIXES.md).

Selected upstream adaptations still need rendered checks:
[FORK_AUDIT_20261022.md](FORK_AUDIT_20261022.md). Upstream ancestry was not merged.
Ragdoll reports 5/50 and stationary-PC loading 54/55 remain open; TODO keeps
input, transition, power-cycle and handoff cases. Optional authored PhysicsAsset/
display retargeting is separate from current acceptance.

Report IDs 1–58 remain tracked in TODO except closed Mining (6) and excluded
Wall41; chair reports 11/36 are duplicates. Previously working nightmares (10)
need acceptance of newer personal-sleep behavior. Use identical current DLLs and
record role, item/model, reproduction and logs in TESTLOG before closing reports.

The latest fact-row gate passes **565 rows / 181 files**. Remaining public-prose
counters: documents **85/82**, src.comment_doc_row **155/0** and
src.files_not_swept **84/0**, with reading-room checkouts absent.
Full-manager-package startup and rendered multiplayer remain unverified.
Requalify changed source through current package scripts/metadata. GitHub descriptions
are published; the 0.9.2160 Thunderstore upload remains pending access.

## Requested event synchronization and frequency

Existing replay/join/location lifetime guards, readiness retries and cancellation
do not establish category-wide effects or phase parity.

The eight-category request remains open. The resumed fresh audit indexed 42,941
pak entries and extracted 37,971 uasset/uexp files (18,854 asset packages).
The relevant inventory decoded 1,621 Blueprint packages / 22,691 functions,
including 69 scheduled-event rows and 190 focused dumps, with no package errors.
The raw inventory's zombieHordeController/ufoAlienDropper rejections were hint-text
false positives. Rechecking all 22,691 functions found no actual decode errors:
`build/requested-events-audit/reclassified-graphs.json`; the raw inventory is retained.
Level/map headers were not extracted; those graphs remain incomplete.
Evidence: `build/requested-events-audit/`.
The imported `sync_native_contract_test.py` passes against this fresh corpus.
Native mapping and each event's effects/phase/join behavior still require proof.

| Requested category | Remaining implementation or acceptance |
|---|---|
| Dreams | Wake rewards in the 11 audited scenes are personal vitals/stats/achievements; scene-local boulders/mannequins do not justify shared reward props. b2199 prevents remote puppet/ragdoll overlaps from consuming a local dream burger. Native sleep/createDream/leaveDream and rewards remain personal; verify scene effects and lifetime on peers, preserving independent nightmare/Insomniac conditions. |
| Ariral Reputation | calcRep uses -75/-25/25/75 tiers; launchEvent is empty, tier listeners and mainGamemode feeding/wakeup/punch/gifts carry real effects. Complete shared state, randomized outcomes, relocation and late join. |
| Player-Activated | Map and verify every listed interaction, simultaneous activation and canonical side effects. |
| Time-Related | Fresh mainGamemode audit finds real-calendar branches outside eventer (June 28 21:00, April 1, November 29 22:49). Complete these, 3AM eligibility, chosen outcomes and uncovered effects. |
| Endings | Establish and verify each ending's per-player versus session consequence; do not replay quit/travel on remote puppets. |
| Story Mode | Complete actor/phase/audio/bookkeeping adapters and current-state late join. Audited horde/dropper need wave/class budgets, alive sets, countdowns, timeline and owner-local health/effects; replaying their start can repeat rewards/save work. Their active attack/animation cadence must not be frequency-scaled. |
| Random | Fresh inventory identifies remaining generators; complete tree visibility/motion/lifetime, camera-dependent bp7 player effects and protected-server consumption/audio/delegates. Black-fog client suppression alone does not mirror black fog. |
| Signals | Audit each processing/playback trigger, funny-rule requirement, chosen outcome, phases and late join. |

Frequency coverage now includes the five verified
`ticker_fossilhoundSpawner`, `ticker_yellowWispSpawner`, `ticker_wispSpawner`
and `ticker_egSpawner`, plus owner-local `ticker_eyers` routes.
The existing night-eyes Blueprint confirms a native 1800–2700s re-arm, 5% roll
and day/night gates. Only the wait is scaled; the existing owner-entity route
remains responsible for its creature. Host owns the first four generators.
The verified peer-symmetric `ticker_fireflySpawner` route is also frequency-scaled
on every peer; its local-camera eligibility/roll and emitter capture/replay remain
native. The shared setting changes only its tick wait.
Development b2198 admits host-owned `ticker_flickerer_C`: only its native
30–60s eligible-roll wait is scaled. Generator-dependent probability, short
visual delays/repetition and the existing lamp/ambient effect transport remain
native. Source/cooked/live-reference regressions pass; multiplayer acceptance remains.
`treeSpawner` (600–900s walking-tree output), `bp7Spawner` (camera-dependent rare
integer roll and per-player pause/quit) and `serverBreaker` (native 1200–600s wait)
remain outside frequency coverage until their complete effects are transported.
Serverbox state delivery alone does not cover protective floppy consumption,
repair audio or delegate notification. Walking-tree appearance/render-dependent
movement and bp7 player consequences also need explicit routes.
Preserve native eligibility, probabilities,
personal dream rolls and story dates; adjust eligible-roll intervals without
changing AI/animation, effect durations or sleep needs.

Real-peer acceptance must cover chosen/repeated events, simultaneous triggers,
live/persisted frequency changes, load windows, late join, reconnect and teardown.

Development b2199 retains serverbox delivery debt per accepted recipient; refused
or partially refused fan-outs, new occupants and refused repair corrections retry.
Clients retain the latest snapshot until native lists/fields and writes are ready;
unfinished re-skin work retries even after IsBroken changes. Box incarnations and
world/session/gamemode/occupancy changes cancel old work. Production regressions
reproduce the old lost retry; protected-server generator contracts also pass.
Dream-burger guards use incarnation-bound function offsets and frame bounds;
local/nonplayer native collection remains intact. Three removal controls fail.
These fixes do not close the category-specific phase or resource transactions.

Runtime investigations remain open. b2198 cancels queued balance credits when
the world changes. Fresh trowel graphs preserve a non-None saved key through
loadData/init/getKey/assignKey; capture a real mismatch before designing repair.
Two isolated b2198 launches overflowed the native VM stack before acceptance:
dumps prove recursive lib_C.end; Untitled_1.CustomEvent's borders guard is the
strongest initial-caller candidate, pending live confirmation. Evidence:
`build/requested-events-native/crash-analysis-20261024.md`. Do not suppress the
native guard. The owner confirmed the game disk is absent on 24 October, so
current-DLL rifle/trowel/balance and real-peer event checks cannot yet run.
