#include "../../src/votv-coop/src/ue_wrap/world/sky_black_hole.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj{int id=0,serial=1;bool live=true;uint8_t flags=0x81;void* light=nullptr;void* other=nullptr;} objs[6];
bool fields=true,callOk=true,kill=false,ready=true;int changes=0;
}
namespace ue_wrap::skysphere {void* Sky(){return &test::objs[0];}}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
void* ClassOf(void* a){return a;}
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
bool IsLiveByIndex(void* a,int32_t i){return i>=0&&i<6&&a==&test::objs[i]&&IsLive(a);}
int32_t SlotSerial(int32_t i){return test::objs[i].serial;}
void* ObjectAt(int32_t i){return i>=0&&i<6?&test::objs[i]:nullptr;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
int32_t FindPropertyOffset(void*,const wchar_t* n){if(!test::fields)return -1;return !wcscmp(n,L"light_blackhole")?offsetof(test::Obj,light):offsetof(test::Obj,other);}
bool FindBoolProperty(void*,const wchar_t* n,int32_t& off,uint8_t& mask){off=offsetof(test::Obj,flags);mask=!wcscmp(n,L"skyHidden")?8:32;return test::fields;}
void* FindDispatchFunctionCached(void*,const wchar_t* n){return &test::objs[!wcscmp(n,L"skyVisibility")?3:4];}
int32_t StructSize(void*){return 2;}
int32_t FunctionFrameSize(void*){return 2;}
std::vector<ParamInfo> FunctionParams(void* f){ParamInfo a{},b{};a.name=f==&test::objs[3]?L"bNewHidden":L"bNewVisibility";a.offset=0;b.name=L"bPropagateToChildren";b.offset=1;return {a,b};}
bool CallFunction(void* a,void* f,void* raw){
 if(!test::callOk)return false;
 auto* obj=static_cast<test::Obj*>(a);uint8_t mask=f==&test::objs[3]?8:32;
 if(f==&test::objs[4])CHECK(!static_cast<bool*>(raw)[1]);
 obj->flags=(obj->flags&~mask)|(*static_cast<bool*>(raw)?mask:0);++test::changes;
 if(test::kill)test::objs[0].live=false;return true;
}
}
namespace ue_wrap::world_identity {void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
int main(){using namespace test;namespace B=ue_wrap::skysphere;
 for(int i=0;i<6;++i)objs[i].id=i;objs[0].light=&objs[1];objs[0].other=&objs[2];
 bool h=true,l=true;CHECK(B::ReadBlackHole(h,l)&&!h&&!l);
 CHECK(B::ApplyBlackHole(true,true)&&changes==2&&objs[0].flags==0x89&&objs[1].flags==0xa1);
 CHECK(B::ApplyBlackHole(true,true)&&changes==2); // duplicates do not retrigger native effects
 CHECK(B::ApplyBlackHole(false,false)&&changes==4&&objs[0].flags==0x81&&objs[1].flags==0x81);
 fields=false;CHECK(!B::ReadBlackHole(h,l)&&!B::ApplyBlackHole(true,true));fields=true;
 objs[0].other=nullptr;CHECK(!B::ReadBlackHole(h,l)&&!B::ApplyBlackHole(true,true));objs[0].other=&objs[2];
 callOk=false;CHECK(!B::ApplyBlackHole(true,true)&&changes==4);callOk=true;
 kill=true;CHECK(!B::ApplyBlackHole(true,true)&&changes==5&&objs[1].flags==0x81);
 kill=false;objs[0].live=true;CHECK(B::ApplyBlackHole(true,true)&&changes==6); // partial apply converges
 puts("PASS real black-hole wrapper/ParamFrame: material/light verbs, packed bits, idempotency, readiness, failed/partial call, destroyed actor");
}

// Reflection names used by ParamFrame missing-field diagnostics.
namespace ue_wrap::reflection {
std::wstring ToString(const FName&){return L"fixture_function";}
const FName& NameOf(void*){static const FName name{};return name;}
}
