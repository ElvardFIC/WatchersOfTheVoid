// ue_wrap/actors/fire_extinguisher.cpp -- see ue_wrap/actors/fire_extinguisher.h.

#include "ue_wrap/actors/fire_extinguisher.h"

#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/component_calls.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"

#include <cstdint>

namespace ue_wrap::fire_extinguisher {
namespace {

namespace R = reflection;
namespace P = profile;

R::FName g_extName{0, 0};
R::FName g_mountName{0, 0};
bool     g_namesResolved = false;

// A member's place in the instance, read off the first live object of its class.
struct Member {
    int32_t offset = -1;
    uint8_t mask = 0;   // a bool's mask; 0 for a non-bool
    bool    tried = false;
};
Member g_mountHeld, g_charge, g_spraying, g_thrusting;

bool SameName(const R::FName& a, const R::FName& b) {
    return a.ComparisonIndex == b.ComparisonIndex && a.Number == b.Number;
}

bool IsA(void* obj, const R::FName& name) {
    if (!obj || !R::IsLive(obj) || !g_namesResolved) return false;
    void* cls = R::ClassOf(obj);
    for (int hops = 0; hops < 16 && cls; ++hops) {
        if (SameName(R::NameOf(cls), name)) return true;
        cls = R::SuperStructOf(cls);
    }
    return false;
}

bool Resolve(Member& m, void* obj, const wchar_t* name, bool isBool) {
    if (m.offset >= 0) return true;
    if (m.tried) return false;
    m.tried = true;
    void* cls = R::ClassOf(obj);
    if (isBool) {
        int32_t off = -1;
        uint8_t mask = 0;
        if (!R::FindBoolProperty(cls, name, off, mask) || off < 0 || mask == 0) return false;
        m.offset = off;
        m.mask = mask;
        return true;
    }
    const int32_t off = R::FindPropertyOffset(cls, name);
    if (off < 0) return false;
    m.offset = off;
    return true;
}

bool ReadBool(void* ext, Member& m, const wchar_t* name, bool& out) {
    if (!IsExtinguisher(ext) || !Resolve(m, ext, name, /*isBool=*/true)) return false;
    out = (reinterpret_cast<const uint8_t*>(ext)[m.offset] & m.mask) != 0;
    return true;
}

}  // namespace

bool ResolveNames() {
    if (g_namesResolved) return true;
    const R::FName ext = fname_utils::StringToFName(P::name::FireExtinguisherClass);
    const R::FName mount = fname_utils::StringToFName(P::name::FireExtinguisherMountClass);
    if (ext.ComparisonIndex == 0 || mount.ComparisonIndex == 0) return false;
    g_extName = ext;
    g_mountName = mount;
    g_namesResolved = true;
    return true;
}

bool IsExtinguisher(void* obj) { return IsA(obj, g_extName); }
bool IsMount(void* obj) { return IsA(obj, g_mountName); }

void* MountedExtinguisher(void* mount) {
    if (!IsMount(mount) || !Resolve(g_mountHeld, mount, P::name::FireExtMountHeldProp, false))
        return nullptr;
    return *reinterpret_cast<void* const*>(reinterpret_cast<const uint8_t*>(mount) + g_mountHeld.offset);
}

bool ReadCharge(void* ext, float& out) {
    if (!IsExtinguisher(ext) || !Resolve(g_charge, ext, P::name::FireExtChargeProp, false)) return false;
    out = *reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(ext) + g_charge.offset);
    return true;
}

bool ReadSpraying(void* ext, bool& out) {
    return ReadBool(ext, g_spraying, P::name::FireExtSprayingProp, out);
}

bool ReadThrusting(void* ext, bool& out) {
    return ReadBool(ext, g_thrusting, P::name::FireExtThrustingProp, out);
}

bool WriteCharge(void* ext, float value) {
    if (!IsExtinguisher(ext) || !Resolve(g_charge, ext, P::name::FireExtChargeProp, false)) return false;
    *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(ext) + g_charge.offset) = value;
    return true;
}

bool WriteSpraying(void* ext, bool value) {
    if (!IsExtinguisher(ext) || !Resolve(g_spraying, ext, P::name::FireExtSprayingProp, true)) return false;
    auto* byte = reinterpret_cast<uint8_t*>(ext) + g_spraying.offset;
    *byte = value ? static_cast<uint8_t>(*byte | g_spraying.mask)
                  : static_cast<uint8_t>(*byte & ~g_spraying.mask);
    return true;
}

// Native fire() also starts a Tick that traces from player 0's camera and spends
// charge. Remote hand displays need only its particle/audio components.
bool SetRemoteSpray(void* ext, bool spraying) {
    if (!IsExtinguisher(ext)) return false;
    CachedObjRef actor; actor.Set(ext);
    static R::InstanceOffset effectField{L"eff_ext"}, audioField{L"fireExtFire"};
    auto component = [&](R::InstanceOffset& field) {
        CachedObjRef ref;
        const int32_t off = field.Of(ext);
        if (off >= 0) ref.Set(*reinterpret_cast<void**>(static_cast<uint8_t*>(ext) + off));
        return ref;
    };
    auto effect = component(effectField), audio = component(audioField);
    if (!effect.Get() || !audio.Get()) return false;
    return engine::SetActorTickEnabled(ext, false) && actor.Get() &&
        WriteSpraying(ext, spraying) && effect.Get() &&
        component_calls::SetActive(effect.Get(), spraying, false) && actor.Get() && audio.Get() &&
        component_calls::SetVolumeMultiplier(audio.Get(), .5f) && actor.Get() && audio.Get() &&
        component_calls::SetActive(audio.Get(), spraying, false) && actor.Get();
}

bool AimRemoteSpray(void* ext, const FRotator& rotation) {
    if (!IsExtinguisher(ext)) return false;
    static R::InstanceOffset field{L"eff_ext"};
    const int32_t off = field.Of(ext);
    if (off < 0) return false;
    void* effect = *reinterpret_cast<void**>(static_cast<uint8_t*>(ext) + off);
    return effect && R::IsLive(effect) && engine::SetComponentWorldRotation(effect, rotation);
}

}  // namespace ue_wrap::fire_extinguisher
