// Actual sit wrapper, ParamFrame, rotation wrapper and extracted view methods.
#ifndef SEAT_WRAPPER_SOURCE
#define SEAT_WRAPPER_SOURCE "../../src/votv-coop/src/ue_wrap/actors/sit_pose.cpp"
#endif
#include SEAT_WRAPPER_SOURCE
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include "coop/net/protocol.h"
#ifndef SEAT_BODY_YAW_HEADER
#define SEAT_BODY_YAW_HEADER "coop/player/puppet_body_yaw.h"
#endif
#include SEAT_BODY_YAW_HEADER
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {alignas(8)uint8_t memory[0x800]{};int id=0,serial=1;bool live=true;void* cls=nullptr;void* world=nullptr;
 uint8_t flags=0xa5;void* body=nullptr;void* vis=nullptr;void* component=nullptr;int kind=0,calls=0;
 ue_wrap::FVector relative{},location{};ue_wrap::FRotator rotation{};ue_wrap::reflection::FName name{1,0};}objects[120];
void* animBase=&objects[110];void* relativeFunction=&objects[112];void* rotationFunction=&objects[113];
uint32_t generation=1;uintptr_t worldToken=11;
float half=88;bool callOk=true,rotationRead=true,baseLoaded=true;
int failAt=0,totalCalls=0,lookCalls=0;float probeFacing=0;
std::wstring missingParam;ue_wrap::FVector forward{1,0,0},look{};
std::function<void()> onFrame,onCall,onBase,onParent,onHead,onBool;
Obj& O(void* p){return *static_cast<Obj*>(p);}void At(std::function<void()>& f){auto copy=std::move(f);f={};if(copy)copy();}
void Parent(void* p,void* parent){*reinterpret_cast<void**>(static_cast<uint8_t*>(p)+ue_wrap::profile::off::USceneComponent_AttachParent)=parent;}
void Anim(void* p,void* anim){*reinterpret_cast<void**>(static_cast<uint8_t*>(p)+ue_wrap::profile::off::USkeletalMesh_AnimScriptInstance)=anim;}
void Reset(){
 onFrame=onCall=onBase=onParent=onHead=onBool={};++generation;++worldToken;
 for(int i=0;i<120;++i){int serial=objects[i].serial+1;objects[i]={};objects[i].id=i;objects[i].serial=serial;
  objects[i].cls=&objects[111];if(i<110)objects[i].world=reinterpret_cast<void*>(worldToken);}
 for(int i=0;i<32;++i)objects[i].body=&objects[32+i];
 objects[65].cls=&objects[110];Anim(&objects[32],&objects[65]);objects[0].component=&objects[68];
 objects[112].kind=0;objects[113].kind=1;objects[115].kind=0;objects[116].kind=1;
 animBase=&objects[110];relativeFunction=&objects[112];rotationFunction=&objects[113];
 half=88;callOk=rotationRead=baseLoaded=true;failAt=totalCalls=lookCalls=0;missingParam.clear();forward={1,0,0};look={};probeFacing=0;
 for(auto& entry:ue_wrap::sit_pose::g_shift)entry={};++ue_wrap::sit_pose::g_shiftRevision;
}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&test::O(p).live;}int32_t InternalIndexOf(void* p){return p?test::O(p).id:-1;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<120&&p==&test::objects[i]&&test::objects[i].live;}
void* ObjectAt(int32_t i){return i>=0&&i<120?&test::objects[i]:nullptr;}
void* ClassOf(void* p){return p?test::O(p).cls:nullptr;}
bool IsDescendantOfAny(void* cls,void* const* bases,size_t count,int){for(size_t i=0;i<count;++i)if(cls==bases[i])return true;return false;}
void* FindDispatchFunctionCached(void*,const wchar_t* n){return !wcscmp(n,L"K2_SetRelativeLocation")?test::relativeFunction:test::rotationFunction;}
int32_t FindPropertyOffset(void*,const wchar_t* n){return !wcscmp(n,L"lag_fl")?offsetof(test::Obj,component):!wcscmp(n,L"sitarrow")?offsetof(test::Obj,component):-1;}
bool FindBoolProperty(void*,const wchar_t* n,int32_t& offset,uint8_t& mask){
 offset=offsetof(test::Obj,flags);mask=!wcscmp(n,L"isOnAtv")?2:4;test::At(test::onBool);return true;
}
int32_t FunctionFrameSize(void*){test::At(test::onFrame);return 16;}
std::vector<ParamInfo> FunctionParams(void* fn){
 std::vector<ParamInfo> p={{test::O(fn).kind==0?L"NewLocation":L"NewRotation",0,12,0},{L"bSweep",12,1,0},{L"bTeleport",13,1,0}};
 for(auto it=p.begin();it!=p.end();)if(it->name==test::missingParam)it=p.erase(it);else ++it;return p;
}
const FName& NameOf(void* p){return test::O(p).name;}std::wstring ToString(const FName&){return L"function";}
bool CallFunction(void* p,void* fn,void* args){
 ++test::totalCalls;++test::O(p).calls;const bool ok=test::callOk&&test::totalCalls!=test::failAt;
 CHECK(!*reinterpret_cast<bool*>(static_cast<uint8_t*>(args)+12)&&*reinterpret_cast<bool*>(static_cast<uint8_t*>(args)+13));
 if(ok){if(test::O(fn).kind==0)std::memcpy(&test::O(p).relative,args,12);else std::memcpy(&test::O(p).rotation,args,12);}
 test::At(test::onCall);return ok;
}
}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t*){return test::baseLoaded?test::animBase:nullptr;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::generation;}void* CurrentWorld(){return reinterpret_cast<void*>(test::worldToken);}
void* WorldOf(void* p){return p?test::O(p).world:nullptr;}
}
namespace ue_wrap::puppet {
void* GetNativeBodyMeshComponent(void* p){return test::O(p).body;}
void* GetMeshPlayerVisibleComponent(void* p){return test::O(p).vis;}
void DriveHeadLookAtWorld(void*,const FVector& p){++test::lookCalls;test::look=p;}
}
namespace ue_wrap::engine {
float GetActorCharacterHalfHeight(void*){return test::half;}
FVector GetActorForwardVector(void*){return test::forward;}
FVector GetComponentRelativeLocation(void* p){auto base=test::O(p).relative;test::At(test::onBase);return base;}
bool TryGetComponentWorldRotation(void* p,FRotator& r){r=test::O(p).rotation;test::At(test::onParent);return test::rotationRead;}
bool TryGetActorRotation(void* p,FRotator& r){r=test::O(p).rotation;return true;}
namespace R=reflection;namespace P=profile;
#include "seat_rotation.inc"
}
namespace coop::puppet_head_probe {void Tick(void*,float body,float,float){test::probeFacing=body;}}
namespace coop {
class RemotePlayer {public:
 void* actor_=&test::objects[0];uint8_t curStateBits_=net::kStateBitSitting;
 ue_wrap::FRotator aim{10,90,0};
 bool valid()const{return actor_&&ue_wrap::reflection::IsLive(actor_);}
 #include "chair_sitting.inc"
 ue_wrap::FRotator GetSyncedAimRotation()const{return aim;}
 bool TryGetHeadPosition(ue_wrap::FVector& out)const{out=test::O(actor_).location;out.Z+=100;test::At(test::onHead);return true;}
 void RefreshSeatView();void RefreshView();
};
namespace R=ue_wrap::reflection;namespace E=ue_wrap::engine;namespace Pup=ue_wrap::puppet;
#include "seat_view.inc"
}
namespace test {
namespace S=ue_wrap::sit_pose;
bool Near(float a,float b){return std::fabs(a-b)<.001f;}
void AnimRecovery(){
 Reset();S::DriveSitting(&objects[0],true);CHECK((objects[65].flags&2)!=0);
 animBase=&objects[114];objects[65].cls=animBase;objects[65].flags=0xa5;S::DriveSitting(&objects[0],true);CHECK((objects[65].flags&2)!=0);
 baseLoaded=false;objects[65].flags=0xa5;S::DriveSitting(&objects[0],true);CHECK(objects[65].flags==0xa5);
}
void SitMask(){Reset();S::DriveSitting(&objects[0],true);CHECK(objects[65].flags==0xa3&&S::ReadIsOnAtv(&objects[0]));}
void IkMask(){Reset();S::DriveSitting(&objects[0],true);S::DriveSitting(&objects[0],false);CHECK(objects[65].flags==0xa5&&!S::ReadIsOnAtv(&objects[0]));}
void Setter(){
 for(auto field:{L"NewLocation",L"bSweep",L"bTeleport"}){Reset();missingParam=field;CHECK(!S::SetRelative(&objects[32],{1,2,3})&&!totalCalls);}
 Reset();CHECK(S::SetRelative(&objects[32],{1,2,3}));relativeFunction=&objects[115];CHECK(S::SetRelative(&objects[32],{4,5,6})&&objects[32].relative.X==4);
 Reset();onFrame=[]{++objects[32].serial;};CHECK(!S::SetRelative(&objects[32],{1,2,3})&&!totalCalls);
}
void Geometry(){
 for(float bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-1.f,10000.f}){Reset();half=bad;CHECK(S::ModelHalfLength(&objects[0])==88&&std::isfinite(S::ModelHead(&objects[0])));}
 Reset();S::ShiftMesh(&objects[0],std::numeric_limits<float>::infinity(),3);CHECK(!totalCalls);
 forward.X=std::numeric_limits<float>::quiet_NaN();S::ShiftMesh(&objects[0],10,3);CHECK(!totalCalls);
}
void BaseCapture(){
 for(int mode=0;mode<3;++mode){Reset();onBase=[mode]{if(mode==0)++objects[32].serial;else if(mode==1)objects[32].cls=&objects[114];else Parent(&objects[32],&objects[64]);};
  S::ShiftMesh(&objects[0],10,0);CHECK(!totalCalls);if(auto* entry=S::FindShift(&objects[0],false))CHECK(!entry->a.held);}
 Reset();objects[32].relative.X=std::numeric_limits<float>::quiet_NaN();S::ShiftMesh(&objects[0],10,0);CHECK(!totalCalls);
}
void ParentSpace(){
 Reset();objects[32].relative={1,2,3};Parent(&objects[32],&objects[64]);objects[64].rotation.Roll=90;
 S::ShiftMesh(&objects[0],10,0);CHECK(Near(objects[32].relative.Y,-8)&&Near(objects[32].relative.Z,3));
 Reset();Parent(&objects[32],&objects[64]);objects[64].rotation.Yaw=90;S::ShiftMesh(&objects[0],0,10);CHECK(Near(objects[32].relative.Y,-10));
 Reset();Parent(&objects[32],&objects[64]);objects[64].rotation.Pitch=45;forward={.70710678f,0,.70710678f};
 S::ShiftMesh(&objects[0],0,10);CHECK(Near(objects[32].relative.X,10)&&Near(objects[32].relative.Z,0));
 Reset();Parent(&objects[32],&objects[64]);onParent=[]{Parent(&objects[32],nullptr);};S::ShiftMesh(&objects[0],10,0);CHECK(!totalCalls);
}
void RestoreRetry(){
 Reset();objects[32].relative.Z=2;S::ShiftMesh(&objects[0],10,0);CHECK(objects[32].relative.Z==12);
 callOk=false;S::ClearMeshShift(&objects[0]);CHECK(objects[32].relative.Z==12);
 callOk=true;S::ClearMeshShift(&objects[0]);CHECK(objects[32].relative.Z==2);
 S::ShiftMesh(&objects[0],10,0);onCall=[]{S::ShiftMesh(&objects[0],20,0);};S::ClearMeshShift(&objects[0]);CHECK(objects[32].relative.Z==22);
 S::ClearMeshShift(&objects[0]);CHECK(objects[32].relative.Z==2);
 Reset();objects[0].vis=&objects[66];failAt=2;S::ShiftMesh(&objects[0],10,0);failAt=0;S::ShiftMesh(&objects[0],10,0);
 CHECK(objects[32].relative.Z==10&&objects[66].relative.Z==10);
}
void Capacity(){Reset();for(int i=0;i<32;++i)S::ShiftMesh(&objects[i],10,0);for(int i=0;i<32;++i)CHECK(objects[32+i].relative.Z==10);
 for(int i=0;i<32;++i)S::ClearMeshShift(&objects[i]);for(int i=0;i<32;++i)CHECK(objects[32+i].relative.Z==0);}
void SeatedYaw(){
 coop::puppet_body_yaw::State state;state.Reset(0);state.Seated(1000,0);CHECK(state.Seated(1050,1)&&state.Yaw()==1);
 state.Seated(1100,179);state.Seated(1150,-179);CHECK(state.Yaw()==-179);CHECK(!state.Update(1200,0,-179,0));
}
void HeadRefresh(){
 Reset();coop::RemotePlayer p;objects[0].location={100,200,300};objects[0].rotation.Yaw=33;p.RefreshSeatView();
 CHECK(lookCalls==1&&Near(look.X,100)&&look.Y>690&&look.Z>480&&probeFacing==33);
 objects[0].location.X=900;p.RefreshSeatView();CHECK(Near(look.X,900));
 onHead=[]{++objects[0].serial;};int previous=lookCalls;p.RefreshSeatView();CHECK(lookCalls==previous);
}
void FlashlightAim(){
 Reset();coop::RemotePlayer p;objects[0].rotation={45,22,90};p.RefreshSeatView();CHECK(objects[68].rotation.Pitch==10&&objects[68].rotation.Yaw==90&&objects[68].rotation.Roll==0);
 p.curStateBits_=coop::net::kStateBitRagdoll;int calls=totalCalls;p.RefreshSeatView();CHECK(totalCalls==calls);
}
void RotationSetter(){
 for(auto field:{L"NewRotation",L"bSweep",L"bTeleport"}){Reset();missingParam=field;CHECK(!ue_wrap::engine::SetComponentWorldRotation(&objects[68],{10,20,0})&&!totalCalls);}
 Reset();CHECK(ue_wrap::engine::SetComponentWorldRotation(&objects[68],{10,20,0}));rotationFunction=&objects[116];CHECK(ue_wrap::engine::SetComponentWorldRotation(&objects[68],{30,40,0})&&objects[68].rotation.Yaw==40);
 Reset();rotationFunction=nullptr;coop::RemotePlayer p;p.RefreshSeatView();CHECK(!totalCalls&&objects[68].rotation.Yaw==0);
}
}
int main(int argc,char** argv){using namespace test;struct Group{const char* name;void(*run)();}groups[]={{"6 anim recovery",AnimRecovery},{"7 sit mask",SitMask},{"8 IK mask",IkMask},
 {"9 mesh setter",Setter},{"10 geometry",Geometry},{"11 base capture",BaseCapture},{"12 parent space",ParentSpace},{"13 restore retry",RestoreRetry},
 {"14 capacity",Capacity},{"15 seated yaw",SeatedYaw},{"16 head refresh",HeadRefresh},{"17 flashlight aim",FlashlightAim},{"18 rotation setter",RotationSetter}};
 bool ran=false;for(const auto& group:groups)if(argc<2||std::atoi(argv[1])==std::atoi(group.name)){group.run();std::printf("PASS seat %s\n",group.name);ran=true;}CHECK(ran);return 0;
}
