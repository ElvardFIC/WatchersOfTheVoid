#include "ue_wrap/devices/kitchen_machine.h"
#include "ue_wrap/actors/swinger.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/component_calls.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine_component.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_physics.h"
#include <algorithm>
#include <cstring>

namespace ue_wrap::kitchen_machine {
namespace {
namespace R = reflection;
const wchar_t* const kClasses[] = {L"prop_microwave_C", L"prop_coffeeMaker_C",
    L"prop_coffeevending_C", L"prop_minioven_C", L"prop_bbq_C"};
CachedObjRef g_classes[5];

// These small layouts are measured on the LIVE class by name. Keep them with the class's
// slot and serial: a Blueprint reloaded by travel must never inherit stale field offsets.
struct Field {
    const wchar_t* name;
    R::InstanceOffset offset;
    explicit Field(const wchar_t* n) : name(n), offset(n) {}
    template<class T> bool Get(void* a, T& v) {
        const int32_t off = offset.Of(a);
        if (off < 0) return false;
        std::memcpy(&v, static_cast<const uint8_t*>(a) + off, sizeof(v));
        return true;
    }
    template<class T> bool Put(void* a, const T& v) {
        const int32_t off = offset.Of(a);
        if (off < 0) return false;
        std::memcpy(static_cast<uint8_t*>(a) + off, &v, sizeof(v));
        return true;
    }
};
// Blueprint bools are reflected as native byte bools in this game. Resolve their mask as well
// so a recook using a packed bool never writes its neighbours.
struct Bit {
    const wchar_t* name;
    CachedObjRef cls;
    int32_t off = -1;
    uint8_t mask = 0;
    explicit Bit(const wchar_t* n) : name(n) {}
    bool Resolve(void* a) {
        void* c = R::ClassOf(a);
        if (c != cls.Get()) {
            cls.Set(c);
            off = -1; mask = 0;
            R::FindBoolProperty(c, name, off, mask);
            if (off<0 || !mask) UE_LOGW("kitchen: %ls bool %ls did not resolve",R::ClassNameOf(a).c_str(),name);
        }
        return off >= 0 && mask != 0;
    }
    bool Get(void* a, bool& value) {
        if (!Resolve(a)) return false;
        value = (static_cast<const uint8_t*>(a)[off] & mask) != 0;
        return true;
    }
    bool Put(void* a, bool value) {
        if (!Resolve(a)) return false;
        auto& b = static_cast<uint8_t*>(a)[off];
        b = static_cast<uint8_t>(value ? b | mask : b & ~mask);
        return true;
    }
};
struct Layout {
    Bit active{L"active"}, powered{L"powered"}, cup{L"hasCup"}, coffee{L"hasCoffee"},
        ready{L"coffeeReady"}, processing{L"processing"}, hasPower{L"hasPower"},
        opened{L"opened"}, inputSwitch{L"input_switch"}, inputTemp{L"input_temp"};
    Field mode{L"mode"}, timeMode{L"timeMode"}, time{L"time"}, timer{L"timer"},
        power{L"power"}, door{L"door"}, doorObj{L"doorObj"}, child{L"ChildActor"},
        light{L"PointLight"}, loop{L"microwave_loop"}, spin{L"spin"}, buzz{L"audio_buzz"},
        coals{L"coals"}, burn{L"a"};
};
Layout g_layouts[5];
Layout& Fields(Kind k) { return g_layouts[static_cast<int>(k)-1]; }
void* Object(Field& f, void* a) {
    void* p = nullptr;
    return f.Get(a,p) && p && R::IsLive(p) ? p : nullptr;
}
bool ReadFlag(Bit& f, void* a, uint8_t bit, uint8_t& flags) {
    bool b = false;
    if (!f.Get(a,b)) return false;
    if (b) flags |= bit;
    return true;
}
}

Kind Type(void* actor) {
    void* cls = actor ? R::ClassOf(actor) : nullptr;
    if (!cls) return Kind::None;
    for (int i=0;i<5;++i) {
        void* base = g_classes[i].Get();
        if (!base) {
            base = object_index::ClassByName(kClasses[i]);
            if (base) g_classes[i].Set(base);
        }
        if (base && R::IsDescendantOfAny(cls,&base,1)) return static_cast<Kind>(i+1);
    }
    return Kind::None;
}
void* Door(void* actor) {
    const Kind k = Type(actor);
    if (k != Kind::Microwave && k != Kind::MiniOven) return nullptr;
    auto& l = Fields(k);
    void* comp = Object(l.door,actor);
    return comp ? Object(l.child,comp) : nullptr;
}
void* DoorParent(void* child) {
    // Called from the global swing verb: do not dispatch another Blueprint here.
    // Open/Close also occur on non-actor objects; never read Actor fields there.
    if (!child || !swinger::EnsureResolved() || !swinger::IsSwinger(child)) return nullptr;
    void* a = engine::ParentActorOf(child);
    return a && Door(a)==child ? a : nullptr;
}
bool OwnsDoor(void* actor) {
    // Child actors can have locally minted keys. Only the parent's identity is shared.
    return Door(actor)!=nullptr;
}
bool SetDoor(void* actor, bool opened) {
    void* d = Door(actor);
    if (!d || !swinger::EnsureResolved() || !swinger::IsSwinger(d)) return false;
    bool current = false;
    if (!swinger::TryReadOpen(d,current)) return false;
    // Open/Close change a constraint's target, not the mesh transform. A kinematic
    // child plays the sound and changes `opened` but its door never actually moves.
    void* mesh = prop::GetStaticMesh(d);
    if (mesh && !prop::IsFrozen(d) && !engine::IsComponentSimulatingPhysics(mesh))
        engine::SetComponentSimulatePhysics(mesh,true);
    if (current==opened) return true;
    return opened ? swinger::CallOpen(d,false) : swinger::CallClose(d);
}
namespace {
bool TransformOf(void* object,const wchar_t* verb,FTransform& out) {
    if (!object) return false;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(object),verb));
    return f.valid() && Call(object,f) && f.GetRaw(L"ReturnValue",&out,sizeof(out));
}
bool TransformMath(const wchar_t* verb,const wchar_t* secondName,const FTransform& a,const FTransform& b,FTransform& out) {
    static CachedObjRef cdo;
    void* object=cdo.Get();
    if (!object) { object=R::FindClassDefaultObject(L"KismetMathLibrary"); if (object) cdo.Set(object); }
    if (!object) return false;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(object),verb));
    return f.valid() && f.Set<FTransform>(L"A",a) && f.Set<FTransform>(secondName,b) &&
        Call(object,f) && f.GetRaw(L"ReturnValue",&out,sizeof(out));
}
}
bool ReadDoorPose(void* actor,FTransform& out) {
    void* child=Door(actor);
    void* mesh=child ? prop::GetStaticMesh(child) : nullptr;
    FTransform door{},parent{};
    return TransformOf(mesh,L"K2_GetComponentToWorld",door) && TransformOf(actor,L"GetTransform",parent) &&
        TransformMath(L"MakeRelativeTransform",L"RelativeTo",door,parent,out);
}
bool ApplyDoorPose(void* actor,const FTransform& pose) {
    void* child=Door(actor);
    void* mesh=child ? prop::GetStaticMesh(child) : nullptr;
    if (!mesh) return false;
    FTransform parent{},world{};
    if (!TransformOf(actor,L"GetTransform",parent) || !TransformMath(L"ComposeTransforms",L"B",pose,parent,world)) return false;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(mesh),L"K2_SetWorldTransform"));
    // Keep the native hinge intact. Teleport the rigid body, never sweep its old
    // closed position through an item another peer is inserting into the oven.
    return f.valid() && f.Set<FTransform>(L"NewTransform",world) && f.Set<bool>(L"bTeleport",true) && Call(mesh,f);
}
void* DoorUser(void* actor) {
    void* child=Door(actor);
    static Field user{L"player"};
    return child ? Object(user,child) : nullptr;
}
bool MoveDoor(void* actor,void* player,const FVector2D& mouse) {
    void* child=Door(actor);
    if (!child || !player || !R::IsLive(player)) return false;
    static Field user{L"player"};
    void* saved=nullptr;
    if (!user.Get(child,saved)) return false;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(child),L"mouseAssign"));
    if (!f.valid() || !f.Set<FVector2D>(L"mouseXY",mouse)) return false;
    if (!SetDoor(actor,true)) return false;
    user.Put(child,player);
    const bool ok=Call(child,f);
    user.Put(child,saved);
    return ok;
}
bool Read(void* actor, State& out) {
    State s{};
    s.kind = Type(actor);
    if (s.kind==Kind::None) return false;
    auto& l = Fields(s.kind);
    bool ok = true;
    if (s.kind==Kind::Microwave || s.kind==Kind::MiniOven) {
        ok &= ReadFlag(l.active,actor,Active,s.flags);
        ok &= ReadFlag(l.powered,actor,Powered,s.flags);
        if (void* d = Door(actor)) {
            bool opened = false;
            if (swinger::EnsureResolved() && swinger::TryReadOpen(d,opened) && opened) s.flags |= Opened;
        }
        s.hasDoorPose=ReadDoorPose(actor,s.doorPose);
    }
    if (s.kind==Kind::Microwave) {
        ok &= l.timeMode.Get(actor,s.mode);
        ok &= l.time.Get(actor,s.remaining);
        ok &= l.timer.Get(actor,s.timer);
    } else if (s.kind==Kind::MiniOven) {
        ok &= l.mode.Get(actor,s.mode);
        ok &= l.power.Get(actor,s.power);
    } else if (s.kind==Kind::CoffeeMaker) {
        ok &= ReadFlag(l.cup,actor,HasCup,s.flags);
        ok &= ReadFlag(l.coffee,actor,HasCoffee,s.flags);
        ok &= ReadFlag(l.ready,actor,CoffeeReady,s.flags);
        ok &= ReadFlag(l.hasPower,actor,Powered,s.flags);
    } else if(s.kind==Kind::Grill) {
        ok &= ReadFlag(l.active,actor,Active,s.flags);
        ok &= l.coals.Get(actor,s.coals);
        ok &= l.burn.Get(actor,s.burn);
    } else {
        ok &= ReadFlag(l.processing,actor,Processing,s.flags);
    }
    if (ok) out=s;
    return ok;
}
bool Apply(void* actor, const State& s) {
    if (Type(actor)!=s.kind || s.kind==Kind::None) return false;
    auto& l = Fields(s.kind);
    State before{};
    if (!Read(actor,before)) return false;
    // Doors retain their own native swing animation and callbacks. Write the canonical fields
    // AFTER that call: a microwave's door callback can turn its running bit off.
    if (s.kind==Kind::Microwave || s.kind==Kind::MiniOven) {
        if (OwnsDoor(actor)) SetDoor(actor,(s.flags&Opened)!=0);
        if (s.hasDoorPose) ApplyDoorPose(actor,s.doorPose);
        l.active.Put(actor,(s.flags&Active)!=0);
        l.powered.Put(actor,(s.flags&Powered)!=0);
    }
    if (s.kind==Kind::Microwave) {
        l.opened.Put(actor,(s.flags&Opened)!=0);
        l.timeMode.Put(actor,s.mode); l.time.Put(actor,s.remaining); l.timer.Put(actor,s.timer);
        engine::SetComponentVisible(Object(l.light,actor),(s.flags&Powered) && (s.flags&(Active|Opened)),false);
        void* loop = Object(l.loop,actor);
        const bool running=(s.flags&(Active|Powered))==(Active|Powered);
        const bool wasRunning=(before.flags&(Active|Powered))==(Active|Powered);
        if (running!=wasRunning) component_calls::SetActive(loop,running,false);
    } else if (s.kind==Kind::MiniOven) {
        l.mode.Put(actor,s.mode); l.power.Put(actor,s.power);
        if ((before.flags^s.flags)&(Active|Powered)) component_calls::CallParamlessNamed(actor,L"upd");
    } else if (s.kind==Kind::CoffeeMaker) {
        l.cup.Put(actor,(s.flags&HasCup)!=0); l.coffee.Put(actor,(s.flags&HasCoffee)!=0);
        l.ready.Put(actor,(s.flags&CoffeeReady)!=0); l.hasPower.Put(actor,(s.flags&Powered)!=0);
        if (before.flags!=s.flags) component_calls::CallParamlessNamed(actor,L"upd");
    } else if(s.kind==Kind::Grill) {
        // upd paints the coals, fire and burning loop. It does not spend fuel or
        // heat food; replica ReceiveTick is suppressed by the kitchen controller.
        if(!l.active.Put(actor,(s.flags&Active)!=0)||!l.coals.Put(actor,s.coals)||!l.burn.Put(actor,s.burn))return false;
        return component_calls::CallParamlessNamed(actor,L"upd");
    } else {
        l.processing.Put(actor,(s.flags&Processing)!=0);
        if ((before.flags^s.flags)&Processing) {
            void* buzz=Object(l.buzz,actor);
            component_calls::SetActive(buzz,(s.flags&Processing)!=0,false);
        }
    }
    return true;
}
uint8_t ReadControl(void* actor) {
    if (Type(actor)!=Kind::MiniOven) return 0;
    auto& l=Fields(Kind::MiniOven);
    bool temp=false, sw=false;
    l.inputTemp.Get(actor,temp); l.inputSwitch.Get(actor,sw);
    return temp ? 2 : (sw ? 1 : 0);
}
bool Press(void* actor, void* player, uint8_t action, uint8_t control) {
    const Kind k=Type(actor);
    if (k==Kind::None || k==Kind::Grill || (action!=4 && !(k==Kind::Microwave && action==8))) return false;
    if (k==Kind::MiniOven) {
        if (control!=1 && control!=2) return false;
    }
    void* fn=R::FindDispatchFunctionCached(R::ClassOf(actor),L"actionOptionIndex");
    if (!fn) return false;
    ParamFrame f(fn);
    if (!f.valid() || !f.Set<uint8_t>(L"action",action)) return false;
    f.Set<void*>(L"player",player);
    if (k!=Kind::MiniOven) return Call(actor,f);
    auto& l=Fields(k);
    bool sw=false,temp=false;
    if (!l.inputSwitch.Get(actor,sw) || !l.inputTemp.Get(actor,temp)) return false;
    l.inputSwitch.Put(actor,control==1); l.inputTemp.Put(actor,control==2);
    const bool ok=Call(actor,f);
    l.inputSwitch.Put(actor,sw); l.inputTemp.Put(actor,temp);
    return ok;
}
bool SetGrillFire(void* actor,bool active) {
    State state{};if(!Read(actor,state)||state.kind!=Kind::Grill)return false;
    if(active) {
        if(state.flags&Active)return true;
        if(state.coals<=0)return false;
        return component_calls::CallParamlessNamed(actor,L"attemptIgnite");
    }
    return Fields(Kind::Grill).active.Put(actor,false)&&component_calls::CallParamlessNamed(actor,L"upd");
}
void Animate(void* actor, float seconds) {
    if (Type(actor)!=Kind::Microwave) return;
    auto& l=Fields(Kind::Microwave);
    bool active=false,powered=false;
    if (!l.active.Get(actor,active) || !active || !l.powered.Get(actor,powered) || !powered) return;
    void* spin=Object(l.spin,actor);
    if (!spin) return;
    void* fn=R::FindDispatchFunctionCached(R::ClassOf(spin),L"K2_AddLocalRotation");
    if (!fn) return;
    ParamFrame f(fn);
    const FRotator rot{0.f,60.f*std::clamp(seconds,0.f,0.1f),0.f};
    if (f.valid() && f.Set<FRotator>(L"DeltaRotation",rot)) Call(spin,f);
}
}
