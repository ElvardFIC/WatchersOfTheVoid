// Production spawn/teleport bodies; engine and reflected-call boundaries doubled.
#include "ue_wrap/devices/atv.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <functional>
#include <limits>
namespace test {
struct Obj {int id=0,serial=1,kind=-1;bool live=true;void* cls=nullptr;void* world=nullptr;ue_wrap::reflection::FName key{};} objects[16];
void *actorClass,*baseClass,*serviceClass,*service,*context,*beginFunction,*finishFunction,*teleportFunction,*beginResult;
int calls[3]{},destroyed=0,failFunction=-1,invalidFunction=-1,finishMode=0;
int keyOffset=offsetof(Obj,key);
uint32_t generation=1;
uintptr_t worldToken=101;
bool lineage=true,beginReadOk=true,callOk[3]{true,true,true};
void* destroyedActor=nullptr;
std::wstring failField,nameValue;
std::function<void()> onName,onKey;
std::function<void(int)> onFrame,onNative;
ue_wrap::FTransform lastTransform{};
ue_wrap::FVector lastTeleport{};
ue_wrap::FRotator lastFacing{};
void* lastActorClass=nullptr;
struct Begin {void* ctx;void* cls;ue_wrap::FTransform transform;uint8_t collision;void* owner;void* result;};
struct Finish {void* actor;ue_wrap::FTransform transform;void* result;};
struct Teleport {ue_wrap::FVector location;ue_wrap::FRotator rotation;};
void* World(){return reinterpret_cast<void*>(worldToken);}
void Callback(std::function<void()>& f){auto copy=std::move(f);f={};if(copy)copy();}
void Callback(std::function<void(int)>& f,int stage){auto copy=f;if(copy)copy(stage);}
void Reset(){
 onName=onKey={};onFrame=onNative={};++generation;worldToken=generation+100;
 for(int i=0;i<16;++i){const int serial=objects[i].serial+1;objects[i]={};objects[i].id=i;objects[i].serial=serial;objects[i].cls=&objects[15];}
 actorClass=baseClass=&objects[2];serviceClass=&objects[4];service=&objects[5];context=&objects[6];
 beginFunction=&objects[7];finishFunction=&objects[8];teleportFunction=&objects[9];beginResult=&objects[0];
 objects[0].cls=actorClass;objects[0].world=World();objects[1].world=World();objects[1].key={777,0};objects[5].cls=serviceClass;objects[14].cls=&objects[13];
 objects[7].kind=objects[10].kind=0;objects[8].kind=1;objects[9].kind=2;
 std::memset(calls,0,sizeof(calls));destroyed=0;destroyedActor=nullptr;failFunction=invalidFunction=-1;finishMode=0;
 failField.clear();nameValue=L"coopatv1";lineage=beginReadOk=true;for(auto& ok:callOk)ok=true;
 lastTransform={};lastTeleport={};lastFacing={};lastActorClass=nullptr;keyOffset=offsetof(Obj,key);
}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::reflection {
void* FindClass(const wchar_t* n){if(!_wcsicmp(n,L"GameplayStatics"))return test::serviceClass;return !_wcsicmp(n,L"ATV_C")?test::actorClass:nullptr;}
void* FindClassDefaultObject(const wchar_t*){return test::service;}
void* FindFunction(void*,const wchar_t* n){return !_wcsicmp(n,L"BeginDeferredActorSpawnFromClass")?test::beginFunction:test::finishFunction;}
void* FindDispatchFunctionCached(void*,const wchar_t*){return test::teleportFunction;}
void* ClassOf(void* p){return p?static_cast<test::Obj*>(p)->cls:nullptr;}
bool IsDescendantOfAny(void* cls,void* const*,size_t,int){return test::lineage && (cls==&test::objects[2] || cls==&test::objects[11]);}
int32_t FindPropertyOffset(void*,const wchar_t*){return test::keyOffset;}
bool IsLive(void* p){return p && static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t i){return i>=0 && i<16 && p==&test::objects[i] && test::objects[i].live;}
std::wstring ToString(const FName& name){test::Callback(test::onKey);return name.ComparisonIndex==123?test::nameValue:L"mismatch";}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::generation;}
void* CurrentWorld(){return test::World();}
void* WorldOf(void* p){return p?static_cast<test::Obj*>(p)->world:nullptr;}
}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring& key){test::nameValue=key;test::Callback(test::onName);return {123,0};}}
namespace ue_wrap::engine {
void* GetWorldContext(){return test::context;}
void RotatorToQuat(float,float,float,float& x,float& y,float& z,float& w){x=y=z=0;w=1;}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn){
 if(!fn)return;
 const int stage=static_cast<test::Obj*>(fn)->kind;test::Callback(test::onFrame,stage);
 if(stage==test::invalidFunction){fn_=nullptr;return;}
 buf_.resize(stage==0?sizeof(test::Begin):stage==1?sizeof(test::Finish):sizeof(test::Teleport));
}
bool ParamFrame::SetRaw(const wchar_t* name,const void* data,int32_t size){
 const int stage=static_cast<test::Obj*>(fn_)->kind;if(stage==test::failFunction && test::failField==name)return false;
 size_t off=0;
 if(stage==0){if(!wcscmp(name,L"ActorClass"))off=offsetof(test::Begin,cls);if(!wcscmp(name,L"SpawnTransform"))off=offsetof(test::Begin,transform);if(!wcscmp(name,L"CollisionHandlingOverride"))off=offsetof(test::Begin,collision);if(!wcscmp(name,L"Owner"))off=offsetof(test::Begin,owner);}
 if(stage==1 && !wcscmp(name,L"SpawnTransform"))off=offsetof(test::Finish,transform);
 if(stage==2 && !wcscmp(name,L"NewRotation"))off=offsetof(test::Teleport,rotation);
 assert(off+size<=buf_.size());std::memcpy(buf_.data()+off,data,size);return true;
}
bool ParamFrame::GetRaw(const wchar_t* name,void* data,int32_t size)const{
 assert(!wcscmp(name,L"ReturnValue") && size==sizeof(void*));const int stage=static_cast<test::Obj*>(fn_)->kind;
 if(stage==0 && !test::beginReadOk)return false;if(stage==1 && test::finishMode==1)return false;
 const size_t offset=stage==0?offsetof(test::Begin,result):offsetof(test::Finish,result);
 std::memcpy(data,buf_.data()+offset,size);return true;
}
bool Call(void* object,ParamFrame& frame){
 const int stage=static_cast<test::Obj*>(frame.function())->kind;++test::calls[stage];
 if(stage<2)assert(object==test::service);
 if(stage==0){
  auto* args=static_cast<test::Begin*>(frame.data());assert(args->ctx==test::context && args->collision==1 && !args->owner);
  test::lastActorClass=args->cls;test::lastTransform=args->transform;args->result=test::beginResult;
  if(args->result==&test::objects[0]){test::objects[0].cls=args->cls;test::objects[0].world=test::World();}
 }
 if(stage==1){
  auto* args=static_cast<test::Finish*>(frame.data());args->result=test::finishMode==2?nullptr:test::finishMode==3?&test::objects[1]:args->actor;
 }
 if(stage==2){assert(object==&test::objects[0]);auto* args=static_cast<test::Teleport*>(frame.data());test::lastTeleport=args->location;test::lastFacing=args->rotation;}
 test::Callback(test::onNative,stage);return test::callOk[stage];
}
}
namespace ue_wrap::atv {
namespace R=ue_wrap::reflection;
bool EnsureResolved(){return true;}
void* AtvClass(){return test::baseClass;}
bool IsAtv(void* p){void* base=AtvClass();return R::IsDescendantOfAny(R::ClassOf(p),&base,1);}
bool DestroyMirror(void* p){++test::destroyed;test::destroyedActor=p;static_cast<test::Obj*>(p)->live=false;return true;}
#include "atv_spawn.inc"
}
int main(){
 namespace A=ue_wrap::atv;using F=A::SpawnFail;using namespace test;
 const ue_wrap::FVector loc{100,200,50};const ue_wrap::FRotator rot{};F why;
 const auto spawn=[&]{return A::SpawnMirror(L"ATV_C",loc,rot,L"coopatv1",&why);};
 for(const auto field:{L"WorldContextObject",L"ActorClass",L"SpawnTransform",L"CollisionHandlingOverride",L"Owner"}){
  Reset();failFunction=0;failField=field;assert(!spawn() && why==F::BeginFailed && !calls[0] && !calls[1] && !destroyed);
 }
 for(const auto field:{L"Actor",L"SpawnTransform"}){
  Reset();failFunction=1;failField=field;assert(!spawn() && why==F::FinishFailed && calls[0]==1 && !calls[1] && destroyed==1);
 }
 for(int mode=1;mode<=3;++mode){
  Reset();finishMode=mode;assert(!spawn() && why==F::FinishFailed && calls[1]==1 && !calls[2] && destroyed==1);
 }
 Reset();invalidFunction=1;assert(!spawn() && why==F::FinishFailed && destroyed==1);
 for(const auto field:{L"NewLocation",L"NewRotation"}){
  Reset();failFunction=2;failField=field;assert(!A::TeleportRig(&objects[0],loc,rot) && !calls[2]);
 }
 Reset();assert(spawn()==&objects[0] && why==F::None && !destroyed);
 assert(calls[0]==1 && calls[1]==1 && calls[2]==1 && objects[0].key.ComparisonIndex==123);
 std::puts("PASS ATV spawn argument failures, finish result/cleanup, teleport arguments and successful mint");return 0;
}
