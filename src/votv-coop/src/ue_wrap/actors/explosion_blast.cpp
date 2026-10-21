#include "ue_wrap/actors/prop_explosion.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/engine/engine.h"
#include <cmath>
#include <cstring>
namespace ue_wrap::prop_explosion {
namespace {
namespace R=reflection;
template<class T> bool Field(void* a,const wchar_t* name,T& value,bool write=false){
    int off=R::FindPropertyOffset(R::ClassOf(a),name);if(off<0)return false;
    if(write)std::memcpy(static_cast<uint8_t*>(a)+off,&value,sizeof(T));
    else std::memcpy(&value,static_cast<uint8_t*>(a)+off,sizeof(T));return true;
}
bool Bit(void* a,const wchar_t* name,bool& value,bool write=false){
    int off=-1;uint8_t mask=0;if(!R::FindBoolProperty(R::ClassOf(a),name,off,mask)||off<0||!mask)return false;
    auto& b=static_cast<uint8_t*>(a)[off];if(write)b=value?uint8_t(b|mask):uint8_t(b&~mask);else value=(b&mask)!=0;return true;
}
bool Range(float v,float lo,float hi){return std::isfinite(v)&&v>=lo&&v<=hi;}
bool Fields(void* a,Blast& b,bool write){
    return Field(a,L"radius",b.radius,write)&&Field(a,L"damage",b.damage,write)&&
        Field(a,L"fireStrength",b.fireStrength,write)&&Field(a,L"fireFuel",b.fireFuel,write)&&
        Field(a,L"innerShake",b.innerShake,write)&&Field(a,L"outerShake",b.outerShake,write)&&
        Field(a,L"force",b.force,write)&&Field(a,L"debrisForce",b.debrisForce,write)&&
        Field(a,L"impact",b.impact,write)&&Field(a,L"debris",b.debris,write)&&
        Bit(a,L"ignoreWalls",b.ignoreWalls,write)&&Bit(a,L"Skip Setting",b.skipSetting,write);
}
}
bool IsGameplayBlast(void* a){
    if(!a||!R::IsLive(a)||!world_identity::CurrentWorld()||
       world_identity::WorldOf(a)!=world_identity::CurrentWorld()||R::NameStartsWith(R::NameOf(a),L"Default__"))return false;
    const auto cls=R::ClassNameOf(a);return cls==L"explosion_C"||cls==L"explosion_saltrocket_C";
}
bool IsPrivatePcBlast(void* a){
    return a && R::IsLive(a) && world_identity::CurrentWorld() &&
        world_identity::WorldOf(a)==world_identity::CurrentWorld() &&
        !R::NameStartsWith(R::NameOf(a),L"Default__") && R::ClassNameOf(a)==L"explosion_pc_C";
}
bool Valid(const Blast& b){
    const auto& t=b.transform;
    const float norm=t.RotX*t.RotX+t.RotY*t.RotY+t.RotZ*t.RotZ+t.RotW*t.RotW;
    return b.variant<=1&&Range(norm,.99f,1.01f)&&
        Range(t.TX,-1e6f,1e6f)&&Range(t.TY,-1e6f,1e6f)&&Range(t.TZ,-1e6f,1e6f)&&
        Range(t.SX,.001f,1000)&&Range(t.SY,.001f,1000)&&Range(t.SZ,.001f,1000)&&
        Range(b.radius,.001f,1e6f)&&Range(b.damage,0,1e6f)&&Range(b.fireStrength,0,1000)&&Range(b.fireFuel,0,1000)&&
        Range(b.innerShake,0,1e6f)&&Range(b.outerShake,0,1e6f)&&Range(b.force,0,1e7f)&&Range(b.debrisForce,0,1e7f)&&
        Range(b.impact.X,-1e8f,1e8f)&&Range(b.impact.Y,-1e8f,1e8f)&&Range(b.impact.Z,-1e8f,1e8f)&&b.debris>=0&&b.debris<=128;
}
bool ReadBlast(void* a,Blast& out){
    if(!IsGameplayBlast(a))return false;CachedObjRef ref;ref.Set(a);auto world=world_identity::Generation();Blast b;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(a),L"GetTransform"));
    if(!f.valid()||!Call(a,f)||!ref.Get()||world!=world_identity::Generation()||
       !f.GetRaw(L"ReturnValue",&b.transform,sizeof(b.transform)))return false;
    b.variant=R::ClassNameOf(a)==L"explosion_saltrocket_C"?1:0;
    void* shake=nullptr;if(!Fields(a,b,false)||!Field(a,L"shake",shake))return false;
    // All gameplay spawn sites use this class or no shake; never load arbitrary wire classes.
    if(shake&&(!R::IsLive(shake)||!R::NameEquals(R::NameOf(shake),L"shake_explosion_C")))return false;
    b.shake=shake!=nullptr;if(!Valid(b))return false;out=b;return true;
}
bool ReplayBlast(const Blast& input,bool authority,const std::function<bool(void*)>& beforeFinish,const std::function<bool()>& current){
    if(!Valid(input)||!current())return false;auto b=input;
    void* cls=object_index::ClassByName(b.variant?L"explosion_saltrocket_C":L"explosion_C");
    void* shake=b.shake?object_index::ClassByName(L"shake_explosion_C"):nullptr;
    void* gs=R::FindClassDefaultObject(L"GameplayStatics");if(!cls||(b.shake&&!shake)||!gs)return false;
    ParamFrame begin(R::FindDispatchFunctionCached(R::ClassOf(gs),L"BeginDeferredActorSpawnFromClass"));
    ParamFrame finish(R::FindDispatchFunctionCached(R::ClassOf(gs),L"FinishSpawningActor"));
    if(!begin.valid()||!finish.valid()||!begin.Set<void*>(L"WorldContextObject",engine::GetWorldContext())||
       !begin.Set<void*>(L"ActorClass",cls)||!begin.SetRaw(L"SpawnTransform",&b.transform,sizeof(b.transform))||
       !begin.Set<uint8_t>(L"CollisionHandlingOverride",1)||!begin.Set<void*>(L"Owner",nullptr)||!current()||!Call(gs,begin))return false;
    void* a=begin.Get<void*>(L"ReturnValue");if(!a)return false;CachedObjRef ref;ref.Set(a);const auto world=world_identity::Generation();
    auto valid=[&]{return current()&&world==world_identity::Generation()&&ref.Get();};
    auto clean=[&]{if(void* live=ref.Get())engine::DestroyActor(live);};
    if(!valid()||!IsGameplayBlast(a)){clean();return false;}
    if(!authority){b.force=0;b.debris=0;}
    if(!Fields(a,b,true)||!Field(a,L"shake",shake,true)||!finish.Set<void*>(L"Actor",a)||
       !finish.SetRaw(L"SpawnTransform",&b.transform,sizeof(b.transform))||!valid()||!beforeFinish(a)||!valid()){clean();return false;}
    Call(gs,finish);return true; // Finish may already have emitted side effects, even on failure.
}
}
