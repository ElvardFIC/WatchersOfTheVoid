#pragma once
#include <functional>
namespace ue_wrap::atv_steering {
bool Read(void* actor, float& yaw);
bool Apply(void* actor, float yaw, const std::function<bool()>& current);
}
