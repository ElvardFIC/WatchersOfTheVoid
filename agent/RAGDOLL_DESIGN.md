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

The mesh swap rebuilds physics and briefly disables simulation. Preserve the
original per-bone motion, hold actual SetPhysicsLinearVelocity commands for only
that component while its bodies are replaced, then restore them once physics is
ready. If the original impulse already ran, distribute the old pelvis translation
to the newly added limbs and restore native per-bone differences. If it comes
later, let the original call run normally. No extra guessed launch impulse and
no duplicate replay. The hold is disarmed outside that short transition.

## Alternatives

| Approach | Consequence |
|---|---|
| Keep the stock body invisible and drive a separate selected-model display | Retains the exact five-body simulation and native camera. Requires authored retargeting between combined thighs/lowlegs and separate limbs; there are no native independent arm/leg bodies to copy. A blind CopyPose/MasterPose swap cannot produce a correct full articulated model. Worth a prototype only if that simpler physical silhouette is acceptable. |
| Replace only the stock body's mesh and retain its five-body physics asset | The selected skeleton lacks thighs/lowlegs, so some native shapes/joints cannot bind. Not a safe generic model replacement. |
| Current native actor with the model's compatible full game physics asset | Keeps native lifecycle and Unreal physics, supports independent limbs and the existing network pose stream. Requires a controlled mesh/physics transition and the known common rig's pivot correction. |
| Author a dedicated correctly scaled player PhysicsAsset in Unreal | Best long-term route to remove runtime correction of the common asset's joint templates. Requires asset authoring/cooking and qualification across supported meshes. It would still use native Unreal physics. This asset has not been created or claimed tested. |
| Construct the native actor with the correct full rig before simulation starts | The native graph finishes deferred spawning before assigning playermodel, attaching the camera, calling SetSimulatePhysics and fallImpulse. A properly cooked compatible ragdoll subclass/component template could initialize the final rig during construction and avoid replacing live bodies afterward. This is the strongest simplification to prototype with the dedicated asset. It needs per-skin construction selection, correct standing/crouched transforms and camera/bone compatibility; do not permanently mutate shared class templates or claim it works without native construction tests. |

Current choice: fix the lost native command at the transition and retain the
working full-rig implementation. A render-only retargeter is a different rigging
task, not a one-line simplification. Do not replace the current system with an
unvalidated prototype or change NPC/shared defaults persistently.

The proposed construction route follows the native order in ragdollMode:
FinishSpawningActor (1447), playermodel assignment (1575), camera attachment
(1724), simulation (1863) and delayed movement transfer (2174). This is a design
finding, not an implemented replacement. A final rig present before registration
and simulation should remove the need to preserve commands across a live-body
swap; that expectation must be tested with authored assets, not assumed.

Native audit evidence: build/ragdoll-evidence/native-ragdollMode.txt,
native-playergraph.txt (fallImpulse continuation 10805), and the extracted
playerRagdoll/kel_lmao_Physics properties under build/ragdoll-tools/assets.
Momentum regressions/evidence: build/ragdoll-momentum-check/.
