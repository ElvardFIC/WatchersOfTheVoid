# Player ragdoll: native behavior and selected models

22 October 2026. Scope: player ragdoll, not Entity implementation.

The mod uses the game's playerRagdoll_C/playerRagdoll_crouch_C actor, native
ragdollMode, ownership, interactions, camera recovery and Unreal/PhysX simulation.
It has no separate ragdoll solver. The native player body is kel_lmao, with
kel_lmao_Physics and animbp_kelRagdoll. Its five rigid bodies are lowlegs, thighs,
pelvis, chest and head; the first two combine the legs. The selected full models
use a different skeleton layout, with separate limbs. The common full game asset
kerfurOmegaV1_Physics supplies 17 rigid bodies and 16 joints.

Native ragdollMode adds a backward chest velocity and calls fallImpulse. That
Blueprint waits Delay(0), then adds fallVeloc to every socket's physical body.
fallVeloc is the game's smoothed movement history. Reading the stopped character
or the pelvis before that latent action does not recover entry momentum.

The selected mesh and compatible physics are installed in the local player's
FinishSpawningActor post-hook. This returns before native ragdollMode assigns
playermodel, attaches the camera, enables physics and issues its impulses. Start
the initialized reference pose immediately; normal entry no longer waits for a
30 ms polling warmup or replaces a body that has already fallen. The late-refresh
fallback still handles an actor created before the watcher was registered.

The fallback mesh swap rebuilds physics and briefly disables simulation. Preserve the
original per-bone motion, hold actual SetPhysicsLinearVelocity commands for only
that component while its bodies are replaced, then restore them once physics is
ready. If the original impulse already ran, distribute the old pelvis translation
to the newly added limbs and restore native per-bone differences. If it comes
later, let the original call run normally. No extra guessed launch impulse and
no duplicate replay. The hold is disarmed outside that short transition.

Native wakeup reattaches the camera arm to crouchRoot, captures its relative pose
and blends pitch/roll upright. Its final forceGetUp clears the relative rotation.
The capsule still has the pre-fall yaw, so that final reset used to turn the view
even with no mouse input. At the local recovery attachment, hand the arm's current
yaw to the controller/capsule once and preserve the arm's full world pose. The
native animation then ends at the same heading. No post-recovery camera lock or
delayed forced correction is needed; subsequent player turning remains native.

## Alternatives

| Approach | Consequence |
|---|---|
| Keep the stock body invisible and drive a separate selected-model display | Retains the exact five-body simulation and native camera. Requires authored retargeting between combined thighs/lowlegs and separate limbs; there are no native independent arm/leg bodies to copy. A blind CopyPose/MasterPose swap cannot produce a correct full articulated model. Worth a prototype only if that simpler physical silhouette is acceptable. |
| Replace only the stock body's mesh and retain its five-body physics asset | The selected skeleton lacks thighs/lowlegs, so some native shapes/joints cannot bind. Not a safe generic model replacement. |
| Current native actor with the model's compatible full game physics asset | Keeps native lifecycle and Unreal physics, supports independent limbs and the existing network pose stream. Requires a controlled mesh/physics transition and the known common rig's pivot correction. |
| Author a dedicated correctly scaled player PhysicsAsset in Unreal | Best long-term route to remove runtime correction of the common asset's joint templates. Requires asset authoring/cooking and qualification across supported meshes. It would still use native Unreal physics. This asset has not been created or claimed tested. |
| Initialize the correct full rig as native construction returns | Implemented in b2184 using the existing native actor and common compatible game asset. The scoped FinishSpawningActor hook installs the local selected rig before camera/physics/impulses. No new cooked subclass or persistent shared class-template mutation is required. The dedicated PhysicsAsset remains an optional asset-authoring improvement. |

Current choice: initialize the full rig at the native construction boundary,
retain the game's movement commands and let native recovery own its animation.
The compatible asset's joint correction and guarded late-refresh fallback remain.

The implemented construction route follows the native order in ragdollMode:
FinishSpawningActor (1447), playermodel assignment (1575), camera attachment
(1724), simulation (1863) and delayed movement transfer (2174). The native tests
verify selected-mesh simulation before ragdollMode returns, completed get-up and
subsequent heading stability. Momentum is observed after actual native per-bone
commands, before floor friction can alter the first polling sample.

Native audit evidence: build/ragdoll-evidence/native-ragdollMode.txt,
native-playergraph.txt (fallImpulse continuation 10805), and the extracted
playerRagdoll/kel_lmao_Physics properties under build/ragdoll-tools/assets.
Current regressions/evidence: build/ragdoll-transition-check/.
