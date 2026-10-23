#include "../../src/votv-coop/src/coop/props/explosion_sync.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace S=coop::explosion_sync;namespace X=ue_wrap::prop_explosion;namespace sg=ue_wrap::script_gate;
namespace coop::explosion_prop_motion { int observations=0,births=0; void Observe(void*,void*){++observations;} void ObserveBlast(void*){++births;} }
namespace test {
struct Obj {int id,serial=1;bool live=true;} objects[8]{{0},{1},{2},{3},{4},{5},{6},{7}};
auto& can=objects[1];auto& blast=objects[2];auto& local=objects[3];auto& foreign=objects[4];
uint32_t world=1;bool hooks=true,announced=true,episode=false,hand=false,exists=true,replayOk=true,breakOk=true,bodyActive=false;
int breaks=0,replays=0,destroys=0;std::function<void()> callback;
X::State state{{100,200,300},1,1000,50,1,40,100,2000,false,false};
void Callback(){auto c=std::move(callback);callback={};if(c)c();}
}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<8?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return i>=0&&i<8?&test::objects[i]:nullptr;}
bool IsLiveByIndex(void* a,int32_t i){return IsLive(a)&&ObjectAt(i)==a;}
std::wstring ClassNameOf(void* a){return a==&test::local||a==&test::foreign?L"mainPlayer_C":L"prop_C";}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return (void*)(uintptr_t)test::world;}void* WorldOf(void*){return CurrentWorld();}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::script_gate {
bool WatchName(const wchar_t*,int,PreFn,PostFn){return true;}bool WatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn){return true;}
void ResolvePendingNames(){}bool NameWatchLive(const wchar_t*,int){return test::hooks;}bool ClassNameWatchLive(const wchar_t*,const wchar_t*,int){return test::hooks;}
bool IsBodyActive(void* f,void*){return f&&test::bodyActive;}
}
namespace coop::net_pump {bool IsInAnnouncedWorld(void*){return test::announced;}}
namespace coop::world_load_episode {bool InEpisode(){return test::episode;}bool InReconcileWindow(){return test::episode;}}
namespace coop::hand_item {bool IsHandAxisActor(void*){return test::hand;}void* LocalHandActor(){return &test::objects[5];}}
namespace coop::players {bool Registry::IsLocal(void* a){return a==&test::local;}}
namespace coop::prop_element_tracker {void* FindLiveActorByKey(const std::wstring& k){return test::exists&&test::can.live&&k==L"can"?&test::can:nullptr;}}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){return L"can";}}
namespace ue_wrap::engine {bool DestroyActor(void* a){++test::destroys;static_cast<test::Obj*>(a)->live=false;return true;}}
namespace ue_wrap::prop_explosion {
bool IsGascan(void* a){return a==&test::can&&test::can.live;}
bool IsExplosion(void* a){return a==&test::blast&&test::blast.live;}
bool Valid(const State& s){return std::isfinite(s.damage)&&s.damage>=0&&s.damage<=10000&&std::isfinite(s.radius)&&s.radius>0&&s.radius<=10000;}
bool Read(void*,State& s){s=test::state;return true;}
bool BreakGascan(void* a,bool fire){++test::breaks;if(!test::breakOk)return false;sg::Call c{};c.object=a;c.function=&test::objects[6];c.tag=fire?0x45580002:0x45580001;c.depth=1;
 if(S::OnBreak(c)==sg::Verdict::Run){test::bodyActive=true;sg::Call b{};b.object=&test::blast;S::OnBegin(b);test::Callback();test::bodyActive=false;S::AfterBreak(c);}return true;}
bool Replay(const State&,const std::function<bool(void*)>& mark,const std::function<bool()>& current){
 if(!test::replayOk||!current())return false;if(!mark(&test::blast))return false;++test::replays;test::Callback();return true;}
}
using namespace test;coop::net::Session session;
void Reset(coop::net::Role role=coop::net::Role::Host){S::OnDisconnect();CHECK(S::g_sources.empty());++world;
 for(int i=0;i<8;++i)objects[i]={i};session={};session.mode=role;hooks=announced=exists=replayOk=breakOk=true;
 episode=hand=false;breaks=replays=destroys=0;callback={};coop::element::body=coop::element::reach=true;S::Install(&session);}
auto Packet(uint8_t op=0,uint32_t seq=1){auto p=S::Encode(state,L"can");p.op=op;p.sequence=seq;return p;}
sg::Verdict Break(){sg::Call c{};c.object=&can;c.depth=1;c.tag=0x45580001;auto v=S::OnBreak(c);if(v==sg::Verdict::Run)S::AfterBreak(c);return v;}
void NativeHost(){X::BreakGascan(&can,false);}
int main(){using coop::net::Role;using sg::Verdict;
 Reset();NativeHost();CHECK(coop::explosion_prop_motion::births==1);sg::Call hit{};hit.callerObject=&blast;hit.object=&can;hit.tag=0x45580006;
 S::OnEffect(hit);CHECK(coop::explosion_prop_motion::observations==1);
 hit.tag=0x45580004;S::OnEffect(hit);CHECK(coop::explosion_prop_motion::observations==1);
 hit.tag=0x45580006;hit.callerObject=&can;S::OnEffect(hit);CHECK(coop::explosion_prop_motion::observations==1);
 Reset(Role::Client);S::Receive(Packet(1),0);S::Tick();hit.callerObject=&blast;S::OnEffect(hit);CHECK(coop::explosion_prop_motion::observations==1);
 {const auto before=coop::explosion_prop_motion::births;const float damage=state.damage;state.damage=0;Reset();NativeHost();
 CHECK(coop::explosion_prop_motion::births==before+1);state.damage=damage;}
 Reset(Role::Client);CHECK(Break()==Verdict::Cancel&&S::g_requests.size()==1);can.live=false;CHECK(S::ConsumesClientDestroy(&can));++can.serial;CHECK(!S::ConsumesClientDestroy(&can));
 session.sendOk[0]=false;S::Tick();CHECK(S::g_requests.size()==1);session.sendOk[0]=true;S::Tick();CHECK(S::g_requests.empty()&&session.sent.size()==1);
 Reset(Role::Client);announced=false;CHECK(Break()==Verdict::Run);announced=true;episode=true;CHECK(Break()==Verdict::Run);episode=false;hand=true;CHECK(Break()==Verdict::Run);
 // An indexed world gascan remains host-owned while effect watches resolve.
 // The client can enqueue its intent without authoring an unobserved blast.
 hand=false;hooks=false;CHECK(Break()==Verdict::Cancel&&S::g_requests.size()==1);
 Reset();S::Receive(Packet(),1);S::Tick();CHECK(breaks==1&&destroys==1&&session.sent.size()==3);S::Receive(Packet(),1);S::Tick();CHECK(breaks==1);
 Reset();NativeHost();CHECK(S::g_blasts.size()==1);NativeHost();CHECK(S::g_blasts.size()==1);S::Receive(Packet(),1);S::Tick();CHECK(destroys==0&&session.sent.size()==3); // already consumed by native break
 Reset();S::Receive(Packet(),1);exists=false;S::Tick();CHECK(!breaks&&S::g_requests.size()==1);exists=true;S::Tick();CHECK(breaks==1);
 Reset();S::Receive(Packet(),1);coop::element::body=false;S::Tick();CHECK(!breaks);coop::element::body=true;S::Tick();CHECK(breaks==1);
 Reset();S::Receive(Packet(),1);coop::element::reach=false;S::Tick();CHECK(!breaks&&S::g_requests.empty());
 Reset();S::Receive(Packet(),1);S::OnPeerLeft(1);S::Tick();CHECK(!breaks);S::Receive(Packet(),1);S::Tick();CHECK(breaks==1);
 for(int i=0;i<4;++i){Reset();auto p=Packet();if(i==0)p.op=1;if(i==1)p.sequence=0;if(i==2)p.key.len=32;S::Receive(p,i==3?0:1);S::Tick();CHECK(!breaks);}
 Reset();S::Receive(Packet(),1);S::g_requests.front().at=S::Now()-11000;S::Tick();CHECK(!breaks);
 Reset();S::Receive(Packet(),1);breakOk=false;S::Tick();CHECK(destroys==0&&S::g_requests.size()==1);breakOk=true;S::Tick();CHECK(destroys==1);
 Reset();S::Receive(Packet(),1);callback=[]{S::OnDisconnect();};S::Tick();CHECK(!destroys&&S::g_sources.empty());
 Reset();S::Receive(Packet(),1);callback=[]{++can.serial;};S::Tick();CHECK(!destroys);
 Reset();NativeHost();++blast.serial;++can.serial;NativeHost();session.sendOk[1]=false;S::Tick();CHECK(session.sent.size()==4);session.sendOk[1]=true;S::Tick();CHECK(session.sent.size()==6&&session.sent[4].p.sequence<session.sent[5].p.sequence);
 Reset(Role::Client);S::Receive(Packet(1),1);S::Tick();CHECK(!replays);S::Receive(Packet(1),0);S::Tick();CHECK(replays==1);S::Receive(Packet(1),0);S::Tick();CHECK(replays==1);
 Reset(Role::Client);replayOk=false;S::Receive(Packet(1),0);S::Tick();CHECK(!replays&&S::g_blasts.size()==1);replayOk=true;S::Tick();CHECK(!replays);S::g_replayAt=0;S::Tick();CHECK(replays==1&&S::g_blasts.empty());
 Reset(Role::Client);S::Receive(Packet(1),0);S::g_blasts.front().at=S::Now()-11000;S::Tick();CHECK(!replays);
 Reset(Role::Client);S::Receive(Packet(1),0);callback=[]{S::OnDisconnect();};S::Tick();CHECK(replays==1&&S::g_blasts.empty());
 Reset(Role::Client);S::Receive(Packet(1),0);S::Tick();sg::Call c{};c.callerObject=&blast;c.object=&can;CHECK(S::OnEffect(c)==Verdict::Cancel&&S::IsReplica(&blast));c.object=&local;CHECK(S::OnEffect(c)==Verdict::Run);c.object=&foreign;CHECK(S::OnEffect(c)==Verdict::Cancel);c.object=&objects[5];CHECK(S::OnEffect(c)==Verdict::Run);c.object=&blast;CHECK(S::OnRune(c)==Verdict::Cancel);
 Reset();NativeHost();c.callerObject=&blast;c.object=&can;CHECK(S::OnEffect(c)==Verdict::Run);c.object=&foreign;CHECK(S::OnEffect(c)==Verdict::Cancel);c.object=&local;CHECK(S::OnEffect(c)==Verdict::Run);
 Reset();c={};c.object=&can;c.function=&objects[6];c.depth=1;CHECK(S::OnBreak(c)==Verdict::Run); // another watch cancels; no POST
 {sg::Call b{};b.object=&blast;S::OnBegin(b);CHECK(S::g_blasts.empty());}
 ++can.serial;CHECK(S::OnBreak(c)==Verdict::Run&&S::g_sources.size()==1);S::AfterBreak(c);
 Reset(Role::Client);S::Receive(Packet(1),0);episode=true;S::Tick();CHECK(!replays&&S::g_blasts.empty());
 puts("PASS actual explosion controller: request/destroy ownership, host validation, native dedup, lifetime, queue ordering/retry, no join replay, local damage/world authority");
}
