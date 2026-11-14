#include "../../src/votv-coop/src/ue_wrap/actors/rufus.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include "../../src/votv-coop/src/coop/creatures/rufus_birth_guard.h"
#include "../../src/votv-coop/src/coop/creatures/npc_spawn_lifetime.h"
#include "coop/creatures/npc_class_policy.h"
#include "coop/creatures/geom_octa_sync.h"
#include "coop/config/config.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
enum Id{Actor,Mesh,Arm,Anim,Thicc,Mesh2,Arm2,Anim2,Attach,Console,Other, RufusType=20,ThiccType,ForeignType,MeshType,ArmType,AnimType,ConsoleType,PlushType,Count=64};
struct Obj {int id=0,serial=1;bool live=true;void* cls=nullptr;void* super=nullptr;void* world=nullptr;
    ue_wrap::reflection::FName name{};void* mesh=nullptr;void* arm=nullptr;void* anim=nullptr;void* pawn=nullptr;void* parent=nullptr;
    ue_wrap::FRotator rotation{10,40,20};ue_wrap::FVector location{1,2,3},scale{1,1,1};} objects[Count];
Obj& O(void* p){CHECK(p);return *static_cast<Obj*>(p);}void* At(Id i){return &objects[i];}
uint32_t generation=1,serial=1;uintptr_t world=100;coop::net::Session session;coop::net::Session* currentSession=&session;
bool installed=true,disabled=false,watch=true,registration=true,markOk=true,attachOk=true,nativeOk=true,getOk=true,setOk=true,createOk=true;
std::set<std::pair<int,int>> marks;std::map<int,std::wstring> names;
std::wstring fieldAt,missingParam;int badSize=-1;std::function<void()> onField,onFrame,onCall,onGet,onSet,onResolve,onPose;
int attaches=0,gets=0,sets=0,creates=0,parks=0,relays=0;void* receivedParent=nullptr;
void Fire(std::function<void()>& callback){auto f=std::move(callback);callback={};if(f)f();}
void Reset();
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&test::O(p).live;}void* ClassOf(void* p){return p?test::O(p).cls:nullptr;}
int32_t InternalIndexOf(void* p){return p?test::O(p).id:-1;}int32_t SlotSerial(int32_t i){return i>=0&&i<test::Count?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}void* ObjectAt(int32_t i){return i>=0&&i<test::Count?&test::objects[i]:nullptr;}
bool IsLiveByIndex(void* p,int32_t i){return p&&p==ObjectAt(i)&&IsLive(p);}void* SuperStructOf(void* p){return test::O(p).super;}
const FName& NameOf(void* p){return test::O(p).name;}bool NameEquals(const FName& n,const wchar_t* s){return test::names[n.ComparisonIndex]==s;}
std::wstring ToString(const FName& n){return test::names[n.ComparisonIndex];}
int32_t FindPropertyOffset(void*,const wchar_t* name){int off=-1;
    if(!wcscmp(name,L"Mesh"))off=offsetof(test::Obj,mesh);if(!wcscmp(name,L"SpringArm"))off=offsetof(test::Obj,arm);
    if(!wcscmp(name,L"AnimScriptInstance"))off=offsetof(test::Obj,anim);if(!wcscmp(name,L"pawn"))off=offsetof(test::Obj,pawn);
    if(!wcscmp(name,L"AttachParent"))off=offsetof(test::Obj,parent);if(test::fieldAt==name)test::Fire(test::onField);return off;}
void* FindDispatchFunctionCached(void*,const wchar_t* name){CHECK(!wcscmp(name,L"K2_AttachToComponent"));return test::At(test::Attach);}
int32_t FunctionFrameSize(void*){test::Fire(test::onFrame);return 24;}
std::vector<ParamInfo> FunctionParams(void*){std::vector<ParamInfo> p={{L"Parent",0,8,0},{L"SocketName",8,8,0},{L"LocationRule",16,1,0},
    {L"RotationRule",17,1,0},{L"ScaleRule",18,1,0},{L"bWeldSimulatedBodies",19,1,0},{L"ReturnValue",20,1,0}};
    for(auto it=p.begin();it!=p.end();)if(it->name==test::missingParam){if(test::badSize<0)it=p.erase(it);else{it->size=test::badSize;++it;}}else ++it;return p;}
bool CallFunction(void* actor,void* function,void* args){CHECK(function==test::At(test::Attach));++test::attaches;auto* bytes=static_cast<uint8_t*>(args);
    void* parent=nullptr;std::memcpy(&parent,bytes,8);test::receivedParent=parent;
    CHECK(bytes[16]==0&&bytes[17]==0&&bytes[18]==0&&bytes[19]==1);
    uint64_t socket=1;std::memcpy(&socket,bytes+8,8);CHECK(!socket);
    if(test::attachOk)test::O(actor).parent=parent;std::memcpy(bytes+20,&test::attachOk,1);test::Fire(test::onCall);return test::nativeOk;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::generation;}void* CurrentWorld(){return reinterpret_cast<void*>(test::world);}
void* WorldOf(void* p){return p?test::O(p).world:nullptr;}}
namespace ue_wrap::engine {
bool TryGetComponentWorldRotation(void* p,FRotator& out){++test::gets;out=test::O(p).rotation;test::Fire(test::onGet);return test::getOk;}
bool SetComponentWorldRotation(void* p,const FRotator& r){++test::sets;if(test::setOk)test::O(p).rotation=r;test::Fire(test::onSet);return test::setOk;}
bool TryGetActorLocation(void* p,FVector& out){out=test::O(p).location;test::Fire(test::onPose);return test::getOk;}
bool TryGetActorRotation(void* p,FRotator& out){out=test::O(p).rotation;test::Fire(test::onPose);return test::getOk;}
FVector GetActorScale3D(void* p){auto value=test::O(p).scale;test::Fire(test::onPose);return value;}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t* cls,const wchar_t* name,int,PreFn,PostFn){CHECK(!wcscmp(cls,L"rufus_C")&&!wcscmp(name,L"ExecuteUbergraph_rufus"));return test::registration;}
void ResolvePendingNames(){test::Fire(test::onResolve);}bool ClassNameWatchLive(const wchar_t*,const wchar_t*,int){return test::watch;}
}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace coop::npc_mirror_brain {
bool IsMarkedMirror(void* p){return p&&test::marks.contains({test::O(p).id,test::O(p).serial});}
void MarkMirror(void* p){if(test::markOk)test::marks.insert({test::O(p).id,test::O(p).serial});}
void UnmarkMirror(int32_t id,int32_t serial){test::marks.erase({id,serial});}
bool PrepareBirth(void* p){return ue_wrap::rufus::IsFamily(p)?rufus_birth::Prepare(p):true;}
void SetUpPresentation(void* p,Origin){if(ue_wrap::rufus::IsFamily(p))ue_wrap::rufus::PreparePresentation(p);}
}
namespace coop::element {
using ElementId=uint32_t;inline constexpr ElementId kInvalidId=0;
struct Npc {std::string type;void* actor=nullptr;void SetTypeName(std::string s){type=std::move(s);}const std::string& GetTypeName()const{return type;}
    void SetActor(void* p,int){actor=p;}};
struct Manager {std::map<uint32_t,std::unique_ptr<Npc>> rows;uint32_t next=1;
    uint32_t AllocAndInstall(std::unique_ptr<Npc> p,bool){auto id=next++;rows[id]=std::move(p);return id;}
    Npc* Get(uint32_t id){auto it=rows.find(id);return it==rows.end()?nullptr:it->second.get();}
    std::unique_ptr<Npc> Take(uint32_t id){auto out=std::move(rows[id]);rows.erase(id);return out;}};
Manager manager;Manager& NpcMirrors(){return manager;}
struct ElementDeleter{static ElementDeleter& Get(){static ElementDeleter instance;return instance;}void Enqueue(std::unique_ptr<Npc>) {}};
bool CreateOrAdoptNpcMirror(ElementId id,void* p,const std::wstring&,int){++test::creates;if(!test::createOk)return false;
    auto npc=std::make_unique<Npc>();npc->actor=p;manager.rows[id]=std::move(npc);return true;}
}
namespace coop::npc_sync {
std::map<void*,uint32_t> reverse;coop::net::Session* GetSession(){return test::currentSession;}
bool IsInstalled(){return test::installed;}bool IsHostNpcSyncDisabled(){return test::disabled;}void RefreshLateAllowlist(){}
bool IsAllowlistedClass(void* p){return coop::npc_class_policy::MatchesAncestryName(p);}
uint32_t GetNpcIdForActor(void* p){auto it=reverse.find(p);return it==reverse.end()?0:it->second;}
void MapActorToNpcId(void* p,uint32_t id){reverse[p]=id;}
}
namespace coop::kerfur_entity {enum class Form{Npc};void AllocKerfurId(void*,uint32_t,Form,const std::wstring&){}
uint32_t GetOriginOffEidForEid(uint32_t){return 0;}}
namespace coop::world_actor_sync {void HostEnrollExSpawn(void*){CHECK(false);}}
namespace coop::creature_event_relay {void OnMirrorBound(uint32_t,void*){++test::relays;}}
namespace ue_wrap::kerfur {void NeutralizeAiTimers(void*){}}
namespace ue_wrap::puppet {void DisableCharacterTicks(void*){++test::parks;}}
#include "rufus_extracted.h"
namespace test {
namespace N=coop::npc_world_enum;namespace M=ue_wrap::rufus;namespace G=coop::npc_mirror_brain;namespace B=G::rufus_birth;
void Reset(){
    ++generation;++serial;world=100+generation;session={};currentSession=&session;
    for(int i=0;i<Count;++i){objects[i]={};objects[i].id=i;objects[i].serial=int(generation*1000)+i;objects[i].cls=&objects[i];objects[i].name.ComparisonIndex=i;}
    objects[Actor].cls=At(RufusType);objects[Thicc].cls=At(ThiccType);objects[ThiccType].super=At(RufusType);
    objects[Other].cls=At(PlushType);objects[Mesh].cls=objects[Mesh2].cls=At(MeshType);objects[Arm].cls=objects[Arm2].cls=At(ArmType);
    objects[Anim].cls=objects[Anim2].cls=At(AnimType);objects[Console].cls=At(ConsoleType);
    objects[Actor].mesh=At(Mesh);objects[Thicc].mesh=At(Mesh2);objects[Actor].arm=At(Arm);objects[Thicc].arm=At(Arm2);
    objects[Mesh].anim=At(Anim);objects[Mesh2].anim=At(Anim2);
    for(auto id:{Actor,Thicc,Mesh,Mesh2,Arm,Arm2,Anim,Anim2,Other,Console})objects[id].world=reinterpret_cast<void*>(world);
    names={{RufusType,L"rufus_C"},{ThiccType,L"thiccfus_C"},{ForeignType,L"foreign_C"},{MeshType,L"SkeletalMeshComponent"},
        {ArmType,L"SpringArmComponent"},{AnimType,L"rufus_Skeleton_AnimBlueprint_C"},{ConsoleType,L"ui_console_C"},{PlushType,L"prop_thiccfus_C"}};
    installed=watch=registration=markOk=attachOk=nativeOk=getOk=setOk=createOk=true;disabled=false;
    fieldAt=missingParam=L"";badSize=-1;onField=onFrame=onCall=onGet=onSet=onResolve=onPose={};
    attaches=gets=sets=creates=parks=relays=0;receivedParent=nullptr;marks.clear();coop::element::manager={};coop::npc_sync::reverse.clear();N::g_pendingExSpawns.clear();
}
void Catch(void* p=At(Actor)){N::OnBeginDeferredExSpawn(nullptr,At(Console),p);}
void Family(){Reset();CHECK(coop::npc_class_policy::MatchesAncestryName(At(RufusType))&&M::IsFamily(At(Actor)));}
void Thiccfus(){Reset();CHECK(coop::npc_class_policy::MatchesAncestryName(At(ThiccType))&&M::IsFamily(At(Thicc)));
    CHECK(!M::IsFamily(At(Other))&&!coop::npc_class_policy::MatchesAncestryName(At(PlushType)));}
void ConsoleCatch(){Reset();Catch();Catch(At(Thicc));CHECK(N::g_pendingExSpawns.size()==2);Catch(At(Other));CHECK(N::g_pendingExSpawns.size()==2);
    objects[Console].cls=At(ForeignType);Catch(At(Actor));CHECK(N::g_pendingExSpawns.size()==2);Reset();Catch();N::DrainPendingExSpawns();CHECK(session.spawns.size()==1);}
void SerialCatch(){for(int cause=0;cause<3;++cause){Reset();Catch();if(cause==0)++objects[Actor].serial;
    if(cause==1)++objects[RufusType].serial;if(cause==2)objects[Actor].cls=At(ThiccType);N::DrainPendingExSpawns();CHECK(coop::element::manager.rows.empty());}}
void ScopeCatch(){for(int cause=0;cause<6;++cause){Reset();Catch();if(cause==0)++serial;if(cause==1)++generation;
    if(cause==2)++world;if(cause==3)session.active=false;if(cause==4)session.mode=coop::net::Role::Client;if(cause==5)currentSession=nullptr;
    N::DrainPendingExSpawns();CHECK(coop::element::manager.rows.empty());}Reset();session.linked=false;session.ready=false;Catch();N::DrainPendingExSpawns();CHECK(coop::element::manager.rows.size()==1&&session.spawns.empty());}
void Dedup(){Reset();for(int i=0;i<300;++i)Catch();CHECK(N::g_pendingExSpawns.size()==1);Catch(At(Thicc));CHECK(N::g_pendingExSpawns.size()==2);
    ++objects[Actor].serial;Catch();CHECK(N::g_pendingExSpawns.size()==2);N::DrainPendingExSpawns();CHECK(coop::element::manager.rows.size()==2);}
void FiniteBirth(){for(int i=0;i<9;++i){Reset();float* values[]={&objects[Actor].location.X,&objects[Actor].location.Y,&objects[Actor].location.Z,
    &objects[Actor].rotation.Pitch,&objects[Actor].rotation.Yaw,&objects[Actor].rotation.Roll,&objects[Actor].scale.X,&objects[Actor].scale.Y,&objects[Actor].scale.Z};
    *values[i]=NAN;Catch();N::DrainPendingExSpawns();CHECK(coop::element::manager.rows.empty()&&session.spawns.empty());}
    Reset();Catch();onPose=[]{++world;};N::DrainPendingExSpawns();CHECK(coop::element::manager.rows.empty());}
void HostYaw(){Reset();float yaw=99;objects[Mesh].rotation.Yaw=123;CHECK(M::ReadBodyYaw(At(Actor),yaw)&&yaw==123);}
void ClientYaw(){Reset();CHECK(M::DriveBodyYaw(At(Thicc),156)&&objects[Mesh2].rotation.Yaw==156&&objects[Anim2].pawn==At(Thicc));}
void Gate(int32_t entry){Reset();CHECK(B::Prepare(At(Actor)));ue_wrap::script_gate::Call call{};call.object=At(Actor);call.locals=reinterpret_cast<uint8_t*>(&entry);
    CHECK(B::Before(call)==ue_wrap::script_gate::Verdict::Cancel&&!attaches&&!sets);}
void BeginPlay(){Gate(4128);}void Tick(){Gate(4566);}void Resumes(){for(int32_t e:{15,274,4375})Gate(e);}
void Damage(){for(int32_t e:{1104,1242,2894})Gate(e);}void Corpse(){for(int32_t e:{5630,6061})Gate(e);}
void NativeOwner(){Reset();ue_wrap::script_gate::Call call{};call.object=At(Actor);CHECK(B::Before(call)==ue_wrap::script_gate::Verdict::Run);
    G::MarkMirror(At(Other));call.object=At(Other);CHECK(B::Before(call)==ue_wrap::script_gate::Verdict::Run);}
void BirthReady(){for(int cause=0;cause<4;++cause){Reset();if(cause==0)watch=false;if(cause==1)registration=false;if(cause==2)markOk=false;
    if(cause==3)onResolve=[]{++world;};CHECK(!B::Prepare(At(Actor))&&!G::IsMarkedMirror(At(Actor)));}
    Reset();watch=false;CHECK(!B::ReadyForClass(At(RufusType))&&!B::ReadyForClass(At(ThiccType))&&B::ReadyForClass(At(PlushType)));}
void Adoption(){Reset();watch=false;CHECK(!adoption_fixture::BindAsMirror(5,At(Actor),L"rufus_C")&&!creates);
    Reset();createOk=false;CHECK(!adoption_fixture::BindAsMirror(5,At(Actor),L"rufus_C")&&!G::IsMarkedMirror(At(Actor)));
    Reset();CHECK(adoption_fixture::BindAsMirror(5,At(Actor),L"rufus_C")&&G::IsMarkedMirror(At(Actor))&&parks==1&&relays==1);}
void Attachment(){Reset();CHECK(M::PreparePresentation(At(Actor))&&attaches==1&&receivedParent==At(Arm)&&objects[Mesh].parent==At(Arm));
    CHECK(M::PreparePresentation(At(Actor))&&attaches==1);}
void Binding(){for(int cause=0;cause<5;++cause){Reset();onFrame=[cause]{if(cause==0)++objects[Actor].serial;if(cause==1)++objects[MeshType].serial;
    if(cause==2)objects[Actor].arm=At(Arm2);if(cause==3)++generation;if(cause==4)objects[Mesh].anim=At(Anim2);};
    CHECK(!M::PreparePresentation(At(Actor))&&!attaches&&!objects[Anim].pawn);}}
void Frame(){for(const wchar_t* name:{L"Parent",L"SocketName",L"LocationRule",L"RotationRule",L"ScaleRule",L"bWeldSimulatedBodies",L"ReturnValue"}){
    Reset();missingParam=name;CHECK(!M::PreparePresentation(At(Actor))&&!attaches);
    Reset();missingParam=name;badSize=16;CHECK(!M::PreparePresentation(At(Actor))&&!attaches);}
    Reset();onFrame=[]{++objects[Attach].serial;};CHECK(!M::PreparePresentation(At(Actor))&&!attaches);}
void RefusedAttach(){Reset();attachOk=false;CHECK(!M::PreparePresentation(At(Actor))&&!objects[Anim].pawn);attachOk=true;
    CHECK(M::PreparePresentation(At(Actor))&&attaches==2);
    Reset();nativeOk=false;CHECK(!M::PreparePresentation(At(Actor))&&attaches==1);nativeOk=true;CHECK(M::PreparePresentation(At(Actor))&&attaches==1);}
void Pawn(){Reset();CHECK(M::PreparePresentation(At(Actor))&&objects[Anim].pawn==At(Actor));}
void AnimRecovery(){for(bool foreign:{false,true}){Reset();if(foreign)objects[Anim].cls=At(ForeignType);else objects[Mesh].anim=nullptr;
    CHECK(!M::PreparePresentation(At(Actor))&&!attaches);objects[Anim].cls=At(AnimType);objects[Mesh].anim=At(Anim);
    CHECK(M::PreparePresentation(At(Actor)));}}
void BadYaw(){Reset();float yaw=99;objects[Mesh].rotation.Yaw=NAN;CHECK(!M::ReadBodyYaw(At(Actor),yaw)&&yaw==99);
    Reset();getOk=false;CHECK(!M::ReadBodyYaw(At(Actor),yaw)&&yaw==99);
    Reset();onGet=[]{++objects[Mesh].serial;};CHECK(!M::ReadBodyYaw(At(Actor),yaw)&&yaw==99);}
void PitchRoll(){Reset();CHECK(!M::DriveBodyYaw(At(Actor),NAN)&&!attaches&&!sets);CHECK(M::DriveBodyYaw(At(Actor),170)&&objects[Mesh].rotation.Pitch==10&&objects[Mesh].rotation.Roll==20);
    setOk=false;CHECK(!M::DriveBodyYaw(At(Actor),180));setOk=true;CHECK(M::DriveBodyYaw(At(Actor),180)&&attaches==1);}
}
int main(){using namespace test;void(*groups[])()={Family,Thiccfus,ConsoleCatch,SerialCatch,ScopeCatch,Dedup,FiniteBirth,HostYaw,ClientYaw,
    BeginPlay,Tick,Resumes,Damage,Corpse,NativeOwner,BirthReady,Adoption,Attachment,Binding,Frame,RefusedAttach,Pawn,AnimRecovery,BadYaw,PitchRoll};
    for(int i=0;i<25;++i){groups[i]();std::printf("PASS Rufus %d\n",i+1);}return 0;}

namespace coop::geom_octa_sync { void OnNpcSpawnSent(uint32_t,void*,const char*,size_t,bool,int) {} }
