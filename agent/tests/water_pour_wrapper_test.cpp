#include "../../src/votv-coop/src/ue_wrap/actors/water_pour.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
enum Id {Actor,Other,PourClass,IgnoredClass,Mesh,Library,World,ParticleClass,SoundClass,AttClass,
    Particle,ParticlePackage,Sound,SoundPackage,Att,AttPackage,HitStruct,GetVelocity,SpawnEmitter,PlaySound,Collision,Foreign,Life,SplashSound,SplashPackage,Count};
struct Obj {int index=0,serial=1;bool live=true;float soap=12,power=1;} objects[Count];
struct Hit {ue_wrap::FVector location{100,200,300},normal{0,0,1},impact{9,9,9},impactNormal{1,0,0};};
struct Params {void* world=nullptr;void* asset=nullptr;ue_wrap::FVector location{},velocity{};
    ue_wrap::FRotator rotation{};ue_wrap::FVector scale{};bool destroy=false;uint8_t pooling=0;bool activate=false;
    float volume=0,pitch=0,start=0,life=0;void* attenuation=nullptr;void* concurrency=nullptr;void* owner=nullptr;void* result=nullptr;};
uint32_t world=1;bool fields=true,classReady=true,params=true,callOk=true,beginOk=true,finishOk=true,velocityOk=true,meshOk=true,locationOk=true,ignoreOther=false;
bool shot=false;
float soundPitch=2;
float soundVolume=1;
Id soundAsset=Sound;
ue_wrap::FVector soundAt{110,210,310};
ue_wrap::FVector constructedAt{};
int locationWrites=0,rotationWrites=0,lifeWrites=0;
int parameterBytes=sizeof(Params);
int begins=0,finishes=0,velocityWrites=0,destroys=0,particles=0,sounds=0,enumerations=0;
ue_wrap::water_pour::Birth expected{{100,200,300},{5,6,7},12};
ue_wrap::FRotator rotation{};
std::wstring missing;
std::function<void()> onLocation,onLookup,onVelocity,onBegin,onFinish,onSetVelocity,onEmitter,onSound;
void Run(std::function<void()>& fn){auto f=std::move(fn);fn={};if(f)f();}
void* At(Id id){return &objects[id];}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p && static_cast<test::Obj*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0 && i<test::Count && p==&test::objects[i] && IsLive(p);}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->index:-1;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return &test::objects[i];}
void* ClassOf(void* p){if(p==test::At(test::Actor))return test::At(test::shot?test::IgnoredClass:test::PourClass);
    if(p==test::At(test::Sound) || p==test::At(test::SplashSound))return test::At(test::SoundClass);
    if(p==test::At(test::Other))return test::At(test::ignoreOther?test::IgnoredClass:test::Foreign);return p;}
bool IsDescendantOfAny(void* cls,void* const* bases,size_t count,int){return count==1 && cls==bases[0];}
int32_t InstanceOffset::Of(void*){CHECK(!wcscmp(prop_,L"soapAmount") || !wcscmp(prop_,L"power"));return !test::fields?-1:(!wcscmp(prop_,L"power")?offsetof(test::Obj,power):offsetof(test::Obj,soap));}
int32_t FindPropertyOffset(void* cls,const wchar_t* name){
    if(!test::fields)return -1;if(cls==test::At(test::IgnoredClass)){CHECK(!wcscmp(name,L"power"));return offsetof(test::Obj,power);}
    if(cls==test::At(test::PourClass)){CHECK(!wcscmp(name,L"soapAmount"));return offsetof(test::Obj,soap);}
    CHECK(cls==test::At(test::HitStruct));if(!wcscmp(name,L"Location"))return offsetof(test::Hit,location);
    CHECK(!wcscmp(name,L"Normal"));test::Run(test::onLookup);return offsetof(test::Hit,normal);}
void* PropertyInnerStruct(void* cls,const wchar_t* name){CHECK(cls==test::At(test::Collision) && (!wcscmp(name,L"InputPin5") || !wcscmp(name,L"Hit")));return test::fields?test::At(test::HitStruct):nullptr;}
int32_t StructSize(void* cls){CHECK(cls==test::At(test::HitStruct));return sizeof(test::Hit);}
const FName& NameOf(void* p){static FName names[test::Count]{};const int i=static_cast<test::Obj*>(p)->index;names[i].ComparisonIndex=i;return names[i];}
bool NameEquals(const FName& name,const wchar_t* text){using namespace test;
    switch(name.ComparisonIndex){case Particle:return !wcscmp(text,L"eff_waterThrowSplash");case Sound:return !wcscmp(text,L"wade_Cue");case Att:return !wcscmp(text,L"att_default");
    case ParticlePackage:return !wcscmp(text,L"/Game/particles/eff_waterThrowSplash");case SoundPackage:return !wcscmp(text,L"/Game/audio/effects/physics/footsteps/wade_Cue");
    case AttPackage:return !wcscmp(text,L"/Game/audio/misc/att_default");case HitStruct:return !wcscmp(text,L"HitResult");
    case SoundClass:return !wcscmp(text,L"SoundCue");case SplashSound:return !wcscmp(text,L"water_splash_Cue");
    case SplashPackage:return !wcscmp(text,L"/Game/audio/effects/physics/water/water_splash_Cue");default:return false;}}
void* OuterOf(void* p){using namespace test;if(p==At(Particle))return At(ParticlePackage);if(p==At(Sound))return At(SoundPackage);if(p==At(Att))return At(AttPackage);if(p==At(SplashSound))return At(SplashPackage);return nullptr;}
void* FindClassDefaultObject(const wchar_t* name){CHECK(!wcscmp(name,L"GameplayStatics"));return test::At(test::Library);}
void* FindDispatchFunctionCached(void*,const wchar_t* name){using namespace test;if(missing==name)return nullptr;
    if(!wcscmp(name,L"GetVelocity"))return At(GetVelocity);if(!wcscmp(name,L"SpawnEmitterAtLocation"))return At(SpawnEmitter);
    if(!wcscmp(name,L"SetLifeSpan"))return At(Life);
    CHECK(!wcscmp(name,L"PlaySoundAtLocation"));return At(PlaySound);}
int32_t FunctionFrameSize(void*){return test::parameterBytes;}
std::vector<ParamInfo> FunctionParams(void* fn){using namespace test;std::vector<ParamInfo> result;
    const auto add=[&](const wchar_t* name,int offset){if(params || wcscmp(name,L"bAutoActivate")){ParamInfo p{};p.name=name;p.offset=offset;result.push_back(p);}};
    if(fn==At(GetVelocity)){add(L"ReturnValue",offsetof(Params,velocity));return result;}
    if(fn==At(Life)){add(L"InLifespan",offsetof(Params,life));return result;}
    CHECK(fn==At(SpawnEmitter) || fn==At(PlaySound));
    #define ADD(name,field) add(L##name,offsetof(Params,field))
    ADD("WorldContextObject",world);ADD("Location",location);ADD("Rotation",rotation);
    if(fn==At(SpawnEmitter)){ADD("EmitterTemplate",asset);ADD("Scale",scale);ADD("bAutoDestroy",destroy);ADD("PoolingMethod",pooling);ADD("bAutoActivate",activate);ADD("ReturnValue",result);}
    else {ADD("Sound",asset);ADD("VolumeMultiplier",volume);ADD("PitchMultiplier",pitch);ADD("StartTime",start);ADD("AttenuationSettings",attenuation);ADD("ConcurrencySettings",concurrency);ADD("OwningActor",owner);}
    #undef ADD
    return result;
}
int32_t FindParamOffset(void* fn,const wchar_t* name){for(const auto& p:FunctionParams(fn))if(p.name==name)return p.offset;return -1;}
bool CallFunction(void* object,void* fn,void* raw){using namespace test;auto& p=*static_cast<Params*>(raw);
    if(fn==At(GetVelocity)){CHECK(object==At(Actor));p.velocity=expected.velocity;Run(onVelocity);return callOk;}
    if(fn==At(Life)){CHECK(object==At(Actor) && p.life==10);++lifeWrites;return callOk;}
    CHECK(object==At(Library) && p.world==At(World));
    if(fn==At(SpawnEmitter)){
        CHECK(p.asset==At(Particle) && p.location.X==100 && p.location.Y==200 && p.location.Z==300);
        CHECK(p.scale.X==1 && p.scale.Y==1 && p.scale.Z==1 && p.destroy && !p.pooling && p.activate);
        rotation=p.rotation;++particles;p.result=At(Mesh);Run(onEmitter);
    }else {CHECK(fn==At(PlaySound) && p.asset==At(soundAsset) && p.location.X==soundAt.X && p.location.Y==soundAt.Y && p.location.Z==soundAt.Z);
        CHECK(p.volume==soundVolume && p.pitch==soundPitch && p.start==0 && p.attenuation==At(Att) && !p.concurrency && !p.owner);++sounds;Run(onSound);}
    return callOk;
}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* name){using namespace test;
    if(!wcscmp(name,L"prop_sponge_bucketPour_C"))return classReady?At(PourClass):nullptr;
    if(!wcscmp(name,L"prop_sponge_p_C"))return At(IgnoredClass);
    if(!wcscmp(name,L"ParticleSystem"))return At(ParticleClass);if(!wcscmp(name,L"SoundCue"))return At(SoundClass);
    CHECK(!wcscmp(name,L"SoundAttenuation"));return At(AttClass);}
size_t ForEachInstance(void* cls,InstanceFn fn,void* context){using namespace test;++enumerations;
    if(cls==At(ParticleClass))fn(context,At(Particle),Particle);else if(cls==At(SoundClass)){fn(context,At(Sound),Sound);fn(context,At(SplashSound),SplashSound);}
    else {CHECK(cls==At(AttClass));fn(context,At(Att),Att);}return 1;}
}
namespace ue_wrap::engine {
bool TryGetActorLocation(void* actor,FVector& out){CHECK(actor==test::At(test::Actor));out=test::expected.location;test::Run(test::onLocation);return test::locationOk;}
bool TryGetActorRotation(void* actor,FRotator& out){CHECK(actor==test::At(test::Actor));out=test::expected.rotation;return true;}
bool SetActorLocation(void* actor,const FVector& at){CHECK(actor==test::At(test::Actor) && at.X==test::expected.location.X);test::constructedAt=at;++test::locationWrites;return true;}
bool SetActorRotation(void* actor,const FRotator& value){CHECK(actor==test::At(test::Actor) && value.Yaw==test::expected.rotation.Yaw);++test::rotationWrites;return true;}
void* BeginDeferredSpawn(void* cls,const FVector& at,const FRotator& rotation){using namespace test;CHECK(cls==At(shot?IgnoredClass:PourClass) && at.X==100 && rotation.Pitch==expected.rotation.Pitch && rotation.Yaw==expected.rotation.Yaw);
    ++begins;constructedAt=at;objects[Actor].live=true;Run(onBegin);return beginOk?At(Actor):nullptr;}
bool FinishDeferredSpawn(void* actor,const FVector& at,const FRotator&){using namespace test;CHECK(actor==At(Actor) && at.X==100 && (shot || objects[Actor].soap==expected.soap));
    constructedAt.Z+=25;++finishes;Run(onFinish);return finishOk;}
void* GetStaticMeshComponent(void* actor){CHECK(actor==test::At(test::Actor));return test::meshOk?test::At(test::Mesh):nullptr;}
bool SetComponentLinearVelocity(void* component,float x,float y,float z){using namespace test;CHECK(component==At(Mesh) && x==expected.velocity.X && y==expected.velocity.Y && z==expected.velocity.Z);
    ++velocityWrites;Run(onSetVelocity);return velocityOk;}
bool DestroyActor(void* actor){CHECK(actor==test::At(test::Actor));++test::destroys;test::objects[test::Actor].live=false;return true;}
void* GetWorldContext(){return test::At(test::World);}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::building_material {bool IsBucket(void* object){return object==test::At(test::Actor) && !test::shot;}}
int main(){using namespace test;namespace W=ue_wrap::water_pour;
    for(int i=0;i<Count;++i)objects[i].index=i;bool current=true;const auto valid=[&]{return current;};
    W::Birth state{};CHECK(W::ReadBirth(At(Actor),state,valid) && state.velocity.X==5 && state.soap==12);
    fields=false;CHECK(!W::ReadBirth(At(Actor),state,valid));fields=true;CHECK(W::ReadBirth(At(Actor),state,valid));
    onVelocity=[]{++world;};CHECK(!W::ReadBirth(At(Actor),state,valid));
    onVelocity=[]{++objects[Actor].serial;};CHECK(!W::ReadBirth(At(Actor),state,valid));
    onVelocity=[&]{current=false;};CHECK(!W::ReadBirth(At(Actor),state,valid));current=true;
    missing=L"GetVelocity";CHECK(!W::ReadBirth(At(Actor),state,valid));missing.clear();callOk=false;CHECK(!W::ReadBirth(At(Actor),state,valid));callOk=true;
    Hit hit;W::Impact impact{};expected.location={110,210,310};
    CHECK(W::ReadImpact(At(Actor),At(Other),At(Collision),&hit,impact) && impact.location.X==100 && impact.normal.Z==1 && impact.audioLocation.X==110);
    ignoreOther=true;CHECK(!W::ReadImpact(At(Actor),At(Other),At(Collision),&hit,impact));ignoreOther=false;
    onLookup=[]{++world;};CHECK(!W::ReadImpact(At(Actor),nullptr,At(Collision),&hit,impact));
    onLocation=[]{++objects[Actor].serial;};CHECK(!W::ReadImpact(At(Actor),nullptr,At(Collision),&hit,impact));
    fields=false;CHECK(!W::ReadImpact(At(Actor),nullptr,At(Collision),&hit,impact));fields=true;
    CHECK(W::Surface(impact,valid) && particles==1 && std::fabs(rotation.Pitch-90)<.001f && rotation.Roll==0);
    CHECK(W::Sound(impact,valid) && sounds==1);auto walks=enumerations;
    CHECK(W::Surface(impact,valid) && W::Sound(impact,valid) && enumerations==walks);
    onEmitter=[&]{current=false;};CHECK(W::Surface(impact,valid));CHECK(!W::Sound(impact,valid));current=true;
    callOk=false;CHECK(W::Surface(impact,valid) && W::Sound(impact,valid));callOk=true; // dispatched one-shots are consumed
    missing=L"SpawnEmitterAtLocation";auto count=particles;CHECK(!W::Surface(impact,valid) && particles==count);missing.clear();
    params=false;++objects[SpawnEmitter].serial;CHECK(!W::Surface(impact,valid));params=true;++objects[SpawnEmitter].serial;CHECK(W::Surface(impact,valid));
    expected.location={100,200,300};classReady=false;CHECK(!W::Author(expected,valid).attempted && begins==0);classReady=true;
    fields=false;CHECK(!W::Author(expected,valid).attempted && begins==0);fields=true;
    auto result=W::Author(expected,valid);CHECK(result.attempted && result.actor==At(Actor) && finishes==1 && velocityWrites==1);
    CHECK(constructedAt.Z==expected.location.Z && locationWrites==1 && rotationWrites==1);
    beginOk=false;result=W::Author(expected,valid);CHECK(result.attempted && !result.actor);beginOk=true;
    finishOk=false;result=W::Author(expected,valid);CHECK(result.attempted && !result.actor && destroys==1);finishOk=true;
    velocityOk=false;result=W::Author(expected,valid);CHECK(result.attempted && !result.actor && destroys==2);velocityOk=true;
    onFinish=[&]{current=false;};count=velocityWrites;result=W::Author(expected,valid);CHECK(result.attempted && !result.actor && velocityWrites==count && destroys==3);current=true;
    onFinish=[]{++world;};result=W::Author(expected,valid);CHECK(result.attempted && !result.actor && destroys==3);
    onSetVelocity=[]{++objects[Actor].serial;};result=W::Author(expected,valid);CHECK(result.attempted && !result.actor && destroys==3);
    current=false;count=begins;CHECK(!W::Author(expected,valid).attempted && begins==count);current=true;
    expected.velocity.X=30000;CHECK(!W::Valid(expected) && !W::Author(expected,valid).attempted);expected.velocity.X=5;
    expected.soap=-1;CHECK(!W::Valid(expected));expected.soap=12;impact.normal={};CHECK(!W::Valid(impact));impact.normal={0,0,1};impact.location.X=NAN;CHECK(!W::Valid(impact));
    expected.kind=W::Kind::Washer;expected.soap=0;expected.rotation={12,34,5};shot=true;objects[Actor].power=.5f;
    CHECK(!W::ReadBirth(At(Actor),state,valid));objects[Actor].power=1;
    CHECK(W::ReadBirth(At(Actor),state,valid) && state.kind==W::Kind::Washer && state.soap==0 && state.power==1 && state.rotation.Yaw==34);
    objects[Actor].power=.5f;onFinish=[]{CHECK(objects[Actor].power==.5f);};result=W::Author(expected,valid);
    CHECK(result.attempted && result.actor==At(Actor) && objects[Actor].power==1 && lifeWrites==1 && constructedAt.Z==expected.location.Z);
    missing=L"SetLifeSpan";count=begins;CHECK(!W::Author(expected,valid).attempted && begins==count);missing.clear();
    const auto oldVelocities=velocityWrites;callOk=false;result=W::Author(expected,valid);CHECK(result.attempted && !result.actor && velocityWrites==oldVelocities);callOk=true;
    expected.soap=1;CHECK(!W::Valid(expected));expected.soap=0;expected.power=.5f;CHECK(!W::Valid(expected));expected.power=1;
    impact.location={100,200,300};impact.launch=true;soundPitch=1.5f;CHECK(W::Sound(impact,valid));
    shot=false;objects[Actor].live=true;Params input{};input.world=At(Actor);input.asset=At(Sound);input.location={100,200,300};input.volume=.25f;input.pitch=1;
    W::BucketCue cue{};CHECK(W::ReadBucketCue(At(Actor),At(PlaySound),reinterpret_cast<uint8_t*>(&input),false,cue,valid) && cue.kind==0 && cue.location.Z==300);
    soundVolume=.25f;soundPitch=1;soundAt=cue.location;CHECK(W::BucketSound(cue,valid));
    input.volume=1;input.pitch=1.5f;input.location.X=NAN;
    CHECK(W::ReadBucketCue(At(Actor),At(PlaySound),reinterpret_cast<uint8_t*>(&input),true,cue,valid) && cue.kind==1 && cue.location.X==100);
    soundVolume=1;soundPitch=1.5f;CHECK(W::BucketSound(cue,valid));
    input.asset=At(SplashSound);input.volume=.5f;input.pitch=1;
    CHECK(W::ReadBucketCue(At(Actor),At(PlaySound),reinterpret_cast<uint8_t*>(&input),true,cue,valid) && cue.kind==2);
    soundAsset=SplashSound;soundVolume=.5f;soundPitch=1;CHECK(W::BucketSound(cue,valid));
    cue.kind=99;input.pitch=1.1f;CHECK(!W::ReadBucketCue(At(Actor),At(PlaySound),reinterpret_cast<uint8_t*>(&input),true,cue,valid) && cue.kind==99);
    input.pitch=1;onLocation=[]{++world;};CHECK(!W::ReadBucketCue(At(Actor),At(PlaySound),reinterpret_cast<uint8_t*>(&input),true,cue,valid));
    input.world=At(Other);CHECK(!W::ReadBucketCue(At(Actor),At(PlaySound),reinterpret_cast<uint8_t*>(&input),true,cue,valid));
    input.world=At(Actor);input.asset=At(Sound);input.volume=.25f;input.pitch=1;input.location={100,200,300};
    CHECK(!W::ReadBucketCue(At(Actor),At(PlaySound),reinterpret_cast<uint8_t*>(&input),false,cue,[]{return false;}));
    parameterBytes=4;CHECK(!W::ReadBucketCue(At(Actor),At(PlaySound),reinterpret_cast<uint8_t*>(&input),false,cue,valid));parameterBytes=sizeof(Params);
    input.volume=1;input.pitch=1.5f;onLocation=[]{++objects[Sound].serial;};CHECK(!W::ReadBucketCue(At(Actor),At(PlaySound),reinterpret_cast<uint8_t*>(&input),true,cue,valid));
    puts("PASS water projectile wrapper/ParamFrame: exact bucket/washer classes, post-native speed/power/soap/rotation, final bounds-offset repair, native lifetime and distinct launch/impact sound arguments with terminal construction guards");
}
