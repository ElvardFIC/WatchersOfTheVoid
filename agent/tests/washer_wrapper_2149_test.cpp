#include "../../src/votv-coop/src/ue_wrap/actors/pressure_washer.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int id=0,serial=1;bool live=true;void* cls=nullptr;ue_wrap::reflection::FName name{};float level=3;uint8_t fire=0x81;} objs[12];
bool fields=true,callOk=true,destroyOnTick=false,particleOk=true;int emissions=0,sounds=0;
uint32_t world=1;std::function<void()> callback,onTick;
void Run(std::function<void()>& fn){auto f=std::move(fn);fn={};if(f)f();}
struct Params {void* world;void* effect;ue_wrap::FVector location;ue_wrap::FRotator rotation;ue_wrap::FVector scale;bool destroy;uint8_t pooling;bool activate;void* result;};
}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
void* ClassOf(void* a){return static_cast<test::Obj*>(a)->cls;}
void* SuperStructOf(void*){return nullptr;}
const FName& NameOf(void* a){return static_cast<test::Obj*>(a)->name;}
bool NameEquals(const FName& n,const wchar_t* s){
 const wchar_t* names[]{L"",L"prop_waterShooter_C",L"",L"",L"eff_waterThrower",L"wade_Cue",L"",L"att_default",
 L"/Game/particles/eff_waterThrower",L"/Game/audio/effects/physics/footsteps/wade_Cue",L"/Game/audio/misc/att_default",L"eff_waterThrower"};
 return n.ComparisonIndex<12 && !wcscmp(names[n.ComparisonIndex],s);}
void* OuterOf(void* p){if(p==&test::objs[4])return &test::objs[8];if(p==&test::objs[5])return &test::objs[9];if(p==&test::objs[7])return &test::objs[10];return nullptr;}
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
bool IsLiveByIndex(void* a,int32_t i){return i>=0&&i<12&&a==&test::objs[i]&&IsLive(a);}
int32_t SlotSerial(int32_t i){return test::objs[i].serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return &test::objs[i];}
int32_t FindPropertyOffset(void*,const wchar_t* n){return test::fields&&!wcscmp(n,L"level")?offsetof(test::Obj,level):-1;}
bool FindBoolProperty(void*,const wchar_t* n,int32_t& off,uint8_t& mask){off=offsetof(test::Obj,fire);mask=1;return test::fields&&!wcscmp(n,L"fire");}
void* FindClass(const wchar_t*){return &test::objs[1];}
void* FindClassDefaultObject(const wchar_t*){return &test::objs[3];}
void* FindDispatchFunctionCached(void*,const wchar_t* n){CHECK(!wcscmp(n,L"SpawnEmitterAtLocation"));return &test::objs[2];}
void* FindFunction(void*,const wchar_t*){std::abort();}
void* FindObject(const wchar_t*,const wchar_t*){std::abort();}
int32_t FunctionFrameSize(void*){return sizeof(test::Params);}
std::vector<ParamInfo> FunctionParams(void*){
 std::vector<ParamInfo> out;
 auto add=[&](const wchar_t* n,int off){ParamInfo p{};p.name=n;p.offset=off;out.push_back(p);};
 #define ADD(n,field) add(L##n,offsetof(test::Params,field))
 ADD("WorldContextObject",world);ADD("EmitterTemplate",effect);ADD("Location",location);ADD("Rotation",rotation);
 ADD("Scale",scale);ADD("bAutoDestroy",destroy);ADD("PoolingMethod",pooling);ADD("bAutoActivate",activate);ADD("ReturnValue",result);
 #undef ADD
 return out;
}
bool CallFunction(void* object,void* fn,void* raw){
 CHECK(object==&test::objs[3]&&fn==&test::objs[2]);auto& p=*static_cast<test::Params*>(raw);
 CHECK(p.world==&test::objs[0]&&p.effect==&test::objs[4]);
 CHECK(p.location.X==1&&p.rotation.Yaw==34&&p.scale.X==1&&p.scale.Y==1&&p.scale.Z==1);
 CHECK(p.destroy&&p.activate&&p.pooling==0);++test::emissions;
 p.result=test::particleOk?&test::objs[6]:nullptr;test::Run(test::callback);return test::callOk;
}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* name){CHECK(!wcscmp(name,L"ParticleSystem") || !wcscmp(name,L"SoundCue") || !wcscmp(name,L"SoundAttenuation"));return &test::objs[11];}
size_t ForEachInstance(void*,InstanceFn fn,void* ctx){for(int i:{11,4,5,7})fn(ctx,&test::objs[i],i);return 4;}
}
namespace ue_wrap::engine {
bool SetActorTickEnabled(void*,bool enabled){CHECK(!enabled);if(test::destroyOnTick)test::objs[0].live=false;test::Run(test::onTick);return true;}
void PlaySoundAtLocation(void*,void*,const FVector& p,void* att,float vol,float pitch){CHECK(p.X==1&&att&&vol==1&&pitch==1.5f);++test::sounds;}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
int main(){using namespace test;namespace W=ue_wrap::pressure_washer;
 for(int i=0;i<12;++i){objs[i].id=i;objs[i].name.ComparisonIndex=i;}objs[0].cls=&objs[1];objs[1].name.ComparisonIndex=1;
 bool firing=false;float level=0;
 fields=false;CHECK(!W::Read(&objs[0],firing,level));fields=true;
 CHECK(W::Read(&objs[0],firing,level)&&firing&&level==3); // same live class must retry metadata
 CHECK(W::SetRemoteLevel(&objs[0],0)&&objs[0].level==0&&objs[0].fire==0x80); // preserve adjacent bits
 CHECK(!W::SetRemoteLevel(&objs[0],11));CHECK(!W::SetRemoteLevel(&objs[0],std::numeric_limits<float>::quiet_NaN()));
 CHECK(W::EmitRemote(&objs[0],{1,2,3},{12,34,0})&&emissions==1&&sounds==1);
 particleOk=false;CHECK(!W::EmitRemote(&objs[0],{1,2,3},{12,34,0})&&sounds==1);particleOk=true;
 callOk=false;CHECK(!W::EmitRemote(&objs[0],{1,2,3},{12,34,0})&&sounds==1);callOk=true;
 CHECK(!W::EmitRemote(&objs[0],{std::numeric_limits<float>::quiet_NaN(),0,0},{})&&sounds==1);
 destroyOnTick=true;CHECK(!W::SetRemoteLevel(&objs[0],4)&&objs[0].level==0);objs[0].live=true;destroyOnTick=false;
 ++objs[1].serial;fields=false;CHECK(!W::Read(&objs[0],firing,level));
 ++objs[1].serial;fields=true;CHECK(W::Read(&objs[0],firing,level)); // class recreation refreshes metadata
 bool current=true;const auto valid=[&]{return current;};auto soundsBefore=sounds;
 callback=[&]{current=false;};CHECK(!W::EmitRemote(&objs[0],{1,2,3},{12,34,0},valid) && sounds==soundsBefore);current=true;
 callback=[]{++world;};CHECK(!W::EmitRemote(&objs[0],{1,2,3},{12,34,0}) && sounds==soundsBefore);
 callback=[]{++objs[0].serial;};CHECK(!W::EmitRemote(&objs[0],{1,2,3},{12,34,0}) && sounds==soundsBefore);
 onTick=[&]{current=false;};CHECK(!W::SetRemoteLevel(&objs[0],4,valid) && objs[0].level==0);current=true;
 onTick=[]{++world;};CHECK(!W::SetRemoteLevel(&objs[0],4) && objs[0].level==0);
 CHECK(W::EmitRemote(&objs[0],{1,2,3},{12,34,0}) && sounds==soundsBefore+1);
 objs[1].name.ComparisonIndex=2;CHECK(!W::EmitRemote(&objs[0],{1,2,3},{12,34,0}));
 puts("PASS real washer wrapper and ParamFrame: display-only calls, charge/flags, failed emission, lifetime and class reload");
}

// Reflection names used by ParamFrame missing-field diagnostics.
namespace ue_wrap::reflection {
std::wstring ToString(const FName&){return L"fixture_function";}
}
