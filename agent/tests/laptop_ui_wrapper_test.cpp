#include "../../src/votv-coop/src/ue_wrap/devices/laptop_ui.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include "ue_wrap/core/log.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
namespace test {
struct Array { void* data=nullptr; int32_t num=0,max=50; };
struct Obj {
    int id=0,serial=1;bool live=true;std::wstring cls;
    void* laptop=nullptr;void* gm=nullptr;void* save=nullptr;
    void* switches[3]{};void* child=nullptr;
    Array cart,slots;int index=0;
} objs[20];
using Item=std::array<uint8_t,0x50>;
std::array<Item,50> cart{};
std::array<void*,50> slots{};
Item apple{},cheese{},large{};
uint32_t world=1;
bool catalog=true,noop=false,callOK=true;
int adds=0,removes=0,rebuilds=0,sets=0;
std::function<void()> callback;
void Init() {
    for(int i=0;i<20;++i)objs[i].id=i;
    objs[0].laptop=&objs[1];objs[1].cls=L"ui_laptop_C";
    objs[1].laptop=&objs[5];objs[5].gm=&objs[0];objs[0].save=&objs[6];
    for(int i=0;i<3;++i)objs[1].switches[i]=&objs[2+i];
    objs[1].child=&objs[7];objs[1].cart.data=cart.data();objs[1].slots.data=slots.data();
    for(auto& p:slots)p=&objs[7];
    int32_t one=1,huge=51;std::memcpy(apple.data()+8,&one,4);std::memcpy(cheese.data()+8,&one,4);std::memcpy(large.data()+8,&huge,4);
}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
void* ObjectAt(int32_t i){return i>=0&&i<20?&test::objs[i]:nullptr;}
int32_t SlotSerial(int32_t i){return i>=0&&i<20?test::objs[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
bool IsLiveByIndex(void* a,int32_t i){return i>=0&&i<20&&a==&test::objs[i]&&IsLive(a);}
void* ClassOf(void* a){return a;}
std::wstring ClassNameOf(void* a){return static_cast<test::Obj*>(a)->cls;}
std::wstring ToString(const FName& n){return n.ComparisonIndex==1?L"apple":n.ComparisonIndex==2?L"cheese":n.ComparisonIndex==3?L"large":L"None";}
int32_t FindPropertyOffset(void* cls,const wchar_t* n){
    using test::Obj;
    if(!wcscmp(n,L"laptop"))return offsetof(Obj,laptop);
    if(!wcscmp(n,L"cart"))return offsetof(Obj,cart);
    if(!wcscmp(n,L"slots_cart"))return offsetof(Obj,slots);
    if(!wcscmp(n,L"gamemode"))return offsetof(Obj,gm);
    if(!wcscmp(n,L"saveSlot"))return offsetof(Obj,save);
    const wchar_t* names[]{L"switcher",L"switcher_media",L"switcher_mediaViewer"};
    for(int i=0;i<3;++i)if(!wcscmp(n,names[i]))return int(offsetof(Obj,switches)+i*sizeof(void*));
    if(!wcscmp(n,L"vb_cart")||!wcscmp(n,L"txt_price")||!wcscmp(n,L"txt_cartSize"))return offsetof(Obj,child);
    CHECK(false);return -1;
}
void* FindDispatchFunctionCached(void*,const wchar_t* name){
    const wchar_t* names[]{L"GetActiveWidgetIndex",L"SetActiveWidgetIndex",L"addStoreCart",L"removeStoreCart",L"genStoreCart"};
    for(int i=0;i<5;++i)if(!wcscmp(names[i],name))return &test::objs[10+i];
    CHECK(false);return nullptr; // no makeAnOrder/createRC/leaveRC may be replayed
}
int32_t FunctionFrameSize(void*){return 96;}
int32_t FindParamSize(void*,const wchar_t*){return 0x4d;}
std::vector<ParamInfo> FunctionParams(void*){
    std::vector<ParamInfo> out;
    for(auto name:{L"ReturnValue",L"Index",L"index",L"struct_store"}){
        ParamInfo p{};p.name=name;p.offset=!wcscmp(name,L"struct_store")?8:0;out.push_back(p);
    }
    return out;
}
bool CallFunction(void* object,void* fn,void* raw){
    using namespace test;
    if(!callOK)return false;
    auto cb=std::move(callback);callback={};if(cb)cb();
    auto* o=static_cast<Obj*>(object);int id=static_cast<Obj*>(fn)->id;
    if(id==10){std::memcpy(raw,&o->index,4);return true;}
    if(id==11){++sets;if(!noop)std::memcpy(&o->index,raw,4);return true;}
    if(noop)return true;
    CHECK(o==&objs[1]);
    if(id==12){
        ++adds;auto* b=static_cast<uint8_t*>(raw)+8;FName key{};std::memcpy(&key,b,8);
        CHECK(key.ComparisonIndex==1||key.ComparisonIndex==2); // the live table itself is unnamed
        CHECK(o->cart.num<50);std::memcpy(cart[o->cart.num++].data(),b,0x4d);++o->slots.num;
    }else if(id==13){
        ++removes;int index=0;std::memcpy(&index,raw,4);CHECK(index>=0&&index<o->cart.num);
        for(int i=index;i+1<o->cart.num;++i)cart[i]=cart[i+1];--o->cart.num;--o->slots.num;
    }else{CHECK(id==14);++rebuilds;}
    return true;
}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return nullptr;}
}
namespace ue_wrap::world_singleton {void* Gamemode(){return &test::objs[0];}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::store_catalog {
bool Ready(){return test::catalog;}
bool ReadLayout(Layout& out){out.name=0;out.size=8;return test::catalog;}
const Row* Find(const std::wstring& s){
    static Row rows[]{{test::apple.data(),1,{1,0}},{test::cheese.data(),2,{2,0}},{test::large.data(),1,{3,0}}};
    return !test::catalog?nullptr:s==L"apple"?&rows[0]:s==L"cheese"?&rows[1]:s==L"large"?&rows[2]:nullptr;
}
}
int main(){
    using namespace test;namespace UI=ue_wrap::laptop_ui;Init();UI::State state;
    CHECK(UI::Read(state)&&state.cart.empty());
    state.tabs={3,1,1};state.cart={L"apple",L"apple",L"cheese"};
    CHECK(UI::Apply(state)&&adds==3&&removes==0&&rebuilds==1);
    UI::State read;CHECK(UI::Read(read)&&read==state);
    CHECK(UI::Apply(state)&&adds==3&&rebuilds==1); // idempotent paint
    state.cart={L"apple",L"cheese"};CHECK(UI::Apply(state)&&adds==4&&removes==2);
    state.cart={L"apple",L"unknown"};CHECK(!UI::Apply(state)&&removes==2&&adds==4); // prevalidate before removing
    state.cart={L"large"};CHECK(!UI::Valid(state)&&removes==2);
    state.cart.assign(51,L"apple");CHECK(!UI::Valid(state));
    state.cart={L"cheese"};state.tabs[0]=12;CHECK(!UI::Apply(state));state.tabs[0]=4;
    objs[7].live=false;CHECK(!UI::Apply(state)&&removes==2);objs[7].live=true;
    catalog=false;CHECK(!UI::Apply(state)&&removes==2);catalog=true;
    noop=true;CHECK(!UI::Apply(state));noop=false; // a successful dispatch alone is not success
    CHECK(UI::Apply(state));
    ue_wrap::reflection::FName none{};std::memcpy(cart[0].data(),&none,sizeof(none));
    CHECK(!UI::Read(read)&&UI::Apply(state)); // recover an older unnamed refused-order cart
    const auto calls=adds;state.cart={L"apple"};callback=[] {++world;};
    CHECK(!UI::Apply(state)&&adds==calls); // world changed inside first removal; never continue adding
    CHECK(UI::Apply(state));
    state.cart.clear();CHECK(UI::Apply(state)&&objs[1].cart.num==0&&objs[1].slots.num==0);
    objs[1].cart.num=-1;CHECK(!UI::Read(read));objs[1].cart.num=0;
    objs[2].live=false;CHECK(!UI::Read(read));
    puts("PASS laptop UI wrapper: reflected tabs, duplicate/reordered cart rows, native catalog key stamping, suffix edits, native rebuild, validation before mutation, no purchase/camera replay, effect and world fences");
}
