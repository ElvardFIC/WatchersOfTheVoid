#include "../../src/votv-coop/src/coop/props/explosion_events.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace S=coop::explosion_events;namespace X=ue_wrap::prop_explosion;namespace sg=ue_wrap::script_gate;
namespace test {
struct Obj{int id,serial=1;bool live=true;} objects[12]{{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11}};
auto& source=objects[1];auto& blast=objects[2];auto& local=objects[3];auto& foreign=objects[4];auto& hand=objects[5];
uint32_t world=1,handGeneration=17;uint8_t self=0;bool hooks=true,announced=true,loading=false,mirror=false,handheld=false,spawnOk=true,exists=true,readOk=true,gascan=false;
int plays=0,destroys=0;bool authority=false;std::wstring sourceClass=L"prop_pipebomb_C",key=L"bomb";
X::Blast state;std::function<void()> callback;
void Callback(){auto cb=std::move(callback);callback={};if(cb)cb();}
}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<12?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return i>=0&&i<12?&test::objects[i]:nullptr;}
bool IsLiveByIndex(void* a,int32_t i){return IsLive(a)&&ObjectAt(i)==a;}
std::wstring ClassNameOf(void* a){return a==&test::local||a==&test::foreign?L"mainPlayer_C":test::sourceClass;}
void* ClassOf(void*){return &test::objects[6];}void* FindClassDefaultObject(const wchar_t*){return &test::objects[7];}
void* FindDispatchFunctionCached(void*,const wchar_t*){return &test::objects[6];}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return (void*)(uintptr_t)test::world;}void* WorldOf(void*){return CurrentWorld();}}
namespace ue_wrap::log{void Write(Level,const char*,...) {}}
namespace ue_wrap::script_gate{
bool WatchName(const wchar_t*,int,PreFn,PostFn){return true;}bool WatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn){return true;}
void ResolvePendingNames(){}bool NameWatchLive(const wchar_t*,int){return test::hooks;}bool ClassNameWatchLive(const wchar_t*,const wchar_t*,int){return test::hooks;}}
namespace ue_wrap::ufunction_hook {bool InstallPostHook(void*,PostNativeCallback,bool){return test::hooks;}}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){return test::key;}bool IsDescendantOfProp(void*){return test::sourceClass.rfind(L"prop_",0)==0;}}
namespace ue_wrap::prop_explosion {
bool IsGameplayBlast(void* a){return a==&test::blast&&test::blast.live;}
bool Valid(const Blast& b){return b.variant<=1&&std::isfinite(b.damage)&&b.damage>=0&&b.damage<=1e6f;}
bool ReadBlast(void*,Blast& b){b=test::state;return test::readOk;}
bool ReplayBlast(const Blast&,bool auth,const std::function<bool(void*)>& mark,const std::function<bool()>& current){
 if(!test::spawnOk||!current())return false;test::blast.live=true;++test::blast.serial;
 if(!mark(&test::blast))return false;test::authority=auth;++test::plays;
 sg::Call c{};c.object=&test::blast;CHECK(S::OnBegin(c)==sg::Verdict::Run);test::Callback();return true;}
}
namespace ue_wrap::engine {
bool TryGetActorLocation(void*,FVector& out){out={0,0,0};test::Callback();return true;}
bool DestroyActor(void* a){++test::destroys;static_cast<test::Obj*>(a)->live=false;return true;}
bool SetActorHiddenInGame(void*,bool){return true;}
}
namespace coop::net_pump {bool IsInAnnouncedWorld(void*){return test::announced;}}
namespace coop::world_load_episode {bool InEpisode(){return test::loading;}bool InReconcileWindow(){return test::loading;}}
namespace coop::prop_element_tracker {void* FindLiveActorByKey(const std::wstring& k){return test::exists&&test::source.live&&k==test::key?&test::source:nullptr;}}
namespace coop::prop_echo_suppress {bool IsMirrorSpawn(void*){return test::mirror;}}
namespace coop::hand_item {
bool IsMirrorActor(void*){return test::mirror;}bool IsHandAxisActor(void*){return test::handheld;}
void* LocalHandActor(){return test::handheld?&test::source:&test::hand;}
uint32_t Generation(uint8_t){return test::handGeneration;}
bool HeldClassIs(uint8_t,const wchar_t* c){return test::sourceClass==c;}bool LastHeldClassIs(uint8_t,const wchar_t* c){return test::sourceClass==c;}
}
namespace coop::players {bool Registry::IsLocal(void* a){return a==&test::local;}uint8_t Registry::LocalPeerId()const{return test::self;}}
namespace coop::explosion_sync {bool IsReplica(void*){return false;}bool CapturingGascan(){return test::gascan;}}
using namespace test;coop::net::Session session;
void Reset(coop::net::Role role=coop::net::Role::Host){S::OnDisconnect();++world;for(int i=0;i<12;++i)objects[i]={i};session={};session.mode=role;self=role==coop::net::Role::Host?0:1;
 hooks=announced=spawnOk=exists=readOk=true;loading=mirror=handheld=gascan=false;handGeneration=17;sourceClass=L"prop_pipebomb_C";key=L"bomb";plays=destroys=0;callback={};state={};coop::element::body=coop::element::reach=true;
 S::Install(&session);S::g_begin.Set(&objects[6]);}
auto Native(void* src=&source){S::Capture(nullptr,src,&blast);sg::Call c{};c.object=&blast;return S::OnBegin(c);}
S::P Packet(uint32_t seq=1,uint32_t ordinal=1){S::P p{};S::Encode(state,p);coop::net::WireKeyFromString(key,p.key);S::SetClass(p,&source);p.sequence=seq;p.ordinal=ordinal;p.origin=1;return p;}
void NewBlast(){blast.live=true;++blast.serial;}
int main(){using coop::net::Role;using sg::Verdict;
 Reset();CHECK(Native()==Verdict::Run);S::Tick();CHECK(session.sent.size()==3&&session.sent[0].p.ordinal==1&&!plays);
 Reset(Role::Client);CHECK(Native()==Verdict::Cancel);source.live=false;CHECK(S::ConsumesClientDestroy(&source));S::Tick();CHECK(session.sent.size()==2&&session.sent[0].p.op==0&&session.sent[1].p.op==2&&session.sent[0].p.sequence<session.sent[1].p.sequence);
 ++source.serial;CHECK(!S::ConsumesClientDestroy(&source));
 Reset();S::Receive(Packet(),1);S::Tick();CHECK(plays==1&&authority&&source.live&&session.sent.size()==3);auto death=Packet(2);death.op=2;S::Receive(death,1);S::Tick();CHECK(!source.live);S::Receive(Packet(3),1);S::Tick();CHECK(plays==1);
 Reset();CHECK(Native()==Verdict::Run);S::Receive(Packet(),1);S::Tick();CHECK(!plays&&session.sent.size()==3);
 Reset();S::Receive(Packet(),1);S::Tick();NewBlast();CHECK(Native()==Verdict::Cancel&&plays==1); // host timer echoes client's same keyed blast
 Reset();for(int i=0;i<5;++i){NewBlast();CHECK(Native()==Verdict::Run);}for(auto& e:S::Row(Packet())->events)e.at=S::Now()-3000;
 S::Receive(Packet(),1);S::Tick();CHECK(plays==1); // late joiner's ordinal one is independent of host ordinal five
 Reset();S::Receive(Packet(),1);S::Tick();S::Receive(Packet(2,2),1);S::Tick();CHECK(plays==2); // identical repeated explosions from same peer remain distinct
 Reset();S::Receive(Packet(),1);S::Tick();death=Packet(2);death.op=2;key=L"replacement";S::Receive(death,1);S::Tick();CHECK(source.live); // reused/key-changed source cannot be destroyed
 Reset();S::Receive(Packet(),1);callback=[]{S::OnPeerLeft(1);};S::Tick();CHECK(!plays); // peer leaves during authorization
 Reset();auto handEvent=Packet();handEvent.scope=1;handEvent.handGeneration=17;S::Receive(handEvent,1);callback=[]{session.ready[1]=false;S::OnPeerLeft(1);};S::Tick();CHECK(plays==1&&session.sent.size()==2); // finished blast still reaches remaining peers
 Reset();handGeneration=18;S::Receive(handEvent,1);S::Tick();CHECK(plays==1); // just-destroyed hand source
 Reset();handGeneration=19;S::Receive(handEvent,1);S::Tick();CHECK(!plays); // stale hand generation
 Reset();handGeneration=16;S::Receive(handEvent,1);S::Tick();CHECK(!plays&&!S::g_requests.empty());handGeneration=17;S::Tick();CHECK(plays==1);
 Reset();exists=false;S::Receive(Packet(),1);S::Tick();CHECK(!plays&&!S::g_requests.empty());exists=true;S::Tick();CHECK(plays==1);
 Reset();coop::element::body=false;S::Receive(Packet(),1);S::Tick();CHECK(!plays&&!S::g_requests.empty());coop::element::body=true;S::Tick();CHECK(plays==1);
 Reset();coop::element::reach=false;S::Receive(Packet(),1);S::Tick();CHECK(!plays&&S::g_requests.empty());
 Reset();S::Receive(Packet(),1);S::OnPeerLeft(1);S::Tick();CHECK(!plays);S::Receive(Packet(),1);S::Tick();CHECK(plays==1);
 Reset();S::Receive(Packet(),1);callback=[]{S::OnDisconnect();};S::Tick();CHECK(!plays&&S::g_requests.empty());
 Reset(Role::Client);handheld=true;CHECK(Native()==Verdict::Cancel);CHECK(S::g_requests.front().p.scope==1&&S::g_requests.front().p.handGeneration==17);
 Reset(Role::Client);key=L"";CHECK(Native()==Verdict::Cancel&&S::g_requests.front().p.scope==2);
 Reset(Role::Client);key=L"";sourceClass=L"lightningStrike_C";CHECK(Native()==Verdict::Cancel&&S::g_requests.empty());
 Reset();key=L"";sourceClass=L"lightningStrike_C";CHECK(Native()==Verdict::Run&&S::g_broadcast.size()==1);
 Reset(Role::Client);mirror=true;CHECK(Native()==Verdict::Cancel&&S::g_requests.empty());
 Reset(Role::Client);loading=true;CHECK(Native()==Verdict::Cancel&&S::g_requests.empty());
 Reset();gascan=true;CHECK(Native()==Verdict::Run&&S::g_broadcast.empty());
 Reset();Native();NewBlast();Native();session.sendOk[1]=false;S::Tick();CHECK(session.sent.size()==4);session.sendOk[1]=true;S::Tick();CHECK(session.sent.size()==6&&session.sent[4].p.sequence<session.sent[5].p.sequence);
 Reset(Role::Client);auto p=Packet();p.op=1;p.origin=0;S::Receive(p,2);S::Tick();CHECK(!plays);S::Receive(p,0);S::Tick();CHECK(plays==1&&!authority);S::Receive(p,0);S::Tick();CHECK(plays==1);
 sg::Call c{};c.callerObject=&blast;c.object=&source;CHECK(S::Effect(c)==Verdict::Cancel&&S::IsReplica(&blast));c.object=&local;CHECK(S::Effect(c)==Verdict::Run);c.object=&foreign;CHECK(S::Effect(c)==Verdict::Cancel);c.object=&hand;CHECK(S::Effect(c)==Verdict::Run);c.object=&blast;CHECK(S::Rune(c)==Verdict::Cancel);
 Reset(Role::Client);p=Packet();p.op=1;spawnOk=false;S::Receive(p,0);S::Tick();CHECK(!plays&&S::g_replays.size()==1);spawnOk=true;S::g_retry=0;S::Tick();CHECK(plays==1);
 Reset();S::Receive(Packet(),1);S::g_requests.front().at=S::Now()-11000;S::Tick();CHECK(!plays);
 for(int i=0;i<10;++i){Reset();auto bad=Packet();if(i==0)bad.op=1;if(i==1)bad.origin=2;if(i==2)bad.variant=3;if(i==3)bad.damage=NAN;if(i==4)bad.sourceClass.len=64;if(i==5)bad.key.len=32;if(i==6)bad.scope=3;if(i==7)bad.reserved=1;if(i==8)bad.sequence=0;if(i==9)bad.ordinal=0;S::Receive(bad,1);S::Tick();CHECK(!plays);}
 puts("PASS actual general explosion controller: capture/source identity, world/hand/projectile/event routes, host authority, destroy ordering, dedup, native echo, gates, retries, peer/world lifecycle");
}
