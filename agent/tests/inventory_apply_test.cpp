#ifndef INVENTORY_APPLY_SOURCE
#define INVENTORY_APPLY_SOURCE "../../src/votv-coop/src/ue_wrap/actors/inventory.cpp"
#endif
#include INVENTORY_APPLY_SOURCE
#ifndef INVENTORY_CONTAINER_SOURCE
#define INVENTORY_CONTAINER_SOURCE "../../src/votv-coop/src/ue_wrap/actors/container_inventory.cpp"
#endif
#include INVENTORY_CONTAINER_SOURCE
#include "../../src/votv-coop/src/ue_wrap/actors/save_record.cpp"
#ifndef INVENTORY_OFFSET_SOURCE
#define INVENTORY_OFFSET_SOURCE "instance_offset_production.inc"
#endif
#include INVENTORY_OFFSET_SOURCE
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace I=ue_wrap::inventory;
namespace C=ue_wrap::container_inventory;
namespace SR=ue_wrap::save_record;
namespace test {
struct Obj {int id=0,serial=1;bool live=true;int bias=0;Obj* cls=nullptr;ue_wrap::reflection::FName name{};std::wstring label;alignas(16) uint8_t fields[0x700]{};} objects[32];
alignas(16) uint8_t stack[32]{},oldItem[256]{},foreignItem[256]{},oldWorn[2*0x120]{},oldHand[0x120]{};
uint32_t world=1;bool metadata=true,boolReady=true,classesReady=true,namesReady=true,stringsReady=true,signalsReady=true;
int allocations=0,failAt=0,decodes=0;
std::set<void*> owned;
std::map<std::wstring,int> ids;std::map<int,std::wstring> words;
std::map<const void*,ue_wrap::signal_dynamic::Row> signals;
std::function<void()> onNative,onAllocate;
std::function<void()> onVolume;
int volumeCalls=0,nameCalls=0;
void Callback(std::function<void()>& source){auto fn=std::move(source);source={};if(fn)fn();}
int Field(void* p,int offset){return int(offsetof(Obj,fields))+static_cast<Obj*>(p)->cls->bias+offset;}
template<class T>void Set(void* p,int offset,T value){std::memcpy(static_cast<uint8_t*>(p)+Field(p,offset),&value,sizeof(value));}
void* Save(){return &objects[1];}
I::PlayerInventory Rich() {
    I::PlayerInventory inv;SR::SaveRecord r;r.className=L"prop_sample_C";r.key=L"original_key";
    r.bools={{1,0},{}};r.floats={{.25f,0.f}};r.ints={{7}};r.strings={{L"saved text"}};r.names={{L"saved_name",L""}};
    r.classes={{L"prop_sample_C"}};r.vectors={{{1,2,3}}};r.rotators={{{4,5,6}}};r.transforms={{{0,0,0,1,2,3,4,1,1,1}}};r.bytes={{4,9}};
    ue_wrap::signal_dynamic::Row signal;signal.name=L"signal";signal.signal=L"template";signal.size=12;r.signals.push_back(signal);
    inv.inventory.push_back(r);I::EquipRecord e;e.propName=L"sample";e.propKey=r.key;e.tag=L"hand";e.data=r;
    inv.equipment.push_back(e);inv.hold.push_back(e);return inv;
}
void Reset() {
    for(void* p:owned)std::free(p);owned.clear();signals.clear();ids.clear();words.clear();
    world=1;metadata=boolReady=classesReady=namesReady=stringsReady=signalsReady=true;allocations=failAt=decodes=0;onNative=onAllocate=onVolume={};volumeCalls=nameCalls=0;
    for(int i=0;i<32;++i){const int serial=objects[i].serial+1;objects[i]={};objects[i].id=i;objects[i].serial=serial;objects[i].name.ComparisonIndex=10000+i;objects[i].cls=&objects[31];}
    for(int i=0;i<4;++i)objects[i].cls=&objects[10+i];objects[4].cls=&objects[11];
    objects[10].label=L"mainGamemode_C";objects[11].label=L"saveSlot_C";objects[12].label=L"prop_container_C";objects[13].label=L"propInventory_C";
    objects[20].label=L"prop_sample_C";objects[21].label=L"prop_foreign_C";
    std::memset(stack,0,sizeof(stack));std::memset(oldItem,0,sizeof(oldItem));std::memset(foreignItem,0,sizeof(foreignItem));
    std::memset(oldWorn,0,sizeof(oldWorn));std::memset(oldHand,0,sizeof(oldHand));
    Set(&objects[0],0,Save());Set(&objects[0],8,static_cast<void*>(&objects[2]));Set(&objects[2],0,static_cast<void*>(&objects[3]));
    Set(&objects[3],0,int32_t(0));Set(&objects[3],4,uint8_t(0x24));Set(&objects[3],8,static_cast<void*>(&objects[2]));Set(&objects[3],16,12.f);
    SR::WriteArrHeader(Save(),Field(Save(),32),stack,2);SR::WriteArrHeader(stack,0,oldItem,1);SR::WriteArrHeader(stack+16,0,foreignItem,1);
    SR::WriteArrHeader(Save(),0x440,oldWorn,2);SR::WriteArrHeader(Save(),0x450,oldHand,1);
    void* cls=&objects[20];std::memcpy(oldItem,&cls,sizeof(cls));cls=&objects[21];std::memcpy(foreignItem,&cls,sizeof(cls));
}
struct Images {
    std::array<uint8_t,16> stackHeader{},carried{},worn{},hand{};
    Images(){std::memcpy(stackHeader.data(),static_cast<uint8_t*>(Save())+Field(Save(),32),16);std::memcpy(carried.data(),stack,16);std::memcpy(worn.data(),static_cast<uint8_t*>(Save())+0x440,16);std::memcpy(hand.data(),static_cast<uint8_t*>(Save())+0x450,16);}
    bool Same()const{return !std::memcmp(stackHeader.data(),static_cast<uint8_t*>(Save())+Field(Save(),32),16)&&!std::memcmp(carried.data(),stack,16)&&!std::memcmp(worn.data(),static_cast<uint8_t*>(Save())+0x440,16)&&!std::memcmp(hand.data(),static_cast<uint8_t*>(Save())+0x450,16);}
};
SR::SaveRecord Item(){SR::SaveRecord r;auto a=SR::ReadArr(stack,0);CHECK(a.num==1);SR::ReadSaveRecord(a.data,r);return r;}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){for(auto& o:test::objects)if(p==&o)return o.live;return false;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<32&&p==&test::objects[i]&&test::objects[i].live;}
int32_t InternalIndexOf(void* p){for(auto& o:test::objects)if(p==&o)return o.id;return -1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<32?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void* p){return p?static_cast<test::Obj*>(p)->cls:nullptr;}
std::wstring ClassNameOf(void* p){return p?static_cast<test::Obj*>(p)->cls->label:L"";}
const FName& NameOf(void* p){return static_cast<test::Obj*>(p)->name;}
std::wstring ToString(const FName& n){return n.ComparisonIndex>=10000?test::objects[n.ComparisonIndex-10000].label:test::words[n.ComparisonIndex];}
int32_t FindPropertyOffset(void* cls,const wchar_t* name) {
    if(!test::metadata)return -1;const std::wstring n=name;
    int off=n==L"playerContainer"?8:n==L"GObjStack"?32:n==L"Player"?4:n==L"Owner"?8:n==L"currVol"?16:0;
    return int(offsetof(test::Obj,fields))+static_cast<test::Obj*>(cls)->bias+off;
}
bool FindBoolProperty(void* cls,const wchar_t* n,int32_t& off,uint8_t& mask){off=FindPropertyOffset(cls,n);mask=4;return test::boolReady&&off>=0;}
void* FindClass(const wchar_t* n){return test::classesReady&&!wcscmp(n,L"prop_sample_C")?&test::objects[20]:nullptr;}
void* FindFunction(void*,const wchar_t*){std::abort();}
void* FindDispatchFunction(void*,const wchar_t*,void**){std::abort();}
#ifndef CONTAINER_APPLY_HARNESS
void* FindDispatchFunctionCached(void*,const wchar_t* n){return &test::objects[!wcscmp(n,L"updateVolumesAndMass")?26:27];}
#endif
void* SuperStructOf(void*){std::abort();}
bool IsDescendantOfAny(void*,void* const*,size_t,int){std::abort();}
void* EngineAlloc(size_t bytes,uint32_t) {
    ++test::allocations;test::Callback(test::onAllocate);if(test::failAt==test::allocations)return nullptr;
    void* p=std::malloc(bytes);if(p)test::owned.insert(p);return p;
}
void EngineFree(void* p){CHECK(test::owned.erase(p)==1);std::free(p);}
}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring& s) {
    test::Callback(test::onNative);if(!test::namesReady||s.empty()||!::_wcsicmp(s.c_str(),L"None"))return {};
    auto& id=test::ids[s];if(!id){id=int(test::ids.size());test::words[id]=s;}return {id,0};
}}
namespace ue_wrap::fstring_utils {bool MintFString(const std::wstring& s,void* out) {
    test::Callback(test::onNative);if(!test::stringsReady)return false;
    reflection::FString f{};if(!s.empty()) {
        f.Data=static_cast<wchar_t*>(reflection::EngineAlloc((s.size()+1)*sizeof(wchar_t)));if(!f.Data)return false;
        std::memcpy(f.Data,s.c_str(),(s.size()+1)*sizeof(wchar_t));f.Num=f.Max=int32_t(s.size()+1);
    }
    std::memcpy(out,&f,sizeof(f));return true;
}}
namespace ue_wrap::signal_dynamic {
bool ReadStruct(const void* p,Row& out,bool){auto it=test::signals.find(p);if(it==test::signals.end())return false;out=it->second;return true;}
bool WriteStructLive(void* p,const Row& row){test::Callback(test::onNative);if(!test::signalsReady)return false;test::signals[p]=row;test::signals[p].image.clear();return true;}
}
#ifndef CONTAINER_APPLY_HARNESS
namespace ue_wrap::object_index {void* ClassByName(const wchar_t* n){return &test::objects[!wcscmp(n,L"propInventory_C")?13:12];}}
#endif
namespace ue_wrap::prop {bool WalksToBase(void* cls,void* base){return cls==base;}std::wstring GetInteractableKeyString(void*){std::abort();}}
#ifndef CONTAINER_APPLY_HARNESS
namespace ue_wrap::component_calls {bool CallParamless(void*,void* fn){if(fn==&test::objects[26]){++test::volumeCalls;test::Callback(test::onVolume);}else ++test::nameCalls;return true;}}
#endif
namespace ue_wrap::building_material {bool RefreshHeldMaterialRecord(void*,save_record::SaveRecord&){std::abort();}}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn){}
int32_t ParamFrame::OffsetOf(const wchar_t*)const{std::abort();}
bool Call(void*,ParamFrame&){std::abort();}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::world_singleton {void* Gamemode(){return &test::objects[0];}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
int main() {
    using namespace test;Reset();const auto rich=Rich();
    CHECK(I::ApplyToSaveObject(Save(),rich));const int count=allocations;auto record=Item();
    CHECK(record.className==rich.inventory[0].className&&record.key==rich.inventory[0].key&&record.strings==rich.inventory[0].strings&&record.ints==rich.inventory[0].ints&&record.names==rich.inventory[0].names&&record.signals==rich.inventory[0].signals);
    auto worn=SR::ReadArr(Save(),0x440),held=SR::ReadArr(Save(),0x450);CHECK(worn.num==2&&held.num==1);
    I::EquipRecord empty;I::ReadEquipRecord(worn.data+0x120,empty);CHECK(empty.data.className.empty()&&empty.data.xform==SR::SaveRecord{}.xform);
    for(int failure=1;failure<=count;++failure){Reset();Images before;failAt=failure;const bool applied=I::ApplyToSaveObject(Save(),rich);if(applied||!before.Same())std::fprintf(stderr,"allocation control %d/%d: applied=%d unchanged=%d\n",failure,count,applied,before.Same());CHECK(!applied&&before.Same());}
    for(int failure=0;failure<4;++failure) {
        Reset();Images before;if(failure==0)classesReady=false;if(failure==1)namesReady=false;if(failure==2)stringsReady=false;if(failure==3)signalsReady=false;
        CHECK(!I::ApplyToSaveObject(Save(),rich)&&before.Same());
    }
    Reset();auto bytes=allocations;CHECK(!SR::AllocZeroed(std::numeric_limits<size_t>::max(),16)&&allocations==bytes);
    Reset();CHECK(I::ApplyToSaveObject(Save(),{}));CHECK(SR::ReadArr(stack,0).num==0&&SR::ReadArr(Save(),0x440).num==2&&SR::ReadArr(Save(),0x450).num==1);
    puts("PASS production inventory/codec: every outer and nested allocation failure preserves all three arrays; class/name/string/signal failures refuse; fixed empty slots and overflow are safe");

    for(int change=0;change<5;++change) {
        Reset();Images before;onNative=[change] {if(change==0)++world;if(change==1)++objects[1].serial;if(change==2)Set(&objects[0],0,static_cast<void*>(&objects[4]));if(change==3)++objects[11].serial;if(change==4)++objects[0].serial;};
        CHECK(!I::ApplyToSaveObject(Save(),rich)&&before.Same());
    }
    Reset();onNative=[]{SR::WriteArrHeader(Save(),0x450,nullptr,0);};CHECK(!I::ApplyToSaveObject(Save(),rich));CHECK(SR::ReadArr(stack,0).data==oldItem&&SR::ReadArr(Save(),0x450).num==0);
    Reset();onNative=[]{SR::WriteArrHeader(Save(),Field(Save(),32),nullptr,0);};CHECK(!I::ApplyToSaveObject(Save(),rich));CHECK(SR::ReadArr(Save(),Field(Save(),32)).num==0);
    Reset();Images before;onAllocate=[]{++world;};CHECK(!I::ApplyToSaveObject(Save(),rich)&&before.Same()&&owned.empty());
    Reset();auto input=rich;onNative=[&]{input={};};CHECK(I::ApplyToSaveObject(Save(),input)&&Item().key==L"original_key");
    Reset();onNative=[]{auto next=Rich();next.inventory[0].key=L"newer";CHECK(I::ApplyToSaveObject(Save(),next));};CHECK(!I::ApplyToSaveObject(Save(),rich)&&Item().key==L"newer");
    puts("PASS staging lifetime: owned input, nested supersession, world/actor/class/gamemode/save replacement and reentrant array-header changes");

    Reset();I::LivePersonalStore store;CHECK(I::ReadLivePersonalStore(store)&&store.slotIndex==0&&store.records.size()==1&&I::LivePersonalStoreCount()==1);
    Set(&objects[3],0,int32_t(1));CHECK(!I::ReadLivePersonalStore(store)&&store.records.empty()&&store.slotIndex==-1&&I::LivePersonalStoreCount()==-1);
    Set(&objects[3],0,int32_t(0));Set(&objects[3],4,uint8_t(0x20));CHECK(!I::ReadLivePersonalStore(store)&&C::IsWorldInventory(&objects[3]));
    Set(&objects[3],4,uint8_t(0x24));CHECK(!C::IsWorldInventory(&objects[3])&&I::LivePersonalStoreCount()==1);
    ++objects[13].serial;boolReady=false;CHECK(!C::IsWorldInventory(&objects[3])&&I::LivePersonalStoreCount()==-1);boolReady=true;CHECK(I::LivePersonalStoreCount()==1);
    objects[3].cls=&objects[14];CHECK(!C::IsWorldInventory(&objects[3])&&I::LivePersonalStoreCount()==-1);
    puts("PASS personal/world boundary: actual packed Player bit, exact baked slot zero, unknown metadata/class refusal and matching count reader");

    Reset();CHECK(I::ResolveSaveSlot()==Save()&&C::InventoryOf(&objects[2])==&objects[3]&&C::OwnerOf(&objects[3])==&objects[2]);float volume=0;CHECK(C::CurrentVolume(&objects[3],volume)&&volume==12);
    // The very same class addresses acquire different fields on reincarnation.
    for(int i=10;i<=13;++i){++objects[i].serial;objects[i].bias=128;}
    Set(&objects[0],0,Save());Set(&objects[0],8,static_cast<void*>(&objects[2]));Set(&objects[2],0,static_cast<void*>(&objects[3]));
    Set(&objects[3],0,int32_t(0));Set(&objects[3],4,uint8_t(4));Set(&objects[3],8,static_cast<void*>(&objects[2]));Set(&objects[3],16,27.f);
    SR::WriteArrHeader(Save(),Field(Save(),32),stack,2);
    CHECK(I::ResolveSaveSlot()==Save()&&C::InventoryOf(&objects[2])==&objects[3]&&C::OwnerOf(&objects[3])==&objects[2]);
    CHECK(I::LivePersonalStoreCount()==1&&C::ContentsSlot(&objects[3])==stack&&C::CurrentVolume(&objects[3],volume)&&volume==27);
    metadata=false;++objects[10].serial;CHECK(!I::ResolveSaveSlot());metadata=true;++objects[10].serial;CHECK(I::ResolveSaveSlot()==Save());
    puts("PASS production InstanceOffset and bool-cache reincarnation: refreshed save/container/index/volume layouts and recovery from an old failed lookup");
    Reset();SR::Arr strict{};
    CHECK(SR::TryReadArr(stack,0,strict)&&strict.num==1);
    for(int mode=0;mode<5;++mode) {
        Reset();I::LivePersonalStore personal;I::PlayerInventory all;
        if(mode==0){int32_t n=-1;std::memcpy(stack+8,&n,4);}
        if(mode==1){int32_t max=0;std::memcpy(stack+12,&max,4);}
        if(mode==2){void* bad=nullptr;std::memcpy(stack,&bad,8);}
        if(mode==3){void* bad=reinterpret_cast<void*>(3);std::memcpy(stack,&bad,8);}
        if(mode==4){int32_t max=-1;std::memcpy(stack+12,&max,4);}
        CHECK(!I::ReadLivePersonalStore(personal)&&personal.records.empty()&&I::LivePersonalStoreCount()==-1&&!I::ReadAll(all)&&all.inventory.empty());
    }
    for(int off:{0x440,0x450}) {
        Reset();int32_t bad=-1;std::memcpy(static_cast<uint8_t*>(Save())+off+8,&bad,4);Images before;
        I::PlayerInventory all=Rich();CHECK(!I::ReadAll(all)&&all.inventory.empty()&&all.equipment.empty()&&all.hold.empty());
        CHECK(!I::ApplyToSaveObject(Save(),rich)&&before.Same());
    }
    Reset();SR::WriteArrHeader(stack,0,nullptr,0);I::LivePersonalStore personal;CHECK(I::ReadLivePersonalStore(personal)&&personal.records.empty()&&I::LivePersonalStoreCount()==0);
    Reset();int32_t zero=0;std::memcpy(stack+8,&zero,4);CHECK(SR::TryReadArr(stack,0,strict)&&strict.num==0&&strict.data==oldItem);
    puts("PASS strict outer headers: corrupt personal/equipment/hand/target data refuses; genuine and reserved empty arrays remain valid");
    Reset();CHECK(I::ApplyToSaveObject(Save(),rich));auto hand=SR::ReadArr(Save(),0x450);std::array<uint8_t,0x120> image{};std::memcpy(image.data(),hand.data,image.size());
    stringsReady=false;CHECK(!I::WriteHeldItem(L"changed",L"prop_sample_C")&&!std::memcmp(image.data(),hand.data,image.size()));stringsReady=true;
    onNative=[]{++world;};CHECK(!I::WriteHeldItem(L"changed",L"prop_sample_C")&&!std::memcmp(image.data(),hand.data,image.size()));world=1;
    CHECK(I::WriteHeldItem(L"changed",L"prop_sample_C"));I::EquipRecord changed;I::ReadEquipRecord(hand.data,changed);CHECK(changed.propName==L"changed"&&changed.data.names[0][0]==L"changed"&&changed.propKey==rich.hold[0].propKey&&changed.data.strings==rich.hold[0].data.strings);
    Reset();bool allowed=true;Images beforeGuard;onNative=[&]{allowed=false;};CHECK(!I::ApplyToSaveObjectWhile(Save(),rich,[&]{return allowed;})&&beforeGuard.Same());
    puts("PASS held replacement stages full data and refuses failed/stale writes; caller cancellation stops profile publication");
    Reset();C::RederiveShownState(&objects[2],&objects[3]);CHECK(volumeCalls==1&&nameCalls==1);
    for(int mode=0;mode<5;++mode){Reset();onVolume=[mode]{if(mode==0)++world;if(mode==1)++objects[3].serial;if(mode==2)Set(&objects[2],0,static_cast<void*>(&objects[5]));if(mode==3)Set(&objects[3],8,static_cast<void*>(&objects[5]));if(mode==4)++objects[27].serial;};C::RederiveShownState(&objects[2],&objects[3]);CHECK(volumeCalls==1&&nameCalls==1);}
    puts("PASS derived container state: names rebuild before volume/mass; retired callbacks never dispatch again");
    Reset();
    return 0;
}
