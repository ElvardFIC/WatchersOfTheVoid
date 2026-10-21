#include "../../src/votv-coop/src/ue_wrap/actors/prop_fire.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace F=ue_wrap::prop_fire;
namespace test {
struct Obj {int id=0,serial=1;bool live=true;void* cls=nullptr;ue_wrap::reflection::FName name{};
 void* flame=nullptr;void* owner=nullptr;float fuel=0,speed=1;uint8_t bits=0;} objects[12];
Obj& actor=objects[1];Obj& flame=objects[2];Obj& base=objects[3];Obj& cls=objects[4];
uint32_t world=1;bool bind=true,dispatch=true,current=true;int calls=0,ignites=0,extinguishes=0,lastFunction=0;
bool complete=true;
bool flameField=true;
std::function<void()> callback;
std::function<void(const wchar_t*)> metadata;
void Reset(){++world;for(int i=0;i<12;++i){objects[i]={};objects[i].id=i;objects[i].serial=int(world);objects[i].cls=&cls;objects[i].name={i,0};}
 flame.owner=&actor;calls=ignites=extinguishes=0;bind=dispatch=current=complete=flameField=true;callback={};metadata={};}
}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<12?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return i>=0&&i<12?&test::objects[i]:nullptr;}
bool IsLiveByIndex(void* a,int32_t i){return IsLive(a)&&InternalIndexOf(a)==i;}
void* ClassOf(void* a){return a?static_cast<test::Obj*>(a)->cls:nullptr;}
std::wstring ClassNameOf(void* a){return a==&test::flame?L"comp_flammable_C":L"prop_C";}
const FName& NameOf(void* a){return static_cast<test::Obj*>(a)->name;}
bool NameStartsWith(const FName&,const wchar_t*){return false;}
int32_t FindPropertyOffset(void*,const wchar_t* n){using O=test::Obj;std::wstring name=n;
 if(test::metadata){auto callback=test::metadata;callback(n);}
 if(name==L"flameBase")return test::flameField?offsetof(O,flame):-1;if(name==L"owner")return offsetof(O,owner);
 if(name==L"fuel")return offsetof(O,fuel);if(name==L"fuelSpeed")return offsetof(O,speed);return -1;}
bool FindBoolProperty(void*,const wchar_t*,int32_t& off,uint8_t& mask){off=offsetof(test::Obj,bits);mask=4;return true;}
void* FindDispatchFunctionCached(void* c,const wchar_t* n){if(std::wstring(n)==L"extinguish")return &test::objects[6];if(std::wstring(n)==L"ignite")return c==&test::base?&test::objects[7]:&test::objects[5];return nullptr;}
int32_t FunctionFrameSize(void*){return 16;}
std::vector<ParamInfo> FunctionParams(void*){return test::bind?std::vector<ParamInfo>{{L"fuel",0,4,0}}:std::vector<ParamInfo>{};}
bool CallFunction(void* a,void* f,void* p){using namespace test;CHECK(a);++calls;lastFunction=InternalIndexOf(f);if(!dispatch)return false;
 if(f==&objects[6]){++extinguishes;flame.live=false;actor.flame=nullptr;}
 else {++ignites;flame.live=true;actor.flame=&flame;float v=0;std::memcpy(&v,p,4);flame.fuel=v;flame.bits|=4;}
 if(callback){auto cb=std::move(callback);callback={};cb();}
 return complete;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return (void*)(uintptr_t)test::world;}void* WorldOf(void*){return CurrentWorld();}}
namespace ue_wrap::prop {bool IsDescendantOfProp(void* a){return a==&test::actor && test::actor.cls==&test::cls;}}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* n){return std::wstring(n)==L"prop_C"?&test::base:std::wstring(n)==L"grime_gasoline_C"?&test::objects[8]:&test::cls;}
size_t ForEachInstance(void*,InstanceFn fn,void* ctx){fn(ctx,&test::flame,2);return 1;}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
using namespace test;
bool Apply(F::State s={true,30,1}){return F::Apply(&actor,s,[]{return current;});}
#ifndef PROP_FIRE_WRAPPER_FIXTURE_ONLY
int main(){
 Reset();F::State s{};CHECK(F::Read(&actor,s)&&!s.burning);CHECK(Apply());CHECK(ignites==1&&flame.fuel==30&&flame.speed==1&&lastFunction==7);
 CHECK(Apply({true,20,.5f})&&ignites==1&&flame.fuel==20&&flame.speed==.5f); // no repeated ignition/fuel extension
 std::vector<ue_wrap::CachedObjRef> rows;F::CollectBurning(rows);CHECK(rows.size()==1&&rows[0].Is(&actor));
 CHECK(Apply({false,0,1})&&extinguishes==1);CHECK(F::Read(&actor,s)&&!s.burning);CHECK(Apply({false,0,1})&&extinguishes==1);
 Reset();dispatch=false;CHECK(!Apply()&&calls==1);dispatch=true;CHECK(Apply());
 Reset();bind=false;CHECK(!Apply()&&!calls);
 for(int i=0;i<3;++i){Reset();callback=[i]{if(i==0)++world;if(i==1)++actor.serial;if(i==2)current=false;};CHECK(!Apply());}
 Reset();current=false;CHECK(!Apply()&&!calls);
 Reset();CHECK(!Apply({true,INFINITY,1})&&!Apply({true,0,1})&&!Apply({true,61,1})&&!Apply({true,30,-1})&&!calls);
 Reset();actor.flame=&flame;flame.bits=0x84;flame.fuel=25;CHECK(F::Read(&actor,s)&&s.burning);CHECK(Apply({true,15,1})&&flame.bits==0x84);
 flame.owner=nullptr;CHECK(!F::Read(&actor,s)&&!Apply());
 Reset();actor.flame=&flame;flame.bits=4;flame.fuel=-1;CHECK(F::Read(&actor,s)&&!s.burning&&s.fuel==0);
 Reset();CHECK(F::Ignite(&actor,40)&&ignites==1);CHECK(F::Extinguish(&actor)&&extinguishes==1);
 puts("PASS actual fire wrapper/ParamFrame: component creation, fuel refresh, extinguish, bounds, binding/dispatch failures and cancellation");
}
#endif
