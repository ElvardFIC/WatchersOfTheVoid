#include "../../src/votv-coop/src/ue_wrap/actors/inventory.cpp"
#include "../../src/votv-coop/src/ue_wrap/actors/container_view.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
using Record=ue_wrap::save_record::SaveRecord;
Record written;
struct Obj {bool live=true;void* screen=nullptr;void* player=nullptr;void* active=nullptr;void* entered=nullptr;} gm,screen,player,box,other;
int fn=0,calls=0;bool haveFn=true;
}
namespace ue_wrap::save_record {
bool UpdateScalarGroups(void*,const SaveRecord&,const SaveRecord&){std::abort();}
void WriteFNameField(void*,const std::wstring&){}
void WriteSaveRecord(uint8_t*,const SaveRecord& r,ClassResolver){test::written=r;}
bool WriteSaveRecordChecked(uint8_t* at,const SaveRecord& r,ClassResolver resolve){WriteSaveRecord(at,r,resolve);return true;}
Arr ReadArr(const void*,int32_t){std::abort();}
bool TryReadArr(const void*,int32_t,Arr&){std::abort();}
void WriteArrHeader(void*,int32_t,void*,int32_t){std::abort();}
void* AllocZeroed(size_t,size_t){std::abort();}
std::wstring ReadFNameAt(const void*,int32_t){std::abort();}
void ReadSaveRecord(const void*,SaveRecord&){std::abort();}
}
namespace ue_wrap::log {void Write(Level,const char*,...){}}
namespace ue_wrap::container_inventory {bool ReadPersonalFlag(void*,bool&){std::abort();}}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t*){std::abort();}}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void*){std::abort();}int32_t SlotSerial(int32_t){std::abort();}
int32_t AllocateSlotSerial(int32_t){std::abort();}bool IsLiveByIndex(void*,int32_t){std::abort();}
void* FindClass(const wchar_t*){std::abort();}void* FindFunction(void*,const wchar_t*){std::abort();}
bool IsDescendantOfAny(void*,void* const*,size_t,int){std::abort();}
void EngineFree(void*){std::abort();}void* EngineAlloc(size_t,uint32_t){std::abort();}
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
void* ClassOf(void* a){return a;}
int32_t InstanceOffset::Of(void*){std::abort();}
int32_t FindPropertyOffset(void*,const wchar_t* n){const std::wstring name=n;
 if(name==L"propInventory")return offsetof(test::Obj,screen);
 if(name==L"mainPlayer")return offsetof(test::Obj,player);
 if(name==L"activeInterface")return offsetof(test::Obj,active);
 if(name==L"entered")return offsetof(test::Obj,entered);return -1;}
void* FindDispatchFunctionCached(void*,const wchar_t* n){CHECK(std::wstring(n)==L"gen");return test::haveFn?&test::fn:nullptr;}
}
namespace ue_wrap::world_singleton {void* Gamemode(){return &test::gm;}}
namespace ue_wrap::world_identity {uint32_t Generation(){std::abort();}void* CurrentWorld(){std::abort();}void* WorldOf(void*){std::abort();}}
namespace ue_wrap::building_material {
bool RefreshHeldMaterialRecord(void*,save_record::SaveRecord&){std::abort();}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* f):fn_(f){}
bool ParamFrame::SetRaw(const wchar_t*,const void*,int32_t){std::abort();}
bool Call(void* obj,ParamFrame&){CHECK(obj==&test::screen);++test::calls;return true;}
}
int main(){using namespace test;
 // Native AddEquipment searches for a whole empty struct, with this identity transform.
 const std::array<float,10> nativeEmpty{0,0,0,1,0,0,0,1,1,1};
 uint8_t buf[0x120]{};ue_wrap::inventory::EquipRecord rec;
 ue_wrap::inventory::WriteEquipRecord(buf,rec);CHECK(written.xform==nativeEmpty);
 rec.data.xform={};ue_wrap::inventory::WriteEquipRecord(buf,rec);CHECK(written.xform==nativeEmpty);
 // Existing item data and nonempty slots never get replaced by an empty record.
 rec.propName=L"backpack";rec.data.className=L"prop_backpack_C";rec.data.floats={{.25f}};
 ue_wrap::inventory::WriteEquipRecord(buf,rec);CHECK((written.xform==std::array<float,10>{}));CHECK(written.floats==rec.data.floats);
 gm.screen=&screen;gm.player=&player;screen.entered=&box;
 CHECK(!ue_wrap::container_view::Refresh(&box));CHECK(calls==0); // closed
 player.active=&screen;CHECK(!ue_wrap::container_view::Refresh(&other));CHECK(calls==0);
 CHECK(ue_wrap::container_view::Refresh(&box));CHECK(calls==1);
 screen.live=false;CHECK(!ue_wrap::container_view::Refresh(&box));screen.live=true;
 box.live=false;CHECK(!ue_wrap::container_view::Refresh(&box));box.live=true;
 haveFn=false;CHECK(!ue_wrap::container_view::Refresh(&box));CHECK(calls==1);
 puts("PASS native empty equipment equality, old-profile repair, occupied data preservation, active matching container refresh only");
}
