#pragma once
#include "ue_wrap/actors/save_record.h"
#include <cmath>
namespace ue_wrap::wall_drying {
bool IsWetWall(void* actor);
// Native customWall float group 0 keeps health/maxHealth at 0/1; a wet wall
// also saves dryTime/timeToDry at 2/3. Older two-value records remain readable.
inline bool Valid(const std::vector<float>& fields) {
    if (fields.size() != 2 && fields.size() != 4) return false;
    for (float v : fields) if (!std::isfinite(v) || v < 0 || v > 1'000'000.f) return false;
    return fields[1] > 0 && (fields.size() == 2 || fields[3] > 0);
}
bool Capture(void* actor, save_record::SaveRecord& record);
bool Restore(void* actor, const save_record::SaveRecord& record);
bool SaveNative(void* actor, void* nativeRecord);
bool LoadNative(void* actor, const void* nativeRecord);
}
