#pragma once
#include "ue_wrap/core/cached_obj_ref.h"
#include <functional>
namespace ue_wrap::llama_events {
struct Owner {CachedObjRef actor,cls;void* world=nullptr;uint32_t generation=0;};
struct Event {CachedObjRef actor,cls;bool active=false,ambient=false;};
enum class Result {Done,Retry,Retired};
bool Identify(void* actor,Owner& out);
bool Retired(const Owner& owner);
// Removes only this sender's verified entries and contribution; never derives
// the global count from array length. guarded is sticky after native h/entry cleanup.
Result Cleanup(const Owner& owner,bool& guarded,const std::function<bool()>& current);
bool Absent(const Owner& owner,bool& out,const std::function<bool()>& current);
bool ReadEvent(void* function,const uint8_t* parameters,Event& out);
}
