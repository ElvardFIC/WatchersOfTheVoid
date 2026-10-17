// ue_wrap/world/bad_sun.h -- the Bad Sun: the world-event actor badSun_C, read per copy. A copy polls from 0.2 s
// after its BeginPlay and then every 5 s; at the first poll inside 7 <= hour <= 18 it calls lib_C::setEvent(true,
// this), creates a ui_badSun widget on the local viewport, stores it in its `widget` member and stops polling
// (the decompiled badSun Blueprint, its ubergraph's hour test). So a copy is ACTIVE iff `widget` is set, and an
// active copy sits in the gamemode's activeEvents_senders. The join design that rests on this reads it as every
// peer's own copy. Engine access only; no network or co-op state. Game thread.
#pragma once

namespace ue_wrap::bad_sun {

// Resolve the `widget` member on badSun_C, the class from the object index. False while the class is not loaded
// (no copy can exist: not a failure). Once it has loaded the member resolves on that pass or never: a failed pass
// latches OFF with one warning. True once the member is in hand.
bool EnsureResolved();

// The number of live badSun_C instances, after the index has applied every announced birth and death; `*first`,
// when given, is the first live one or null. A class that is not loaded counts 0. -1 when the index could not
// drain (off the game thread, or inside a drain): the caller asks again.
int CountLive(void** first);

// Whether `copy`'s widget is set, the design's definition of an active copy. False for null or while the member
// has not resolved.
bool IsActive(void* copy);

// Whether `copy` is among the gamemode's activeEvents_senders, the list setEvent(true) appends its caller to.
// False for null, or while that list does not read.
bool IsActiveEventSender(void* copy);

}  // namespace ue_wrap::bad_sun
