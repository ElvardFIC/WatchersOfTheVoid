#ifndef WATOY_WRAPPER_SOURCE
#define WATOY_WRAPPER_SOURCE "../../src/votv-coop/src/ue_wrap/actors/watoy.cpp"
#endif
#include WATOY_WRAPPER_SOURCE
#include <cstdio>
#include <cstdlib>
#include <map>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
enum Index {World,Actor,Audio,Zero,NativeEffect,Mesh,ToyClass,ZeroClass,ParticleClass,Template,Package,Library,System,Math,Spawned,Other,PSCClass,Count};
struct Obj {int index=0,serial=1;bool live=true,active=false;void* audio=nullptr;void* zero=nullptr;void* owner=nullptr;void* effect=nullptr;float volume=.1f,pitch=1;uint8_t flags=1;} objects[Count];
uint32_t world=1;bool fields=true,playing=false,queryOk=true,spawnOk=true,valid=true;
int starts=0,stops=0,spawns=0,destroys=0,activations=0,cues=0;
std::wstring callFail;
std::wstring afterName;
std::function<void(const std::wstring&)> callback;
struct Function:Obj {std::wstring name;};
std::map<std::wstring,Function> functions;
std::map<std::wstring,void*> dispatchOverrides;
std::wstring schemaFunction,schemaParameter;
int schemaFault=0;
std::wstring missingField,metaName,badOutputFunction,badOutputName,poisonOutput;
std::function<void()> metaCallback;
std::vector<std::wstring> calls;
void* meshOverride=nullptr;void* spawnResult=nullptr;
void* classes[Count]{};
void Meta(const std::wstring& name){if(name!=metaName)return;auto callback=std::move(metaCallback);metaCallback={};if(callback)callback();}
using Bytes=std::vector<uint8_t>;
std::map<const ue_wrap::ParamFrame*,std::map<std::wstring,Bytes>> frames;
void* At(Index i){return &objects[i];}
void* Fn(const wchar_t* name){auto& value=functions[name];if(value.name.empty()){
    value.name=name;value.index=Count+int(functions.size());}return &value;}
Obj* ByIndex(int i){if(i>=0 && i<Count)return &objects[i];
    for(auto& [name,fn]:functions)if(fn.index==i)return &fn;return nullptr;}
const std::wstring& FunctionName(void* p){return static_cast<Function*>(p)->name;}
template<class T> T Get(const ue_wrap::ParamFrame& f,const wchar_t* name){T out{};CHECK(f.GetRaw(name,&out,sizeof(out)));return out;}
template<class T> void Put(ue_wrap::ParamFrame& f,const wchar_t* name,const T& value){CHECK(f.SetRaw(name,&value,sizeof(value)));}
void Callback(const std::wstring& function){if(!afterName.empty() && afterName!=function)return;afterName.clear();auto f=std::move(callback);callback={};if(f)f(function);}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p && static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->index:-1;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
int32_t SlotSerial(int32_t i){auto* value=test::ByIndex(i);return value?value->serial:0;}
bool IsLiveByIndex(void* p,int32_t i){return p && p==test::ByIndex(i) && IsLive(p);}
void* ClassOf(void* p){if(p && static_cast<test::Obj*>(p)->index>=test::Count)return p;
    if(p && test::classes[static_cast<test::Obj*>(p)->index])return test::classes[static_cast<test::Obj*>(p)->index];
    if(p==test::At(test::Actor))return test::At(test::ToyClass);if(p==test::At(test::Zero))return test::At(test::ZeroClass);
    if(p==test::At(test::Spawned) || p==test::At(test::NativeEffect))return test::At(test::PSCClass);return p;}
bool IsDescendantOfAny(void* cls,void* const* bases,size_t n,int){return n==1 && cls==bases[0];}
const FName& NameOf(void* p){static FName names[test::Count]{};const auto i=static_cast<test::Obj*>(p)->index;names[i].ComparisonIndex=i;return names[i];}
bool NameEquals(const FName& name,const wchar_t* text){return (name.ComparisonIndex==test::Template && !wcscmp(text,L"eff_zgrav")) ||
    (name.ComparisonIndex==test::Package && !wcscmp(text,L"/Game/particles/eff_zgrav"));}
void* OuterOf(void* p){if(p==test::At(test::Zero))return test::At(test::Actor);return p==test::At(test::Template)?test::At(test::Package):nullptr;}
void* FindClassDefaultObject(const wchar_t* name){if(!wcscmp(name,L"GameplayStatics"))return test::At(test::Library);
    if(!wcscmp(name,L"KismetSystemLibrary"))return test::At(test::System);CHECK(!wcscmp(name,L"KismetMathLibrary"));return test::At(test::Math);}
void* FindDispatchFunctionCached(void*,const wchar_t* name){test::Meta(std::wstring(L"resolve:")+name);
    const auto it=test::dispatchOverrides.find(name);return it==test::dispatchOverrides.end()?test::Fn(name):it->second;}
std::vector<ParamInfo> FunctionParams(void* function){
    using namespace test;const auto& name=FunctionName(function);std::vector<ParamInfo> result;
    const auto add=[&](const wchar_t* field,int bytes){int offset=result.empty()?0:result.back().offset+result.back().size;
        result.push_back({field,offset,bytes,0x80});};
    if(name==L"IsPlaying")add(L"ReturnValue",1);
    else if(name==L"SetVolumeMultiplier")add(L"NewVolumeMultiplier",4);
    else if(name==L"SetPitchMultiplier")add(L"NewPitchMultiplier",4);
    else if(name==L"Play")add(L"StartTime",4);
    else if(name==L"SetActive"){add(L"bNewActive",1);add(L"bReset",1);}
    else if(name==L"K2_GetComponentToWorld")add(L"ReturnValue",sizeof(FTransform));
    else if(name==L"GetComponentBounds"){add(L"Component",sizeof(void*));add(L"Origin",sizeof(FVector));add(L"BoxExtent",sizeof(FVector));add(L"SphereRadius",4);}
    else if(name==L"InverseTransformLocation"){add(L"T",sizeof(FTransform));add(L"Location",sizeof(FVector));add(L"ReturnValue",sizeof(FVector));}
    else if(name==L"SpawnEmitterAttached"){
        add(L"EmitterTemplate",sizeof(void*));add(L"AttachToComponent",sizeof(void*));add(L"AttachPointName",sizeof(FName));
        add(L"Location",sizeof(FVector));add(L"Rotation",sizeof(FRotator));add(L"Scale",sizeof(FVector));
        add(L"LocationType",1);add(L"bAutoDestroy",1);add(L"PoolingMethod",1);add(L"bAutoActivate",1);add(L"ReturnValue",sizeof(void*));
    }else CHECK(name==L"activate" || name==L"Stop");
    if(schemaFunction==name){
        for(auto& field:result)if(field.name==schemaParameter){
            if(schemaFault==1)++field.size;if(schemaFault==2)field.offset=256;if(schemaFault==3)field.offset=0;
        }
        if(schemaFault==4)add(L"Unexpected",4);
        if(schemaFault==5)std::erase_if(result,[](const auto& p){return p.name==schemaParameter;});
        if(schemaFault==6 && !result.empty())result.push_back(result.front());
    }
    return result;
}
int32_t FunctionFrameSize(void*){return 256;}
int32_t InstanceOffset::Of(void*) {
    test::Meta(std::wstring(L"field:")+prop_);
    if(!test::fields || test::missingField==prop_)return -1;
    if(!wcscmp(prop_,L"Audio"))return offsetof(test::Obj,audio);
    if(!wcscmp(prop_,L"a"))return offsetof(test::Obj,zero);
    if(!wcscmp(prop_,L"owner"))return offsetof(test::Obj,owner);
    if(!wcscmp(prop_,L"eff"))return offsetof(test::Obj,effect);
    if(!wcscmp(prop_,L"VolumeMultiplier"))return offsetof(test::Obj,volume);
    CHECK(!wcscmp(prop_,L"PitchMultiplier"));return offsetof(test::Obj,pitch);
}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& off,uint8_t& mask){CHECK(!wcscmp(name,L"dir"));test::Meta(L"bool:dir");off=offsetof(test::Obj,flags);mask=1;return test::fields;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}void* CurrentWorld(){return test::At(test::World);}void* WorldOf(void*){return CurrentWorld();}
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* name){if(!wcscmp(name,L"prop_watoy_C"))return test::At(test::ToyClass);
    if(!wcscmp(name,L"ParticleSystemComponent"))return test::At(test::PSCClass);
    if(!wcscmp(name,L"comp_zerog_C"))return test::At(test::ZeroClass);CHECK(!wcscmp(name,L"ParticleSystem"));return test::At(test::ParticleClass);}
size_t ForEachInstance(void* cls,void(*fn)(void*,void*,int32_t),void* ctx){auto i=cls==test::At(test::ToyClass)?test::Actor:test::Template;fn(ctx,test::At(i),i);return 1;}
}
namespace ue_wrap::prop {void* GetStaticMesh(void* actor){CHECK(actor==test::At(test::Actor));return test::meshOverride?test::meshOverride:test::At(test::Mesh);}}
namespace ue_wrap::engine {
void* GetWorldContext(){return test::At(test::World);}
void* FindSound(const std::wstring& name){CHECK(name==L"SoundWave /Game/audio/sort/buttonlong10.buttonlong10");return test::At(test::Template);}
void PlaySound2D(void* context,void* sound,float volume,float pitch,float age,bool ui){CHECK(context==test::At(test::World) && sound==test::At(test::Template));CHECK(volume==1 && pitch==.25f && age==0 && ui);++test::cues;}
bool DestroyComponent(void* effect,void*){CHECK(effect==test::At(test::Spawned));++test::destroys;static_cast<test::Obj*>(effect)->live=false;return true;}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn){test::frames[this].clear();}
int32_t ParamFrame::OffsetOf(const wchar_t* name)const{for(const auto& param:reflection::FunctionParams(fn_))if(param.name==name)return param.offset;return -1;}
bool ParamFrame::SetRaw(const wchar_t* name,const void* p,int32_t n){auto& bytes=test::frames[this][name];bytes.resize(n);std::memcpy(bytes.data(),p,n);return true;}
bool ParamFrame::GetRaw(const wchar_t* name,void* p,int32_t n)const {if(test::FunctionName(fn_)==test::badOutputFunction && name==test::badOutputName)return false;
    const auto& fields=test::frames[this];auto it=fields.find(name);if(it==fields.end() || it->second.size()!=n)return false;std::memcpy(p,it->second.data(),n);return true;}
bool Call(void* object,ParamFrame& f) {
    using namespace test;const auto name=FunctionName(f.function());calls.push_back(name);if(name==callFail)return false;
    if(name==L"activate"){CHECK(object==At(Actor));++activations;}
    else if(name==L"IsPlaying"){CHECK(object==At(Audio));if(!queryOk)return false;Put(f,L"ReturnValue",playing);}
    else if(name==L"SetPitchMultiplier"){CHECK(object==At(Audio));objects[Audio].pitch=Get<float>(f,L"NewPitchMultiplier");}
    else if(name==L"SetVolumeMultiplier"){CHECK(object==At(Audio));objects[Audio].volume=Get<float>(f,L"NewVolumeMultiplier");}
    else if(name==L"Play"){CHECK(object==At(Audio) && Get<float>(f,L"StartTime")==0);playing=true;++starts;}
    else if(name==L"Stop"){CHECK(object==At(Audio));playing=false;++stops;}
    else if(name==L"SetActive"){CHECK((object==At(Spawned) || object==At(NativeEffect)) && !Get<bool>(f,L"bReset"));
        static_cast<Obj*>(object)->active=Get<bool>(f,L"bNewActive");}
    else if(name==L"K2_GetComponentToWorld"){CHECK(object==At(Mesh));FTransform t{};t.TX=12;if(poisonOutput==name)t.RotX=NAN;Put(f,L"ReturnValue",t);}
    else if(name==L"GetComponentBounds"){CHECK(object==At(System) && Get<void*>(f,L"Component")==At(Mesh));Put(f,L"Origin",FVector{poisonOutput==name?NAN:22,32,42});}
    else if(name==L"InverseTransformLocation"){CHECK(object==At(Math) && Get<FTransform>(f,L"T").TX==12 && Get<FVector>(f,L"Location").X==22);Put(f,L"ReturnValue",FVector{poisonOutput==name?NAN:10,20,30});}
    else if(name==L"SpawnEmitterAttached") {
        CHECK(object==At(Library) && Get<void*>(f,L"EmitterTemplate")==At(Template) && Get<void*>(f,L"AttachToComponent")==At(Mesh));
        const auto pos=Get<FVector>(f,L"Location");CHECK(pos.X==10 && pos.Y==20 && pos.Z==30);
        CHECK(Get<FVector>(f,L"Scale").X==1 && Get<uint8_t>(f,L"LocationType")==0 && !Get<bool>(f,L"bAutoDestroy") && Get<bool>(f,L"bAutoActivate"));
        CHECK(Get<uint8_t>(f,L"PoolingMethod")==0 && Get<reflection::FName>(f,L"AttachPointName").ComparisonIndex==0);
        if(!spawnOk)return false;++spawns;if(!objects[Spawned].live)++objects[Spawned].serial;objects[Spawned].live=true;objects[Spawned].active=true;
        Put(f,L"ReturnValue",spawnResult?spawnResult:At(Spawned));
    } else if(name==L"GetOwner"){CHECK(object==At(Zero));Put(f,L"ReturnValue",At(Actor));}
    else CHECK(false);
    Callback(name);return true;
}
}
namespace ue_wrap::component_calls {
bool CallParamlessNamed(void* actor,const wchar_t* name){CHECK(actor==test::At(test::Actor) && !wcscmp(name,L"activate"));++test::activations;return true;}
bool SetVolumeMultiplier(void* audio,float volume){CHECK(audio==test::At(test::Audio));test::objects[test::Audio].volume=volume;return true;}
bool SetAudioPlaying(void* comp,bool active,const std::function<bool()>& current){
    CHECK(comp==test::At(test::Audio));if(current && !current())return false;
    if(test::playing!=active){if(active)++test::starts;else ++test::stops;test::playing=active;}return true;
}
bool SetActive(void* comp,bool active,bool reset){CHECK(!reset);if(comp==test::At(test::Audio)){
    if(active)++test::starts;else ++test::stops;test::playing=active;
} else {CHECK(comp==test::At(test::Spawned) || comp==test::At(test::NativeEffect));static_cast<test::Obj*>(comp)->active=active;}return true;}
}
#ifndef WATOY_WRAPPER_FIXTURE_ONLY
int main() {
    using namespace test;namespace W=ue_wrap::watoy;
    for(int i=0;i<Count;++i)objects[i].index=i;objects[Actor].audio=At(Audio);objects[Actor].zero=At(Zero);objects[Zero].owner=At(Actor);
    auto current=[] {return valid;};W::State state{};CHECK(W::Read(At(Actor),state) && state.active && state.effect);
    objects[Zero].flags=0;CHECK(W::Read(At(Actor),state) && state.active && !state.effect);objects[Zero].flags=1;
    objects[Zero].live=false;CHECK(W::Read(At(Actor),state) && !state.active && !state.effect);objects[Zero].live=true;
    CHECK(!W::IsToy(At(Other)) && W::ZeroGravityOwner(At(Other))==nullptr && W::ZeroGravityOwner(At(Zero))==At(Actor));
    objects[Zero].owner=nullptr;CHECK(W::ZeroGravityOwner(At(Zero))==At(Actor));objects[Zero].owner=At(Actor);
    W::View view;state={true,.6f,2.5f,true};
    CHECK(W::Apply(view,At(Actor),state,current) && playing && spawns==1 && starts==1 && view.OwnsEffect());
    CHECK(objects[Audio].volume==.6f && objects[Audio].pitch==2.5f && objects[Spawned].active && activations==0);
    CHECK(W::Apply(view,At(Actor),state,current) && starts==1 && spawns==1); // unchanged audio/particles do not restart
    playing=false;CHECK(W::Apply(view,At(Actor),state,current) && starts==2 && spawns==1);
    queryOk=false;CHECK(!W::Apply(view,At(Actor),state,current) && starts==2);queryOk=true;
    state.effect=false;CHECK(W::Apply(view,At(Actor),state,current) && destroys==1 && playing && !view.Effect().Get());
    state.active=false;CHECK(W::Apply(view,At(Actor),state,current) && !playing && stops==1);
    state={true,.7f,3,true};objects[Zero].effect=At(NativeEffect);
    CHECK(W::Apply(view,At(Actor),state,current) && !view.OwnsEffect() && spawns==1 && objects[NativeEffect].active);
    W::Clear(view);CHECK(!view.Actor().Get() && !objects[NativeEffect].active && destroys==1);objects[Zero].effect=nullptr;
    spawnOk=false;CHECK(!W::Apply(view,At(Actor),state,current) && !view.Effect().Get());spawnOk=true;
    CHECK(W::Apply(view,At(Actor),state,current) && spawns==2);W::Clear(view);CHECK(destroys==2);
    // A change of session/world during the spawn releases the just-created PSC.
    callback=[&](const std::wstring& name){CHECK(name==L"IsPlaying");valid=false;};CHECK(!W::Apply(view,At(Actor),state,current));W::Clear(view);valid=true;
    afterName=L"SpawnEmitterAttached";callback=[&](const std::wstring&){valid=false;};
    CHECK(!W::Apply(view,At(Actor),state,current) && !view.Effect().Get() && destroys==3 && spawns==3);valid=true;
    callFail=L"InverseTransformLocation";CHECK(!W::Apply(view,At(Actor),state,current));CHECK(!view.Effect().Get());callFail.clear();
    callback=[&](const std::wstring&){++world;};CHECK(!W::Apply(view,At(Actor),state,current));W::Clear(view);
    fields=false;CHECK(!W::Read(At(Actor),state));fields=true;
    state={true,.5f,1,true};CHECK(W::Apply(view,At(Actor),state,current));
    objects[Spawned].live=false;++objects[Spawned].serial;CHECK(W::Apply(view,At(Actor),state,current) && spawns==5);
    W::Clear(view);auto invalid=state;invalid.pitch=NAN;CHECK(!W::Apply(view,At(Actor),invalid,current));
    invalid=state;invalid.active=false;CHECK(!W::Apply(view,At(Actor),invalid,current));
    CHECK(W::PlayStartCue() && cues==1);CHECK(W::Activate(At(Actor)) && activations==1);
    puts("PASS watoy wrapper: native phase fields, audio recovery, exact particle attachment, native-effect adoption, owned cleanup, refusal/serial/world retry, no physics replay");
}
#endif
