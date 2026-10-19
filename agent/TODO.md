# TODO - current priorities

Current source/wire protocol: **2153** (continued 2152 entity work), game **0.9.0n**.
Delivery package: **0.9.2152**, containing DLL **b2153 / protocol 2153**.
All peers must install the same package; older protocols are incompatible.

Owner instruction (19 October): continue source/DLL/protocol **2153** in place.
Do not create separate versions during builds without the owner's explicit order.

## Entity category request - OPEN

The owner's complete taxonomy, native class mappings and unresolved behaviors are
in [WORK.md](WORK.md), "Complete requested entity coverage and remaining work".
Do not close a creature from allowlist membership or transform replication alone.

Current grayboar work: host physical-body movement feed, owner-directed bite damage,
foreign-health protection, stable adopted-replica authority across drive handoffs,
and native AnimBP locomotion/movement-loop audio input from interpolated velocity.
Host native sensePawn scans Pawn candidates; actual multiplayer target detection
still needs testing. No generic client AI or gameplay-verb replay was added.
Client host-replica bite-overlap and attack-notify bodies are now refused, covering
the local attack impulse and arbitrary-object damage as well as player health.
Death playback, exact time alignment and helper/turning coordination remain open;
live attack/roar cosmetic relay is implemented and awaits two-player verification.
Mid-action join now samples the native active montage position/rate and resumes
that animation with local queue-age compensation. It does not restart one-shot
audio, or compensate the unsynchronized host/client clocks/network transit.
Host replicas also refuse native sensePawn target assignment and detected roar/
target-delegate binding on clients. Already-running helper tasks and delegates
from before save adoption remain follow-ups; this does not establish shared AI.

Next implementation:
1. Grayboar death montage/sound cues, remaining join/audio/timing work, and client
   helper/turning coordination. Live attack/roar cosmetic cues are implemented;
   two-player playback and timing still need verification. Preserve native host targeting,
   damage ownership and corpse/loot authority; do not replay loot/damage as cosmetics.
2. Host/client tests for spawn, target, movement, action, damage, death, reconnect
   and mid-event join. Personal Insomniac/nightmare conditions remain peer-local.


## Other open priorities

- Run the ten priority gameplay checks in WORK.md on matching peer builds.
- Ragdoll: selected authored rig, entry/get-up, native local body and exact limbs
  remain gameplay follow-ups.
- Bench58: record exact item/authoring peer and logs for simple-placement loss;
  birth/save retry code does not prove the original symptom resolved.
- Token52: identify the exact counter/UI before changing balance code.
- Wall21/22: atomic cement request/result accounting across hand/slot/drop/retry/
  disconnect; preserve the existing no-spend refusal. Wall41 remains excluded.
- Full backlog and report status: [WORK.md](WORK.md), [TESTLOG.md](TESTLOG.md).

## Current verification and boundaries

package/check_2152.ps1 passes all 9 runners, including expanded entity/boar tests.
Full MSVC Release DLL resources identify **0.9.0n b2153**. Hash and evidence:
build/release-2152-logs/verification.json and entity-validation.json.
Release scripts explicitly distinguish package 2152 from DLL/protocol 2153;
workspace dist/ contains the 2152 ZIP and SHA256SUMS-2152.txt.
Offline regressions do not establish two-player gameplay correctness.
Live grayboar cues use host-only reliable kind 200 within development 2153. Install
matching current DLLs on both peers; the existing delivery ZIP predates this work.

All 21 merge conflicts are resolved; the merge still awaits its commit. Current
source/test changes remain uncommitted. Preserve all work. Commit, push, game
installation and publication require an explicit request. No other-agent workflow.
Update TODO/WORK/TESTLOG in place; detailed prior evidence remains in WORK/TESTLOG.
