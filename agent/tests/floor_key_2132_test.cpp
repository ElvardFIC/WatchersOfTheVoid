#include "../../src/votv-coop/src/ue_wrap/actors/floor_bed.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj { int id; void* cls=nullptr; void* super=nullptr; ue_wrap::reflection::FName name{}; };
Obj base{1},variant{2},bunk{3},prop{4},actor{5},fn{6};
int serial=1,calls=0,world=1,oldWorld=2,frameSize=8;
void* current=&world;
bool live=true,verb=true,callOk=true,write=true;
std::wstring key=L"None";
std::function<void()> callback;
}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a && test::live;}
void* ClassOf(void* a){return static_cast<test::Obj*>(a)->cls;}
void* SuperStructOf(void* c){return static_cast<test::Obj*>(c)->super;}
const FName& NameOf(void* c){return static_cast<test::Obj*>(c)->name;}
bool NameEquals(const FName& n,const wchar_t* s){CHECK(std::wstring(s)==L"sleepingbagWrap_C");return n.ComparisonIndex==1;}
int32_t InternalIndexOf(void* a){return static_cast<test::Obj*>(a)->id;}
int32_t SlotSerial(int32_t){return test::serial;}
int32_t AllocateSlotSerial(int32_t){return test::serial;}
bool IsLiveByIndex(void* a,int32_t i){return a && test::live && InternalIndexOf(a)==i;}
void* ObjectAt(int32_t i){return i==6?&test::fn:nullptr;}
void* FindDispatchFunctionCached(void*,const wchar_t* n){CHECK(std::wstring(n)==L"getKey");return test::verb?&test::fn:nullptr;}
int32_t FunctionFrameSize(void*){return test::frameSize;}
std::vector<ParamInfo> FunctionParams(void*){return {{L"key",0,8,0}};}
bool CallFunction(void* actor,void* fn,void* params){
    CHECK(actor==&test::actor && fn==&test::fn && params);++test::calls;
    auto cb=std::move(test::callback);test::callback={};if(cb)cb();
    if(!test::callOk)return false;
    if(test::write)test::key=L"native-guid";
    return true;
}
}
namespace ue_wrap::world_identity {void* WorldOf(void*){return &test::world;}void* CurrentWorld(){return test::current;}}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){return test::key;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
int main(){using namespace test;using namespace ue_wrap::floor_bed;
    base.name.ComparisonIndex=1;variant.super=&base;bunk.super=&prop;actor.cls=&variant;
    CHECK(IsFloorBed(&actor));actor.cls=&base;CHECK(IsFloorBed(&actor));
    actor.cls=&bunk;CHECK(!IsFloorBed(&actor) && !EnsureKey(&actor) && calls==0);
    actor.cls=&variant;live=false;CHECK(!EnsureKey(&actor));live=true;CHECK(!EnsureKey(nullptr));
    key=L"saved-key";CHECK(EnsureKey(&actor) && calls==0 && key==L"saved-key");
    key=L"None";verb=false;CHECK(!EnsureKey(&actor));verb=true;
    callOk=false;CHECK(!EnsureKey(&actor));callOk=true;
    write=false;CHECK(!EnsureKey(&actor));write=true;
    callback=[]{CHECK(!EnsureKey(&actor));};CHECK(EnsureKey(&actor));CHECK(key==L"native-guid");
    int before=calls;CHECK(EnsureKey(&actor) && calls==before);
    key=L"";callback=[]{++serial;};CHECK(!EnsureKey(&actor));
    key=L"";callback=[]{current=&oldWorld;};CHECK(!EnsureKey(&actor));current=&world;
    key=L"";callback=[]{live=false;};CHECK(!EnsureKey(&actor));live=true;
    key=L"";frameSize=-1;++serial;CHECK(!EnsureKey(&actor));
    frameSize=8;++serial;CHECK(EnsureKey(&actor));
    puts("PASS: real floor wrapper/ParamFrame, lineage, existing keys, native failure/readback, recursive call, identity/world/death");
}
