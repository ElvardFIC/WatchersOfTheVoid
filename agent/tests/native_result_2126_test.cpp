// A guard page after a one-byte return catches the real thunk's old 8-byte read.
#include "../../src/votv-coop/src/ue_wrap/core/ufunction_hook.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1); } } while(0)
namespace ue_wrap::game_thread { bool IsGameThread() { return true; } }
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace ue_wrap::reflection { std::vector<ParamInfo> FunctionParams(void*) { return {}; } }
namespace ue_wrap::script_gate { const ExecHandler* ExecHandlers() { return nullptr; } }
using namespace ue_wrap::ufunction_hook;
namespace {
int calls=0;void* expected=nullptr;bool changeBool=false;
void __fastcall Original(void*,void*,void*) {}
void Observe(void*,void*,void* value) {
    ++calls;CHECK(value==expected);
    if (changeBool) { CHECK(CurrentResult());*static_cast<bool*>(CurrentResult())=true; }
}
}
int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    SYSTEM_INFO info{};GetSystemInfo(&info);
    auto* pages=static_cast<uint8_t*>(VirtualAlloc(nullptr,info.dwPageSize*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));CHECK(pages);
    DWORD old=0;CHECK(VirtualProtect(pages+info.dwPageSize,info.dwPageSize,PAGE_NOACCESS,&old));
    auto& slot=g_slots[0];slot.original=&Original;slot.cb=&Observe;slot.armed.store(true);
    slot.resultBytes=1;changeBool=true;auto* last=pages+info.dwPageSize-1;*last=0;
    NativeThunk<0>(nullptr,nullptr,last);CHECK(*last==1 && calls==1 && !CurrentResult());
    changeBool=false;slot.resultBytes=0;
    NativeThunk<0>(nullptr,nullptr,pages+info.dwPageSize);CHECK(calls==2);
    expected=pages;void* value=pages;slot.resultBytes=sizeof(void*);
    NativeThunk<0>(nullptr,nullptr,&value);CHECK(calls==3);
    VirtualFree(pages,0,MEM_RELEASE);
    std::puts("PASS: actual native thunk bool guard page, void return, pointer return, callback override");
}
