#include "../../src/votv-coop/src/ue_wrap/core/component_calls.cpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int serial=1;bool live=true,active=true,playing=false;} audio;
float volume=1;
uint32_t world=1;bool query=true,metadata=true,param=true;
int plays=0,stops=0;
std::wstring missing;
std::map<std::wstring,std::wstring> functions;
std::map<const ue_wrap::ParamFrame*,bool> frames;
std::function<void()> callback;
}
namespace ue_wrap::reflection {
void* FindFunction(void*,const wchar_t*){std::abort();}
int32_t FindParamOffset(void*,const wchar_t*){std::abort();}
bool IsLive(void* object){return object==&test::audio && test::audio.live;}
void* ClassOf(void* object){return object;}
int32_t InternalIndexOf(void* object){return object==&test::audio?0:-1;}
int32_t SlotSerial(int32_t){return test::audio.serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* object,int32_t i){return i==0 && IsLive(object);}
void* FindDispatchFunctionCached(void*,const wchar_t* name){if(test::missing==name)return nullptr;auto& fn=test::functions[name];fn=name;return &fn;}
}
namespace ue_wrap::ftext_utils {bool MintFText(const wchar_t*,void*){std::abort();}}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return CurrentWorld();}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn){test::frames[this]=false;}
bool ParamFrame::SetRaw(const wchar_t* name,const void* data,int32_t size){CHECK(size==sizeof(float));
    if(!wcscmp(name,L"StartTime"))CHECK(*static_cast<const float*>(data)==0);
    else {CHECK(!wcscmp(name,L"NewVolumeMultiplier"));if(test::param)test::volume=*static_cast<const float*>(data);}return test::param;}
bool ParamFrame::GetRaw(const wchar_t* name,void* data,int32_t size)const {CHECK(!wcscmp(name,L"ReturnValue") && size==sizeof(bool));*static_cast<bool*>(data)=test::frames[this];return test::metadata;}
bool Call(void* object,ParamFrame& frame){
    CHECK(object==&test::audio);const auto& name=*static_cast<std::wstring*>(frame.function());
    if(name==L"IsPlaying"){if(!test::query)return false;test::frames[&frame]=test::audio.playing;}
    else if(name==L"Play"){++test::plays;test::audio.playing=true;}
    else if(name==L"SetVolumeMultiplier"){}
    else {CHECK(name==L"Stop");++test::stops;test::audio.playing=false;}
    auto callback=std::move(test::callback);test::callback={};if(callback)callback();return true;
}
}
int main(){
    using namespace test;using ue_wrap::component_calls::SetAudioPlaying;
    CHECK(SetAudioPlaying(&audio,true) && plays==1 && audio.active && audio.playing);
    CHECK(SetAudioPlaying(&audio,true) && plays==1);audio.playing=false;
    CHECK(SetAudioPlaying(&audio,true) && plays==2); // still active, but the voice stopped
    CHECK(SetAudioPlaying(&audio,false) && stops==1);CHECK(SetAudioPlaying(&audio,false) && stops==1);
    query=false;CHECK(!SetAudioPlaying(&audio,true) && plays==2);query=true;
    metadata=false;CHECK(!SetAudioPlaying(&audio,true) && plays==2);metadata=true;
    missing=L"Play";CHECK(!SetAudioPlaying(&audio,true));missing.clear();param=false;CHECK(!SetAudioPlaying(&audio,true));param=true;
    callback=[]{++audio.serial;};CHECK(!SetAudioPlaying(&audio,true) && plays==2);
    callback=[]{++world;};CHECK(!SetAudioPlaying(&audio,true) && plays==2);
    bool current=true;callback=[&]{current=false;};CHECK(!SetAudioPlaying(&audio,true,[&]{return current;}) && plays==2);
    current=true;CHECK(SetAudioPlaying(&audio,true,[&]{return current;}) && plays==3);
    audio.live=false;CHECK(!SetAudioPlaying(&audio,false));
    using ue_wrap::component_calls::SetVolumeMultiplier;audio.live=true;
    CHECK(SetVolumeMultiplier(&audio,.08f) && volume==.08f);missing=L"SetVolumeMultiplier";CHECK(!SetVolumeMultiplier(&audio,.04f));missing.clear();
    CHECK(SetVolumeMultiplier(&audio,.04f) && volume==.04f);param=false;CHECK(!SetVolumeMultiplier(&audio,.06f));param=true;
    callback=[]{++audio.serial;};CHECK(!SetVolumeMultiplier(&audio,.03f));
    callback=[]{++world;};CHECK(!SetVolumeMultiplier(&audio,.02f));CHECK(!SetVolumeMultiplier(&audio,NAN));
    puts("PASS audio playback: Play/Stop by actual voice state, active-but-stopped recovery, no unchanged restart, reflected StartTime, query/metadata/verb retry and serial/world/reentrant fences");
}
