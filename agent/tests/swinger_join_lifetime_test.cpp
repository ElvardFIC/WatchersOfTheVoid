#include "../../src/votv-coop/src/coop/interactables/swinger_join_pose.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace S=coop::swinger_join_pose;
namespace test {
struct Obj {int id=0,serial=1;bool live=true,owned=false,open=true;void* cls=nullptr;};
Obj cls{1},otherCls{2},lid{3},other{4};
Obj* objects[]={nullptr,&cls,&otherCls,&lid,&other};
void* bound=&lid;
uint32_t world=1;
uint64_t pass=1;
int sends=0,locations=0,rotations=0,velocities=0,reads=0;
bool locOk=true,rotOk=true,velOk=true,settled=false;
std::function<void()> onOpen,onLoc,onRot,onVel,onRead;
coop::net::Session session;
void Fire(std::function<void()>& fn){auto f=std::move(fn);fn={};if(f)f();}
coop::net::SwingerRestPosePayload Packet(float x=1){
 coop::net::SwingerRestPosePayload p{};coop::net::WireKeyFromString(L"mv_lid",p.key);p.locX=x;return p;
}
void Reset(){
 S::OnDisconnect();++world;++pass;bound=&lid;
 for(auto* o:objects)if(o){++o->serial;o->live=true;o->owned=false;o->open=true;o->cls=&cls;}
 locations=rotations=velocities=reads=0;locOk=rotOk=velOk=true;settled=false;
 onOpen={};onLoc={};onRot={};onVel={};onRead={};session={};sends=0;session.beforeSend=[]{++sends;};
}
}
namespace ue_wrap::reflection {
void* ClassOf(void* p){return p?static_cast<test::Obj*>(p)->cls:nullptr;}
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
void* ObjectAt(int32_t i){return i>0&&i<5?test::objects[i]:nullptr;}
bool IsLiveByIndex(void* p,int32_t i){return p&&p==ObjectAt(i)&&IsLive(p);}
int32_t SlotSerial(int32_t i){auto* p=static_cast<test::Obj*>(ObjectAt(i));return p?p->serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace coop::net::session_serial {uint32_t Current(){return 1;}}
namespace ue_wrap::game_thread {bool IsDefinitelyOffGameThread(){return false;}}
namespace ue_wrap::hot_path {uint64_t CallerRva(const void*){return 0;}unsigned long CurrentThreadId(){return 1;}}
namespace ue_wrap::log {void Write(Level,const char*,...){}}
namespace coop::interactable_sync {
void* ContainerActor(const std::wstring&){return test::bound;}
void ContainerLids(std::vector<std::pair<std::wstring,void*>>& out){out={{L"mv_lid",test::bound}};}
uint64_t ContainerIndexPasses(){return test::pass;}
bool ContainerWaitSettled(uint64_t){return test::settled;}
std::chrono::steady_clock::duration ContainerWaitBackstop(){return std::chrono::seconds(10);}
}
namespace coop::remote_prop {bool IsOwnedHere(void* p){return static_cast<test::Obj*>(p)->owned;}}
namespace ue_wrap::swinger {bool TryReadOpen(void* p,bool& out){out=static_cast<test::Obj*>(p)->open;test::Fire(test::onOpen);return true;}}
namespace ue_wrap::engine {
bool SetActorLocation(void*,const FVector&){++test::locations;test::Fire(test::onLoc);return test::locOk;}
bool SetActorRotation(void*,const FRotator&){++test::rotations;test::Fire(test::onRot);return test::rotOk;}
bool SetActorRootPhysicsVelocity(void*,const FVector&,const FVector&){++test::velocities;test::Fire(test::onVel);return test::velOk;}
bool TryGetActorLocation(void*,FVector& out){++test::reads;out={1,2,3};test::Fire(test::onRead);return true;}
bool TryGetActorRotation(void*,FRotator& out){++test::reads;out={};return true;}
}
int main(){
 using namespace test;
 using State=S::dev::State;
 Reset();S::OnReliable(Packet());CHECK(locations==1&&rotations==1&&velocities==1&&S::dev::Outcome(L"mv_lid")==State::Applied);
 // A callback can grow the pending map while Tick holds its snapshot.
 Reset();lid.open=false;S::OnReliable(Packet());lid.open=true;
 onLoc=[]{lid.open=false;for(int i=0;i<80;++i){auto p=Packet();coop::net::WireKeyFromString(L"mv_"+std::to_wstring(i),p.key);S::OnReliable(p);}lid.open=true;};
 ++pass;S::Tick();CHECK(locations==1&&S::g_pending.size()==80);puts("PASS SI01");
 Reset();onLoc=[]{S::Forget(L"mv_lid");};S::OnReliable(Packet());CHECK(locations==1&&!rotations&&!velocities&&S::dev::Outcome(L"mv_lid")==State::Dropped);
 Reset();onLoc=[]{lid.open=false;S::OnReliable(Packet(99));};S::OnReliable(Packet());CHECK(S::g_pending.at(L"mv_lid")->rest.loc.X==99&&!rotations);puts("PASS SI02");
 for(int phase=0;phase<3;++phase)for(int change=0;change<6;++change){
  Reset();auto cb=[change]{switch(change){case 0:++world;break;case 1:++lid.serial;break;case 2:lid.cls=&otherCls;break;case 3:bound=&other;break;case 4:S::OnDisconnect();break;case 5:++cls.serial;break;}};
  if(phase==0)onOpen=cb;else if(phase==1)onLoc=cb;else onRot=cb;
  S::OnReliable(Packet());CHECK(velocities==0&&rotations<=(phase==2?1:0));
 }
 puts("PASS SI03");
 for(int phase=0;phase<3;++phase){
  Reset();auto cb=[]{lid.owned=true;};if(phase==0)onOpen=cb;else if(phase==1)onLoc=cb;else onRot=cb;
  S::OnReliable(Packet());CHECK(!velocities&&rotations<=(phase==2?1:0));
 }
 puts("PASS SI04");
 Reset();locOk=false;S::OnReliable(Packet());CHECK(!rotations&&!velocities&&S::dev::Outcome(L"mv_lid")==State::Pending);
 Reset();rotOk=false;S::OnReliable(Packet());CHECK(!velocities&&S::dev::Outcome(L"mv_lid")==State::Pending);
 Reset();velOk=false;S::OnReliable(Packet());CHECK(S::dev::Outcome(L"mv_lid")==State::Pending);velOk=true;++pass;S::Tick();CHECK(S::dev::Outcome(L"mv_lid")==State::Applied);puts("PASS SI05");
 Reset();lid.open=false;S::OnReliable(Packet());S::Tick();S::g_pending.at(L"mv_lid")->deadline=std::chrono::steady_clock::now()-std::chrono::seconds(1);S::Tick();CHECK(S::dev::Outcome(L"mv_lid")==State::Dropped);puts("PASS SI06");
 Reset();S::QueueConnectBroadcastForSlot(&session,31);CHECK(sends==1);
 Reset();S::QueueConnectBroadcastForSlot(&session,0);S::QueueConnectBroadcastForSlot(&session,32);CHECK(sends==0);
 for(int change=0;change<5;++change){
  Reset();onRead=[change]{switch(change){case 0:++lid.serial;break;case 1:++world;break;case 2:++session.generations[31];break;case 3:++session.stops;break;case 4:S::OnDisconnect();break;}};
  S::QueueConnectBroadcastForSlot(&session,31);CHECK(sends==0&&reads==1);
 }
 puts("PASS SI07");
 Reset();lid.open=false;S::OnReliable(Packet());lid.open=true;onLoc=[]{++pass;S::Tick();};++pass;S::Tick();CHECK(locations==1&&velocities==1);
 Reset();bound=nullptr;S::OnReliable(Packet());bound=&lid;++pass;S::Tick();CHECK(locations==1);
 puts("PASS actual swinger join: callback ownership, late binding, retry, deadline and 32-peer capture");
}
