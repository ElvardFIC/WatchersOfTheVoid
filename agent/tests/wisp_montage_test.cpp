#ifndef WISP_NATIVE_SOURCE
#define WISP_NATIVE_SOURCE "../../src/votv-coop/src/ue_wrap/actors/wisp.cpp"
#endif
#include WISP_NATIVE_SOURCE
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <limits>

namespace test {
struct Obj { int id, serial=1; bool live=true; };
alignas(void*) unsigned char wisp[4096]{}, mesh[4096]{};
alignas(void*) unsigned char player[4096]{}, movement[4096]{}, lag[4096]{}, playerMesh[4096]{};
Obj anim{0}, cls{1}, montage{2};
void* objects[]={&anim,&cls,&montage,wisp,mesh,player,movement,lag,playerMesh};
int serials[]={1,1,1,1,1,1,1,1,1};
bool readResult=true, playCall=true, refuseParam=false, recycleAnim=false, recycleWisp=false;
bool missingJump=false;
float result=2.f;
int plays=0,jumps=0;
int moves=0,attaches=0,detaches=0,visible=0;
uint32_t world=1;int spatialReads=0,traces=0;float targetX=100;
bool current=true,attachOK=true;
std::function<void()> onMove,onAttach,onDetach,onVisible;
std::function<void()> onPosition,onTrace;
void Callback(std::function<void()>& fn){auto once=std::move(fn);fn={};if(once)once();}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){for(auto* obj:test::objects)if(obj==p)return true;return false;}
int32_t InternalIndexOf(void* p){for(int i=0;i<std::size(test::objects);++i)if(test::objects[i]==p)return i;return -1;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<std::size(test::objects)&&test::objects[i]==p;}
int32_t SlotSerial(int32_t i){return i>=0&&i<std::size(test::objects)?test::serials[i]:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void*){return &test::cls;}
void* FindClass(const wchar_t*){return &test::cls;}
void* FindObject(const wchar_t*,const wchar_t*){return &test::montage;}
void* FindFunction(void*,const wchar_t* name){
 if(test::missingJump&&!wcscmp(name,L"Montage_JumpToSection"))return nullptr;
 return const_cast<wchar_t*>(name);
}
int32_t FindPropertyOffset(void*,const wchar_t*){return 32;}
bool FindBoolProperty(void*,const wchar_t*,int32_t&,uint8_t&){assert(false);return false;}
int32_t FindParamOffset(void*,const wchar_t*){return 0;}
bool IsDescendantOfAny(void*,void* const*,size_t,int){return true;}
const FName& NameOf(void*){static FName n{};return n;}
bool NameEquals(const FName&,const wchar_t*){return true;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* WorldOf(void*){return nullptr;}void* CurrentWorld(){return nullptr;}}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring&){return {};}}
namespace ue_wrap::engine {
bool AttachActorToComponentSocket(void* actor,void* parent,const wchar_t* socket){
 assert(actor==test::player&&parent==test::mesh&&!wcscmp(socket,L"playerGrab"));++test::attaches;test::Callback(test::onAttach);return test::attachOK;}
bool DetachActorFromParent(void* actor){assert(actor==test::player);++test::detaches;test::Callback(test::onDetach);return true;}
bool GetBoneWorldLocationByName(void*,const wchar_t*,FVector&){assert(false);return false;}
bool SetAnimTickAlways(void*){assert(false);return false;}
bool SetSceneComponentVisibility(void* comp,bool show,bool propagate){assert(comp==test::playerMesh&&show&&!propagate);++test::visible;test::Callback(test::onVisible);return true;}
bool TryGetActorLocation(void* actor,FVector& out){assert(actor==test::wisp||actor==test::player);++test::spatialReads;
 out={actor==test::wisp?0:test::targetX,0,0};test::Callback(test::onPosition);return true;}
}
namespace ue_wrap::trace {int LineBlockedStatDyn(void* actor,const FVector&,const FVector&){assert(actor==test::wisp);++test::traces;test::Callback(test::onTrace);return 0;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn),buf_(64){}
bool ParamFrame::SetRaw(const wchar_t*,const void*,int32_t){return !test::refuseParam;}
bool ParamFrame::GetRaw(const wchar_t* name,void* out,int32_t size)const{
 assert(!wcscmp(name,L"ReturnValue")&&size==sizeof(float));
 if(!test::readResult)return false;
 std::memcpy(out,&test::result,size);return true;
}
bool Call(void* obj,ParamFrame& f){
 const auto* name=static_cast<const wchar_t*>(f.function());
 if(!wcscmp(name,L"SetMovementMode")) {
  assert(obj==test::movement);++test::moves;test::Callback(test::onMove);return true;
 }
 assert(obj==&test::anim);
 if(!wcscmp(name,L"Montage_Play")){
  ++test::plays;
  if(test::recycleAnim)++test::serials[0];
  if(test::recycleWisp)++test::serials[3];
  return test::playCall;
 }
 assert(!wcscmp(name,L"Montage_JumpToSection"));++test::jumps;return true;
}
}
int main(){namespace W=ue_wrap::wisp;using namespace test;
 *reinterpret_cast<void**>(wisp+ue_wrap::profile::off::ACharacter_Mesh)=mesh;
 *reinterpret_cast<void**>(mesh+ue_wrap::profile::off::USkeletalMesh_AnimScriptInstance)=&anim;
 assert(W::PlayFatalityMontage(wisp)&&plays==1&&jumps==1);
 for(float refused:{0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
  result=refused;assert(!W::PlayFatalityMontage(wisp));assert(jumps==1);
 }
 result=2.f;readResult=false;assert(!W::PlayFatalityMontage(wisp));assert(jumps==1);
 readResult=true;playCall=false;assert(!W::PlayFatalityMontage(wisp));assert(jumps==1);
 playCall=true;refuseParam=true;const int before=plays;
 assert(!W::PlayFatalityMontage(wisp)&&plays==before);refuseParam=false;
 recycleAnim=true;assert(!W::PlayFatalityMontage(wisp)&&jumps==1);recycleAnim=false;
 recycleWisp=true;assert(!W::PlayFatalityMontage(wisp)&&jumps==1);recycleWisp=false;
 assert(W::PlayFatalityMontage(wisp)&&jumps==2);
 W::g_montageJumpFn=nullptr;missingJump=true;
 assert(W::PlayFatalityMontage(wisp)&&jumps==2); // Optional jump must not restart successful playback.
 puts("PASS actual wisp montage playback result, refused/malformed frames, callback recycling and optional section");
 recycleAnim=recycleWisp=refuseParam=false;playCall=readResult=true;result=2;
 *reinterpret_cast<void**>(player+ue_wrap::profile::off::ACharacter_CharacterMovement)=movement;
 *reinterpret_cast<void**>(player+ue_wrap::profile::off::ACharacter_Mesh)=playerMesh;
 *reinterpret_cast<void**>(player+16)=lag;
 W::g_grabStateResolve.store(1);W::g_lagOff=16;
 W::g_heldByte=8;W::g_heldMask=1;W::g_ctrlYawByte=9;W::g_ctrlYawMask=1;
 W::g_pawnCtrlByte=8;W::g_pawnCtrlMask=1;
 W::g_setMoveModeFn=const_cast<wchar_t*>(L"SetMovementMode");
 auto allowed=[] {return current;};
 auto clear=[] {player[8]=0;player[9]=1;lag[8]=1;current=true;};
 clear();assert(W::ApplyGrabToLocalPlayerWhile(wisp,player,allowed));
 assert(player[8]==1&&player[9]==0&&lag[8]==0);
 assert(W::ReleaseGrabOnLocalPlayerWhile(player,allowed));
 assert(player[8]==0&&player[9]==1&&lag[8]==1);
 for(int phase=0;phase<3;++phase) {
  clear();const auto beforeAttach=attaches,beforeVisible=visible;
  auto retire=[]{current=false;};
  if(phase==0)onMove=retire;if(phase==1)onAttach=retire;if(phase==2)onVisible=retire;
  assert(!W::ApplyGrabToLocalPlayerWhile(wisp,player,allowed));
  assert(player[8]==0&&player[9]==1&&lag[8]==1);
  assert(attaches==beforeAttach+(phase>0?1:0)&&visible==beforeVisible+(phase>1?1:0));
 }
 for(int phase=0;phase<2;++phase) {
  current=true;player[8]=1;player[9]=lag[8]=0;
  const auto beforeMove=moves;
  if(phase==0)onDetach=[]{current=false;};else onMove=[]{current=false;};
  assert(!W::ReleaseGrabOnLocalPlayerWhile(player,allowed));
  assert(player[8]==1&&player[9]==0&&lag[8]==0&&moves==beforeMove+phase);
 }
 clear();onMove=[]{++serials[5];};const auto beforeAttach=attaches;
 assert(!W::ApplyGrabToLocalPlayerWhile(wisp,player,allowed)&&attaches==beforeAttach);
 clear();refuseParam=true;
 assert(!W::ApplyGrabToLocalPlayerWhile(wisp,player,allowed)&&attaches==beforeAttach);
 puts("PASS actual wisp capture/release stops native work and flag writes after movement/attach/visibility/detach retirement, actor recycling and parameter refusal");
 refuseParam=false;
 assert(W::InGrabRange(wisp,player)&&W::DistanceTo(wisp,player)==100&&W::CanReach(wisp,player));
 for(int change=0;change<3;++change){
  const auto beforeReads=spatialReads;onPosition=[change]{if(change==0)++world;if(change==1)++serials[3];if(change==2)++serials[5];};
  assert(!W::InGrabRange(wisp,player)&&spatialReads==beforeReads+1);
 }
 onTrace=[]{++world;};assert(!W::CanReach(wisp,player));
 const auto beforeTraces=traces;targetX=std::numeric_limits<float>::infinity();
 assert(!W::CanReach(wisp,player)&&traces==beforeTraces);
 puts("PASS actual spatial gates pin both actors/world across native position reads and LOS callbacks, and refuse nonfinite trace inputs");
}
