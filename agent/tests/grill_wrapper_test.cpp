#include "../../src/votv-coop/src/ue_wrap/devices/grill.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
enum Index {Actor,Audio,Class,Other,Count};
struct Obj {int index=0,serial=1;bool live=true;uint8_t flags=0xA0;int coals=0;float burn=0,volume=1;void* audio=nullptr;} objects[Count];
bool fields=true,boolFields=true,playing=false,query=true,repaint=true;
int starts=0,stops=0,lidPaints=0,firePaints=0;
uint32_t world=1;
std::function<void()> callback;
std::function<void()> metadataCallback;
std::wstring after;
std::map<std::wstring,std::wstring> functions;
std::map<const ue_wrap::ParamFrame*,bool> frames;
void* At(Index i){return &objects[i];}
void Callback(const wchar_t* name){if(!after.empty() && after!=name)return;after.clear();auto f=std::move(callback);callback={};if(f)f();}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p && static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->index:-1;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t i){return i>=0 && i<test::Count && p==test::At(test::Index(i)) && IsLive(p);}
void* ClassOf(void* p){return p==test::At(test::Actor)?test::At(test::Class):p;}
bool IsDescendantOfAny(void* cls,void* const* bases,size_t count,int){return count==1 && cls==bases[0];}
int32_t InstanceOffset::Of(void*) {
    auto cb=std::move(test::metadataCallback);test::metadataCallback={};if(cb)cb();
    if(!test::fields)return -1;
    if(!wcscmp(prop_,L"coals"))return offsetof(test::Obj,coals);
    if(!wcscmp(prop_,L"a"))return offsetof(test::Obj,burn);
    if(!wcscmp(prop_,L"cookingLoop"))return offsetof(test::Obj,audio);
    CHECK(!wcscmp(prop_,L"VolumeMultiplier"));return offsetof(test::Obj,volume);
}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& off,uint8_t& mask) {
    if(!test::fields || !test::boolFields)return false;off=offsetof(test::Obj,flags);
    if(!wcscmp(name,L"active"))mask=1;else {CHECK(!wcscmp(name,L"hasLid"));mask=4;}return true;
}
void* FindDispatchFunctionCached(void*,const wchar_t* name){CHECK(!wcscmp(name,L"IsPlaying"));auto& fn=test::functions[name];fn=name;return &fn;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return CurrentWorld();}
}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t* name){CHECK(!wcscmp(name,L"prop_bbq_C"));return test::At(test::Class);}}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn){test::frames[this]=false;}
bool ParamFrame::GetRaw(const wchar_t* name,void* out,int32_t size)const {
    CHECK(!wcscmp(name,L"ReturnValue") && size==sizeof(bool));*static_cast<bool*>(out)=test::frames[this];return true;
}
bool Call(void* object,ParamFrame& frame){CHECK(object==test::At(test::Audio));test::Callback(L"IsPlaying");test::frames[&frame]=test::playing;return test::query;}
}
namespace ue_wrap::component_calls {
bool CallParamlessNamed(void* object,const wchar_t* name) {
    CHECK(object==test::At(test::Actor));
    if(!wcscmp(name,L"updLid"))++test::lidPaints;else {CHECK(!wcscmp(name,L"upd"));++test::firePaints;}
    test::Callback(name);return test::repaint;
}
bool SetVolumeMultiplier(void* object,float volume){CHECK(object==test::At(test::Audio));test::objects[test::Audio].volume=volume;test::Callback(L"volume");return true;}
bool SetActive(void* object,bool active,bool reset){CHECK(object==test::At(test::Audio) && !reset);if(active)++test::starts;else ++test::stops;test::playing=active;test::Callback(L"active");return true;}
bool SetAudioPlaying(void* object,bool active,const std::function<bool()>& current){return
    (!current || current()) && (test::playing==active || SetActive(object,active,false));}
}
int main() {
    using namespace test;namespace G=ue_wrap::grill;
    for(int i=0;i<Count;++i)objects[i].index=i;objects[Actor].audio=At(Audio);
    G::State out{};CHECK(G::Read(At(Actor),out) && !out.active && !out.lid && !out.cooking && out.volume==1);
    G::State state{true,true,true,3,.5f,.07f};CHECK(G::Apply(At(Actor),state));
    CHECK(objects[Actor].flags==0xA5 && objects[Actor].coals==3 && objects[Actor].burn==.5f && objects[Audio].volume==.07f);
    CHECK(starts==1 && playing && lidPaints==1 && firePaints==1 && G::Read(At(Actor),out) && out.lid && out.cooking);
    CHECK(G::Apply(At(Actor),state) && starts==1);playing=false;CHECK(G::Apply(At(Actor),state) && starts==2);
    state.lid=state.cooking=false;CHECK(G::Apply(At(Actor),state) && stops==1 && objects[Actor].flags==0xA1);
    auto before=out;fields=false;CHECK(!G::Read(At(Actor),out) && out.volume==before.volume && !G::Apply(At(Actor),state));fields=true;
    query=false;CHECK(!G::Apply(At(Actor),state));query=true;
    state.coals=17;CHECK(!G::Apply(At(Actor),state));state.coals=3;state.volume=NAN;CHECK(!G::Valid(state));state.volume=.05f;
    callback=[]{++objects[Actor].serial;};auto count=lidPaints;CHECK(!G::Apply(At(Actor),state) && lidPaints==count);
    callback=[]{++world;};CHECK(!G::Read(At(Actor),out));
    objects[Audio].live=false;CHECK(!G::Apply(At(Actor),state));objects[Audio].live=true;
    CHECK(!G::Read(At(Other),out));objects[Actor].flags=0xA5;repaint=false;
    CHECK(!G::Apply(At(Actor),state));repaint=true;CHECK(G::Apply(At(Actor),state));
    CHECK(objects[Actor].flags==0xA1);
    bool current=true;auto guard=[&]{return current;};after=L"updLid";callback=[&]{current=false;};auto fires=firePaints;
    CHECK(!G::Apply(At(Actor),state,guard) && firePaints==fires);current=true;
    callback=[]{objects[Actor].audio=nullptr;};CHECK(!G::Apply(At(Actor),state));objects[Actor].audio=At(Audio);
    playing=true;boolFields=false;CHECK(G::StopSound(At(Actor)) && !playing);boolFields=true;
    playing=true;CHECK(!G::StopSound(At(Actor),[]{return false;}) && playing);CHECK(G::StopSound(At(Actor)) && !playing);
    // Stock edits do not depend on an audio component and cannot restore an old
    // coal count after metadata resolution re-enters another native increment.
    objects[Actor].audio=nullptr;objects[Actor].coals=3;objects[Actor].flags=0xA5;query=false;
    G::Stock stock{};CHECK(G::ReadStock(At(Actor),stock) && stock.coals==3 && stock.lid);
    const auto oldBurn=objects[Actor].burn;auto yes=[]{return true;};
    CHECK(G::ChangeStock(At(Actor),1,4,yes)==G::Edit::Applied && objects[Actor].coals==4 && objects[Actor].flags==0xA5 && objects[Actor].burn==oldBurn);
    CHECK(G::ChangeStock(At(Actor),1,6,yes)==G::Edit::Retry && objects[Actor].coals==4);
    metadataCallback=[]{++objects[Actor].coals;};CHECK(G::ChangeStock(At(Actor),1,5,yes)==G::Edit::Retry && objects[Actor].coals==5);
    repaint=false;CHECK(G::ChangeStock(At(Actor),2,0,yes)==G::Edit::Interrupted && objects[Actor].flags==0xA1 && objects[Actor].coals==5);repaint=true;
    const auto oldPaints=lidPaints;CHECK(G::ChangeStock(At(Actor),2,1,[]{return false;})==G::Edit::Retry && lidPaints==oldPaints);
    metadataCallback=[]{++objects[Actor].serial;};stock.coals=99;CHECK(!G::ReadStock(At(Actor),stock) && stock.coals==99);
    puts("PASS grill wrapper: native lid/fire repaint, exact fuel fields, canonical cooking loop/volume, no unchanged restart, audio recovery, packed flags, missing fields and serial/world fences");
}
