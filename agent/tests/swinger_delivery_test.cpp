#ifndef SWINGER_DELIVERY_SOURCE
#define SWINGER_DELIVERY_SOURCE "../../src/votv-coop/src/coop/interactables/swinger_join_pose.cpp"
#endif
#include SWINGER_DELIVERY_SOURCE
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace S=coop::swinger_join_pose;
namespace N=coop::net;
namespace test {
struct Obj {int id=0,serial=1;bool live=true,open=true;void* cls=nullptr;};
Obj cls{1},otherCls{2},lid{3},other{4};
Obj* objects[]={nullptr,&cls,&otherCls,&lid,&other};
uint32_t world=1,serial=1; uint64_t pass=1;
N::Session session,second;
std::unordered_map<std::wstring,void*> bindings;
std::vector<std::pair<std::wstring,void*>> lids;
enum Boundary { Bind,Open,Location,Rotation,Send };
std::array<std::function<void()>,4> hooks;
std::function<void()> collect;
bool openOK=true,locationOK=true,rotationOK=true;
ue_wrap::FVector location{1,2,3}; ue_wrap::FRotator rotation{4,5,6};
int opens=0,locations=0,rotations=0;
void Fire(std::function<void()>& slot){auto fn=std::move(slot);slot={};if(fn)fn();}
void Hook(int boundary,std::function<void()> fn){
 if(boundary==Send)session.onSend=[fn](const void*,size_t){fn();};else hooks.at(boundary)=fn;
}
void Due(){for(const auto& j:S::g_hostJoins)if(j)for(const auto& l:j->lids)l->due={};}
void Pump(){Due();S::Tick();}
void Reset(){
 S::OnDisconnect();++world;++serial;++pass;
 for(auto* o:objects)if(o){++o->serial;o->live=true;o->open=true;o->cls=&cls;}
 session={};second={};bindings={{L"mv_lid",&lid},{L"mv_other",&other}};lids={{L"mv_lid",&lid}};
 hooks={};collect={};openOK=locationOK=rotationOK=true;location={1,2,3};rotation={4,5,6};
 opens=locations=rotations=0;
}
void Queue(int slot=1){S::QueueConnectBroadcastForSlot(&session,slot);}
bool Pending(int slot=1){return bool(S::g_hostJoins.at(slot));}
void Passed(const char* family,int n){std::printf("PASS %s%02d\n",family,n);}
void Change(int n){switch(n){case 0:++world;break;case 1:++serial;break;case 2:++lid.serial;break;
 case 3:++cls.serial;break;case 4:++session.generations[1];break;}}
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
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace ue_wrap::game_thread {bool IsDefinitelyOffGameThread(){return false;}}
namespace ue_wrap::hot_path {uint64_t CallerRva(const void*){return 0;}unsigned long CurrentThreadId(){return 1;}}
namespace ue_wrap::log {void Write(Level,const char*,...){}}
namespace coop::interactable_sync {
void* ContainerActor(const std::wstring& key){auto it=test::bindings.find(key);void* p=it==test::bindings.end()?nullptr:it->second;test::Fire(test::hooks[test::Bind]);return p;}
void ContainerLids(std::vector<std::pair<std::wstring,void*>>& out){out=test::lids;test::Fire(test::collect);}
uint64_t ContainerIndexPasses(){return test::pass;}
bool ContainerWaitSettled(uint64_t){return false;}
std::chrono::steady_clock::duration ContainerWaitBackstop(){return std::chrono::seconds(10);}
}
namespace coop::remote_prop {bool IsOwnedHere(void*){return false;}}
namespace ue_wrap::swinger {bool TryReadOpen(void* p,bool& out){++test::opens;out=static_cast<test::Obj*>(p)->open;test::Fire(test::hooks[test::Open]);return test::openOK;}}
namespace ue_wrap::engine {
bool SetActorLocation(void*,const FVector&){return true;}
bool SetActorRotation(void*,const FRotator&){return true;}
bool SetActorRootPhysicsVelocity(void*,const FVector&,const FVector&){return true;}
bool TryGetActorLocation(void*,FVector& out){++test::locations;out=test::location;test::Fire(test::hooks[test::Location]);return test::locationOK;}
bool TryGetActorRotation(void*,FRotator& out){++test::rotations;out=test::rotation;test::Fire(test::hooks[test::Rotation]);return test::rotationOK;}
}
int main(){using namespace test;
 Reset();Queue();CHECK(Pending()&&session.sent.size()==1&&!session.sent[0].accepted);session.sendOK=true;Pump();CHECK(!Pending()&&session.sent.size()==2);Passed("SJ",1);
 Reset();lid.open=false;N::SwingerRestPosePayload incoming{};N::WireKeyFromString(L"mv_lid",incoming.key);S::OnReliable(incoming);auto received=S::g_pending.at(L"mv_lid");lid.open=true;Queue();S::QueueConnectBroadcastForSlot(&second,2);CHECK(!Pending(1)&&Pending(2)&&S::g_hostSession==&second&&S::g_pending.at(L"mv_lid")==received);
 for(int variant=0;variant<3;++variant){Reset();Queue();if(variant==0)second.mode=N::Role::Client;else second.linked=false;S::QueueConnectBroadcastForSlot(variant==2?nullptr:&second,2);Pump();CHECK(!Pending(1)&&session.sent.size()==1);}Passed("SJ",2);
 Reset();Queue();++serial;Pump();CHECK(!Pending()&&session.sent.size()==1);Passed("SJ",3);
 Reset();Queue();++session.stops;Pump();CHECK(!Pending()&&session.sent.size()==1);Passed("SJ",4);
 Reset();Queue();++world;Pump();CHECK(!Pending()&&session.sent.size()==1);Passed("SJ",5);
 Reset();Queue();session.mode=N::Role::Client;Pump();CHECK(!Pending()&&session.sent.size()==1);Passed("SJ",6);
 for(int variant=0;variant<3;++variant){Reset();Queue();if(variant==0)++session.generations[1];else if(variant==1)session.generations[1]=0;else session.online[1]=false;Pump();CHECK(!Pending()&&session.sent.size()==1);}Passed("SJ",7);
 Reset();session.ready[1]=false;Queue();CHECK(Pending()&&opens==0&&session.sent.empty());S::Tick();CHECK(Pending()&&opens==0);session.ready[1]=true;session.sendOK=true;S::Tick();CHECK(!Pending()&&session.sent.size()==1);Passed("SJ",8);
 Reset();session.sendOK=true;lids={{L"not_portable",&lid},{L"mv_"+std::wstring(29,L'a'),&lid},{L"mv_\u0100",&lid},{L"mv_\n",&lid},{L"mv_lid",&lid},{L"mv_lid",&lid},{L"mv_null",nullptr}};Queue();for(int i=0;i<8;++i)Pump();CHECK(session.sent.size()==1&&!Pending());Passed("SJ",9);
 Reset();session.sendOK=true;lids.push_back({L"mv_other",&other});Hook(Location,[]{lids.clear();});Queue();Pump();CHECK(session.sent.size()==2&&!Pending()&&N::StringFromWireKey(session.sent[1].payload.key)==L"mv_other");Passed("SJ",10);
 Reset();session.sendOK=true;collect=[]{lids={{L"mv_other",&other}};Queue();};Queue();CHECK(session.sent.size()==1&&N::StringFromWireKey(session.sent[0].payload.key)==L"mv_other"&&!Pending());Passed("SJ",11);
 Reset();session.sendOK=true;Hook(Send,[]{S::Tick();Queue();S::Tick();});Queue();CHECK(session.maxDepth==1&&session.sent.size()==1&&Pending());Pump();CHECK(session.sent.size()==2&&!Pending());Passed("SJ",12);
 Reset();Queue();auto count=session.sent.size();for(int i=0;i<5;++i)S::Tick();CHECK(session.sent.size()==count&&Pending());Pump();CHECK(session.sent.size()==count+1);
 for(int variant=0;variant<2;++variant){Reset();if(variant==0)locationOK=false;else rotationOK=false;Queue();CHECK(Pending()&&session.sent.empty());const int reads=locations+rotations;S::Tick();CHECK(locations+rotations==reads);locationOK=rotationOK=true;session.sendOK=true;Pump();CHECK(!Pending()&&session.sent.size()==1);}Passed("SJ",13);
 Reset();Queue();auto debt=S::g_hostJoins[1];debt->lids[0]->due=std::chrono::steady_clock::now()+std::chrono::hours(1);S::Tick();CHECK(S::g_hostJoins[1]==debt&&session.sent.size()==1);Passed("SJ",14);
 Reset();Queue();for(int i=0;i<12;++i)Pump();CHECK(Pending()&&session.sent.size()==13);session.sendOK=true;Pump();CHECK(!Pending()&&session.sent.size()==14);Passed("SJ",15);
 Reset();Queue(1);Queue(31);CHECK(Pending(1)&&Pending(31));session.ready[1]=false;session.sendOK=true;Pump();CHECK(Pending(1)&&!Pending(31)&&session.sent.back().slot==31);session.ready[1]=true;Pump();CHECK(!Pending(1));Queue(0);Queue(32);CHECK(session.sent.size()==4);Passed("SJ",16);
 Reset();session.sendOK=true;Hook(Send,[](void){location={99,98,97};S::OnDisconnect();Queue();});Queue();CHECK(session.sent.size()==1&&session.sent[0].payload.locX==1&&Pending());Pump();CHECK(session.sent[1].payload.locX==99&&!Pending());Passed("SJ",17);
 Reset();session.sendOK=true;Hook(Send,[]{location={88,0,0};Queue();});Queue();CHECK(Pending()&&session.sent.size()==1);Pump();CHECK(!Pending()&&session.sent.size()==2&&session.sent.back().payload.locX==88);Passed("SJ",18);
 Reset();Queue();location={21,22,23};rotation={24,25,26};session.sendOK=true;Pump();CHECK(locations==2&&rotations==2&&session.sent.back().payload.locX==21&&session.sent.back().payload.rotYaw==25);Passed("SJ",19);
 Reset();lid.open=false;Queue();CHECK(!Pending()&&session.sent.empty()&&locations==0);Reset();openOK=false;Queue();CHECK(Pending()&&session.sent.empty());openOK=true;session.sendOK=true;Pump();CHECK(!Pending()&&session.sent.size()==1);Passed("SJ",20);
 for(int variant=0;variant<4;++variant){Reset();Queue();if(variant==0)++lid.serial;else if(variant==1)++cls.serial;else if(variant==2)lid.cls=&otherCls;else bindings[L"mv_lid"]=&other;Pump();CHECK(!Pending()&&session.sent.size()==1);}Passed("SJ",21);
 for(int variant=0;variant<7;++variant){Reset();if(variant<3){float* xyz=&location.X;xyz[variant]=N::kMaxCoord+1;}else if(variant==3)location.X=std::numeric_limits<float>::infinity();else {if(variant==4)rotation.Pitch=std::numeric_limits<float>::quiet_NaN();if(variant==5)rotation.Yaw=std::numeric_limits<float>::infinity();if(variant==6)rotation.Roll=std::numeric_limits<float>::quiet_NaN();}Queue();CHECK(Pending()&&session.sent.empty());location={1,2,3};rotation={4,5,6};session.sendOK=true;Pump();CHECK(!Pending()&&session.sent.size()==1);}Passed("SJ",22);
 Reset();lids.push_back({L"mv_other",&other});session.sendOK=true;Queue();session.sendOK=false;Pump();CHECK(Pending()&&session.sent.size()==2);Pump();CHECK(session.sent.size()==2);session.sendOK=true;Pump();CHECK(!Pending()&&session.sent.size()==3&&N::StringFromWireKey(session.sent.back().payload.key)==L"mv_other");Passed("SJ",23);
 Reset();S::g_hostDraining=true;for(int slot=1;slot<32;++slot)Queue(slot);S::g_hostDraining=false;session.sendOK=true;S::Tick();CHECK(session.sent.size()==S::kHostBudget);S::Tick();CHECK(session.sent.size()==31);Passed("SJ",24);
 Reset();lids.push_back({L"mv_other",&other});Queue(1);Queue(31);S::Forget(L"mv_lid");session.sendOK=true;for(int i=0;i<5;++i)Pump();CHECK(!Pending(1)&&!Pending(31));for(const auto& p:session.sent)if(p.accepted)CHECK(N::StringFromWireKey(p.payload.key)==L"mv_other");
 Reset();collect=[]{S::Forget(L"mv_lid");};Queue();CHECK(!Pending()&&session.sent.empty());Passed("SJ",25);
 for(int retry=0;retry<2;++retry)for(int boundary=0;boundary<5;++boundary)for(int change=0;change<5;++change){
  Reset();if(retry){Queue();CHECK(Pending());}session.sendOK=true;
  const auto sent=session.sent.size();const int beforeOpen=opens,beforeLocation=locations,beforeRotation=rotations;
  Hook(boundary,[change]{Change(change);});if(retry)Pump();else Queue();
  CHECK(session.sent.size()==sent+(boundary==Send?1:0));
  CHECK(opens==beforeOpen+(boundary>=Open?1:0));
  CHECK(locations==beforeLocation+(boundary>=Location?1:0));
  CHECK(rotations==beforeRotation+(boundary>=Rotation?1:0));
  Pump();CHECK(!Pending()&&session.sent.size()==sent+(boundary==Send?1:0));
  Passed("SD",retry*25+boundary*5+change+1);
 }
 puts("PASS actual swinger host join delivery: 75 cases");
}
