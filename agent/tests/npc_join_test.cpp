#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cstddef>
#include <limits>
#include <functional>
#include <memory>
#include <thread>
#include <set>
#include <new>
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/core/log.h"
#include "coop/net/session.h"
#include "coop/net/net_clock.h"
#include "coop/element/registry.h"
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
#ifndef NPC_JOIN_SOURCE
#define NPC_JOIN_SOURCE "../../src/votv-coop/src/coop/creatures/npc_join_delivery.cpp"
#endif
#include NPC_JOIN_SOURCE
#ifndef ELEMENT_SOURCE
#define ELEMENT_SOURCE "../../src/votv-coop/src/coop/element/element.cpp"
#endif
#include ELEMENT_SOURCE
#ifndef REGISTRY_SOURCE
#define REGISTRY_SOURCE "../../src/votv-coop/src/coop/element/registry.cpp"
#endif
#include REGISTRY_SOURCE
namespace test {
namespace J=coop::npc_join_delivery;namespace E=coop::element;namespace N=coop::net;
struct Object {void* root=nullptr;ue_wrap::reflection::FName key{1,0};int cls=100;ue_wrap::FVector position{10,20,30};ue_wrap::FRotator rotation{1,2,3};ue_wrap::FVector scale{2,3,4};} objects[256];
int serial[256];bool live[256];
N::Session session,successor;N::Session* active=&session;
uint32_t world=1,sessionSerial=1;int worldObject=0,worlds[2];
std::vector<std::unique_ptr<E::Element>> owners;
std::function<void(int)> boundary;
std::function<void()> onProperty;
int nativeCalls=0,notifications=0,completionCalls=0;
bool poseOkay=true,keyReadable=true,keyFound=true,frameOkay=true;
std::wstring keyType=L"NameProperty";
int keyOffset=offsetof(Object,key),keySize=sizeof(Object);
E::ElementId offId=E::kInvalidId;
void Boundary(int where){++nativeCalls;if(boundary)boundary(where);}
void Reset(){
 J::OnDisconnect();owners.clear();
 for(int i=0;i<256;++i){objects[i]={};++serial[i];live[i]=true;objects[i].root=&objects[200];}
 objects[200].cls=101;session={};successor={};active=&session;world=1;worldObject=0;++sessionSerial;
 boundary={};onProperty={};nativeCalls=notifications=completionCalls=0;poseOkay=keyReadable=keyFound=frameOkay=true;
 keyType=L"NameProperty";keyOffset=offsetof(Object,key);keySize=sizeof(Object);offId=E::kInvalidId;N::testNow=100;
 session.onSend=[]{Boundary(4);};
}
E::Element* Add(int object=0){
 auto p=std::make_unique<E::Element>(E::ElementType::Npc);p->SetTypeName("geomOcta_C");
 CHECK(E::Registry::Get().AllocHostId(p.get())!=E::kInvalidId);
 if(object>=0)p->SetActor(&objects[object],object);
 auto* value=p.get();owners.push_back(std::move(p));return value;
}
void Advance(uint64_t by=250){N::testNow+=by;J::Tick();}
size_t Count(N::ReliableKind kind,int slot=31){size_t n=0;for(const auto& p:session.packets)if(p.kind==kind&&p.slot==slot)++n;return n;}
size_t Attempts(N::ReliableKind kind,int slot=31){size_t n=0;for(const auto& p:session.attempts)if(p.kind==kind&&p.slot==slot)++n;return n;}
void Queue(){J::Queue(31);J::Tick();}
void Replace(E::ElementId id){
 for(auto& p:owners)if(p&&p->GetId()==id){p.reset();break;}
 for(uint32_t n=0;n<E::kHostRangeSize;++n){auto* p=Add(-1);if(p->GetId()==id){p->SetActor(&objects[0],0);return;}}
 CHECK(false);
}
void Invalidate(int cause,E::ElementId id){
 if(cause==0){++world;worldObject=1;}
 if(cause==1)active=&successor;if(cause==2)++sessionSerial;if(cause==3)++session.stops;
 if(cause==4)session.mode=N::Role::Client;if(cause==5)++session.generations[31];
 if(cause==6)session.ResetWorld(31);if(cause==7)++serial[0];if(cause==8)++serial[100];if(cause==9)Replace(id);
}
std::vector<uint8_t> Marker(uint32_t sent=7){N::SnapshotEndPayload end{};end.propSent=sent;std::vector<uint8_t> result(sizeof end);std::memcpy(result.data(),&end,sizeof end);return result;}
uint64_t Complete(int slot=31){auto token=J::BeginSnapshot(slot);CHECK(token);CHECK(J::CompleteSnapshot(slot,token,Marker(),[](auto*,int){++completionCalls;}));return token;}
void Pass(const char* family,int n){std::printf("PASS %s%02d\n",family,n);}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){for(int i=0;i<256;++i)if(p==&test::objects[i])return i;return -1;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<256&&p==&test::objects[i]&&test::live[i];}
bool IsLive(void* p){return IsLiveByIndex(p,InternalIndexOf(p));}
int32_t AllocateSlotSerial(int32_t i){return i>=0&&i<256?test::serial[i]:0;}
int32_t SlotSerial(int32_t i){return AllocateSlotSerial(i);}
void* ObjectAt(int32_t i){return i>=0&&i<256?&test::objects[i]:nullptr;}
void* ClassOf(void* p){return p?&test::objects[static_cast<test::Object*>(p)->cls]:nullptr;}
int32_t StructSize(void*){return test::keySize;}
bool PropertyOffsetAndClass(void* klass,const wchar_t* name,int32_t& offset,std::wstring& type){
 if(!std::wcscmp(name,L"RootComponent")){offset=offsetof(test::Object,root);type=L"ObjectProperty";return true;}
 offset=klass==&test::objects[102]?offsetof(test::Object,rotation):test::keyOffset;type=test::keyType;
 if(test::onProperty)test::onProperty();return test::keyFound;
}
void* FindDispatchFunctionCached(void*,const wchar_t* name){int n=!std::wcscmp(name,L"location")?0:!std::wcscmp(name,L"rotation")?1:2;return &test::objects[110+n];}
std::wstring ToString(const FName&){test::Boundary(3);return test::keyReadable?L"saved":L"";}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return &test::worlds[test::worldObject];}void* WorldOf(void* p){return ue_wrap::reflection::InternalIndexOf(p)<100?CurrentWorld():nullptr;}}
namespace coop::net::session_serial {uint32_t Current(){return test::sessionSerial;}}
namespace coop::npc_sync {net::Session* GetSession(){return test::active;}}
namespace coop::kerfur_entity {coop::element::ElementId GetOriginOffEidForEid(coop::element::ElementId){return test::offId;}}
namespace coop::geom_octa_sync {void OnNpcSpawnSent(uint32_t,void*,const char*,size_t,bool,int){++test::notifications;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap {
namespace P::name {const wchar_t* GetActorLocationFn=L"location";const wchar_t* GetActorRotationFn=L"rotation";const wchar_t* GetActorScale3DFn=L"scale";}
namespace R=reflection;
struct ParamFrame{
 int stage;float output[3]{};
 explicit ParamFrame(void* fn):stage(R::InternalIndexOf(fn)-110){}
 bool valid()const{return test::frameOkay;}
 bool GetRaw(const wchar_t*,void* value,int size){CHECK(size==12);std::memcpy(value,output,12);return true;}
};
bool Call(void* actor,ParamFrame& frame){
 auto* object=static_cast<test::Object*>(actor);
 const void* value=frame.stage==0?static_cast<const void*>(&object->position):frame.stage==1?static_cast<const void*>(&object->rotation):static_cast<const void*>(&object->scale);
 std::memcpy(frame.output,value,12);test::Boundary(frame.stage);return test::poseOkay;
}
namespace engine {namespace P=ue_wrap::P;
#include "npc_transform.inc"
}
namespace kerfur {
#include "npc_save_key.inc"
}
}
namespace test_snapshot {
inline uint32_t seed=1;inline bool coherent=true,purge=false;
inline int tails=0,closed=0,deferred=0;inline std::function<void()> onTail,onDequeue;
uint32_t SeedGeneration(){return seed;}bool IsRegistrySeededForCurrentWorld(){return coherent;}bool InPurgeEpisode(){return purge;}
}
namespace coop::snapshot_census {void BuildHostTail(std::vector<uint8_t>& tail,int){++test_snapshot::tails;if(test_snapshot::onTail)test_snapshot::onTail();tail={12,34};}}
namespace coop::dev::eid_lifetime_trace {void EmitVerdict(){}}
namespace coop::prop_snapshot {
namespace PT=test_snapshot;
std::atomic<coop::net::Session*> g_session_ptr{&test::session};
int g_currentTargetSlot=-1;uint64_t g_drainCompletionToken=0;uint32_t g_drainSeedGen=1,g_snapshotSentTotal=0,g_snapshotDropped=0;
enum class Skip{kCount=1};uint32_t g_snapshotSkipped[1]{};const char* kSkipNames[]={"unread"};
std::vector<int> g_snapshotCandidates;
enum class DeferReason {RegistryIncoherent};
void CloseBracket_(int slot){CHECK(slot>0&&slot<32);++test_snapshot::closed;}
void DeferSlot_(int,DeferReason,const char*){++test_snapshot::deferred;}
bool HasSnapshotFor(int slot){return g_currentTargetSlot==slot||coop::npc_join_delivery::HasCompletion(slot);}
void ClearDrainState_(){g_currentTargetSlot=-1;g_drainCompletionToken=0;g_snapshotSentTotal=0;}
void DequeuePending_(){if(test_snapshot::onDequeue)test_snapshot::onDequeue();}
#include "npc_snapshot_complete.inc"
}
int main(){using namespace test;
 for(int n=1;n<=25;++n){Reset();auto* element=Add();const auto id=element->GetId();
  switch(n){
  case 1: session.mode=N::Role::Client;Queue();CHECK(session.packets.empty()&&!J::HasPending(31));break;
  case 2: for(int mode=0;mode<2;++mode){session.active=mode!=0;session.linked=mode!=1;Queue();CHECK(session.packets.empty());}break;
  case 3: for(int slot:{-1,0,32,99})J::Queue(slot);J::Tick();CHECK(session.packets.empty());break;
  case 4: session.generations[31]=0;Queue();CHECK(!J::HasPending(31));session.generations[31]=1;session.online[31]=false;Queue();CHECK(session.packets.empty());break;
  case 5: session.ready=false;Queue();CHECK(J::HasPending(31)&&nativeCalls==0);session.ready=true;Advance();CHECK(Count(N::ReliableKind::EntitySpawn)==1);break;
  case 6: session.sendOk=false;Queue();CHECK(nativeCalls==5);for(int i=0;i<9;++i)Advance();CHECK(J::HasPending(31));session.sendOk=true;Advance();CHECK(Count(N::ReliableKind::EntitySpawn)==1&&!J::HasPending(31));break;
  case 7: Add(1);session.allow=[=](int,N::ReliableKind,const void* p){return static_cast<const N::EntitySpawnPayload*>(p)->elementId==id;};Queue();CHECK(Count(N::ReliableKind::EntitySpawn)==1);session.allow={};Advance();CHECK(Count(N::ReliableKind::EntitySpawn)==2);break;
  case 8: J::Queue(31);owners[0].reset();Add(1);J::Tick();CHECK(session.packets.empty());break;
  case 9: J::Queue(31);Replace(id);J::Tick();CHECK(session.packets.empty());break;
  case 10: element->SetActor(nullptr,-1);Queue();CHECK(J::HasPending(31)&&session.packets.empty());element->SetActor(&objects[0],0);Advance();CHECK(Count(N::ReliableKind::EntitySpawn)==1);break;
  case 11: J::Queue(31);live[0]=false;J::Tick();CHECK(!J::HasPending(31)&&session.packets.empty());break;
  case 12: J::Queue(31);element->SetBeingDeleted(true);J::Tick();CHECK(!J::HasPending(31)&&session.packets.empty());break;
  case 13: J::Queue(31);objects[0].cls=102;J::Tick();CHECK(session.packets.empty());break;
  case 14: J::Queue(31);element->SetTypeName("other_C");J::Tick();CHECK(session.packets.empty());break;
  case 15: keyReadable=false;Queue();CHECK(J::HasPending(31)&&session.packets.empty());keyReadable=true;Advance();CHECK(Count(N::ReliableKind::EntitySpawn)==1);break;
  case 16: keyType=L"IntProperty";Queue();CHECK(J::HasPending(31)&&session.packets.empty());++serial[100];J::Queue(31);keyType=L"NameProperty";J::Tick();CHECK(Count(N::ReliableKind::EntitySpawn)==1);break;
  case 17: offId=125;Queue();CHECK(session.packets.size()==1);{N::EntitySpawnPayload p{};std::memcpy(&p,session.packets[0].data.data(),sizeof p);CHECK(p.retireOffEid==125&&p.savePersisted==1&&p.scaleY==3);}break;
  case 18: objects[0].position.X=std::numeric_limits<float>::infinity();Queue();CHECK(session.packets.empty()&&J::HasPending(31));objects[0].position.X=N::kMaxCoord+10;Advance();CHECK(session.packets.empty());objects[0].position.X=40;Advance();CHECK(Count(N::ReliableKind::EntitySpawn)==1);break;
  case 19: session.sendOk=false;Queue();objects[0].position.X=77;session.sendOk=true;Advance();{N::EntitySpawnPayload p{};std::memcpy(&p,session.packets[0].data.data(),sizeof p);CHECK(p.locX==77);}break;
  case 20: session.sendOk=false;Queue();N::testNow=5;session.sendOk=true;J::Tick();CHECK(Count(N::ReliableKind::EntitySpawn)==1);break;
  case 21: for(int i=1;i<40;++i)Add(i);Queue();CHECK(session.packets.size()<=12&&J::HasPending(31));break;
  case 22: for(int slot=1;slot<32;++slot){session.online[slot]=true;J::Queue(slot);}for(int i=0;i<4;++i)J::Tick();for(int slot=1;slot<32;++slot)CHECK(Count(N::ReliableKind::EntitySpawn,slot)==1);break;
  case 23: {bool replaced=false;boundary=[&](int stage){if(stage==0&&!replaced){replaced=true;objects[0].position.X=88;J::Queue(31);}};Queue();boundary={};Advance();CHECK(Count(N::ReliableKind::EntitySpawn)==1);N::EntitySpawnPayload p{};std::memcpy(&p,session.packets[0].data.data(),sizeof p);CHECK(p.locX==88);}break;
  case 24: {Add(1);int depth=0,maximum=0;boundary=[&](int){++depth;if(depth>maximum)maximum=depth;J::Tick();--depth;};Queue();CHECK(maximum==1&&Count(N::ReliableKind::EntitySpawn)==2);}break;
  case 25: session.online[1]=true;J::Queue(31);J::Queue(1);{auto p=Marker();auto a=J::BeginSnapshot(31),b=J::BeginSnapshot(1);CHECK(J::CompleteSnapshot(31,a,p,[](auto*,int){++completionCalls;}));CHECK(J::CompleteSnapshot(1,b,p,[](auto*,int){++completionCalls;}));session.allow=[](int slot,N::ReliableKind,const void*){return slot!=31;};J::Tick();CHECK(Count(N::ReliableKind::SnapshotComplete,1)==1&&Attempts(N::ReliableKind::SnapshotComplete)==0&&completionCalls==1);session.allow={};Advance();CHECK(Count(N::ReliableKind::SnapshotComplete)==1&&completionCalls==2);}break;
  }Pass("NJ",n);
 }
 for(int stage=0;stage<5;++stage)for(int cause=0;cause<10;++cause){Reset();auto id=Add()->GetId();bool changed=false;
  boundary=[&](int where){if(where==stage&&!changed){changed=true;Invalidate(cause,id);}};
  Queue();CHECK(changed);CHECK(nativeCalls==stage+1);CHECK(session.packets.size()==(stage==4?1u:0u));CHECK(notifications==0);
  boundary={};Advance();CHECK(!J::HasPending(31));Pass("NK",stage*10+cause+1);
 }
 // Completion bytes and ownership remain independent of mutable caller storage.
 Reset();Add();auto token=J::BeginSnapshot(31);auto payload=Marker(19);CHECK(J::CompleteSnapshot(31,token,payload,[](auto*,int){++completionCalls;}));payload.assign(payload.size(),255);J::Tick();
 CHECK(completionCalls==1&&!J::SnapshotCurrent(31,token));{N::SnapshotEndPayload p{};std::memcpy(&p,session.packets.back().data.data(),sizeof p);CHECK(p.propSent==19);}
 Reset();Add();Complete();session.sendOk=false;for(int i=0;i<9;++i)Advance();CHECK(J::HasCompletion(31)&&!completionCalls);session.sendOk=true;Advance();CHECK(completionCalls==1);
 Reset();Add();auto old=Complete();auto latest=J::BeginSnapshot(31);CHECK(old!=latest);J::Tick();CHECK(session.packets.empty()&&!completionCalls);
 CHECK(!J::CompleteSnapshot(31,old,Marker(),{}));
 Reset();Add();J::Queue(31);session.sendOk=false;token=J::BeginSnapshot(31);int invalid=0;CHECK(J::CompleteSnapshot(31,token,Marker(),{},[]{return false;},[&](auto*,int){++invalid;}));J::Tick();CHECK(invalid==1&&session.packets.empty()&&J::HasPending(31));
 Reset();Add();Complete();session.ResetWorld(31);J::Tick();CHECK(session.packets.empty()&&!completionCalls);
 Reset();Add();Complete();bool replaced=false;session.onSend=[&]{if(!replaced){replaced=true;auto fresh=J::BeginSnapshot(31);CHECK(J::CompleteSnapshot(31,fresh,Marker(),[](auto*,int){++completionCalls;}));}};J::Tick();CHECK(!completionCalls&&J::HasCompletion(31));session.onSend={};Advance();CHECK(completionCalls==1);
 Reset();Add();token=Complete();replaced=false;session.onSend=[&]{if(!replaced){replaced=true;CHECK(J::CompleteSnapshot(31,token,Marker(29),[](auto*,int){++completionCalls;}));}};J::Tick();CHECK(!completionCalls&&J::HasCompletion(31));session.onSend={};Advance();CHECK(completionCalls==1);
 Reset();Add();boundary=[](int stage){if(stage==4)++world;};Queue();CHECK(!notifications);
 Reset();for(int i=0;i<70;++i)Add(i);const auto refused=owners.back()->GetId();session.allow=[=](int,N::ReliableKind,const void* p){return static_cast<const N::EntitySpawnPayload*>(p)->elementId!=refused;};Queue();for(int i=0;i<8;++i)Advance();CHECK(J::HasPending(31)&&Count(N::ReliableKind::EntitySpawn)==69);auto cursor=J::g_joins[31]->cursor;J::Tick();CHECK(J::g_joins[31]->cursor-cursor<=64);
 Reset();for(int i=0;i<20;++i)Add(i);session.online[1]=true;J::Queue(31);Complete(1);J::Tick();CHECK(Count(N::ReliableKind::SnapshotComplete,1)==1&&Count(N::ReliableKind::EntitySpawn)<=12);
 Reset();objects[0].key={0,0};objects[1].cls=102;bool inner=false;onProperty=[&]{if(!inner){inner=true;bool saved=false;CHECK(ue_wrap::kerfur::TryHasSaveKey(&objects[1],saved)&&saved);}};bool saved=true;CHECK(ue_wrap::kerfur::TryHasSaveKey(&objects[0],saved)&&!saved);
 Reset();auto* binding=Add();++serial[0];Queue();CHECK(session.packets.empty()&&J::HasPending(31));binding->SetActor(&objects[0],0);Advance();CHECK(Count(N::ReliableKind::EntitySpawn)==1);
 Reset();binding=Add();boundary=[&](int stage){if(stage==4){++serial[0];binding->SetActor(&objects[0],0);}};Queue();CHECK(!notifications);
 Reset();binding=Add();auto* canonical=Add();binding->SetActor(&objects[0],0);CHECK(E::Registry::Get().EidForActor(&objects[0])==canonical->GetId());Queue();CHECK(Count(N::ReliableKind::EntitySpawn)==1&&!J::HasPending(31));
 // The actual prop completion function detaches a marker for each recipient.
 namespace PS=coop::prop_snapshot;namespace TS=test_snapshot;
 Reset();Add();session.online[1]=true;session.allow=[](int slot,N::ReliableKind,const void*){return slot!=31;};J::Queue(31);
 TS::seed=1;TS::coherent=true;TS::purge=false;TS::tails=TS::closed=TS::deferred=0;TS::onTail=TS::onDequeue={};
 PS::g_currentTargetSlot=31;PS::g_drainCompletionToken=J::BeginSnapshot(31);PS::g_snapshotSentTotal=7;
 PS::CompleteDrainForCurrentSlot(&session);CHECK(PS::g_currentTargetSlot==-1&&J::HasCompletion(31)&&TS::tails==1&&!TS::closed);
 PS::g_currentTargetSlot=1;PS::g_drainCompletionToken=J::BeginSnapshot(1);PS::g_snapshotSentTotal=11;
 PS::CompleteDrainForCurrentSlot(&session);J::Tick();CHECK(TS::closed==1&&session.closed.size()==1&&session.closed[0]==1&&J::HasCompletion(31));
 session.allow={};Advance();CHECK(TS::closed==2&&TS::tails==2&&session.closed.size()==2);
 Reset();Add();TS::tails=TS::closed=TS::deferred=0;PS::g_currentTargetSlot=31;PS::g_drainCompletionToken=J::BeginSnapshot(31);PS::g_snapshotSentTotal=7;
 PS::CompleteDrainForCurrentSlot(&session);++TS::seed;J::Tick();CHECK(TS::deferred==1&&!TS::closed&&session.packets.empty());
 Reset();Add();TS::seed=1;TS::closed=TS::deferred=0;PS::g_currentTargetSlot=31;PS::g_drainCompletionToken=J::BeginSnapshot(31);PS::g_snapshotSentTotal=7;
 TS::onTail=[] {PS::g_drainCompletionToken=J::BeginSnapshot(31);PS::g_snapshotSentTotal=55;};PS::CompleteDrainForCurrentSlot(&session);
 CHECK(PS::g_currentTargetSlot==31&&!J::HasCompletion(31)&&PS::g_snapshotSentTotal==55);TS::onTail={};PS::CompleteDrainForCurrentSlot(&session);J::Tick();CHECK(TS::closed==1);
 puts("PASS actual prop completion: independent slots, census/seed ownership, newer bracket preservation and gate closure after accepted marker");
 // The actual constructor gives a different identity even at the same C++ address.
 alignas(E::Element) unsigned char storage[sizeof(E::Element)];auto* first=new(storage)E::Element(E::ElementType::Npc);auto identity=first->GetIncarnation();first->~Element();auto* second=new(storage)E::Element(E::ElementType::Npc);CHECK(second->GetIncarnation()!=identity);second->~Element();
 Reset();
 puts("PASS NPC join delivery: 75 named continuation fixes; actual Registry/Element, transform and save-key bodies; independent snapshot completion");
}
