#include "ue_wrap/actors/watoy.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/component_calls.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_audio.h"
#include "ue_wrap/engine/engine_component.h"
#include <chrono>
#include <cmath>
#include <cstring>
namespace ue_wrap::watoy {
namespace {
namespace R=reflection;
template<class T> bool Field(void* actor,R::InstanceOffset& field,T& value) {
    CachedObjRef live;live.Set(actor);
    const auto world=world_identity::Generation();
    const int off=field.Of(actor);
    if(off<0 || !live.Get() || world!=world_identity::Generation())return false;
    std::memcpy(&value,static_cast<uint8_t*>(actor)+off,sizeof(T));return true;
}
void* Object(void* actor,R::InstanceOffset& field) {
    void* result=nullptr;
    return actor && Field(actor,field,result) && result && R::IsLive(result)?result:nullptr;
}
void* Audio(void* actor) {
    static R::InstanceOffset field{L"Audio"};return IsToy(actor)?Object(actor,field):nullptr;
}
bool Valid(const State& s) {
    return std::isfinite(s.volume) && s.volume>=0 && s.volume<=1 &&
        std::isfinite(s.pitch) && s.pitch>0 && s.pitch<=100 && (!s.effect || s.active);
}
void* Zero(void* actor) {
    static R::InstanceOffset field{L"a"};return Object(actor,field);
}
void* NativeEffect(void* actor) {
    static R::InstanceOffset field{L"eff"};return Object(Zero(actor),field);
}
void* EffectTemplate() {
    static CachedObjRef result;static uint64_t retry=0;
    if(result.Get())return result.Get();
    const auto now=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    if(now<retry)return nullptr;retry=now+1000;
    void* cls=object_index::ClassByName(L"ParticleSystem");if(!cls)return nullptr;
    object_index::ForEachInstance(cls,[](void* ctx,void* object,int32_t){
        if(!R::NameEquals(R::NameOf(object),L"eff_zgrav"))return;
        void* package=R::OuterOf(object);
        if(package && !R::OuterOf(package) && R::NameEquals(R::NameOf(package),L"/Game/particles/eff_zgrav"))
            static_cast<CachedObjRef*>(ctx)->Set(object);
    },&result);
    return result.Get();
}
void* SpawnEffect(void* actor,const std::function<bool()>& current) {
    CachedObjRef mesh;mesh.Set(prop::GetStaticMesh(actor));
    CachedObjRef tmpl;tmpl.Set(EffectTemplate());
    void* library=R::FindClassDefaultObject(L"GameplayStatics");
    void* system=R::FindClassDefaultObject(L"KismetSystemLibrary");
    void* math=R::FindClassDefaultObject(L"KismetMathLibrary");
    if(!mesh.Get() || !tmpl.Get() || !library || !system || !math || !current())return nullptr;
    ParamFrame transform(R::FindDispatchFunctionCached(R::ClassOf(mesh.Get()),L"K2_GetComponentToWorld"));
    if(!transform.valid() || !Call(mesh.Get(),transform) || !current() || !mesh.Get())return nullptr;
    const auto world=transform.Get<FTransform>(L"ReturnValue");
    ParamFrame bounds(R::FindDispatchFunctionCached(R::ClassOf(system),L"GetComponentBounds"));
    if(!bounds.valid() || !bounds.Set<void*>(L"Component",mesh.Get()) || !Call(system,bounds) || !current() || !mesh.Get())return nullptr;
    const auto origin=bounds.Get<FVector>(L"Origin");
    ParamFrame inverse(R::FindDispatchFunctionCached(R::ClassOf(math),L"InverseTransformLocation"));
    if(!inverse.valid() || !inverse.Set<FTransform>(L"T",world) || !inverse.Set<FVector>(L"Location",origin) ||
       !Call(math,inverse) || !current() || !mesh.Get())return nullptr;
    const auto location=inverse.Get<FVector>(L"ReturnValue");
    ParamFrame spawn(R::FindDispatchFunctionCached(R::ClassOf(library),L"SpawnEmitterAttached"));
    if(!spawn.valid() || !tmpl.Get() || !spawn.Set<void*>(L"EmitterTemplate",tmpl.Get()) ||
       !spawn.Set<void*>(L"AttachToComponent",mesh.Get()) || !spawn.Set<R::FName>(L"AttachPointName",{}) ||
       !spawn.Set<FVector>(L"Location",location) || !spawn.Set<FRotator>(L"Rotation",{}) ||
       !spawn.Set<FVector>(L"Scale",{1,1,1}) || !spawn.Set<uint8_t>(L"LocationType",0) ||
       !spawn.Set<bool>(L"bAutoDestroy",false) || !spawn.Set<uint8_t>(L"PoolingMethod",0) ||
       !spawn.Set<bool>(L"bAutoActivate",true) || !Call(library,spawn))return nullptr;
    return spawn.Get<void*>(L"ReturnValue");
}
}
bool IsToy(void* actor) {
    if(!actor || !R::IsLive(actor))return false;
    void* cls=object_index::ClassByName(L"prop_watoy_C");
    return cls && R::IsDescendantOfAny(R::ClassOf(actor),&cls,1);
}
size_t Collect(CachedObjRef* out,size_t cap) {
    void* cls=object_index::ClassByName(L"prop_watoy_C");
    struct Context {CachedObjRef* out;size_t cap,count=0;void* world;};
    Context c{out,cap,0,world_identity::CurrentWorld()};
    if(!out || !cap || !cls || !c.world)return 0;
    object_index::ForEachInstance(cls,[](void* ctx,void* actor,int32_t){
        auto& c=*static_cast<Context*>(ctx);
        if(c.count<c.cap && IsToy(actor) && world_identity::WorldOf(actor)==c.world)
            c.out[c.count++].Set(actor);
    },&c);
    return c.count;
}
bool Read(void* actor,State& out) {
    if(!IsToy(actor))return false;
    CachedObjRef live;live.Set(actor);
    const auto world=world_identity::Generation();
    void* audio=Audio(actor);if(!audio)return false;
    CachedObjRef sound;sound.Set(audio);
    static R::InstanceOffset volume{L"VolumeMultiplier"},pitch{L"PitchMultiplier"};
    State value{};void* zero=Zero(actor);value.active=zero!=nullptr;
    if(zero) {
        int off=-1;uint8_t mask=0;CachedObjRef component;component.Set(zero);
        if(!R::FindBoolProperty(R::ClassOf(zero),L"dir",off,mask) || off<0 || !mask || !component.Get())return false;
        value.effect=(static_cast<uint8_t*>(zero)[off]&mask)!=0;
    }
    if(!Field(audio,volume,value.volume) || !Field(audio,pitch,value.pitch) ||
       !Valid(value) || !sound.Get() || !live.Get() || world!=world_identity::Generation())return false;
    out=value;return true;
}
bool Activate(void* actor) {
    return IsToy(actor) && component_calls::CallParamlessNamed(actor,L"activate");
}
bool ApplyAudio(void* actor,const State& state) {
    if(!Valid(state))return false;
    CachedObjRef live;live.Set(actor);
    const auto world=world_identity::Generation();
    CachedObjRef audio;audio.Set(Audio(actor));if(!audio.Get())return false;
    auto valid=[&]{return live.Get() && audio.Get() && world==world_identity::Generation();};
    ParamFrame playing(R::FindDispatchFunctionCached(R::ClassOf(audio.Get()),L"IsPlaying"));
    if(!valid() || !playing.valid() || !Call(audio.Get(),playing) || !valid())return false;
    if(!component_calls::SetVolumeMultiplier(audio.Get(),state.volume) || !valid())return false;
    ParamFrame pitch(R::FindDispatchFunctionCached(R::ClassOf(audio.Get()),L"SetPitchMultiplier"));
    if(!valid() || !pitch.valid() || !pitch.Set<float>(L"NewPitchMultiplier",state.pitch) ||
       !Call(audio.Get(),pitch) || !valid())return false;
    return component_calls::SetAudioPlaying(audio.Get(),state.active,valid);
}
void Clear(View& view) {
    const auto old=view;view={};
    if(void* actor=old.actor.Get())ApplyAudio(actor,{});
    if(void* effect=old.effect.Get()) {
        component_calls::SetActive(effect,false,false);
        if(old.ownsEffect && old.effect.Get())engine::DestroyComponent(old.effect.Get(),old.actor.Get());
    }
}
bool Apply(View& view,void* actor,const State& state,const std::function<bool()>& current) {
    if(!Valid(state) || !IsToy(actor) || !current())return false;
    const auto world=world_identity::Generation();CachedObjRef live;live.Set(actor);
    const auto valid=[&]{return current() && live.Get() && world==world_identity::Generation();};
    if(!view.actor.Is(actor)){Clear(view);if(!valid())return false;view.actor=live;}
    if(!ApplyAudio(actor,state) || !valid())return false;
    if(!view.effect.Get() && state.effect) {
        view.effect.Set(NativeEffect(actor));view.ownsEffect=false;
        if(!view.effect.Get()){view.effect.Set(SpawnEffect(actor,valid));view.ownsEffect=true;}
        if(!valid()){Clear(view);return false;}
        if(!view.effect.Get())return false;
    }
    if(void* effect=view.effect.Get()) {
        if(!component_calls::SetActive(effect,state.effect,false) || !valid())return false;
        if(!state.effect && view.ownsEffect) {
            const auto old=view.effect;view.effect={};view.ownsEffect=false;
            if(old.Get())engine::DestroyComponent(old.Get(),actor);
            if(!valid())return false;
        }
    }
    return true;
}
bool PlayStartCue() {
    void* sound=engine::FindSound(L"SoundWave /Game/audio/sort/buttonlong10.buttonlong10");
    void* context=engine::GetWorldContext();if(!sound || !context)return false;
    engine::PlaySound2D(context,sound,1.f,.25f,0.f,true);return true;
}
void* ZeroGravityOwner(void* component) {
    if(!component || !R::IsLive(component))return nullptr;
    void* cls=object_index::ClassByName(L"comp_zerog_C");
    if(!cls || !R::IsDescendantOfAny(R::ClassOf(component),&cls,1))return nullptr;
    // AddComponent makes the actor the component's outer, already before the
    // Blueprint BeginPlay fills its owner variable. No dispatch inside force hooks.
    void* owner=R::OuterOf(component);
    return IsToy(owner)?owner:nullptr;
}
}
