// ue_wrap/actors/fossilhound.cpp -- visible body yaw, adapted from VOTV-MP/Multivoid.
#include "ue_wrap/actors/fossilhound.h"
#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/engine/engine.h"
#include <cstddef>
#include <cstdint>
namespace ue_wrap::fossilhound {
namespace {
namespace P = profile;
namespace R = reflection;
void* LiveAt(void* base, size_t off) {
    void* p = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(base) + off);
    return (p && R::IsLive(p)) ? p : nullptr;
}
// The hound's mesh when `actor` is a fossilhound_C or a descendant, else null. The class chain is compared by name (the
// name resolved once, as npc_mirror_brain's table does), so no class is looked up per call and an unloaded class is
// never held: the host asks this for every NPC on every pose send.
void* HoundMesh(void* actor) {
    if (!actor || !R::IsLive(actor)) return nullptr;
    static R::FName sName{};
    static bool sResolved = false;
    if (!sResolved) {
        if (!fname_utils::Resolved()) return nullptr;
        const R::FName name = fname_utils::StringToFName(P::name::NpcClass_Fossilhound);
        if (name.ComparisonIndex == 0 && name.Number == 0) return nullptr;  // NAME_None: not latched, asked again
        sName = name;
        sResolved = true;
    }
    int hops = 0;
    for (void* cls = R::ClassOf(actor); cls && hops < 16; cls = R::SuperStructOf(cls), ++hops) {
        const R::FName& n = R::NameOf(cls);
        if (n.ComparisonIndex == sName.ComparisonIndex && n.Number == sName.Number)
            return LiveAt(actor, P::off::ACharacter_Mesh);
    }
    return nullptr;
}

}  // namespace

bool ReadBodyYaw(void* actor, float& outYaw) {
    void* mesh = HoundMesh(actor);
    if (!mesh) return false;
    outYaw = engine::GetComponentWorldRotation(mesh).Yaw;
    return true;
}

void DriveBodyYaw(void* actor, float yaw) {
    if (void* mesh = HoundMesh(actor)) engine::SetComponentWorldRotation(mesh, FRotator{0.f, yaw, 0.f});
}

}  // namespace ue_wrap::fossilhound
