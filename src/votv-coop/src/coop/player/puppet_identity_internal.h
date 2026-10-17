// coop/player/puppet_identity_internal.h -- what puppet_identity.cpp (the stub rows, the arming, the per-world
// resolve) and puppet_identity_scan.cpp (the four native seams) share.

#pragma once

#include <cstdint>

namespace coop::puppet_identity {

// A row's index in Counters(), and the refusal count and log line it owns.
enum Row : uint8_t {
    kWispW, kWispO, kWispP, kKelura, kOrborb, kWellTpBegin, kWellTpEnd, kDreamBoulder, kMapTransition,
    kTriggerOpenLevel, kTriggerSpikes, kDreamClimb, kDreamJump, kDreamRunFin, kDreamRunWok, kDreamfillDoor,
    kGrunt, kShower, kBirch, kErie, kSkullzone,
    kRowCount
};

// RemoveActors' predicate: drop the puppet bodies. `ctx` is unused.
bool IsPuppetBodyFn(void* ctx, void* actor);

// Count a refusal and say it once per row per world: the toucher's slot when `actor` is in hand, else unknown.
void NoteRefused(Row row, void* actor);

// Is the grunt stub's marked scope on this thread's chain, in a world the resolve still describes.
bool InGruntScope();

namespace scan {

// Install each native seam not yet installed, at most once per `generation` (it moves with each per-world
// resolve), and arm the post hooks for `puppets` registered puppets.
void InstallPump(int generation, int puppets);

// Arm the three post hooks while a puppet exists. Idempotent.
void Rearm(int puppets);

// Re-resolve the scan rows whose class moved, or all of them when `unlistVersion` moved. True when any row was
// looked at again.
bool Resolve(uint64_t unlistVersion);

// The SetControlRotation pre hook, armed only while a grunt's touch by a puppet runs.
void ArmGruntWindow(bool on);
bool GruntWindowArmed();

// Forget every resolved pointer and disarm the grunt window.
void Clear();

}  // namespace scan
}  // namespace coop::puppet_identity
