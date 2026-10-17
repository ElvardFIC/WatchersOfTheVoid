// ue_wrap/world/bad_sun.h -- the Bad Sun: the world-event actor badSun_C, read per copy. 0.2 s after its BeginPlay
// a copy registers itself, then tests the hour every 5 s, first at 5.2 s; at the first test inside 7 <= hour <= 18
// it calls lib_C::setEvent(true, this), creates a ui_badSun widget on the local viewport, stores it in its `widget`
// member and stops polling (the decompiled badSun Blueprint, its ubergraph's hour test). So a copy is ACTIVE iff
// `widget` is set, and an active copy sits in the gamemode's activeEvents_senders. The join design that rests on
// this reads it as every peer's own copy. Engine access only; no network or co-op state. Game thread.
#pragma once

namespace ue_wrap::bad_sun {

// Resolve the `widget` member on badSun_C, the class from the object index. False while the class is not loaded
// (no copy can exist: not a failure). Once it has loaded the member resolves on that pass or never: a failed pass
// latches OFF with one warning. True once the member is in hand.
bool EnsureResolved();

// The number of badSun_C instances of the running world (the class default object and a departed world's copies
// do not count), after the index has applied every announced birth and death, capped at 8; `*first`, when given,
// is the first one or null. A class that is not loaded counts 0. -1 when the index could not drain (off the game
// thread, or inside a drain): the caller asks again.
int CountLive(void** first);

// Read whether `copy`'s widget is set, the design's definition of an active copy. False, with `active` false, for
// null or while the member has not resolved: the answer is then unknown, not "inactive".
bool IsActive(void* copy, bool& active);

// Read whether `copy` is among the gamemode's activeEvents_senders, the list setEvent(true) appends its caller to.
// False, with `sender` false, for null or while that list does not read.
bool IsActiveEventSender(void* copy, bool& sender);

}  // namespace ue_wrap::bad_sun
