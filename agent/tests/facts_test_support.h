#pragma once
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/core/class_resolve.h"
#include "ue_wrap/core/object_index.h"
#include "coop/props/disc_record_ceiling.h"
// Keep the production row metadata in offline harnesses. Each harness still
// supplies its own resolution and registration behavior.
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* ident,const char* feature,
                   const wchar_t* cls,const wchar_t* fn,Mode mode)
    :ident_(ident),feature_(feature),cls_(cls),fn_(fn),mode_(mode),next_(nullptr){}
void Function::NoteUse(Use,bool)const{}
}
namespace ue_wrap::class_resolve {
const Family& AnyFamily() { static const Family family{L"test",nullptr,0,nullptr}; return family; }
Resolved ResolveClass(const wchar_t* name, const Family&, Load) {
    void* cls = ue_wrap::object_index::ClassByName(name);
    return {cls ? ClassState::Found : ClassState::Absent, cls};
}
}
namespace coop::disc_record { size_t MaxBodyBytes() { return kTransportBodyBytes; } }
