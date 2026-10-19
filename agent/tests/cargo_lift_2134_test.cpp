#include "../../src/votv-coop/src/coop/world/cargo_lift_sync.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace S=coop::cargo_lift_sync;namespace L=ue_wrap::cargo_lift;
namespace test {
struct Obj {int id,serial=1;bool live=true;} actor{1},other{2};
uint32_t world=1;bool ready=true,applyOk=true,announced=true,buttonOk=true,hooks=true;
int applies=0,activations=0,releases=0;L::State state;
std::function<void()> callback;void Callback(){auto c=std::move(callback);callback={};if(c)c();}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
int32_t SlotSerial(int32_t i){return i==1?test::actor.serial:test::other.serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* a,int32_t i){return a&&static_cast<test::Obj*>(a)->live&&InternalIndexOf(a)==i;}
int32_t ParamOffset::Of(void*){return std::wstring(param_)==L"action"?0:std::wstring(param_)==L"lookAtComponent"?8:std::wstring(param_)==L"player"?16:0;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return (void*)(uintptr_t)test::world;}void* WorldOf(void*){return CurrentWorld();}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::script_gate {bool WatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn){return true;}void ResolvePendingNames(){}bool ClassNameWatchLive(const wchar_t*,const wchar_t*,int){return test::hooks;}}
namespace coop::net_pump {bool IsInAnnouncedWorld(void*){return test::announced;}}
namespace ue_wrap::cargo_lift {
bool IsLift(void* a){return a==&test::actor||a==&test::other;}
std::wstring Key(void*){return L"cargoLift_1";}
void Enumerate(std::vector<CachedObjRef>& out){out.clear();if(test::ready){CachedObjRef r;r.Set(&test::actor);out.push_back(r);}}
int ButtonIndex(void*,void* c){return c==&test::other?2:-1;}
bool ButtonPosition(void*,int b,FVector& p){p={10,20,float(600+b)};test::Callback();return test::buttonOk;}
bool Read(void*,State& s){s=test::state;test::Callback();return test::ready;}
bool Activate(void*,int,void*){if(test::state.moving)return false;++test::activations;test::state.moving=true;test::Callback();return true;}
bool Valid(const State& s){return std::isfinite(s.alpha)&&s.alpha>=0&&s.alpha<=1;}
bool Apply(void*,const State& s,Mirror& m,bool,const std::function<bool()>& current){++test::applies;m.token=1;test::Callback();if(!current()||!test::applyOk)return false;test::state=s;return true;}
void Release(Mirror& m,const std::function<bool()>&){if(m.token)++test::releases;m={};}
}
using namespace test;
coop::net::Session session;
void Reset(coop::net::Role role=coop::net::Role::Host){S::OnDisconnect();++world;actor={1};other={2};session={};session.mode=role;ready=applyOk=announced=buttonOk=hooks=true;applies=activations=releases=0;state={};callback={};coop::element::body=coop::element::reach=true;coop::players::Registry::Get().puppet.actor=&other;S::Install(&session);}
void Tick(){S::g_nextScan=S::g_nextSend=0;S::Tick();}
coop::net::CargoLiftPayload Packet(uint8_t op=0,uint32_t seq=1){auto p=S::Encode(L"cargoLift_1",state);p.op=op;p.sequence=seq;p.button=2;return p;}
int main(){using coop::net::Role;
 Reset();Tick();CHECK(session.sent.size()==1);Tick();CHECK(session.sent.size()==1);state.position.Z=100;Tick();CHECK(session.sent.back().state.z==100);
 Reset();S::QueueConnect(2);ready=false;Tick();CHECK(S::g_seed==4);ready=true;Tick();CHECK(session.sent.back().slot==2);
 Reset();Tick();session.sendOk=false;S::QueueConnect(2);Tick();CHECK(S::g_rows[0].seeds==4);session.sendOk=true;Tick();CHECK(S::g_rows[0].seeds==0&&session.sent.back().slot==2);
 Reset();auto p=Packet();S::Receive(p,1);Tick();CHECK(activations==1&&coop::element::checked.Z==602);S::Receive(p,1);state.moving=false;Tick();CHECK(activations==1);
 Reset();p=Packet();S::Receive(p,1);ready=false;Tick();CHECK(activations==0&&S::g_requests.size()==1);ready=true;Tick();CHECK(activations==1);
 Reset();S::Receive(p,1);coop::element::body=false;Tick();CHECK(S::g_requests.size()==1);coop::element::body=true;Tick();CHECK(activations==1);
 Reset();S::Receive(p,1);coop::element::reach=false;Tick();CHECK(activations==0&&S::g_requests.empty());
 Reset();S::Receive(p,1);S::g_requests.front().at=S::Now()-4000;Tick();CHECK(activations==0);
 Reset();S::Receive(p,1);S::OnPeerLeft(1);CHECK(S::g_requests.empty());Tick();CHECK(activations==0);S::Receive(p,1);Tick();CHECK(activations==1);
 for(int i=0;i<7;++i){Reset();auto bad=p;if(i==0)bad.op=1;if(i==1)bad.sequence=0;if(i==2)bad.button=3;if(i==3)bad.key.len=32;if(i==4)bad.key.data[0]=0;S::Receive(bad,i==5?0:i==6?4:1);Tick();CHECK(activations==0);}
 Reset();for(int i=1;i<=20;++i)S::Receive(Packet(0,i),1);CHECK(S::g_requests.size()==4);
 Reset(Role::Client);p=Packet(1);p.alpha=.5;p.z=300;ready=false;S::Receive(p,0);Tick();CHECK(applies==0);ready=true;Tick();CHECK(applies==1&&state.position.Z==300);
 auto stale=p;stale.z=900;S::Receive(stale,0);CHECK(S::g_rows[0].canonical.z==300);stale.sequence=2;S::Receive(stale,1);CHECK(S::g_rows[0].canonical.sequence==1);Tick();CHECK(state.position.Z==300);
 p.sequence=2;p.z=350;S::Receive(p,0);S::g_rows[0].at=S::Now()-25;Tick();CHECK(state.position.Z>315&&state.position.Z<335);
 S::g_rows[0].at=S::Now()-60;Tick();CHECK(state.position.Z==350);++actor.serial;Tick();CHECK(state.position.Z==350);
 Reset(Role::Client);p=Packet(1);S::Receive(p,0);applyOk=false;Tick();CHECK(!S::g_rows[0].painted);applyOk=true;Tick();CHECK(S::g_rows[0].painted);S::OnDisconnect();CHECK(releases==1);
 for(int i=0;i<5;++i){Reset(Role::Client);p=Packet(1);S::Receive(p,0);callback=[i,p]{if(i==0)S::OnDisconnect();if(i==1)++world;if(i==2)++actor.serial;if(i==3){auto q=p;q.sequence=2;S::Receive(q,0);}if(i==4){S::OnDisconnect();S::Install(&session);S::Receive(p,0);S::g_rows[0].actor.Set(&actor);}};Tick();CHECK(!S::g_rows[0].painted);}
 Reset(Role::Client);S::Receive(Packet(1),0);callback=[]{Tick();};Tick();CHECK(applies==1);
 Reset();S::Receive(Packet(),1);callback=[]{S::OnDisconnect();};Tick();CHECK(activations==0);
 Reset();S::Receive(Packet(),1);callback=[]{++actor.serial;};Tick();CHECK(activations==0);
 Reset();S::Receive(Packet(),1);callback=[]{S::OnPeerLeft(1);};Tick();CHECK(activations==0);
 Reset(Role::Client);S::Receive(Packet(1),0);hooks=false;Tick();CHECK(applies==0);hooks=true;Tick();CHECK(applies==1);
 Reset();Tick();callback=[]{++world;};Tick();CHECK(session.sent.size()==1);
 Reset(Role::Client);uint8_t locals[32]{};locals[0]=4;void* comp=&other;void* who=coop::players::Registry::Get().Local();std::memcpy(locals+8,&comp,8);std::memcpy(locals+16,&who,8);
 ue_wrap::script_gate::Call c{};c.object=&actor;c.locals=locals;
 CHECK(S::OnAction(c)==ue_wrap::script_gate::Verdict::Cancel);session.sendOk=false;Tick();CHECK(S::g_requests.size()==1);session.sendOk=true;Tick();CHECK(S::g_requests.empty()&&session.sent.back().state.button==2);
 int32_t ep=1355;std::memcpy(locals,&ep,4);CHECK(S::OnGraph(c)==ue_wrap::script_gate::Verdict::Run);ep=15;std::memcpy(locals,&ep,4);CHECK(S::OnGraph(c)==ue_wrap::script_gate::Verdict::Cancel);
 Reset(Role::Host);CHECK(S::OnGraph(c)==ue_wrap::script_gate::Verdict::Run);
 auto a=L::State{},b=a;a.rotation.Yaw=179;b.rotation.Yaw=-179;CHECK(std::fabs(S::Blend(a,b,.5f).rotation.Yaw)==180);
 a.rotation.Yaw=360;b.rotation.Yaw=-360;CHECK(S::Blend(a,b,.5f).rotation.Yaw==0);
 puts("PASS lift actual controller: authority, reach, bounded retries/duplicates, seeds, readiness, interpolation, teardown and callback lifetimes");
}
