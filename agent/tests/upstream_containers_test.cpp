#include "coop/props/container_act_host.h"
#include "coop/props/container_act_state.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/actors/container_inventory.h"
#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/reflection.h"
#include <cstdlib>
#include <cstdarg>
#include <cstdio>

namespace { int failures=0; }
namespace ue_wrap::log {
void Write(Level level,const char* format,...) {
    if (level==Level::Error) ++failures;
    va_list args;va_start(args,format);std::vprintf(format,args);va_end(args);std::puts("");
}
}
namespace ue_wrap::field_io {
std::vector<std::wstring> ReadFStringArrayField(const void*,int32_t){std::abort();}
bool WriteFStringArrayField(void*,int32_t,const std::vector<std::wstring>&){std::abort();}
}
namespace ue_wrap::reflection {
int32_t FindPropertyOffset(void*,const wchar_t*){std::abort();}
void* FindClass(const wchar_t*){std::abort();}
void* FindClassDefaultObject(const wchar_t*){std::abort();}
bool IsDescendantOfAny(void*,void* const*,size_t,int32_t){std::abort();}
}
int main() {
    coop::props::container_act_host::RunSelftest();
    coop::props::container_act_state::State state;
    coop::props::container_act_state::Hold hold;
    hold.slot=31;hold.id=1;hold.eid=7;
    if (!state.Open(hold) || !state.Match(31,1)) return 1;
    if (state.Open(hold) || state.Match(30,1) || state.Match(31,2)) return 1;
    if (!state.Close(31,1) || state.Match(31,1)) return 1;
    return failures ? 1 : 0;
}

// Native class lookup is outside the pure selftests: they ask only an untyped plain record.
namespace ue_wrap::container_inventory {
bool IsContainerRecord(const save_record::SaveRecord& r){if(!r.className.empty())std::abort();return false;}
void ClearInventoryIndex(save_record::SaveRecord&){std::abort();}
}
