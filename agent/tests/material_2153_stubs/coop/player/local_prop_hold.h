#pragma once
#include <functional>
#include <utility>
namespace coop::local_prop_hold {
inline bool held=false;
inline std::function<void()> onRead;
inline bool IsHeld(void*){auto callback=std::move(onRead);onRead={};if(callback)callback();return held;}
}
