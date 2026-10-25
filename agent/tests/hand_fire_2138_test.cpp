#include "../../src/votv-coop/src/coop/player/hand_item.cpp"
#include "../../src/votv-coop/src/coop/player/hand_history.cpp"
#include "../../src/votv-coop/src/coop/player/hand_fire_sync.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace H=coop::hand_item; namespace S=coop::hand_fire_sync; namespace F=ue_wrap::prop_fire;
#ifndef CANNED_HAND_TEST
namespace ue_wrap::canned_food {
bool Read(void*, State&) { return false; }
bool Apply(void*, const State&) { std::abort(); }
}
#endif
namespace test {
struct Obj {int id,serial=1;bool live=true;F::State fire{};};
Obj objs[7]{{0},{1},{2},{3},{4},{5},{6}};
uint32_t world=1;uint8_t self=0;void* held=&objs[1];
bool hooks=true,applyOk=true;int applies=0,destroys=0,releases=0,fireReleases=0;std::function<void()> callback;
bool propClass=true;
std::array<coop::roster_ledger::Row,coop::players::kMaxPeers> handPeople;
std::function<void()> spawnCallback,destroyCallback;
coop::RemotePlayer puppet;
void Callback(){auto c=std::move(callback);callback={};if(c)c();}
}
namespace coop::roster_ledger {const Row& Get(int slot){return test::handPeople.at(slot);}}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<7?test::objs[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<7&&p==&test::objs[i]&&test::objs[i].live;}
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
std::wstring ClassNameOf(void*){return L"prop_C";}
void* FindClass(const wchar_t*){return &test::objs[0];}
int32_t FindPropertyOffset(void*,const wchar_t*){return -1;}
std::wstring ToString(const FName&){return {};}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return (void*)(uintptr_t)test::world;}void* WorldOf(void*){return CurrentWorld();}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::game_thread {bool IsDefinitelyOffGameThread(){return false;}}
namespace ue_wrap::hot_path {uintptr_t CallerRva(const void*){return 0;}unsigned long CurrentThreadId(){return 0;}}
namespace ue_wrap::script_gate {bool WatchName(const wchar_t*,int,PreFn,PostFn){return true;}void ResolvePendingNames(){}bool NameWatchLive(const wchar_t*,int){return test::hooks;}}
namespace ue_wrap::prop {bool IsDescendantOfProp(void*){return true;}bool IsClassDescendantOfProp(void*){return test::propClass;}}
namespace ue_wrap::inventory {std::wstring ReadHeldItemName(){return L"box";}}
namespace ue_wrap::puppet {void* GetSkeletalMeshComponent(void*){return nullptr;}}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t*){return &test::objs[0];}}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring&){return {};}}
namespace ue_wrap::engine {
bool GetBoneWorldLocationByName(void*,const wchar_t*,FVector&){return false;}
void* GetController(void*){return nullptr;}
FRotator GetControlRotation(void*){return {};}
bool TryGetActorRotation(void*,FRotator& r){r={};return true;}
bool TryGetActorLocation(void*,FVector& p){p={};return true;}
bool ReadMainPlayerGrabState(void*,MainPlayerGrabState& s){s.holdingActor=test::held;return true;}
bool DestroyActor(void* p){++test::destroys;static_cast<test::Obj*>(p)->live=false;auto c=std::move(test::destroyCallback);test::destroyCallback={};if(c)c();return true;}
void* BeginDeferredSpawn(void*,const FVector&,const FRotator&){auto& o=test::objs[3];++o.serial;o.live=true;return &o;}
bool FinishDeferredSpawn(void*,const FVector&,const FRotator&){auto c=std::move(test::spawnCallback);test::spawnCallback={};if(c)c();return true;}
bool SetActorSimulatePhysics(void*,bool){return true;}
bool SetActorEnableCollision(void*,bool){return true;}
bool SetActorLocation(void*,const FVector&){return true;}
bool SetActorRotation(void*,const FRotator&){return true;}
}
namespace coop::prop_echo_suppress {void MarkIncomingDestroy(void*){}void MarkMirrorSpawn(void*){}ScopedMirrorSpawn::ScopedMirrorSpawn(){}ScopedMirrorSpawn::~ScopedMirrorSpawn(){}}
namespace coop::trash_collect_sync {bool EnsureHeldItemBroadcast(void*,net::Session*){++test::releases;return true;}}
namespace coop::prop_fire_sync {void OnHandReleased(void*){++test::fireReleases;}}
namespace coop::players {
Registry& Registry::Get(){static Registry r;return r;}
uint8_t Registry::LocalPeerId()const{return test::self;}
coop::RemotePlayer* Registry::Puppet(uint8_t){return &test::puppet;}
}
namespace ue_wrap::prop_fire {
bool Extinguish(void* p,bool* attempted,const std::function<bool()>& current){
    if(attempted)*attempted=false;if(current && !current())return false;
    if(attempted)*attempted=true;static_cast<test::Obj*>(p)->fire={};return true;}
bool Valid(const State& s){return std::isfinite(s.fuel)&&s.fuel>=0&&s.fuel<=60&&std::isfinite(s.fuelSpeed)&&s.fuelSpeed>=0&&s.fuelSpeed<=100&&(!s.burning||s.fuel>0);}
bool Read(void* p,State& s){s=static_cast<test::Obj*>(p)->fire;return true;}
bool Apply(void* p,const State& s,const std::function<bool()>& current){++test::applies;test::Callback();if(!current()||!test::applyOk)return false;static_cast<test::Obj*>(p)->fire=s;return true;}
}
using namespace test;
coop::net::Session session;
void Reset(coop::net::Role role=coop::net::Role::Host){
 S::OnDisconnect();H::Reset();++world;session={};session.mode=role;self=role==coop::net::Role::Host?0:1;
 for(int i=0;i<7;++i)objs[i]={i};held=&objs[1];puppet.actor=&objs[2];puppet.posed=true;hooks=applyOk=propClass=true;applies=destroys=releases=fireReleases=0;callback={};spawnCallback={};destroyCallback={};S::Install(&session);
}
void Owner(){H::TickOwner(session,&objs[0],held);}
void Tick(){S::g_poll=0;S::Tick();}
auto Packet(uint8_t slot=1,uint32_t gen=1,uint32_t seq=1){return coop::net::HandFirePayload{gen,seq,30,1,slot,1,0};}
void Hand(uint8_t slot,uint32_t gen=1,bool has=true){
 H::SlotHand h;h.has=has;h.cls=L"prop_C";h.name=L"box";h.item=L"box";h.generation=gen;
 auto bytes=H::BuildPayload(slot,h);coop::net::Session::ReliableMessage m{bytes.data(),int(bytes.size()),session.mode==coop::net::Role::Host?int(slot):0};
 H::HandleHandItem(session,m);H::TickMirrors();
}
int main(){using coop::net::Role;using ue_wrap::script_gate::Verdict;
 Reset();Owner();CHECK(H::Generation(0)==1);objs[1].fire={true,30,1};Tick();CHECK(session.sent.back().kind==coop::net::ReliableKind::HandFire);
 // Same class/name/item but a new actor is a new hand, even at the same recycled address.
 ++objs[1].serial;Owner();CHECK(H::Generation(0)==2);held=nullptr;Owner();CHECK(H::Generation(0)==0&&fireReleases==1);
 held=&objs[1];Owner();CHECK(H::Generation(0)==4);
 Reset();Owner();held=&objs[4];CHECK(H::Generation(0)==0); // fresh holding read invalidates old identity before TickOwner
 Reset();session.sendOk=false;Owner();CHECK(H::g_dirty[0]);session.sendOk=true;Owner();CHECK(!H::g_dirty[0]&&!session.sent.empty());
 Reset();H::SlotHand max;max.has=true;max.cls=max.name=max.item=std::wstring(63,L'x');max.generation=1;CHECK(H::BuildPayload(1,max).size()==224);
 // Fire can precede its hand actor. Duplicates do not reapply or reset the native burn timer.
 Reset();S::Receive(Packet(),1);Tick();CHECK(applies==0);Hand(1);Tick();CHECK(applies==1&&objs[3].fire.burning);
 S::Receive(Packet(),1);Tick();CHECK(applies==1);CHECK(H::MirrorGeneration(1)==1);
 // Switch/stow destroys the old mirror. A late old fire packet cannot ignite its successor.
 Hand(1,2);Tick();CHECK(applies==1&&H::MirrorGeneration(1)==2);S::Receive(Packet(1,1,2),1);Tick();CHECK(applies==1);
 S::Receive(Packet(1,2,3),1);Tick();CHECK(applies==2);Hand(1,3,false);Tick();CHECK(!H::MirrorActorForSlot(1)&&!objs[3].fire.burning);
 Hand(1,2);CHECK(!H::MirrorActorForSlot(1)); // older HandItem rejected after stow
 Reset();Hand(1);S::Receive(Packet(),2);Tick();CHECK(!applies);S::Receive(Packet(0),1);Tick();CHECK(!applies);
 Reset(Role::Client);Hand(0);S::Receive(Packet(0),2);Tick();CHECK(!applies);S::Receive(Packet(0),0);Tick();CHECK(applies==1);
 Reset();Hand(1);auto bad=Packet();bad.fuel=INFINITY;S::Receive(bad,1);Tick();CHECK(!applies);
 for(int i=0;i<7;++i){Reset();Hand(1);bad=Packet();if(i==0)bad.sequence=0;if(i==1)bad.generation=0;if(i==2)bad.reserved=1;if(i==3)bad.burning=2;if(i==4)bad.fuelSpeed=-1;if(i==5)bad.fuel=61;if(i==6)bad.fuelSpeed=NAN;S::Receive(bad,1);Tick();CHECK(!applies);}
 Reset();Hand(1);S::Receive(Packet(),1);Tick();++objs[3].serial;CHECK(!H::MirrorActorForSlot(1));H::TickMirrors();Tick();CHECK(applies==2); // fresh mirror gets retained fire
 Reset();Owner();objs[1].fire={true,30,1};session.sendOk=false;Tick();CHECK(S::g_rows[0].pending);objs[1].fire={};session.sendOk=true;Tick();CHECK(!S::g_rows[0].state.burning&&!S::g_rows[0].pending); // newest OFF wins while blocked
 Reset();S::Receive(Packet(),1);S::g_rows[1].received=S::Now()-40000;Hand(1);Tick();CHECK(applies==1&&!objs[3].fire.burning);
 Reset();Hand(1);S::Receive(Packet(),1);S::g_rows[1].received=S::Now()+1000;Tick();CHECK(applies==1&&objs[3].fire.fuel==30);
 Reset();propClass=false;Hand(1);CHECK(!H::MirrorActorForSlot(1));
 Reset();Hand(1);S::Receive(Packet(),1);applyOk=false;Tick();CHECK(!S::g_rows[1].applied);auto attempts=applies;Tick();CHECK(applies==attempts);applyOk=true;S::g_rows[1].retryAt=0;Tick();CHECK(S::g_rows[1].applied);
 Reset();Hand(1);S::Receive(Packet(),1);Tick();S::QueueConnect(2);session.sendOk=false;Tick();CHECK(S::g_rows[1].pending&4);session.sendOk=true;Tick();CHECK(!(S::g_rows[1].pending&4));
 for(int i=0;i<4;++i){Reset();Hand(1);S::Receive(Packet(),1);callback=[i]{if(i==0)S::OnDisconnect();if(i==1)++world;if(i==2)++objs[3].serial;if(i==3)Hand(1,2);};Tick();CHECK(!S::g_rows[1].applied);}
 Reset();Hand(1);S::Receive(Packet(),1);callback=[]{S::Receive(Packet(1,1,2),1);};Tick();CHECK(!S::g_rows[1].applied);Tick();CHECK(S::g_rows[1].applied);
 Reset();Hand(1);S::Receive(Packet(),1);S::OnPeerLeft(1);Tick();CHECK(!applies);
 Reset();puppet.posed=false;Hand(1);S::Receive(Packet(),1);Tick();CHECK(!applies&&!H::MirrorActorForSlot(1));puppet.posed=true;H::TickMirrors();Tick();CHECK(applies==1);
 Reset();spawnCallback=[]{H::Reset();};Hand(1);CHECK(!H::MirrorActorForSlot(1)&&destroys==1);
 Reset();Hand(1);destroyCallback=[]{H::Reset();};H::OnSlotDisconnected(1);CHECK(!H::MirrorActorForSlot(1)&&destroys==1);
 Reset();Hand(1);ue_wrap::script_gate::Call c{};c.object=&objs[3];c.tag=0x48460003;CHECK(S::GuardMirror(c)==Verdict::Cancel);c.object=&objs[0];CHECK(S::GuardMirror(c)==Verdict::Run);
 c.object=&objs[3];c.tag=0x48460001;CHECK(S::GuardMirror(c)==Verdict::Cancel);S::g_applying.Set(c.object);CHECK(S::GuardMirror(c)==Verdict::Run);c.tag=0x48460004;CHECK(S::GuardMirror(c)==Verdict::Cancel);S::g_applying.Reset();
 Reset();Owner();objs[1].fire={true,30,1};hooks=false;Tick();for(auto& sent:session.sent)CHECK(sent.kind!=coop::net::ReliableKind::HandFire);
 puts("PASS actual hand axis/fire: generation, stow/switch, queue retry, authority, delayed actors, duplicates, expiry, late join, native guards and reentrant lifecycle");
 return 0;
}
