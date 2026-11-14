#pragma once
#include "ue_wrap/core/cached_obj_ref.h"
#include <functional>
namespace ue_wrap::llama_begin_restore {
struct Snapshot {
    CachedObjRef actor,cls,sphere,sphereClass,timerType;
    void* world=nullptr;uint32_t generation=0;int32_t sphereOffset=-1,timerOffset=-1;
    uint8_t collision=0;
};
enum class Result {Done,Retry,Retired};
// Capture before the suppressed base Begin body, with its timer still zero.
bool Capture(void* actor,Snapshot& out,const std::function<bool()>& current);
bool Current(const Snapshot& snapshot);
bool Retired(const Snapshot& snapshot);
// graphAttempted is sticky at graph108 admission, including uncertain completion.
// A never-confirmed Begin can restore its captured collision, but cannot create a timer.
Result Restore(const Snapshot& snapshot,bool confirmed,bool& graphAttempted,const std::function<bool()>& current);
}
