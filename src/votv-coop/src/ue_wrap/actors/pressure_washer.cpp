#include "ue_wrap/actors/pressure_washer.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"
#include <cmath>
#include <cstring>

namespace ue_wrap::pressure_washer {
namespace {
namespace R = reflection;
CachedObjRef g_class;
int32_t g_level = -1, g_fire = -1;
uint8_t g_fireMask = 0;
bool Fields(void* actor) {
    if (!IsWasher(actor)) return false;
    void* cls = R::ClassOf(actor);
    if (!g_class.Is(cls)) {
        g_class.Set(cls);
        g_level = R::FindPropertyOffset(cls, L"level");
        g_fire = -1; g_fireMask = 0;
        R::FindBoolProperty(cls, L"fire", g_fire, g_fireMask);
    }
    return g_level >= 0 && g_fire >= 0 && g_fireMask;
}
}
bool IsWasher(void* actor) {
    if (!actor || !R::IsLive(actor)) return false;
    for (void* cls = R::ClassOf(actor); cls; cls = R::SuperStructOf(cls))
        if (R::NameEquals(R::NameOf(cls), L"prop_waterShooter_C")) return true;
    return false;
}
bool Read(void* actor, bool& firing, float& level) {
    if (!Fields(actor)) return false;
    std::memcpy(&level, static_cast<uint8_t*>(actor) + g_level, sizeof(level));
    firing = (static_cast<uint8_t*>(actor)[g_fire] & g_fireMask) != 0;
    return std::isfinite(level);
}
bool SetRemoteLevel(void* actor, float level) {
    if (!std::isfinite(level) || level < 0.f || level > 10.f || !Fields(actor)) return false;
    CachedObjRef live; live.Set(actor);
    if (!engine::SetActorTickEnabled(actor, false) || !live.Get()) return false;
    std::memcpy(static_cast<uint8_t*>(actor) + g_level, &level, sizeof(level));
    static_cast<uint8_t*>(actor)[g_fire] &= uint8_t(~g_fireMask);
    return true;
}
bool EmitRemote(void* actor, const FVector& origin, const FRotator& aim) {
    if (!IsWasher(actor)) return false;
    for (float value : {origin.X, origin.Y, origin.Z, aim.Pitch, aim.Yaw, aim.Roll})
        if (!std::isfinite(value)) return false;
    static CachedObjRef effect, sound, attenuation;
    static void* cdo = nullptr;
    static void* fn = nullptr;
    if (!effect.Get()) effect.Set(R::FindObject(L"eff_waterThrower", L"ParticleSystem"));
    if (!sound.Get()) sound.Set(R::FindObject(L"wade_Cue", L"SoundCue"));
    if (!attenuation.Get()) attenuation.Set(R::FindObject(L"att_default", L"SoundAttenuation"));
    if (!cdo) cdo = R::FindClassDefaultObject(L"GameplayStatics");
    if (!fn) if (void* cls = R::FindClass(L"GameplayStatics"))
        fn = R::FindFunction(cls, L"SpawnEmitterAtLocation");
    if (!cdo || !fn || !effect.Get()) return false;
    CachedObjRef live; live.Set(actor);
    ParamFrame f(fn);
    if (!f.valid() || !f.Set<void*>(L"WorldContextObject", actor) ||
        !f.Set<void*>(L"EmitterTemplate", effect.Get()) || !f.Set(L"Location", origin) ||
        !f.Set(L"Rotation", aim) || !f.Set(L"Scale", FVector{1,1,1}) ||
        !f.Set(L"bAutoDestroy", true) || !f.Set(L"bAutoActivateSystem", true) ||
        !Call(cdo, f) || !live.Get() || !f.Get<void*>(L"ReturnValue")) return false;
    if (sound.Get() && attenuation.Get())
        engine::PlaySoundAtLocation(actor, sound.Get(), origin, attenuation.Get(), 1.f, 1.5f);
    return true;
}
}
