# TODO — current priorities

Continuation size: **75 concrete fixes per pass**. Checked items mean implemented
code with recorded verification; unchecked items mean remaining implementation
or a named acceptance test that has not passed. Completed code does not close an
unreproduced report or an unperformed rendered multiplayer test.

## Active development — 0.9.2161 / b2217

Owner correction: **keep working on 2161**. A continuation does not change the
public version or automatically prepare a package. Historical 2162 labels were
an assistant mistake; their evidence remains historical, not current qualification.

### Prepared package — 0.9.2161 (27 October)

- [x] Run all 121 registered offline validation groups against the current source snapshot.
- [x] Build/verify Release b2217, protocol 2217 and the production 0.9.2161 label.
- [x] Prepare and verify the 32-member manager ZIP with the exact qualified DLL.
- [x] Reject failed/focused/incomplete qualification, stale label and mismatched DLL controls.
- [x] Refresh package README/RELEASE/CHANGELOG and record archive/DLL checksums.
- [ ] Rendered manager startup and real-peer gameplay acceptance remain open.

Archive: dist/ElvardFIC-WatchersOfTheVoid-0.9.2161.zip. Offline package evidence:
build/package-current/verification.json. No version bump, game installation or publication.

### Implemented — TODO Console/events: 75 llama marker/event/Begin fixes (27 October)

Keep 0.9.2161 / b2217 / protocol 2217. Extend the existing PlacedSave path for
the native llamaSoul save marker; separate canonical presentation from native
event accounting; recover Sphere/timer initialization suppressed at client Begin.
No new reliable kind, wire shape, package or release identity.

- [x] 1. Recognize the exact llamaSoul marker class.
- [x] 2. Validate current marker owner/world identity.
- [x] 3. Read the native save key.
- [x] 4. Refuse foreign save-key takeover.
- [x] 5. Require measured actor_save ancestry.
- [x] 6. Validate native getData dispatch.
- [x] 7. Validate native loadData dispatch.
- [x] 8. Normalize only the exact marker record class.
- [x] 9. Refuse extra marker record fields.
- [x] 10. Validate finite location/rotation.
- [x] 11. Validate marker scale.
- [x] 12. Verify the native generated key.
- [x] 13. Recheck source after native callbacks.
- [x] 14. Preserve the complete native key without truncation.
- [x] 15. Materialize the exact marker class on clients.
- [x] 16. Check key vacancy before loading a marker.
- [x] 17. Validate the spawned actor incarnation.
- [x] 18. Apply transform explicitly; base loadData restores only key.
- [x] 19. Verify key readback and ownership after apply.
- [x] 20. Adopt an existing keyed save-loaded marker.
- [x] 21. Keep marker creation host-authoritative.
- [x] 22. Refuse client-authored marker removal.
- [x] 23. Refuse marker care/harvest/unrelated operations.
- [x] 24. Discover late marker births through the existing census.
- [x] 25. Preserve native marker actors across disconnect/rejoin.
- [x] 26. Stop canonical client Apply from minting native h.
- [x] 27. Keep canonical light/audio presentation through wire state.
- [x] 28. Discover local events active before connection.
- [x] 29. Prepare native accounting before the first canonical apply.
- [x] 30. Debit only verified matching sender entries.
- [x] 31. Validate GameMode/class/world ownership.
- [x] 32. Validate active-event counter type/range.
- [x] 33. Validate the native sender array.
- [x] 34. Account for repeated entries of the same sender.
- [x] 35. Preserve unrelated event senders.
- [x] 36. Subtract only llama's captured event contribution.
- [x] 37. Clear native h while preserving adjacent bits.
- [x] 38. Revalidate immediately before committing accounting changes.
- [x] 39. Preserve caller output on failed native reads.
- [x] 40. Retry transient unavailable metadata without changing accounting.
- [x] 41. Exclude host-owned actors from client cleanup.
- [x] 42. Never debit an absent sender.
- [x] 43. Prevent repeated accounting cleanup.
- [x] 44. Refuse retired actor/class/world owners.
- [x] 45. Bound weak accounting-guard records.
- [x] 46. Block a legacy false event debit when its sender is absent.
- [x] 47. Preserve legitimate new native activation.
- [x] 48. Preserve registered native event completion.
- [x] 49. Keep the guard through same-world disconnect with its own script-gate hold.
- [x] 50. Retire dead/world-replaced guards and skip duplicate generic event replay.
- [x] 51. Track only suppressed original-true client Begins.
- [x] 52. Require no pre-existing timer before new-Begin recovery.
- [x] 53. Leave existing handles to the prior pause-lease module.
- [x] 54. Capture the original Sphere collision setting.
- [x] 55. Require the actual Sphere component type.
- [x] 56. Validate source-to-Sphere fields.
- [x] 57. Validate GetCollisionEnabled and its byte return.
- [x] 58. Preserve output after failed/retired preparation.
- [x] 59. Bind recovery to actor/class/world incarnation.
- [x] 60. Bind recovery to the Sphere/component-class incarnation.
- [x] 61. Validate TimerHandle layout even for a zero handle.
- [x] 62. Pair PRE/POST/cancel with exact invocation ownership.
- [x] 63. Reserve before callback-capable native preparation.
- [x] 64. Commit only a successful original-true park and admitted base init.
- [x] 65. Discard canceled unmodified PRE without synthesizing work.
- [x] 66. Wait for the original Begin stack to finish.
- [x] 67. Restore collision only while its disabled state remains ours.
- [x] 68. Refuse a replaced Sphere.
- [x] 69. Never replace an externally created nonzero timer.
- [x] 70. Invoke only native graph108, never base Begin1146.
- [x] 71. Defer rearming until SP/host authority.
- [x] 72. Admit native timer creation once, including uncertain completion.
- [x] 73. Read back the actual native handle after graph admission.
- [x] 74. Bound/deduplicate and retire Begin recovery records.
- [x] 75. Retain safe pre-admission restoration refusals for paced retry.

PASS 75 new numbered actual-code lifecycle groups: 25 marker wrapper/PlacedSave
cases, 25 event wrapper/controller cases and 25 Begin wrapper/controller cases.
Root integration also checks accounting readiness before canonical Apply and
refused Begin preparation before base init. Current combined qualification is
recorded in TESTLOG; engine/codec/registry/transport boundaries are doubled.
- [ ] Accept rendered native save/reload, same-world teardown, event/audio timing,
  marker adoption and timer rearm. Canonical visuals do not synthesize native
  host activeEvents contributions on clients. An old/new latent continuation on
  the same actor has no causal nonce; absent-sender protection does not prove
  phase reconstruction. Unknown metadata cannot prove absent membership, so the
  guard retains native execution. Missing POST permits collision cleanup only;
  unknown admitted timer creation is never replayed. Safe cleanup refusals need
  a later Install/Tick; there is no idle SP retry pump. Full regression/package
  and the wider Console/events category remain open.

### Earlier implementation — TODO Console/events: 75 llama timer/delivery/completion fixes (27 October)

Keep 0.9.2161 / b2217 / protocol 2217. Continue the llama follow-ups with three
separate owners: native pre-session timer leases, canonical state/cue delivery
and completion/motion after launch admission. No new network kind or package.

- [x] 1. Discover client llama timers already running before connection.
- [x] 2. Repeat the timer census for late/save-loaded actors.
- [x] 3. Require the timer owner's actual llama class and current world.
- [x] 4. Exclude hand-axis actors from timer parking.
- [x] 5. Bind timer parking to the source actor incarnation.
- [x] 6. Bind timer parking to the source class incarnation.
- [x] 7. Validate the timer property's type before parking.
- [x] 8. Validate the inner TimerHandle type and size.
- [x] 9. Refuse zero or malformed timer handles.
- [x] 10. Confirm the native timer exists before mutation.
- [x] 11. Distinguish active timers from paused timers.
- [x] 12. Leave timers already paused before coop untouched.
- [x] 13. Validate pause/unpause/query function bindings.
- [x] 14. Validate their exact parameter spans.
- [x] 15. Pause without clearing or rearming the native timer.
- [x] 16. Confirm the actual paused result.
- [x] 17. Retain uncertain dispatched pause ownership for reconciliation.
- [x] 18. Bind timer leases to session/start/role/world.
- [x] 19. Reserve timer ownership before callbacks.
- [x] 20. Refuse nested timer acquisition/restoration.
- [x] 21. Bound and deduplicate native timer leases.
- [x] 22. Preserve the native interval's remaining phase through pause/unpause.
- [x] 23. Resume only the captured timer parked by this controller.
- [x] 24. Leave replacement or already stopped timers untouched.
- [x] 25. Restore timer leases on disconnect, role change and session restart.
- [x] 26. Preserve a still-live cue through same-active heartbeat.
- [x] 27. Preserve the original cue timestamp through heartbeat.
- [x] 28. Preserve quiet recipient intent through same-pulse heartbeat.
- [x] 29. Track each recipient's already delivered sound pulse.
- [x] 30. Prevent newer same-pulse packets from extending local cue expiry.
- [x] 31. Prevent same-pulse sound resurrection after a quiet snapshot.
- [x] 32. Keep packet ordering after client state expiry.
- [x] 33. Keep heard-pulse history after client state expiry.
- [x] 34. Reconcile clock rollback before installing a newly received cue.
- [x] 35. Saturate receive-expiry arithmetic.
- [x] 36. Make client poll pacing safe across unsigned clock overflow.
- [x] 37. Resolve the current canonical tracker actor for each apply.
- [x] 38. Recheck announced-world membership inside apply ownership.
- [x] 39. Recheck recipient identity/connection after source callbacks before send.
- [x] 40. Recheck recipient readiness immediately before send.
- [x] 41. Recheck state/debt revision after nested source sampling or requeue.
- [x] 42. Rotate source send order under the shared budget.
- [x] 43. Rotate recipient send order.
- [x] 44. Bound per-row send attempts to prevent refusal fanout starvation.
- [x] 45. Restore parked Begin flags when an invocation disappears.
- [x] 46. Restore parked Begin flags at disconnect.
- [x] 47. Restore a locally held park token after its capture disappears in a callback.
- [x] 48. Restore in matching POST even if the function token retired.
- [x] 49. Retain refused Begin-flag restoration for retry.
- [x] 50. Restore the old role's parked flag before a same-world role/session change.
- [x] 51. Separate target admission from source-owned launch completion.
- [x] 52. Finish an admitted launch after the selected peer disconnects.
- [x] 53. Finish an admitted launch after the selected target moves.
- [x] 54. Finish an admitted launch after the selected body is replaced.
- [x] 55. Stop completion when the source actor/class/world retires.
- [x] 56. Recheck source position after rotation before clearing the timer.
- [x] 57. Dispatch the native tail once after an uncertain velocity attempt.
- [x] 58. Keep tail ownership independent of retired velocity-function binding.
- [x] 59. Queue motion admission for remote jumps.
- [x] 60. Queue motion admission for native local-host jumps.
- [x] 61. Confirm Coast admission through IsDriven readback.
- [x] 62. Retry motion after late prop-element enrollment.
- [x] 63. Pace retained motion retries.
- [x] 64. Bound pending motion rows.
- [x] 65. Cancel motion on source/key/class retirement.
- [x] 66. Retire motion on session/start/world replacement.
- [x] 67. Yield motion to a newer player-hand claim.
- [x] 68. Yield launch/motion ownership to Kerfur.
- [x] 69. Cancel pending motion on explicit reset.
- [x] 70. Bound motion lifetime and handle clock rollback.
- [x] 71. Release source capacity after failed/unarmed capture.
- [x] 72. Preserve consumed-timer latches while pruning idle poll records.
- [x] 73. Admit a newly armed llama after stale poll-record pressure.
- [x] 74. Reserve queued motion before callback-capable Coast admission.
- [x] 75. Keep newer motion debt through older callback completion.

PASS 75 new numbered actual-code recovery groups: 25 timer wrapper/controller/
ParamFrame cases, 25 state/Begin cases and 25 launch/motion cases. The earlier
100 llama groups also pass. Engine/reflection/registry/transport/object slots
and clocks are doubled; current combined Release qualification is in TESTLOG.
- [ ] Accept rendered same-world disconnect/rejoin, preserved timer phase,
  heartbeat sound/quiet joins, physical motion after late enrollment and native
  tail completion. Marker/save/event-count lifecycle and restoration of
  never-created Begin timers/Sphere collision remain separate gaps. An external
  pause on an identical already-paused handle is not causally distinguishable.
  Refused timer cleanup is retained for a later Install/Tick, without an idle SP
  retry pump. Ordering/heard tombstones retain at most 256 keys; oldest expired
  rows may be evicted. Full regression/package and the wider category remain open.

### Earlier implementation — TODO Console/events: 75 remote-player llama trigger fixes (27 October)

Owner changes continuation size to **75**; AGENTS and agent/README now match.
Keep 0.9.2161 / b2217 / protocol 2217. Continue the existing llama subtask rather
than closing the wider Console/events category. No new network kind or package.

- [x] 1. Restrict remote target selection to the host.
- [x] 2. Stop selection outside a running connected session.
- [x] 3. Require the target's world-ready slot.
- [x] 4. Require the target's transport-ready slot.
- [x] 5. Select registered players rather than class-scanned actors.
- [x] 6. Refuse dead remote players.
- [x] 7. Wait for the target's first network pose.
- [x] 8. Refuse remote ragdoll display bodies.
- [x] 9. Confirm proximity using native Pawn overlap.
- [x] 10. Keep local-host triggering native, but require its own body in that overlap; prevent a remote overlap from launching toward camera zero.
- [x] 11. Refuse unrelated overlapping Pawns.
- [x] 12. Validate finite target body coordinates.
- [x] 13. Validate finite target eye coordinates.
- [x] 14. Bound the eye/body separation to 600 cm.
- [x] 15. Choose the nearest visible remote eye.
- [x] 16. Break equal-distance ties by slot.
- [x] 17. Trace native static/dynamic occlusion from the Audio origin.
- [x] 18. Refuse activation when the trace is unavailable.
- [x] 19. Try another target when a nearer player is obstructed.
- [x] 20. Bind selection to the roster person number.
- [x] 21. Bind selection to the transport generation.
- [x] 22. Hold the target body's slot/serial incarnation.
- [x] 23. Hold the target body's class incarnation.
- [x] 24. Recheck target membership/life/pose after native callbacks; refuse movement beyond 1 cm during admission.
- [x] 25. Exclude llama owned by a hand stream or hand axis.
- [x] 26. Require a current prop_llama class.
- [x] 27. Refuse an already active h state.
- [x] 28. Require the native yumpskare setting.
- [x] 29. Require a StructProperty timer field.
- [x] 30. Require the actual eight-byte TimerHandle type.
- [x] 31. Refuse zero timer handles.
- [x] 32. Confirm native timer activity, including before launch.
- [x] 33. Require a typed current SphereComponent.
- [x] 34. Validate the float radius and retain its value through admission.
- [x] 35. Read the actual world-space sphere center.
- [x] 36. Read the actual Audio world origin for visibility.
- [x] 37. Require a typed current StaticMeshComponent.
- [x] 38. Require active physics simulation, including before launch.
- [x] 39. Retain each source-to-component binding.
- [x] 40. Retain the source world pointer and generation.
- [x] 41. Validate native query functions and exact nonoverlapping parameter spans.
- [x] 42. Use native Pawn query type byte 2.
- [x] 43. Remove the mainPlayer-only native class filter for registered cooperative bodies.
- [x] 44. Own overlap input arrays across the native call.
- [x] 45. Free engine-owned overlap output after success.
- [x] 46. Free engine-owned overlap output after call refusal.
- [x] 47. Validate overlap counts/capacity/null-data shape before access.
- [x] 48. Deduplicate overlapping actor incarnations.
- [x] 49. Bound selection to 31 registered candidates; reject native arrays beyond 65536 rows without truncating away players behind unrelated Pawns.
- [x] 50. Preserve caller output after a failed or retired native snapshot.
- [x] 51. Launch toward the selected eye rather than host camera zero.
- [x] 52. Preserve the native cross-product rotation axes.
- [x] 53. Refuse a degenerate horizontal launch direction.
- [x] 54. Validate the native calculated rotator before mutation.
- [x] 55. Preserve native velocity multiplication by three.
- [x] 56. Preserve native vertical velocity addition of 100.
- [x] 57. Refuse non-finite launch velocity and intermediate axes.
- [x] 58. Prepare all required launch functions/frames before changing physics; recheck source points after callback-capable preparation.
- [x] 59. Validate rotation parameters and disable physics teleport as natively authored.
- [x] 60. Honor the native rotation call's boolean refusal.
- [x] 61. Clear the captured native timer before the impulse.
- [x] 62. Copy back the cleared handle only while the captured timer still owns that field.
- [x] 63. Replace velocity rather than add to current velocity.
- [x] 64. Preserve native BoneName None.
- [x] 65. Run native graph tail 212 for Audio, Sphere collision, h, light, event accounting and the three-second continuation.
- [x] 66. Publish remote activation through existing LlamaState and once-only client cues.
- [x] 67. Open the existing host prop coasting stream after remote launch admission.
- [x] 68. Open that motion stream after the native local-host jump as well.
- [x] 69. Consume admitted/uncertain one-shot launches without another impulse.
- [x] 70. Retry preparation refusals on a later poll.
- [x] 71. Recognize a replacement native timer as a new activation.
- [x] 72. Reserve admission before callbacks and refuse nested selection/native jump checks.
- [x] 73. Refuse queued jump/check delegates from a consumed timer.
- [x] 74. Bound activation records to 256 and retire them on source/session/start/world replacement.
- [x] 75. Keep the native 500 ms poll cadence and recover clock rollback.

Verification: 75 numbered focused cases across actual target controller, native
wrapper/ParamFrame and state-controller integration. Engine/registry/transport/
object slots and clock boundaries are doubled. Wider qualification is in TESTLOG.
- [ ] Accept rendered remote/local triggering, direction, physics streaming,
  sound/light, equal-distance/obstructed players, held props, join/reconnect and
  save/reload with identical DLLs. Marker/save/event-count lifecycle and
  pre-session timer restoration remain open. Native effects already admitted
  cannot be rolled back; uncertain completion consumes that timer's one-shot.
  Full regression/package and the wider Console/events category remain open.

### Earlier implementation — TODO Console/events: 25 llama jump/light/audio fixes (26 October)

Keep 0.9.2161 / b2217 / protocol 2217. Follow the llama part of Console/events.
Use the existing prop lane for motion and a dedicated host-only LlamaState (253)
for h/light state and an expiring sound edge. Development DLLs must match.

- [x] Sample the canonical llama event state on the host.
- [x] Refuse independent client jump/check/timer-resume and destroy bookkeeping bodies.
- [x] Preserve base prop BeginPlay while parking its timer setting only for that invocation; restore the original setting afterward.
- [x] Bind sampling/application to current world/key and exclude hand-axis actors.
- [x] Validate the graph EntryPoint field, exact size/span and current function.
- [x] Capture host jump PRE/POST with exact invocation, frame, function, source and key ownership.
- [x] Consume a dispatched sound once, including uncertain native completion.
- [x] Apply the canonical h bit and PointLight visibility without native jump replay.
- [x] Preserve neighboring h/yumpskare flag bits and original timer-setting value.
- [x] Require typed current PointLightComponent and AudioComponent fields.
- [x] Validate effect-function identities and exact visibility/Activate parameters.
- [x] Hold source/component/class bindings around native effects and callbacks.
- [x] Keep failed native state reads from overwriting caller output.
- [x] Bound/deduplicate collected actors, keyed rows and pending invocation captures.
- [x] Retire state on same-pointer session restart, running/role/link or host-connection replacement.
- [x] Retire state on world-pointer/generation/readiness changes.
- [x] Preserve newer nested source samples with per-key sampling revisions.
- [x] Own received bytes and validate host sender, key, booleans and lifetime shape.
- [x] Bind outbound debt to recipient person, connection and join revision.
- [x] Retain debt through temporary recipient world/lane readiness loss.
- [x] Own each outgoing packet and preserve requeued debt across send callbacks.
- [x] Retain client state while its announced world actor/components become ready.
- [x] Handle sequence/pulse wrap and reject stale/duplicate packets or repeated cues.
- [x] Bound sound debt with remaining lifetime, clock-rollback retirement and quiet join seeds.
- [x] Wire host authorship, normal lane, dispatcher and all session lifecycle callbacks.

PASS 17 actual-controller groups at 32 peers and eight actual-wrapper/ParamFrame
groups; all 25 item numbers are covered. Cooked contracts verify native graph,
base initialization, jump/physics, light/audio, event-count and console dependencies.
Current combined qualification is recorded in TESTLOG.
- [ ] Accept rendered host/client llama jump and ordinary prop motion, sound/light,
  late join/reconnect, held-item transitions and save/world reload. The llamaSoul
  marker/save/event-count lifecycle and restoration of pre-session timers remain
  open. Remote-player trigger selection is implemented by the 27 October batch above.
  Lifetime bounds are local send/receive/admission bounds, not a synchronized
  audio clock or proof of maximum reliable-transport delay. Full regression/package
  and the wider Console/events category remain open.

### Earlier implementation — TODO Console/events: 25 Rufus/thiccfus birth and pose fixes (26 October)

Keep 0.9.2161 / b2217 / protocol 2217. Follow the first open Console/events family
with verified native rufus_C -> Character, thiccfus_C -> rufus_C, and a separate
prop_thiccfus_C plush. This is birth/pose and mirror authority, not complete gameplay.

- [x] Add rufus_C as a late NPC base without delaying lane installation.
- [x] Recognize inherited thiccfus_C on first load while excluding prop_thiccfus_C.
- [x] Catch console deferred births using the exact permitted output classes.
- [x] Hold queued output object/class slot serials rather than bare index liveness.
- [x] Bind queued births to session/start serial, host/running state and world identity.
- [x] Deduplicate pending births and remove retired debt before consuming queue capacity.
- [x] Validate finite location/rotation/scale and current actor before enrollment.
- [x] Stream the actual Rufus mesh world yaw through the existing body-yaw field.
- [x] Apply the host mesh yaw on Rufus/thiccfus mirrors.
- [x] Refuse mirror BeginPlay's independent AI and per-player HUD creation.
- [x] Refuse mirror Tick's local-player repositioning and camera-dependent decisions.
- [x] Refuse mirror AI delegates and latent graph continuations.
- [x] Refuse mirror-local damage and explosion gameplay execution.
- [x] Refuse mirror corpse/plush child production and destructive gameplay branches.
- [x] Keep unmarked host/local actors' native graph behavior unchanged.
- [x] Require a live class-scoped graph guard before BeginDeferred and FinishSpawning.
- [x] Require guard admission before adoption; undo newly added marks after refused binding.
- [x] Restore native Mesh -> SpringArm KeepRelative attachment without HUD/AI replay.
- [x] Hold actor, mesh, arm, AnimBP and class bindings throughout preparation.
- [x] Validate attachment function identity and every parameter/return span.
- [x] Honor attachment refusal and retry presentation without repeating known attachment.
- [x] Set the matching animation instance's native pawn reference to the mirror actor.
- [x] Retry missing/foreign AnimBP preparation via pose dirtiness, even while stationary.
- [x] Reject failed/non-finite/replaced yaw reads without overwriting the caller's output.
- [x] Preserve mesh pitch/roll and honor checked rotation-write failures.

PASS 25 focused actual-wrapper/catch/drain/enroll/adoption/ParamFrame groups, current
cooked contracts and actual NPC pose retry regression. Current qualification is in
TESTLOG. Engine, registry, transport and script-gate dispatch boundaries are doubled.
- [ ] Accept rendered console birth, both models, motion/animation/body yaw, join/
  rejoin and destruction. Cross-player targeting/grab outcomes, audio/HUD/event
  presentation, corpse mesh/loot and damage/explosion feedback transport remain open.
  Adoption does not undo effects that ran before marking. Native scale reads retain
  the engine helper's unit-scale failure fallback. Full regression/package remain open.

### Earlier implementation — TODO Console/events: 25 Maxwell native playback fixes (26 October)

Follow the open Maxwell-phase subtask in Console/events. Keep 0.9.2161 / b2217 /
protocol 2217. These boundaries protect native music/squeak presentation; they do
not complete Rufus/thiccfus, llama, grunt AI/damage/effects or rendered acceptance.

- [x] Refuse null/zero-capacity collection buffers and respect their bounds.
- [x] Retire collected results when the world pointer/generation/readiness changes.
- [x] Deduplicate the same toy returned by several indexed variant classes.
- [x] Hold the toy's exact current class through sampling and native operations.
- [x] Require a current AudioComponent before reading or dispatching toy audio.
- [x] Retain the exact toy-field/component binding across native queries.
- [x] Revalidate isUsed's class, offset and mask while preserving adjacent bits.
- [x] Publish music state only when used/volume/pitch remain coherent after IsPlaying.
- [x] Reject malformed present pitch instead of silently substituting a guessed rate.
- [x] Leave the caller's squeak-pitch output intact after failed or retired reads.
- [x] Revalidate the exact Sound field/component/resource relationship for clock reads.
- [x] Hold SoundWave object/class incarnations through clock preparation and readback.
- [x] Read duration, looping and pitch coherently, including callback-side changes.
- [x] Validate current IsPlaying binding and its exact one-byte return before dispatch.
- [x] Copy incoming State before native callbacks can overwrite caller-owned data.
- [x] Validate complete phase/rate/volume shape before changing native fields.
- [x] Validate volume function identity and exact float parameter around dispatch.
- [x] Validate pitch function identity and exact float parameter around dispatch.
- [x] Validate Play identity and exact StartTime parameter before a seek/start.
- [x] Require unchanged duration/rate/resource before and after native phase seeking.
- [x] Bind Stop to the captured function, component, class and world.
- [x] Bind touch to the captured toy/function and refuse stale completion.
- [x] Prepare ToggleActive before touch and revalidate that exact binding afterward.
- [x] Guard squeak pitch/Activate preparation while consuming an admitted one-shot once.
- [x] Preserve nested newer Playback state with invocation revisions.

PASS 25 actual-wrapper/real-ParamFrame groups in run_maxwell_native, plus the
existing toy music/phase/lifetime regression. Current Release and combined checks
are recorded in TESTLOG. Engine/reflection and object-slot boundaries are doubled.
- [ ] Accept rendered host/client Maxwell/variant phase, native schemas, pause/
  restart, world reload and late-join timing with identical DLLs. Optional phase
  still requires a known looping SoundWave; no seek is guessed for other sources.
  Already admitted native effects cannot be rolled back. Full regression/package
  and the broader Console/events category remain open.

### Earlier implementation — TODO Console/events: 25 Madness recovery fixes (26 October)

Follow the first open Remaining implementation category, Console/events,
specifically its named Madness playback-recovery subtask. Keep 0.9.2161 / b2217 /
protocol 2217. This does not close Rufus/thiccfus, llama or grunt AI/effects.

- [x] Retire Madness state on a same-address session restart using the start serial.
- [x] Bind state to running/connected role, world pointer and generation; clear retired presentation.
- [x] Recover polling and age compensation after clock rollback without unsigned underflow.
- [x] Validate coherent active/HUD/music/score/time snapshots before host publication.
- [x] Hold the master and its class through sampling; re-resolve a master whose class binding retired.
- [x] Apply network age/transit compensation only while active music is playing; bound extrapolation.
- [x] Capture recipient person and transport generation when queuing a send.
- [x] Retain send debt through temporary world/lane readiness loss.
- [x] Cancel departed/replaced/unknown-recipient debt before sending.
- [x] Own a separate outgoing packet for each recipient through callback-capable transport.
- [x] Preserve newer same-person join debt using per-recipient revisions.
- [x] Finish retained cleanup before starting a different event; keep partial current-event presentation across preparation retries.
- [x] Read fields through current object/class/layout bindings and support the actual master lineage.
- [x] Retain exact master-to-HUD and count-source identity through native reads.
- [x] Continue reading active HUD/score when the known music field is temporarily null.
- [x] Hold IsPlaying/age function identity and validate exact return spans before dispatch/readback.
- [x] Validate widget factory/CDO/class and the typed returned widget before adoption.
- [x] Retain a created widget through viewport refusal and avoid repeated AddToViewport after success.
- [x] Validate the returned audio component; recover stopped/dead audio without restarting unchanged live music.
- [x] Hold original world/context/factory dependencies through widget/audio preparation and completion.
- [x] Refuse malformed factory-return layouts before allocation; check exact GetRaw results and retain typed uncertain completions.
- [x] Retain text_count/class binding and write matching text/count directly, without addCount's enemy-spawn side effect.
- [x] Stop/remove only the captured owned component and class incarnation.
- [x] Keep fresh nested View ownership through older Apply/Clear completion using monotonic revisions.
- [x] Retain refused Stop/RemoveFromParent cleanup for a later retry while the controller owns the View.

PASS 25 focused runtime groups using actual controller/wrapper/ParamFrame,
previous console/Madness regressions and cooked native dependencies. Combined
qualification and hashes are recorded in TESTLOG.
- [ ] Rendered host/client Madness music seek, pause/resume, HUD/score, late join,
  reconnect, event replacement and viewport readiness. Exact audio sample timing,
  enemy/variant AI, damage and effects remain unverified. Unknown native outcomes
  cannot be rolled back; failed cleanup after final controller teardown has no
  retained owner. The broader Console/events category, full regression and package
  qualification remain open.

### Earlier implementation — 25 host companion lifecycle fixes (26 October)

Keep 0.9.2161 / b2217 / protocol 2217. The original one-ATV-for-everyone report
remains open; the host controller now uses a private checked lifecycle module.

- [x] Reset companion state on world pointer/generation change.
- [x] Bind creation/publication to the running connected host session and start serial; exclude old-session disconnect cleanup.
- [x] Keep minted actor/class identity in checked object references.
- [x] Keep settling source identity by slot serial instead of raw pointer equality.
- [x] Refuse non-finite source position and all three rotation axes.
- [x] Restart the two-second stillness window after movement exceeding 2 cm.
- [x] Restart stillness after pitch/yaw/roll changes exceeding one degree, using wrapped angles.
- [x] Recover clock rollback and support a stillness window beginning at time zero.
- [x] Reset stillness on unknown, zero, two or more live ATVs.
- [x] Require a new stillness window after riding or carrying the source.
- [x] Recheck source identity and final pose after parking/count/occupancy preparation; snapshot candidate tokens before native reads.
- [x] Recount the world after parking search and before minting.
- [x] Scope parking-search and announcement timestamps to the current lifecycle.
- [x] Permit the sole persisted coopatv-key rig to serve as a source.
- [x] Give a second saved rig five seconds to appear without letting stale save counts stall forever.
- [x] Prevent nested TrySpawnCompanion from creating a second simultaneous candidate.
- [x] Validate the returned rig's class/incarnation/world and leave foreign returns untouched.
- [x] Remove a third candidate without setting a permanent spawn-success latch; allow a later one-ATV world to retry.
- [x] Retain a refused live-controller destroy and retry cleanup without minting another body.
- [x] Remove only a still-matching owned index row after successful destruction.
- [x] Never send a blanket key destroy for a foreign or missing index row.
- [x] Validate bounded ASCII save keys and exact readback while holding the minted actor through publication.
- [x] Preserve existing seat/syncer/clock state when publishing an already indexed instance.
- [x] Rebind only an owned source row into an available target, recheck native completion and move the freshest index state/world stamp.
- [x] Retain refused spawn-announcement debt and retry with a 250 ms ceiling while preserving newer scope/state.

PASS 25 real-lifecycle groups with engine/count/save/transport boundaries doubled;
the production ATV controller compiles. Combined source/DLL qualification is in
TESTLOG. No rendered or save-file acceptance is implied.
- [ ] Reproduce the original garage failure on host and clients, verify two rigs,
  late saved-body arrival, save/reload, reconnect, no third rig and exact garage
  parking. Existing native key reservation still refuses occupied save names;
  stale save records are never overwritten. Failed disconnect cleanup and already
  accepted native effects are not rolled back/retried after teardown. Full
  regression/package qualification remain open.

### Earlier implementation — 25 seat presentation fixes (26 October)

Owner changes continuation size to **25 fixes**; AGENTS and agent/README now
match. Keep 0.9.2161 / b2217 / protocol 2217. No package/version change.

- [x] Defer NoBody/NoTarget chair admission and apply when the first posed sender/target becomes readable.
- [x] Bound body anchors to 450 cm of the named chair using overflow-safe distance arithmetic.
- [x] Retain exact current-world chair ownership through reads and authorization.
- [x] Hold local pawn/class identity and confirm an unchanged local seat across occupancy reads.
- [x] Restore tracked local/remote mesh shifts on scope retirement; preserve a fresh nested session during cleanup.
- [x] Recover a replaced AnimBP base class through the current object index instead of a raw static latch.
- [x] Read/write isOnAtv with its reflected bool mask, preserving adjacent bits.
- [x] Write useLegIK through its separate reflected bool mask.
- [x] Resolve a current mesh setter on the component class and require every frame argument before dispatch.
- [x] Reject non-finite model dimensions, forward vectors and shifts; use a bounded capsule-height fallback.
- [x] Capture a finite authored mesh base only while component/class/parent identity remains current.
- [x] Transform shifts through the complete parent rotation and retain vertical forward motion on pitched seats.
- [x] Retain authored bases across refused/partial writes and retry restoration without erasing nested shifts.
- [x] Expand mesh-shift capacity from 16 to all 32 player bodies.
- [x] Follow streamed seated body yaw directly, bypassing standing head-lead hysteresis.
- [x] Refresh head look from the final seated body/head location after chair or ATV placement.
- [x] Refresh flashlight world aim after seat tilt/rotation, retaining the current pawn/arm binding.
- [x] Remove the raw flashlight-rotation fallback; recover checked world-rotation functions and require all arguments.
- [x] Hold ATV actor/class/world identity through driver-pose preparation and completion.
- [x] Retain the exact ATV seat component/class binding through both transform reads and apply.
- [x] Require successful finite position/rotation reads for all six driver-pose fields.
- [x] Refuse fallback driver placement after a failed ATV rotation read.
- [x] Validate the finite, nonzero bounded fallback forward vector before calculating the seat point.
- [x] Stop subsequent driver writes on native refusal, unknown/replaced person, transport/slot/pawn or scope change.
- [x] Cancel the old chair anchor when the authoritative ATV seat table takes ownership of that player's placement.

PASS 25 new groups with production controller/wrapper/ParamFrame and extracted
view/rotation methods, 32-player mesh-capacity and driver transition cases.
Compiled removal controls and combined qualification are recorded in TESTLOG.
- [ ] Rendered host/client chair/ATV alignment on ordinary/custom models,
  head/flashlight direction, banked/pitched vehicles, standing/IK recovery,
  moving chairs, late joins/reconnect and 32-player load. Live reflection schemas
  and engine-native completion remain unmeasured. The zero-return relative-location
  getter cannot distinguish a failed read from an authored zero. Accepted native
  effects cannot be rolled back. Original second ATV reproduction, full regression
  and package qualification remain open.

### Earlier implementation — ten chair/body rotation fixes (26 October)

Keep 0.9.2161 / b2217 / protocol 2217. Owner explicitly requests synchronised
rotation of the player seated in a chair. No wire layout or version changes.

- [x] Send small body translations/turns at up to 20 Hz: replace the 40 cm movement threshold with 1 cm and use a 0.5 degree turn threshold.
- [x] Detect pitch/yaw/roll changes of the chair, including wrapped angles; retain the one-second heartbeat and refused-send retry.
- [x] Interpolate body position/yaw and chair angles together over 50 ms using shortest arcs; rebase continuously and apply after prop presentation.
- [x] Treat OFF as anchor cancellation only; never rotate the chair from a stand-up packet.
- [x] Validate the named chair, exact key, host reach and local occupancy before admitting a body anchor.
- [x] Retain a pending named chair and retry lookup before applying the body; wait for a missing puppet.
- [x] Retire anchors across world/session/role, roster-person or transport replacement, dead/reused chair and heartbeat expiry.
- [x] Own each outgoing/incoming packet and validate finite pose, bounded ASCII key, pawn/chair/class incarnation and exact current seat during capture.
- [x] Release chair placement through stand/laptop/sleep/ragdoll/death transitions; share the actual RemotePlayer sitting predicate with presentation.
- [x] Preserve fresh state through old send completion: check revision/session/pawn/seat/class and suppress nested Tick drains.

PASS ten current-controller groups at 32 slots and ten compiled removal controls.
The real RemotePlayer IsSitting predicate is extracted unchanged. Current cooked
assets confirm parent-chair sittingOn attachment and clearing on unsit. Combined
qualification and hashes are recorded in TESTLOG; engine/transport boundaries
remain doubled in the controller fixture.
- [ ] Rendered host/client chair spinning, model/mesh/head/flashlight alignment,
  late join/reconnect, moving chairs, normal stand/laptop/sleep/ragdoll transitions
  and ATV driver/passenger behavior. Pre-pose admission now waits in the latest
  continuation instead of discarding the packet. Accepted native writes
  and sends cannot be rolled back. Original missing second ATV, full regression
  and package qualification remain open.

### Earlier implementation — ten ATV spawn/placement fixes (26 October)

Keep 0.9.2161 / b2217 / protocol 2217. The owner's report remains that only one
ATV appears for everyone; these code checks do not establish its live cause.

- [x] Reject non-finite spawn location/rotation before deferred actor creation.
- [x] Resolve a fresh checked GameplayStatics class/CDO/begin/finish tuple for each spawn; detect retired or replaced bindings.
- [x] Freeze requested class/key/pose and hold class/base/key-offset identity through FName conversion and completion.
- [x] Fence deferred spawning by the captured world generation, world context and context class.
- [x] Validate the deferred return's class/world before writing its raw key; leave foreign returns untouched.
- [x] Hold deferred actor/class incarnation through finish preparation, dispatch and completion.
- [x] Clean up only the captured actor incarnation/class in the original world/generation; preserve replacements.
- [x] Fence TeleportRig by finite pose and current actor/class/function/world through preparation and completion.
- [x] Refuse publication and clean up an owned rig when its whole-rig teleport fails.
- [x] Hold source ATV identity through parking queries, stop immediately on retirement and refuse a changed source pose without publishing output.

PASS ten spawn/parking fixture groups and ten compiled removal controls, spawn
argument/finish recovery and existing mount/wall/pit/tilt regressions. Current
combined source/DLL qualification and exact hashes are recorded in TESTLOG.
- [ ] Reproduce host/client garage creation in the game: confirm two complete
  rigs, first join, reconnect/reload, riding/grabbing and preserved save behavior.
  Live reflection schemas, real garage traces and rendered wheel placement remain
  unverified. Unknown deferred returns cannot be identified or safely rolled back;
  already dispatched effects cannot be undone. Full regression/package remain open.

### Earlier implementation — ten lamp/ambient/switch-audio fixes (26 October)

Keep 0.9.2161 / b2217 / protocol 2217. Continue the remaining native presentation
boundaries from the previous lighting audit.

- [x] Capture lamp actor/class/function/world identity before frame preparation; reject stale preparation/completion.
- [x] Capture ambient actor/class/function/world identity before component/frame preparation.
- [x] Hold exact bottomLight/component/class binding through reads, apply preparation and completion; leave failed read output unchanged.
- [x] Recover replaced sound-function metadata and verify the exact hooked callee with a scoped native-hook accessor.
- [x] Bound cue/volume/pitch parameter spans before installing or reading the sound hook.
- [x] Copy cue/volume/pitch before callback-capable engine work.
- [x] Replace the raw attenuation latch with checked resource/class references and recover replacement/readiness loss.
- [x] Reject non-finite/invalid audio multipliers and non-finite positions before spatial playback.
- [x] Retain replay-source, cue, attenuation, function and world currency through location preparation.
- [x] Guard attenuation construction against reentry and restore exact enclosing replay identity after nested use.

PASS ten new presentation groups and ten compiled removal controls, previous ten
native-lighting groups/controls, actual native-hook PRE/POST callee/cleanup tests,
and lamp/ambient/cooking regression. The current suite includes
run_light_presentation. Combined qualification and exact hashes are in TESTLOG.
- [ ] Rendered multiplayer lamp/ambient/flicker/switch audio and joins/reloads;
  original garage-companion reproduction, full regression and package qualification.
  Inner engine audio-dispatch preparation/schema is not established by these
  boundary tests. Native side effects already dispatched cannot be rolled back.

### Earlier implementation — ten native lighting fixes (26 October)

Development stays 0.9.2161 / b2217 / protocol 2217. Continue the lighting audit
through the real root/switch wrapper and ParamFrame, without changing versions.

- [x] Re-resolve light-root metadata after partial load, class/function replacement or serial reuse; require runTrigger before readiness.
- [x] Re-resolve switch metadata after class/use-function replacement or serial reuse.
- [x] Refuse missing root Key/live-state fields instead of substituting fixed offsets.
- [x] Refuse missing switch Key/A/objects fields instead of substituting fixed offsets.
- [x] Read the root's live isActive through its reflected bool byte/mask.
- [x] Read the switch's A through its reflected bool byte/mask.
- [x] Read the group's active breaker through its own bool byte/mask, separate from live state.
- [x] Validate linked-array counts/capacity and retain source/header/entry/target identity during SwitchRoot lookup.
- [x] Check all runTrigger/breaker arguments and hold actor/class/function/world/layout through native group calls.
- [x] Hold switch/class/function/world/layout through replay-frame and sound preparation and native use completion.

PASS ten real-wrapper/ParamFrame groups and ten compiled removal controls.
Lamp/ambient and current cooking/wrapper regressions pass. Existing replay sound
and nested-switch tests now resolve real switch metadata instead of injecting a
private raw function pointer. Current combined qualification is recorded in
TESTLOG and build/qualification-current; source/DLL identity is checked there.
- [ ] Rendered switch/group/breaker/flicker behavior, world reload and host/client
  joins; full regression and package qualification. Accepted native calls cannot
  be rolled back, and a false completion may already have side effects. Named
  sound-hook/resource and lamp/ambient preparation fixes are implemented above;
  inner engine audio-dispatch boundaries remain open.

### Earlier implementation — ten flashlight join-delivery fixes (26 October)

Keep public 0.9.2161 / b2217 / protocol 2217. Close the previously open targeted
flashlight replay retry work; rendered acceptance remains separate.

- [x] Retain a refused local-player seed per recipient instead of relying on broadcast dedup reset.
- [x] Retain refused host cache relays with the original sender-slot attribution.
- [x] Sample current local state again after missing pawn/component or transport readiness.
- [x] Replay OFF as well as ON to clear stale seeded light.
- [x] Bind each recipient to its captured person and connection; cancel departure/reuse debt.
- [x] Cancel relays for a departed/replaced/disconnected author; require known author connection identity.
- [x] Own replay packet bytes through callback-capable sends instead of lending mutable cache storage.
- [x] Preserve newer enqueue/cache/publication revisions through older send completion and nested ticks.
- [x] Drain all targeted seeds/relays through one rotating 64-attempt-per-tick budget.
- [x] Gate replay by live session/world/role, allowed target and link/lane/world readiness.

PASS ten production-controller groups at 32-slot capacity and ten compiled
removal controls. The stress case retains 961 debts across refusal, caps each
tick at 64 attempts and finishes without duplicate recipient/origin pairs.
Previous flashlight state/lifetime cases and six controls also pass. Combined
Release/ABI, public label and selected related-group evidence is recorded in
qualification-current; TESTLOG describes the exact snapshot and limits.
- [ ] Rendered host/client late join/reconnect, battery-empty/OFF/mode changes,
  departure/slot reuse and reliable backpressure. Unknown recipient identities
  are refused until a new valid queue request. Admission is not atomic with
  net-thread teardown; no delivery rollback or retry after accepted transport is
  claimed. Full regression and package qualification remain open.

### Earlier audit — ATV creation, lights and console (26 October)

Owner reports that only one ATV appears for everyone and requests flashlight,
console and light-synchronisation checks. Keep 0.9.2161 / b2217 / protocol 2217.

- [x] Retry incomplete ATV hit-delegate resolution/registration once a second.
- [x] Ensure the hit guard on subsequent pump installs, outside the one-time index/subscription latch.
- [x] Require each NormalImpulse vector to fit its own live parameter frame before registration.
- [x] Retry rejected honk and mount watches independently without duplicating accepted watches.
- [x] Retry initially refused light-group runTrigger registration instead of disabling it permanently.
- [x] Stop client light blocking and host light capture when the session is no longer running.
- [x] Check all deferred ATV spawn arguments before dispatching BeginDeferredActorSpawnFromClass.
- [x] Check finish arguments and exact returned actor; destroy failed deferred bodies before publication.
- [x] Refuse teleportVehicle dispatch when location or rotation argument preparation fails.
- [x] Refuse non-finite yaw before selecting a companion parking location.
- [x] Additional audit repair: remove merged duplicate console declarations, statements and nested panel loop; restore compilation with the lifetime checks retained.

PASS 11 selected current groups, combined Release/ABI and production public-label
formatter. Console: 21 callbacks, 14 delivery cases, ten native groups and 23
compiled removal controls. Flashlight state: six removal controls; remote click:
ten removal controls and production compilation. Restore the flashlight fixture's
single definitions and remove its duplicate lifetime run. Update the obsolete
2149 mount fixture to current syncer/stamped-seat queries; it does not test host
authority or the deleted AcceptIncomingOccupancy model. Register ATV recovery,
placement, lamp and cooked-console checks in the current suite.
Evidence: `build/qualification-current/verification.json` and `audit-extra.json`.
- [ ] Reproduce the reported missing companion in-game, then confirm two ATVs
  beside the garage's original ATV on host and clients, no third spawn after
  reconnect, and preservation of an already saved second ATV's position.
  The permanent hit-guard failure is a confirmed blocker; it is not yet proven
  to be the cause of the owner's particular save/gameplay report.
- [ ] Rendered flashlight beam/mode/battery/late-join, group/lamp/ambient flicker,
  console concurrent authors/clear/history/reconnect; full regression and package
  qualification remain open. Targeted flashlight join retry is implemented above.
  This audit does not establish complete metadata/session lifetime
  for every native spawn helper. No archive, install, commit or version bump.

### Earlier implementation — ten console native-operation fixes

The user assigned this continuation to the console and left flashlight work to
the other session. Public version/protocol remain 0.9.2161 / 2217.

- [x] Reject cached layouts whose live class/function/CDO bindings were replaced.
- [x] Validate the atlas class, incarnation and exact visible-terminal binding.
- [x] Own factory/context/class dependencies and retire a typed candidate from a stale factory completion.
- [x] Hold terminal/function/dish identity through native init preparation and dispatch.
- [x] Recheck terminal/world and the written command before invoking enterCommand.
- [x] Guard writeToLog preparation, text length and bracket type before native calls.
- [x] Hold the text-box class through FText/frame preparation and input clearing.
- [x] Resolve both log fields before clearing; stop further writes after retirement.
- [x] Bound parameter-header/type reads by the live function frame and publish output arguments together.
- [x] Validate desk class and bounded TArray header, retaining exact array/candidate identity during panel search.

PASS ten new native groups, existing input/metadata tests, 21 callback scenarios,
14 delivery scenarios and 23 compiled removal controls (ten new). Current cooked
ui_console proof confirms clr clears consoleLine and logText together.
Both native wrapper and controller compile with production headers. Console-only
source hashes/logs: `build/sat-console-callbacks-check/console-verification.json`;
this continuation does not overwrite the shared DLL or earlier Release report.
- [ ] Rendered concurrent authors/clear/history/reconnect acceptance and a stable
  combined Release/full-regression/package qualification. Native preparation
  checks do not establish atomic rollback, full reflected property/struct schemas,
  session predicates inside every native helper or at-most-once retry after an
  uncertain native call. The last qualified DLL describes an earlier snapshot.

### Implemented — ten console metadata and flashlight lifetime fixes

Version remains 0.9.2161 / b2217. Four completed console fixes:
- [x] Retry delayed console metadata after five incomplete resolution passes.
- [x] Reset every layout/parameter offset before a new resolution attempt.
- [x] Validate all eleven class/function/CDO dependencies before using the cached layout; re-resolve replacements.
- [x] Bound FString lengths and validate header capacity, storage and terminator before console reads.

PASS 21 console controller scenarios, the actual native input wrapper with four new
regression groups, thirteen compiled removal controls, and a production native-wrapper
compile. The owner clarified there is no other writer and authorised reconciling the
observed Seat/Scope edits. The file now uses one SessionScope and one PublishState path.

Six completed flashlight fixes, completing this continuation's ten items:
- [x] Reconcile session/world/role/running/link lifetime.
- [x] Bind deferred states and the host replay cache to the original player and connection.
- [x] Require exact registry actor-to-slot association.
- [x] Copy received payloads before callback-capable engine work.
- [x] Recheck source identity before the first light setter and after binding getters.
- [x] Protect the publication signature from stale send completion; retry current state after an older send finishes late.

PASS six new flashlight scenarios and six compiled removal controls. The debug
toggle now samples/publishes on the game thread through the same publication path.
PASS five selected current groups, Release/ABI and production version formatter;
evidence: `build/qualification-current/verification.json`. Public version remains
0.9.2161; internal b2217/protocol 2217. No new archive or normal profile installation.
- [ ] Rendered two-player flashlight/console acceptance, full regression and package
  qualification for this snapshot. Targeted host late-join replay retry is now
  implemented in the join-delivery continuation above.

### Implemented — flashlight and console priority continuation (26 October)

Owner prioritises flashlight operation and sharing each player's submitted console
text. Development remains 0.9.2161 / b2217 / protocol 2217. The planned and completed
ten fixes are:

- [x] Restrict flashlight publication to the registry's exact local player.
- [x] Reject flashlight snapshots retired during player/component reads.
- [x] Reject invalid flashlight state, non-finite intensity and invalid cone ranges before applying/caching.
- [x] Stop older flashlight setters when a newer per-slot state arrives.
- [x] Preserve enclosing echo suppression through nested flashlight application.
- [x] Match console POST by invocation, function incarnation, stack, locals and tag.
- [x] Copy console body attribution before callback-capable checks can change the frame vector.
- [x] Refuse oversized shared commands without consuming input or executing a truncated command.
- [x] Refuse unreadable/invalid console output instead of allowing an unsynchronised local write.
- [x] Republish unchanged flashlight state after local pawn replacement, including same-address reincarnation.

PASS five selected current validation groups, Release/ABI and production version
formatter; 21 console callback scenarios, nine compiled removal controls and native
input wrapper; flashlight source/component/setter regression and ten click groups.
Evidence: `build/qualification-current/verification.json`. This is focused evidence.
- [ ] Rendered multiplayer flashlight beam/mode/battery/joins and simultaneous
  console authors/clear/history/reconnect acceptance; full regression and package
  qualification for this snapshot. Submitted console output is shared; live
  unfinished keystrokes are not added by this continuation. No new archive/install.

### Implemented — ten remote flashlight-click fixes

- [x] Refuse invalid peer slots before per-slot state or audio preparation.
- [x] Refuse local/foreign/unregistered actors and keep exact registry slot association through preparation.
- [x] Own puppet/class/world identity through resource and position reads.
- [x] Retire the dedup latch on session serial, world, person and puppet incarnation changes.
- [x] Keep preparation failures retryable on a subsequent apply; commit only before the one native attempt.
- [x] Replace the raw sound latch with live object/class references and recover replaced assets.
- [x] Replace the raw attenuation latch with live object/class references and recover replaced runtime attenuation.
- [x] Reserve a per-slot revision during preparation; nested different states supersede older work and same-state repeats cannot duplicate it.
- [x] Refuse non-finite puppet positions without consuming the state transition.
- [x] Wait for spatial attenuation instead of dispatching an unattenuated map-wide click.

PASS ten new lifetime/readiness/reentry groups at 32-slot capacity, ten compiled
removal controls and both production TUs. The shared attenuation retains its native
one-third sphere/falloff tuning; the original local-player click is untouched.
PASS 3/3 selected related groups, Release/ABI and the production 2161 formatter;
current focused evidence: `build/qualification-current/verification.json`.
- [ ] Rendered remote toggles/mode changes/joins, full regression and new package
  qualification. Preparation can retry only on a subsequent apply. Native dispatch
  is a void wrapper: admission is one attempt, not proof of audible completion;
  internal reflected-call preparation lifetime remains outside these checks.

### Implemented — ten held-toy audio fixes

- [x] Retire work across session serial, world, role, running and link changes.
- [x] Fence deferred music/pulses by sender person, transport generation and peer epoch.
- [x] Bind recipient delivery to the exact person/connection/peer epoch.
- [x] Freeze hand actor, person, generation and action/Alt choice before native use.
- [x] Match exact PRE/POST invocation/function/stack/tag; reserve bounded ownership and prune skipped POST.
- [x] Admit bounded pulses before committing receive deduplication or charging the host rate window.
- [x] Copy incoming packets before reconciliation/native cleanup can overwrite decode storage.
- [x] Share a 64-send-attempt budget across transient pulses and persistent music per tick.
- [x] Check strict transient expiry across callbacks and retire work on clock rollback without resetting receive watermarks.
- [x] Complete a persistent delivery only for the captured recipient revision; preserve a fresh join's pending bit.

PASS legacy held/world-toy/contact fixtures, ten new 32-seat lifetime groups,
ten compiled removal controls and seven production-TU compiles. A use observed
with an unknown hand generation is not queued for adoption by a later hand;
the original native local sound still runs. Remote pitch preparation tests now
model the real wrapper's current check before Activate.
PASS 8/8 related groups, Release/ABI and the production 2161 formatter;
current focused evidence: `build/qualification-current/verification.json`.
- [ ] Rendered held-toy music/squeak, initial-hand readiness, reconnect and native
  timing acceptance; full regression/package qualification for this changed snapshot.
  Hook-function/schema lifetime and native presentation/authorization work budgets
  remain separate open implementation boundaries.

### Implemented — ten watoy native-wrapper fixes

- [x] Own object/class/world identity through offset resolution before field reads.
- [x] Fail unknown `a` metadata instead of reporting an invented inactive state.
- [x] Keep the exact actor/audio/zero component bindings through complete state sampling.
- [x] Carry the requester/session predicate into reflected activation preparation/admission.
- [x] Carry presentation currency through audio query/volume/pitch/start and recheck Audio binding.
- [x] Own service CDOs and their classes through effect transform/bounds/spawn work.
- [x] Recheck the original mesh binding and live template through effect preparation.
- [x] Require successful output reads and finite transform, bounds and inverse-location values.
- [x] Admit only live particle-component spawn results and release a stale owned candidate.
- [x] Use View revisions and local candidates so reentrant Apply/Clear preserves fresh ownership.

PASS ten new native-wrapper lifetime groups, ten new compiled removal controls,
existing controller/wrapper and ten controller lifetime groups, ten existing
controller controls and four production-TU compiles.
PASS 7/7 related groups, Release/ABI and the production 2161 formatter;
current focused evidence: `build/qualification-current/verification.json`.
- [ ] Rendered host/client audio/effect acceptance and a new full regression/package
  qualification. Full reflected parameter schema/ownership checks, cleanup across
  independent copied View owners, original hand provenance and native-work budgets
  remain open; these tests do not establish complete native parity.

### Implemented — ten watoy controller fixes

- [x] Retire state across session serial, world, role, running and link changes;
  preserve a fresh owner installed during native view release.
- [x] Match activation POST by exact invocation, live function, actor and stack.
- [x] Reserve bounded PRE capacity before native state reads; prune inactive invocations.
- [x] Freeze requester person/transport/peer epoch for immediate and deferred authorization.
- [x] Bind state/cue recipients to exact person, transport generation and peer epoch.
- [x] Flush owned state snapshots; complete only the matching current row after sends.
- [x] Abort retired drains and sanitize detached retries before restoring queue ownership.
- [x] Prune snapshots and exact row revisions; detach views before native cleanup.
- [x] Admit host/client cues before committing receive sequence watermarks.
- [x] Copy incoming packets before authorization/native reads can overwrite decode storage.

PASS the legacy controller/native wrapper, ten new 32-seat lifetime groups,
ten compiled removal controls and four production-TU compiles. PASS 7/7 related
groups, Release/ABI and the production 2161 formatter; current focused evidence
is `build/qualification-current/verification.json`.
- [ ] Rendered multiplayer acceptance, full regression and qualification of a new
  package for the changed snapshot. Controller tests do not establish complete
  native preparation lifetime or effect/audio parity.

### Implemented — ten world toy music/squeak fixes

- [x] Retire queues on session-start serial, running/link, world and role transitions.
- [x] Freeze requester transport generation before lookup/authorization; reject replacement completion.
- [x] Bind deferred state/pulse recipients to their transport generation as well as person/epoch.
- [x] Match PRE/POST by live function incarnation and exact stack, invocation and actor.
- [x] Reserve bounded PRE capacity before key/Alt reads and commit only the owned reservation.
- [x] Commit pulse receive deduplication only after bounded admission on host and client.
- [x] Copy the incoming packet before callback-capable authorization/native work.
- [x] Share a 64-send-attempt budget across pulses, client intents and host state per tick.
- [x] Recheck transient deadlines across native callbacks; retire pending work on clock rollback.
- [x] Apply the same printable, non-space ASCII key domain to local sampling and wire input.

PASS ten lifetime groups at 32-seat capacity, ten compiled protection-removal controls,
the existing wrapper/phase/clock fixtures and four production-TU compiles.
PASS 7/7 related groups, Release/ABI and the production 2161 label formatter;
current focused evidence: `build/qualification-current/verification.json`.
- [ ] Rendered host/client music/squeak acceptance across reconnect, saturation and teardown.
  Full regression and package qualification for the changed snapshot remain open.

### Implemented — ten sponge-foam fixes

- [x] Retire captures/effects across world, start serial, role, running and link changes.
- [x] Match exact invocation/function/stack/object POST and prune skipped POST records.
- [x] Reserve bounded PRE ownership before callback-capable key/trace/stock reads.
- [x] Freeze the original key/hand generation before native use; never adopt a future hand.
- [x] Recheck actor/class/key and authorization before presentation and relays.
- [x] Fence source departure/replacement by peer epoch and host transport generation.
- [x] Fence recipient delivery by exact person, connection generation and peer epoch.
- [x] Advance receive deduplication only after bounded queue admission succeeds.
- [x] Bound authorizations/presentations to eight and sends to 64 per tick;
  rotate presentation past unready mirrors while preserving wire order, expire
  at 1,500 ms across callbacks and retire clock-rollback work.
- [x] Own actor/player/class/hitResult schema through trace/stock reads and publish
  complete output only while the exact player link and struct binding remain current.

PASS legacy controller/wrapper, ten new lifetime groups, trace replacement cases,
eleven compiled fault controls and four production-TU compiles. PASS 5/5 related
groups, Release/ABI and the production 2161 label formatter. Evidence:
`build/qualification-current/verification.json` (focused, not full qualification).
- [ ] Full regression/package qualification for this changed snapshot and rendered
  host/client foam acceptance, including two authors, hand swaps, joins and teardown.
  These cosmetic effects do not replay cleaning/soap debit or become join history.

## Earlier 2161 continuation — native canvas (b2217)

- [x] Preview and run cleanup before the version bump: 752 files / 535.9 MiB
  removed; retained 2161 DLL/archive hashes match. `package/cleanup-inventory.json`.
- [x] Implement the ten native-canvas fixes below and run focused checks.
- [x] Complete fresh full regression, Release/ABI, menu-label and archive qualification
  for the historical snapshot mistakenly labelled 2162: 95/95 groups, 32 archive
  members and five package rejection controls passed; this is not current foam evidence.

### Implemented — ten native canvas fixes (b2217)

- [x] Replace permanent raw class/function/layout latches with incarnation-owned,
  checked publication; incomplete or replaced native metadata can resolve again.
- [x] Validate live window class and packed cv flag for observation, including
  subclasses; refuse dead/foreign actors and non-finite stroke corners.
- [x] Guard player, held item, sponge stock, material and getter/name preparation
  through brush reads; publish all output scalars only after current proof holds.
- [x] Refuse missing/invalid stock and unreadable dynamic-material values instead
  of inventing defaults or mixing a replacement hand/material with older input.
- [x] Check factory context/parent/output bindings and returned MID type, own the
  candidate pin locally, and reject retired factory/pin completion before publication.
- [x] Detach brush ownership before unroot callbacks; retired teardown and nested
  factory/draw work cannot release or overwrite a fresh owner.
- [x] Read the current gamemode/panels/packed signal flag and wait on unknown state;
  refreshed classes and missing metadata cannot falsely authorize the shared canvas.
- [x] Fence native canvas/scalar preparation by source, actor/class, function/owner,
  canvas and material identity; honor failed scalar calls and reject nested draws.
- [x] Check every frame parameter's live offset/size/bounds and Vector2D type/layout;
  incomplete frames and stale schemas cannot dispatch default or malformed draws.
- [x] Keep a FIFO dab through pre-draw Retry, but consume an admitted native attempt
  once even on uncertain failure; completion owns its exact queued row.

PASS 14 controller + 14 actual-wrapper/ParamFrame scenario groups, 17 compiled
fault controls, cooked canvas open/join/timer/close/draw proof and both production TUs.
- [ ] Accept rendered host/client sponge/mop wiping, temporary native readiness,
  scalar/factory failure recovery, signal transitions, joins/reconnect and teardown.
  Original-dab brush provenance, atomic material-scalar sampling, thrown sponges,
  periodic dynamic grime and full-save parity remain open.

## Previous local release — 0.9.2161

- [x] Establish mandatory cleanup before each new version and reconcile TODO/WORK.
  The 25 October cleanup removes obsolete delivery copies, generated test binaries,
  staging, caches and superseded publication drafts; inventory and retained hashes:
  `package/cleanup-inventory.json`. Current source, tests, dependencies and evidence stay.
- [x] Implement the next selected fixes for 2161 and record their checks here and
  in WORK/TESTLOG, keeping implementation separate from rendered acceptance.
- [x] Prepare and qualify 2161 with the current build/package scripts; update release
  metadata and package descriptions together when preparing its actual build.

2161 uses **b2216 / protocol 2216**. All 95 current-source regression groups pass
after the corrected protocol-test group rerun; no 2160 test results are reused.

### Implemented — ten bay-window stroke fixes (b2216)

- [x] Retire queues on world, session-start serial, role, running and connection
  changes, including reuse of the same Session pointer.
- [x] Retain a refused reliable dab in FIFO order; later dabs cannot overtake it.
- [x] Seal each outgoing dab's brush once at its first drain, retaining the payload
  through backpressure instead of resampling another held brush on retry.
- [x] Bound sends to eight per tick and expire outgoing dabs after 20 seconds.
- [x] Own rows across transport/native callbacks; nested Tick cannot resend a dab
  and old completion cannot erase a freshly installed queue.
- [x] Validate incoming draw ranges, active source and ready host-side origins;
  preserve host-relayed original sender slots on clients.
- [x] Refuse new overflow instead of evicting the oldest admitted inbound dab;
  preserve the 256-row caps and expire parked or ready incoming work.
- [x] Scope the scan/index to world generation and object slot serial, refusing
  same-address replacement and stale scan completion.
- [x] Revalidate source/window and signal playback between draws; own native
  replay depth and retire each admitted attempt once, including uncertain failure.
- [x] Retry failed hook installation/arming, re-resolve retired hook functions
  and refuse observations retired during native corner capture.

PASS 13 controller scenario groups, seven compiled fault controls and the
production TU compile. `run_window_stroke` is part of the full current suite.
- [ ] Accept rendered sponge/mop wiping on both roles, concurrent authors,
  backpressure, signal playback, late join/reconnect and world replacement.
  Native preparation/callback/schema guards are implemented in the 2161 continuation above.
  Thrown sponges, first-drain brush provenance, periodic dynamic grime, full-save
  parity and rendered acceptance remain open.

## Priority acceptance — 0.9.2160

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

Current development: **DLL b2217 / protocol 2217**, game **0.9.0n**, package
**0.9.2161**. Upstream `ad5315611` is merged with all local history,
including native-action/hit-result fixes from `ddc711220`.
Current foam changes have focused checks; related regression/Release evidence is
`build/qualification-current/verification.json`. Earlier complete 95-group results
describe older snapshots. See [TESTLOG.md](TESTLOG.md).
Native blast/ATV evidence was recorded on earlier DLLs. Rendered gameplay and
real keyboard input still need acceptance.

Current package preparation remains open for this changed 2161 snapshot.
Historical 2162 archives/reports retain the erroneous label and are not current delivery.
- [x] Prepare and verify package 0.9.2160, including its public version label and
  shared description; publish README/release descriptions and short About text on GitHub.
- [ ] Prepare/qualify and publish 0.9.2161 only when explicitly requested; reconcile its
  README/manifest/changelog. Earlier GitHub descriptions describe 2160.
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

### Implemented — ten native damage/slice/hit fixes (b2215)

- [x] Capture client actor/class, player/person and function/owner before native
  argument lookup; retired input cannot enqueue a hit against a replacement.
- [x] Refuse reflected client damage/slice echoes and revalidate the current tool
  before submission, preserving host ownership of world mutations.
- [x] Revalidate the receiver's exact world key and current author through damage
  and slice preparation; authorization cannot migrate to a retagged target.
- [x] Recheck the live melee tool and damage allowance after callback-capable
  preparation instead of retaining an old held-item proof.
- [x] Guard inherited damage function/owner, actor/class and parameter binding
  through native dispatch, without reading a retired target for failure logging.
- [x] Bind damage to the current mesh/class and finite position through hit
  construction; metadata and component replacement cannot authorize an old hit.
- [x] Invoke the native actor isNotSawable getter, including overrides; it is a
  function rather than a component field. Refuse failed/unreadable/reentrant
  getter work and preserve source/function/tool currency through slice dispatch.
- [x] Carry caller/source ownership through wall damage metadata, location and
  argument preparation, while preserving native destruction as a valid outcome.
- [x] Check every MakeHitResult binding, exact destination struct, blocking flag,
  weak target and source/function lifetime before writing the owned hit bytes.
- [x] Protect hit-field destination/class/offset/schema and bounds through native
  construction; retry missing/replaced layout metadata and retain caller location.

PASS 23 new scenario groups and ten compiled fault controls, included in the
94-group qualified source snapshot. Native/cooked contracts distinguish inherited
damage, the base no-op slice and the hacksaw's separate mesh-cut/broken path.
Rendered impacts, cutting/fragment replication and full gameplay acceptance stay open.

### Implemented — ten native damage/slice/hit fixes (b2215)

- [x] Capture client actor/class, player/person and function/owner before native
  argument lookup; retired input cannot enqueue a hit against a replacement.
- [x] Refuse reflected client damage/slice echoes and revalidate the current tool
  before submission, preserving host ownership of world mutations.
- [x] Revalidate the receiver's exact world key and current author through damage
  and slice preparation; authorization cannot migrate to a retagged target.
- [x] Recheck the live melee tool and damage allowance after callback-capable
  preparation instead of retaining an old held-item proof.
- [x] Guard inherited damage function/owner, actor/class and parameter binding
  through native dispatch, without reading a retired target for failure logging.
- [x] Bind damage to the current mesh/class and finite position through hit
  construction; metadata and component replacement cannot authorize an old hit.
- [x] Invoke the native actor isNotSawable getter, including overrides; it is a
  function rather than a component field. Refuse failed/unreadable/reentrant
  getter work and preserve source/function/tool currency through slice dispatch.
- [x] Carry caller/source ownership through wall damage metadata, location and
  argument preparation, while preserving native destruction as a valid outcome.
- [x] Check every MakeHitResult binding, exact destination struct, blocking flag,
  weak target and source/function lifetime before writing the owned hit bytes.
- [x] Protect hit-field destination/class/offset/schema and bounds through native
  construction; retry missing/replaced layout metadata and retain caller location.

PASS 23 new scenario groups and ten compiled fault controls, included in the
94-group qualified source snapshot. Native/cooked contracts distinguish inherited
damage, the base no-op slice and the hacksaw's separate mesh-cut/broken path.
Rendered impacts, cutting/fragment replication and full gameplay acceptance stay open.

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

- [x] Console/events — Madness playback recovery: current controller/native
  lifetime, recipient retry, music/HUD recovery and owned cleanup implemented
  above with 25 focused code groups. Rendered acceptance remains open.
- [x] Console/events — Maxwell native phase/recovery boundaries: exact source/class/
  function ownership, coherent clocks, checked seeks and nested Playback recovery
  implemented above with 25 actual-wrapper groups. Rendered timing remains open.
- [x] Console/events — Rufus/thiccfus birth/pose and parked mirror graph: late
  ancestry admission, exact console catches, scoped/de-duplicated enrollment,
  guarded native mesh/AnimBP setup and streamed mesh yaw implemented above.
- [x] Console/events — llama jump/light/audio: host state and expiring sound edge,
  client graph authority with preserved base initialization, native presentation,
  scoped recipient retries and quiet join seeds implemented above.
- [x] Console/events — llama remote targets, admitted completion and motion debt:
  registered visible target selection, source-owned tail and confirmed paced prop
  motion admission implemented above; rendered physics remains open.
- [x] Console/events — llama pre-session timers and recovery boundaries: exact
  pause leases, bounded delivery/tombstones and owned Begin flag recovery are
  implemented above; unknown native outcomes do not imply rollback or idle retry.
- [x] Console/events — llamaSoul marker and native event/Begin recovery: existing
  PlacedSave record transport/adoption, verified sender cleanup, absent-sender
  guard and isolated108 rearm are implemented above with 75 focused groups.
- [ ] Console/events (12–15, 26, 42): complete Rufus/thiccfus gameplay transport,
  cross-player targets, effects and child state; accept llama marker/save/reload,
  native event cleanup and rendered jump/light/audio/prop-motion. Broader client
  event-count presentation/transport and old/new latent phase causality remain
  beyond the verified local cleanup boundary. Complete Madness grunts
  and variant AI/damage/effects; accept the host-ordered submitted/periodic/clear
  console output in game. b2209 implements recipient/source-safe queues, history,
  busy state and all 31 remote terminal seats with local regression coverage.
  Accept rendered Madness playback recovery and Maxwell phase. b2210 implements client
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

Use matching **0.9.2161 / b2217** packages for new checks. Record role,
item/model, reproduction and logs.
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
- [ ] Chairs (11/36): chair/body rotation and transition code implemented above;
  rendered host/client rotation and laptop/sleep/ragdoll acceptance remain open.
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
  The latest fact-row gate passes (565 rows/181 files);
  The latest fact-row gate passes (565 rows/181 files);
  older missing-reference/unused-row findings are superseded.

## Release preparation

The current 2161 snapshot is packaged as an offline test build after the complete
121-group suite and archive checks. The last recorded GitHub description update
was 2160. Thunderstore publication remains pending an explicit owner request;
the verified local ZIP is ready for manager import. Use package/build_current.ps1
and package/release.json for later builds. Native startup and rendered multiplayer
acceptance remain open; see TESTLOG for verification limits.

Commit, push, game installation and publication require an explicit request.
No other-agent workflow. Update the active tracking documents in place.
