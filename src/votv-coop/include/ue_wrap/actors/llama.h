#pragma once
#include "ue_wrap/core/cached_obj_ref.h"
#include <cstddef>
#include <functional>
namespace ue_wrap::llama {
bool IsLlama(void* actor);
size_t Collect(CachedObjRef* out,size_t capacity);
bool Read(void* actor,bool& active);
bool Entry(void* function,const uint8_t* parameters,int32_t& entry);
bool ParkBegin(void* actor,bool& original,const std::function<bool()>& current);
bool RestoreBegin(void* actor,bool original,const std::function<bool()>& current);
// dispatched is true after one-shot admission, including uncertain native completion.
bool Apply(void* actor,bool active,bool cue,bool& dispatched,const std::function<bool()>& current);
}
