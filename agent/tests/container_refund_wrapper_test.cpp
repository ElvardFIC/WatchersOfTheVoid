// Refund wrapper regression: actual checked codec, array and instance-offset code.
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
std::function<void()> onNative,onAllocate,onFree;
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
    world=1;metadata=boolReady=classesReady=namesReady=stringsReady=signalsReady=true;allocations=failAt=decodes=0;onNative=onAllocate=onVolume=onFree={};volumeCalls=nameCalls=0;
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
void EngineFree(void* p){CHECK(test::owned.erase(p)==1);std::free(p);test::Callback(test::onFree);}
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
namespace ue_wrap::object_index {void* ClassByName(const wchar_t* n){if(!wcscmp(n,L"propInventory_C"))return &test::objects[13];if(!wcscmp(n,L"prop_container_C"))return &test::objects[12];return reflection::FindClass(n);}}
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
namespace refund_test {
using Record=SR::SaveRecord;
bool current=true;
const char* scenario="setup";
bool Equal(const Record& a,const Record& b){return a.className==b.className&&a.key==b.key&&a.xform==b.xform&&a.bools==b.bools&&a.floats==b.floats&&a.ints==b.ints&&a.strings==b.strings&&a.signals==b.signals&&a.classes==b.classes&&a.vectors==b.vectors&&a.rotators==b.rotators&&a.transforms==b.transforms&&a.bytes==b.bytes&&a.names==b.names;}
void* Inv(){return &test::objects[3];}
uint8_t* Slot(){return test::stack+16;}
Record Rich(const std::wstring& key=L"refund") {auto record=test::Rich().inventory[0];record.key=key;record.floats={{0.f,.25f}};return record;}
void Rows(const std::vector<Record>& records){auto* bytes=static_cast<uint8_t*>(SR::AllocZeroed(records.size(),SR::kSaveStride));CHECK(bytes||records.empty());for(size_t i=0;i<records.size();++i)CHECK(SR::WriteSaveRecordChecked(bytes+i*SR::kSaveStride,records[i]));SR::WriteArrHeader(Slot(),0,bytes,int32_t(records.size()));}
std::vector<Record> Rows(){std::vector<Record> out;CHECK(C::ReadContents(Inv(),out,128));return out;}
void Reset(){test::Reset();current=true;test::Set(Inv(),0,int32_t(1));test::Set(Inv(),4,uint8_t(0x20));Rows({Rich(L"survivor")});}
std::array<uint8_t,16> Header(){std::array<uint8_t,16> h{};std::memcpy(h.data(),Slot(),h.size());return h;}
bool Refund(const Record& record){return C::RefundRecordWhile(Inv(),record,[]{return current;});}
void RichRefund(){Reset();const auto survivor=Rows()[0],refund=Rich();CHECK(Refund(refund));const auto rows=Rows();CHECK(rows.size()==2&&Equal(rows[0],survivor)&&Equal(rows[1],refund));CHECK(test::volumeCalls==0&&test::nameCalls==0);Reset();auto nested=Rich();nested.className=L"prop_container_C";nested.ints={{18,7}};CHECK(Refund(nested));CHECK(Equal(Rows()[1],nested));}
void AllocationFailures(){
 Reset();const int begin=test::allocations;CHECK(Refund(Rich()));const int count=test::allocations-begin;CHECK(count>5);
 for(int step=1;step<=count;++step){Reset();const auto before=Header();const auto survivor=Rows()[0];test::failAt=test::allocations+step;CHECK(!Refund(Rich()));CHECK(Header()==before&&Equal(Rows()[0],survivor)&&Rows().size()==1);test::failAt=0;CHECK(Refund(Rich()));CHECK(Rows().size()==2);}
}
void ConstructionFailures(){for(int kind=0;kind<4;++kind){Reset();const auto before=Header();if(kind==0)test::classesReady=false;if(kind==1)test::namesReady=false;if(kind==2)test::stringsReady=false;if(kind==3)test::signalsReady=false;CHECK(!Refund(Rich()));CHECK(Header()==before&&Rows().size()==1);}}
void Deduplication(){
 Reset();auto existing=Rich();existing.floats={{.9f,0.f}};Rows({existing});const auto before=Header();const auto allocations=test::allocations;CHECK(Refund(Rich()));CHECK(Header()==before&&test::allocations==allocations&&Equal(Rows()[0],existing));
 auto wrong=Rich();wrong.className=L"prop_foreign_C";CHECK(!Refund(wrong));CHECK(Header()==before&&Equal(Rows()[0],existing));
 for(int kind=0;kind<3;++kind){Reset();auto bad=Rich();if(kind==0)bad.key.clear();if(kind==1)bad.key=L"None";if(kind==2)bad.className.clear();const auto header=Header();CHECK(!Refund(bad)&&Header()==header);}
}
void Retirement(){
 for(int kind=0;kind<13;++kind){Reset();const auto before=Header();test::onNative=[kind]{
  if(kind==0)++test::world;if(kind==1)++test::objects[1].serial;if(kind==2)++test::objects[2].serial;if(kind==3)++test::objects[3].serial;
  if(kind==4)++test::objects[11].serial;if(kind==5)++test::objects[12].serial;if(kind==6)++test::objects[13].serial;
  if(kind==7)test::Set(&test::objects[0],0,static_cast<void*>(&test::objects[4]));
  if(kind==8)test::Set(&test::objects[2],0,static_cast<void*>(&test::objects[5]));
  if(kind==9)test::Set(Inv(),8,static_cast<void*>(&test::objects[5]));if(kind==10)test::Set(Inv(),0,int32_t(0));
  if(kind==11)Rows({Rich(L"new source")});if(kind==12)current=false;
 };CHECK(!Refund(Rich()));if(kind!=11)CHECK(Header()==before);else CHECK(Rows().size()==1&&Rows()[0].key==L"new source");}
 Reset();const auto before=Header();test::onAllocate=[]{++test::world;};CHECK(!Refund(Rich())&&Header()==before);
}
void OwnedAndReentrant(){
 Reset();auto input=Rich();const auto expected=input;test::onNative=[&]{input={};};CHECK(Refund(input));CHECK(Equal(Rows()[1],expected));
 Reset();test::onNative=[]{CHECK(Refund(Rich(L"nested")));};CHECK(!Refund(Rich(L"outer")));const auto rows=Rows();CHECK(rows.size()==2&&rows[1].key==L"nested");
}
void LiveEdits(){
 Reset();auto original=Rows()[0],newer=original;newer.floats[0][0]=.75f;
 test::onNative=[&]{const auto array=SR::ReadArr(Slot(),0);CHECK(SR::UpdateScalarGroups(const_cast<uint8_t*>(array.data),original,newer));};
 CHECK(Refund(Rich()));CHECK(Equal(Rows()[0],newer));
 // An in-place return of this very identity during class/name construction is
 // already the refund. Do not append another row or overwrite its newer state.
 Reset();auto returned=Rows()[0];returned.key=L"refund";returned.floats[0][0]=.8f;
 const auto previous=Rows()[0];test::onNative=[&]{const auto array=SR::ReadArr(Slot(),0);auto* bytes=const_cast<uint8_t*>(array.data);CHECK(SR::UpdateScalarGroups(bytes,previous,returned));SR::WriteFNameField(bytes+0x40,returned.key);};
 CHECK(Refund(Rich()));CHECK(Rows().size()==1&&Equal(Rows()[0],returned));
}
void CleanupReentry(){Reset();test::onFree=[]{++test::world;current=false;};CHECK(Refund(Rich()));CHECK(Rows().size()==2&&Rows()[1].key==L"refund");current=true;const auto before=Header();CHECK(Refund(Rich())&&Header()==before&&Rows().size()==2);}
void Boundaries(){Reset();test::Set(Inv(),0,int32_t(0));const auto personal=SR::ReadArr(test::stack,0).data;CHECK(!Refund(Rich()));CHECK(SR::ReadArr(test::stack,0).data==personal);Reset();test::Set(Inv(),4,uint8_t(0x24));CHECK(!Refund(Rich()));}
}
int main(int argc,char** argv){using namespace refund_test;struct Case{const char* name;void(*run)();};const Case cases[]={{"rich-refund",RichRefund},{"allocation-failures",AllocationFailures},{"construction-failures",ConstructionFailures},{"deduplication",Deduplication},{"retirement",Retirement},{"owned-reentrant",OwnedAndReentrant},{"live-edits",LiveEdits},{"cleanup-reentry",CleanupReentry},{"boundaries",Boundaries}};
 bool ran=false;for(const auto& item:cases)if(argc<2||std::string(argv[1])==item.name){scenario=item.name;item.run();std::printf("PASS refund wrapper %s\n",scenario);ran=true;}CHECK(ran);test::Reset();std::puts("PASS actual container refund wrapper and checked save codec");return 0;}
