# How we work

## Owner's standing preferences

- Continue useful work autonomously within the requested scope. Avoid repeatedly asking permission for ordinary reversible edits and checks.
- Prefer several bounded fixes per continuation when feasible, but do not claim unrelated issues are fixed just because they share a symptom.
- Keep tests focused. Do not rerun all historical suites or repeat broad asset research without a new reason. Quality includes hidden failures, not test volume.
- The receiving assistant has **only a monthly usage limit**, as clarified by the owner. Do not apply this workspace assistant's historical five-hour or weekly quota rules, per-window stopping points, or fixed quota-reserve percentages to that assistant. Continue the authorized work while its monthly allowance permits; do not stop merely because an earlier continuation covered a few issues.
- Monitor the receiving assistant's own monthly usage if its tools expose it. Used percentage is not remaining percentage; historical reports from this workspace do not describe that account. If usage is unavailable, do not invent a balance. Save exact progress before an interruption or monthly exhaustion. Treat “continue” as resuming recorded work, not as evidence that the monthly allowance has reset.
- Release packaging is currently deferred until the TestLog work is handled. Before the eventual release, review **all changes after2146** and allow enough capacity for compilation and package repairs, without imposing the old five-hour budget's10–15% rule. Focused translation-unit/test compilation is allowed now; it is different from producing a release DLL/ZIP. These release and scope decisions are separate from account usage limits.
- Do not commit, push, install, publish or modify the public release without new authorization. Preserve the large existing uncommitted working tree; never reset it to make an incoming patch easier to apply.
- TestLog41, the secret basement wall, is explicitly excluded. The detachable computer-room panels in27 are a separate issue.

## Investigation and implementation loop

1. **Recover context narrowly.** Inspect Git status/diff, the newest continuation note and the relevant TestLog row. Separate the user's observations from our hypotheses and prior assistant claims.
2. **Identify the actual native action.** Establish actor class, inheritance, function/override, caller, and whether this is a world object, selected-hand actor, inventory record or display mirror. Record exact native evidence when available. A matching name alone is not proof.
3. **Trace the whole lifecycle.** Follow creation, identity assignment, mutation, ownership transfer, removal, save/reload and late join. Find which existing network channel should carry each result before adding another.
4. **Choose authority deliberately.** Shared persistent outcomes generally need the host's canonical state. Some personal behavior must run on the owning player. Do not blindly replay a verb on every peer if it also consumes an item, creates loot, moves the local player or applies damage.
5. **Implement a bounded change.** Keep reflection and engine field access in `ue_wrap`; put session policy, validation, queues and synchronization in `coop`. Register new files in `sources.cmake` and wire every lifecycle entry point. If protocol meaning/layout changes, update `kProtocolVersion` and inspect packet routing/lane/relay behavior. Do not update release metadata as though an unbuilt source revision were a shipped version.
6. **Review the failure paths.** Then run the smallest useful checks covering the behavior and compile the affected production files. Add tests for meaningful state transitions and failure cases, not merely assertions that repeat the implementation.
7. **Leave a usable handoff.** Record evidence, changed behavior, test results, remaining limits and the next concrete step. Update `WORK_<revision>.md`, `TODO_<revision>.md`, `TODO_CONTINUE.md` and `TESTLOG_STATUS.md` as appropriate, mirroring `agent/` and `src/agent/`.

## Hidden failures we actively check

- Duplicate or delayed requests must not spawn loot twice, resurrect removed actors, or overwrite a newer canonical record.
- Failed/partial sends need a bounded retry policy. “Queued” is not automatically “received and applied.” Preserve event order where it matters.
- A joiner may load an older save while the host keeps playing. Handle live births, removals, dependency arrival and deduplication against actors already loaded from that save.
- Slot reuse must not transfer an old player's pending operation to a new occupant. Use the roster's occupancy identity where needed.
- Raw UObject pointers are unsafe across ticks. Use the existing serial/world-aware `CachedObjRef`, and recheck after native calls that can destroy objects or reenter the controller.
- Do not hold iterators/references into mutable queues across engine callbacks. Check world/session changes during deferred work.
- Unreal packed booleans require the reflected bit mask. Function frames need verified offsets/sizes; inheritance and overridden Blueprint bodies affect which hook actually runs.
- Cancelling a Blueprint body does not initialize its return/out parameters. Check its callers: for example, wall repair refusal must report `isFine=true`, otherwise the trowel can spend cement even though no repair ran. Preserve packed bits and update the actual Blueprint out parameter as well as the local frame.
- The selected hand, inventory data, physical grab and remote display are different representations. Fixing one does not prove the others preserve state.
- Avoid full object-array scans on hot paths, frequent searches for unloaded classes, and copying large unchanged records every frame. Prefer existing indexed lookup and bounded work budgets.

These are review prompts, not proof that every current implementation satisfies them. Report counterexamples and limitations honestly.

## Verification and reporting

For build naming, version metadata, changelog and package presentation, follow [BUILD_AND_RELEASE_STANDARD.md](BUILD_AND_RELEASE_STANDARD.md). It records the fork's actual conventions and separates an unbuilt source revision from a shipped release.

Use separate labels: **investigated**, **implemented**, **focused tests passed**, **production files compiled**, **release built**, and **tested in multiplayer**. Mocked engine-boundary tests cannot establish real Blueprint side effects or two-player gameplay.

On the owner's workspace, the recent focused runner is:

```powershell
# Run from C:/Users/Elvar/Downloads/MultiVoid+
& ./src/agent/tests/run_2170.ps1 -CompileSources
# Only if changing the selected-hand material display:
& ./src/agent/tests/run_2159.ps1 -OnlyBucket -CompileSources
```

These locate the local compiler helper and compile test executables/individual production files. They do not produce a release package. Inspect the runners and choose checks appropriate to your actual change; these commands are examples, not requirements to rerun them on every task. The second runner retains its historical filename but its selected tests now include2166 coverage. In a standalone checkout, use `./agent/tests/` instead of `./src/agent/tests/` and configure an available C++20/MSVC environment; the owner's `_toolchain/env.ps1` path is not portable. Missing tooling is a verification limitation, not a reason to claim a pass.

If another assistant supplies fixes, compare them to the current implementation before merging. Preserve useful parts, resolve overlaps, and document provenance and what changed. Return changed source files or a patch, a concise English work note, exact verification results and remaining risks. Do not include game assets, toolchains or generated build directories in a handoff. Coordinate file ownership with the owner if two agents might edit the same checkout simultaneously; this guide does not authorize messaging or delegating to other agents.
