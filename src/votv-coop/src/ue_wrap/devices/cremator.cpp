#include "ue_wrap/devices/cremator.h"

#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/component_calls.h"
#include "ue_wrap/engine/world_identity.h"
#include "coop/element/portable_identity.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/actors/swinger.h"

#include <cstdint>
#include <cstring>
#include <chrono>
#include <cmath>

namespace ue_wrap::cremator {
namespace {
namespace R = reflection;
CachedObjRef g_machineClass;
CachedObjRef g_doorClass;
UE_FACT_FN(kRowIgniteInside, "ue_wrap.cremator", L"cremator_C", L"igniteInside", Declared);
struct BoolField { const wchar_t* name; void* cls=nullptr; int32_t off=-1; uint8_t mask=0; };
BoolField g_active{L"isActive"}, g_anim{L"anim"}, g_doorOpened{L"opened"},g_closed{L"isClosed"};
BoolField g_lookingHandle{L"lookingAt_handle"}, g_lookingValve{L"lookingAt_valve"};
R::InstanceOffset g_windup{L"windup"},g_gas{L"gasAmount"},g_actors{L"actors"},g_health{L"health"};
R::InstanceOffset g_swingerDoor{L"swingerDoor"},g_firebox{L"firebox"},g_burning{L"burning"},g_handle{L"handle"};
uint32_t g_generation = 0;
std::chrono::steady_clock::time_point g_nextScan{};
std::vector<void*> g_machines;
std::vector<int32_t> g_machineIndices;

bool IsOf(void* actor, void* base) {
    if (!actor || !base) return false;
    void* cls = R::ClassOf(actor);
    void* bases[] = {base};
    return cls && R::IsDescendantOfAny(cls, bases, 1);
}
bool ReadBoolField(void* actor, BoolField& field, bool& value) {
    if (!IsCremator(actor)) return false;
    void* cls=R::ClassOf(actor);
    if(field.cls!=cls){field.cls=cls;field.off=-1;field.mask=0;
        if(!R::FindBoolProperty(cls,field.name,field.off,field.mask)||field.off<0||!field.mask)field.off=-1;}
    if(field.off<0)return false;
    value=(static_cast<const uint8_t*>(actor)[field.off]&field.mask)!=0;return true;
}
bool WriteBoolField(void* actor, BoolField& field, bool value) {
    bool current = false;
    if (!ReadBoolField(actor, field, current)) return false;
    if (current == value) return true;
    auto* bytes = static_cast<uint8_t*>(actor);
    if (value) bytes[field.off] |= field.mask;
    else bytes[field.off] &= static_cast<uint8_t>(~field.mask);
    return true;
}
bool ReadFloatField(void* actor,R::InstanceOffset& field,float& value){if(!IsCremator(actor))return false;int32_t off=field.Of(actor);if(off<0)return false;std::memcpy(&value,static_cast<const uint8_t*>(actor)+off,sizeof(value));return true;}
void* ReadObjectField(void* actor,R::InstanceOffset& field){if(!IsCremator(actor))return nullptr;int32_t off=field.Of(actor);if(off<0)return nullptr;void* p=nullptr;std::memcpy(&p,static_cast<const uint8_t*>(actor)+off,sizeof(p));return p&&R::IsLive(p)?p:nullptr;}
}

bool EnsureResolved() {
    if (g_machineClass.Get() && g_doorClass.Get()) return true;
    if (!g_machineClass.Get()) g_machineClass.Set(object_index::ClassByName(L"cremator_C"));
    if (!g_doorClass.Get()) g_doorClass.Set(object_index::ClassByName(L"prop_swinger_crematorDoor_C"));
    return g_machineClass.Get() && g_doorClass.Get();
}

bool IsCremator(void* actor) { EnsureResolved(); return IsOf(actor, g_machineClass.Get()); }

bool ReadAnim(void* actor,bool& value){return ReadBoolField(actor,g_anim,value);}
bool ReadActive(void* actor,bool& value){return ReadBoolField(actor,g_active,value);}
bool ReadClosed(void* actor,bool& value){return ReadBoolField(actor,g_closed,value);}
bool ReadLookingAtHandle(void* actor,bool& value){return ReadBoolField(actor,g_lookingHandle,value);}
bool WriteLookingAtHandle(void* actor,bool value){return WriteBoolField(actor,g_lookingHandle,value);}
bool ReadLookingAtValve(void* actor,bool& value){return ReadBoolField(actor,g_lookingValve,value);}
bool ReadWindup(void* actor,float& value){return ReadFloatField(actor,g_windup,value);}
bool ReadGasAmount(void* actor,float& value){return ReadFloatField(actor,g_gas,value);}
void* Firebox(void* actor){return ReadObjectField(actor,g_firebox);}
void* Handle(void* actor){return ReadObjectField(actor,g_handle);}
bool HandleLocation(void* actor,FVector& at){void* component=Handle(actor);return component&&ue_wrap::engine::TryGetComponentLocation(component,at);}
void* BurnVolume(void* actor){return ReadObjectField(actor,g_burning);}
void* Door(void* actor){if(!IsCremator(actor))return nullptr;const int32_t off=g_swingerDoor.Of(actor);if(off<0)return nullptr;void* p=nullptr;std::memcpy(&p,static_cast<const uint8_t*>(actor)+off,sizeof(p));return p&&R::IsLive(p)?p:nullptr;}
bool ReadDoorOpened(void* actor,bool& value){void* door=Door(actor);if(!door)return false;void* cls=R::ClassOf(door);if(g_doorOpened.cls!=cls){g_doorOpened.cls=cls;g_doorOpened.off=-1;g_doorOpened.mask=0;if(!R::FindBoolProperty(cls,g_doorOpened.name,g_doorOpened.off,g_doorOpened.mask)||g_doorOpened.off<0||!g_doorOpened.mask)g_doorOpened.off=-1;}if(g_doorOpened.off<0)return false;value=(static_cast<const uint8_t*>(door)[g_doorOpened.off]&g_doorOpened.mask)!=0;return true;}
bool OverlapsBurnVolume(void* actor,void* other){if(!IsCremator(actor)||!other||!R::IsLive(other))return false;const int32_t off=g_burning.Of(actor);if(off<0)return false;void* volume=nullptr;std::memcpy(&volume,static_cast<const uint8_t*>(actor)+off,sizeof(volume));if(!volume||!R::IsLive(volume))return false;void* fn=R::FindDispatchFunctionCached(R::ClassOf(volume),L"IsOverlappingActor");if(!fn)return false;ParamFrame f(fn);if(!f.valid())return false;f.Set<void*>(L"Other",other);return Call(volume,f)&&f.Get<bool>(L"ReturnValue");}
bool FireboxLocation(void* actor,FVector& at){void* component=Firebox(actor);return component&&ue_wrap::engine::TryGetComponentLocation(component,at);}
bool ShutDoor(void* actor,bool& wasOpened){wasOpened=false;void* door=Door(actor);return !(door&&ReadDoorOpened(actor,wasOpened)&&wasOpened)||ue_wrap::swinger::CallClose(door);}
bool OpenDoor(void* actor,bool& opened){opened=false;void* door=Door(actor);if(!door)return false;const bool ran=(ReadDoorOpened(actor,opened)&&opened)||ue_wrap::swinger::CallOpen(door,false);ReadDoorOpened(actor,opened);return ran;}
Machine ReadMachine(void* actor){Machine m;if(!IsCremator(actor))return m;int32_t actors=g_actors.Of(actor);if(actors>=0)std::memcpy(&m.actors,static_cast<const uint8_t*>(actor)+actors+sizeof(void*),sizeof(m.actors));int32_t health=g_health.Of(actor);if(health<0)return m;void* data=nullptr;int32_t n=0;const auto* base=static_cast<const uint8_t*>(actor)+health;std::memcpy(&data,base,sizeof(data));std::memcpy(&n,base+sizeof(void*),sizeof(n));if(n>0&&data)std::memcpy(&m.health0,data,sizeof(m.health0));return m;}
bool LoopActor(void* callerFunction,const uint8_t* callerLocals,void*& actor){static void* clsCache=nullptr;static void* uber=nullptr;static int32_t uberIdx=-1,off=-1;actor=nullptr;void* cls=g_machineClass.Get();if(!cls||!callerFunction||!callerLocals)return false;if(cls!=clsCache||(uber&&(!R::IsLiveByIndex(uber,uberIdx)||R::OuterOf(uber)!=cls))){clsCache=cls;uber=R::FindFunction(cls,L"ExecuteUbergraph_cremator");uberIdx=uber?R::InternalIndexOf(uber):-1;off=uber&&ue_wrap::facts::ResolveOn(kRowIgniteInside,cls)?R::FindPropertyOffset(uber,L"CallFunc_Array_Get_Item"):-1;}if(off<0||callerFunction!=uber)return false;std::memcpy(&actor,callerLocals+off,sizeof(actor));return actor!=nullptr;}

const std::vector<void*>& Instances() {
    EnsureResolved();
    const uint32_t gen = world_identity::Generation();
    const auto now=std::chrono::steady_clock::now();
    if (g_generation == gen) {
        bool live = true;
        for (size_t i = 0; i < g_machines.size(); ++i)
            if (!R::IsLiveByIndex(g_machines[i], g_machineIndices[i])) { live = false; break; }
        if (live && (!g_machines.empty() || now<g_nextScan)) return g_machines;
    }
    g_machines.clear();
    g_machineIndices.clear();
    if (void* machineClass=g_machineClass.Get()) {
        const int32_t n = R::NumObjects();
        for (int32_t i = 0; i < n; ++i) {
            void* a = R::ObjectAt(i);
            if (a && IsOf(a, machineClass) && R::IsLive(a) && !R::NameStartsWith(R::NameOf(a), L"Default__")) {
                g_machines.push_back(a);
                g_machineIndices.push_back(R::InternalIndexOf(a));
            }
        }
    }
    g_generation = gen;
    g_nextScan=now+std::chrono::seconds(1);
    return g_machines;
}

Nearest FindNearestFirebox(const FVector& from){Nearest out{};out.from=from;out.best=INFINITY;for(void* actor:Instances()){FVector at{};if(!FireboxLocation(actor,at))continue;const float dx=at.X-from.X,dy=at.Y-from.Y,dz=at.Z-from.Z;const float d2=dx*dx+dy*dy+dz*dz;if(d2<out.best){out.best=d2;out.actor=actor;out.at=at;}}return out;}
MoveResult FindFireboxMove(const FVector& from,FireboxMove& out){out=FireboxMove{};const Nearest nearest=FindNearestFirebox(from);if(!nearest.actor)return MoveResult::NoCremator;out.actor=nearest.actor;out.at=nearest.at;return ue_wrap::engine::TryGetActorRotation(nearest.actor,out.rot)?MoveResult::Found:MoveResult::RotationUnread;}

namespace {
int32_t BoolOffset(void* actor, const wchar_t* name) {
    void* cls = actor ? R::ClassOf(actor) : nullptr;
    return cls ? R::FindPropertyOffset(cls, name) : -1;
}
bool ReadBool(void* actor, const wchar_t* name, bool& out) {
    const int32_t off = BoolOffset(actor, name);
    if (off < 0) return false;
    std::memcpy(&out, reinterpret_cast<const uint8_t*>(actor) + off, sizeof(out));
    return true;
}
}

bool CanUseLever(void* actor) {
    bool info = false, looking = false;
    return CanReplayLever(actor) && ReadBool(actor, L"lookingAt_info", info) && info &&
           ReadBool(actor, L"lookingAt_handle", looking) && looking;
}

bool CanReplayLever(void* actor) {
    bool animating = true;
    return IsCremator(actor) && ReadBool(actor, L"anim", animating) && !animating;
}

bool TryReadLeverActive(void* actor, bool& active) {
    return IsCremator(actor) && ReadBool(actor, L"isActive", active);
}


bool CallUseLever(void* actor) {
    if (!IsCremator(actor)) return false;
    void* fn = R::FindFunction(R::ClassOf(actor), L"useLever");
    if (!fn) return false;
    ue_wrap::ParamFrame frame(fn);
    return frame.valid() && ue_wrap::Call(actor, frame);
}


std::wstring LeverWireKey(void* actor) {
    if (!IsCremator(actor)) return {};
    const std::wstring base = coop::element::PortableWireKey(actor);
    return base.empty() ? std::wstring{} : base + L"/lever";
}

void* FindByLeverWireKey(const std::wstring& key) {
    constexpr wchar_t suffix[] = L"/lever";
    if (key.size() <= 6 || key.compare(key.size() - 6, 6, suffix) != 0) return nullptr;
    const std::wstring base = key.substr(0, key.size() - 6);
    void* match = nullptr;
    for (void* actor : Instances()) {
        if (!actor || coop::element::PortableWireKey(actor) != base) continue;
        if (match && match != actor) return nullptr;  // never guess between duplicate identities
        match = actor;
    }
    return match;
}
}
