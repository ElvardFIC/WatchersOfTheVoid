#include "ue_wrap/world/event_location.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/world/world_singleton.h"
#include <cmath>
#include <cstring>
namespace ue_wrap::event_location {
namespace {
namespace R=reflection;
template<class T> bool Field(void* obj,const wchar_t* name,T& value) {
    if(!obj || !R::IsLive(obj))return false;
    const int off=R::FindPropertyOffset(R::ClassOf(obj),name);
    if(off<0)return false;
    std::memcpy(&value,static_cast<uint8_t*>(obj)+off,sizeof(T));return true;
}
bool Family(void* obj,const wchar_t* name) {
    if(!obj || !R::IsLive(obj))return false;
    for(void* c=R::ClassOf(obj);c;c=R::SuperStructOf(c))if(R::NameEquals(R::NameOf(c),name))return true;
    return false;
}
bool Finite(const FVector& v){return std::isfinite(v.X)&&std::isfinite(v.Y)&&std::isfinite(v.Z);}
}
void* Controller(const wchar_t* field) {
    void* eventer=nullptr,*controller=nullptr;
    if(!Field(world_singleton::Gamemode(),L"eventer",eventer) || !Field(eventer,field,controller))return nullptr;
    return controller && R::IsLive(controller)?controller:nullptr;
}
void* TriggerBox(void* controller) {
    void* box=nullptr;
    return Family(controller,L"trigger_TBoxActivator_C") && Field(controller,L"triggerBox",box) &&
        Family(box,L"trigger_box_C")?box:nullptr;
}
bool Armed(void* box) {
    if(!Family(box,L"trigger_box_C"))return false;
    int32_t off=-1;uint8_t mask=0;
    return R::FindBoolProperty(R::ClassOf(box),L"isActive",off,mask) && off>=0 && mask &&
        (static_cast<uint8_t*>(box)[off]&mask)!=0;
}
bool ReadBox(void* box,Box& bounds) {
    void* component=nullptr;if(!Field(box,L"Box",component)||!component||!R::IsLive(component))return false;
    // Respect the native exact-class filter; never synthesize overlaps of props
    // or special-only zones just because a player happens to stand there.
    struct Array {void** data;int32_t num,max;};Array filter{};
    if(!Field(box,L"filter",filter)||filter.num<0||filter.num>64||filter.max<filter.num)return false;
    if(filter.num) {
        if(!filter.data)return false;
        bool player=false;
        for(int i=0;i<filter.num;++i)if(filter.data[i]&&R::IsLive(filter.data[i])&&
            R::NameEquals(R::NameOf(filter.data[i]),L"mainPlayer_C"))player=true;
        if(!player)return false;
    }
    CachedObjRef live;live.Set(component);
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(component),L"K2_GetComponentToWorld"));
    if(!f.valid()||!Call(component,f)||!live.Get()||!f.GetRaw(L"ReturnValue",&bounds.transform,sizeof(bounds.transform)))return false;
    return Field(component,L"BoxExtent",bounds.extent) && Finite(bounds.extent);
}
bool Contains(const Box& b,const FVector& p) {
    const auto& t=b.transform;
    if(!Finite(p)||!Finite(b.extent)||b.extent.X<=0||b.extent.Y<=0||b.extent.Z<=0)return false;
    const float norm=t.RotX*t.RotX+t.RotY*t.RotY+t.RotZ*t.RotZ+t.RotW*t.RotW;
    if(!std::isfinite(norm)||std::fabs(norm-1.f)>.01f)return false;
    // Inverse quaternion rotation followed by inverse scale; supports rotated
    // and mirrored/scaled trigger boxes without expanding their world AABB.
    const FVector d{p.X-t.TX,p.Y-t.TY,p.Z-t.TZ},q{-t.RotX,-t.RotY,-t.RotZ};
    const FVector cross{2*(q.Y*d.Z-q.Z*d.Y),2*(q.Z*d.X-q.X*d.Z),2*(q.X*d.Y-q.Y*d.X)};
    const FVector v{d.X+t.RotW*cross.X+q.Y*cross.Z-q.Z*cross.Y,
        d.Y+t.RotW*cross.Y+q.Z*cross.X-q.X*cross.Z,
        d.Z+t.RotW*cross.Z+q.X*cross.Y-q.Y*cross.X};
    if(!Finite(v)||!std::isfinite(t.SX)||!std::isfinite(t.SY)||!std::isfinite(t.SZ)||
       std::fabs(t.SX)<.0001f||std::fabs(t.SY)<.0001f||std::fabs(t.SZ)<.0001f)return false;
    return std::fabs(v.X/t.SX)<=b.extent.X && std::fabs(v.Y/t.SY)<=b.extent.Y && std::fabs(v.Z/t.SZ)<=b.extent.Z;
}
bool RunBox(void* box) {
    if(!Armed(box))return false;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(box),L"runAll"));
    return f.valid() && f.Set<int32_t>(L"allIndex",-1) && Call(box,f);
}
bool ReadAgravQuery(void* fn,const uint8_t* locals,Box& bounds) {
    if(!fn||!locals||!R::NameEquals(R::NameOf(fn),L"ExecuteUbergraph_trigger_agrav"))return false;
    void* owner=R::OuterOf(fn);
    if(!owner||!R::NameEquals(R::NameOf(owner),L"trigger_agrav_C"))return false;
    const int pos=R::FindPropertyOffset(fn,L"CallFunc_K2_GetComponentLocation_ReturnValue");
    const int ext=R::FindPropertyOffset(fn,L"CallFunc_GetScaledBoxExtent_ReturnValue");
    const int size=R::FunctionFrameSize(fn);
    if(pos<0||ext<0||pos>size||ext>size||sizeof(FVector)>size-pos||sizeof(FVector)>size-ext)return false;
    FVector center{};bounds={};std::memcpy(&center,locals+pos,sizeof(center));
    std::memcpy(&bounds.extent,locals+ext,sizeof(bounds.extent));bounds.transform=MakeTransform(center);
    return Finite(center)&&Finite(bounds.extent);
}
}
