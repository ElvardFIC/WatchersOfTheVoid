#include "../../src/votv-coop/src/ue_wrap/actors/food.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int index=0,serial=1;bool live=true;void* cls=nullptr;
    float temperature=300,ripeness=.5f;int uses=2;
    float newTemperature=400,newRipeness=.7f;int newUses=4;
} objects[5];
void* base=&objects[0];bool loaded=true,fields=true,newLayout=false,cooking=false,progressReady=true;
uint32_t world=1;int lookups=0;float progress=12.5f;
std::function<void()> onLookup,onProgress;
void Callback(std::function<void()>& source){auto f=std::move(source);source={};if(f)f();}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p && static_cast<test::Obj*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t index){return index>=0 && index<5 && p==&test::objects[index] && IsLive(p);}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->index:-1;}
int32_t SlotSerial(int32_t index){return test::objects[index].serial;}
int32_t AllocateSlotSerial(int32_t index){return SlotSerial(index);}
void* ClassOf(void* p){return static_cast<test::Obj*>(p)->cls;}
bool IsDescendantOfAny(void*,void* const*,size_t,int){return false;}
int32_t FindPropertyOffset(void* cls,const wchar_t* name){
    CHECK(cls==test::base);++test::lookups;int result=-1;
    if(test::fields) {
        if(!wcscmp(name,L"temperature"))result=test::newLayout?offsetof(test::Obj,newTemperature):offsetof(test::Obj,temperature);
        else if(!wcscmp(name,L"ripeness"))result=test::newLayout?offsetof(test::Obj,newRipeness):offsetof(test::Obj,ripeness);
        else {CHECK(!wcscmp(name,L"uses"));result=test::newLayout?offsetof(test::Obj,newUses):offsetof(test::Obj,uses);}
    }
    test::Callback(test::onLookup);return result;
}
void* FindClass(const wchar_t*){std::abort();}
std::wstring ClassNameOf(void*){std::abort();}
bool FindBoolProperty(void*,const wchar_t*,int32_t&,uint8_t&){std::abort();}
void* FindDispatchFunctionCached(void*,const wchar_t*){std::abort();}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}void* CurrentWorld(){return &test::objects[4];}void* WorldOf(void*){return CurrentWorld();}
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* name){CHECK(!wcscmp(name,L"prop_food_C"));return test::loaded?test::base:nullptr;}
}
namespace ue_wrap::cooking_food {
bool IsCookingFood(void*){return test::cooking;}
bool ReadProgress(void*,float& value){test::Callback(test::onProgress);if(!test::progressReady)return false;value=test::progress;return true;}
bool WriteProgress(void*,float value){test::Callback(test::onProgress);if(!test::progressReady || !test::cooking)return false;test::progress=value;return true;}
}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){std::abort();}}
namespace ue_wrap {
ParamFrame::ParamFrame(void*){std::abort();}
bool Call(void*,ParamFrame&){std::abort();}
}
int main() {
    using namespace test;namespace F=ue_wrap::food;
    for(int i=0;i<5;++i)objects[i].index=i;objects[1].cls=base;objects[2].cls=&objects[4];
    F::Condition state{};CHECK(F::Read(&objects[1],state) && state.temperature==300 && state.uses==2 && state.cookProgress==-1);
    auto count=lookups;for(int i=0;i<1000;++i)CHECK(F::Read(&objects[1],state));CHECK(lookups==count);
    CHECK(!F::IsFood(nullptr) && !F::IsFood(&objects[2]));objects[1].live=false;CHECK(!F::Read(&objects[1],state));objects[1].live=true;
    loaded=false;CHECK(!F::Read(&objects[1],state));loaded=true;
    // A new tenant at the same class address must not inherit cached offsets.
    ++objects[0].serial;newLayout=true;CHECK(F::Read(&objects[1],state) && state.temperature==400 && state.uses==4 && lookups==count+3);
    state={350,.8f,7};CHECK(F::Write(&objects[1],state) && objects[1].newUses==7 && objects[1].uses==2);
    // An incomplete lookup is retried; it cannot become a permanent negative cache.
    ++objects[0].serial;fields=false;CHECK(!F::Read(&objects[1],state));fields=true;
    CHECK(F::Read(&objects[1],state) && state.temperature==350);
    base=&objects[3];objects[1].cls=base;newLayout=false;
    onLookup=[]{++world;};auto before=state;CHECK(!F::Read(&objects[1],state) && state.temperature==before.temperature);
    CHECK(F::Read(&objects[1],state) && state.temperature==300);
    ++objects[3].serial;onLookup=[]{++objects[1].serial;};before=state;
    CHECK(!F::Read(&objects[1],state) && state.uses==before.uses);CHECK(F::Read(&objects[1],state));
    cooking=true;CHECK(F::Read(&objects[1],state) && state.cookProgress==12.5f);
    progressReady=false;state={123,4,9,99};before=state;CHECK(!F::Read(&objects[1],state) && state.temperature==123 && state.cookProgress==99);
    CHECK(!F::Write(&objects[1],state) && objects[1].temperature==300);progressReady=true;
    onProgress=[]{++world;};CHECK(!F::Read(&objects[1],state) && state.temperature==123);
    onProgress=[]{++objects[1].serial;};CHECK(!F::Read(&objects[1],state) && state.uses==9);
    onProgress=[]{++world;};CHECK(!F::Write(&objects[1],state) && objects[1].temperature==300);
    bool current=true;onProgress=[&]{current=false;};CHECK(!F::Write(&objects[1],state,[&]{return current;}) && objects[1].temperature==300);
    onProgress=[]{++objects[3].serial;};CHECK(!F::Write(&objects[1],state) && objects[1].temperature==300);
    CHECK(F::Write(&objects[1],state) && objects[1].temperature==123 && progress==99);
    puts("PASS food wrapper: indexed class cache, constant lookup count, missing fields, class/actor serial reuse, world fencing and atomic read output");
}
