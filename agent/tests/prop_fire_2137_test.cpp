#include "../../src/votv-coop/src/coop/props/prop_fire_sync.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace S=coop::prop_fire_sync;namespace F=ue_wrap::prop_fire;
namespace test {
struct Obj {int id,serial=1;bool live=true;} actor{1},other{2};
uint32_t world=1;bool ready=true,applyOk=true,announced=true,hooks=true,hand=false,replica=false;
int applies=0,ignites=0,extinguishes=0;F::State state;
std::function<void()> callback;void Callback(){auto c=std::move(callback);callback={};if(c)c();}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
int32_t SlotSerial(int32_t i){return i==1?test::actor.serial:test::other.serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* a,int32_t i){return a&&static_cast<test::Obj*>(a)->live&&InternalIndexOf(a)==i;}
int32_t ParamOffset::Of(void*){return 0;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return (void*)(uintptr_t)test::world;}void* WorldOf(void*){return CurrentWorld();}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::script_gate {bool WatchName(const wchar_t*,int,PreFn,PostFn){return true;}void ResolvePendingNames(){}bool NameWatchLive(const wchar_t*,int){return test::hooks;}}
namespace coop::net_pump {bool IsInAnnouncedWorld(void*){return test::announced;}}
namespace coop::explosion_sync {bool IsReplica(void*){return test::replica;}}
namespace coop::explosion_events {bool IsReplica(void*){return test::replica;}}
namespace coop::hand_item {bool IsHandAxisActor(void*){return test::hand;}}
namespace coop::prop_element_tracker {void* FindLiveActorByKey(const std::wstring& k){return test::ready?(k==L"box"?&test::actor:k==L"other"?&test::other:nullptr):nullptr;}}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void* a){return a==&test::actor?L"box":a==&test::other?L"other":L"";}}
namespace ue_wrap::prop_fire {
bool IsProp(void* a){return (a==&test::actor||a==&test::other)&&static_cast<test::Obj*>(a)->live;}
void* Owner(void* c){return c==(void*)123?&test::other:nullptr;}
bool Read(void*,State& s){s=test::state;return true;}
bool Valid(const State& s){return std::isfinite(s.fuel)&&s.fuel>=0&&s.fuel<=60&&std::isfinite(s.fuelSpeed)&&s.fuelSpeed>=0&&s.fuelSpeed<=100&&(!s.burning||s.fuel>0);}
void CollectBurning(std::vector<CachedObjRef>& out){out.clear();if(test::ready&&test::state.burning){CachedObjRef r;r.Set(&test::actor);out.push_back(r);}}
bool Ignite(void*,float fuel){++test::ignites;test::state={true,fuel,1};test::Callback();return test::applyOk;}
bool Extinguish(void*){++test::extinguishes;test::state={};test::Callback();return test::applyOk;}
bool Apply(void*,const State& s,const std::function<bool()>& current){++test::applies;test::Callback();if(!current()||!test::applyOk)return false;test::state=s;return true;}
}
using namespace test;
coop::net::Session session;
void Reset(coop::net::Role role=coop::net::Role::Host){S::OnDisconnect();++world;actor={1};other={2};session={};session.mode=role;ready=applyOk=hooks=true;hand=false;announced=role==coop::net::Role::Client;applies=ignites=extinguishes=0;state={};callback={};coop::element::body=coop::element::reach=true;S::Install(&session);}
void Tick(){S::g_poll=0;S::Tick();}
coop::net::PropFirePayload Packet(uint8_t op=0,uint32_t seq=1){coop::net::PropFirePayload p{};coop::net::WireKeyFromString(L"box",p.key);p.op=op;p.sequence=seq;p.fuel=30;p.fuelSpeed=1;p.burning=op==2;return p;}
ue_wrap::script_gate::Verdict Verb(void* a=&actor,int tag=0x46490001,void* caller=nullptr){float fuel=30;ue_wrap::script_gate::Call c{};c.object=a;c.locals=reinterpret_cast<uint8_t*>(&fuel);c.tag=tag;c.callerObject=caller;return S::OnVerb(c);}
int main(){using coop::net::Role;using ue_wrap::script_gate::Verdict;
 Reset();state={true,30,1};Tick();CHECK(session.sent.size()==1&&!announced&&session.sent[0].state.burning);
 state={};Tick();CHECK(!session.sent.back().state.burning);auto count=session.sent.size();Tick();CHECK(session.sent.size()==count);S::QueueConnect(2);Tick();CHECK(session.sent.back().slot==2&&!session.sent.back().state.burning);
 Reset();auto p=Packet();S::Receive(p,1);Tick();CHECK(ignites==1&&session.sent.back().state.burning);S::Receive(p,1);Tick();CHECK(ignites==1);
 p.op=1;p.sequence=2;S::Receive(p,1);Tick();CHECK(extinguishes==1&&!session.sent.back().state.burning);
 Reset();S::Receive(Packet(),1);ready=false;Tick();CHECK(!ignites&&S::g_requests.size()==1);ready=true;Tick();CHECK(ignites==1);
 Reset();S::Receive(Packet(),1);coop::element::body=false;Tick();CHECK(!ignites&&!S::g_requests.empty());coop::element::body=true;Tick();CHECK(ignites==1);
 Reset();S::Receive(Packet(),1);coop::element::reach=false;Tick();CHECK(!ignites&&S::g_requests.empty());
 Reset();S::Receive(Packet(),1);S::g_requests.front().at=S::Now()-6000;Tick();CHECK(!ignites);
 Reset();S::Receive(Packet(),1);S::OnPeerLeft(1);Tick();CHECK(!ignites);S::Receive(Packet(),1);Tick();CHECK(ignites==1);
 for(int i=0;i<10;++i){Reset();p=Packet();if(i==0)p.op=2;if(i==1)p.sequence=0;if(i==2)p.fuel=INFINITY;if(i==3)p.fuel=-1;if(i==4)p.key.len=32;if(i==5)p.reserved=1;if(i==6)p.key.data[0]=0;if(i==7)p.fuel=61;S::Receive(p,i==8?0:i==9?4:1);Tick();CHECK(!ignites);}
 Reset();state={true,30,1};session.sendOk=false;S::QueueConnect(2);Tick();CHECK(S::g_rows[0].dirty&&S::g_rows[0].seeds==4);session.sendOk=true;Tick();CHECK(!S::g_rows[0].dirty&&!S::g_rows[0].seeds);
 Reset(Role::Client);p=Packet(2);ready=false;S::Receive(p,0);Tick();CHECK(!applies);ready=true;Tick();CHECK(applies==1&&state.burning);S::Receive(p,0);Tick();CHECK(applies==1);
 p.sequence=2;p.burning=0;p.fuel=0;S::Receive(p,0);Tick();CHECK(applies==2&&!state.burning);S::Receive(Packet(2),0);Tick();CHECK(applies==2);
 Reset(Role::Client);S::Receive(Packet(2),1);Tick();CHECK(!applies);S::Receive(Packet(2),0);announced=false;Tick();CHECK(!applies);announced=true;Tick();CHECK(applies==1);
 Reset(Role::Client);S::Receive(Packet(2),0);hand=true;Tick();CHECK(!applies);hand=false;Tick();CHECK(applies==1);
 Reset(Role::Client);S::Receive(Packet(2),0);S::g_rows[0].received=S::Now()-40000;Tick();CHECK(applies==1&&!state.burning);
 Reset(Role::Client);S::Receive(Packet(2),0);S::g_rows[0].received=S::Now()+1000;Tick();CHECK(applies==1&&state.fuel==30);
 Reset();S::Receive(Packet(),1);S::g_requests.front().at=S::Now()+1000;Tick();CHECK(ignites==1);
 Reset(Role::Client);state={true,23,1};S::OnHandReleased(&actor);CHECK(S::g_requests.size()==1&&S::g_requests.front().p.fuel==23);Tick();CHECK(session.sent.size()==1);
 Reset(Role::Client);state={true,23,1};hand=true;S::OnHandReleased(&actor);CHECK(S::g_requests.empty());
 Reset();state={true,23,1};S::OnHandReleased(&actor);CHECK(S::g_requests.empty());
 Reset(Role::Client);S::Receive(Packet(2),0);applyOk=false;Tick();CHECK(!S::g_rows[0].applied);applyOk=true;Tick();CHECK(S::g_rows[0].applied);
 Reset(Role::Client);S::Receive(Packet(2),0);callback=[]{S::Receive(Packet(2,2),0);};Tick();CHECK(!S::g_rows[0].applied);Tick();CHECK(S::g_rows[0].applied);
 for(int i=0;i<3;++i){Reset(Role::Client);S::Receive(Packet(2),0);callback=[i]{if(i==0)S::OnDisconnect();if(i==1)++world;if(i==2)++actor.serial;};Tick();CHECK(!S::g_rows[0].applied);}
 Reset(Role::Client);CHECK(Verb()==Verdict::Cancel);CHECK(Verb(&other)==Verdict::Cancel);CHECK(Verb(&actor,0x46490002)==Verdict::Cancel);CHECK(S::g_requests.size()==2&&S::g_requests[0].p.sequence<S::g_requests[1].p.sequence);
 session.sendOk=false;Tick();CHECK(S::g_requests.size()==2);session.sendOk=true;Tick();CHECK(session.sent.size()==2&&session.sent[0].state.sequence<session.sent[1].state.sequence);
 Reset(Role::Client);replica=true;CHECK(Verb()==Verdict::Cancel&&S::g_requests.empty());CHECK(Verb((void*)456)==Verdict::Run);replica=false;
 Reset(Role::Client);CHECK(Verb(&actor,0x46490001,(void*)123)==Verdict::Cancel&&S::g_requests.empty());CHECK(Verb((void*)456)==Verdict::Run); // native player ignition unaffected
 {S::Applying apply;CHECK(Verb()==Verdict::Run);}CHECK(S::g_applying==0);
 ue_wrap::script_gate::Call c{};c.object=&actor;CHECK(S::OnDamage(c)==Verdict::Cancel);c.object=(void*)456;CHECK(S::OnDamage(c)==Verdict::Run);
 Reset();S::Receive(Packet(),1);applyOk=false;callback=[]{auto off=Packet(1,2);S::Receive(off,1);};Tick();CHECK(S::g_requests.size()==1&&S::g_requests.front().p.op==1);applyOk=true;Tick();CHECK(!state.burning&&S::g_requests.empty());
 Reset();S::Receive(Packet(),1);S::Receive(Packet(1,2),1);Tick();CHECK(!ignites&&extinguishes==1);
 Reset();state={true,30,1};hooks=false;Tick();CHECK(session.sent.empty());hooks=true;Tick();CHECK(!session.sent.empty());
 puts("PASS actual fire controller: authority, expiry, late join, reach, ready role, sequences/coalescing, failures, native local player, hand exclusion and lifecycle");
}
