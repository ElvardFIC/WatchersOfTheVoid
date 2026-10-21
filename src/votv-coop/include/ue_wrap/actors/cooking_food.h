#pragma once
#include "ue_wrap/actors/save_record.h"
namespace ue_wrap::cooking_food {
bool IsCookingClass(void* cls);
bool IsCookingFood(void* actor);
bool ReadProgress(void* actor,float& progress);
bool WriteProgress(void* actor,float progress);
// The native food serializer has no cooking-progress field. Keep its groups
// intact and append group 2 [cookProgress] for the measured cooking family.
bool SaveNative(void* actor,void* record);
bool LoadNative(void* actor,const void* record);
bool RefreshHeldRecord(void* actor,save_record::SaveRecord& record);
}
