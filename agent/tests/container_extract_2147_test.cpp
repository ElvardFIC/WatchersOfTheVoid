#ifndef CONTAINER_RESTORE_SOURCE
#define CONTAINER_RESTORE_SOURCE "../../src/votv-coop/src/coop/items/container_extract_restore.h"
#endif
#include CONTAINER_RESTORE_SOURCE
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
using Record=ue_wrap::save_record::SaveRecord;
struct Obj{int id;int serial=1;bool live=true;} fn{0},owner{1},actor{2},other{3};
Obj* objects[]{&fn,&owner,&actor,&other};
Record source,live;
bool layout=true,apply=true;int calls=0;
uint32_t world=1;std::function<void()> duringDispatch,afterApply;
alignas(16) uint8_t locals[0x110]{};
void Reset(){source={};source.className=L"prop_batts_C";source.key=L"original-key";source.floats={{0},{.25f}};
 live={};live.className=source.className;live.key=L"temporary-key";live.floats={{0},{1.f}};
 layout=apply=true;calls=0;world=1;duringDispatch=afterApply={};
 for(auto* p:objects)p->live=true;
 ++fn.serial;void* p=&actor;std::memcpy(locals+0x100,&p,sizeof(p));}
}
namespace ue_wrap::reflection {
const FName& NameOf(void* p){static FName names[]{{1,0},{2,0},{3,0},{4,0}};return names[static_cast<test::Obj*>(p)->id];}
std::wstring ToString(const FName& n){return n.ComparisonIndex==1?L"extract":n.ComparisonIndex==2?L"prop_container_C":L"unrelated";}
void* OuterOf(void*){return &test::owner;}void* ClassOf(void* p){return p;}
std::wstring ClassNameOf(void*){return L"prop_batts_C";}
int32_t InternalIndexOf(void* p){return static_cast<test::Obj*>(p)->id;}
int32_t SlotSerial(int32_t i){return test::objects[i]->serial;}int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<4&&p==test::objects[i]&&test::objects[i]->live;}
int32_t FunctionFrameSize(void*){return sizeof(test::locals);}
std::vector<StructFieldInfo> EnumerateStructFields(void*){return {{L"CallFunc_takeObj_Output",test::layout?0:0x108,0x100,0},{L"CallFunc_FinishSpawningActor_ReturnValue",0x100,8,0}};}
void* FindDispatchFunctionCached(void*,const wchar_t*){return &test::fn;}
}
namespace ue_wrap::world_identity {void* WorldOf(void*){return nullptr;}void* CurrentWorld(){return nullptr;}uint32_t Generation(){return test::world;}}
namespace ue_wrap::save_record {
void ReadSaveRecord(const void* p,SaveRecord& r){CHECK(p==test::locals);r=test::source;}
bool ApplyRecordVia(void* a,void*,const SaveRecord& r,ClassResolver){
 auto cb=std::move(test::duringDispatch);test::duringDispatch={};if(cb)cb();
 CHECK(a==&test::actor);++test::calls;if(test::apply)test::live=r;
 cb=std::move(test::afterApply);test::afterApply={};if(cb)cb();return test::apply;
}
bool ApplyRecordViaWhile(void* a,void* fn,const SaveRecord& r,const std::function<bool()>& current,ClassResolver resolve){
 auto cb=std::move(test::duringDispatch);test::duringDispatch={};if(cb)cb();
 return current && current() && ApplyRecordVia(a,fn,r,resolve);
}
}
int main(){using namespace test;namespace D=coop::inventory_pickup_sync::detail;
 ue_wrap::script_gate::Call c{};c.callerObject=&owner;c.callerFunction=&fn;c.callerLocals=locals;
 for(float charge:{.25f,0.f}){Reset();source.floats[1][0]=charge;
  CHECK(D::RestoreContainerCarrier(c,&actor)==D::ExtractRestore::Restored);
  CHECK(calls==1&&live.key==source.key&&live.floats==source.floats);
  // Both the next pocket capture and a surviving actor on a full inventory now hold this value.
  auto pocketCapture=live;CHECK(pocketCapture.floats[1][0]==charge);}
 Reset();CHECK(D::RestoreContainerCarrier(c,&other)==D::ExtractRestore::Unrelated&&calls==0);
 Reset();c.callerFunction=&other;CHECK(D::RestoreContainerCarrier(c,&actor)==D::ExtractRestore::Unrelated&&calls==0);c.callerFunction=&fn;
 Reset();layout=false;CHECK(D::RestoreContainerCarrier(c,&actor)==D::ExtractRestore::Failed&&calls==0);
 Reset();source.className=L"different_C";CHECK(D::RestoreContainerCarrier(c,&actor)==D::ExtractRestore::Failed&&calls==0);
 Reset();source.key.clear();CHECK(D::RestoreContainerCarrier(c,&actor)==D::ExtractRestore::Failed&&calls==0);
 Reset();apply=false;CHECK(D::RestoreContainerCarrier(c,&actor)==D::ExtractRestore::Failed);
 for(int cause=0;cause<8;++cause){
  Reset();duringDispatch=[=]{
   if(cause==0)owner.live=false;if(cause==1)++owner.serial;
   if(cause==2)actor.live=false;if(cause==3)++actor.serial;
   if(cause==4)fn.live=false;if(cause==5)++world;if(cause==7)++fn.serial;
   if(cause==6){void* p=&other;std::memcpy(locals+0x100,&p,sizeof(p));}
  };
  CHECK(D::RestoreContainerCarrier(c,&actor)==D::ExtractRestore::Invalidated&&calls==0&&live.key==L"temporary-key");
 }
 Reset();afterApply=[]{owner.live=false;};
 CHECK(D::RestoreContainerCarrier(c,&actor)==D::ExtractRestore::Invalidated&&calls==1&&live.key==source.key);
 Reset();CHECK(D::RestoreContainerCarrier(c,&actor)==D::ExtractRestore::Restored&&calls==1);
 puts("PASS scoped container extraction: saved partial/zero state and key, unrelated carrier/caller, frame bounds, class/key validation, native failure");
}
