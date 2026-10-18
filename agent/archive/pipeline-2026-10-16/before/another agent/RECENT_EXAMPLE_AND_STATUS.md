# Current update: source2170, last locally verified package2146

16 October 2026. Owner confirms reports49–58 on build2168; that package has not been inspected locally.2170 partially addresses51: socket-endpoint authority for plugging/unplugging, native cable limit/occupancy retained, session/world/actor/component guards across callbacks. Two focused executables, one native-bytecode check and two production compilations passed. No full DLL or gameplay test. Other new reports, client wall repair and atomic cement accounting remain open. Read WORK_2170/TODO_2170 and the tracker. Earlier snapshots below are historical.

# Historical update: source2169, last package2146

16 October 2026. Personal profiles are parsed and sequence-checked before host storage; partial assemblies reset on slot/GUID changes and disconnect. Held water/soap/alcohol, concrete-bucket/cement-bag stock and sponge wetness now refresh from exact matching local actors before profile hashing, extending2168 trowel support. Sampling checks session/world/pawn lifetime. Two focused executables and four production compilations passed. No full DLL or multiplayer test. Client wall repair and atomic material transactions remain open; no-spend refusal stays in place. Read WORK_2169/TODO_2169. Earlier snapshots below are historical.

# Historical update: source2168, last package2146

16 October 2026. Supported completed-wall client melee damage uses the existing reliable action channel, with host weapon/reach/identity checks and native damage followed by placed-save state or retirement. Outgoing personal profiles now refresh held-trowel concrete from the matching local hand. Five focused executables and four production translation units passed; no full DLL or multiplayer test. Actual client repair and atomic cement transactions remain open. Read WORK_2168/TODO_2168. Damage effects use the wall origin; exact impact/remote effects remain. Trowel identity mismatches defer profile polling and require gameplay validation. Earlier snapshots below are historical.
# Historical update: source2167, last package2146

15 October 2026. Existing supported walls now enroll through the shared sliced scan hub with preserved native keys. Clients reuse save twins; legacy retirement is wall-only. Host placement/mutation/retirement tracking works without guests, and pre-mutation enrollment covers destruction before the census. Keyless census actors wait for native initialization. Four focused executables and two production translation units passed; no full DLL or multiplayer test. Client damage/repair and cement transactions remain open. Read WORK_2167/TODO_2167. The2166 snapshot below is historical; its existing-wall enrollment limitation is superseded for the four supported wall classes, not for plants/faucets/hoses.
# Recent example and remaining work

Snapshot: **15 October2026, source2166, last packaged release2146**. Refresh from the latest `../agent` handoff before acting. Source revisions2147–2166 are not packaged releases; the accumulated work still requires review and multiplayer validation.

## Latest example: wall construction and concrete display (2166, TestLog22/21)

**Report:** only the builder could see a placed brick-wall outline or the finished wall. Cement/trowel state also differed between players.

**Evidence and implementation:** placed walls belong to the `actor_save_C` path, so2166 extends2163's placed-save carrier. It covers `customWall_unfinished_C`, `customWall_C`, `customWall_rad_C` and `customWall_wetConcrete_C`. Native overlap consumes physical building materials and may spawn a successor; running it independently on both machines would duplicate those side effects. The host now processes queued overlaps after initialization, rechecking collision and live identity. Clients receive the resulting progress, completed actor and predecessor retirement. Wet concrete's successor also uses this path.

Two easily missed details were corrected: native outline `loadData` did not restore the material's successor class, and the old carrier skipped state replies to the local creator. The wrapper now restores that class using native `findMat`, and the author receives wall progress too. Scale is applied before native initialization computes requirements. Deferred work is bounded and invalidated on world/identity changes; work queued immediately before the last guest leaves still drains on the host.

Concrete buckets in another player's selected hand use the existing tool-display channel, with integer units0..20 and a dry/wet bit. The display setter paints only the disposable mirror, hides empty fill and restores it on refill. It does not replace the owner's inventory/world stock or execute pouring/drying. Protocol2166 reflects the new message semantics even though the packet size remains unchanged.

**A hidden failure found during review:** cancelling client wall repair left a false `isFine` out parameter, which tells the caller to spend cement. Refusal now writes true to the local and actual Blueprint out parameter. This prevents loss; it does not implement the missing client repair transaction.

**Actual validation:** four wall-focused executables plus two existing material/tool-display executables passed; seven affected production translation units compiled. After the final repair safeguard, only the changed authority test/TU were rerun, successfully. No full DLL link, release package or two-player test was performed.

**Still open:** client damage/repair with atomic trowel consumption; enrollment of legacy/non-mvpl and pre-session walls; creative force-build; remote construction cues; drying timer persistence; material concurrency. The placed outline is covered, not the tool's pre-placement preview. Do not call TestLog21/22 fully complete. The excluded basement wall41 was not changed.

Read `../agent/WORK_2166.md` for evidence and `../agent/TODO_2166.md` for the next steps. Key repository-relative files:

| File | Responsibility |
|---|---|
| `src/votv-coop/src/coop/world/placed_save_sync.cpp` | Placement, adoption, progress, retries and retirement |
| `src/votv-coop/src/coop/world/wall_build_sync.cpp` | Host construction authority, deferred overlap and client refusal |
| `src/votv-coop/src/ue_wrap/actors/wall_build.cpp` | Native overlap replay and material-class restoration |
| `src/votv-coop/src/ue_wrap/actors/placed_save_record.cpp` | Wall record schemas and bounds |
| `src/votv-coop/src/coop/player/hand_extinguisher_sync.cpp` | Existing hand-tool channel, now also concrete-bucket display |
| `src/votv-coop/src/ue_wrap/actors/building_material.cpp` | Native material reads and display-only setters |
| `agent/tests/run_2166.ps1` | Wall checks and affected-file compilation |
| `agent/tests/run_2159.ps1` | With `-OnlyBucket`: current material display checks |

## Intervening fixes: event schedule and Mimic (2164–2165)

The user's corrected sky report47 is the black disk with a yellow ring: `earthTp/newsky.tp`, not Bad Sun or black fog. Source2164 adds the visual material/light state. Source2165 moves schedule authority to the common `saveSlot.settime` entry, including loading and internal Blueprint routes, so each scheduled event does not need its own duplicate gate. This protects event history while keeping ordinary time updates. Audio, delayed mail and pre-session latent effects remain open; schedule authority alone does not synchronize every event outcome.

The user identified report48's disguised creature as **Mimic**, `prop_impostor_C`, generated by `impostorPropsMaster_C`. Source2165 blocks independent client generation and client-camera-based culling; actual class/disguise/birth/death/join reuse the ordinary keyed prop carrier. Lifetime remains host-controlled, even when a client can still see the creature. These fixes passed focused checks/compilation but need gameplay confirmation. They do not close all creature issues35/40. See WORK_2164/2165; do not resume the old walking-tree guesses or ask the user to identify Mimic again. The reports concerned old releases, not demonstrated regressions of the current unbuilt source.

## Earlier example: client placements vanish after save/reload (2163, TestLog46)

**Report:** crops, faucets and hoses placed by a non-host player disappeared when the world was reloaded.

**Evidence:** the existing generic spawn path admitted `prop_C` descendants. The placed forms, `faucet_C`, `groundHose_C` and `growingPlant_C`, belong to the separate `actor_save_C` lineage. The native client action succeeded locally, but the host did not receive an equivalent save actor. The inventory faucet/hose props were not the same actors as their installed forms.

Further native inspection established that:

- A faucet's caller writes some links after `FinishSpawningActor` returns. Capturing too early loses that information.
- A hose needs its completed spline points and faucet/sprinkler keys, not just its location. It is committed after native `finish`.
- Plants are created through buried-item processing or the pot's graph. Their saved state includes plant identity, growth-related values and a pot attachment key.
- Writing `actor_save.key` alone does not register it in the game's key-to-object map; native `getKey` performs that registration.

**Implemented approach:** exact placement-factory observation, persistent placement keys, capture after native initialization, reliable chunked records, host adoption, owner acknowledgement without respawning, delayed link repair, save-twin deduplication and retirement records for late joiners. Network input is validated and arbitrary serialized groups are not passed through.

**Hidden cases examined:** partial send followed by retry; removal before the full creation arrives; repeated creation after the host's state has changed; missing key registry; dependencies arriving later; recycled peer slots; world changes during callbacks; failed deletion; and a client's autonomous plant death incorrectly deleting the host's plant. Resting rows avoid copying a large hose record every frame.

**Actual validation:** two focused tests passed and five production translation units compiled. One subsequently adjusted wrapper compiled again. There was **no two-player game test, no real save/reload confirmation, and no new release DLL/ZIP**.

**Limits:** this is partial coverage of46. Later watering/fertilizing/harvest, pot transitions, re-routing an accepted hose and simultaneous interactions still need work. Actors loaded before session startup need consideration when extending live enrollment. Already-lost objects from old saves are not recovered. Do not describe the feature as fully synchronized gardening.

Read `../agent/WORK_2163.md` for the detailed evidence and `../agent/TODO_2163.md` for continuation. In a standalone Git checkout, the concrete files are:

| Repository-relative file | Responsibility |
|---|---|
| `src/votv-coop/src/ue_wrap/actors/placed_save.cpp` | Native capture, identity, spawn/apply and link repair |
| `src/votv-coop/src/ue_wrap/actors/placed_save_record.cpp` | Engine-independent record validation |
| `src/votv-coop/src/coop/world/placed_save_sync.cpp` | Authority, delivery, retries, adoption and retirement |
| `src/votv-coop/include/coop/net/protocol.h` | `PlacedSave` kind197, introduced in2163; current protocol2166 |
| `src/votv-coop/src/coop/net/session_lanes.h` | Ordered lane assignment; no blind relay |
| `src/votv-coop/src/coop/dispatch/event_dispatch_world.cpp` | Receive dispatch |
| `src/votv-coop/src/coop/session/subsystems.cpp` | Session lifecycle integration |
| `agent/tests/run_2163.ps1` | Focused validation runner |

## A second example: why replaying an action can duplicate outcomes

Source2162 addressed safe opening and grill ignition. Safe dial success and hacking converge on native `opened`. The client reports completion to the host, which opens the safe and creates loot once. Existing saved-state traffic needed protection so an early optimistic client record could not make the host skip loot creation, or a stale record relock the safe. An initial-record exception preserves already-unlocked items when dropped.

The grill uses host ignition/extinguishing and active/coal/burn snapshots, with retry and delayed-fuel handling. It still has open coal-concurrency, lid and personal-item cases. See `../agent/WORK_2162.md`; do not replace these paths merely because a simpler replay looks sufficient.

## Remaining work overview

Use `../agent/TESTLOG_STATUS.md` for precise row-by-row status. This overview deliberately distinguishes unfinished implementation from gameplay confirmation.

- **Placement/building:** finish46's subsequent interactions and22's client damage/repair, old/pre-session enrollment and remaining construction effects. Do not reimplement the newly added outline/build carrier.
- **Materials:** water20, cement/trowel21, sponge44 and grill45 still have concurrent changes, drying-timer ownership, personal-item transitions or secondary effects to address. Concrete-bucket selected-hand fill/dry display is now implemented in2166.
- **Events and creatures:** complete console event behavior/effects/order12–14, Airy26, living creatures35 and Flesh Eerie40. Source2161's event-gather teleport fix42 is not complete event simulation.
- **Latest reports:** sky47 still needs audio/mail/latent-phase work and gameplay confirmation; Mimic48 needs gameplay verification with its explicit host-lifetime limitation. Reuse2165's common schedule gate instead of adding one for every event.
- **Physics:** remaining grip contention/jitter2, contact convergence3, gun beams/freeze/scale/non-prop targets9 and unusual crowbar cases29.
- **Items:** battery and other consumable transitions25/39, remaining package contents34, toy sounds/gameplay15 and preview gaps38.
- **Presentation/performance:** sleeping body31 and verification/profiling of residual stutters16.
- **Before release:** review all post2146 changes, resolve remaining work with the owner, perform the authorized build/package checks and obtain real two-player verification.

Several other rows already have source fixes awaiting gameplay. Do not reimplement them from the original bug text without checking their latest status. Basement wall41 remains excluded by the owner's request.
