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
struct Owned {
    CachedObjRef object,cls;uint32_t world=world_identity::Generation();
    explicit Owned(void* value){object.Set(value);if(object.Get())cls.Set(R::ClassOf(value));}
    bool Current()const{return object.Get() && cls.Get() && R::ClassOf(object.Get())==cls.Get() && world==world_identity::Generation();}
};
uint64_t g_revision=0;
uint64_t Revision(){if(!++g_revision)++g_revision;return g_revision;}
template<class T> bool Field(void* actor,R::InstanceOffset& field,T& value) {
    Owned live(actor);
    const int off=field.Of(actor);
    if(off<0 || !live.Current())return false;
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
bool ReadZero(void* actor,void*& value){
    static R::InstanceOffset field{L"a"};void* raw=nullptr;
    if(!Field(actor,field,raw))return false;
    value=raw && R::IsLive(raw)?raw:nullptr;return true;
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
    Owned source(actor),mesh(prop::GetStaticMesh(actor)),tmpl(EffectTemplate());
    Owned libraryRef(R::FindClassDefaultObject(L"GameplayStatics")),systemRef(R::FindClassDefaultObject(L"KismetSystemLibrary")),mathRef(R::FindClassDefaultObject(L"KismetMathLibrary"));
    void* library=libraryRef.object.Get();void* system=systemRef.object.Get();void* math=mathRef.object.Get();
    const auto valid=[&]{return current() && source.Current() && mesh.Current() && tmpl.Current() &&
        libraryRef.Current() && systemRef.Current() && mathRef.Current() && prop::GetStaticMesh(actor)==mesh.object.Get();};
    if(!valid())return nullptr;
    ParamFrame transform(R::FindDispatchFunctionCached(R::ClassOf(mesh.object.Get()),L"K2_GetComponentToWorld"));
    if(!transform.valid() || !valid() || !Call(mesh.object.Get(),transform) || !valid())return nullptr;
    FTransform world{};
    if(!transform.GetRaw(L"ReturnValue",&world,sizeof(world)) ||
       !std::isfinite(world.RotX) || !std::isfinite(world.RotY) || !std::isfinite(world.RotZ) || !std::isfinite(world.RotW) ||
       !std::isfinite(world.TX) || !std::isfinite(world.TY) || !std::isfinite(world.TZ) ||
       !std::isfinite(world.SX) || !std::isfinite(world.SY) || !std::isfinite(world.SZ) || !valid())return nullptr;
    ParamFrame bounds(R::FindDispatchFunctionCached(R::ClassOf(system),L"GetComponentBounds"));
    if(!bounds.valid() || !bounds.Set<void*>(L"Component",mesh.object.Get()) || !valid() || !Call(system,bounds) || !valid())return nullptr;
    FVector origin{};
    if(!bounds.GetRaw(L"Origin",&origin,sizeof(origin)) || !std::isfinite(origin.X) || !std::isfinite(origin.Y) || !std::isfinite(origin.Z) || !valid())return nullptr;
    ParamFrame inverse(R::FindDispatchFunctionCached(R::ClassOf(math),L"InverseTransformLocation"));
    if(!inverse.valid() || !inverse.Set<FTransform>(L"T",world) || !inverse.Set<FVector>(L"Location",origin) ||
       !valid() || !Call(math,inverse) || !valid())return nullptr;
    FVector location{};
    if(!inverse.GetRaw(L"ReturnValue",&location,sizeof(location)) || !std::isfinite(location.X) || !std::isfinite(location.Y) || !std::isfinite(location.Z) || !valid())return nullptr;
    ParamFrame spawn(R::FindDispatchFunctionCached(R::ClassOf(library),L"SpawnEmitterAttached"));
    if(!spawn.valid() || !valid() || !spawn.Set<void*>(L"EmitterTemplate",tmpl.object.Get()) ||
       !spawn.Set<void*>(L"AttachToComponent",mesh.object.Get()) || !spawn.Set<R::FName>(L"AttachPointName",{}) ||
       !spawn.Set<FVector>(L"Location",location) || !spawn.Set<FRotator>(L"Rotation",{}) ||
       !spawn.Set<FVector>(L"Scale",{1,1,1}) || !spawn.Set<uint8_t>(L"LocationType",0) ||
       !spawn.Set<bool>(L"bAutoDestroy",false) || !spawn.Set<uint8_t>(L"PoolingMethod",0) ||
       !spawn.Set<bool>(L"bAutoActivate",true) || !valid() || !Call(library,spawn))return nullptr;
    void* result=nullptr;if(!spawn.GetRaw(L"ReturnValue",&result,sizeof(result)) || !result || !R::IsLive(result))return nullptr;
    Owned candidate(result);
    void* componentClass=object_index::ClassByName(L"ParticleSystemComponent");
    if(!componentClass || !candidate.Current() || !R::IsDescendantOfAny(candidate.cls.Get(),&componentClass,1))return nullptr;
    if(!valid() || !candidate.Current()){
        if(candidate.object.Get())engine::DestroyComponent(candidate.object.Get(),source.object.Get());
        return nullptr;
    }
    return candidate.object.Get();
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
    Owned live(actor);
    void* audio=Audio(actor);if(!audio)return false;
    Owned sound(audio);
    static R::InstanceOffset volume{L"VolumeMultiplier"},pitch{L"PitchMultiplier"};
    State value{};void* zero=nullptr;if(!ReadZero(actor,zero))return false;
    Owned component(zero);value.active=zero!=nullptr;
    const auto valid=[&]{void* actual=nullptr;return live.Current() && sound.Current() && Audio(actor)==audio &&
        ReadZero(actor,actual) && actual==zero && (!zero || component.Current()) && live.Current() && sound.Current();};
    if(zero) {
        int off=-1;uint8_t mask=0;
        if(!R::FindBoolProperty(R::ClassOf(zero),L"dir",off,mask) || off<0 || !mask || !valid())return false;
        value.effect=(static_cast<uint8_t*>(zero)[off]&mask)!=0;
    }
    if(!Field(audio,volume,value.volume) || !Field(audio,pitch,value.pitch) ||
       !Valid(value) || !valid())return false;
    out=value;return true;
}
bool Activate(void* actor,const std::function<bool()>& current) {
    if(!IsToy(actor))return false;Owned live(actor);
    const auto valid=[&]{return live.Current() && (!current || current());};
    ParamFrame activate(R::FindDispatchFunctionCached(R::ClassOf(actor),L"activate"));
    return activate.valid() && valid() && Call(actor,activate) && valid();
}
bool ApplyAudio(void* actor,const State& state,const std::function<bool()>& current) {
    if(!Valid(state) || (current && !current()))return false;
    Owned live(actor),audio(Audio(actor));
    auto valid=[&]{return live.Current() && audio.Current() && Audio(actor)==audio.object.Get() &&
        live.Current() && audio.Current() && (!current || current());};
    if(!valid())return false;
    ParamFrame playing(R::FindDispatchFunctionCached(R::ClassOf(audio.object.Get()),L"IsPlaying"));
    if(!valid() || !playing.valid() || !Call(audio.object.Get(),playing) || !valid())return false;
    if(!component_calls::SetVolumeMultiplier(audio.object.Get(),state.volume) || !valid())return false;
    ParamFrame pitch(R::FindDispatchFunctionCached(R::ClassOf(audio.object.Get()),L"SetPitchMultiplier"));
    if(!valid() || !pitch.valid() || !pitch.Set<float>(L"NewPitchMultiplier",state.pitch) ||
       !valid() || !Call(audio.object.Get(),pitch) || !valid())return false;
    return component_calls::SetAudioPlaying(audio.object.Get(),state.active,valid) && valid();
}
void Clear(View& view) {
    const auto old=view;view={};const auto revision=view.revision=Revision();
    const auto owns=[&]{return view.revision==revision;};
    if(void* actor=old.actor.Get())ApplyAudio(actor,{},owns);
    if(void* effect=old.effect.Get()) {
        if(!owns() && view.effect.Is(effect))return;
        component_calls::SetActive(effect,false,false);
        if(!owns() && view.effect.Is(effect))return;
        if(old.ownsEffect && old.effect.Get())engine::DestroyComponent(old.effect.Get(),old.actor.Get());
    }
}
bool Apply(View& view,void* actor,const State& state,const std::function<bool()>& current) {
    if(!Valid(state) || !IsToy(actor) || !current())return false;
    Owned live(actor);
    if(!view.actor.Is(actor)){
        Clear(view);if(!current() || !live.Current() || view.actor.Get())return false;view.actor=live.object;
    }
    const auto revision=view.revision=Revision();
    const auto valid=[&]{return view.revision==revision && current() && live.Current();};
    if(!ApplyAudio(actor,state,valid) || !valid())return false;
    if(!view.effect.Get() && state.effect) {
        CachedObjRef candidate;candidate.Set(NativeEffect(actor));bool owned=false;
        if(!candidate.Get()){candidate.Set(SpawnEffect(actor,valid));owned=true;}
        if(!valid()){
            if(owned && candidate.Get() && !view.effect.Is(candidate.Get()))engine::DestroyComponent(candidate.Get(),live.object.Get());
            return false;
        }
        view.effect=candidate;view.ownsEffect=owned;
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
