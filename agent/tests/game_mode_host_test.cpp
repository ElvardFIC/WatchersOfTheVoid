#include "ui/host_game_mode.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/core/sdk_profile.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <functional>

#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
#define UE_LOGW(...) ((void)0)
#define UE_LOGI(...) ((void)0)
namespace test {
struct Obj {int id,serial=1;bool live=true;void* cls=nullptr;ue_wrap::reflection::FName map{};int mode=0;};
Obj objects[]{{0},{1}};
bool field=true,names=true,writable=true,prefixReady=true;int named=0,derived=0;
std::function<void()> onName;
void Reset(){for(auto& o:objects){++o.serial;o.live=true;o.cls=&objects[1];o.map={};o.mode=0;}
 field=names=writable=prefixReady=true;named=0;derived=0;onName={};}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p && static_cast<test::Obj*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0 && i<2 && p==&test::objects[i] && IsLive(p);}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
int32_t SlotSerial(int32_t i){return i>=0 && i<2?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void* p){return static_cast<test::Obj*>(p)->cls;}
int32_t FindPropertyOffset(void*,const wchar_t* name){CHECK(!std::wcscmp(name,L"mainMap"));return test::field?offsetof(test::Obj,map):-1;}
}
namespace ue_wrap::world_identity {void* WorldOf(void*){return nullptr;}void* CurrentWorld(){return nullptr;}}
namespace ue_wrap::fname_utils {
reflection::FName StringToFName(const std::wstring& value){++test::named;
 auto cb=std::move(test::onName);test::onName={};if(cb)cb();
 if(!test::names)return {};
 CHECK(value==L"tutorial3" || value==ue_wrap::profile::name::GameplayLevel);
 return {value==L"tutorial3"?30:10,0};}
}
namespace ue_wrap::game_mode {
#include "mode_level.inc"
int WriteTo(void* gi,int mode){if(!test::writable || !IsValid(mode))return -1;
 auto& value=static_cast<test::Obj*>(gi)->mode;int old=value;value=mode;return old;}
int ReadFrom(void* gi){return static_cast<test::Obj*>(gi)->mode;}
}
namespace ue_wrap::save_browser {
namespace R=reflection;
#include "mode_save_map.inc"
}
namespace ue_wrap::engine {
bool g_gameModeApplied=false;
bool ResolveSavePrefixFn(){return test::prefixReady;}
int DeriveModeFromSlot(const wchar_t*){return test::derived;}
#include "mode_apply.inc"
}
int main(){namespace H=ui::host_game_mode;namespace G=ue_wrap::game_mode;namespace E=ue_wrap::engine;
 using namespace test;
 CHECK(H::Value()==G::kStory);
 const int ordinals[]{2,0,1,6,4,7};
 for(size_t i=0;i<H::kChoices.size();++i){const auto& choice=H::kChoices[i];
  CHECK(choice.mode==ordinals[i] && H::IndexOf(choice.mode)==int(i));
  CHECK(H::Select(choice.mode) && H::Value()==choice.mode && !std::wcscmp(H::Label(choice.mode),choice.wideLabel));
  Reset();CHECK(ue_wrap::save_browser::InitializeNewGameMap(&objects[0],choice.mode));
  CHECK(objects[0].map.ComparisonIndex==(choice.mode==G::kTutorial?30:10));
  E::g_gameModeApplied=false;E::ApplyGameModeFromSlot(&objects[0],L"zcoop_peer",choice.mode);
  CHECK(E::g_gameModeApplied && objects[0].mode==choice.mode);
  Reset();E::g_gameModeApplied=false;writable=false;E::ApplyGameModeFromSlot(&objects[0],L"new",choice.mode);
  CHECK(!E::g_gameModeApplied);writable=true;E::ApplyGameModeFromSlot(&objects[0],L"new",choice.mode);
  CHECK(E::g_gameModeApplied && objects[0].mode==choice.mode);
 }
 const auto prior=H::Value();for(int mode:{-1,3,5,8,255})CHECK(!H::Select(mode) && H::Value()==prior);
 for(int refusal=0;refusal<5;++refusal){Reset();
  if(refusal==0)field=false;if(refusal==1)names=false;
  if(refusal==2)onName=[]{++objects[0].serial;};
  if(refusal==3)onName=[]{++objects[1].serial;};
  if(refusal==4)onName=[]{objects[0].cls=nullptr;};
  CHECK(!ue_wrap::save_browser::InitializeNewGameMap(&objects[0],G::kTutorial));
  CHECK(objects[0].map.ComparisonIndex==0);
 }
 Reset();CHECK(!ue_wrap::save_browser::InitializeNewGameMap(&objects[0],255) && !named);
 E::g_gameModeApplied=false;prefixReady=false;E::ApplyGameModeFromSlot(&objects[0],L"a_existing");CHECK(!E::g_gameModeApplied);
 prefixReady=true;derived=G::kAmbient;E::ApplyGameModeFromSlot(&objects[0],L"a_existing");
 CHECK(E::g_gameModeApplied && objects[0].mode==G::kAmbient);
 std::puts("PASS six host modes: cooked ordinals, shared selection, save map identity, mode readback/retry and existing-save ownership");
}
