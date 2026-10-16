// ue_wrap/actors/sleep.h -- standalone engine access for VOTV's sleep and timelapse state: the gamemode's
// isSleep flag, the bed it sleeps in, the global time dilation and the nightmare probability override.
// Principle-7 wrapper, no network logic; coop/sleep_sync reads and writes through it.
//
// The whole timelapse is ONE engine call -- SetGlobalTimeDilation(20) at sleep entry, 1.0 at wake
// -- plus ONE world flag, mainGamemode.isSleep. The natural wake fires when saveSlot.sleep reaches
// 100, and that need REFILLS during sleep at the dilated rate. The nightmare roll runs every 500
// in-sleep seconds, weighted by bedSleepProb() -- the bed's own dreamProb, 0.15 with no bed, or
// mainGamemode.dreamProbability when that is >= 0, where -1 is the single-player sentinel and 0
// kills the rolls. A winning roll calls wakeup() and then createDream().
//
// gamemode.wakeup() is the idempotent END of the timelapse: re-possess, camera, dilation back to
// 1.0, isSleep false. It also rolls the 10% gearer gift, but only when saveSlot.sleep is >= 99 AND
// the bed is a bed_C. The sleep need itself is a vitals row (ue_wrap/actors/vitals, Field::Sleep).

#pragma once

#include <cstdint>
#include <string>

namespace ue_wrap::sleep {

// Resolve mainGamemode_C fields/verbs + the GameplayStatics dilation calls (throttled lazy
// retry). Game thread.
bool EnsureResolved();

// mainGamemode.isSleep -- THE world sleep flag (false if unresolved).
bool IsSleeping();

// The class name of the bed the gamemode sleeps in (mainGamemode.bed, set by sleep() before
// isSleep rises, null for a bedless entry; wakeup() leaves it, so read it only while asleep);
// empty with no bed or unresolved. Game thread.
std::wstring BedClassName();

// mainGamemode.dreamProbability: -1 = SP sentinel (use bed.dreamProb);
// 0 = nightmares suppressed. Returns false if unresolved.
bool SetDreamProbability(float v);

// Reflected gamemode.wakeup() -- the native timelapse END (idempotent: BP
// no-ops when !isSleep). Game thread.
bool CallWakeup();

// UGameplayStatics::SetGlobalTimeDilation / GetGlobalTimeDilation via the
// CDO + a persistent WorldContextObject. Get returns <= 0 on failure.
bool SetGlobalTimeDilation(float v);
float GetGlobalTimeDilation();

// ---- dev-drill helpers (coop/dev/midnight_drill) ----
// First live bed actor (bed_C or a descendant), or null. One GUObjectArray walk per call --
// one-shot use only.
void* FindBed();
// Reflected gamemode.sleep(bed, dropItem=false, ignoreRagdoll=true) -- the
// native entry incl. its validation gates (event/hunger/ragdoll aborts).
bool CallSleep(void* bed);

}  // namespace ue_wrap::sleep
