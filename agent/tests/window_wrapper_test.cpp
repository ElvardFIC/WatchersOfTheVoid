#include "../../src/votv-coop/src/ue_wrap/devices/base_window.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
enum Id{Actor,Base,NewBase,Child,Mesh,OtherMesh,Function,ChildFunction,Count};
struct Obj{int id=0,serial=1;bool live=true;float clean=.8f,alternate=.4f;ue_wrap::reflection::FName key{13,0};void* mesh=nullptr;} objects[Count];
uint32_t world=1;bool loaded=true,alternate=false,child=false,callOK=true;
int lookups=0,calls=0;void* selected=nullptr;void* called=nullptr;std::wstring missing;
std::function<void()> onResolve,onCall,onName;
void* At(Id id){return &objects[id];}
void Run(std::function<void()>& f){auto fn=std::move(f);f={};if(fn)fn();}
void Reset(){for(int i=0;i<Count;++i){objects[i]={};objects[i].id=i;}objects[Actor].mesh=At(Mesh);++world;
    loaded=callOK=true;alternate=child=false;lookups=calls=0;selected=At(Base);called=nullptr;missing.clear();
    onResolve=onCall=onName={};ue_wrap::base_window::g_layout={};}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p && static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return test::At(test::Id(i));}
bool IsLiveByIndex(void* p,int32_t i){return i>=0 && i<test::Count && p==ObjectAt(i) && IsLive(p);}
void* ClassOf(void* p){return p==test::At(test::Actor)?(test::child?test::At(test::Child):test::selected):p;}
bool IsDescendantOfAny(void* cls,void* const* bases,size_t n,int){return n==1 && bases[0]==test::selected && (cls==test::selected || cls==test::At(test::Child));}
void* FindClass(const wchar_t*){std::abort();}
int32_t FindPropertyOffset(void*,const wchar_t* name){++test::lookups;if(test::missing==name)return -1;
    if(!wcscmp(name,L"clean"))return test::alternate?offsetof(test::Obj,alternate):offsetof(test::Obj,clean);
    if(!wcscmp(name,L"Key"))return offsetof(test::Obj,key);CHECK(!wcscmp(name,L"StaticMesh1"));return offsetof(test::Obj,mesh);}
void* FindDispatchFunctionCached(void*,const wchar_t* name){CHECK(!wcscmp(name,L"setClean"));test::Run(test::onResolve);
    return test::missing==name?nullptr:test::At(test::child?test::ChildFunction:test::Function);}
std::wstring ToString(const FName& name){CHECK(name.ComparisonIndex==13);test::Run(test::onName);return L"window0";}
int32_t FunctionFrameSize(void*){return 0;}std::vector<ParamInfo> FunctionParams(void*){std::abort();}
bool CallFunction(void* actor,void* fn,void* raw){CHECK(actor==test::At(test::Actor) && !raw);++test::calls;test::called=fn;
    test::Run(test::onCall);return test::callOK;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* p){return p==test::At(test::Actor)?CurrentWorld():nullptr;}}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t* n){CHECK(!wcscmp(n,L"baseWindow_C"));return test::loaded?test::selected:nullptr;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
int main(){using namespace test;namespace W=ue_wrap::base_window;Reset();float clean=-9;
    loaded=false;for(int i=0;i<100;++i)CHECK(!W::ReadClean(At(Actor),clean));CHECK(!lookups && clean==-9);loaded=true;
    missing=L"clean";CHECK(!W::ReadClean(At(Actor),clean));missing.clear();CHECK(W::ReadClean(At(Actor),clean) && clean==.8f);
    CHECK(W::GetKeyString(At(Actor))==L"window0");const auto ready=lookups;CHECK(W::EnsureResolved() && lookups==ready);
    ++objects[Base].serial;alternate=true;CHECK(W::ReadClean(At(Actor),clean) && clean==.4f);
    objects[Base].live=false;selected=At(NewBase);CHECK(W::ReadClean(At(Actor),clean) && clean==.4f);
    Reset();missing=L"StaticMesh1";CHECK(!W::WriteCleanAndApply(At(Actor),.2f) && objects[Actor].clean==.8f);
    missing.clear();CHECK(W::WriteCleanAndApply(At(Actor),.2f) && calls==1 && objects[Actor].clean==.2f);
    Reset();missing=L"setClean";CHECK(!W::WriteCleanAndApply(At(Actor),.2f) && objects[Actor].clean==.8f);
    missing.clear();CHECK(W::WriteCleanAndApply(At(Actor),.2f));Reset();child=true;
    CHECK(W::WriteCleanAndApply(At(Actor),.2f) && called==At(ChildFunction));
    Reset();callOK=false;CHECK(!W::WriteCleanAndApply(At(Actor),.2f) && objects[Actor].clean==.2f);
    callOK=true;CHECK(W::WriteCleanAndApply(At(Actor),.2f) && calls==2);
    for(int cause=0;cause<7;++cause){Reset();bool current=true;
        onResolve=[&,cause]{if(cause==0)++world;if(cause==1)++objects[Actor].serial;if(cause==2)current=false;
            if(cause==3)++objects[Base].serial;if(cause==4)objects[Actor].mesh=At(OtherMesh);
            if(cause==5)++objects[Mesh].serial;if(cause==6)child=true;};
        CHECK(!W::WriteCleanAndApply(At(Actor),.2f,[&]{return current;}) && !calls && objects[Actor].clean==.8f);
    }
    for(int cause=0;cause<7;++cause){Reset();bool current=true;
        onCall=[&,cause]{if(cause==0)++world;if(cause==1)++objects[Actor].serial;if(cause==2)current=false;
            if(cause==3)++objects[Base].serial;if(cause==4)objects[Actor].mesh=At(OtherMesh);
            if(cause==5)++objects[Mesh].serial;if(cause==6)objects[Actor].clean=.1f;};
        CHECK(!W::WriteCleanAndApply(At(Actor),.2f,[&]{return current;}) && calls==1);
    }
    Reset();onName=[]{++objects[Actor].serial;};CHECK(W::GetKeyString(At(Actor)).empty());
    Reset();objects[Actor].mesh=nullptr;CHECK(!W::WriteCleanAndApply(At(Actor),.2f));
    CHECK(!W::WriteCleanAndApply(At(Actor),NAN));objects[Actor].clean=NAN;CHECK(!W::ReadClean(At(Actor),clean));
    puts("PASS actual window wrapper/ParamFrame: indexed class/serial reload, field/function/component readiness, native no-input dispatch, partial repaint retry and actor/class/world/component/current/value fences");
}
