#pragma once
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/engine/world_identity.h"
#include "coop/session/net_pump.h"
#include "coop/props/active_drive.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int id=1,serial=1;bool live=true;uint8_t flags=0;void* clipboard=nullptr;};
inline Obj actor{}, ui{2};inline uint32_t world=1;inline uint64_t now=1000;
inline bool announced=true,fields=true;inline std::function<void()> callback;
inline bool worldUnavailable=false;
inline void Callback(){auto f=std::move(callback);callback={};if(f)f();}
}
namespace ue_wrap::reflection {
inline int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
#ifndef TEST_CUSTOM_ACTION_REFS
inline int32_t SlotSerial(int32_t i){return i==1?test::actor.serial:test::ui.serial;}
#endif
inline int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
inline bool IsLiveByIndex(void* a,int32_t){return a&&static_cast<test::Obj*>(a)->live;}
inline bool IsLive(void* a){return IsLiveByIndex(a,0);}
#ifndef TEST_CUSTOM_ACTION_REFS
inline void* ClassOf(void* a){return a;}
#endif
inline int32_t FindPropertyOffset(void*,const wchar_t*){return offsetof(test::Obj,clipboard);}
inline bool FindBoolProperty(void*,const wchar_t* n,int32_t& off,uint8_t& mask){off=offsetof(test::Obj,flags);mask=std::wstring(n)==L"skipTextSave"?2:1;return test::fields;}
inline int32_t ParamOffset::Of(void*){return std::wstring(param_)==L"action"?0:8;}
}
#ifndef TEST_CUSTOM_WORLD_REFS
namespace ue_wrap::world_identity {
inline uint32_t Generation(){return test::world;}
inline void* CurrentWorld(){return test::worldUnavailable?nullptr:reinterpret_cast<void*>(uintptr_t(test::world));}
inline void* WorldOf(void*){return CurrentWorld();}
}
#endif
namespace coop::active_drive {inline uint64_t NowMs(){return test::now;}}
namespace ue_wrap::log {inline void Write(Level,const char*,...) {}}
namespace coop::net_pump {inline bool IsInAnnouncedWorld(void*){return test::announced;}}
namespace ue_wrap::script_gate {
inline std::wstring lastWatchClass, lastWatchFunction;
inline std::vector<std::pair<std::wstring,std::wstring>> watchedBodies;
inline bool WatchClassName(const wchar_t* cls,const wchar_t* fn,int,PreFn,PostFn){lastWatchClass=cls;lastWatchFunction=fn;watchedBodies.emplace_back(cls,fn);return true;}
inline bool watchLive=true;
inline bool ClassNameWatchLive(const wchar_t*,const wchar_t*,int){return watchLive;}
inline void ResolvePendingNames(){}
}
