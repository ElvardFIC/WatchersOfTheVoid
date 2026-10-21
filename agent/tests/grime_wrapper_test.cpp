#include "../../src/votv-coop/src/ue_wrap/devices/grime.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
enum Id {Actor,Base,NewBase,Child,Material,OtherMaterial,BaseFunction,ChildFunction,Count};
struct Obj {int id=0,serial=1;bool live=true;float process=80,alternate=40;int32_t type=7;void* material=nullptr;ue_wrap::reflection::FName parameter{13,0};} objects[Count];
uint32_t world=1;bool loaded=true,alternate=false,child=false,callOK=true,scalarOK=true,makeMaterial=true;
int creates=0,scalars=0,lookups=0;float ratio=0;void* called=nullptr;std::wstring missing;
void* selected=nullptr;std::function<void()> onCreate,onScalar;
void* At(Id id){return &objects[id];}
void Callback(std::function<void()>& fn){auto callback=std::move(fn);fn={};if(callback)callback();}
void Reset(){for(int id=0;id<Count;++id){objects[id]={};objects[id].id=id;}++world;
    loaded=callOK=scalarOK=makeMaterial=true;alternate=child=false;creates=scalars=lookups=0;ratio=0;called=nullptr;missing.clear();
    selected=At(Base);onCreate=onScalar={};ue_wrap::grime::g_layout={};}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p && static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
int32_t SlotSerial(int32_t id){return test::objects[id].serial;}int32_t AllocateSlotSerial(int32_t id){return SlotSerial(id);}
void* ObjectAt(int32_t id){return test::At(test::Id(id));}
bool IsLiveByIndex(void* p,int32_t id){return p==ObjectAt(id) && IsLive(p);}
void* ClassOf(void* p){return p==test::At(test::Actor)?(test::child?test::At(test::Child):test::selected):p;}
bool IsDescendantOfAny(void* cls,void* const* bases,size_t n,int){return n==1 && bases[0]==test::selected && (cls==test::selected || cls==test::At(test::Child));}
void* FindClass(const wchar_t*){std::abort();}
int32_t FindPropertyOffset(void*,const wchar_t* name){++test::lookups;if(test::missing==name)return -1;
    if(!wcscmp(name,L"process"))return test::alternate?offsetof(test::Obj,alternate):offsetof(test::Obj,process);
    if(!wcscmp(name,L"Type"))return offsetof(test::Obj,type);if(!wcscmp(name,L"dynmat"))return offsetof(test::Obj,material);
    CHECK(!wcscmp(name,L"cleanParameter"));return offsetof(test::Obj,parameter);}
void* FindFunction(void*,const wchar_t* name){CHECK(!wcscmp(name,L"applyMaterial"));return test::missing==name?nullptr:test::At(test::BaseFunction);}
void* FindDispatchFunctionCached(void*,const wchar_t* name){CHECK(!wcscmp(name,L"applyMaterial"));return test::missing==name?nullptr:test::At(test::child?test::ChildFunction:test::BaseFunction);}
int32_t FunctionFrameSize(void*){return 0;}std::vector<ParamInfo> FunctionParams(void*){std::abort();}
bool CallFunction(void* object,void* fn,void* raw){CHECK(object==test::At(test::Actor) && !raw);++test::creates;test::called=fn;
    if(test::makeMaterial)test::objects[test::Actor].material=test::At(test::Material);test::Callback(test::onCreate);return test::callOK;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* p){return p==test::At(test::Actor)?CurrentWorld():nullptr;}
}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t* n){CHECK(!wcscmp(n,L"grime_C"));return test::loaded?test::selected:nullptr;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::engine {bool SetScalarParameterValue(void* material,const reflection::FName& parameter,float value){
    CHECK((material==test::At(test::Material)||material==test::At(test::OtherMaterial)) && parameter.ComparisonIndex==13);
    ++test::scalars;test::ratio=value;test::Callback(test::onScalar);return test::scalarOK;}}
int main(){using namespace test;namespace G=ue_wrap::grime;Reset();
    float process=-9;loaded=false;for(int i=0;i<100;++i)CHECK(!G::EnsureResolved());CHECK(!lookups);loaded=true;
    missing=L"process";CHECK(!G::ReadProcess(At(Actor),process) && process==-9);missing.clear();CHECK(G::ReadProcess(At(Actor),process) && process==80);
    const auto resolved=lookups;CHECK(G::EnsureResolved() && lookups==resolved);int32_t type=0;CHECK(G::ReadType(At(Actor),type) && type==7);
    ++objects[Base].serial;alternate=true;CHECK(G::ReadProcess(At(Actor),process) && process==40);
    objects[Base].live=false;selected=At(NewBase);CHECK(G::IsGrime(At(Actor)) && G::ReadProcess(At(Actor),process) && process==40);
    Reset();missing=L"dynmat";CHECK(!G::WriteProcessAndApply(At(Actor),20) && objects[Actor].process==80);
    missing.clear();CHECK(G::WriteProcessAndApply(At(Actor),20) && creates==1 && scalars==1 && ratio==.2f);
    CHECK(G::WriteProcessAndApply(At(Actor),10) && creates==1 && scalars==2 && ratio==.1f);
    Reset();child=true;CHECK(G::WriteProcessAndApply(At(Actor),25) && called==At(ChildFunction) && ratio==.25f);
    Reset();callOK=false;CHECK(!G::WriteProcessAndApply(At(Actor),25) && creates==1 && !scalars);
    callOK=true;CHECK(G::WriteProcessAndApply(At(Actor),25) && creates==1 && scalars==1); // partial setup is reused
    Reset();objects[Actor].material=At(Material);scalarOK=false;CHECK(!G::WriteProcessAndApply(At(Actor),25) && !creates);
    scalarOK=true;CHECK(G::WriteProcessAndApply(At(Actor),25) && !creates);
    for(int cause=0;cause<5;++cause){Reset();bool current=true;
        onCreate=[&,cause]{if(cause==0)++world;if(cause==1)++objects[Actor].serial;if(cause==2)current=false;
            if(cause==3)objects[Actor].process=5;if(cause==4)++objects[Base].serial;};
        CHECK(!G::WriteProcessAndApply(At(Actor),25,[&]{return current;}) && !scalars);
    }
    Reset();objects[Actor].material=At(Material);onScalar=[]{objects[Actor].material=At(OtherMaterial);};
    CHECK(!G::WriteProcessAndApply(At(Actor),25) && !creates);
    Reset();objects[Actor].live=false;CHECK(!G::ReadProcess(At(Actor),process));
    CHECK(G::ReadEndedProcess(At(Actor),Actor,objects[Actor].serial,process) && process==80);
    CHECK(!G::ReadEndedProcess(At(Actor),Actor,objects[Actor].serial+1,process));
    Reset();objects[Actor].process=NAN;CHECK(!G::ReadProcess(At(Actor),process));CHECK(!G::WriteProcessAndApply(At(Actor),NAN));
    puts("PASS grime wrapper/actual ParamFrame: indexed missing-class lookup, reflected-field retry, class serial/reload, native ratio and subclass setup, partial material reuse, ended-slot reads and current/actor/class/world/material fences");
}
