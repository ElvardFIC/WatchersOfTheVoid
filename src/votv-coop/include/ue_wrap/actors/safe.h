#pragma once
#include "ue_wrap/actors/save_record.h"
namespace ue_wrap::safe {
bool IsSafe(void* actor);
bool ReadLocked(void* actor,bool& locked);
// Native hacked() is guarded by locked and creates the contents once.
bool Unlock(void* actor);
// A delayed inventory record must not relock an already-open host safe.
bool PreserveUnlocked(void* actor,save_record::SaveRecord& record);
bool PreserveHostLock(void* actor,save_record::SaveRecord& record);
}
