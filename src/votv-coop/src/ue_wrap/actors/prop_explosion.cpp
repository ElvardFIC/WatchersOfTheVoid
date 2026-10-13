#include "ue_wrap/actors/prop_explosion.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/engine/engine.h"
#include <cmath>
#include <cstring>
namespace ue_wrap::prop_explosion {
namespace {
namespace R=reflection;
bool InWorld(void* a) {
    return a&&R::IsLive(a)&&world_identity::CurrentWorld()&&
        world_identity::WorldOf(a)==world_identity::CurrentWorld()&&
        !R::NameStartsWith(R::NameOf(a),L"Default__");
}
template<class T> bool ReadField(void* a,const wchar_t* name,T& value) {
    const int off=R::FindPropertyOffset(R::ClassOf(a),name);if(off<0)return false;
    std::memcpy(&value,static_cast<uint8_t*>(a)+off,sizeof(value));return true;
}
template<class T> bool WriteField(void* a,const wchar_t* name,const T& value) {
    const int off=R::FindPropertyOffset(R::ClassOf(a),name);if(off<0)return false;
    std::memcpy(static_cast<uint8_t*>(a)+off,&value,sizeof(value));return true;
}
bool Bool(void* a,const wchar_t* name,bool& value,bool write=false) {
    int off=-1;uint8_t mask=0;
    if(!R::FindBoolProperty(R::ClassOf(a),name,off,mask)||off<0||!mask)return false;
    auto& b=static_cast<uint8_t*>(a)[off];
    if(write)b=value?uint8_t(b|mask):uint8_t(b&~mask);else value=(b&mask)!=0;
    return true;
}
bool Range(float x,float low,float high){return std::isfinite(x)&&x>=low&&x<=high;}
}
bool IsGascan(void* a){return InWorld(a)&&R::ClassNameOf(a)==L"prop_gascan_C";}
bool IsExplosion(void* a){return InWorld(a)&&R::ClassNameOf(a)==L"explosion_C";}
bool Valid(const State& s) {
    return Range(s.position.X,-1e6f,1e6f)&&Range(s.position.Y,-1e6f,1e6f)&&Range(s.position.Z,-1e6f,1e6f)&&
        Range(s.scale,.01f,10)&&Range(s.radius,.01f,10000)&&Range(s.damage,0,10000)&&
        Range(s.fireStrength,0,1)&&Range(s.fireFuel,0,60)&&
        Range(s.innerShake,0,1e6f)&&Range(s.outerShake,0,1e6f);
}
bool Read(void* a,State& out) {
    if(!IsExplosion(a))return false;
    CachedObjRef ref;ref.Set(a);const auto world=world_identity::Generation();State s;
    if(!engine::TryGetActorLocation(a,s.position)||!ref.Get()||world!=world_identity::Generation())return false;
    auto scale=engine::GetActorScale3D(a);
    if(!ref.Get()||world!=world_identity::Generation()||scale.X!=scale.Y||scale.X!=scale.Z)return false;
    s.scale=scale.X;
    if(!ReadField(a,L"radius",s.radius)||!ReadField(a,L"damage",s.damage)||
        !ReadField(a,L"fireStrength",s.fireStrength)||!ReadField(a,L"fireFuel",s.fireFuel)||
        !ReadField(a,L"innerShake",s.innerShake)||!ReadField(a,L"outerShake",s.outerShake)||
        !Bool(a,L"ignoreWalls",s.ignoreWalls)||!Bool(a,L"Skip Setting",s.skipSetting)||!Valid(s))return false;
    out=s;return true;
}
bool BreakGascan(void* a,bool fire) {
    if(!IsGascan(a))return false;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(a),fire?L"broken_fire":L"broken"));
    return f.valid()&&Call(a,f);
}
bool Replay(const State& s,const std::function<bool(void*)>& beforeFinish,const std::function<bool()>& current) {
    if(!Valid(s)||!current())return false;
    void* cls=object_index::ClassByName(L"explosion_C");
    void* shake=object_index::ClassByName(L"shake_explosion_C");
    void* gs=R::FindClassDefaultObject(L"GameplayStatics");
    if(!cls||!shake||!gs)return false;
    ParamFrame begin(R::FindDispatchFunctionCached(R::ClassOf(gs),L"BeginDeferredActorSpawnFromClass"));
    ParamFrame finish(R::FindDispatchFunctionCached(R::ClassOf(gs),L"FinishSpawningActor"));
    FTransform transform{};transform.RotW=1;transform.TX=s.position.X;transform.TY=s.position.Y;transform.TZ=s.position.Z;
    transform.SX=transform.SY=transform.SZ=s.scale;
    if(!begin.valid()||!finish.valid()||!begin.Set<void*>(L"WorldContextObject",engine::GetWorldContext())||
        !begin.Set<void*>(L"ActorClass",cls)||!begin.SetRaw(L"SpawnTransform",&transform,sizeof(transform))||
        !begin.Set<uint8_t>(L"CollisionHandlingOverride",1)||!begin.Set<void*>(L"Owner",nullptr)||!current())return false;
    if(!Call(gs,begin))return false;
    void* a=begin.Get<void*>(L"ReturnValue");if(!a)return false;
    CachedObjRef ref;ref.Set(a);const auto world=world_identity::Generation();
    const auto valid=[&]{return current()&&ref.Get()&&world==world_identity::Generation();};
    const auto cleanup=[&]{if(void* p=ref.Get())engine::DestroyActor(p);};
    if(!valid()||!IsExplosion(a)){cleanup();return false;}
    bool walls=s.ignoreWalls,skip=s.skipSetting;
    // Native damage/contact on this peer's player stays live. World damage and
    // chain reactions belong to host; no second random debris/physics simulation.
    if(!WriteField(a,L"radius",s.radius)||!WriteField(a,L"damage",s.damage)||
        !WriteField(a,L"fireStrength",s.fireStrength)||!WriteField(a,L"fireFuel",s.fireFuel)||
        !WriteField(a,L"innerShake",s.innerShake)||!WriteField(a,L"outerShake",s.outerShake)||
        !WriteField(a,L"force",0.f)||!WriteField(a,L"debris",int32_t(0))||
        !WriteField(a,L"shake",shake)||!Bool(a,L"ignoreWalls",walls,true)||!Bool(a,L"Skip Setting",skip,true)||
        !finish.Set<void*>(L"Actor",a)||!finish.SetRaw(L"SpawnTransform",&transform,sizeof(transform))||
        !valid()||!beforeFinish(a)||!valid()){cleanup();return false;}
    Call(gs,finish);
    return true;
}
}
