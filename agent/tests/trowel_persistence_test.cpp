#include "../../src/votv-coop/src/ue_wrap/actors/inventory.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int id=0,serial=1;bool live=true;void* save=nullptr;} objects[4];
alignas(16) uint8_t held[0x120]{};
uint32_t world=1;int units=5,writes=0;bool present=true,writable=true,matching=true;
std::wstring key=L"tool",outerKey=L"tool";
ue_wrap::save_record::SaveRecord record;
std::function<void()> onRefresh,onDecode;
std::function<void(ue_wrap::save_record::SaveRecord&)> refreshFields;
void Reset(){units=5;writes=0;present=writable=matching=true;key=outerKey=L"tool";world=1;
 for(int i=0;i<4;++i){objects[i]={};objects[i].id=i;}objects[0].save=&objects[1];
 std::memset(held,0,sizeof(held));record={};record.className=L"prop_wallfixer_C";record.key=L"tool";
 record.ints={{5}};record.names={{L"wallfixer"}};record.strings={{L"keep"}};onRefresh={};onDecode={};refreshFields={};}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<4&&p==&test::objects[i]&&IsLive(p);}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void* p){return p;}
int32_t FindPropertyOffset(void*,const wchar_t*){return offsetof(test::Obj,save);}
int32_t InstanceOffset::Of(void* p){return p?FindPropertyOffset(ClassOf(p),prop_):-1;}
void* FindClass(const wchar_t*){std::abort();}
void EngineFree(void*){std::abort();}void* EngineAlloc(size_t,uint32_t){std::abort();}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return CurrentWorld();}
}
namespace ue_wrap::world_singleton {void* Gamemode(){return &test::objects[0];}}
namespace ue_wrap::save_record {
void WriteArrHeader(void*,int32_t,void*,int32_t){std::abort();}void* AllocZeroed(size_t,size_t){std::abort();}
void WriteFNameField(void*,const std::wstring&){std::abort();}
void WriteSaveRecord(uint8_t*,const SaveRecord&,ClassResolver){std::abort();}
bool WriteSaveRecordChecked(uint8_t*,const SaveRecord&,ClassResolver){std::abort();}
Arr ReadArr(const void* p,int32_t off){CHECK(p==&test::objects[1]&&off==0x450);return test::present?Arr{test::held,1}:Arr{};}
bool TryReadArr(const void* p,int32_t off,Arr& out){out=ReadArr(p,off);return true;}
std::wstring ReadFNameAt(const void*,int32_t off){return off==8?test::outerKey:off==0?L"wallfixer":L"tag";}
void ReadSaveRecord(const void*,SaveRecord& out){out=test::record;auto cb=std::move(test::onDecode);test::onDecode={};if(cb)cb();}
bool UpdateScalarGroups(void* p,const SaveRecord& before,const SaveRecord& after){CHECK(p==test::held+0x10);
 if(!test::writable||test::record.ints!=before.ints||test::record.floats!=before.floats||test::record.bools!=before.bools)return false;
 const auto sizes=[](const auto& a,const auto& b){if(a.size()!=b.size())return false;
  for(size_t i=0;i<a.size();++i)if(a[i].size()!=b[i].size())return false;return true;};
 if(!sizes(before.ints,after.ints)||!sizes(before.floats,after.floats)||!sizes(before.bools,after.bools))return false;
 ++test::writes;test::record.ints=after.ints;test::record.floats=after.floats;test::record.bools=after.bools;return true;}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::container_inventory {bool ReadPersonalFlag(void*,bool&){std::abort();}}
namespace ue_wrap::building_material {
bool IsTrowel(void* p){return p==&test::objects[2];}
bool ReadConcrete(void*,int32_t& out){out=test::units;return out>=0&&out<=5;}
bool RefreshTrowelRecord(void*,save_record::SaveRecord& out){
 if(!test::matching||out.key!=test::key||out.className!=L"prop_wallfixer_C")return false;
 out.ints={{test::units}};auto cb=std::move(test::onRefresh);test::onRefresh={};if(cb)cb();return true;}
bool RefreshHeldMaterialRecord(void* actor,save_record::SaveRecord& out){
 if(out.className==L"prop_wallfixer_C")return RefreshTrowelRecord(actor,out);
 if(!test::matching||out.key!=test::key||!test::refreshFields)return false;
 test::refreshFields(out);auto cb=std::move(test::onRefresh);test::onRefresh={};if(cb)cb();return true;}
}
int main(){using namespace test;const auto PersistHeldTrowelStock=[](void* actor,const std::function<bool()>& current={}){
 return ue_wrap::inventory::PersistHeldMaterialStock(actor,current);};
 Reset();auto before=record;for(int value:{4,0,5}){units=value;CHECK(PersistHeldTrowelStock(&objects[2],{}));CHECK(record.ints[0][0]==value);}
 CHECK(record.names==before.names&&record.strings==before.strings&&record.key==before.key&&record.xform==before.xform);
 Reset();key=L"other";CHECK(!PersistHeldTrowelStock(&objects[2])&&writes==0);
 Reset();outerKey=L"other";CHECK(!PersistHeldTrowelStock(&objects[2])&&writes==0);
 Reset();record.className=L"prop_bucket_C";CHECK(!PersistHeldTrowelStock(&objects[2])&&writes==0);
 Reset();present=false;CHECK(!PersistHeldTrowelStock(&objects[2])&&writes==0);
 Reset();writable=false;CHECK(!PersistHeldTrowelStock(&objects[2])&&writes==0);
 Reset();onRefresh=[] {++objects[2].serial;};CHECK(!PersistHeldTrowelStock(&objects[2])&&writes==0);
 Reset();onRefresh=[] {++world;};CHECK(!PersistHeldTrowelStock(&objects[2])&&writes==0);
 Reset();onRefresh=[] {++held[0];};CHECK(!PersistHeldTrowelStock(&objects[2])&&writes==0);
 Reset();onRefresh=[] {units=1;};CHECK(!PersistHeldTrowelStock(&objects[2])&&writes==0);
 Reset();onDecode=[] {objects[0].save=&objects[3];};CHECK(!PersistHeldTrowelStock(&objects[2])&&writes==0);
 Reset();CHECK(!PersistHeldTrowelStock(&objects[2],[]{return false;})&&writes==0);
 for(const auto* cls:{L"prop_bucket_C",L"prop_concreteBucket_C",L"prop_cementBag_C",L"prop_sponge_C"}) {
  Reset();record.className=cls;record.bools={{1,0,1},{1}};record.floats={{100,200},{38,23}};
  refreshFields=[](auto& out){out.ints={{0}};out.floats[1]={0,0};out.bools[1]={0};};
  const auto saved=record;
  CHECK(ue_wrap::inventory::PersistHeldMaterialStock(&objects[2])&&writes==1);
  CHECK(record.ints[0][0]==0&&record.floats[1][0]==0&&record.bools[1][0]==0);
  CHECK(record.floats[0]==saved.floats[0]&&record.bools[0]==saved.bools[0]&&record.names==saved.names&&record.strings==saved.strings);
  onRefresh=[] {++objects[2].serial;};CHECK(!ue_wrap::inventory::PersistHeldMaterialStock(&objects[2])&&writes==1);
 }
 Reset();record.className=L"unrelated_C";CHECK(!ue_wrap::inventory::PersistHeldMaterialStock(&objects[2])&&writes==0);
 Reset();record.className=L"prop_sponge_C";record.floats={{100},{1,5}};
 refreshFields=[](auto& out){out.floats[1]={.5f,0};};
 onRefresh=[] {refreshFields=[](auto& out){out.floats[1]={.25f,0};};};
 CHECK(!ue_wrap::inventory::PersistHeldMaterialStock(&objects[2])&&writes==0);
 Reset();record.className=L"prop_sponge_C";record.floats={{100},{1,5}};
 refreshFields=[](auto& out){out.floats[1]={0,0};};onRefresh=[] {record.floats[1][1]=2;};
 CHECK(!ue_wrap::inventory::PersistHeldMaterialStock(&objects[2])&&writes==0&&record.floats[1][1]==2);
 Reset();record.className=L"prop_bucket_C";refreshFields=[](auto& out){out.floats={{0}};};
 CHECK(!ue_wrap::inventory::PersistHeldMaterialStock(&objects[2])&&writes==0);
 puts("PASS trowel native hold persistence: zero/refill, exact inner/outer keys and class, no slot allocation, unchanged unrelated data, missing schema, hand/slot/world/serial and reentrant stock refusal");
 puts("PASS held material persistence: water/concrete/bag/sponge, zero stock, unchanged base groups, stock/slot reentry and schema refusal");
}
