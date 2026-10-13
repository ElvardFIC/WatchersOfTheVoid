#include "ue_wrap/actors/physics_gun.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/world_identity.h"
#include <cstring>

namespace ue_wrap::physics_gun {
namespace {
namespace R = reflection;
CachedObjRef g_class;
int g_hold = -1, g_target = -1, g_component = -1;
uint8_t g_mask = 0;
bool g_supported = false;
bool Resolve(void* gun) {
    void* cls = R::ClassOf(gun);
    if (g_class.Is(cls)) return g_supported;
    g_class.Set(cls); g_supported = false;
    g_hold = g_target = g_component = -1; g_mask = 0;
    for (void* c = cls; c; c = R::SuperStructOf(c)) {
        if (!R::NameEquals(R::NameOf(c), L"prop_physgun_C")) continue;
        g_target = R::FindPropertyOffset(cls, L"objectPickup");
        g_component = R::FindPropertyOffset(cls, L"component");
        g_supported = R::FindBoolProperty(cls, L"hold", g_hold, g_mask) &&
            g_hold >= 0 && g_mask && g_target >= 0 && g_component >= 0;
        break;
    }
    return g_supported;
}
}
void* HeldProp(void* gun) {
    void* world = world_identity::CurrentWorld();
    if (!world || !gun || !R::IsLive(gun) || world_identity::WorldOf(gun) != world || !Resolve(gun))
        return nullptr;
    const auto* bytes = static_cast<const uint8_t*>(gun);
    // hold is cleared before the native tick clears objectPickup. That stale
    // pointer must not keep the network hold alive for another frame.
    if (!(bytes[g_hold] & g_mask)) return nullptr;
    void* target = nullptr; void* component = nullptr;
    std::memcpy(&target, bytes + g_target, sizeof(target));
    std::memcpy(&component, bytes + g_component, sizeof(component));
    if (!target || target == gun || !R::IsLive(target) || !component || !R::IsLive(component) ||
        world_identity::WorldOf(target) != world || !prop::IsDescendantOfProp(target)) return nullptr;
    return target;
}
}
