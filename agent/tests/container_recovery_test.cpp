// LOCAL extraction escrow only: actual production controller, native-like VM
// ordering, engine objects/loadData/refund edge doubled. Wire arbitration and
// saved-world reconstruction are deliberately outside this fixture.
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/gc_pin.h"
#include "ue_wrap/actors/save_record.h"
#include "ue_wrap/actors/container_inventory.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
namespace test {
using Record=ue_wrap::save_record::SaveRecord;
const char* scenario="setup";
struct Obj{int id,serial=1;bool live=true;int pins=0;std::wstring name;Obj* cls=nullptr;Obj* outer=nullptr;Record state;};
std::deque<Obj> objects;std::vector<Obj*> slots{nullptr};
Obj *containerClass=nullptr,*inventoryClass=nullptr,*itemClass=nullptr,*container=nullptr,*inventory=nullptr,*save=nullptr,*player=nullptr,*extractFn=nullptr,*takeFn=nullptr,*pocketFn=nullptr,*loadFn=nullptr;
Obj* actualInventory=nullptr;Obj* actualOwner=nullptr;Obj* actualSave=nullptr;
struct Hook{int tag;ue_wrap::script_gate::PreFn pre;ue_wrap::script_gate::PostFn post;};
std::map<std::wstring,Hook> hooks;std::set<uint64_t> active;uint64_t sequence=0;
std::vector<Record> rows,pocket;std::map<const void*,Record> recordImages;
alignas(16) std::array<uint8_t,32*0x100> nativeRows{},replacementRows{};
alignas(16) std::array<uint8_t,16> header{};
uint32_t world=1,serial=1;uint64_t now=1000;
coop::net::Session session,other,third;coop::net::Session* installed=nullptr;
bool applyOK=true,refundOK=true,destroyOK=true;
int loads=0,refunds=0,destroys=0,derived=0,notifications=0;
std::function<void()> onLoad,onRefund,onDestroy,onDerive;
void Fail(int line,const char* expression){std::fprintf(stderr,"FAIL %s line %d: %s\n",scenario,line,expression);std::exit(1);}
Obj* Make(const std::wstring& name,Obj* cls=nullptr){objects.push_back({int(slots.size())});auto* p=&objects.back();p->name=name;p->cls=cls?cls:p;slots.push_back(p);return p;}
bool Equal(const Record&a,const Record&b){return a.className==b.className&&a.key==b.key&&a.xform==b.xform&&a.bools==b.bools&&a.floats==b.floats&&a.ints==b.ints&&a.strings==b.strings&&a.signals==b.signals&&a.classes==b.classes&&a.vectors==b.vectors&&a.rotators==b.rotators&&a.transforms==b.transforms&&a.bytes==b.bytes&&a.names==b.names;}
Record Rich(const std::wstring& key=L"saved"){Record r;r.className=L"prop_sample_C";r.key=key;r.bools={{1,0},{}};r.floats={{0.f,.25f}};r.ints={{7,2}};r.strings={{L"saved text"}};r.names={{L"saved name"}};r.classes={{L"prop_sample_C"}};r.vectors={{{1,2,3}}};r.rotators={{{4,5,6}}};r.transforms={{{0,0,0,1,2,3,4,1,1,1}}};r.bytes={{3,4}};ue_wrap::signal_dynamic::Row signal;signal.name=L"signal";signal.signal=L"template";signal.size=12;r.signals.push_back(signal);return r;}
void Sync(){void* data=rows.empty()?nullptr:nativeRows.data();int32_t count=int32_t(rows.size());std::memcpy(header.data(),&data,8);std::memcpy(header.data()+8,&count,4);std::memcpy(header.data()+12,&count,4);for(size_t i=0;i<rows.size();++i)recordImages[nativeRows.data()+i*0x100]=rows[i];}
void Callback(std::function<void()>& source){auto callback=std::move(source);source={};if(callback)callback();}
}
#define CHECK(expression) do{if(!(expression))test::Fail(__LINE__,#expression);}while(0)
#include "facts_test_support.h"
#ifndef CONTAINER_RECOVERY_SOURCE
#define CONTAINER_RECOVERY_SOURCE "../../src/votv-coop/src/coop/items/container_extract_recovery.cpp"
#endif
#include CONTAINER_RECOVERY_SOURCE
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
bool IsLiveByIndex(void* p,int32_t i){return i>0&&size_t(i)<test::slots.size()&&test::slots[i]==p&&test::slots[i]->live;}
int32_t SlotSerial(int32_t i){return i>0&&size_t(i)<test::slots.size()?test::slots[i]->serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void* p){return p?static_cast<test::Obj*>(p)->cls:nullptr;}
void* OuterOf(void* p){return p?static_cast<test::Obj*>(p)->outer:nullptr;}
const FName& NameOf(void* p){static FName name{};name.ComparisonIndex=static_cast<test::Obj*>(p)->id;name.Number=0;return name;}
std::wstring ToString(const FName& name){return test::slots.at(name.ComparisonIndex)->name;}
std::wstring ClassNameOf(void* p){return static_cast<test::Obj*>(p)->cls->name;}
int32_t FunctionFrameSize(void* fn){return fn==test::pocketFn?16:0x130;}
int32_t FindParamOffset(void* fn,const wchar_t* name){std::wstring n=name;if(n==L"index"||n==L"InputPin")return 0;if(n==L"removeVol")return 4;if(n==L"return")return fn==test::pocketFn?8:5;if(n==L"Output")return 0x10;if(n==L"object")return 0x110;return -1;}
std::vector<StructFieldInfo> EnumerateStructFields(void*){return {{L"CallFunc_takeObj_Output",0x10,0x100,0},{L"CallFunc_FinishSpawningActor_ReturnValue",0x110,8,0},{L"CallFunc_putObjectInventory2_return",0x118,1,0},{L"CallFunc_BeginDeferredActorSpawnFromClass_ReturnValue",0x120,8,0},{L"CallFunc_takeObj_return",0x128,1,0}};}
void* FindDispatchFunctionCached(void*,const wchar_t* name){return !wcscmp(name,L"loadData")?test::loadFn:nullptr;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}void* WorldOf(void*){return CurrentWorld();}}
namespace ue_wrap::game_thread {bool IsGameThread(){return true;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::script_gate {
bool WatchName(const facts::Function& fn,int tag,PreFn pre,PostFn post){test::hooks[fn.Name()]={tag,pre,post};return true;}
bool UnwatchName(const facts::Function& fn,int,PreFn,PostFn){test::hooks.erase(fn.Name());return true;}
bool IsInvocationActive(uint64_t id){return test::active.count(id)!=0;}
uint8_t* OutParamPtr(const Call& call,int32_t offset){return call.locals?call.locals+offset:nullptr;}
}
namespace ue_wrap {
bool GcPin::Pin(void* p){Release();obj_=p;if(p)++static_cast<test::Obj*>(p)->pins;return p!=nullptr;}
void GcPin::Release(){if(obj_)--static_cast<test::Obj*>(obj_)->pins;obj_=nullptr;}
}
namespace coop::net {uint64_t NowMs(){return test::now;}}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace ue_wrap::inventory {void* ResolveSaveSlot(){return test::actualSave;}}
namespace ue_wrap::container_inventory {
bool IsContainer(void* p){return p==test::container&&test::container->live;}
bool IsInventory(void* p){return p==test::actualInventory&&test::inventory->live;}
void* InventoryOf(void* p){return p==test::container?test::actualInventory:nullptr;}
void* OwnerOf(void* p){return p==test::actualInventory?test::actualOwner:nullptr;}
bool IsWorldInventory(void* p){return p==test::actualInventory&&test::inventory->live;}
bool ReadPersonalFlag(void* p,bool& personal){personal=false;return IsWorldInventory(p);}
uint8_t* WorldContentsSlot(void* p){return p==test::actualInventory?test::header.data():nullptr;}
uint8_t* ContentsSlot(void* p){return WorldContentsSlot(p);}
bool ReadContents(void* p,std::vector<save_record::SaveRecord>& records,size_t cap,int32_t* count){if(p!=test::actualInventory||test::rows.size()>cap)return false;records=test::rows;if(count)*count=int32_t(records.size());return true;}
bool RefundRecordWhile(void*,const save_record::SaveRecord& value,const std::function<bool()>& current){const auto record=value;if(!current())return false;++test::refunds;test::Callback(test::onRefund);if(!current()||!test::refundOK)return false;for(const auto& row:test::rows)if(row.key==record.key)return row.className==record.className;test::rows.push_back(record);test::Sync();return true;}
void RederiveShownState(void*,void*){++test::derived;test::Callback(test::onDerive);}
void RederiveShownStateWhile(void* a,void* inv,const std::function<bool()>& current){if(current())RederiveShownState(a,inv);}
}
namespace ue_wrap::save_record {
void ReadSaveRecord(const void* p,SaveRecord& record){CHECK(test::recordImages.count(p));record=test::recordImages.at(p);}
bool ApplyRecordViaWhile(void* p,void*,const SaveRecord& value,const std::function<bool()>& current,ClassResolver){const auto record=value;if(!current())return false;++test::loads;test::Callback(test::onLoad);if(!current()||!test::applyOK)return false;static_cast<test::Obj*>(p)->state=record;return true;}
bool TryReadArr(const void* p,int32_t offset,Arr& a){const auto* raw=static_cast<const uint8_t*>(p)+offset;std::memcpy(&a.data,raw,8);std::memcpy(&a.num,raw+8,4);return a.num>=0&&a.num<=32;}
}
namespace coop::prop_lifecycle {void DestroyLocalProp(void* p,bool deferred){CHECK(!deferred);++test::destroys;test::Callback(test::onDestroy);if(test::destroyOK&&p)static_cast<test::Obj*>(p)->live=false;}}
namespace coop::props::container_contents_sync {void NoteLocalContentsChange(void* owner){CHECK(owner==test::container);++test::notifications;}}
namespace test {
namespace C=coop::container_extract_recovery;namespace sg=ue_wrap::script_gate;using V=sg::Verdict;using Result=C::RestoreResult;
struct Frame{alignas(16) std::array<uint8_t,0x140> bytes{};sg::Call call{};Hook hook{};};
void Install(coop::net::Session* session){installed=session;C::Install(session);}
void Reset(coop::net::Role role=coop::net::Role::Host){onLoad=onRefund=onDestroy=onDerive={};C::OnDisconnect();for(const auto& object:objects)CHECK(object.pins==0);objects.clear();slots.resize(1);active.clear();rows.clear();pocket.clear();recordImages.clear();world=1;++serial;now=1000;loads=refunds=destroys=derived=notifications=0;applyOK=refundOK=destroyOK=true;session={};other={};session.mode=other.mode=role;
 containerClass=Make(L"prop_container_C");inventoryClass=Make(L"propInventory_C");itemClass=Make(L"prop_sample_C");container=Make(L"box",containerClass);inventory=Make(L"inventory",inventoryClass);save=Make(L"save");player=Make(L"player");
 extractFn=Make(L"extract");extractFn->outer=containerClass;takeFn=Make(L"takeObj");takeFn->outer=inventoryClass;pocketFn=Make(L"putObjectInventory2");loadFn=Make(L"loadData");actualOwner=container;actualInventory=inventory;actualSave=save;coop::players::Registry::Get().local=player;
 rows={Rich(),Rich(L"survivor")};Sync();Install(&session);CHECK(hooks.count(L"extract")&&hooks.count(L"takeObj"));
}
V Begin(Frame& f,const wchar_t* name,Obj* obj,Obj* fn,int index=0,Frame* parent=nullptr){f.hook=hooks.at(name);f.call={};f.call.object=obj;f.call.function=fn;f.call.locals=f.bytes.data();f.call.tag=f.hook.tag;f.call.invocation=++sequence;if(parent){f.call.callerObject=container;f.call.callerFunction=extractFn;f.call.callerLocals=parent->bytes.data();}std::memcpy(f.bytes.data(),&index,4);active.insert(f.call.invocation);const auto verdict=f.hook.pre?f.hook.pre(f.call):V::Run;if(verdict==V::Cancel)active.erase(f.call.invocation);return verdict;}
void Post(Frame& f){if(f.hook.post)f.hook.post(f.call);active.erase(f.call.invocation);}
void Take(Frame& outer,int index=0){Frame take;CHECK(Begin(take,L"takeObj",inventory,takeFn,index,&outer)==V::Run);const auto removed=rows.at(index);rows.erase(rows.begin()+index);Sync();take.bytes[5]=1;recordImages[take.bytes.data()+0x10]=removed;recordImages[outer.bytes.data()+0x10]=removed;Post(take);}
Obj* Carrier(Frame& outer){auto* actor=Make(L"temporary",itemClass);actor->state=Rich(L"temporary");actor->state.floats={{1.f}};void* raw=actor;std::memcpy(outer.bytes.data()+0x110,&raw,8);return actor;}
Result Pickup(Frame& outer,Obj* actor,bool accepted=true,bool notify=true){Frame pocketFrame;pocketFrame.call.object=player;pocketFrame.call.function=pocketFn;pocketFrame.call.locals=pocketFrame.bytes.data();pocketFrame.call.invocation=++sequence;pocketFrame.call.callerObject=container;pocketFrame.call.callerFunction=extractFn;pocketFrame.call.callerLocals=outer.bytes.data();active.insert(pocketFrame.call.invocation);void* raw=actor;std::memcpy(pocketFrame.bytes.data(),&raw,8);
 const auto result=C::RestoreForPickup(pocketFrame.call,actor);if(result==Result::Restored){pocketFrame.bytes[8]=accepted;outer.bytes[0x118]=accepted;if(accepted){pocket.push_back(actor->state);actor->live=false;}if(notify)C::OnPocketPost(pocketFrame.call);}active.erase(pocketFrame.call.invocation);return result;}
void Pump(int count=1){for(int i=0;i<count;++i){now+=1000;C::Install(installed);}}
void RefundFailure(){for(auto role:{coop::net::Role::Host,coop::net::Role::Client}){Reset(role);const auto original=rows[0];Frame extract;CHECK(Begin(extract,L"extract",container,extractFn)==V::Run);Take(extract);auto* actor=Carrier(extract);applyOK=refundOK=false;CHECK(Pickup(extract,actor)==Result::Failed);Post(extract);recordImages[extract.bytes.data()+0x10]=Rich(L"overwritten locals");refundOK=true;Pump(3);CHECK(pocket.empty()&&!actor->live);CHECK(rows.size()==2&&Equal(rows.back(),original));CHECK(destroys==1&&refunds==2&&notifications==1);Pump(3);CHECK(rows.size()==2&&refunds==2&&notifications==1);}}
void SpawnNull(){Reset();const auto original=rows[0];Frame extract;CHECK(Begin(extract,L"extract",container,extractFn)==V::Run);Take(extract);Post(extract);Pump(3);CHECK(rows.size()==2&&Equal(rows.back(),original)&&pocket.empty()&&loads==0);}
void PartialSpawn(){Reset();Frame extract;CHECK(Begin(extract,L"extract",container,extractFn)==V::Run);Take(extract);auto* actor=Make(L"begun",itemClass);void* raw=actor;std::memcpy(extract.bytes.data()+0x120,&raw,8);Post(extract);Pump(2);CHECK(!actor->live&&destroys==1&&refunds==1&&rows.size()==2&&pocket.empty());}
void SkippedPocket(){for(bool accepted:{false,true}){Reset();Frame extract;CHECK(Begin(extract,L"extract",container,extractFn)==V::Run);Take(extract);auto* actor=Carrier(extract);CHECK(Pickup(extract,actor,accepted,false)==Result::Restored);Post(extract);Pump(2);CHECK(refunds==0);CHECK(pocket.size()==size_t(accepted));CHECK(C::g_debts.size()==size_t(!accepted));}}
void SkippedTake(){Reset();const auto original=rows[0];Frame extract,take;CHECK(Begin(extract,L"extract",container,extractFn)==V::Run);CHECK(Begin(take,L"takeObj",inventory,takeFn,0,&extract)==V::Run);rows.erase(rows.begin());Sync();active.erase(take.call.invocation);Post(extract);Pump(2);CHECK(rows.size()==1&&refunds==0&&C::g_debts.size()==1&&Equal(C::g_debts.begin()->second->record,original));}
void DisconnectSettlement(){Reset();Frame extract;CHECK(Begin(extract,L"extract",container,extractFn)==V::Run);Take(extract);auto* actor=Carrier(extract);applyOK=refundOK=false;CHECK(Pickup(extract,actor)==Result::Failed);Post(extract);CHECK(rows.size()==1);refundOK=true;session.linked=false;C::OnDisconnect();CHECK(rows.size()==2&&C::g_debts.empty()&&notifications==1);}
void NestedInstall(){Reset();third={};Frame extract;CHECK(Begin(extract,L"extract",container,extractFn)==V::Run);Take(extract);auto* actor=Carrier(extract);applyOK=refundOK=false;CHECK(Pickup(extract,actor)==Result::Failed);Post(extract);refundOK=true;onRefund=[]{Install(&third);};Install(&other);CHECK(C::g_session==&third);CHECK(refunds>=1);}
void ReservationCap(){Reset();std::vector<Frame> frames(65);for(int i=0;i<64;++i)CHECK(Begin(frames[i],L"extract",container,extractFn)==V::Run);CHECK(Begin(frames[64],L"extract",container,extractFn)==V::Cancel);CHECK(rows.size()==2);active.clear();Pump(9);CHECK(C::g_debts.empty()&&rows.size()==2&&refunds==0);}
void ClassRetirement(){for(int mode=0;mode<5;++mode){Reset();Frame extract;CHECK(Begin(extract,L"extract",container,extractFn)==V::Run);Take(extract);auto* actor=Carrier(extract);applyOK=false;onLoad=[=]{if(mode==0)container->cls=Make(L"new owner class");if(mode==1)inventory->cls=Make(L"new inventory class");if(mode==2)save->cls=Make(L"new save class");if(mode==3)player->cls=Make(L"new player class");if(mode==4)++itemClass->serial;};CHECK(Pickup(extract,actor)==Result::Invalidated);Post(extract);Pump(2);CHECK(refunds==0&&destroys==0&&rows.size()==1);}}
void SuccessfulPocket(){Reset();const auto original=rows[0];Frame extract;CHECK(Begin(extract,L"extract",container,extractFn)==V::Run);Take(extract);auto* actor=Carrier(extract);CHECK(Pickup(extract,actor)==Result::Restored);Post(extract);Pump(3);CHECK(rows.size()==1&&pocket.size()==1&&Equal(pocket[0],original));CHECK(refunds==0&&destroys==0&&!C::Pending(actor)&&notifications==0&&C::g_debts.empty());}
void FullInventory(){Reset();Frame extract;CHECK(Begin(extract,L"extract",container,extractFn)==V::Run);Take(extract);auto* actor=Carrier(extract);CHECK(Pickup(extract,actor,false)==Result::Restored);Post(extract);Pump(3);CHECK(pocket.empty()&&!actor->live&&rows.size()==2&&refunds==1);}
void RefundRetry(){Reset();Frame extract;CHECK(Begin(extract,L"extract",container,extractFn)==V::Run);Take(extract);auto* actor=Carrier(extract);applyOK=false;refundOK=false;CHECK(Pickup(extract,actor)==Result::Failed);Post(extract);Pump(3);CHECK(rows.size()==1&&refunds>0&&C::g_debts.size()==1);refundOK=true;Pump(3);CHECK(rows.size()==2&&!C::Pending(actor)&&C::g_debts.empty()&&notifications==1);}
void DestroyRetry(){Reset();Frame extract;CHECK(Begin(extract,L"extract",container,extractFn)==V::Run);Take(extract);auto* actor=Carrier(extract);applyOK=false;destroyOK=false;CHECK(Pickup(extract,actor)==Result::Failed);Post(extract);Pump(2);CHECK(actor->live&&rows.size()==1&&refunds==0);destroyOK=true;Pump(3);CHECK(!actor->live&&rows.size()==2&&refunds==1);}
void FaultUnwind(){for(bool debited:{false,true}){Reset();const auto original=rows[0];Frame extract;CHECK(Begin(extract,L"extract",container,extractFn)==V::Run);if(debited)Take(extract);active.erase(extract.call.invocation);recordImages[extract.bytes.data()+0x10]=Rich(L"overwritten");Pump(3);CHECK(rows.size()==size_t(debited?1:2)&&pocket.empty()&&refunds==0);if(debited){CHECK(C::g_debts.size()==1&&Equal(C::g_debts.begin()->second->record,original));}else CHECK(C::g_debts.empty());}}
void NestedExtract(){Reset();rows={Rich(L"first"),Rich(L"second"),Rich(L"survivor")};Sync();Frame a,b;CHECK(Begin(a,L"extract",container,extractFn)==V::Run);Take(a);CHECK(Begin(b,L"extract",container,extractFn)==V::Run);Take(b);Post(b);Post(a);Pump(4);CHECK(rows.size()==3&&refunds==2);std::set<std::wstring> keys;for(const auto& row:rows)keys.insert(row.key);CHECK(keys==std::set<std::wstring>({L"first",L"second",L"survivor"}));}
void RetiredSources(){for(int mode=0;mode<11;++mode){Reset();Frame extract;CHECK(Begin(extract,L"extract",container,extractFn)==V::Run);Take(extract);auto* actor=Carrier(extract);applyOK=false;onLoad=[mode]{if(mode==0)++world;if(mode==1)++serial;if(mode==2)++container->serial;if(mode==3)++inventory->serial;if(mode==4)++save->serial;if(mode==5)++player->serial;if(mode==6)coop::players::Registry::Get().local=Make(L"new player");if(mode==7)actualOwner=Make(L"other container");if(mode==8)actualInventory=Make(L"other inventory");if(mode==9)actualSave=Make(L"other save");if(mode==10)Install(&other);};CHECK(Pickup(extract,actor)==Result::Invalidated);Post(extract);Pump(3);CHECK(rows.size()==1&&refunds==0&&pocket.empty());}}
}
int main(int argc,char** argv){using namespace test;struct Case{const char* name;void(*run)();};const Case cases[]={
 {"restore-failure",RefundFailure},{"spawn-null",SpawnNull},{"partial-spawn",PartialSpawn},{"successful-pocket",SuccessfulPocket},
 {"full-inventory",FullInventory},{"refund-retry",RefundRetry},{"destroy-retry",DestroyRetry},{"fault-unwind",FaultUnwind},
 {"skipped-pocket",SkippedPocket},{"skipped-take",SkippedTake},{"nested-extract",NestedExtract},{"retired-sources",RetiredSources},
 {"class-retirement",ClassRetirement},{"disconnect-settlement",DisconnectSettlement},{"nested-install",NestedInstall},{"reservation-cap",ReservationCap}};
 bool ran=false;for(const auto& item:cases)if(argc<2||std::string(argv[1])==item.name){scenario=item.name;item.run();std::printf("PASS local recovery %s\n",scenario);ran=true;}CHECK(ran);onLoad=onRefund=onDestroy=onDerive={};C::OnDisconnect();for(const auto& object:objects)CHECK(object.pins==0);std::puts("PASS actual LOCAL extraction recovery controller");return 0;}
