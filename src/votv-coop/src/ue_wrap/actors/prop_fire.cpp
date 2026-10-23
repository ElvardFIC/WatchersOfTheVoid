// ue_wrap/actors/prop_fire.cpp -- see ue_wrap/actors/prop_fire.h.

#include "ue_wrap/actors/prop_fire.h"

#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"

#include <cstdint>
#include <cstring>

namespace ue_wrap::prop {

bool PropFlameBaseValid(void* prop) {
    static reflection::InstanceOffset kFlameBase{L"flameBase"};
    const int32_t off = kFlameBase.Of(prop);
    if (off < 0) return false;
    void* flame = nullptr;
    std::memcpy(&flame, reinterpret_cast<const uint8_t*>(prop) + off, sizeof(flame));
    return flame && reflection::IsLive(flame);
}

}  // namespace ue_wrap::prop
