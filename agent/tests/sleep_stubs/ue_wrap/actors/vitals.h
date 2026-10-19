#pragma once
#include "ue_wrap/actors/sleep.h"
#include "../../../../../src/votv-coop/include/ue_wrap/actors/vitals.h"
namespace ue_wrap::vitals {
inline bool Read(Field field, float* out) {
    if (field == Field::Sleeping) {*out = sleep::sleeping ? 1.f : 0.f; return true;}
    if (field == Field::Dreaming) {*out = sleep::dreaming ? 1.f : 0.f; return true;}
    return false;
}
}
