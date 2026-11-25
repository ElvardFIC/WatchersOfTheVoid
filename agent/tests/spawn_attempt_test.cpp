#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <cmath>
#include "ue_wrap/engine/engine_pawn.h"
using ue_wrap::engine::PawnInertResult;
using ue_wrap::engine::MakePawnInert;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
#define UE_LOGE(...) ((void)0)
#define UE_LOGW(...) ((void)0)
#define UE_LOGI(...) ((void)0)
struct FVector{float X=0,Y=0,Z=0;};struct FTransform{};
using BeforeFinishFn=void(*)(void*,void*);
namespace P::off {constexpr int APawn_AutoPossessPlayer=0,APawn_AutoPossessAI=0,AActor_AutoReceiveInput=0,AActor_bBlockInput=0,APawn_AIControllerClass=0;}
void* g_gsCdo=reinterpret_cast<void*>(1);
void* g_beginSpawnFn=reinterpret_cast<void*>(2);
void* g_finishSpawnFn=reinterpret_cast<void*>(3);
constexpr uint8_t kAlwaysSpawn=1;
bool resolved=true,beginOkay=true,finishOkay=true,actorLive=true;
bool schema=true,typeLive=true;uint32_t world=1;
std::function<void()> onBegin,onFinish;
bool* observed=nullptr;int calls=0,destroyed=0;
void* actor=reinterpret_cast<void*>(4);
struct ParamFrame{
 void* function;explicit ParamFrame(void* fn):function(fn){}
 bool valid()const{return schema;}
 template<class T>bool Set(const wchar_t*,T){return true;}
 bool SetRaw(const wchar_t*,const void*,size_t){return true;}
 template<class T>T Get(const wchar_t*){return reinterpret_cast<T>(actor);}
};
namespace R {bool IsLive(void* value){return value&&actorLive;}void* ClassOf(void*){return reinterpret_cast<void*>(9);}}
namespace world_identity {uint32_t Generation(){return world;}}
struct CachedObjRef {void* value=nullptr;void Set(void* pointer){value=pointer;}void* Raw()const{return value;}
 void* Get()const{return value && (value==actor?actorLive:typeLive)?value:nullptr;}};
bool ResolveSpawn(){return resolved;}
namespace ue_wrap::engine {PawnInertResult MakePawnInert(void*){return PawnInertResult::Made;}}
FTransform MakeTransform(const FVector&){return {};}
void* GetWorldContext(){return g_gsCdo;}
bool Call(void*,ParamFrame& frame){CHECK(observed&&*observed);++calls;auto hook=frame.function==g_beginSpawnFn?onBegin:onFinish;if(hook)hook();return frame.function==g_beginSpawnFn?beginOkay:finishOkay;}
void DestroyActor(void*){++destroyed;}
#include "spawn_attempt.inc"
int main(){
 for(int cause=0;cause<6;++cause){
  resolved=beginOkay=finishOkay=actorLive=true;actor=reinterpret_cast<void*>(4);calls=destroyed=0;
  bool attempted=true;observed=&attempted;void* cls=reinterpret_cast<void*>(5);
  if(cause==0)cls=nullptr;if(cause==1)resolved=false;if(cause==2)beginOkay=false;
  if(cause==3)actor=nullptr;if(cause==4)finishOkay=false;
  void* result=SpawnActorImpl(cls,{},false,nullptr,nullptr,&attempted);
  CHECK(attempted==(cause>=2));CHECK((result!=nullptr)==(cause==5));
  CHECK(calls==(cause<2?0:cause<4?1:2));CHECK(destroyed==(cause==4?1:0));
 }
 puts("PASS UI03 actual SpawnActorImpl: preflight refusal, before-call receipt, null begin, failed finish and committed actor");
 for(int cause=0;cause<5;++cause){
  resolved=beginOkay=finishOkay=actorLive=schema=typeLive=true;actor=reinterpret_cast<void*>(4);calls=destroyed=0;onBegin=onFinish={};
  bool attempted=true,allowed=true;observed=&attempted;
  if(cause==0)schema=false;
  if(cause==1)onBegin=[]{++world;};
  if(cause==2)onBegin=[&]{allowed=false;};
  if(cause==3)onFinish=[]{++world;};
  if(cause==4)onFinish=[]{actorLive=false;};
  CHECK(!SpawnActorImpl(reinterpret_cast<void*>(5),{},false,nullptr,nullptr,&attempted,[&]{return allowed;}));
  CHECK(attempted==(cause!=0));CHECK(calls==(cause==0?0:cause<3?1:2));CHECK(destroyed==(cause==2?1:0));
 }
 puts("PASS UI09 actual SpawnActorImpl: schema/world/caller/actor continuations and owned unfinished cleanup");
}
