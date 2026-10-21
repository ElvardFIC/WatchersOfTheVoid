#include "../../src/votv-coop/src/ue_wrap/devices/kitchen_machine.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
enum Id {Micro,Vendor,Audio,Light,MicroClass,CoffeeClass,VendorClass,OvenClass,GrillClass,CrematorClass,Count};
struct Obj {int id=0,serial=1;bool live=true,playing=false;uint8_t flags=0;
    int32_t mode=0;float remaining=0,timer=0,power=0;void* door=nullptr;void* light=nullptr;void* audio=nullptr;} objects[Count];
uint32_t world=1;bool fields=true,audioReady=true;int starts=0,stops=0,warnings=0;
std::function<void()> callback;
void* At(Id id){return &objects[id];}
}
namespace ue_wrap::reflection {
bool IsLive(void* object){return object && static_cast<test::Obj*>(object)->live;}
int32_t InternalIndexOf(void* object){return object?static_cast<test::Obj*>(object)->id:-1;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* object,int32_t i){return i>=0 && i<test::Count && object==test::At(test::Id(i)) && IsLive(object);}
void* ClassOf(void* object){if(object==test::At(test::Micro))return test::At(test::MicroClass);if(object==test::At(test::Vendor))return test::At(test::VendorClass);return object;}
bool IsDescendantOfAny(void* cls,void* const* bases,size_t count,int){return count==1 && cls==bases[0];}
std::wstring ClassNameOf(void*){return L"kitchen_test";}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& off,uint8_t& mask) {
    if(!test::fields)return false;off=offsetof(test::Obj,flags);
    if(!wcscmp(name,L"active"))mask=1;else if(!wcscmp(name,L"powered"))mask=2;
    else if(!wcscmp(name,L"opened"))mask=4;else {CHECK(!wcscmp(name,L"processing"));mask=64;}return true;
}
int32_t InstanceOffset::Of(void*) {
    if(!test::fields)return -1;
    if(!wcscmp(prop_,L"door"))return offsetof(test::Obj,door);
    if(!wcscmp(prop_,L"PointLight"))return offsetof(test::Obj,light);
    if(!wcscmp(prop_,L"microwave_loop") || !wcscmp(prop_,L"audio_buzz"))return offsetof(test::Obj,audio);
    if(!wcscmp(prop_,L"timeMode"))return offsetof(test::Obj,mode);
    if(!wcscmp(prop_,L"time"))return offsetof(test::Obj,remaining);
    if(!wcscmp(prop_,L"timer"))return offsetof(test::Obj,timer);
    std::abort();
}
void* FindClassDefaultObject(const wchar_t*){std::abort();}
void* FindDispatchFunctionCached(void*,const wchar_t*){std::abort();}
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* name){const wchar_t* names[]={L"prop_microwave_C",L"prop_coffeeMaker_C",L"prop_coffeevending_C",L"prop_minioven_C",L"prop_bbq_C",L"cremator_C"};
    for(int i=0;i<6;++i)if(!wcscmp(name,names[i]))return test::At(test::Id(test::MicroClass+i));std::abort();}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* object){return object && static_cast<test::Obj*>(object)->id>=test::MicroClass?nullptr:CurrentWorld();}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void*):fn_(nullptr){std::abort();}
bool ParamFrame::SetRaw(const wchar_t*,const void*,int32_t){std::abort();}
bool ParamFrame::GetRaw(const wchar_t*,void*,int32_t)const{std::abort();}
bool Call(void*,ParamFrame&){std::abort();}
}
namespace ue_wrap::grill {bool Read(void*,State&){std::abort();}bool Apply(void*,const State&,const std::function<bool()>&){std::abort();}}
namespace ue_wrap::log {void Write(Level,const char*,...){++test::warnings;}}
namespace ue_wrap::prop {void* GetStaticMesh(void*){std::abort();}bool IsFrozen(void*){std::abort();}}
namespace ue_wrap::swinger {
bool EnsureResolved(){std::abort();}bool IsSwinger(void*){std::abort();}bool TryReadOpen(void*,bool&){std::abort();}
bool CallOpen(void*,bool){std::abort();}bool CallClose(void*){std::abort();}
}
namespace ue_wrap::engine {
void* ParentActorOf(void*,std::wstring*){std::abort();}bool IsComponentSimulatingPhysics(void*){std::abort();}
bool SetComponentSimulatePhysics(void*,bool){std::abort();}
bool SetComponentVisible(void* object,bool,bool){CHECK(object==test::At(test::Light));auto callback=std::move(test::callback);test::callback={};if(callback)callback();return true;}
}
namespace ue_wrap::component_calls {
bool CallParamlessNamed(void*,const wchar_t*){std::abort();}
bool SetAudioPlaying(void* object,bool playing,const std::function<bool()>& current) {
    CHECK(object==test::At(test::Audio));if(!test::audioReady || (current && !current()))return false;
    auto& audio=test::objects[test::Audio];if(audio.playing!=playing){if(playing)++test::starts;else ++test::stops;audio.playing=playing;}return true;
}
}
int main(){
    using namespace test;namespace K=ue_wrap::kitchen_machine;
    for(int i=0;i<Count;++i)objects[i].id=i;objects[Micro].audio=objects[Vendor].audio=At(Audio);objects[Micro].light=At(Light);
    K::State before;fields=false;CHECK(!K::Read(At(Micro),before));const auto warned=warnings;CHECK(!K::Read(At(Micro),before) && warnings==warned);
    fields=true;CHECK(K::Read(At(Micro),before) && before.kind==K::Kind::Microwave);
    K::State running{};running.kind=K::Kind::Microwave;running.flags=K::Active|K::Powered;running.mode=1;running.remaining=30;
    CHECK(K::Apply(At(Micro),running) && objects[Audio].playing && starts==1);
    CHECK(K::Apply(At(Micro),running) && starts==1);objects[Audio].playing=false;
    CHECK(K::Apply(At(Micro),running) && starts==2);audioReady=false;CHECK(!K::Apply(At(Micro),running));audioReady=true;
    running.flags=0;CHECK(K::Apply(At(Micro),running) && !objects[Audio].playing && stops==1);
    K::State vending{};vending.kind=K::Kind::CoffeeVending;vending.flags=K::Processing;
    CHECK(K::Apply(At(Vendor),vending) && starts==3);objects[Audio].playing=false;CHECK(K::Apply(At(Vendor),vending) && starts==4);
    CHECK(K::Apply(At(Vendor),vending) && starts==4);vending.flags=0;CHECK(K::Apply(At(Vendor),vending) && stops==2);
    callback=[]{++world;};running.flags=K::Active|K::Powered;CHECK(!K::Apply(At(Micro),running) && starts==4);
    objects[Micro].live=false;CHECK(K::Type(At(Micro))==K::Kind::None);
    puts("PASS kitchen wrapper: missing-bool retry without log flood, actual microwave/vending state, stopped-loop recovery, no unchanged restart, audio readiness and post-repaint world fence");
}
