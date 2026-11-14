// ue_wrap/actors/sit_pose.cpp -- see sit_pose.h.
#include "ue_wrap/actors/sit_pose.h"
#include "ue_wrap/actors/puppet.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/core/reflected_offset.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_component.h"
#include "ue_wrap/engine/world_identity.h"
#include <cmath>
#include <cstdint>

namespace ue_wrap::sit_pose {
namespace {
namespace R=reflection;namespace E=engine;namespace P=profile;
template<class T>T ReadAt(void* p,size_t offset){return *reinterpret_cast<T*>(static_cast<uint8_t*>(p)+offset);}
bool Finite(const FVector& p){return std::isfinite(p.X)&&std::isfinite(p.Y)&&std::isfinite(p.Z);}
bool Finite(const FRotator& r){return std::isfinite(r.Pitch)&&std::isfinite(r.Yaw)&&std::isfinite(r.Roll);}
void* LiveAnim(void* mesh){
    if(!mesh||!R::IsLive(mesh))return nullptr;
    void* anim=ReadAt<void*>(mesh,P::off::USkeletalMesh_AnimScriptInstance);
    return anim&&R::IsLive(anim)?anim:nullptr;
}
bool IsKerfurAnimBP(void* anim){
    if(!anim||!R::IsLive(anim))return false;
    // Indexed lookup instead of a raw process-lifetime class latch.
    void* base=object_index::ClassByName(P::name::AnimBPKerfurRegularClass);
    if(!base||!R::IsLive(base))return false;
    void* cls=R::ClassOf(anim);
    return cls&&(cls==base||R::IsDescendantOfAny(cls,&base,1));
}
struct Flags {
    CachedObjRef anim,cls,base;
    int32_t sit=-1,ik=-1;uint8_t sitMask=0,ikMask=0;
    bool Current()const{return anim.Get()&&cls.Get()&&base.Get()&&R::ClassOf(anim.Get())==cls.Get()&&
        base.Is(object_index::ClassByName(P::name::AnimBPKerfurRegularClass));}
};
Flags ResolveFlags(void* anim){
    Flags f;if(!IsKerfurAnimBP(anim))return f;
    f.anim.Set(anim);f.cls.Set(R::ClassOf(anim));f.base.Set(object_index::ClassByName(P::name::AnimBPKerfurRegularClass));
    R::FindBoolProperty(f.cls.Get(),L"isOnAtv",f.sit,f.sitMask);
    if(f.Current())R::FindBoolProperty(f.cls.Get(),L"useLegIK",f.ik,f.ikMask);
    return f;
}
void DriveAnim(void* anim,bool sitting){
    const Flags f=ResolveFlags(anim);if(!f.Current())return;
    const auto write=[&](int32_t offset,uint8_t mask,bool value){
        if(offset<0||!mask)return;
        auto& byte=*reinterpret_cast<uint8_t*>(static_cast<uint8_t*>(anim)+offset);
        byte=uint8_t((byte&~mask)|(value?mask:0));
    };
    write(f.sit,f.sitMask,sitting);write(f.ik,f.ikMask,!sitting);
}
bool AnimIsOnAtv(void* anim){
    const Flags f=ResolveFlags(anim);
    return f.Current()&&f.sit>=0&&f.sitMask&&
        (ReadAt<uint8_t>(anim,size_t(f.sit))&f.sitMask)!=0;
}
} // namespace
void DriveSitting(void* pawn,bool sitting){
    if(!pawn||!R::IsLive(pawn))return;CachedObjRef body;body.Set(pawn);
    if(void* vis=puppet::GetMeshPlayerVisibleComponent(pawn))DriveAnim(LiveAnim(vis),sitting);
    if(!body.Get())return;
    if(void* mesh=puppet::GetNativeBodyMeshComponent(pawn))DriveAnim(LiveAnim(mesh),sitting);
}
bool ReadIsOnAtv(void* pawn){
    if(!pawn||!R::IsLive(pawn))return false;CachedObjRef body;body.Set(pawn);
    if(void* vis=puppet::GetMeshPlayerVisibleComponent(pawn))if(AnimIsOnAtv(LiveAnim(vis)))return body.Get()!=nullptr;
    if(!body.Get())return false;
    if(void* mesh=puppet::GetNativeBodyMeshComponent(pawn))return AnimIsOnAtv(LiveAnim(mesh))&&body.Get();
    return false;
}
void* AtvDriverSeat(void* atv){
    if(!atv||!R::IsLive(atv))return nullptr;
    // Resolve on the live class rather than a once-captured layout.
    const int32_t offset=R::FindPropertyOffset(R::ClassOf(atv),L"sitarrow");
    if(offset<0)return nullptr;
    void* component=ReadAt<void*>(atv,size_t(offset));
    return component&&R::IsLive(component)?component:nullptr;
}
float ModelHalfLength(void* pawn){
    const float half=pawn&&R::IsLive(pawn)?E::GetActorCharacterHalfHeight(pawn):0.f;
    return std::isfinite(half)&&half>10.f&&half<=1000.f?half:88.f;
}
float ModelHead(void* pawn){return ModelHalfLength(pawn)*.25f;}

namespace {
struct MeshBase {
    CachedObjRef comp,cls,parent;
    FVector base{};uint32_t generation=0;bool held=false;
};
struct PawnShift {CachedObjRef pawn;MeshBase a,b;uint64_t revision=0;};
PawnShift g_shift[32];
uint64_t g_shiftRevision=0;
void* RawParent(void* comp){return ReadAt<void*>(comp,P::off::USceneComponent_AttachParent);}
bool BindingCurrent(const MeshBase& s){
    return s.held&&s.comp.Get()&&s.cls.Get()&&R::ClassOf(s.comp.Get())==s.cls.Get()&&
        s.generation==world_identity::Generation()&&RawParent(s.comp.Get())==s.parent.Raw()&&
        (!s.parent.Raw()||s.parent.Get());
}
bool SetRelative(void* comp,const FVector& loc){
    if(!comp||!R::IsLive(comp)||!Finite(loc))return false;
    CachedObjRef component,cls,function;component.Set(comp);cls.Set(R::ClassOf(comp));
    function.Set(R::FindDispatchFunctionCached(cls.Get(),L"K2_SetRelativeLocation"));
    const auto generation=world_identity::Generation();void* world=world_identity::CurrentWorld();
    const auto current=[&]{return component.Get()&&cls.Get()&&function.Get()&&world&&
        generation==world_identity::Generation()&&world==world_identity::CurrentWorld()&&
        R::ClassOf(comp)==cls.Get()&&R::FindDispatchFunctionCached(cls.Get(),L"K2_SetRelativeLocation")==function.Get();};
    if(!current())return false;
    ParamFrame f(function.Get());
    if(!f.valid()||!f.Set<FVector>(L"NewLocation",loc)||!f.Set<bool>(L"bSweep",false)||!f.Set<bool>(L"bTeleport",true)||!current())return false;
    return Call(comp,f)&&current();
}
bool ToParent(const MeshBase& slot,const FVector& world,FVector& out){
    if(!BindingCurrent(slot)||!Finite(world))return false;
    if(!slot.parent.Raw()){out=world;return true;}
    FRotator rot{};
    if(!E::TryGetComponentWorldRotation(slot.parent.Get(),rot)||!BindingCurrent(slot)||!Finite(rot))return false;
    constexpr float rad=.01745329252f;
    const float sp=std::sin(rot.Pitch*rad),cp=std::cos(rot.Pitch*rad),sy=std::sin(rot.Yaw*rad),cy=std::cos(rot.Yaw*rad),
        sr=std::sin(rot.Roll*rad),cr=std::cos(rot.Roll*rad);
    // Inverse of UE's complete rotation basis; a banked vehicle is not yaw-only.
    out={cp*cy*world.X+cp*sy*world.Y+sp*world.Z,
        (sr*sp*cy-cr*sy)*world.X+(sr*sp*sy+cr*cy)*world.Y-sr*cp*world.Z,
        -(cr*sp*cy+sr*sy)*world.X+(cy*sr-cr*sp*sy)*world.Y+cr*cp*world.Z};
    return Finite(out)&&BindingCurrent(slot);
}
bool Restore(MeshBase& slot){
    const MeshBase old=slot;slot={};
    if(!BindingCurrent(old))return true; // retired/rebound component has no restore licence
    if(SetRelative(old.comp.Get(),old.base))return true;
    if(BindingCurrent(old))slot=old; // retain refusal for the next clear/shift
    return !slot.held;
}
bool Hold(MeshBase& slot,void* comp){
    if(slot.comp.Is(comp)&&BindingCurrent(slot))return true;
    if(!Restore(slot))return false;
    if(!comp)return true;
    if(!R::IsLive(comp))return false;
    MeshBase next;next.comp.Set(comp);next.cls.Set(R::ClassOf(comp));next.parent.Set(RawParent(comp));
    next.generation=world_identity::Generation();next.held=true;
    if(!BindingCurrent(next))return false;
    const FVector base=E::GetComponentRelativeLocation(comp);
    if(!Finite(base)||!BindingCurrent(next))return false;
    next.base=base;slot=next;return true;
}
bool Apply(const MeshBase& slot,const FVector& world){
    if(!slot.held)return true;
    FVector delta{};if(!ToParent(slot,world,delta)||!BindingCurrent(slot))return false;
    return SetRelative(slot.comp.Get(),{slot.base.X+delta.X,slot.base.Y+delta.Y,slot.base.Z+delta.Z})&&BindingCurrent(slot);
}
PawnShift* FindShift(void* pawn,bool create){
    PawnShift* free=nullptr;
    for(auto& s:g_shift){
        if(s.pawn.Raw()&&!s.pawn.Get()){s={};++g_shiftRevision;}
        if(s.pawn.Is(pawn))return &s;
        if(!s.pawn.Raw()&&!free)free=&s;
    }
    if(!create||!free)return nullptr;
    free->pawn.Set(pawn);
    if(!free->pawn.Get()){*free={};return nullptr;}
    free->revision=++g_shiftRevision;return free;
}
} // namespace
void ShiftMesh(void* pawn,float upCm,float forwardCm){
    if(!pawn||!R::IsLive(pawn)||!std::isfinite(upCm)||!std::isfinite(forwardCm))return;
    if(upCm==0&&forwardCm==0){ClearMeshShift(pawn);return;}
    CachedObjRef player;player.Set(pawn);
    void* body=puppet::GetNativeBodyMeshComponent(pawn);void* vis=puppet::GetMeshPlayerVisibleComponent(pawn);
    if(!player.Get())return;
    const bool visOnBody=vis&&body&&R::IsLive(vis)&&RawParent(vis)==body;
    const FVector fwd=E::GetActorForwardVector(pawn);
    if(!player.Get()||!Finite(fwd))return;
    const FVector world{fwd.X*forwardCm,fwd.Y*forwardCm,upCm+fwd.Z*forwardCm};
    if(!Finite(world))return;
    PawnShift* s=FindShift(pawn,true);if(!s)return;
    uint64_t revision=s->revision,epoch=g_shiftRevision;PawnShift next=*s;
    const auto current=[&]{return player.Get()&&s->pawn.Is(pawn)&&s->revision==revision&&g_shiftRevision==epoch&&
        puppet::GetNativeBodyMeshComponent(pawn)==body&&puppet::GetMeshPlayerVisibleComponent(pawn)==vis;};
    if(!Hold(next.a,body?body:vis)||!current()||!Hold(next.b,(visOnBody||next.a.comp.Is(vis))?nullptr:vis)||!current())return;
    // Keep the authored bases before the first write, including a refused second write.
    next.revision=++g_shiftRevision;*s=next;revision=s->revision;epoch=g_shiftRevision;
    if(!Apply(next.a,world)||!current()||!Apply(next.b,world)||!current())return;
    next.revision=++g_shiftRevision;*s=next;
}
void ClearMeshShift(void* pawn){
    if(!pawn)return;
    PawnShift* s=FindShift(pawn,false);if(!s)return;
    PawnShift saved=*s;*s={};const uint64_t epoch=++g_shiftRevision;
    Restore(saved.a);if(g_shiftRevision!=epoch)return;
    Restore(saved.b);if(g_shiftRevision!=epoch)return;
    if(saved.pawn.Get()&&(saved.a.held||saved.b.held)){
        if(auto* target=FindShift(pawn,true)){saved.revision=++g_shiftRevision;*target=saved;}
    }
}
} // namespace ue_wrap::sit_pose
