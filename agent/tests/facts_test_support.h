#pragma once
#include "ue_wrap/facts/function_row.h"
// Keep the production row metadata in offline harnesses. Each harness still
// supplies its own resolution and registration behavior.
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* ident,const char* feature,
                   const wchar_t* cls,const wchar_t* fn,Mode mode)
    :ident_(ident),feature_(feature),cls_(cls),fn_(fn),mode_(mode),next_(nullptr){}
void Function::NoteUse(Use,bool)const{}
}
