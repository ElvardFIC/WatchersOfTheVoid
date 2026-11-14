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
#include <initializer_list>
#include <vector>
#include <unordered_map>
namespace ue_wrap::watoy {
namespace {
namespace R=reflection;
struct Owned {
    CachedObjRef object,cls;uint32_t world=world_identity::Generation();
    explicit Owned(void* value){object.Set(value);if(object.Get())cls.Set(R::ClassOf(value));}
    bool Current()const{return object.Get() && cls.Get() && R::ClassOf(object.Get())==cls.Get() && world==world_identity::Generation();}
};
struct Parameter {const wchar_t* name;int size;};
struct NativeCall {
    Owned owner;
    const wchar_t* name;
    CachedObjRef function;
    std::vector<Parameter> parameters;
    ParamFrame frame;
    static CachedObjRef Resolve(const Owned& owner,const wchar_t* name){
        CachedObjRef result;
        if(owner.Current())result.Set(R::FindDispatchFunctionCached(owner.cls.Get(),name));
        return result;
    }
    NativeCall(void* object,const wchar_t* verb,std::initializer_list<Parameter> schema)
        :owner(object),name(verb),function(Resolve(owner,verb)),parameters(schema),frame(function.Get()){}
    bool Current()const{
        return owner.Current() && function.Get() &&
            R::FindDispatchFunctionCached(owner.cls.Get(),name)==function.Get() &&
            owner.Current() && function.Get();
    }
    bool Schema()const{
        if(!Current() || !frame.valid())return false;
        const int size=R::FunctionFrameSize(function.Get());
        if(size<0 || size>65536)return false;
        const auto actual=R::FunctionParams(function.Get());
        if(actual.size()!=parameters.size())return false;
        for(size_t i=0;i<actual.size();++i){
            const auto& field=actual[i];
            if(field.size<=0 || field.offset<0 || field.size>size || field.offset>size-field.size)return false;
            for(size_t j=0;j<i;++j)
                if(field.offset<actual[j].offset+actual[j].size && actual[j].offset<field.offset+field.size)return false;
        }
        for(const auto& expected:parameters){
            size_t matches=0;
            for(const auto& field:actual)if(!_wcsicmp(field.name.c_str(),expected.name)){
                if(field.size!=expected.size || frame.ParamOffset(expected.name)!=field.offset)return false;
                ++matches;
            }
            if(matches!=1)return false;
        }
        return Current();
    }
    bool Run(const std::function<bool()>& current,bool* dispatched=nullptr){
        if(dispatched)*dispatched=false;
        if(!Schema() || (current && !current()) || !Schema())return false;
        if(dispatched)*dispatched=true;
        return Call(owner.object.Get(),frame) && Current() && (!current || current()) && Current();
    }
};
bool SetEffectActive(void* effect,bool active,const std::function<bool()>& current){
    NativeCall call(effect,L"SetActive",{{L"bNewActive",sizeof(bool)},{L"bReset",sizeof(bool)}});
    return call.Schema() && call.frame.Set<bool>(L"bNewActive",active) && call.frame.Set<bool>(L"bReset",false) && call.Run(current);
}
uint64_t g_revision=0;
uint64_t Revision(){if(!++g_revision)++g_revision;return g_revision;}
std::unordered_map<void*,std::weak_ptr<Presentation>> g_presentations;
uint32_t g_presentationsWorld=0;
void ScopePresentations(){
    const auto world=world_identity::Generation();
    if(world!=g_presentationsWorld){g_presentations.clear();g_presentationsWorld=world;}
}
std::shared_ptr<Presentation> CurrentPresentation(void* actor){
    ScopePresentations();const auto it=g_presentations.find(actor);if(it==g_presentations.end())return {};
    auto lease=it->second.lock();
    if(lease && !lease->retired && lease->world==g_presentationsWorld && lease->actor.Is(actor) &&
       lease->cls.Get() && R::ClassOf(actor)==lease->cls.Get())return lease;
    g_presentations.erase(it);return {};
}
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
    NativeCall transform(mesh.object.Get(),L"K2_GetComponentToWorld",{{L"ReturnValue",sizeof(FTransform)}});
    if(!transform.Run(valid))return nullptr;
    FTransform world{};
    if(!transform.frame.GetRaw(L"ReturnValue",&world,sizeof(world)) ||
       !std::isfinite(world.RotX) || !std::isfinite(world.RotY) || !std::isfinite(world.RotZ) || !std::isfinite(world.RotW) ||
       !std::isfinite(world.TX) || !std::isfinite(world.TY) || !std::isfinite(world.TZ) ||
       !std::isfinite(world.SX) || !std::isfinite(world.SY) || !std::isfinite(world.SZ) || !valid())return nullptr;
    NativeCall bounds(system,L"GetComponentBounds",{{L"Component",sizeof(void*)},{L"Origin",sizeof(FVector)},
        {L"BoxExtent",sizeof(FVector)},{L"SphereRadius",sizeof(float)}});
    if(!bounds.Schema() || !bounds.frame.Set<void*>(L"Component",mesh.object.Get()) || !bounds.Run(valid))return nullptr;
    FVector origin{};
    if(!bounds.frame.GetRaw(L"Origin",&origin,sizeof(origin)) || !std::isfinite(origin.X) || !std::isfinite(origin.Y) || !std::isfinite(origin.Z) || !valid())return nullptr;
    NativeCall inverse(math,L"InverseTransformLocation",{{L"T",sizeof(FTransform)},{L"Location",sizeof(FVector)},{L"ReturnValue",sizeof(FVector)}});
    if(!inverse.Schema() || !inverse.frame.Set<FTransform>(L"T",world) || !inverse.frame.Set<FVector>(L"Location",origin) ||
       !inverse.Run(valid))return nullptr;
    FVector location{};
    if(!inverse.frame.GetRaw(L"ReturnValue",&location,sizeof(location)) || !std::isfinite(location.X) || !std::isfinite(location.Y) || !std::isfinite(location.Z) || !valid())return nullptr;
    NativeCall spawn(library,L"SpawnEmitterAttached",{{L"EmitterTemplate",sizeof(void*)},{L"AttachToComponent",sizeof(void*)},
        {L"AttachPointName",sizeof(R::FName)},{L"Location",sizeof(FVector)},{L"Rotation",sizeof(FRotator)},
        {L"Scale",sizeof(FVector)},{L"LocationType",1},{L"bAutoDestroy",1},{L"PoolingMethod",1},
        {L"bAutoActivate",1},{L"ReturnValue",sizeof(void*)}});
    if(!spawn.Schema() || !valid() || !spawn.frame.Set<void*>(L"EmitterTemplate",tmpl.object.Get()) ||
       !spawn.frame.Set<void*>(L"AttachToComponent",mesh.object.Get()) || !spawn.frame.Set<R::FName>(L"AttachPointName",{}) ||
       !spawn.frame.Set<FVector>(L"Location",location) || !spawn.frame.Set<FRotator>(L"Rotation",{}) ||
       !spawn.frame.Set<FVector>(L"Scale",{1,1,1}) || !spawn.frame.Set<uint8_t>(L"LocationType",0) ||
       !spawn.frame.Set<bool>(L"bAutoDestroy",false) || !spawn.frame.Set<uint8_t>(L"PoolingMethod",0) ||
       !spawn.frame.Set<bool>(L"bAutoActivate",true) || !valid() || !spawn.Schema() || !Call(library,spawn.frame))return nullptr;
    // Extract the candidate even if the caller retired during spawning, so the
    // cleanup below can destroy the component owned by that retired call.
    void* result=nullptr;if(!spawn.frame.GetRaw(L"ReturnValue",&result,sizeof(result)) || !result || !R::IsLive(result))return nullptr;
    Owned candidate(result);
    void* componentClass=object_index::ClassByName(L"ParticleSystemComponent");
    if(!componentClass || !candidate.Current() || !R::IsDescendantOfAny(candidate.cls.Get(),&componentClass,1))return nullptr;
    if(!valid() || !spawn.Current() || !candidate.Current()){
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
bool Activate(void* actor,const std::function<bool()>& current,bool* dispatched) {
    if(dispatched)*dispatched=false;
    if(!IsToy(actor))return false;Owned live(actor);
    const auto valid=[&]{return live.Current() && (!current || current());};
    NativeCall activate(actor,L"activate",{});
    return activate.Run(valid,dispatched);
}
bool ApplyAudio(void* actor,const State& state,const std::function<bool()>& current) {
    if(!Valid(state) || (current && !current()))return false;
    Owned live(actor),audio(Audio(actor));
    auto valid=[&]{return live.Current() && audio.Current() && Audio(actor)==audio.object.Get() &&
        live.Current() && audio.Current() && (!current || current());};
    if(!valid())return false;
    NativeCall playing(audio.object.Get(),L"IsPlaying",{{L"ReturnValue",sizeof(bool)}});
    bool wasPlaying=false;
    if(!playing.Run(valid) || !playing.frame.GetRaw(L"ReturnValue",&wasPlaying,sizeof(wasPlaying)))return false;
    NativeCall volume(audio.object.Get(),L"SetVolumeMultiplier",{{L"NewVolumeMultiplier",sizeof(float)}});
    if(!volume.Schema() || !volume.frame.Set<float>(L"NewVolumeMultiplier",state.volume) || !volume.Run(valid))return false;
    NativeCall pitch(audio.object.Get(),L"SetPitchMultiplier",{{L"NewPitchMultiplier",sizeof(float)}});
    if(!pitch.Schema() || !pitch.frame.Set<float>(L"NewPitchMultiplier",state.pitch) || !pitch.Run(valid))return false;
    if(wasPlaying==state.active)return valid();
    NativeCall playback(audio.object.Get(),state.active?L"Play":L"Stop",
        state.active?std::initializer_list<Parameter>{{L"StartTime",sizeof(float)}}:std::initializer_list<Parameter>{});
    return playback.Schema() && (!state.active || playback.frame.Set<float>(L"StartTime",0.f)) && playback.Run(valid);
}
void Clear(View& view) {
    const auto lease=view.presentation;
    const auto admitted=view.revision;
    view={};const auto revision=view.revision=Revision();
    if(!lease || lease->retired || lease->revision!=admitted)return;
    const auto old=*lease;
    lease->actor={};lease->effect={};lease->ownsEffect=false;
    lease->retired=true;lease->revision=revision;
    const auto owner=CurrentPresentation(old.actor.Get());
    if(old.world!=world_identity::Generation() || !old.actor.Get() || !old.cls.Get() ||
       R::ClassOf(old.actor.Get())!=old.cls.Get() || (owner && owner!=lease))return;
    const auto owns=[&]{const auto active=CurrentPresentation(old.actor.Get());return view.revision==revision &&
        lease->revision==revision && lease->retired && old.world==world_identity::Generation() && !active;};
    if(void* actor=old.actor.Get())ApplyAudio(actor,{},owns);
    if(void* effect=old.effect.Get()) {
        if(old.world!=world_identity::Generation())return;
        const auto reused=[&]{const auto active=CurrentPresentation(old.actor.Get());return
            view.Effect().Is(effect) || lease->effect.Is(effect) || (active && active->effect.Is(effect));};
        if(!owns() && reused())return;
        SetEffectActive(effect,false,[&]{return old.world==world_identity::Generation() && old.effect.Get() && (owns() || !reused());});
        if(old.world!=world_identity::Generation())return;
        if(!owns() && reused())return;
        if(old.ownsEffect && old.effect.Get())engine::DestroyComponent(old.effect.Get(),old.actor.Get());
    }
}
bool Apply(View& view,void* actor,const State& state,const std::function<bool()>& current) {
    if(!Valid(state) || !IsToy(actor) || !current())return false;
    Owned live(actor);
    if(view.presentation && !view.presentation->retired && !view.presentation->actor.Is(actor)){
        Clear(view);if(!current() || !live.Current() || view.presentation)return false;
    }
    if(auto active=CurrentPresentation(actor))view.presentation=active;
    else {
        // At most 256 world views, 32 hands and their staged replacements are
        // admitted by the controller. Retired weak entries never grow forever.
        if(g_presentations.size()>=512)std::erase_if(g_presentations,[](const auto& row){return row.second.expired();});
        if(g_presentations.size()>=512)return false;
        view.presentation=std::make_shared<Presentation>();
        g_presentations[actor]=view.presentation;
    }
    const auto lease=view.presentation;
    lease->retired=false;lease->actor=live.object;lease->cls=live.cls;lease->world=live.world;
    const auto revision=view.revision=Revision();
    lease->revision=revision;
    const auto valid=[&]{return view.revision==revision && view.presentation==lease &&
        lease->revision==revision && !lease->retired && current() && live.Current();};
    if(!ApplyAudio(actor,state,valid) || !valid())return false;
    if(!lease->effect.Get() && state.effect) {
        CachedObjRef candidate;candidate.Set(NativeEffect(actor));bool owned=false;
        if(!candidate.Get()){candidate.Set(SpawnEffect(actor,valid));owned=true;}
        if(!valid()){
            if(owned && candidate.Get() && !view.Effect().Is(candidate.Get()) && !lease->effect.Is(candidate.Get()))
                engine::DestroyComponent(candidate.Get(),live.object.Get());
            return false;
        }
        lease->effect=candidate;lease->ownsEffect=owned;
        if(!lease->effect.Get())return false;
    }
    if(void* effect=lease->effect.Get()) {
        if(!SetEffectActive(effect,state.effect,valid) || !valid())return false;
        if(!state.effect && lease->ownsEffect) {
            const auto old=lease->effect;lease->effect={};lease->ownsEffect=false;
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
