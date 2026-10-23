#ifndef CONTAINER_BIRTH_SOURCE
#define CONTAINER_BIRTH_SOURCE "../../src/votv-coop/src/coop/props/prop_container_extract.cpp"
#endif
#include CONTAINER_BIRTH_SOURCE
namespace coop::container_extract_recovery {
bool InFlight(){return false;}
bool Pending(void*){return false;}
}
#include "facts_test_support.h"
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <stdexcept>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace S=coop::prop_lifecycle;
namespace test {
struct Obj {
    int index=0, serial=1;bool live=true,mirror=false,covers=true,food=false;
    int actorOff=0,successOff=8,frame=16;uint32_t eid=0;
    std::wstring key=L"restored", cls=L"prop_batts_C", name=L"batt";
    float charge=.25f;
} objects[160];
uint32_t world=1, serial=1;uint64_t now=1000;
thread_local bool gameThread=true;
thread_local std::vector<void*> bodies;
ue_wrap::script_gate::PreFn pre=nullptr;ue_wrap::script_gate::PostFn post=nullptr;
void* externalActor=nullptr;bool externalSuccess=true,external=false;
bool recordOK=true;int records=0,reads=0;float lastCharge=0;
std::vector<int> attempts;
std::function<void()> duringRead,duringMark,duringRecord;
void Callback(std::function<void()>& source){auto callback=std::move(source);source={};if(callback)callback();}
struct Frame {alignas(16) uint8_t bytes[32]{};};
coop::net::Session session;
ue_wrap::script_gate::Call Call(Obj* actor,Frame& frame,int fn=0,bool success=true) {
    auto& function=objects[fn];
    std::memcpy(frame.bytes+function.actorOff,&actor,sizeof(actor));
    frame.bytes[function.successOff]=success;
    ue_wrap::script_gate::Call c{};c.object=&objects[2];c.function=&function;c.locals=frame.bytes;return c;
}
void Reset(coop::net::Role role=coop::net::Role::Host) {
    S::ClearContainerExtractions();CHECK(!S::g_draining);
    session={};session.mode=role;world=serial=1;now=1000;gameThread=true;bodies.clear();
    external=false;externalActor=nullptr;externalSuccess=true;recordOK=true;records=reads=0;
    attempts.clear();duringRead=duringMark=duringRecord={};
    for(int i=0;i<160;++i){objects[i]={};objects[i].index=i;objects[i].key=L"key"+std::to_wstring(i);}
    S::InstallInventory(&session);CHECK(pre && post);
}
void Enqueue(int actor=3,int fn=0) {Frame frame;auto c=Call(&objects[actor],frame,fn);post(c);}
void Pump(){now+=250;S::InstallInventory(&session);}
}
namespace coop::prop_lifecycle {std::atomic<net::Session*> g_session_ptr{nullptr};}
namespace coop::net {uint64_t NowMs(){return test::now;}}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* p){return static_cast<test::Obj*>(p)->index>=3?CurrentWorld():nullptr;}
}
namespace ue_wrap::game_thread {bool IsGameThread(){return test::gameThread;}}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return static_cast<test::Obj*>(p)->index;}
bool IsLiveByIndex(void* p,int32_t index){return index>=0&&index<160&&p==&test::objects[index]&&test::objects[index].live;}
int32_t SlotSerial(int32_t index){return test::objects[index].serial;}
int32_t AllocateSlotSerial(int32_t index){return SlotSerial(index);}
int32_t FunctionFrameSize(void* f){return static_cast<test::Obj*>(f)->frame;}
int32_t FindParamOffset(void* f,const wchar_t* n){auto* o=static_cast<test::Obj*>(f);return !wcscmp(n,L"object")?o->actorOff:o->successOff;}
std::wstring ClassNameOf(void* p){return static_cast<test::Obj*>(p)->cls;}
}
namespace ue_wrap::script_gate {
bool WatchName(const facts::Function&,int,PreFn pre,PostFn post){test::pre=pre;test::post=post;return true;}
bool IsBodyActive(void* f,void*){for(auto* p:test::bodies)if(p==f)return true;return false;}
uint8_t* OutParamPtr(const Call& c,int32_t off){
    if(!test::external)return nullptr;
    return off==static_cast<test::Obj*>(c.function)->actorOff?reinterpret_cast<uint8_t*>(&test::externalActor):reinterpret_cast<uint8_t*>(&test::externalSuccess);
}
}
namespace ue_wrap::prop {
bool IsKeyedInteractable(void*){return true;}
bool IsDescendantOfProp(void*){return true;}
std::wstring GetInteractableKeyString(void* p){++test::reads;test::Callback(test::duringRead);return static_cast<test::Obj*>(p)->key;}
std::wstring GetPropNameString(void* p){return static_cast<test::Obj*>(p)->name;}
}
namespace ue_wrap::food {bool IsFood(void* p){return static_cast<test::Obj*>(p)->food;}}
namespace ue_wrap::engine {
bool TryGetActorLocation(void* p,FVector& v){test::attempts.push_back(static_cast<test::Obj*>(p)->index);v={1,2,3};return true;}
bool TryGetActorRotation(void*,FRotator& r){r={};return true;}
FVector GetActorScale3D(void*){return {1,1,1};}
}
namespace coop::prop_echo_suppress {bool IsMirrorSpawn(void* p){return static_cast<test::Obj*>(p)->mirror;}}
namespace coop::prop_synth_key {std::wstring EnsureKeyForBroadcast(void*,const std::wstring& key,bool){return key;}}
namespace coop::prop_wire_parity {uint8_t PhysFlagsOf(void*){return 0;}}
namespace coop::prop_element_tracker {
void SetSession(net::Session*){}
std::wstring MarkPropElement(void* p,const std::wstring& key,const std::wstring&,EnrollSource){
    auto* o=static_cast<test::Obj*>(p);o->eid=1000+o->index;test::Callback(test::duringMark);return key;
}
coop::element::ElementId GetPropElementIdForActor(void* p){return static_cast<test::Obj*>(p)->eid;}
}
namespace coop::join_membership_sweep {void RecordClaimIfTracking(void*){}}
namespace coop::prop_save_data {
bool Covers(void* p){return static_cast<test::Obj*>(p)->covers;}
bool PublishWithSpawn(net::Session*,void* p,const net::WireKey&,int){++test::records;test::lastCharge=static_cast<test::Obj*>(p)->charge;test::Callback(test::duringRecord);return test::recordOK;}
}
namespace ue_wrap::log {void Write(Level,const char*,...){}}
int main(){
    using namespace test;
    Reset();Frame frame;auto c=Call(&objects[3],frame);pre(c);
    CHECK(!S::ContainerExtractionInFlight()); // PRE is not native-body admission
    bodies.push_back(c.function);CHECK(S::ContainerExtractionInFlight());
    CHECK(S::ContainerBirthPending(&objects[3]));
    bodies.push_back(c.function);pre(c);bodies.pop_back();CHECK(S::ContainerExtractionInFlight());
    bool workerSaw=true;std::thread worker([&]{gameThread=false;workerSaw=S::ContainerExtractionInFlight();});worker.join();CHECK(!workerSaw);
    bodies.pop_back();CHECK(!S::ContainerExtractionInFlight()); // no POST needed after Cancel/fault
    post(c);post(c);CHECK(S::g_pending.size()==1&&session.sent.empty());
    CHECK(S::ContainerBirthPending(&objects[3])&&!S::ContainerBirthPending(&objects[4]));
    objects[3].key=L"saved-final";objects[3].charge=0;Pump();
    CHECK(session.sent.size()==1&&records==1&&lastCharge==0&&S::g_pending.empty());
    CHECK(!S::ContainerBirthPending(&objects[3]));
    CHECK(std::string(session.sent[0].key.data,session.sent[0].key.len)=="saved-final");
    Reset();c=Call(&objects[3],frame,0,false);post(c);CHECK(S::g_pending.empty());
    external=true;externalActor=&objects[4];c=Call(&objects[3],frame);post(c);
    CHECK(S::g_pending.size()==1&&S::g_pending.count(4));
    Reset();c=Call(&objects[3],frame);CHECK(S::CompletedActor(c)==&objects[3]);
    objects[0].actorOff=8;objects[0].successOff=0;++objects[0].serial;
    c=Call(&objects[4],frame);CHECK(S::CompletedActor(c)==&objects[4]);
    Reset();objects[0].actorOff=8;objects[0].successOff=0;++objects[0].serial;
    Enqueue();CHECK(S::g_pending.size()==1);Pump();CHECK(session.sent.size()==1);
    Reset();objects[0].actorOff=12;++objects[0].serial;
    c.function=&objects[0];c.locals=frame.bytes;post(c);CHECK(S::g_pending.empty());
    objects[0].actorOff=-1;++objects[0].serial;post(c);CHECK(S::g_pending.empty());
    for(auto role:{coop::net::Role::Host,coop::net::Role::Client}) {
        Reset(role);session.sendOK=false;Enqueue();Pump();CHECK(session.sent.empty()&&records==0&&S::g_pending.size()==1);
        session.sendOK=true;recordOK=false;Pump();CHECK(session.sent.size()==1&&records==1&&S::g_pending.size()==1);
        recordOK=true;Pump();CHECK(session.sent.size()==1&&records==2&&S::g_pending.empty());
    }
    Reset();Enqueue();++objects[3].serial;Pump();CHECK(session.sent.empty()&&S::g_pending.empty()&&reads==0);
    Reset();Enqueue();objects[3].live=false;Pump();CHECK(session.sent.empty()&&S::g_pending.empty());
    Reset();Enqueue();objects[3].mirror=true;Pump();CHECK(session.sent.empty()&&S::g_pending.empty());
    for(int mode=0;mode<4;++mode){Reset();Enqueue();
        if(mode==0)++world;if(mode==1)++serial;if(mode==2)session.mode=coop::net::Role::Client;
        if(mode==3)S::ClearContainerExtractions();Pump();CHECK(session.sent.empty()&&S::g_pending.empty());}
    Reset();Enqueue();duringRead=[]{S::ClearContainerExtractions();};Pump();CHECK(session.sent.empty()&&records==0);
    Reset();Enqueue();duringMark=[]{++world;};Pump();CHECK(session.sent.empty()&&records==0);
    Reset();Enqueue();duringMark=[]{objects[3].key=L"new-identity";};Pump();CHECK(session.sent.empty()&&records==0);
    Pump();CHECK(session.sent.size()==1&&records==1);
    Reset();Enqueue();session.duringSend=[]{S::ClearContainerExtractions();Enqueue(4);};Pump();
    CHECK(session.sent.size()==1&&records==0&&S::g_pending.count(4));Pump();CHECK(session.sent.size()==2&&records==1);
    Reset();Enqueue();duringRecord=[]{S::ClearContainerExtractions();Enqueue(4);};Pump();CHECK(S::g_pending.count(4));
    Reset();session.sendOK=false;for(int i=3;i<23;++i)Enqueue(i);Pump();CHECK(attempts.size()==8);
    Pump();CHECK(attempts.size()==16&&attempts.back()==18); // cursor cannot starve high identities
    Reset();for(int i=3;i<132;++i)Enqueue(i);CHECK(S::g_pending.size()==128);
    Reset();objects[3].covers=false;Enqueue();Pump();CHECK(session.sent.size()==1&&records==0&&S::g_pending.empty());
    Reset();objects[3].covers=false;objects[3].food=true;Enqueue();Pump();CHECK(records==1);
    puts("PASS extraction scope, incarnation/frame/out validation, deferred final identity, refused spawn/state retry, both roles, fairness/cap and callback lifetime cancellation");
}
