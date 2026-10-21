#define main PriorGateScopeMain
#include "gate_scope_2160_test.cpp"
#undef main
#include <functional>
#include <vector>
namespace gate_test {
using namespace ue_wrap::script_gate;
std::function<Verdict(const Call&)> before;
std::function<void(const Call&)> after;
std::function<void()> body;
std::vector<uint64_t> ids;
int bodies=0,posts=0;
Verdict Pre(const Call& call){CHECK(call.invocation && IsInvocationActive(call.invocation));ids.push_back(call.invocation);
    return before?before(call):Verdict::Run;}
void Post(const Call& call){CHECK(IsInvocationActive(call.invocation));++posts;if(after)after(call);}
uintptr_t __fastcall Original(void*,void*,void*){
    ++bodies;const auto active=CurrentThreadCall();CHECK(active.active && IsObjectBodyActive(active.function,active.object));
    CHECK(t_invocationHead && IsInvocationActive(t_invocationHead->id));if(body)body();return 37;
}
void Frame(uint8_t* frame,void* function,void* object){
    std::memcpy(frame+P::off::FFrame_Node,&function,sizeof(function));std::memcpy(frame+P::off::FFrame_Object,&object,sizeof(object));
}
void Bind(void* function){const auto key=reinterpret_cast<uintptr_t>(function);auto slot=detail::SlotOf(key);
    while(detail::g_fnTable[slot].key.load())slot=(slot+1)&(detail::kSlots-1);
    auto& row=detail::g_fnTable[slot];row.pre=&Pre;row.post=&Post;row.tag=1;row.enabled.store(true);row.resolved.store(true);row.key.store(key);
}
int FaultBoundary(void* frame){
    __try {LoopDetour(nullptr,frame,nullptr);return 0;}
    __except(GetExceptionCode()==EXCEPTION_ACCESS_VIOLATION?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return 1;}
}
}
int main(){using namespace gate_test;CHECK(PriorGateScopeMain()==0);
    alignas(64) uint8_t functions[128]{};int actor=0,other=0;uint8_t outer[256]{},inner[256]{};
    Frame(outer,functions,&actor);Frame(inner,functions+64,&other);Bind(functions);Bind(functions+64);
    g_enabled.store(true);detail::g_fnWatches.store(2);g_trampoline=&Original;
    uint64_t outerId=0,innerId=0;
    before=[&](const Call& call){if(call.object==&actor){outerId=call.invocation;CHECK(!IsObjectBodyActive(functions,&actor));
        CHECK(LoopDetour(nullptr,inner,nullptr)==37);CHECK(IsInvocationActive(outerId) && !IsInvocationActive(innerId));}
        else{innerId=call.invocation;CHECK(innerId!=outerId && IsInvocationActive(outerId));}return Verdict::Run;};
    CHECK(LoopDetour(nullptr,outer,nullptr)==37 && bodies==2 && posts==2);
    CHECK(!IsInvocationActive(outerId) && !IsInvocationActive(innerId));
    before={};after=[&](const Call& call){if(call.object==&actor){outerId=call.invocation;
        CHECK(!IsObjectBodyActive(functions,&actor));CHECK(LoopDetour(nullptr,inner,nullptr)==37 && IsInvocationActive(outerId));}};
    CHECK(LoopDetour(nullptr,outer,nullptr)==37 && bodies==4 && posts==4);CHECK(!IsInvocationActive(outerId));after={};
    before=[](const Call&){return Verdict::Cancel;};CHECK(LoopDetour(nullptr,outer,nullptr)==0 && bodies==4 && posts==4);
    const auto cancelled=ids.back();CHECK(!IsInvocationActive(cancelled));before={};
    body=[]{throw 7;};try{LoopDetour(nullptr,outer,nullptr);CHECK(false);}catch(int value){CHECK(value==7);}
    CHECK(!IsInvocationActive(ids.back()) && !CurrentThreadCall().active);body={};
    body=[]{RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);};
    CHECK(FaultBoundary(outer)==1);
    CHECK(!IsInvocationActive(ids.back()) && !CurrentThreadCall().active);body={};
    before=[&](const Call& call){if(call.object==&actor){outerId=call.invocation;
        body=[]{RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);};
        CHECK(FaultBoundary(inner)==1);body={};
        CHECK(IsInvocationActive(outerId) && !IsInvocationActive(ids.back()));}return Verdict::Run;};
    CHECK(LoopDetour(nullptr,outer,nullptr)==37 && !IsInvocationActive(outerId));before={};
    CHECK(LoopDetour(nullptr,outer,nullptr)==37 && ids.back()!=cancelled && !IsInvocationActive(ids.back()));
    for(size_t i=1;i<ids.size();++i)CHECK(ids[i]>ids[i-1]);CHECK(!IsInvocationActive(0));
    puts("PASS actual gate dispatcher invocation lifetime: unique IDs through nested PRE/POST, unchanged body scopes, cancellation, reused frames, C++ and native SEH unwind");
}
