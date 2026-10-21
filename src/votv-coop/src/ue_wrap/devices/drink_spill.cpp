#include "ue_wrap/devices/drink_spill.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/script_calls.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/devices/grime.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_physics.h"
#include <cmath>
#include <chrono>
#include <array>
#include <cstring>
#include <algorithm>
namespace ue_wrap::drink_spill {
namespace {
namespace R=reflection;
constexpr const wchar_t* kClasses[]={L"grime_beer_C",L"grime_wine_C",L"grime_coffee_C",L"grime_gasoline_C",L"grime_explosionScorch_C"};
template<class T>T Field(void* object,int off){T value{};std::memcpy(&value,static_cast<uint8_t*>(object)+off,sizeof(value));return value;}
struct Life{CachedObjRef actor;uint32_t world=world_identity::Generation();explicit Life(void* a){actor.Set(a);}
    bool Current()const{return actor.Get() && world==world_identity::Generation();}};
bool KindClass(void* cls,uint8_t& kind){if(!cls)return false;for(uint8_t i=0;i<std::size(kClasses);++i)if(R::NameEquals(R::NameOf(cls),kClasses[i])){kind=i;return true;}return false;}
void* SoundAsset(uint8_t index){
    struct Find{const wchar_t* name;const wchar_t* package;CachedObjRef ref;};
    struct Asset{const wchar_t* name;const wchar_t* package;const wchar_t* cls;};
    static constexpr Asset assets[]={
        {L"water_splash_Cue",L"/Game/audio/effects/physics/water/water_splash_Cue",L"SoundCue"},
        {L"att_small",L"/Game/audio/misc/att_small",L"SoundAttenuation"},
        {L"fuel1",L"/Game/audio/effects/fuel1",L"SoundWave"},
        {L"att_default",L"/Game/audio/misc/att_default",L"SoundAttenuation"}};
    static std::array<CachedObjRef,4> cache;static std::array<uint64_t,4> retry{};
    if(index>=std::size(assets))return nullptr;if(cache[index].Get())return cache[index].Get();
    const auto now=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    if(now<retry[index])return nullptr;retry[index]=now+1000;
    const auto& asset=assets[index];Find find{asset.name,asset.package};
    void* cls=object_index::ClassByName(asset.cls);if(!cls)return nullptr;
    object_index::ForEachInstance(cls,[](void* ctx,void* object,int32_t){auto& find=*static_cast<Find*>(ctx);
        if(!R::NameEquals(R::NameOf(object),find.name))return;void* outer=R::OuterOf(object);
        if(outer && !R::OuterOf(outer) && R::NameEquals(R::NameOf(outer),find.package))find.ref.Set(object);
    },&find);cache[index]=find.ref;return cache[index].Get();
}
struct PourLayout{CachedObjRef graph,sound;uint64_t unlist=0,retry=0;int point=-1,end=-1;bool ready=false;};
PourLayout PourFields(uint8_t kind){
    static std::array<PourLayout,2> cache;auto& row=cache[kind==2?1:0];
    const bool coffee=kind==2;void* cls=object_index::ClassByName(coffee?L"prop_food_coffeeCup_C":L"prop_snack_beer_C");
    void* library=object_index::ClassByName(L"GameplayStatics");
    void* graph=cls?R::FindDispatchFunctionCached(cls,coffee?L"ExecuteUbergraph_prop_food_coffeeCup":L"ExecuteUbergraph_prop_snack_beer"):nullptr;
    void* sound=library?R::FindDispatchFunctionCached(library,L"PlaySoundAtLocation"):nullptr;
    const auto unlist=object_index::ClassUnlistVersion();
    if(graph && sound && row.graph.Get()==graph && row.sound.Get()==sound && row.unlist==unlist){
        if(row.ready)return row;const auto now=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        if(now<row.retry)return row;
    }
    row={};row.graph.Set(graph);row.sound.Set(sound);row.unlist=unlist;
    row.retry=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count())+1000;
    if(!graph || !sound)return row;
    const wchar_t* local=coffee?L"CallFunc_K2_GetActorLocation_ReturnValue":L"CallFunc_GetComponentBounds_Origin";
    row.point=R::FindPropertyOffset(graph,local);const int size=R::FunctionFrameSize(graph);
    if(row.point<0 || row.point>size-int(sizeof(FVector)))return row;
    std::vector<script_calls::CallSite> calls;if(!script_calls::EnumerateCalls(graph,calls))return row;
    for(const auto& call:calls)if(script_calls::SameCallee(call,sound) && script_calls::CallHasLocal(call,local)){
        if(row.end>=0){row.end=-1;return row;}row.end=call.end;
    }
    row.ready=row.end>=0 && row.graph.Get() && row.sound.Get() && object_index::ClassUnlistVersion()==unlist;return row;
}
}
bool Valid(const State& s){
    if(s.kind>=std::size(kClasses) || !std::isfinite(s.process) || s.process<0 || s.process>1.e6f || s.type<0 || s.type>4096)return false;
    for(float v:{s.location.X,s.location.Y,s.location.Z})if(!std::isfinite(v) || std::fabs(v)>1.e7f)return false;
    for(float v:{s.scale.X,s.scale.Y,s.scale.Z})if(!std::isfinite(v) || v<(s.kind==kExplosionScorch?.001f:.01f) || v>(s.kind==kExplosionScorch?1000.f:10.f))return false;
    for(float v:{s.rotation.Pitch,s.rotation.Yaw,s.rotation.Roll,s.decalRotation.Pitch,s.decalRotation.Yaw,s.decalRotation.Roll})
        if(!std::isfinite(v) || std::fabs(v)>36000)return false;
    return true;
}
bool IsProjectile(void* actor){return actor && R::IsLive(actor) && R::ClassOf(actor)==object_index::ClassByName(L"grimeProjectile_C");}
bool SourceKind(void* actor,uint8_t& kind){
    if(!actor || !R::IsLive(actor))return false;Life life(actor);void* cls=R::ClassOf(actor);
    void* coffee=object_index::ClassByName(L"prop_food_coffeeCup_C");if(coffee && cls==coffee){kind=2;return life.Current();}
    void* base=object_index::ClassByName(L"prop_snack_beer_C");if(!base || !cls || (cls!=base && !R::IsDescendantOfAny(cls,&base,1)))return false;
    const int off=R::FindPropertyOffset(cls,L"Grunge");if(off<0 || !life.Current())return false;
    return KindClass(Field<void*>(actor,off),kind) && kind<2 && life.Current();
}
bool SourceClassKind(const std::wstring& className,uint8_t& kind){
    void* cls=object_index::ClassByName(className.c_str());if(!cls)return false;CachedObjRef ref;ref.Set(cls);
    void* cdo=R::FindClassDefaultObject(className.c_str());return cdo && ref.Get() && R::ClassOf(cdo)==cls && BreakSourceKind(cdo,kind) && ref.Get();
}
bool BreakSourceKind(void* actor,uint8_t& kind){
    if(SourceKind(actor,kind))return true;
    if(!actor || !R::IsLive(actor))return false;Life life(actor);
    void* cls=object_index::ClassByName(L"prop_gasBottle_C");
    if(!cls || R::ClassOf(actor)!=cls || !life.Current())return false;kind=3;return true;
}
bool ProjectileKind(void* actor,uint8_t& kind){
    if(!IsProjectile(actor))return false;Life life(actor);const int off=R::FindPropertyOffset(R::ClassOf(actor),L"grunge");
    return off>=0 && life.Current() && KindClass(Field<void*>(actor,off),kind) && kind<kExplosionScorch && life.Current();
}
bool PourReady(void* source){uint8_t kind=0;return SourceKind(source,kind) && PourFields(kind).ready;}
bool ReadPourCue(void* source,const ufunction_hook::CallerFrame& frame,uint8_t& kind,FVector& point,const std::function<bool()>& current){
    if(!current || !current() || !SourceKind(source,kind))return false;Life life(source);void* cls=R::ClassOf(source);CachedObjRef sourceClass;sourceClass.Set(cls);const auto fields=PourFields(kind);
    const auto valid=[&]{return life.Current() && sourceClass.Get() && R::ClassOf(source)==cls && fields.graph.Get()==frame.function && fields.sound.Get() &&
        fields.unlist==object_index::ClassUnlistVersion() && current();};
    if(!fields.ready || !frame.locals || frame.codeOffset!=fields.end || !valid())return false;
    FVector value{};std::memcpy(&value,frame.locals+fields.point,sizeof(value));
    for(float v:{value.X,value.Y,value.Z})if(!std::isfinite(v) || std::fabs(v)>1.e7f)return false;
    if(!valid())return false;point=value;return true;
}
bool Valid(const Launch& launch){
    if(launch.kind>=kExplosionScorch)return false;
    State state{launch.location,launch.scale,launch.rotation,{},100,0,launch.kind};if(!Valid(state) || !std::isfinite(launch.size) || launch.size<=0 || launch.size>10)return false;
    for(float v:{launch.velocity.X,launch.velocity.Y,launch.velocity.Z})if(!std::isfinite(v))return false;
    return launch.velocity.X*launch.velocity.X+launch.velocity.Y*launch.velocity.Y+launch.velocity.Z*launch.velocity.Z<=4.e8f;
}
bool ReadLaunch(void* actor,Launch& launch,const std::function<bool()>& current){
    if(!actor || !R::IsLive(actor) || !current || !current())return false;Life life(actor);void* cls=R::ClassOf(actor);CachedObjRef classRef;classRef.Set(cls);
    const auto valid=[&]{return life.Current() && classRef.Get() && R::ClassOf(actor)==cls && current();};Launch value;
    if(!ProjectileKind(actor,value.kind) || !valid() || !engine::TryGetActorLocation(actor,value.location) || !valid() || !engine::TryGetActorRotation(actor,value.rotation) || !valid())return false;
    const int velocity=R::FindPropertyOffset(cls,L"velocity"),size=R::FindPropertyOffset(cls,L"size");if(velocity<0 || size<0 || !valid())return false;
    value.velocity=Field<FVector>(actor,velocity);value.size=Field<float>(actor,size);
    CachedObjRef function;function.Set(R::FindDispatchFunctionCached(cls,L"GetActorScale3D"));ParamFrame scale(function.Get());
    if(!scale.valid() || !valid() || !Call(actor,scale) || !valid() || !function.Get() || !scale.GetRaw(L"ReturnValue",&value.scale,sizeof(value.scale)) || !Valid(value))return false;
    launch=value;return true;
}
Spawn AuthorLaunch(const Launch& launch,const std::function<bool()>& current){
    if(!Valid(launch) || !current || !current())return {};void* cls=object_index::ClassByName(L"grimeProjectile_C"),*grunge=object_index::ClassByName(kClasses[launch.kind]);
    if(!cls || !grunge)return {};CachedObjRef classRef,grungeRef;classRef.Set(cls);grungeRef.Set(grunge);
    const int velocity=R::FindPropertyOffset(cls,L"velocity"),size=R::FindPropertyOffset(cls,L"size"),kind=R::FindPropertyOffset(cls,L"grunge");
    if(velocity<0 || size<0 || kind<0 || !classRef.Get() || !grungeRef.Get() || !current())return {};
    Life life(engine::BeginDeferredSpawn(cls,launch.location,launch.rotation));
    const auto valid=[&]{return life.Current() && classRef.Get() && grungeRef.Get() && R::ClassOf(life.actor.Get())==cls && current();};
    const auto failed=[&]()->Spawn{if(life.world==world_identity::Generation())if(void* actor=life.actor.Get())engine::DestroyActor(actor);return {nullptr,true};};
    if(!valid())return failed();void* actor=life.actor.Get();
    std::memcpy(static_cast<uint8_t*>(actor)+velocity,&launch.velocity,sizeof(launch.velocity));
    std::memcpy(static_cast<uint8_t*>(actor)+size,&launch.size,sizeof(launch.size));std::memcpy(static_cast<uint8_t*>(actor)+kind,&grunge,sizeof(grunge));
    if(!engine::SetActorScale3D(actor,launch.scale) || !valid() || !engine::FinishDeferredSpawn(actor,launch.location,launch.rotation) || !valid() ||
       !engine::SetActorScale3D(actor,launch.scale) || !valid())return failed();
    return {actor,true};
}
bool FreezeLaunch(void* actor,const std::function<bool()>& current){
    uint8_t kind=0;if(!current || !current() || !ProjectileKind(actor,kind))return false;Life life(actor);
    void* cls=R::ClassOf(actor);CachedObjRef classRef;classRef.Set(cls);
    const int off=R::FindPropertyOffset(cls,L"Sphere");if(off<0 || !life.Current() || !classRef.Get() || !current())return false;
    void* sphere=Field<void*>(actor,off);CachedObjRef component;component.Set(sphere);
    const auto valid=[&]{return life.Current() && classRef.Get() && R::ClassOf(actor)==cls && component.Get() && Field<void*>(actor,off)==sphere && current();};
    return valid() && engine::SetActorEnableCollision(actor,false) && valid() && engine::SetComponentSimulatePhysics(sphere,false) && valid();
}
bool DecalKind(void* actor,uint8_t& kind){
    if(!actor || !R::IsLive(actor) || !KindClass(R::ClassOf(actor),kind))return false;
    return kind!=kExplosionScorch || (world_identity::CurrentWorld() && world_identity::WorldOf(actor)==world_identity::CurrentWorld() &&
        !R::NameStartsWith(R::NameOf(actor),L"Default__"));
}
bool Read(void* actor,State& state,const std::function<bool()>& current){
    if(!actor || !current || !current())return false;Life life(actor);State value;
    void* cls=R::ClassOf(actor);CachedObjRef classRef;classRef.Set(cls);
    const auto valid=[&]{return life.Current() && classRef.Get() && R::ClassOf(actor)==cls && current();};
    if(!DecalKind(actor,value.kind) || !valid() || !engine::TryGetActorLocation(actor,value.location) || !valid() ||
       !engine::TryGetActorRotation(actor,value.rotation) || !valid())return false;
    CachedObjRef scaleFunction;scaleFunction.Set(R::FindDispatchFunctionCached(cls,L"GetActorScale3D"));ParamFrame scale(scaleFunction.Get());
    if(!scale.valid() || !valid() || !Call(actor,scale) || !valid() || !scaleFunction.Get() || !scale.GetRaw(L"ReturnValue",&value.scale,sizeof(value.scale)))return false;
    const int off=R::FindPropertyOffset(R::ClassOf(actor),L"Decal");if(off<0 || !valid())return false;
    void* decal=Field<void*>(actor,off);if(!decal || !R::IsLive(decal))return false;CachedObjRef component;component.Set(decal);
    CachedObjRef rotationFunction;rotationFunction.Set(R::FindDispatchFunctionCached(R::ClassOf(decal),L"K2_GetComponentRotation"));ParamFrame rotation(rotationFunction.Get());
    if(!rotation.valid() || !valid() || !Call(decal,rotation) || !valid() ||
       !rotationFunction.Get() || !rotation.GetRaw(L"ReturnValue",&value.decalRotation,sizeof(value.decalRotation)))return false;
    if(!valid() || !component.Get() || Field<void*>(actor,off)!=decal || !grime::ReadProcess(actor,value.process) || !valid() ||
       !grime::ReadType(actor,value.type) || !valid() || !Valid(value))return false;
    state=value;return true;
}
bool ApplyExisting(void* actor,const State& state,const std::function<bool()>& current){
    uint8_t kind=0;if(!Valid(state) || !current || !current() || !DecalKind(actor,kind) || kind!=state.kind)return false;
    Life life(actor);void* cls=R::ClassOf(actor);CachedObjRef classRef;classRef.Set(cls);
    const int process=R::FindPropertyOffset(cls,L"process"),type=R::FindPropertyOffset(cls,L"Type"),decalOff=R::FindPropertyOffset(cls,L"Decal");
    const auto valid=[&]{return life.Current() && classRef.Get() && R::ClassOf(actor)==cls && current();};
    if(process<0 || type<0 || decalOff<0 || !valid())return false;
    void* decal=Field<void*>(actor,decalOff);if(!decal || !R::IsLive(decal))return false;CachedObjRef component,apply;component.Set(decal);
    apply.Set(R::FindDispatchFunctionCached(cls,L"applyMaterial"));if(!apply.Get() || !valid())return false;ParamFrame frame(apply.Get());
    if(!frame.valid() || !valid())return false;
    const float target=state.process;
    std::memcpy(static_cast<uint8_t*>(actor)+process,&target,sizeof(float));std::memcpy(static_cast<uint8_t*>(actor)+type,&state.type,sizeof(int32_t));
    if(!engine::SetActorLocation(actor,state.location) || !valid() || !engine::SetActorRotation(actor,state.rotation) || !valid() ||
       !engine::SetActorScale3D(actor,state.scale) || !valid() || !Call(actor,frame) || !valid() || !apply.Get() || !component.Get() ||
       Field<void*>(actor,decalOff)!=decal || !engine::SetComponentWorldRotation(decal,state.decalRotation) || !valid())return false;
    return grime::WriteProcessAndApply(actor,target,valid) && valid();
}
Spawn Author(const State& state,bool canonical,const std::function<bool()>& current){
    if(!Valid(state) || !current || !current())return {};
    void* cls=object_index::ClassByName(kClasses[state.kind]);if(!cls)return {};CachedObjRef classRef;classRef.Set(cls);
    const int process=R::FindPropertyOffset(cls,L"process"),type=R::FindPropertyOffset(cls,L"Type"),decalOff=R::FindPropertyOffset(cls,L"Decal");
    if(process<0 || type<0 || decalOff<0 || !classRef.Get() || !current())return {};
    Life life(engine::BeginDeferredSpawn(cls,state.location,state.rotation));
    const auto valid=[&]{return life.Current() && classRef.Get() && R::ClassOf(life.actor.Get())==cls && current();};
    const auto failed=[&]()->Spawn{if(life.world==world_identity::Generation())if(void* actor=life.actor.Get())engine::DestroyActor(actor);return {nullptr,true};};
    if(!valid())return failed();void* actor=life.actor.Get();
    if(canonical){std::memcpy(static_cast<uint8_t*>(actor)+process,&state.process,sizeof(float));std::memcpy(static_cast<uint8_t*>(actor)+type,&state.type,sizeof(int32_t));}
    if(!engine::SetActorScale3D(actor,state.scale) || !valid() || !engine::FinishDeferredSpawn(actor,state.location,state.rotation) || !valid())return failed();
    if(!engine::SetActorScale3D(actor,state.scale) || !valid())return failed();
    void* decal=Field<void*>(actor,decalOff);if(!decal || !R::IsLive(decal))return failed();CachedObjRef component;component.Set(decal);
    CachedObjRef apply;apply.Set(R::FindDispatchFunctionCached(cls,L"applyMaterial"));
    if(!apply.Get() || !valid())return failed();ParamFrame frame(apply.Get());
    // Finish reapplies unit spawn scale. Rebuild the native material after restoring
    // the final scale, then restore the host's orientation after native randomisation.
    if(!frame.valid() || !Call(actor,frame) || !valid() || !apply.Get() || !component.Get() ||
       Field<void*>(actor,decalOff)!=decal || !engine::SetComponentWorldRotation(decal,state.decalRotation) || !valid())return failed();
    const float value=canonical?state.process:Field<float>(actor,process);
    if(!grime::WriteProcessAndApply(actor,value,valid) || !valid())return failed();
    return {actor,true};
}
namespace {
bool PlayCue(uint8_t soundIndex,uint8_t attenuationIndex,float pitch,const FVector& point,const std::function<bool()>& current){
    if(!current || !current())return false;
    void* context=engine::GetWorldContext();Life life(context);
    void* sound=SoundAsset(soundIndex);void* attenuation=SoundAsset(attenuationIndex);
    if(!context || !sound || !attenuation || !R::IsLive(sound))return false;
    CachedObjRef soundRef,attenuationRef;soundRef.Set(sound);attenuationRef.Set(attenuation);
    void* cdo=R::FindClassDefaultObject(L"GameplayStatics");if(!cdo)return false;CachedObjRef library,function;library.Set(cdo);
    function.Set(R::FindDispatchFunctionCached(R::ClassOf(cdo),L"PlaySoundAtLocation"));ParamFrame frame(function.Get());
    if(!frame.valid() || !frame.Set(L"WorldContextObject",context) || !frame.Set(L"Sound",sound) || !frame.Set(L"Location",point) ||
       !frame.Set(L"Rotation",FRotator{}) || !frame.Set<float>(L"VolumeMultiplier",.5f) || !frame.Set<float>(L"PitchMultiplier",pitch) ||
       !frame.Set(L"AttenuationSettings",attenuation) || !current() || !life.Current() || !soundRef.Get() || !attenuationRef.Get() || !library.Get() || !function.Get())return false;
    Call(cdo,frame);return true;
}
}
bool ImpactSound(const FVector& point,const std::function<bool()>& current){return PlayCue(0,1,1.25f,point,current);}
bool PourSound(uint8_t kind,const FVector& point,const std::function<bool()>& current){return kind<=2 && PlayCue(kind==2?0:2,kind==2?1:3,kind==2?1.25f:1.f,point,current);}
}
