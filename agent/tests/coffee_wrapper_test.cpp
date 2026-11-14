#ifndef COFFEE_WRAPPER_SOURCE
#define COFFEE_WRAPPER_SOURCE "../../src/votv-coop/src/ue_wrap/devices/coffee.cpp"
#endif
#ifndef APPLIANCE_WRAPPER_SOURCE
#define APPLIANCE_WRAPPER_SOURCE "../../src/votv-coop/src/ue_wrap/devices/appliance.cpp"
#endif
#include COFFEE_WRAPPER_SOURCE
#include APPLIANCE_WRAPPER_SOURCE
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int id=0,serial=1;bool live=true;void* cls=nullptr;uint8_t bytes[8]{};int amounts=9;} obj[12];
uint32_t world=1;
int boolShift=0,calls=0,frameSize=16,missing=0,boolResolves=0;
bool boolReady=true,authority=true;
std::function<void()> onLookup,onBool,onField,onFind,onFrame,onParams,onCall;
void Fire(std::function<void()>& f){auto cb=std::move(f);f={};if(cb)cb();}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<12&&p==&test::obj[i]&&IsLive(p);}
int32_t SlotSerial(int32_t i){return i>=0&&i<12?test::obj[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return i>=0&&i<12?&test::obj[i]:nullptr;}
void* ClassOf(void* p){return p?static_cast<test::Obj*>(p)->cls:nullptr;}
const FName& NameOf(void*){static FName n{};return n;}
std::wstring ToString(const FName&){return L"fixture";}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& off,uint8_t& mask){
    ++test::boolResolves;test::Fire(test::onBool);
    off=int32_t(offsetof(test::Obj,bytes))+test::boolShift+(!wcscmp(name,L"hasCup")?0:!wcscmp(name,L"hasCoffee")?1:2);mask=1;
    return test::boolReady;
}
int32_t InstanceOffset::Of(void*){test::Fire(test::onField);return int32_t(offsetof(test::Obj,amounts));}
void* FindDispatchFunctionCached(void*,const wchar_t*){test::Fire(test::onFind);return &test::obj[8];}
int32_t FunctionFrameSize(void*){test::Fire(test::onFrame);return test::frameSize;}
std::vector<ParamInfo> FunctionParams(void*){
    test::Fire(test::onParams);std::vector<ParamInfo> p;
    if(test::missing!=1)p.push_back({L"player",0,8,0});
    if(test::missing!=2)p.push_back({L"action",test::missing==3?16:8,1,0});return p;
}
bool CallFunction(void* actor,void* fn,void* data){
    CHECK(actor==&test::obj[1]&&fn==&test::obj[8]&&data);
    void* player=nullptr;std::memcpy(&player,data,8);CHECK(player==&test::obj[3]);CHECK(static_cast<uint8_t*>(data)[8]==4);
    ++test::calls;test::Fire(test::onCall);return true;
}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* p){return static_cast<test::Obj*>(p)->id<=3?CurrentWorld():nullptr;}
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* n){test::Fire(test::onLookup);return !wcscmp(n,L"prop_coffeeMaker_C")?&test::obj[5]:!wcscmp(n,L"prop_coffeeBag_C")?&test::obj[6]:&test::obj[4];}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace test {
namespace C=ue_wrap::coffee;namespace A=ue_wrap::appliance;
void Reset(){
    ++world;for(int i=0;i<12;++i){obj[i].id=i;++obj[i].serial;obj[i].live=true;obj[i].cls=nullptr;for(auto& b:obj[i].bytes)b=0;}
    obj[1].cls=&obj[5];obj[2].cls=&obj[6];obj[3].cls=&obj[7];
    C::g_vending.ref.Reset();C::g_maker.ref.Reset();C::g_bag.ref.Reset();
    C::g_hasCup={L"hasCup"};C::g_hasCoffee={L"hasCoffee"};C::g_coffeeReady={L"coffeeReady"};
    calls=missing=boolShift=boolResolves=0;frameSize=16;boolReady=authority=true;
    onLookup=onBool=onField=onFind=onFrame=onParams=onCall={};
}
bool Action(){return A::CallActionWhile(&obj[1],&obj[3],4,[]{return authority;});}
}
int main(){using namespace test;
    Reset();CHECK(C::DeviceOf(&obj[1])==C::Device::Maker&&C::IsBag(&obj[2]));CHECK(C::DeviceOf(nullptr)==C::Device::None);
    Reset();onLookup=[] {++obj[1].serial;};CHECK(C::DeviceOf(&obj[1])==C::Device::None);
    Reset();onLookup=[] {++obj[5].serial;};CHECK(C::DeviceOf(&obj[1])==C::Device::None);
    Reset();C::MakerFlags flags{};CHECK(C::ReadMakerFlags(&obj[1],flags));CHECK(!flags.hasCup&&!flags.hasCoffee&&!flags.coffeeReady);
    ++obj[5].serial;boolShift=3;obj[1].bytes[3]=obj[1].bytes[4]=obj[1].bytes[5]=1;
    CHECK(C::ReadMakerFlags(&obj[1],flags)&&flags.hasCup&&flags.hasCoffee&&flags.coffeeReady&&boolResolves==6);
    Reset();boolReady=false;flags={true,true,true};CHECK(!C::ReadMakerFlags(&obj[1],flags)&&flags.hasCup);
    boolReady=true;CHECK(C::ReadMakerFlags(&obj[1],flags)&&!flags.hasCup&&boolResolves==4);
    Reset();flags={true,true,true};onBool=[] {++obj[1].serial;};CHECK(!C::ReadMakerFlags(&obj[1],flags)&&flags.hasCup);
    Reset();int amount=-1;onField=[] {++obj[2].serial;};CHECK(!C::ReadBagAmounts(&obj[2],amount)&&amount==-1);
    Reset();onField=[] {++obj[2].serial;};CHECK(!C::ReadField(&obj[2],C::g_amounts,amount)&&amount==-1);
    Reset();CHECK(C::ReadBagAmounts(&obj[2],amount)&&amount==9);
    Reset();CHECK(Action()&&calls==1);
    for(int i=1;i<=3;++i){Reset();missing=i;CHECK(!Action()&&calls==0);}
    Reset();onFind=[] {++obj[1].serial;};CHECK(!Action()&&calls==0);
    for(int kind=0;kind<4;++kind){Reset();onFrame=[kind] {++obj[kind==0?3:kind==1?5:kind==2?8:1].serial;};CHECK(!Action()&&calls==0);}
    Reset();onParams=[] {authority=false;};CHECK(!Action()&&calls==0);
    Reset();onCall=[] {authority=false;};CHECK(!Action()&&calls==1);
    Reset();onCall=[] {++obj[1].serial;};CHECK(!Action()&&calls==1);
    Reset();onFrame=[] {++world;};CHECK(!Action()&&calls==0);
    puts("PASS real coffee/appliance wrappers W01-W06, A01-A07: class/actor/function/player lifetime, metadata retry, checked ParamFrame writes and single native attempt");
}
// Adjacent appliance helpers are outside this fixture; accidental use fails loudly.
namespace ue_wrap::reflection {
int32_t FindPropertyOffset(void*,const wchar_t*){std::abort();}
void* FindFunction(void*,const wchar_t*){std::abort();}
bool IsDescendantOfAny(void*,void* const*,size_t,int){std::abort();}
}
namespace ue_wrap::component_calls {bool CallParamlessNamed(void*,const wchar_t*){std::abort();}}
