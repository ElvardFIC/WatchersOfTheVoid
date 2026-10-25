#include "../../src/votv-coop/src/ue_wrap/actors/radial_impulse.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Array { void* data=nullptr;int32_t num=0,max=0; };
struct Obj {int id=0,serial=1;bool live=true;float radius=713,strength=5000;Array types; } objects[10];
auto& component=objects[1];auto& first=objects[2];auto& second=objects[3];auto& library=objects[4];
uint8_t types[2]{3,7};uint32_t world=1;bool current=true,queryOk=true,assets=true,malformed=false,nullOutput=false,foreign=false;
int queries=0,frees=0;std::wstring missing;std::function<void()> locationCallback,queryCallback;
struct Params {void* context=nullptr;ue_wrap::FVector center;float radius=0;Array types;void* cls=nullptr;Array ignored,out;bool result=false;};
void Invoke(std::function<void()>& fn){auto cb=std::move(fn);fn={};if(cb)cb();}
void Reset(){++world;for(int i=0;i<10;++i)objects[i]={i,int(world)};
 types[0]=3;types[1]=7;component.types={types,2,2};current=queryOk=assets=true;malformed=nullOutput=foreign=false;
 queries=frees=0;missing.clear();locationCallback=queryCallback={};}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
void* ObjectAt(int32_t i){return i>=0&&i<10?&test::objects[i]:nullptr;}
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t i){return p&&ObjectAt(i)==p&&IsLive(p);}
int32_t SlotSerial(int32_t i){return i>=0&&i<10?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void* p){return p;}
int32_t FindPropertyOffset(void*,const wchar_t* field){using O=test::Obj;if(test::missing==field)return -1;
 if(!wcscmp(field,L"Radius"))return offsetof(O,radius);if(!wcscmp(field,L"ImpulseStrength"))return offsetof(O,strength);
 CHECK(!wcscmp(field,L"ObjectTypesToAffect"));return offsetof(O,types);}
void* FindDispatchFunctionCached(void*,const wchar_t* fn){CHECK(!wcscmp(fn,L"K2_GetComponentLocation"));return test::assets?&test::objects[8]:nullptr;}
void* FindClassDefaultObject(const wchar_t* cls){CHECK(!wcscmp(cls,L"KismetSystemLibrary"));return &test::library;}
int32_t FunctionFrameSize(void* fn){return fn==&test::objects[8]?sizeof(FVector):sizeof(test::Params);}
std::vector<ParamInfo> FunctionParams(void* fn){using P=test::Params;
 if(fn==&test::objects[8])return {{L"ReturnValue",0,sizeof(FVector),0}};
 return {{L"WorldContextObject",offsetof(P,context),8,0},{L"SpherePos",offsetof(P,center),12,0},
 {L"SphereRadius",offsetof(P,radius),4,0},{L"ObjectTypes",offsetof(P,types),16,0},
 {L"ActorClassFilter",offsetof(P,cls),8,0},{L"ActorsToIgnore",offsetof(P,ignored),16,0},
 {L"OutActors",offsetof(P,out),16,0},{L"ReturnValue",offsetof(P,result),1,0}};
}
bool CallFunction(void* actor,void* fn,void* raw){using namespace test;
 if(fn==&objects[8]){CHECK(actor==&component);*static_cast<FVector*>(raw)={100,200,300};Invoke(locationCallback);return true;}
 CHECK(actor==&library&&fn==&objects[9]);++queries;auto& p=*static_cast<Params*>(raw);
 CHECK(p.context==&component&&p.center.X==100&&p.center.Y==200&&p.center.Z==300&&p.radius==713&&!p.cls);
 CHECK(p.types.num==2&&p.types.max==2&&p.types.data!=component.types.data&&!p.ignored.data&&!p.ignored.num);
 auto* types=static_cast<uint8_t*>(p.types.data);CHECK(types[0]==3&&types[1]==7);
 if(nullOutput)p.out={nullptr,1,1};else{
  auto** out=static_cast<void**>(std::malloc(2*sizeof(void*)));out[0]=&first;out[1]=&second;
  p.out={out,malformed?3:2,2};
 }
 Invoke(queryCallback);return queryOk;
}
void EngineFree(void* p){++test::frees;std::free(p);}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}void* CurrentWorld(){return (void*)(uintptr_t)test::world;}
void* WorldOf(void* p){if(p==&test::second&&test::foreign)return (void*)999;return p==&test::component||p==&test::first||p==&test::second?CurrentWorld():nullptr;}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* id,const char* feature,const wchar_t* cls,const wchar_t* fn,Mode mode)
 : ident_(id),feature_(feature),cls_(cls),fn_(fn),mode_(mode),next_(nullptr){}
Resolved Resolve(const Function& row){CHECK(!wcscmp(row.Name(),L"SphereOverlapActors"));return test::assets?Resolved(&test::objects[9],&row):Resolved{};}
}
bool Collect(std::vector<ue_wrap::CachedObjRef>& found){return ue_wrap::radial_impulse::CollectActors(&test::component,found,[]{return test::current;});}
int main(){using namespace test;std::vector<ue_wrap::CachedObjRef> found;
 Reset();CHECK(Collect(found)&&queries==1&&frees==1&&found.size()==2&&found[0].Is(&first)&&found[1].Is(&second));found.clear();
 Reset();locationCallback=[]{types[0]=12;};CHECK(Collect(found)&&frees==1&&found.size()==2);found.clear();
 for(int i=0;i<9;++i){Reset();if(i==0)component.radius=NAN;if(i==1)component.radius=0;if(i==2)component.strength=INFINITY;
  if(i==3)component.types.num=33;if(i==4)component.types.data=nullptr;if(i==5)component.types.max=1;
  if(i==6)types[0]=32;if(i==7)missing=L"ObjectTypesToAffect";if(i==8)assets=false;
  CHECK(!Collect(found)&&!queries&&!frees&&found.empty());}
 Reset();component.strength=0;CHECK(Collect(found)&&!queries&&found.empty());
 Reset();component.types.num=0;CHECK(Collect(found)&&!queries&&found.empty());
 Reset();queryOk=false;CHECK(!Collect(found)&&frees==1&&found.empty());
 Reset();malformed=true;CHECK(!Collect(found)&&frees==1&&found.empty());
 Reset();nullOutput=true;CHECK(!Collect(found)&&!frees&&found.empty());
 for(int i=0;i<4;++i){Reset();queryCallback=[i]{if(i==0)current=false;if(i==1)++world;if(i==2)++component.serial;if(i==3)component.live=false;};
  CHECK(!Collect(found)&&frees==1&&found.empty());}
 Reset();locationCallback=[]{++component.serial;};CHECK(!Collect(found)&&!queries&&!frees&&found.empty());
 Reset();foreign=true;CHECK(Collect(found)&&frees==1&&found.size()==1&&found[0].Is(&first));
 puts("PASS production radial wrapper/ParamFrame: native sphere/filter, copied inputs, engine-owned outputs, malformed refusal and native callback lifetime");
}

// Reflection names used by ParamFrame missing-field diagnostics.
namespace ue_wrap::reflection {
std::wstring ToString(const FName&){return L"fixture_function";}
const FName& NameOf(void*){static const FName name{};return name;}
}
