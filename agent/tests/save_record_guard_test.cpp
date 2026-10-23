// Production capture/apply/convention functions; native dispatch and frame I/O doubled.
#include "ue_wrap/actors/save_record.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <unordered_map>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int index,serial=1;bool live=true;};
Obj actor{1},cls{2},baseFn{3},getFn{4},loadFn{5};
Obj* objects[]={nullptr,&actor,&cls,&baseFn,&getFn,&loadFn};
uint32_t world=1;std::wstring key=L"item";
int bases=0,leaves=0,loads=0,writes=0,keyReads=0;bool lease=true,clash=false,captureOK=true,dispatchOK=true,raw=false;
std::function<void()> onBase,onLeaf,onWrite,onLoad,onFrame;
bool encodingOK=true;
void Callback(std::function<void()>& slot){auto fn=std::move(slot);slot={};if(fn)fn();}
}
namespace ue_wrap::reflection {
void* ClassOf(void*){return &test::cls;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->index:-1;}
int32_t SlotSerial(int32_t i){return i>0&&i<6?test::objects[i]->serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t i){return i>0&&i<6&&test::objects[i]==p&&test::objects[i]->live;}
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
const FName& NameOf(void*){static FName n{};return n;}
std::wstring ToString(const FName&){return L"prop_test_C";}
}
namespace ue_wrap::world_identity {
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* p){return p==&test::actor?CurrentWorld():nullptr;}
uint32_t Generation(){return test::world;}
}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){++test::keyReads;return test::key;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap {
class ParamFrame {
public:
 explicit ParamFrame(void* fn):fn_(fn){test::Callback(test::onFrame);}
 bool valid()const{return fn_!=nullptr;}
 int32_t ParamOffset(const wchar_t*)const{return 0;}
 int32_t FrameSize()const{return 256;}
 void* data(){return bytes_;}
 void* function()const{return fn_;}
private:void* fn_;alignas(16) uint8_t bytes_[256]{};
};
bool Call(void* actor,ParamFrame& frame){
 CHECK(actor==&test::actor&&test::actor.live&&frame.function()==&test::loadFn&&test::loadFn.live);
 ++test::loads;test::Callback(test::onLoad);return test::dispatchOK;
}
}
namespace ue_wrap::save_record {
namespace R=ue_wrap::reflection;
#include "../../build/save-record-check/context.inc"
struct ClassVerbs {void* getData=nullptr;void* loadData=nullptr;bool overrides=false,conventionChecked=false,conventionChecking=false;};
std::unordered_map<void*,ClassVerbs> g_verbs;
void* g_baseGetData=&test::baseFn;
void* g_propCls=nullptr;uint64_t g_nextPropTryMs=0;
bool EnsurePropClass(){return true;}
const ClassVerbs* VerbsFor(void* cls){return &g_verbs.at(cls);}
bool CallForRecord(void* actor,void* fn,SaveRecord& out){
 CHECK(actor==&test::actor&&test::actor.live&&fn&&static_cast<test::Obj*>(fn)->live);
 out={};out.key=test::key;out.className=L"prop_test_C";out.bools={{1}};out.ints={{7}};
 if(fn==&test::baseFn){++test::bases;test::Callback(test::onBase);}
 else {CHECK(fn==&test::getFn);++test::leaves;if(test::clash)out.ints[0][0]=8;test::Callback(test::onLeaf);}
 return test::captureOK;
}
void WriteSaveRecord(uint8_t*,const SaveRecord& record,ClassResolver resolve){
 ++test::writes;
 if(test::raw)CHECK(record.key==L"foreign"&&record.bools[0][0]==0&&record.ints[0]==std::vector<int32_t>({2,99}));
 else CHECK(record.key==L"item"&&record.bools[0][0]==1&&record.ints[0]==std::vector<int32_t>({7,99}));
 if(resolve)resolve(L"nested_class");test::Callback(test::onWrite);
}
bool WriteSaveRecordChecked(uint8_t* at,const SaveRecord& record,ClassResolver resolve){WriteSaveRecord(at,record,resolve);return test::encodingOK;}
#include "../../build/save-record-check/functions.inc"
}
namespace S=ue_wrap::save_record;
namespace test {
void Reset(){
 for(auto* obj:objects)if(obj){obj->serial=1;obj->live=true;}
 world=1;key=L"item";bases=leaves=loads=writes=keyReads=0;lease=captureOK=dispatchOK=true;clash=raw=false;
 onBase=onLeaf=onWrite=onLoad=onFrame={};encodingOK=true;S::g_baseGetData=&baseFn;S::g_verbs.clear();
 S::g_verbs[&cls]={&getFn,&loadFn,true,true};
}
S::SaveRecord Incoming(){S::SaveRecord r;r.key=L"foreign";r.bools={{0}};r.ints={{2,99}};return r;}
void Invalidate(int reason){
 if(reason==0)actor.live=false;if(reason==1)++actor.serial;if(reason==2)++world;
 if(reason==3)key=L"new_item";if(reason==4)++loadFn.serial;
#if GUARDED_CODEC
 if(reason==5)S::ResetCodecCache();
 if(reason==6)++cls.serial;if(reason==7)++baseFn.serial;
#endif
}
}
int main(){using namespace test;
 Reset();CHECK(S::ApplyRecord(&actor,Incoming(),nullptr)&&bases==1&&writes==1&&loads==1);
 // A live actor can acquire another key while native getData is running.
 Reset();onBase=[]{key=L"new_item";};
 CHECK(!S::ApplyRecord(&actor,Incoming(),nullptr)&&!writes&&!loads);
#if GUARDED_CODEC
 Reset();S::g_verbs.at(&cls)={};S::SaveRecord unsupported;
 CHECK(!S::ApplyRecord(&actor,Incoming(),nullptr)&&!S::CaptureRecord(&actor,unsupported)&&!keyReads&&!bases&&!leaves&&!loads);
 for(int reason=0;reason<8;++reason){
  Reset();onBase=[=]{Invalidate(reason);};CHECK(!S::ApplyRecord(&actor,Incoming(),nullptr)&&!writes&&!loads);
  Reset();onWrite=[=]{Invalidate(reason);};CHECK(!S::ApplyRecord(&actor,Incoming(),nullptr)&&writes==1&&!loads);
 }
 // The caller's row/authority revision reaches every native boundary.
 for(int stage=0;stage<3;++stage){
  Reset();const auto cancel=[]{lease=false;};
  if(stage==0)lease=false;if(stage==1)onBase=cancel;if(stage==2)onWrite=cancel;
  CHECK(!S::ApplyRecordWhile(&actor,Incoming(),[]{return lease;},nullptr)&&!loads);
 }
 Reset();CHECK(!S::ApplyRecordWhile(&actor,Incoming(),{},nullptr)&&!bases&&!writes&&!loads);
 Reset();CHECK(S::ApplyRecordWhile(&actor,Incoming(),[]{return lease;},nullptr)&&loads==1);
 Reset();CHECK(S::ApplyRecord(&actor,Incoming(),[](const wchar_t*)->void*{lease=false;return nullptr;})&&loads==1);
 // The same resolver invalidation must stop a guarded dispatch.
 Reset();CHECK(!S::ApplyRecordWhile(&actor,Incoming(),[]{return lease;},[](const wchar_t*)->void*{lease=false;return nullptr;})&&!loads);
 // First-use convention reads cannot continue on a retired actor/cache/function.
 for(int stage=0;stage<2;++stage)for(int reason=0;reason<8;++reason){
  Reset();S::g_verbs.at(&cls).conventionChecked=false;
  auto cancel=[=]{Invalidate(reason);};if(stage==0)onBase=cancel;else onLeaf=cancel;
  CHECK(!S::ApplyRecord(&actor,Incoming(),nullptr)&&!writes&&!loads);
  if(reason!=5)CHECK(!S::g_verbs.at(&cls).conventionChecked&&!S::g_verbs.at(&cls).conventionChecking);
 }
 // Interrupted/failed convention probes are retried; they never certify a class.
 Reset();S::g_verbs.at(&cls).conventionChecked=false;captureOK=false;
 CHECK(!S::ApplyRecord(&actor,Incoming(),nullptr)&&!S::g_verbs.at(&cls).conventionChecked&&!S::g_verbs.at(&cls).conventionChecking);
 captureOK=true;CHECK(S::ApplyRecord(&actor,Incoming(),nullptr)&&bases==3&&leaves==1&&loads==1);
 Reset();S::g_verbs.at(&cls).conventionChecked=false;
 onBase=[]{S::SaveRecord nested;CHECK(!S::CaptureRecord(&actor,nested));};
 CHECK(S::ApplyRecord(&actor,Incoming(),nullptr)&&bases==2&&leaves==1&&loads==1);
 Reset();S::g_verbs.at(&cls).conventionChecked=false;clash=true;
 CHECK(!S::ApplyRecord(&actor,Incoming(),nullptr)&&!S::g_verbs.at(&cls).overrides&&!loads);
 Reset();S::g_verbs.at(&cls).conventionChecked=false;
 onBase=[]{for(int i=10;i<1000;++i)S::g_verbs[reinterpret_cast<void*>(uintptr_t(i))]={};};
 CHECK(S::ApplyRecord(&actor,Incoming(),nullptr)&&loads==1);
 Reset();S::SaveRecord replaced;onLeaf=[]{++getFn.serial;};CHECK(!S::CaptureRecord(&actor,replaced));
 for(int reason:{0,1,2,3,5}){
  Reset();S::SaveRecord out;onLeaf=[=]{Invalidate(reason);};
  CHECK(!S::CaptureRecord(&actor,out)&&leaves==1);
 }
 Reset();S::SaveRecord out;CHECK(S::CaptureRecord(&actor,out)&&out.key==L"item");
 Reset();dispatchOK=false;CHECK(!S::ApplyRecord(&actor,Incoming(),nullptr));
 Reset();onLoad=[]{++world;};CHECK(!S::ApplyRecord(&actor,Incoming(),nullptr)&&loads==1);
 // Explicit-function restores must guard lifetime while allowing the saved key to replace it.
 Reset();raw=true;onWrite=[]{++world;};
 CHECK(!S::ApplyRecordVia(&actor,&loadFn,Incoming(),nullptr)&&!loads);
 Reset();raw=true;onLoad=[]{key=L"foreign";};
 CHECK(S::ApplyRecordVia(&actor,&loadFn,Incoming(),nullptr)&&loads==1&&!keyReads&&!bases);
 for(int reason:{0,1,2,4,5,6}){
  Reset();raw=true;onWrite=[=]{Invalidate(reason);};
  CHECK(!S::ApplyRecordVia(&actor,&loadFn,Incoming(),nullptr)&&!loads);
 }
 Reset();S::SaveRecord retired;onLeaf=[]{++world;};
 CHECK(!S::CaptureRecordVia(&actor,&getFn,retired));
#if RAW_CODEC
 Reset();raw=true;encodingOK=false;CHECK(!S::ApplyRecordViaWhile(&actor,&loadFn,Incoming(),[]{return lease;},nullptr)&&!loads);
 Reset();raw=true;onFrame=[]{++world;};CHECK(!S::ApplyRecordViaWhile(&actor,&loadFn,Incoming(),[]{return lease;},nullptr)&&!writes&&!loads);
 Reset();raw=true;auto owned=Incoming();onFrame=[&]{owned.key=L"reentered input";};CHECK(S::ApplyRecordViaWhile(&actor,&loadFn,owned,[]{return lease;},nullptr)&&loads==1);
 Reset();raw=true;CHECK(!S::ApplyRecordViaWhile(&actor,&loadFn,Incoming(),{},nullptr)&&!writes&&!loads);
 Reset();raw=true;onWrite=[]{lease=false;};
 CHECK(!S::ApplyRecordViaWhile(&actor,&loadFn,Incoming(),[]{return lease;},nullptr)&&!loads);
 Reset();raw=true;onLoad=[]{key=L"foreign";};
 CHECK(S::ApplyRecordViaWhile(&actor,&loadFn,Incoming(),[]{return lease;},nullptr)&&loads==1&&!keyReads);
 Reset();S::SaveRecord rawRecord;CHECK(S::CaptureRecordVia(&actor,&getFn,rawRecord)&&!keyReads);
#endif
#endif
 puts("PASS codec refuses stale actor/key/world/function/cache and caller revisions before native loadData");
}
