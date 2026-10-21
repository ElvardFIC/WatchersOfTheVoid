#pragma once
#include "ue_wrap/actors/save_record.h"
namespace ue_wrap::grill_saved_state {
bool Supported(void* actor);
bool SaveNative(void* actor,void* record);
bool LoadNative(void* actor,const void* record);
bool RefreshHeldRecord(void* actor,save_record::SaveRecord& record);
bool PreserveHostRecord(void* actor,save_record::SaveRecord& record);
}
