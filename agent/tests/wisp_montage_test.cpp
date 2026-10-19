#include "../../src/votv-coop/src/ue_wrap/actors/wisp.cpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <limits>

namespace test {
struct Obj { int id, serial=1; bool live=true; };
alignas(void*) unsigned char wisp[4096]{}, mesh[4096]{};
Obj anim{0}, cls{1}, montage{2};
void* objects[]={&anim,&cls,&montage,wisp,mesh};
int serials[]={1,1,1,1,1};
bool readResult=true, playCall=true, refuseParam=false, recycleAnim=false, recycleWisp=false;
bool missingJump=false;
float result=2.f;
int plays=0,jumps=0;
}
namespace ue_wrap::reflection {
bool IsLive(void* p){for(auto* obj:test::objects)if(obj==p)return true;return false;}
int32_t InternalIndexOf(void* p){for(int i=0;i<5;++i)if(test::objects[i]==p)return i;return -1;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<5&&test::objects[i]==p;}
int32_t SlotSerial(int32_t i){return i>=0&&i<5?test::serials[i]:0;}
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
namespace ue_wrap::world_identity {void* WorldOf(void*){return nullptr;}void* CurrentWorld(){return nullptr;}}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring&){return {};}}
namespace ue_wrap::engine {
bool AttachActorToComponentSocket(void*,void*,const wchar_t*){assert(false);return false;}
bool DetachActorFromParent(void*){assert(false);return false;}
bool GetBoneWorldLocationByName(void*,const wchar_t*,FVector&){assert(false);return false;}
bool SetAnimTickAlways(void*){assert(false);return false;}
bool SetSceneComponentVisibility(void*,bool,bool){assert(false);return false;}
bool TryGetActorLocation(void*,FVector&){assert(false);return false;}
}
namespace ue_wrap::trace {int LineBlockedStatDyn(void*,const FVector&,const FVector&){assert(false);return -1;}}
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
 assert(obj==&test::anim);
 const auto* name=static_cast<const wchar_t*>(f.function());
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
}
