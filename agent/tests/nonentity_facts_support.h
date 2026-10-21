#pragma once
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/game_thread.h"
#include "coop/player/roster_ledger.h"
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* ident,const char* feature,const wchar_t* cls,const wchar_t* fn,Mode mode)
    : ident_(ident),feature_(feature),cls_(cls),fn_(fn),mode_(mode) {}
Resolved Resolve(const Function&) { return {}; }
}
namespace coop::roster_ledger { void RegisterPerSlotClear(void (*)(void*,int),void*) {} }
namespace ue_wrap::game_thread { bool RegisterInterceptor(const facts::Resolved&,UFunctionInterceptor) { return true; } }
namespace ue_wrap::script_gate { bool WatchName(const facts::Function&,int,PreFn,PostFn) { return true; } }
