#pragma once
#include "ue_wrap/actors/save_record.h"
namespace ue_wrap::material_saved_state {
bool Supported(void* actor);
// Extend the native leaf schemas: concrete float group 1 [dryTimer];
// sponge float group 1 [power, soapAmount]. Native fields keep their indices.
bool SaveNative(void* actor, void* nativeRecord);
bool LoadNative(void* actor, const void* nativeRecord);
}
