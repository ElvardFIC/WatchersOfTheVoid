#ifndef GRAB_WRAPPER_SOURCE
#define GRAB_WRAPPER_SOURCE "../../src/votv-coop/src/ue_wrap/actors/prop_events.cpp"
#endif
#include GRAB_WRAPPER_SOURCE
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test {
struct Obj {int id=0,serial=1;bool live=true;void* cls=nullptr;} objects[10];
uint32_t world=1;
int baseCalls=0,releaseCalls=0,resolves=0,frameSize=0;
bool fnReady=true,classReady=true,callOk=true;
void* releaseFn=&objects[7];
std::function<void()> onFrame;
}
namespace ue_wrap::reflection {
const FName& NameOf(void*){static FName name{};return name;}
std::wstring ToString(const FName&){return L"test";}
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
void* ClassOf(void* p){return static_cast<test::Obj*>(p)->cls;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<10&&p==&test::objects[i]&&IsLive(p);}
int32_t SlotSerial(int32_t i){return i>=0&&i<10?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return i>=0&&i<10?&test::objects[i]:nullptr;}
void* FindClass(const wchar_t* n){return !wcscmp(n,L"prop_glowstick_C")?(test::classReady?&test::objects[3]:nullptr):&test::objects[2];}
bool IsDescendantOfAny(void* cls,void* const* bases,size_t n,int){
    CHECK(n==1&&bases[0]==&test::objects[3]);return cls==bases[0]||cls==&test::objects[4];
}
void* FindFunction(void* owner,const wchar_t* n){
    ++test::resolves;
    if(owner==&test::objects[3]){CHECK(!wcscmp(n,L"releaseGlowstick"));return test::fnReady?test::releaseFn:nullptr;}
    CHECK(owner==&test::objects[2]);return &test::objects[6];
}
int32_t FunctionFrameSize(void*){auto cb=std::move(test::onFrame);test::onFrame={};if(cb)cb();return test::frameSize;}
std::vector<ParamInfo> FunctionParams(void*){return {};}
bool CallFunction(void* actor,void* fn,void* data){
    CHECK(actor==&test::objects[1]&&data==nullptr);
    if(fn==&test::objects[6])++test::baseCalls;
    else {CHECK(fn==test::releaseFn);++test::releaseCalls;}
    return test::callOk;
}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* p){return p==&test::objects[1]?CurrentWorld():nullptr;}
}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t*){return &test::objects[3];}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
int main(){
    using namespace test;namespace P=ue_wrap::prop;using Result=P::GrabPreludeResult;
    auto reset=[] {
        for(int i=0;i<10;++i){objects[i].id=i;++objects[i].serial;objects[i].live=true;}
        objects[1].cls=&objects[4];++world;baseCalls=releaseCalls=resolves=frameSize=0;
        fnReady=classReady=callOk=true;releaseFn=&objects[7];onFrame={};
        P::g_grabPrelude={L"playerGrabbed_pre"};
        P::g_glowstickRelease={L"releaseGlowstick",{},{},false,L"prop_glowstick_C"};
    };
    auto run=[] {return P::ReplayRemoteGrabPrelude(&objects[1]);};
    reset();CHECK(run()==Result::Applied&&releaseCalls==1&&baseCalls==0);
    objects[1].cls=&objects[3];CHECK(run()==Result::Applied&&releaseCalls==2&&resolves==1);
    reset();objects[1].cls=&objects[5];CHECK(run()==Result::Applied&&baseCalls==1&&releaseCalls==0);
    reset();CHECK(P::ReplayRemoteGrabPrelude(nullptr)==Result::NotReady);
    objects[1].live=false;CHECK(run()==Result::NotReady&&resolves==0);
    reset();classReady=false;CHECK(run()==Result::NotReady&&resolves==0);
    classReady=true;CHECK(run()==Result::Applied&&releaseCalls==1);
    reset();fnReady=false;CHECK(run()==Result::NotReady&&resolves==1);
    CHECK(run()==Result::NotReady&&resolves==1); // loaded class is missing the body
    ++objects[3].serial;fnReady=true;CHECK(run()==Result::Applied&&resolves==2);
    reset();CHECK(run()==Result::Applied);++objects[7].serial;releaseFn=&objects[9];
    CHECK(run()==Result::Applied&&resolves==2&&releaseCalls==2); // class lives, function replaced
    reset();frameSize=-1;CHECK(run()==Result::NotReady&&releaseCalls==0);
    reset();callOk=false;CHECK(run()==Result::Failed&&releaseCalls==1);
    reset();onFrame=[] {++objects[1].serial;};CHECK(run()==Result::NotReady&&releaseCalls==0);
    reset();onFrame=[] {++world;};CHECK(run()==Result::NotReady&&releaseCalls==0);
    reset();onFrame=[] {++objects[7].serial;};CHECK(run()==Result::NotReady&&releaseCalls==0);
    puts("PASS actual grab wrapper/ParamFrame: exact glowstick owner and variants, ordinary base fallback, missing class/function/frame, dispatch refusal and actor/function/world retirement");
}

namespace ue_wrap::reflection { void* FindDispatchFunctionCached(void*,const wchar_t*){std::abort();} } // adjacent overlap readers are outside this grab fixture
