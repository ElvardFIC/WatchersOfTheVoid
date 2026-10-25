#ifndef HIT_RESULT_SOURCE
#define HIT_RESULT_SOURCE "../../src/votv-coop/src/ue_wrap/engine/hit_result.cpp"
#endif
#include HIT_RESULT_SOURCE
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
const char* hitCase="setup";
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %s line %d: %s\n",hitCase,__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int id=0,serial=1;bool live=true;void* cls=nullptr;std::array<uint8_t,80> bytes{};} obj[20];
struct Hit {int32_t actor,actorSerial,component,componentSerial;ue_wrap::FVector point,normal;uint8_t blocking;uint8_t pad[3];};
static_assert(sizeof(Hit)==44);
bool source=true,fields=true,nativeOK=true,wrongWeak=false,blocking=true;
bool wrongStruct=false;int intoSize=sizeof(Hit),fieldOffset=offsetof(Obj,bytes),calls=0;
uint32_t world=1;
std::wstring missing;
std::function<void()> nativeCallback;
std::function<void(const char*)> metadataCallback;
void Meta(const char* stage){auto fn=metadataCallback;if(fn)fn(stage);}
void Callback(){auto fn=std::move(nativeCallback);nativeCallback={};if(fn)fn();}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p && static_cast<test::Obj*>(p)->live;}
void* ClassOf(void* p){return static_cast<test::Obj*>(p)->cls;}
void* OuterOf(void* p){return p==&test::obj[2]?&test::obj[3]:&test::obj[6];}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0 && i<20 && p==&test::obj[i] && IsLive(p);}
int32_t SlotSerial(int32_t i){return i>=0 && i<20?test::obj[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return i>=0 && i<20?&test::obj[i]:nullptr;}
void* ResolveWeakObject(int32_t i,int32_t serial){auto* p=ObjectAt(i);return IsLiveByIndex(p,i) && SlotSerial(i)==serial?p:nullptr;}
void* FindClassDefaultObject(const wchar_t*){return &test::obj[1];}
void* FindClass(const wchar_t*){return &test::obj[3];}
void* FindFunction(void*,const wchar_t*){return &test::obj[2];}
void* PropertyInnerStruct(void* owner,const wchar_t*){test::Meta("inner");return test::wrongStruct && owner!=&test::obj[2]?&test::obj[13]:&test::obj[4];}
int32_t StructSize(void* cls){test::Meta("struct-size");return cls==&test::obj[4] || cls==&test::obj[13]?sizeof(test::Hit):sizeof(test::Obj);}
int32_t FindPropertyOffset(void* cls,const wchar_t* name){test::Meta("offset");
 if(cls!=&test::obj[4])return test::fieldOffset;
 if(!test::fields)return -1;
 if(!wcscmp(name,L"Actor"))return offsetof(test::Hit,actor);
 if(!wcscmp(name,L"Component"))return offsetof(test::Hit,component);
 if(!wcscmp(name,L"ImpactPoint"))return offsetof(test::Hit,point);
 if(!wcscmp(name,L"ImpactNormal"))return offsetof(test::Hit,normal);return -1;
}
bool FindBoolProperty(void*,const wchar_t*,int32_t& off,uint8_t& mask){test::Meta("bool");off=offsetof(test::Hit,blocking);mask=1;return test::fields;}
int32_t FindParamSize(void* fn,const wchar_t*){test::Meta("param-size");return fn==&test::obj[2]?sizeof(test::Hit):test::intoSize;}
int32_t FunctionFrameSize(void* fn){test::Meta(fn==&test::obj[2]?"make-frame":"target-frame");return fn==&test::obj[2]?48+sizeof(test::Hit):sizeof(test::Hit);}
std::vector<ParamInfo> FunctionParams(void* fn){test::Meta(fn==&test::obj[2]?"make-params":"target-params");
 std::vector<ParamInfo> rows;auto add=[&](const wchar_t* name,int off){if(test::missing!=name){ParamInfo p{};p.name=name;p.offset=off;rows.push_back(p);}};
 if(fn==&test::obj[2]){add(L"bBlockingHit",0);add(L"HitActor",8);add(L"HitComponent",16);add(L"Location",24);add(L"ImpactPoint",36);add(L"ReturnValue",48);}
 else add(L"hit",0);return rows;
}
bool CallFunction(void* object,void* fn,void* raw){using namespace test;CHECK(object==&obj[1] && fn==&obj[2]);++calls;
 if(!nativeOK)return false;
 auto* bytes=static_cast<uint8_t*>(raw);void* actor=nullptr;void* component=nullptr;
 std::memcpy(&actor,bytes+8,8);std::memcpy(&component,bytes+16,8);CHECK(actor && component);
 Hit result{};result.actor=InternalIndexOf(actor);result.actorSerial=SlotSerial(result.actor);
 result.component=wrongWeak?14:InternalIndexOf(component);result.componentSerial=SlotSerial(result.component);
 std::memcpy(&result.point,bytes+36,12);result.normal={0,0,1};result.blocking=blocking && bytes[0];
 std::memcpy(bytes+48,&result,sizeof(result));Callback();return true;
}
}
namespace ue_wrap::world_identity {void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}uint32_t Generation(){return test::world;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace H=ue_wrap::hit_result;
void ResetHit(){using namespace test;source=fields=nativeOK=blocking=true;wrongWeak=wrongStruct=false;intoSize=sizeof(Hit);fieldOffset=offsetof(Obj,bytes);calls=0;missing.clear();nativeCallback={};metadataCallback={};++world;
 for(int i=0;i<20;++i){obj[i].id=i;++obj[i].serial;obj[i].live=true;obj[i].cls=&obj[i];obj[i].bytes.fill(0xcc);}
 obj[7].cls=&obj[8];obj[9].cls=&obj[10];obj[11].cls=&obj[12];
}
ue_wrap::ParamFrame Destination(){return ue_wrap::ParamFrame(&test::obj[5]);}
void Fill(ue_wrap::ParamFrame& frame){std::array<uint8_t,sizeof(test::Hit)> initial;initial.fill(0xcc);CHECK(frame.SetRaw(L"hit",initial.data(),initial.size()));}
bool Unchanged(ue_wrap::ParamFrame& frame){std::array<uint8_t,sizeof(test::Hit)> bytes{};CHECK(frame.GetRaw(L"hit",bytes.data(),bytes.size()));for(auto b:bytes)if(b!=0xcc)return false;return true;}
bool Write(ue_wrap::ParamFrame& f){return H::WriteWhile(f,L"hit",&test::obj[7],&test::obj[9],{1,2,3},[]{return test::source;});}
bool Field(){return H::WriteFieldWhile(&test::obj[11],L"hit",&test::obj[7],&test::obj[9],{1,2,3},[]{return test::source;});}
template<class Fn>void Case(const char* name,const char* selected,Fn fn,int& count){if(selected && std::strcmp(selected,name))return;hitCase=name;fn();++count;}
int main(int argc,char** argv){using namespace test;const char* selected=argc>1?argv[1]:nullptr;int count=0;
 Case("native-hit",selected,[&]{ResetHit();auto f=Destination();CHECK(Write(f) && calls==1);Hit h{};CHECK(f.GetRaw(L"hit",&h,sizeof(h)));
  CHECK(h.actor==7 && h.component==9 && h.point.X==1 && h.point.Y==2 && h.point.Z==3 && h.blocking);
  CHECK(Field());std::memcpy(&h,obj[11].bytes.data(),sizeof(h));CHECK(h.actor==7 && h.component==9);
 },count);
 Case("required-bindings",selected,[&]{for(const auto* name:{L"bBlockingHit",L"HitActor",L"HitComponent",L"Location",L"ImpactPoint",L"ReturnValue"}){
  ResetHit();missing=name;auto f=Destination();Fill(f);CHECK(!Write(f) && Unchanged(f));
  if(wcscmp(name,L"ReturnValue"))CHECK(calls==0);}
 },count);
 Case("schema-and-result",selected,[&]{for(int cause=0;cause<5;++cause){ResetHit();auto f=Destination();Fill(f);
  if(cause==0)wrongStruct=true;if(cause==1)intoSize=sizeof(Hit)+1;if(cause==2)wrongWeak=true;if(cause==3)blocking=false;if(cause==4)nativeOK=false;
  CHECK(!Write(f) && Unchanged(f));}
 },count);
 Case("native-retirement",selected,[&]{for(int cause=0;cause<11;++cause){ResetHit();auto f=Destination();Fill(f);nativeCallback=[&,cause]{
  if(cause==0)source=false;if(cause==1)++world;if(cause==2)++obj[7].serial;if(cause==3)obj[7].cls=&obj[14];if(cause==4)++obj[9].serial;
  if(cause==5)++obj[2].serial;if(cause==6)++obj[3].serial;if(cause==7)++obj[4].serial;if(cause==8)++obj[5].serial;
  if(cause==9)++obj[6].serial;if(cause==10)++obj[1].serial;};CHECK(!Write(f) && Unchanged(f));}
 },count);
 Case("metadata-retirement",selected,[&]{for(const auto* stage:{"param-size","inner","make-frame","make-params"}){ResetHit();auto f=Destination();Fill(f);
  metadataCallback=[stage](const char* at){if(!std::strcmp(at,stage)){metadataCallback={};source=false;}};
  CHECK(!Write(f) && Unchanged(f) && calls==0);}
 },count);
 Case("field-retirement",selected,[&]{for(int cause=0;cause<8;++cause){ResetHit();const auto before=obj[11].bytes;nativeCallback=[&,cause]{
  if(cause==0)source=false;if(cause==1)++obj[11].serial;if(cause==2)obj[11].cls=&obj[14];if(cause==3)fieldOffset+=1;
  if(cause==4)wrongStruct=true;if(cause==5)++world;if(cause==6)++obj[9].serial;if(cause==7)++obj[4].serial;};
  CHECK(!Field() && obj[11].bytes==before);}
 },count);
 Case("field-shape",selected,[&]{for(int cause=0;cause<4;++cause){ResetHit();if(cause==0)wrongStruct=true;if(cause==1)fieldOffset=-1;
  if(cause==2)fieldOffset=sizeof(Obj);if(cause==3)fieldOffset=std::numeric_limits<int>::max();const auto before=obj[11].bytes;
  CHECK(!Field() && obj[11].bytes==before && calls==0);}
 },count);
 Case("layout-recovery",selected,[&]{ResetHit();auto f=Destination();Fill(f);fields=false;CHECK(!Write(f) && Unchanged(f));fields=true;
  CHECK(Write(f));++obj[4].serial;fields=false;CHECK(!Write(f));fields=true;CHECK(Write(f));
 },count);
 Case("owned-location",selected,[&]{ResetHit();auto f=Destination();ue_wrap::FVector where{4,5,6};metadataCallback=[&](const char* stage){if(!std::strcmp(stage,"make-frame")){metadataCallback={};where={7,8,9};}};
  CHECK(H::WriteWhile(f,L"hit",&obj[7],&obj[9],where,[]{return source;}));Hit h{};CHECK(f.GetRaw(L"hit",&h,sizeof(h)) && h.point.X==4);
  ResetHit();auto invalid=Destination();CHECK(!H::Write(invalid,L"hit",&obj[7],&obj[9],{std::numeric_limits<float>::quiet_NaN(),0,0}) && calls==0);
 },count);
 Case("nested-hit",selected,[&]{ResetHit();auto outer=Destination(),inner=Destination();nativeCallback=[&]{CHECK(Write(inner));};
  CHECK(Write(outer) && calls==2 && !Unchanged(outer) && !Unchanged(inner));
 },count);
 CHECK(count>0);std::printf("PASS hit construction: %d cases, real ParamFrame/native-hit bytes, required bindings, exact schemas, source/metadata/field lifetime and retry\n",count);
}

namespace ue_wrap::reflection {
std::wstring ToString(const FName&){return L"fixture_function";}
const FName& NameOf(void*){static const FName name{};return name;}
}
