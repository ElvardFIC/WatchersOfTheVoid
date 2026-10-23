#include "../../src/votv-coop/src/coop/world/event_schedule_authority.cpp"
#include "../../src/votv-coop/src/coop/world/spawn_authority.cpp"
#include "facts_test_support.h"
namespace ue_wrap::script_gate {
bool WatchName(const facts::Function& row,int tag,PreFn pre,PostFn post){
    return WatchClassName(row.Class(),row.Name(),tag,pre,post);
}
}
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);} } while(0)

namespace test {
using Array = coop::event_schedule_authority::NameArray;
struct Obj { int id; int serial=1; bool live=true; void* cls=nullptr; Array rows{}; };
Obj cls{0}, slot{1}, otherClass{2};
Obj* objects[]={&cls,&slot,&otherClass};
void* world=reinterpret_cast<void*>(1);
bool propertyAvailable=true;
struct Watch { std::wstring cls,fn; int tag; ue_wrap::script_gate::PreFn pre; };
std::vector<Watch> watches;
ue_wrap::script_gate::Verdict Dispatch(const wchar_t* c,const wchar_t* fn,void* object,bool ours=false) {
    using namespace ue_wrap::script_gate;
    for(auto& w:watches)if(w.cls==c&&w.fn==fn) {
        Call call{};call.object=object;call.tag=w.tag;call.fromOurCode=ours;
        if(w.pre(call)==Verdict::Cancel)return Verdict::Cancel;
    }
    return Verdict::Run;
}
void Tick(){CHECK(Dispatch(L"saveSlot_C",L"settime",&slot,true)==ue_wrap::script_gate::Verdict::Run);}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* o){return o?static_cast<test::Obj*>(o)->id:-1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<3?test::objects[i]->serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* o,int32_t i){return i>=0&&i<3&&test::objects[i]==o&&test::objects[i]->live;}
void* ClassOf(void* o){return static_cast<test::Obj*>(o)->cls;}
int32_t FindPropertyOffset(void*,const wchar_t* name){CHECK(std::wstring(name)==L"allEvents");return test::propertyAvailable?offsetof(test::Obj,rows):-1;}
}
namespace ue_wrap::world_identity {
void* CurrentWorld(){return test::world;}
void* WorldOf(void* o){return o==&test::slot?test::world:nullptr;}
}
namespace ue_wrap::log { void Write(Level,const char*,...){} }
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t* cls,const wchar_t* fn,int tag,PreFn pre,PostFn){test::watches.push_back({cls,fn,tag,pre});return true;}
}

int main(){
    using namespace test;namespace E=coop::event_schedule_authority;namespace M=coop::spawn_authority;
    using V=ue_wrap::script_gate::Verdict;
    coop::net::Session s;slot.cls=&cls;
    ue_wrap::reflection::FName rows[4]={{1,0},{2,0},{3,0},{4,0}}, replacement[4]{};
    E::Install(&s);M::Install(&s);auto count=watches.size();E::Install(&s);M::Install(&s);CHECK(watches.size()==count);
    auto reset=[&]{E::OnDisconnect();slot.rows={rows,3,4};E::Install(&s);};
    reset();Tick();CHECK(slot.rows.num==3); // host keeps native schedule
    CHECK(Dispatch(L"impostorPropsMaster_C",L"gen",&slot)==V::Run);
    CHECK(Dispatch(L"ticker_flickerer_C",L"ReceiveTick",&slot)==V::Run);
    s.mode=coop::net::Role::Client;s.linked=false; // active loading window
    CHECK(Dispatch(L"ticker_flickerer_C",L"ReceiveTick",&slot)==V::Cancel);
    CHECK(Dispatch(L"ticker_flickerer_C",L"ReceiveTick",&slot,true)==V::Cancel);
    CHECK(Dispatch(L"ticker_flickerer_C",L"intComs_gamemodeBeginPlay",&slot)==V::Run);
    CHECK(Dispatch(L"ticker_treeSpawner_C",L"ReceiveTick",&slot)==V::Run);
    CHECK(Dispatch(L"ticker_bp7Spawner_C",L"ReceiveTick",&slot)==V::Run);
    CHECK(Dispatch(L"other_C",L"settime",&slot)==V::Run&&slot.rows.num==3);
    Tick();CHECK(slot.rows.num==0&&slot.rows.data==rows&&slot.rows.max==4);
    Tick();E::OnDisconnect();CHECK(slot.rows.num==3); // repeated entry keeps original count
    reset();Tick();otherClass.cls=&cls;otherClass.rows={replacement,2,4};
    CHECK(Dispatch(L"saveSlot_C",L"settime",&otherClass)==V::Run);
    CHECK(otherClass.rows.num==0);E::OnDisconnect();CHECK(slot.rows.num==3&&otherClass.rows.num==2);
    reset();Tick();slot.rows={rows,2,4};Tick();E::OnDisconnect();CHECK(slot.rows.num==2); // new boot held again
    reset();Tick();slot.rows={rows,2,4};E::OnDisconnect();CHECK(slot.rows.num==2); // rebuilt list survives
    reset();Tick();slot.rows.data=replacement;E::OnDisconnect();CHECK(slot.rows.num==0);
    reset();Tick();slot.rows.max=3;E::OnDisconnect();CHECK(slot.rows.num==0);
    reset();Tick();++slot.serial;E::OnDisconnect();CHECK(slot.rows.num==0); // recycled address/slot
    reset();Tick();world=reinterpret_cast<void*>(2);E::OnDisconnect();CHECK(slot.rows.num==0);
    reset();Tick();slot.live=false;E::OnDisconnect();CHECK(slot.rows.num==0);slot.live=true;
    reset();slot.rows={nullptr,3,4};Tick();CHECK(slot.rows.num==3);
    slot.rows={rows,5,4};Tick();CHECK(slot.rows.num==5);
    slot.rows={rows,-1,4};Tick();CHECK(slot.rows.num==-1);
    slot.rows={rows,100001,100001};Tick();CHECK(slot.rows.num==100001);
    // Missing reflected property fails open; a new class incarnation retries resolution.
    reset();slot.cls=&otherClass;propertyAvailable=false;Tick();CHECK(slot.rows.num==3);
    propertyAvailable=true;++otherClass.serial;Tick();CHECK(slot.rows.num==0);
    E::OnDisconnect();CHECK(slot.rows.num==3);slot.cls=&cls;
    E::Install(&s);s.active=false;Tick();CHECK(slot.rows.num==3);s.active=true;
    CHECK(Dispatch(L"impostorPropsMaster_C",L"gen",&slot,true)==V::Cancel);
    CHECK(Dispatch(L"prop_impostor_C",L"ReceiveTick",&slot,true)==V::Cancel);
    CHECK(Dispatch(L"prop_impostor_C",L"init",&slot)==V::Run);
    CHECK(Dispatch(L"prop_impostor_C",L"ReceiveBeginPlay",&slot)==V::Run);
    CHECK(Dispatch(L"prop_C",L"ReceiveTick",&slot)==V::Run);
    s.active=false;CHECK(Dispatch(L"impostorPropsMaster_C",L"gen",&slot)==V::Run);
    CHECK(Dispatch(L"ticker_flickerer_C",L"ReceiveTick",&slot)==V::Run);
    CHECK(Dispatch(L"prop_impostor_C",L"ReceiveTick",&slot)==V::Run);
    s.active=true;s.mode=coop::net::Role::Host;CHECK(Dispatch(L"prop_impostor_C",L"ReceiveTick",&slot)==V::Run);
    CHECK(Dispatch(L"ticker_flickerer_C",L"ReceiveTick",&slot)==V::Run);
    E::OnDisconnect();s.mode=coop::net::Role::Client;Tick();CHECK(slot.rows.num==3);
    puts("PASS authority2165: schedule suppression/restore, loading/reflected calls, array/object lifetime, mimic generator/cull roles and host-only flickerer with native initialization/solo recovery");
}
