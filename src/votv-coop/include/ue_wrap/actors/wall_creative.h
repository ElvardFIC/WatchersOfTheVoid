#pragma once
#include <functional>
namespace ue_wrap::wall_creative {
enum class Build {Retry,Dispatched,Failed};
bool IsBuilderCall(void* tool,void* function);
bool Allowed(void* context,bool& allowed);
Build Finish(void* wall,const std::function<bool()>& current);
}
