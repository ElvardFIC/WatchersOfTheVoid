#pragma once
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/types.h"
#include <functional>
#include <vector>
namespace ue_wrap::llama_trigger {
// Captured native dependencies, never raw actor/component pointers across calls.
struct Probe {
    CachedObjRef actor,cls,sphere,sphereClass,audio,audioClass,mesh,meshClass,timerType;
    void* world=nullptr;uint32_t generation=0;uint64_t timer=0;
    FVector center{},origin{},position{};float radius=0;
};
bool ReadTimer(void* actor,uint64_t& out);
bool Capture(void* actor,Probe& out,const std::function<bool()>& current);
bool Overlaps(const Probe& probe,std::vector<CachedObjRef>& out,const std::function<bool()>& current);
bool ClearLine(const Probe& probe,const FVector& eye,const std::function<bool()>& current);
// Target ownership is needed until admission. After the captured timer is consumed,
// finish the source-owned operation even if the selected player departs or moves.
// admitted means the one-shot timer clear was dispatched, even if completion is unknown.
bool Launch(const Probe& probe,const FVector& eye,bool& admitted,const std::function<bool()>& targetCurrent,
            const std::function<bool()>& sourceCurrent={});
}
