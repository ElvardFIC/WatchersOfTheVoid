#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/players_registry.h"
#include "coop/interactables/atv_sync_internal.h"
#include "ue_wrap/devices/atv.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/world_identity.h"
#include <array>
#include <functional>
#include <limits>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int id=0,serial=1;bool live=true,atv=true,driven=false;void* cls=nullptr;void* world=nullptr;
 ue_wrap::FVector loc{};ue_wrap::FRotator rot{};std::wstring key;}objects[8];
coop::net::Session session,other;coop::net::Session* lane=&session;
uint32_t generation=1,sessionSerial=1;uintptr_t world=10;
int count=1,saved=0,spawnCalls=0,searches=0,destroys=0,sends=0,afterSpawn=-1;
bool held=false,poseOk=true,searchOk=true,destroyOk=true,sendOk=true,writeOk=true,collision=false,takenTwo=false,savedTwo=false;
void* result=&objects[1];std::wstring ensured;
std::function<void()> onPose,onSearch,onSpawn,onDestroy,onCount,onEnsure,onWrite,onSend;
Obj& O(void* p){return *static_cast<Obj*>(p);}void At(std::function<void()>& f){auto copy=std::move(f);f={};if(copy)copy();}
}
namespace coop::net::session_serial {uint32_t Current();}
namespace coop::atv_sync {
namespace R=ue_wrap::reflection;namespace A=ue_wrap::atv;
using ue_wrap::FVector;using ue_wrap::FRotator;using coop::net::WireKeyFromString;
std::unordered_map<std::wstring,AtvEntry> g_atvs;
coop::net::Session* LaneSession(){return test::lane;}
bool IsLocalGrabber(void*,void*){return test::held;}
bool SendAtvSpawn(const std::wstring&,void*,int){++test::sends;test::At(test::onSend);return test::sendOk;}
#include "atv_companion.h"
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&test::O(p).live;}int32_t InternalIndexOf(void* p){return p?test::O(p).id:-1;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<8&&p==&test::objects[i]&&test::objects[i].live;}
void* ClassOf(void* p){return p?test::O(p).cls:nullptr;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::generation;}void* CurrentWorld(){return reinterpret_cast<void*>(test::world);}
void* WorldOf(void* p){return p?test::O(p).world:nullptr;}
}
namespace coop::net::session_serial {uint32_t Current(){return test::sessionSerial;}}
namespace ue_wrap::atv {
bool IsAtv(void* p){return p&&test::O(p).atv;}
int CountLiveSettled(){test::At(test::onCount);return test::count;}
bool GetRootTransform(void* p,FVector& loc,FRotator& rot){loc=test::O(p).loc;rot=test::O(p).rot;test::At(test::onPose);return test::poseOk;}
bool IsDriven(void* p){return test::O(p).driven;}
bool FindCompanionLocation(void* p,FVector& loc){++test::searches;loc=test::O(p).loc;loc.Y+=150;test::At(test::onSearch);return test::searchOk;}
void* SpawnMirror(const std::wstring&,const FVector& loc,const FRotator& rot){
 ++test::spawnCalls;void* p=test::result;if(p){test::O(p).live=true;test::O(p).loc=loc;test::O(p).rot=rot;++test::count;}
 test::At(test::onSpawn);if(test::afterSpawn>=0)test::count=test::afterSpawn;return p;
}
bool DestroyMirror(void* p){++test::destroys;if(!test::destroyOk)return false;test::O(p).live=false;--test::count;test::At(test::onDestroy);return true;}
std::wstring GetKeyString(void* p){return test::O(p).key;}
std::wstring EnsureOwnSaveKey(void* p){auto key=test::ensured.empty()?test::O(p).key:test::ensured;test::At(test::onEnsure);return key;}
int LoadedSaveAtvCount(){return test::saved;}bool LoadedSaveNamesKey(const wchar_t*){return test::savedTwo;}
void* FindOtherLiveByKey(const std::wstring& key,void*){return (test::takenTwo&&key==L"coopatv2")||(test::collision&&key!=L"coopatv2")?&test::objects[2]:nullptr;}
bool WriteSaveKey(void* p,const std::wstring& key){if(!test::writeOk)return false;test::O(p).key=key;test::At(test::onWrite);return true;}
}
namespace test {
namespace S=coop::atv_sync;
S::AtvEntry Entry(void* p){S::AtvEntry e{};e.actor=p;e.idx=O(p).id;e.world=reinterpret_cast<void*>(world);return e;}
void Reset(){
 onPose=onSearch=onSpawn=onDestroy=onCount=onEnsure=onWrite=onSend={};S::ResetCompanion(false);S::g_companionBusy=false;
 session={};other={};lane=&session;++generation;++sessionSerial;++world;
 for(int i=0;i<8;++i){int serial=objects[i].serial+1;objects[i]={};objects[i].id=i;objects[i].serial=serial;objects[i].cls=&objects[3];objects[i].world=reinterpret_cast<void*>(world);}
 objects[3].world=nullptr;objects[0].key=L"original";objects[1].key=L"coopatv1";objects[2].key=L"other";
 S::g_atvs.clear();S::g_atvs[L"original"]=Entry(&objects[0]);coop::players::Registry::Get().local=&objects[0];
 count=1;saved=0;spawnCalls=searches=destroys=sends=0;afterSpawn=-1;held=false;
 poseOk=searchOk=destroyOk=sendOk=writeOk=true;collision=takenTwo=savedTwo=false;result=&objects[1];ensured.clear();
}
void Begin(){S::TrySpawnCompanion(1000);}void Mint(){Begin();S::TrySpawnCompanion(3000);CHECK(spawnCalls==1);}
void Published(){Mint();S::TrySpawnCompanion(4000);CHECK(S::g_companion.key==L"coopatv1");}
void WorldScope(){for(bool pointer:{false,true}){Reset();Mint();if(pointer)++world;else ++generation;S::TrySpawnCompanion(4000);CHECK(!S::g_companion.actor.body.Raw()&&!destroys);}}
void SessionScope(){for(int mode=0;mode<5;++mode){Reset();Mint();if(mode==0)++sessionSerial;if(mode==1)session.active=false;if(mode==2)session.mode=coop::net::Role::Client;if(mode==3)session.linked=false;if(mode==4)lane=&other;S::TrySpawnCompanion(4000);CHECK(!S::g_companion.actor.body.Raw());}
 Reset();Mint();++sessionSerial;S::ResetCompanion(true);CHECK(!destroys);}
void MintIdentity(){Reset();Mint();++objects[1].serial;S::TrySpawnCompanion(4000);CHECK(!S::g_companion.actor.body.Raw()&&!destroys);}
void SourceIdentity(){Reset();Begin();++objects[0].serial;S::TrySpawnCompanion(3000);CHECK(!spawnCalls&&S::g_companion.stillAt==3000);}
void FiniteSource(){for(int axis=0;axis<6;++axis){Reset();const float n=std::numeric_limits<float>::quiet_NaN();switch(axis){case 0:objects[0].loc.X=n;break;case 1:objects[0].loc.Y=n;break;case 2:objects[0].loc.Z=n;break;case 3:objects[0].rot.Pitch=n;break;case 4:objects[0].rot.Yaw=n;break;case 5:objects[0].rot.Roll=n;break;}Begin();S::TrySpawnCompanion(3000);CHECK(!spawnCalls&&!S::g_companion.waiting);}}
void SmallMovement(){Reset();Begin();objects[0].loc.X=3;S::TrySpawnCompanion(3000);S::TrySpawnCompanion(4000);CHECK(!spawnCalls);S::TrySpawnCompanion(5000);CHECK(spawnCalls==1);}
void AngularMovement(){for(int axis=0;axis<3;++axis){Reset();Begin();if(axis==0)objects[0].rot.Pitch=2;else if(axis==1)objects[0].rot.Yaw=2;else objects[0].rot.Roll=2;S::TrySpawnCompanion(3000);CHECK(!spawnCalls&&S::g_companion.stillAt==3000);}}
void Clock(){Reset();S::TrySpawnCompanion(0);S::TrySpawnCompanion(2000);CHECK(spawnCalls==1);Reset();Begin();S::TrySpawnCompanion(500);S::TrySpawnCompanion(2500);CHECK(spawnCalls==1);}
void Counts(){for(int c:{-1,0,2,3}){Reset();Begin();count=c;S::TrySpawnCompanion(2000);count=1;S::TrySpawnCompanion(4000);S::TrySpawnCompanion(5000);CHECK(!spawnCalls);}}
void Occupied(){for(bool carry:{false,true}){Reset();Begin();if(carry)held=true;else objects[0].driven=true;S::TrySpawnCompanion(3000);held=false;objects[0].driven=false;S::TrySpawnCompanion(4000);S::TrySpawnCompanion(5000);CHECK(!spawnCalls);}}
void SearchSource(){for(bool serial:{false,true}){Reset();Begin();onSearch=[serial]{if(serial)++objects[0].serial;else objects[0].loc.X+=10;};S::TrySpawnCompanion(3000);CHECK(!spawnCalls);}
 Reset();Begin();onSearch=[]{onCount=[]{objects[0].loc.X+=10;};};S::TrySpawnCompanion(3000);CHECK(!spawnCalls);}
void SearchCount(){Reset();Begin();onSearch=[]{count=2;};S::TrySpawnCompanion(3000);CHECK(!spawnCalls&&searches==1);}
void SearchScope(){Reset();Begin();searchOk=false;S::TrySpawnCompanion(3000);CHECK(S::g_companion.searched);++sessionSerial;count=2;S::TrySpawnCompanion(3100);CHECK(!S::g_companion.searched&&!S::g_companion.searchAt);}
void SavedSource(){Reset();objects[0].key=L"coopatv1";S::g_atvs.clear();S::g_atvs[L"coopatv1"]=Entry(&objects[0]);Mint();CHECK(spawnCalls==1);}
void SaveWait(){Reset();saved=2;Begin();S::TrySpawnCompanion(3000);S::TrySpawnCompanion(5000);CHECK(!spawnCalls);S::TrySpawnCompanion(8000);CHECK(spawnCalls==1);Reset();saved=2;Begin();S::TrySpawnCompanion(3000);count=2;S::TrySpawnCompanion(8000);CHECK(!spawnCalls);}
void NestedSpawn(){Reset();onSpawn=[]{S::TrySpawnCompanion(3000);};Mint();CHECK(spawnCalls==1);}
void ReturnType(){for(bool foreignWorld:{false,true}){Reset();if(foreignWorld)objects[1].world=reinterpret_cast<void*>(999);else objects[1].atv=false;Mint();CHECK(!S::g_companion.actor.body.Raw()&&!destroys);}}
void ThirdRetry(){Reset();afterSpawn=3;Mint();CHECK(!S::g_companion.actor.body.Raw()&&destroys==1);afterSpawn=-1;count=1;S::TrySpawnCompanion(4000);S::TrySpawnCompanion(6000);CHECK(spawnCalls==2);}
void DestroyRetry(){Reset();afterSpawn=3;destroyOk=false;Mint();CHECK(S::g_companion.cleanup&&S::g_companion.actor.body.Raw());S::TrySpawnCompanion(4000);CHECK(destroys==2&&spawnCalls==1);destroyOk=true;S::TrySpawnCompanion(5000);CHECK(!S::g_companion.actor.body.Raw()&&destroys==3);}
void IndexOwner(){Reset();Published();S::g_atvs[L"coopatv1"]=Entry(&objects[2]);S::RemoveMintedCompanion();CHECK(S::g_atvs.at(L"coopatv1").actor==&objects[2]&&objects[2].live&&session.sent.empty());}
void MissingIndex(){Reset();Published();S::g_atvs.erase(L"coopatv1");S::RemoveMintedCompanion();CHECK(session.sent.empty());}
void PublishKey(){for(const auto& key:{std::wstring(L"None"),std::wstring(32,L'a'),std::wstring(L"mismatch")}){Reset();Mint();ensured=key;CHECK(!S::PublishCompanion(&objects[1])&&S::g_atvs.size()==1);}Reset();Mint();onEnsure=[]{++objects[1].serial;};CHECK(!S::PublishCompanion(&objects[1]));}
void PreserveEntry(){Reset();Mint();auto e=Entry(&objects[1]);e.occupantSlot=2;e.syncerSlot=3;e.lastSentMs=123;S::g_atvs[L"coopatv1"]=e;CHECK(S::PublishCompanion(&objects[1]));CHECK(S::g_atvs.at(L"coopatv1").occupantSlot==2&&S::g_atvs.at(L"coopatv1").lastSentMs==123);}
void Rebind(){Reset();Published();S::g_atvs[L"coopatv2"]=Entry(&objects[2]);CHECK(!S::RebindCompanionKey(L"coopatv1",L"coopatv2")&&objects[1].key==L"coopatv1");S::g_atvs.erase(L"coopatv2");S::g_atvs[L"coopatv1"].occupantSlot=2;
 onWrite=[]{S::g_atvs[L"coopatv1"].occupantSlot=3;};CHECK(S::RebindCompanionKey(L"coopatv1",L"coopatv2"));CHECK(!S::g_atvs.count(L"coopatv1")&&S::g_atvs.at(L"coopatv2").occupantSlot==3&&S::g_atvs.at(L"coopatv2").world==reinterpret_cast<void*>(world));}
void SendRetry(){Reset();sendOk=false;Published();CHECK(S::g_companion.announceDue&&sends==1);S::TrySpawnCompanion(4250);CHECK(sends==2);sendOk=true;S::TrySpawnCompanion(4500);CHECK(sends==3&&!S::g_companion.announceDue);}
}
int main(){using namespace test;void(*groups[])()={WorldScope,SessionScope,MintIdentity,SourceIdentity,FiniteSource,SmallMovement,AngularMovement,Clock,Counts,Occupied,SearchSource,SearchCount,SearchScope,SavedSource,SaveWait,NestedSpawn,ReturnType,ThirdRetry,DestroyRetry,IndexOwner,MissingIndex,PublishKey,PreserveEntry,Rebind,SendRetry};for(int i=0;i<25;++i){groups[i]();std::printf("PASS companion %d\n",i+1);}return 0;}
