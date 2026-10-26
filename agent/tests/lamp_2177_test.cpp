#define UE_LOGE(...) ((void)0)
#include "../../src/votv-coop/src/ue_wrap/devices/lightswitch.cpp"
#include "../../src/votv-coop/src/coop/interactables/lightgroup_verbs.cpp"
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include "facts_test_support.h"
namespace ue_wrap::facts {Resolved Resolve(const Function&){return {};}}
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <functional>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
namespace test {
struct Obj { int id; int serial=1; bool live=true; uint8_t flags=0xA0; void* bottom=nullptr; }
    objects[7]{{0},{1},{2},{3},{4},{5},{6}};
bool loaded=true, indexed=true, announced=true, field=true, param=true, nativeValue=false;
bool nativeLogicalGroup=true;
bool watchAccepted=false;
int groupWatchTries=0;
uint8_t mask=4;
std::vector<bool> edges;
std::vector<bool> ambientEdges;
bool topEnabled=false, topVisible=false, hidden=true;
ue_wrap::script_gate::PreFn pre=nullptr;
ue_wrap::script_gate::PostFn post=nullptr;
ue_wrap::script_gate::PreFn ambientPre=nullptr;
ue_wrap::script_gate::PostFn ambientPost=nullptr;
std::function<void()> callback;
}
namespace ue_wrap::reflection {
int32_t FindParamOffset(void*,const wchar_t*) { std::abort(); }
int32_t FunctionFrameSize(void*) {return 0;}
int32_t FindPropertyOffset(void* cls,const wchar_t* n) {
    CHECK(cls==&test::objects[3] && std::wcscmp(n,L"bottomLight")==0);
    return offsetof(test::Obj,bottom);
}
void* FindClass(const wchar_t*) { std::abort(); }
void* FindFunction(void*,const wchar_t*) { std::abort(); }
std::wstring ToString(const FName&) { std::abort(); }
void* ClassOf(void* p) {
    if(p==&test::objects[4])return &test::objects[3];
    if(p==&test::objects[6])return &test::objects[5];
    return &test::objects[0];
}
bool IsDescendantOfAny(void* c,void* const* bases,size_t,int) { return c==bases[0]; }
bool IsLive(void* p) { return p && static_cast<test::Obj*>(p)->live; }
int32_t InternalIndexOf(void* p) { return static_cast<test::Obj*>(p)->id; }
int32_t SlotSerial(int32_t i) { return test::objects[i].serial; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
bool IsLiveByIndex(void* p,int32_t i) { return p==&test::objects[i] && test::objects[i].live; }
bool FindBoolProperty(void* cls,const wchar_t* n,int32_t& off,uint8_t& mask) {
    if(cls==&test::objects[5]) {
        CHECK(std::wcscmp(n,L"bVisible")==0);off=offsetof(test::Obj,flags);mask=0x10;return test::field;
    }
    CHECK(std::wcscmp(n,L"isActive")==0);off=offsetof(test::Obj,flags);mask=test::mask;return test::field;
}
void* FindDispatchFunctionCached(void*,const wchar_t* n) { CHECK(std::wcscmp(n,L"setActive")==0);return &test::objects[2]; }
}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t* n) {
    return test::loaded?&test::objects[std::wcscmp(n,L"ambientLight_C")==0?3:0]:nullptr;
}}
namespace ue_wrap::ufunction_hook {
void* CurrentNativeFunction(){return nullptr;}
bool InstallPreHook(const facts::Resolved&,PreNativeCallback,bool) { std::abort(); }
}
namespace ue_wrap::engine {
void* SpawnSoundAttenuation(const SoundAttenuationConfig&) { std::abort(); }
void PlaySoundAtLocation(void*,void*,const FVector&,void*,float,float) { std::abort(); }
bool TryGetActorLocation(void*,FVector&) { std::abort(); }
}
namespace ue_wrap::world_identity {uint32_t Generation(){return 1;}void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap {
ParamFrame::ParamFrame(void* f):fn_(f){}
bool ParamFrame::SetRaw(const wchar_t* n,const void* data,int32_t size) {
    CHECK((std::wcscmp(n,L"isActive")==0 || std::wcscmp(n,L"active")==0) && size==1);
    test::nativeValue=*static_cast<const bool*>(data);return test::param;
}
bool Call(void* actor,ParamFrame&) {
    if(actor==&test::objects[4]) {
        auto& flags=test::objects[6].flags;
        flags=(flags&~0x10)|(test::nativeValue?0x10:0);
        test::topVisible=test::nativeValue&&test::topEnabled;
        auto cb=std::move(test::callback);test::callback={};if(cb)cb();return true;
    }
    auto& flags=static_cast<test::Obj*>(actor)->flags;
    flags=(flags&~test::mask)|(test::nativeValue?test::mask:0);
    auto cb=std::move(test::callback);test::callback={};if(cb)cb();return true;
}
}
namespace coop::net_pump {bool IsInAnnouncedWorld(void*) {return test::announced;}}
namespace coop::interactable_sync {
std::wstring AmbientLightKey(void*) {return test::indexed?L"ambient":L"";}
void OnAmbientLightVerb(void* actor) {bool on=false;CHECK(ue_wrap::lightswitch::TryReadAmbientActive(actor,on));test::ambientEdges.push_back(on);}
std::wstring CeilingLampKey(void*) {return test::indexed?L"lamp":L"";}
void OnCeilingLampVerb(void* actor) {bool on=false;CHECK(ue_wrap::lightswitch::TryReadLampActive(actor,on));test::edges.push_back(on);}
std::wstring LightGroupKey(void*) {return {};}
bool ApplyingLightGroup(void*) {return false;}
void OnLightGroupVerb(void*) {}
}
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t* c,const wchar_t* n,int,PreFn pre,PostFn post) {
    if(std::wcscmp(c,L"ambientLight_C")==0) {
        CHECK(std::wcscmp(n,L"setActive")==0);test::ambientPre=pre;test::ambientPost=post;return true;
    }
    CHECK(std::wcscmp(c,L"ceilingLamp_C")==0 && std::wcscmp(n,L"setActive")==0);
    test::pre=pre;test::post=post;return true;
}
bool WatchName(const facts::Function& row,int tag,PreFn pre,PostFn post) {
    if(row.Class())return WatchClassName(row.Class(),row.Name(),tag,pre,post);
    ++test::groupWatchTries;return test::watchAccepted;
}
void ResolvePendingNames(){} bool NameWatchLive(const facts::Function&,int){return true;}
int PendingNameCount(){return 0;}
}
int main() {
    using namespace test;namespace L=ue_wrap::lightswitch;namespace V=coop::lightgroup_verbs;
    using ue_wrap::script_gate::Verdict;
    coop::net::Session session;session.mode=coop::net::Role::Host;V::Install(&session);
    CHECK(groupWatchTries==1 && V::g_reg==V::Reg::Refused);
    V::Tick();CHECK(!V::g_settled); // a refusal must not settle readiness forever
    V::Install(&session);CHECK(groupWatchTries==1); // bounded retry cadence
    watchAccepted=true;V::g_nextWatchTry={};V::Install(&session);V::Tick();
    CHECK(groupWatchTries==2 && V::g_settled && V::g_reg==V::Reg::Registered);
    CHECK(pre && post); ue_wrap::script_gate::Call call{};call.object=&objects[1];
    // Rapid actual lamp transitions with an unchanged persistent group flag.
    for(bool on:{true,false,true,false,true}) {
        CHECK(pre(call)==Verdict::Run);CHECK(L::ApplyLampActive(call.object,on));post(call);
        CHECK(edges.back()==on && nativeLogicalGroup && (objects[1].flags&0xA0)==0xA0);
    }
    CHECK(edges.size()==5);
    session.mode=coop::net::Role::Client;
    CHECK(pre(call)==Verdict::Cancel);post(call);CHECK(edges.size()==5);
    session.active=false;CHECK(pre(call)==Verdict::Run);
    session.mode=coop::net::Role::Host;post(call);CHECK(edges.size()==5);
    session.active=true;session.mode=coop::net::Role::Client;
    call.fromOurCode=true;CHECK(pre(call)==Verdict::Run);CHECK(L::ApplyLampActive(call.object,false));
    call.fromOurCode=false;announced=false;CHECK(pre(call)==Verdict::Run);announced=true;
    indexed=false;CHECK(pre(call)==Verdict::Run);indexed=true;
    V::OnDisconnect();CHECK(pre(call)==Verdict::Run);
    bool on=true;CHECK(L::TryReadLampActive(call.object,on)&&!on);
    param=false;CHECK(!L::ApplyLampActive(call.object,true));param=true;
    ++objects[0].serial;mask=0;CHECK(!L::TryReadLampActive(call.object,on));
    mask=8;field=false;CHECK(!L::TryReadLampActive(call.object,on));field=true;
    CHECK(L::ApplyLampActive(call.object,true));CHECK(objects[1].flags==0xA8);
    callback=[]{++objects[1].serial;};CHECK(!L::ApplyLampActive(call.object,false));
    using namespace coop::net;
    CHECK(LaneForKind(ReliableKind::CeilingLampState)==LaneForKind(ReliableKind::LightGroupState));
    CHECK(!IsClientRelayableReliableKind(ReliableKind::CeilingLampState));
    CHECK(!IsPreWorldSendableKind(ReliableKind::CeilingLampState));
    // Ambient has no isActive: its bottom component is the authoritative phase,
    // even when the user's ambient setting hides the actor or disables its top light.
    session.mode=Role::Host;V::Install(&session);CHECK(ambientPre&&ambientPost);
    call.object=&objects[4];objects[4].bottom=&objects[6];
    CHECK(!L::IsCeilingLamp(call.object) && !L::IsAmbientLight(&objects[1]));
    for(bool active:{true,false,true}) {
        CHECK(ambientPre(call)==Verdict::Run);CHECK(L::ApplyAmbientActive(call.object,active));ambientPost(call);
        CHECK(ambientEdges.back()==active && hidden && !topVisible && (objects[6].flags&0xA0)==0xA0);
    }
    topEnabled=true;CHECK(L::ApplyAmbientActive(call.object,true)&&topVisible);
    CHECK(L::ApplyAmbientActive(call.object,false)&&!topVisible);
    session.mode=Role::Client;CHECK(ambientPre(call)==Verdict::Cancel);
    ambientPost(call);CHECK(ambientEdges.size()==3);
    session.active=false;CHECK(ambientPre(call)==Verdict::Run);
    session.mode=Role::Host;ambientPost(call);CHECK(ambientEdges.size()==3);
    session.active=true;session.mode=Role::Client;
    call.fromOurCode=true;CHECK(ambientPre(call)==Verdict::Run);call.fromOurCode=false;
    indexed=false;CHECK(ambientPre(call)==Verdict::Run);indexed=true;
    announced=false;CHECK(ambientPre(call)==Verdict::Run);announced=true;
    objects[4].bottom=nullptr;CHECK(!L::TryReadAmbientActive(call.object,on));
    CHECK(!L::ApplyAmbientActive(call.object,true));objects[4].bottom=&objects[6];
    objects[6].live=false;CHECK(!L::ApplyAmbientActive(call.object,true));objects[6].live=true;
    ++objects[5].serial;field=false;CHECK(!L::TryReadAmbientActive(call.object,on));field=true;
    CHECK(L::TryReadAmbientActive(call.object,on)&&!on);
    param=false;CHECK(!L::ApplyAmbientActive(call.object,true));param=true;
    callback=[]{++objects[4].serial;};CHECK(!L::ApplyAmbientActive(call.object,true));
    V::OnDisconnect();CHECK(ambientPre(call)==Verdict::Run);
    CHECK(LaneForKind(ReliableKind::AmbientLightState)==LaneForKind(ReliableKind::LightGroupState));
    CHECK(!IsClientRelayableReliableKind(ReliableKind::AmbientLightState));
    CHECK(!IsPreWorldSendableKind(ReliableKind::AmbientLightState));
    std::puts("PASS ambient component visibility, native top-light behavior, guards and lifetime checks");
    std::puts("PASS actual lamp phases, host capture/client guard, native apply, bool mask and class/actor lifetime");
    std::puts("PASS delayed light-group registration, bounded recovery and stopped-session light guards");
}
