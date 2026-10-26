#include "ue_wrap/actors/rufus.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/engine/engine.h"
#include <cmath>
#include <cstring>
namespace ue_wrap::rufus {
namespace {
namespace R=reflection;
struct Live {
    CachedObjRef object,cls;uint32_t generation=world_identity::Generation();void* world=world_identity::CurrentWorld();
    explicit Live(void* p=nullptr){if(p&&R::IsLive(p)){object.Set(p);cls.Set(R::ClassOf(p));}}
    bool Current()const{return world&&world==world_identity::CurrentWorld()&&generation==world_identity::Generation()&&
        object.Get()&&cls.Get()&&R::ClassOf(object.Get())==cls.Get();}
};
template<class T>struct Field {
    Live owner;const wchar_t* name;int offset=-1;
    Field(const Live& object,const wchar_t* property):owner(object),name(property){if(owner.Current())offset=R::FindPropertyOffset(owner.cls.Get(),name);}
    bool Current()const{return offset>=0&&owner.Current()&&R::FindPropertyOffset(owner.cls.Get(),name)==offset&&owner.Current();}
    bool Read(T& out)const{if(!Current())return false;T value{};std::memcpy(&value,static_cast<uint8_t*>(owner.object.Get())+offset,sizeof(value));
        if(!Current())return false;out=value;return true;}
    bool Write(const T& value)const{if(!Current())return false;std::memcpy(static_cast<uint8_t*>(owner.object.Get())+offset,&value,sizeof(value));return Current();}
};
bool Typed(const Live& object,const wchar_t* cls){
    void* p=object.cls.Get();
    for(int i=0;p&&i<64&&object.Current();++i,p=R::SuperStructOf(p)){
        if(!R::IsLive(p))return false;if(R::NameEquals(R::NameOf(p),cls))return true;
    }
    return false;
}
struct Mesh {
    Live actor;Field<void*> field;Live mesh;
    explicit Mesh(void* p):actor(p),field(actor,L"Mesh") {void* m=nullptr;if(IsFamily(p)&&field.Read(m))mesh=Live(m);}
    bool Current()const{void* p=nullptr;return actor.Current()&&mesh.Current()&&Typed(mesh,L"SkeletalMeshComponent")&&
        field.Read(p)&&p==mesh.object.Raw()&&actor.Current()&&mesh.Current();}
};
bool Slot(const ParamFrame& frame,const wchar_t* name,int bytes){
    const int offset=frame.ParamOffset(name),size=frame.FrameSize();
    if(!frame.valid()||offset<0||bytes<=0||size<bytes||offset>size-bytes)return false;
    for(const auto& parameter:R::FunctionParams(frame.function()))
        if(!_wcsicmp(parameter.name.c_str(),name))return parameter.offset==offset&&parameter.size==bytes;
    return false;
}
bool Finite(const FRotator& r){return std::isfinite(r.Pitch)&&std::isfinite(r.Yaw)&&std::isfinite(r.Roll);}
}
bool IsFamilyClass(void* cls){
    for(int i=0;cls&&i<64;++i,cls=R::SuperStructOf(cls)){
        if(!R::IsLive(cls))return false;if(R::NameEquals(R::NameOf(cls),L"rufus_C"))return true;
    }
    return false;
}
bool IsFamily(void* actor){return actor&&R::IsLive(actor)&&IsFamilyClass(R::ClassOf(actor));}
bool PreparePresentation(void* actor){
    if(!IsFamily(actor))return false;
    const Mesh body(actor);if(!body.Current())return false;
    const Field<void*> armField(body.actor,L"SpringArm"),animField(body.mesh,L"AnimScriptInstance");void* p=nullptr;
    if(!armField.Read(p))return false;const Live arm(p);
    if(!animField.Read(p))return false;const Live anim(p);
    if(!Typed(arm,L"SpringArmComponent")||!Typed(anim,L"rufus_Skeleton_AnimBlueprint_C"))return false;
    const Field<void*> pawn(anim,L"pawn"),parent(body.mesh,L"AttachParent");
    const auto current=[&]{void* a=nullptr;void* b=nullptr;return body.Current()&&arm.Current()&&anim.Current()&&
        armField.Read(a)&&a==arm.object.Raw()&&animField.Read(b)&&b==anim.object.Raw()&&pawn.Current()&&parent.Current();};
    void* attached=nullptr;if(!current()||!parent.Read(attached))return false;
    if(attached!=arm.object.Raw()){
        CachedObjRef fn;fn.Set(R::FindDispatchFunctionCached(body.mesh.cls.Get(),L"K2_AttachToComponent"));if(!fn.Get())return false;
        ParamFrame frame(fn.Get());
        const auto functionCurrent=[&]{return current()&&fn.Get()&&R::FindDispatchFunctionCached(body.mesh.cls.Get(),L"K2_AttachToComponent")==fn.Get();};
        const R::FName socket{};const uint8_t keepRelative=0;const bool weld=true;
        if(!Slot(frame,L"Parent",sizeof(void*))||!Slot(frame,L"SocketName",sizeof(socket))||
            !Slot(frame,L"LocationRule",1)||!Slot(frame,L"RotationRule",1)||!Slot(frame,L"ScaleRule",1)||
            !Slot(frame,L"bWeldSimulatedBodies",1)||!Slot(frame,L"ReturnValue",1)||
            !frame.Set<void*>(L"Parent",arm.object.Get())||!frame.Set<R::FName>(L"SocketName",socket)||
            !frame.Set<uint8_t>(L"LocationRule",keepRelative)||!frame.Set<uint8_t>(L"RotationRule",keepRelative)||
            !frame.Set<uint8_t>(L"ScaleRule",keepRelative)||!frame.Set<bool>(L"bWeldSimulatedBodies",weld)||
            !functionCurrent()||!Call(body.mesh.object.Get(),frame)||!functionCurrent())return false;
        bool accepted=false;if(!frame.GetRaw(L"ReturnValue",&accepted,sizeof(accepted))||!accepted||!parent.Read(attached)||attached!=arm.object.Raw())return false;
    }
    return current()&&pawn.Write(body.actor.object.Raw())&&current();
}
bool ReadBodyYaw(void* actor,float& out){
    if(!IsFamily(actor))return false;
    const Mesh body(actor);FRotator rotation{};
    if(!body.Current()||!engine::TryGetComponentWorldRotation(body.mesh.object.Get(),rotation)||!Finite(rotation)||!body.Current())return false;
    out=rotation.Yaw;return true;
}
bool DriveBodyYaw(void* actor,float yaw){
    if(!std::isfinite(yaw)||!PreparePresentation(actor))return false;
    const Mesh body(actor);FRotator rotation{};
    if(!body.Current()||!engine::TryGetComponentWorldRotation(body.mesh.object.Get(),rotation)||!Finite(rotation)||!body.Current())return false;
    rotation.Yaw=yaw;
    return engine::SetComponentWorldRotation(body.mesh.object.Get(),rotation)&&body.Current();
}
}
