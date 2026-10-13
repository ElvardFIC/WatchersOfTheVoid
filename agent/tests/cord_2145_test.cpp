#include "../../src/votv-coop/src/coop/interactables/cord_sync.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace S=coop::cord_sync;
namespace test {
struct Obj {int id=0,serial=1,type=0;bool live=true,drag=false,power=false;void* socket=nullptr;void* occupant=nullptr;std::wstring key;};
Obj objects[512];uint32_t world=1;int identities=0,connects=0,disconnects=0,powers=0;uint64_t now=1000;int writes=0;bool connectOk=true;std::function<void()> callback;
void Callback(){auto f=std::move(callback);callback={};if(f)f();}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<512?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* a,int32_t i){return i>=0&&i<512&&a==&test::objects[i]&&test::objects[i].live;}
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
const FName& NameOf(void*){static FName n{};return n;}
bool NameStartsWith(const FName&,const wchar_t*){return false;}
bool NameEquals(const FName&,const wchar_t*){return false;}
std::wstring ToString(const FName&){return L"socket";}
int32_t ParamOffset::Of(void*){return 0;}
int32_t FunctionFrameSize(void*){return 0;} // This release/pose test does not provide reflected intent frames.
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return (void*)(uintptr_t)test::world;}void* WorldOf(void*){return CurrentWorld();}}
namespace ue_wrap::object_index {uint64_t ClassSetVersion(){return 1;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn){return true;}
bool ClassNameWatchLive(const wchar_t*,const wchar_t*,int){return true;}void ResolvePendingNames(){}
}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void* a){++test::identities;return static_cast<test::Obj*>(a)->key;}}
namespace ue_wrap::engine {void* ParentActorOf(void*,std::wstring*){return nullptr;}}
namespace ue_wrap::cord {
bool IsCord(void* a){return a&&static_cast<test::Obj*>(a)->type==1;}
bool IsSocket(void* a){return a&&static_cast<test::Obj*>(a)->type==2;}
void* Parent(void*){return nullptr;}bool InCableReach(void*,void*){return true;}
void* Socket(void* a){return a?static_cast<test::Obj*>(a)->socket:nullptr;}
void* Occupant(void* a){return a?static_cast<test::Obj*>(a)->occupant:nullptr;}
bool Dragging(void* a){return a&&static_cast<test::Obj*>(a)->drag;}
bool Powered(void* a,bool& out){out=static_cast<test::Obj*>(a)->power;return true;}
bool SetPower(void* a,bool on){auto& v=*static_cast<test::Obj*>(a);if(v.power!=on){v.power=on;++test::powers;test::Callback();}return true;}
bool Disconnect(void* a){auto& v=*static_cast<test::Obj*>(a);if(v.socket)static_cast<test::Obj*>(v.socket)->occupant=nullptr;v.socket=nullptr;++test::disconnects;test::Callback();return true;}
bool Connect(void* a,void* b){auto& v=*static_cast<test::Obj*>(a);auto& s=*static_cast<test::Obj*>(b);
 if(v.socket==b&&s.occupant==a)return true;if(!test::connectOk)return false;
 ++test::connects;v.socket=b;s.occupant=a;test::Callback();return true;}
}
namespace ue_wrap::cord {
bool ReadHeadPose(void*,FVector& p,FRotator& r){p={1,2,3};r={};return true;}
bool WriteHeadPose(void*,const FVector&,const FRotator&){++test::writes;return true;}
}
namespace coop::active_drive {uint64_t NowMs(){return test::now;}}
namespace coop::element {std::wstring PortableWireKey(void* a){return static_cast<test::Obj*>(a)->key;}}
namespace coop::element::scan_hub {void Register(const Consumer&) {}}
namespace coop::net_pump {bool IsInAnnouncedWorld(void*){return true;}}
namespace coop::prop_element_tracker {element::ElementId GetPropElementIdForActor(void* a){return static_cast<test::Obj*>(a)->id+1;}}
namespace coop::remote_prop {element::ElementId ResolveMirrorEidByActor(void*,bool){return element::kInvalidId;}}
using namespace test;coop::net::Session session;
void Reset(int n=1){S::OnDisconnect();++world;session={};session.mode=coop::net::Role::Client;S::Install(&session);S::ResetWorld();
 for(int i=0;i<512;++i){objects[i]={};objects[i].id=i;objects[i].serial=int(world);objects[i].key=L"k"+std::to_wstring(i);}
 for(int i=0;i<n;++i){objects[i].type=1;objects[n+i].type=2;objects[i].socket=&objects[n+i];objects[n+i].occupant=&objects[i];
  ue_wrap::CachedObjRef a,b;a.Set(&objects[i]);b.Set(&objects[n+i]);S::g_actors.push_back(a);S::g_actors.push_back(b);}
 identities=connects=disconnects=powers=writes=0;now=1000;callback={};connectOk=true;}
coop::net::CordStatePayload Packet(int cord=0,int socket=1){coop::net::CordStatePayload p{};S::Identity(&objects[cord],p.cord);S::Identity(&objects[socket],p.socket);return p;}
int main(){
 Reset(100);for(int i=0;i<100;++i)S::Apply(Packet(i,100+i));identities=0;S::g_next=0;S::Tick();
 std::printf("100 idle cords +100 sockets: %d identity reads per client repaint, native mutations=%d\n",identities,connects+disconnects+powers);
#ifdef PERF_BASELINE
 CHECK(identities==20000);return 0;
#else
 CHECK(identities==200&&connects+disconnects+powers==0);
 identities=0;S::g_next=0;S::Tick();CHECK(identities==200);
 Reset();auto p=Packet();identities=0;S::Apply(p);S::Apply(p);CHECK(identities==0);S::Tick();CHECK(identities==2&&!connects);
 Reset();p=Packet();objects[0].socket=nullptr;objects[1].occupant=nullptr;S::Apply(p);S::Tick();CHECK(connects==1);
 Reset();p=Packet();objects[0].drag=true;p.socket={};S::Apply(p);S::Tick();CHECK(!disconnects);objects[0].drag=false;S::g_next=0;S::Tick();CHECK(disconnects==1);
 Reset();p=Packet();p.powered=1;S::Apply(p);S::Tick();CHECK(powers==1);S::g_next=0;S::Tick();CHECK(powers==1);
 Reset();p=Packet();p.socket={};S::Apply(p);callback=[]{S::OnDisconnect();};S::Tick();CHECK(disconnects==1&&S::g_pending.Rows().empty());
 Reset();p=Packet();p.powered=1;objects[0].socket=nullptr;objects[1].occupant=nullptr;S::Apply(p);callback=[]{++objects[0].serial;};S::Tick();CHECK(powers==1&&!connects);
 Reset();p=Packet();S::Apply(p);++world;S::Tick();CHECK(S::g_pending.Rows().empty()&&!connects);
 Reset();p=Packet();objects[0].socket=nullptr;objects[1].occupant=nullptr;connectOk=false;S::Apply(p);S::Tick();CHECK(!connects);connectOk=true;S::g_next=0;S::Tick();CHECK(connects==1);
 Reset(2);p=Packet(0,2);objects[1].key=objects[0].key;p.socket={};S::Apply(p);S::Tick();CHECK(!disconnects); // duplicate identity refuses, never chooses first
 Reset();p=Packet();S::Apply(p);objects[0].key=L"changed";S::Tick();CHECK(!connects&&!powers); // re-read identity on each pass
 Reset(2);p=Packet(0,2);p.powered=1;auto second=Packet(1,3);second.powered=1;S::Apply(p);S::Apply(second);
 callback=[]{objects[1].key=L"changed by callback";};S::Tick();CHECK(powers==1); // mutation invalidates pass cache
 Reset();p=Packet();p.powered=1;S::Apply(p);callback=[]{S::Tick();};S::Tick();CHECK(powers==1); // no reentrant pass
 Reset();p=Packet();p.powered=1;S::Apply(p);callback=[p]{auto newer=p;newer.powered=0;S::Apply(newer);};S::Tick();S::Tick();CHECK(powers==2&&!objects[1].power); // callback's latest canonical survives the current snapshot

 Reset();objects[0].socket=nullptr;objects[1].occupant=nullptr;
 coop::net::PropPoseSnapshot pose{};coop::net::WireKeyFromString(L"__mvcord_k0",pose.key);pose.holdGen=1;pose.x=10;
 S::ApplyDragPose(pose,0);CHECK(writes==1);
 coop::net::PropReleasePayload release{};release.key=pose.key;release.holdGen=1;
 S::ApplyDragRelease(release,0);CHECK(disconnects==1);S::ApplyDragPose(pose,0);CHECK(writes==1);
 pose.holdGen=2;S::ApplyDragPose(pose,0);CHECK(writes==2);S::ApplyDragRelease(release,0);CHECK(disconnects==1);
 now+=1600;S::Tick();CHECK(disconnects==2);S::ApplyDragPose(pose,0);CHECK(writes==3);
 S::OnPeerLeft(0);CHECK(disconnects==3);
 Reset();objects[0].socket=nullptr;S::ApplyDragRelease(release,0);pose.holdGen=1;S::ApplyDragPose(pose,0);CHECK(writes==0);
 pose.holdGen=2;objects[0].drag=true;S::ApplyDragPose(pose,0);CHECK(writes==0);
 CHECK(S::UpdateLocalDragPose(session,false));objects[0].drag=false;session.sendOk=false;
 CHECK(!S::UpdateLocalDragPose(session,false));CHECK(S::g_releases.size()==1&&!session.poseActive);
 S::Tick();CHECK(S::g_releases.size()==1);session.sendOk=true;S::Tick();CHECK(S::g_releases.empty());
 CHECK(!session.sent.empty()&&session.sent.back().kind==coop::net::ReliableKind::PropRelease);
 puts("PASS cord release/reordered UDP, no lost release under backpressure, timeout/resume, owner leave, local hold and existing link/performance checks");

#endif
}
