#ifndef WALL_WRAPPER_SOURCE
#define WALL_WRAPPER_SOURCE "../../src/votv-coop/src/ue_wrap/actors/wall_build.cpp"
#endif
#include WALL_WRAPPER_SOURCE
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int id=0,serial=1;bool live=true;void* overlap=nullptr;void* cls=nullptr;} objs[400];
bool touches=true,callOk=true,kill=false,fields=true,materialOK=true;int feeds=0,restores=0;
bool solid=false,locationOK=true,damageFields=true;int damages=0;uint32_t world=1;
ue_wrap::FVector locationValue{1,2,3};
void (*locCallback)()=nullptr;
void (*overlapCallback)()=nullptr;
void (*nameCallback)()=nullptr;
int queries=0;
bool flexible=false;
std::function<void(const char*)> metadata;
std::function<bool(void*,void*,void*)> nativeFeed;
void Meta(const char* stage){auto callback=metadata;if(callback)callback(stage);}
}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
void* ClassOf(void* a){return a&&static_cast<test::Obj*>(a)->cls?static_cast<test::Obj*>(a)->cls:a;}
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
bool IsLiveByIndex(void* a,int32_t i){return i>=0&&i<400&&a==&test::objs[i]&&IsLive(a);}
int32_t SlotSerial(int32_t i){return test::objs[i].serial;}
void* ObjectAt(int32_t i){return i>=0&&i<400?&test::objs[i]:nullptr;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
int32_t InstanceOffset::Of(void*){test::Meta("offset");return test::fields?offsetof(test::Obj,overlap):-1;}
void* FindDispatchFunctionCached(void*,const wchar_t* n){test::Meta(!wcscmp(n,L"IsOverlappingActor")?"find-query":"find-feed");return &test::objs[!wcscmp(n,L"damage")?6:!wcscmp(n,L"IsOverlappingActor")?3:!wcscmp(n,L"findMat")?5:4];}
void* OuterOf(void* fn){return fn;}
int32_t StructSize(void*){return 16;}
int32_t FunctionFrameSize(void* f){return f==&test::objs[6]?32:16;}
std::vector<ParamInfo> FunctionParams(void* f){
 test::Meta(f==&test::objs[3]?"query-params":"feed-params");
 if(f==&test::objs[6]){ParamInfo a{},b{},c{};a.name=L"damage";a.offset=0;b.name=L"Location";b.offset=4;c.name=test::damageFields?L"Rotation":L"wrong";c.offset=16;return {a,b,c};}
 ParamInfo a{},b{};a.name=f==&test::objs[3]?L"Other":f==&test::objs[5]?L"B":L"OtherActor";a.offset=0;
 b.name=f==&test::objs[5]?L"return":L"ReturnValue";b.offset=8;return {a,b};
}
bool CallFunction(void* a,void* f,void* raw){
 if(f==&test::objs[3])++test::queries;
 if(!test::callOk)return false;
 auto* p=static_cast<uint8_t*>(raw);
 if(f==&test::objs[3]){
  if(!test::flexible)CHECK(a==&test::objs[2]&&*reinterpret_cast<void**>(p)==&test::objs[1]);p[8]=test::touches;
  if(test::kill)test::objs[1].live=false;
  if(test::overlapCallback)test::overlapCallback();
 }
 else if(f==&test::objs[4]){if(test::nativeFeed)return test::nativeFeed(a,*reinterpret_cast<void**>(p),raw);CHECK(a==&test::objs[0]&&*reinterpret_cast<void**>(p)==&test::objs[1]);++test::feeds;}
 else if(f==&test::objs[6]){CHECK(*reinterpret_cast<float*>(p)==20.f);auto* location=reinterpret_cast<FVector*>(p+4);CHECK(location->X==1&&location->Y==2&&location->Z==3);++test::damages;if(test::kill)test::objs[0].live=false;}
 else {CHECK(a==&test::objs[0]&&reinterpret_cast<FName*>(p)->ComparisonIndex==9);p[8]=test::materialOK;++test::restores;}
 return true;
}
}
namespace ue_wrap::world_identity {void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}uint32_t Generation(){return test::world;}}
namespace ue_wrap::engine {bool TryGetActorLocation(void*,FVector& p){p=test::locationValue;if(test::locCallback)test::locCallback();return test::locationOK;}}
namespace ue_wrap::save_record {std::wstring ReadFNameAt(const void*,int32_t){return L"native_wall_17";}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring&){if(test::nameCallback)test::nameCallback();return {9,0};}}
namespace ue_wrap::placed_save {Kind Type(void* a){return a==&test::objs[0]&&reflection::IsLive(a)?(test::solid?Kind::Wall:Kind::WallOutline):Kind::None;}}
int main(){using namespace test;namespace W=ue_wrap::wall_build;
 for(int i=0;i<7;++i)objs[i].id=i;objs[0].overlap=&objs[2];
 CHECK(W::Feed(&objs[0],&objs[1])&&feeds==1);
 touches=false;CHECK(W::Feed(&objs[0],&objs[1])&&feeds==1);touches=true;
 fields=false;CHECK(!W::Feed(&objs[0],&objs[1]));fields=true;
 callOk=false;CHECK(!W::Feed(&objs[0],&objs[1])&&feeds==1);callOk=true;
 kill=true;CHECK(W::Feed(&objs[0],&objs[1])&&feeds==1);kill=false;objs[1].live=true;
 CHECK(W::RestoreMaterial(&objs[0],L"")&&restores==0);
 CHECK(W::RestoreMaterial(&objs[0],L"brick")&&restores==1);
 materialOK=false;CHECK(!W::RestoreMaterial(&objs[0],L"invalid"));
 CHECK(!W::Damage(&objs[0],20.f));solid=true;
 CHECK(W::Key(&objs[0])==L"native_wall_17");fields=false;CHECK(W::Key(&objs[0]).empty());fields=true;
 CHECK(W::Damage(&objs[0],20.f)&&damages==1);
 CHECK(!W::Damage(&objs[0],0)&&!W::Damage(&objs[0],NAN));
 damageFields=false;++objs[6].serial;CHECK(!W::Damage(&objs[0],20.f)&&damages==1);damageFields=true;++objs[6].serial;
 locationOK=false;CHECK(!W::Damage(&objs[0],20.f));locationOK=true;
 locCallback=[]{++world;};CHECK(!W::Damage(&objs[0],20.f)&&damages==1);locCallback=nullptr;
 kill=true;CHECK(W::Damage(&objs[0],20.f)&&damages==2&&!objs[0].live);
 kill=false;objs[0].live=true;solid=false;
 const int previous=feeds;
 overlapCallback=[]{++world;};CHECK(!W::Feed(&objs[0],&objs[1])&&feeds==previous);
 overlapCallback=[]{++objs[0].serial;};CHECK(W::Feed(&objs[0],&objs[1])&&feeds==previous);
 overlapCallback=[]{++objs[1].serial;};CHECK(W::Feed(&objs[0],&objs[1])&&feeds==previous);
 overlapCallback=[]{solid=true;};CHECK(W::Feed(&objs[0],&objs[1])&&feeds==previous);solid=false;
 overlapCallback=[]{++objs[2].serial;};CHECK(!W::Feed(&objs[0],&objs[1])&&feeds==previous);
 overlapCallback=[]{objs[0].overlap=&objs[6];};CHECK(!W::Feed(&objs[0],&objs[1])&&feeds==previous);
 objs[0].overlap=&objs[2];overlapCallback=nullptr;CHECK(W::Feed(&objs[0],&objs[1])&&feeds==previous+1);
 const int oldRestores=restores;materialOK=true;
 nameCallback=[]{++world;};CHECK(!W::RestoreMaterial(&objs[0],L"brick")&&restores==oldRestores);nameCallback=nullptr;
 puts("PASS wall feed context: post-query world/actor/material/completed-wall refusal, component identity retry and material-lookup travel guard");
 puts("PASS real wall wrapper/ParamFrame: live overlap, native arguments, dead source, failures and restored material lookup");
 return 0;
}

// Reflection names used by ParamFrame missing-field diagnostics.
namespace ue_wrap::reflection {
std::wstring ToString(const FName&){return L"fixture_function";}
const FName& NameOf(void*){static const FName name{};return name;}
}
