#include "../../src/votv-coop/src/ue_wrap/devices/drink_spill.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
using namespace ue_wrap;
enum Id{Actor,Source,Mesh,BeerClass,WineClass,SourceClass,ProjectileClass,Projectile,ScaleFn,RotationFn,ApplyFn,SoundFn,Library,World,SoundClass,AttClass,Sound,Att,SoundPackage,AttPackage,BadSound,BadPackage,CoffeeClass,CupClass,Cup,GameplayClass,BeerGraph,CupGraph,FuelClass,Fuel,FuelPackage,DefaultAtt,DefaultAttPackage,GasClass,BottleClass,ScorchClass,Count};
struct Obj {int id=0,serial=1;bool live=true;void* cls=nullptr;void* decal=nullptr;void* grunge=nullptr;float process=100;int32_t type=3;
    FVector at{100,200,300},scale{.25f,.25f,.25f},velocity{1000,0,0};float size=1,lifeSpan=0;FRotator rot{12,34,56};} objects[Count];
struct Params {void* world=nullptr;void* sound=nullptr;FVector location{},scale{};FRotator rotation{};float volume=0,pitch=0;void* attenuation=nullptr;} ;
uint32_t world=1;bool loaded=true,callOK=true,paintOK=true;
bool foreignWorld=false,cdo=false;
bool collision=true,simulation=true;
uint64_t schema=1;bool decode=true,duplicate=false,wrongCallee=false;std::function<void()> onLayout;
int births=0,finishes=0,applies=0,sounds=0,destroys=0,paints=0;
std::wstring missing,missingParam;std::function<void()> onBegin,onFinish,onApply,onPaint,onSound,onScale;
FVector materialScale{};FRotator decalRotation{90,10,5};
void* At(Id id){return &objects[id];}
void Run(std::function<void()>& fn){auto f=std::move(fn);fn={};if(f)f();}
drink_spill::State Sample(){return {{100,200,300},{.25f,.25f,.25f},{12,34,56},{90,10,5},40,3,0};}
void Reset(){for(int i=0;i<Count;++i){objects[i]={};objects[i].id=i;}++world;++schema;decode=true;duplicate=wrongCallee=false;onLayout={};loaded=callOK=paintOK=true;
    foreignWorld=cdo=false;
    births=finishes=applies=sounds=destroys=paints=0;collision=simulation=true;missing.clear();missingParam.clear();onBegin=onFinish=onApply=onPaint=onSound=onScale={};
    objects[Actor].cls=At(BeerClass);objects[Actor].decal=At(Mesh);objects[Source].cls=At(SourceClass);objects[Source].grunge=At(BeerClass);
    objects[Projectile].cls=At(ProjectileClass);objects[Projectile].grunge=At(BeerClass);objects[Projectile].decal=At(Mesh);objects[Cup].cls=At(CupClass);materialScale={};decalRotation=Sample().decalRotation;}
const wchar_t* Name(int id){switch(id){case ScorchClass:return L"grime_explosionScorch_C";case GasClass:return L"grime_gasoline_C";case BeerClass:return L"grime_beer_C";case WineClass:return L"grime_wine_C";case CoffeeClass:return L"grime_coffee_C";case Sound:case BadSound:return L"water_splash_Cue";
 case Att:return L"att_small";case SoundPackage:return L"/Game/audio/effects/physics/water/water_splash_Cue";case AttPackage:return L"/Game/audio/misc/att_small";
 case Fuel:return L"fuel1";case FuelPackage:return L"/Game/audio/effects/fuel1";case DefaultAtt:return L"att_default";case DefaultAttPackage:return L"/Game/audio/misc/att_default";
 default:return L"wrong";}}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p && static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return &test::objects[i];}bool IsLiveByIndex(void* p,int32_t i){return i>=0 && i<test::Count && p==ObjectAt(i) && IsLive(p);}
void* ClassOf(void* p){return static_cast<test::Obj*>(p)->cls;}
const FName& NameOf(void* p){static FName name{};name.ComparisonIndex=static_cast<test::Obj*>(p)->id;return name;}
bool NameEquals(const FName& n,const wchar_t* text){return !wcscmp(test::Name(n.ComparisonIndex),text);}
bool NameStartsWith(const FName& n,const wchar_t* text){return test::cdo || !wcsncmp(test::Name(n.ComparisonIndex),text,wcslen(text));}
void* OuterOf(void* p){using namespace test;if(p==At(Sound))return At(SoundPackage);if(p==At(Att))return At(AttPackage);if(p==At(BadSound))return At(BadPackage);
    if(p==At(Fuel))return At(FuelPackage);if(p==At(DefaultAtt))return At(DefaultAttPackage);return nullptr;}
bool IsDescendantOfAny(void*,void* const*,size_t,int){return false;}
int32_t FindPropertyOffset(void*,const wchar_t* name){using namespace test;if(missing==name)return -1;
    if(!wcscmp(name,L"CallFunc_GetComponentBounds_Origin") || !wcscmp(name,L"CallFunc_K2_GetActorLocation_ReturnValue"))return offsetof(Params,location);
    if(!wcscmp(name,L"process"))return offsetof(Obj,process);if(!wcscmp(name,L"Type"))return offsetof(Obj,type);
    if(!wcscmp(name,L"Decal") || !wcscmp(name,L"Sphere"))return offsetof(Obj,decal);if(!wcscmp(name,L"velocity"))return offsetof(Obj,velocity);if(!wcscmp(name,L"size"))return offsetof(Obj,size);
    CHECK(!wcscmp(name,L"Grunge") || !wcscmp(name,L"grunge"));return offsetof(Obj,grunge);}
void* FindDispatchFunctionCached(void*,const wchar_t* name){using namespace test;if(missing==name)return nullptr;
    if(!wcscmp(name,L"ExecuteUbergraph_prop_snack_beer"))return At(BeerGraph);if(!wcscmp(name,L"ExecuteUbergraph_prop_food_coffeeCup"))return At(CupGraph);
    if(!wcscmp(name,L"GetActorScale3D"))return At(ScaleFn);if(!wcscmp(name,L"K2_GetComponentRotation"))return At(RotationFn);
    if(!wcscmp(name,L"applyMaterial"))return At(ApplyFn);CHECK(!wcscmp(name,L"PlaySoundAtLocation"));return At(SoundFn);}
void* FindClassDefaultObject(const wchar_t* name){if(!wcscmp(name,L"grime_explosionScorch_C"))return test::At(test::Actor);if(!wcscmp(name,L"prop_food_coffeeCup_C"))return test::At(test::Cup);if(!wcscmp(name,L"prop_snack_beer_C") || !wcscmp(name,L"prop_gasBottle_C"))return test::At(test::Source);CHECK(!wcscmp(name,L"GameplayStatics"));return test::At(test::Library);}
int32_t FunctionFrameSize(void* fn){return fn==test::At(test::ApplyFn)?0:sizeof(test::Params);}
std::vector<ParamInfo> FunctionParams(void* fn){using namespace test;std::vector<ParamInfo> result;
    const auto add=[&](const wchar_t* name,int offset){if(missingParam!=name){ParamInfo p{};p.name=name;p.offset=offset;result.push_back(p);}};
    if(fn==At(ScaleFn)){add(L"ReturnValue",offsetof(Params,scale));return result;}if(fn==At(RotationFn)){add(L"ReturnValue",offsetof(Params,rotation));return result;}
    CHECK(fn==At(SoundFn));add(L"WorldContextObject",offsetof(Params,world));add(L"Sound",offsetof(Params,sound));add(L"Location",offsetof(Params,location));
    add(L"Rotation",offsetof(Params,rotation));add(L"VolumeMultiplier",offsetof(Params,volume));add(L"PitchMultiplier",offsetof(Params,pitch));add(L"AttenuationSettings",offsetof(Params,attenuation));return result;}
bool CallFunction(void* object,void* fn,void* raw){using namespace test;
    if(fn==At(ApplyFn)){CHECK(object==At(Actor) && !raw);++applies;materialScale=objects[Actor].scale;decalRotation={-1,-1,-1};Run(onApply);return callOK;}
    auto& p=*static_cast<Params*>(raw);
    if(fn==At(ScaleFn)){p.scale=static_cast<Obj*>(object)->scale;Run(onScale);return callOK;}
    if(fn==At(RotationFn)){CHECK(object==At(Mesh));p.rotation=decalRotation;return callOK;}
    CHECK(fn==At(SoundFn) && object==At(Library) && p.world==At(World));
    CHECK((p.sound==At(Sound) && p.attenuation==At(Att) && p.pitch==1.25f) || (p.sound==At(Fuel) && p.attenuation==At(DefaultAtt) && p.pitch==1.f));
    CHECK(p.volume==.5f && p.location.X==100);++sounds;Run(onSound);return callOK;
}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return test::At(test::World);}void* WorldOf(void* p){return !test::foreignWorld && (p==test::At(test::Actor) || p==test::At(test::Source))?CurrentWorld():nullptr;}}
namespace ue_wrap::object_index {
uint64_t ClassUnlistVersion(){return test::schema;}
void* ClassByName(const wchar_t* n){using namespace test;if(!wcscmp(n,L"grime_beer_C"))return loaded?At(BeerClass):nullptr;if(!wcscmp(n,L"grime_wine_C"))return At(WineClass);
    if(!wcscmp(n,L"grime_explosionScorch_C"))return loaded?At(ScorchClass):nullptr;
    if(!wcscmp(n,L"grime_gasoline_C"))return At(GasClass);if(!wcscmp(n,L"prop_gasBottle_C"))return At(BottleClass);
    if(!wcscmp(n,L"prop_snack_beer_C"))return At(SourceClass);if(!wcscmp(n,L"grimeProjectile_C"))return At(ProjectileClass);
    if(!wcscmp(n,L"prop_food_coffeeCup_C"))return At(CupClass);if(!wcscmp(n,L"grime_coffee_C"))return At(CoffeeClass);
    if(!wcscmp(n,L"GameplayStatics"))return At(GameplayClass);if(!wcscmp(n,L"SoundWave"))return At(FuelClass);
    if(!wcscmp(n,L"SoundCue"))return At(SoundClass);CHECK(!wcscmp(n,L"SoundAttenuation"));return At(AttClass);}
size_t ForEachInstance(void* cls,InstanceFn fn,void* ctx){using namespace test;if(cls==At(SoundClass)){fn(ctx,At(BadSound),BadSound);fn(ctx,At(Sound),Sound);return 2;}
    if(cls==At(FuelClass)){fn(ctx,At(Fuel),Fuel);return 1;}CHECK(cls==At(AttClass));fn(ctx,At(Att),Att);fn(ctx,At(DefaultAtt),DefaultAtt);return 2;}
}
namespace ue_wrap::script_calls {
bool EnumerateCalls(void* graph,std::vector<CallSite>& out){using namespace test;CallSite call{};call.callee=At(wrongCallee?ApplyFn:SoundFn);call.end=777;call.locals[0]=graph;call.localCount=1;
    out={call};if(duplicate)out.push_back(call);Run(onLayout);return decode;}
bool SameCallee(const CallSite& call,void* fn){return call.callee==fn;}
bool CallHasLocal(const CallSite& call,const wchar_t* name){using namespace test;return (call.locals[0]==At(BeerGraph) && !wcscmp(name,L"CallFunc_GetComponentBounds_Origin")) ||
    (call.locals[0]==At(CupGraph) && !wcscmp(name,L"CallFunc_K2_GetActorLocation_ReturnValue"));}
}
namespace ue_wrap::engine {
void* BeginDeferredSpawn(void* cls,const FVector&,const FRotator&){++test::births;test::objects[test::Actor].cls=cls;
    if(cls==test::At(test::ProjectileClass))test::objects[test::Actor].lifeSpan=60;test::Run(test::onBegin);return test::At(test::Actor);}
bool FinishDeferredSpawn(void*,const FVector&,const FRotator&){++test::finishes;test::objects[test::Actor].scale={1,1,1};test::Run(test::onFinish);return test::callOK;}
bool SetActorScale3D(void* actor,const FVector& value){static_cast<test::Obj*>(actor)->scale=value;return true;}
bool SetActorEnableCollision(void*,bool enabled){test::collision=enabled;test::Run(test::onPaint);return true;}
bool SetComponentSimulatePhysics(void*,bool enabled){test::simulation=enabled;return true;}
bool SetActorLocation(void*,const FVector& value){test::objects[test::Actor].at=value;return true;}
bool SetActorRotation(void*,const FRotator& value){test::objects[test::Actor].rot=value;return true;}
bool SetComponentWorldRotation(void*,const FRotator& value){test::decalRotation=value;return true;}
bool TryGetActorLocation(void* actor,FVector& value){value=static_cast<test::Obj*>(actor)->at;return true;}
bool TryGetActorRotation(void* actor,FRotator& value){value=static_cast<test::Obj*>(actor)->rot;return true;}
bool DestroyActor(void*){++test::destroys;return true;}void* GetWorldContext(){return test::At(test::World);}
}
namespace ue_wrap::grime {
bool ReadProcess(void*,float& value){value=test::objects[test::Actor].process;return true;}
bool ReadType(void*,int32_t& value){value=test::objects[test::Actor].type;return true;}
bool WriteProcessAndApply(void*,float value,const std::function<bool()>& current){++test::paints;test::Run(test::onPaint);if(!current())return false;test::objects[test::Actor].process=value;return test::paintOK;}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
int main(){using namespace test;namespace N=ue_wrap::drink_spill;Reset();auto state=Sample();auto current=[] {return true;};N::State read{};uint8_t kind=9;
    CHECK(N::SourceKind(At(Source),kind) && kind==0 && N::IsProjectile(At(Projectile)) && N::DecalKind(At(Actor),kind));
    CHECK(N::Read(At(Actor),read,current) && read.scale.X==.25f && read.decalRotation.Pitch==90);
    callOK=false;CHECK(!N::Read(At(Actor),read,current));callOK=true;
    loaded=false;CHECK(!N::Author(state,true,current).attempted && !births);loaded=true;
    missing=L"Decal";CHECK(!N::Author(state,true,current).attempted);missing.clear();
    CHECK(N::Author(state,true,current).actor==At(Actor) && births==1 && finishes==1 && applies==1 && paints==1);
    CHECK(materialScale.X==.25f && decalRotation.Pitch==90 && objects[Actor].process==40 && objects[Actor].type==3);
    Reset();CHECK(N::Author(state,false,current).actor && objects[Actor].process==100);
    Reset();objects[Actor].at.X+=.5f;CHECK(N::ApplyExisting(At(Actor),state,current) && !births && objects[Actor].at.X==100 && objects[Actor].process==40);
    CHECK(materialScale.X==.25f && decalRotation.Pitch==90);objects[Source].grunge=At(WineClass);CHECK(N::SourceKind(At(Source),kind) && kind==1);
    for(int cause=0;cause<5;++cause){Reset();bool active=true;onApply=[&,cause]{if(cause==0)++world;if(cause==1)++objects[Actor].serial;
        if(cause==2)active=false;if(cause==3)++objects[Mesh].serial;if(cause==4)++objects[BeerClass].serial;};
        auto result=N::Author(state,true,[&]{return active;});CHECK(result.attempted && !result.actor && !paints);}
    Reset();paintOK=false;auto result=N::Author(state,true,current);CHECK(result.attempted && !result.actor && destroys==1);
    Reset();onScale=[]{++objects[Actor].serial;};CHECK(!N::Read(At(Actor),read,current));
    Reset();onScale=[]{++objects[BeerClass].serial;};CHECK(!N::Read(At(Actor),read,current));
    Reset();onScale=[]{++objects[ScaleFn].serial;};CHECK(!N::Read(At(Actor),read,current));
    Reset();onScale=[]{objects[Actor].cls=At(WineClass);};CHECK(!N::Read(At(Actor),read,current));
    Reset();objects[Actor].process=10;CHECK(N::ApplyExisting(At(Actor),state,current) && objects[Actor].process==40);
    Reset();CHECK(N::ImpactSound(state.location,current) && sounds==1);
    CHECK(!N::ImpactSound(state.location,[]{return false;}));
    state.scale.X=11;CHECK(!N::Valid(state));state=Sample();state.location.X=NAN;CHECK(!N::Valid(state));
    Reset();CHECK(N::SourceClassKind(L"prop_snack_beer_C",kind) && kind==0 && N::ProjectileKind(At(Projectile),kind));
    N::Launch launch{},captured{};launch.location={100,200,300};launch.velocity={1234,200,-50};
    CHECK(!N::ReadLaunch(nullptr,captured,current));CHECK(N::ReadLaunch(At(Projectile),captured,current) && captured.velocity.X==1000 && captured.scale.X==.25f);
    onFinish=[&]{CHECK(objects[Actor].velocity.X==1234 && objects[Actor].size==1 && objects[Actor].grunge==At(BeerClass));};
    CHECK(N::AuthorLaunch(launch,current).actor==At(Actor) && finishes==1 && objects[Actor].scale.X==.25f && objects[Actor].lifeSpan==60);
    Reset();missing=L"velocity";CHECK(!N::AuthorLaunch(launch,current).attempted && !births);missing.clear();
    for(int cause=0;cause<4;++cause){Reset();bool active=true;onFinish=[&,cause]{if(cause==0)++world;if(cause==1)++objects[Actor].serial;if(cause==2)active=false;if(cause==3)++objects[BeerClass].serial;};
        auto failed=N::AuthorLaunch(launch,[&]{return active;});CHECK(failed.attempted && !failed.actor);}
    Reset();onScale=[]{++objects[ScaleFn].serial;};CHECK(!N::ReadLaunch(At(Projectile),captured,current));
    launch.velocity.X=NAN;CHECK(!N::Valid(launch));launch.velocity={21000,0,0};CHECK(!N::Valid(launch));
    Reset();CHECK(N::FreezeLaunch(At(Projectile),current) && !collision && !simulation && objects[Projectile].velocity.X==1000);
    Reset();onPaint=[]{++objects[Mesh].serial;};CHECK(!N::FreezeLaunch(At(Projectile),current) && simulation);
    Reset();onPaint=[]{++world;};CHECK(!N::FreezeLaunch(At(Projectile),current) && simulation);
    Reset();missing=L"Sphere";CHECK(!N::FreezeLaunch(At(Projectile),current) && collision && simulation);
    Reset();objects[Cup].cls=At(CupClass);CHECK(N::SourceKind(At(Cup),kind) && kind==2 && N::SourceClassKind(L"prop_food_coffeeCup_C",kind) && kind==2);
    launch={};launch.kind=2;launch.size=5;launch.scale={.75f,.75f,.75f};launch.velocity={1000,0,0};
    CHECK(N::AuthorLaunch(launch,current).actor && objects[Actor].grunge==At(CoffeeClass) && objects[Actor].size==5 && objects[Actor].scale.X==.75f);
    Reset();state=Sample();state.kind=2;state.scale={.75f,.75f,.75f};CHECK(N::Author(state,true,current).actor && objects[Actor].cls==At(CoffeeClass) && materialScale.X==.75f);
    Reset();CHECK(N::PourReady(At(Source)) && N::PourReady(At(Cup)));Params locals{};locals.location={100,200,300};FVector point{};
    ufunction_hook::CallerFrame frame{At(BeerGraph),reinterpret_cast<uint8_t*>(&locals),777};
    CHECK(N::ReadPourCue(At(Source),frame,kind,point,current) && kind==0 && point.X==100);
    --frame.codeOffset;CHECK(!N::ReadPourCue(At(Source),frame,kind,point,current));++frame.codeOffset;
    frame.function=At(CupGraph);CHECK(!N::ReadPourCue(At(Source),frame,kind,point,current));CHECK(N::ReadPourCue(At(Cup),frame,kind,point,current) && kind==2);
    frame.locals=nullptr;CHECK(!N::ReadPourCue(At(Cup),frame,kind,point,current));frame.locals=reinterpret_cast<uint8_t*>(&locals);
    locals.location.X=NAN;CHECK(!N::ReadPourCue(At(Cup),frame,kind,point,current));locals.location.X=100;
    for(int cause=0;cause<5;++cause){Reset();frame.function=At(BeerGraph);onLayout=[cause]{if(cause==0)++world;if(cause==1)++objects[Source].serial;
        if(cause==2)++objects[BeerGraph].serial;if(cause==3)++objects[SourceClass].serial;if(cause==4)++schema;};CHECK(!N::ReadPourCue(At(Source),frame,kind,point,current));}
    Reset();duplicate=true;CHECK(!N::PourReady(At(Source)));Reset();wrongCallee=true;CHECK(!N::PourReady(At(Source)));Reset();decode=false;CHECK(!N::PourReady(At(Source)));
    Reset();CHECK(N::PourSound(0,locals.location,current) && sounds==1);CHECK(N::PourSound(2,locals.location,current) && sounds==2);CHECK(!N::PourSound(3,locals.location,current));
    Reset();objects[Source].grunge=At(CoffeeClass);CHECK(!N::SourceKind(At(Source),kind));
    Reset();objects[Source].cls=At(BottleClass);CHECK(N::BreakSourceKind(At(Source),kind) && kind==3 && !N::SourceKind(At(Source),kind));
    CHECK(N::SourceClassKind(L"prop_gasBottle_C",kind) && kind==3);
    objects[Source].cls=At(GasClass);CHECK(!N::BreakSourceKind(At(Source),kind));
    objects[Actor].cls=At(GasClass);CHECK(N::DecalKind(At(Actor),kind) && kind==3);
    objects[Projectile].grunge=At(GasClass);CHECK(N::ProjectileKind(At(Projectile),kind) && kind==3);
    state=Sample();state.kind=3;CHECK(N::Author(state,true,current).actor==At(Actor));
    puts("PASS drink-spill wrapper/actual ParamFrame: native launch fields before BeginPlay, final scale/lifespan, exact class/assets, transform/material capture, defaults/canonical state, terminal construction and serial/world/current/mesh fences");
    return 0;
}
