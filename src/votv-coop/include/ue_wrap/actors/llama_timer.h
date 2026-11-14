#pragma once
#include "ue_wrap/core/cached_obj_ref.h"
#include <functional>
namespace ue_wrap::llama_timer {
// Existing native timer only. No replacement handle or synthesized timer phase.
struct Lease {
    CachedObjRef actor,cls,type;void* world=nullptr;uint32_t generation=0;
    uint64_t handle=0;int32_t offset=-1;
};
struct State {Lease lease;bool exists=false,active=false,paused=false;};
enum class Result {Done,Retry,Retired};
bool Current(const Lease& lease);
// A temporary reflection/readiness miss is unknown, not proof that ownership retired.
bool Retired(const Lease& lease);
bool Inspect(void* actor,State& out,const std::function<bool()>& current);
// dispatched is sticky: true immediately before native mutation, including an unknown result.
Result Pause(const Lease& lease,bool& dispatched,const std::function<bool()>& current);
Result Resume(const Lease& lease,const std::function<bool()>& current);
}
