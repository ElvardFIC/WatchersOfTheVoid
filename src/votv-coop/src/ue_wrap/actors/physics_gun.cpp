#include "ue_wrap/actors/physics_gun.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/component_calls.h"
#include <cmath>
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
    int depth = 0;
    for (void* c = cls; c && depth++ < 64; c = R::SuperStructOf(c)) {
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
void* HeldActor(void* gun) {
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
        R::OuterOf(component) != target || world_identity::WorldOf(target) != world) return nullptr;
    return target;
}
void* HeldProp(void* gun) {
    void* actor = HeldActor(gun);
    return actor && prop::IsDescendantOfProp(actor) ? actor : nullptr;
}

bool ReleaseHeldProp(void* gun, void* target, void* player) {
    if (!target || HeldProp(gun) != target) return false;
    CachedObjRef identity;
    identity.Set(gun);
    // release() is an empty extension event in 0.9.0n. The real input-release
    // graph clears hold, breaks the soft constraint and restores compBodyInst.
    ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(gun), L"playerHandRelease_LMB"));
    return frame.valid() && frame.Set<void*>(L"player", player) && Call(gun, frame) &&
        (!identity.Is(gun) || HeldProp(gun) != target);
}

namespace {
uint8_t ToolKind(void* gun) {
    void* c=R::ClassOf(gun);
    for (int depth=0;c && depth<64;++depth,c=R::SuperStructOf(c)) {
        if (R::NameEquals(R::NameOf(c),L"prop_physgun_C")) return 1;
        if (R::NameEquals(R::NameOf(c),L"prop_gravgun_C")) return 2;
    }
    return 0;
}
R::InstanceOffset beam{L"beam"}, start{L"eff_gphysgunBeam_start"}, end{L"eff_gphysgunBeam_end"},
    pivot{L"softPivot"}, loop{L"object_hold_loop"}, grab{L"object_grab"},
    thrown{L"object_throw"}, released{L"object_release"}, visualObject{L"obj"},
    gravityHold{L"eff_gravigunN1"}, gravityAttract{L"eff_gravigunN2"};
void* Pointer(void* gun,R::InstanceOffset& field) {
    const int off=field.Of(gun);
    void* value=nullptr;
    if (off>=0) std::memcpy(&value,static_cast<uint8_t*>(gun)+off,sizeof(value));
    return value;
}
void* Component(void* gun,R::InstanceOffset& field) {
    void* value=Pointer(gun,field);
    return value && R::IsLive(value) ? value : nullptr;
}
bool Active(void* comp) {
    if (!comp) return false;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(comp),L"IsActive"));
    return f.valid() && Call(comp,f) && f.Get<bool>(L"ReturnValue");
}
bool Location(void* comp,FVector& out) {
    if (!comp) return false;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(comp),L"K2_GetComponentLocation"));
    return f.valid() && Call(comp,f) && f.GetRaw(L"ReturnValue",&out,sizeof(out));
}
void Place(void* comp,const FVector& at) {
    if (!comp) return;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(comp),L"K2_SetWorldLocation"));
    if (f.valid() && f.Set<FVector>(L"NewLocation",at) && f.Set<bool>(L"bSweep",false) &&
        f.Set<bool>(L"bTeleport",true)) Call(comp,f);
}
}
Visuals ReadVisuals(void* gun) {
    Visuals s{};
    if (!gun || !R::IsLive(gun)) return s;
    CachedObjRef ref; ref.Set(gun);
    s.kind=ToolKind(gun);
    if (s.kind==1) {
        if (Resolve(gun) && (static_cast<uint8_t*>(gun)[g_hold]&g_mask) &&
            Location(Component(gun,pivot),s.target)) s.flags=1;
    } else if (s.kind==2) {
        s.object=Component(gun,visualObject);
        if (ref.Get() && Active(Component(gun,loop))) s.flags|=1;
        if (ref.Get() && Active(Component(gun,grab))) s.flags|=2;
        if (ref.Get() && Active(Component(gun,thrown))) s.flags|=4;
        if (ref.Get() && Active(Component(gun,released))) s.flags|=8;
        if (ref.Get() && Active(Component(gun,gravityAttract))) s.flags|=16;
    }
    if (!ref.Get()) return {};
    if (!std::isfinite(s.target.X) || !std::isfinite(s.target.Y) || !std::isfinite(s.target.Z)) return {};
    return s;
}
void ApplyVisuals(void* gun,const Visuals& s) {
    if (!gun || !R::IsLive(gun)) return;
    const uint8_t kind=ToolKind(gun);
    if (!kind) return;
    CachedObjRef ref; ref.Set(gun);
    const auto toggle=[&](R::InstanceOffset& field,bool active) {
        if (!ref.Get()) return;
        void* component=Component(gun,field);
        if (!component) return;
        CachedObjRef compRef; compRef.Set(component);
        // These are first-person components in the native gun. A remote hand
        // needs their third-person visibility, without running weapon gameplay.
        const wchar_t* verbs[]={L"SetOnlyOwnerSee",L"SetOwnerNoSee"};
        const wchar_t* args[]={L"bNewOnlyOwnerSee",L"bNewOwnerNoSee"};
        for (int i=0;i<2;++i) {
            if (!compRef.Get()) return;
            ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(component),verbs[i]));
            if (f.valid() && f.Set<bool>(args[i],false)) Call(component,f);
        }
        if (compRef.Get()) component_calls::SetActive(component,active,false);
    };
    const uint8_t flags=s.kind==kind ? s.flags : 0;
    if (kind==2) {
        // Native `set` ONLY paints lights/hold particles and creates/destroys
        // its four beam components. Forces remain in comp_gravitygun, which is
        // never replayed. Its normal tick updates those cosmetic beam endpoints.
        void* object=(flags&1) && s.object && R::IsLive(s.object) ? s.object : nullptr;
        if (Pointer(gun,visualObject)!=object) {
            ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(gun),L"set"));
            if (f.valid() && f.Set<void*>(L"obj",object)) Call(gun,f);
        }
        if (!ref.Get()) return;
        ParamFrame attract(R::FindDispatchFunctionCached(R::ClassOf(gun),L"attract"));
        if (attract.valid() && attract.Set<bool>(L"Condition",(flags&16)!=0)) Call(gun,attract);
        if (!ref.Get()) return;
        toggle(gravityHold,(flags&1)!=0); toggle(gravityAttract,(flags&16)!=0);
        toggle(loop,flags&1); toggle(grab,flags&2); toggle(thrown,flags&4); toggle(released,flags&8);
        return;
    }
    const bool on=(flags&1) && std::isfinite(s.target.X) && std::isfinite(s.target.Y) && std::isfinite(s.target.Z);
    if (on) {
        Place(Component(gun,pivot),s.target);
        if (!ref.Get()) return;
        Place(Component(gun,end),s.target);
        if (!ref.Get()) return;
        FVector muzzle{};
        void* particle=Component(gun,beam);
        CachedObjRef particleRef; particleRef.Set(particle);
        if (particle && Location(Component(gun,start),muzzle) && ref.Get() && particleRef.Get()) {
            ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(particle),L"SetBeamEndPoint"));
            if (f.valid() && f.Set<int32_t>(L"EmitterIndex",0) && f.Set<FVector>(L"NewEndPoint",muzzle)) Call(particle,f);
        }
    }
    toggle(beam,on); toggle(start,on); toggle(end,on);
}
}
