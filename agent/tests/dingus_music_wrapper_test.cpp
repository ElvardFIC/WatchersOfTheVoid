#include "../../src/votv-coop/src/ue_wrap/core/component_calls.cpp"
#include "../../src/votv-coop/src/ue_wrap/actors/dingus_music.cpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
enum Index {Actor,Audio,Class,Other,Sound,WaveClass,OtherAudio,OtherSound,Count=32};
struct Obj {int index=0,serial=1;bool live=true,active=true,playing=false;uint8_t flags=0xA0;float volume=1,pitch=1,duration=69.1716f;void* component=nullptr;void* sound=nullptr;} objects[Count];
void* At(Index i){return &objects[i];}
uint32_t world=1;bool fields=true,result=true,parameters=true;
bool clockFields=true;
int plays=0,stops=0,chirps=0,touches=0,toggles=0;
std::wstring missing,after;
std::function<void()> callback;
struct Params {bool playing=false,reset=false;float value=0;};
bool Dispatch(void* object,void* function,Params& params);
std::map<const ue_wrap::ParamFrame*,Params> frames;
std::map<std::wstring,Obj*> functions;std::map<void*,std::wstring> names;
float lastStart=0;bool allowSeek=false;
void Callback(const std::wstring& name){if(!after.empty() && after!=name)return;
    auto f=std::move(callback);callback={};after.clear();if(f)f();}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p && static_cast<test::Obj*>(p)->live;}
void* ClassOf(void* p){return p==test::At(test::Actor)?test::At(test::Class):(p==test::At(test::Sound)||p==test::At(test::OtherSound))?test::At(test::WaveClass):p;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->index:-1;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return i>=0 && i<test::Count?test::At(test::Index(i)):nullptr;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0 && i<test::Count && p==test::At(test::Index(i)) && IsLive(p);}
bool IsDescendantOfAny(void* cls,void* const* bases,size_t count,int){return count==1 && cls==bases[0];}
int32_t InstanceOffset::Of(void*){if(!test::fields)return -1;
    if(!wcscmp(prop_,L"ws1") || !wcscmp(prop_,L"spk") || !wcscmp(prop_,L"qwak"))return offsetof(test::Obj,component);
    if(!wcscmp(prop_,L"VolumeMultiplier"))return offsetof(test::Obj,volume);
    if(!wcscmp(prop_,L"Sound"))return offsetof(test::Obj,sound);
    CHECK(!wcscmp(prop_,L"PitchMultiplier"));return offsetof(test::Obj,pitch);}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& off,uint8_t& mask){CHECK(!wcscmp(name,L"isUsed") || !wcscmp(name,L"bLooping"));
    off=offsetof(test::Obj,flags);mask=!wcscmp(name,L"bLooping")?2:4;return test::fields && (wcscmp(name,L"bLooping") || test::clockFields);}
int32_t FindPropertyOffset(void*,const wchar_t* name){CHECK(!wcscmp(name,L"Duration"));return test::fields && test::clockFields?offsetof(test::Obj,duration):-1;}
void* FindDispatchFunctionCached(void*,const wchar_t* name){if(test::missing==name)return nullptr;
    auto& fn=test::functions[name];if(!fn){CHECK(test::functions.size()+8<test::Count);fn=&test::objects[test::functions.size()+8];test::names[fn]=name;}return fn;}
void* FindFunction(void*,const wchar_t*){std::abort();}
int32_t FindParamOffset(void*,const wchar_t*){std::abort();}
#ifndef DINGUS_REAL_FRAME
int32_t FunctionFrameSize(void*){std::abort();}
#else
int32_t FunctionFrameSize(void* fn){const auto& name=test::names.at(fn);return name==L"Stop" || name==L"touch" || name==L"ToggleActive"?0:sizeof(test::Params);}
std::vector<ParamInfo> FunctionParams(void*){if(!test::parameters)return {};return {{L"ReturnValue",offsetof(test::Params,playing),1,0},
    {L"bReset",offsetof(test::Params,reset),1,0},{L"StartTime",offsetof(test::Params,value),4,0},
    {L"NewVolumeMultiplier",offsetof(test::Params,value),4,0},{L"NewPitchMultiplier",offsetof(test::Params,value),4,0}};}
bool CallFunction(void* object,void* function,void* raw){test::Params empty{};return test::Dispatch(object,function,raw?*static_cast<test::Params*>(raw):empty);}
#endif
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return CurrentWorld();}
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* name){return !wcscmp(name,L"prop_dingus_C")?test::At(test::Class):!wcscmp(name,L"SoundWave")?test::At(test::WaveClass):nullptr;}
size_t ForEachInstance(void*,InstanceFn,void*){std::abort();}
}
namespace ue_wrap::ftext_utils {bool MintFText(const wchar_t*,void*){std::abort();}}
#ifndef DINGUS_REAL_FRAME
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn){test::frames[this]={};}
bool ParamFrame::SetRaw(const wchar_t* name,const void* value,int32_t size){
    if(!wcscmp(name,L"bReset")){CHECK(size==sizeof(bool));test::frames[this].reset=*static_cast<const bool*>(value);return test::parameters;}
    CHECK(size==sizeof(float));
    CHECK(!wcscmp(name,L"StartTime") || !wcscmp(name,L"NewVolumeMultiplier") || !wcscmp(name,L"NewPitchMultiplier"));
    test::frames[this].value=*static_cast<const float*>(value);return test::parameters;}
bool ParamFrame::GetRaw(const wchar_t* name,void* out,int32_t size)const {CHECK(!wcscmp(name,L"ReturnValue") && size==sizeof(bool));
    *static_cast<bool*>(out)=test::frames[this].playing;return test::result;}
bool Call(void* object,ParamFrame& frame){return test::Dispatch(object,frame.function(),test::frames[&frame]);}
}
#endif
namespace test {
bool Dispatch(void* object,void* function,Params& params){
    const auto& name=names.at(function);
    if(object==test::At(test::Actor)){CHECK(name==L"touch");++test::touches;test::Callback(name);return true;}
    CHECK(object==test::At(test::Audio) || object==test::At(test::OtherAudio));auto& audio=*static_cast<Obj*>(object);
    if(name==L"IsPlaying")params.playing=audio.playing;
    else if(name==L"SetVolumeMultiplier")audio.volume=params.value;
    else if(name==L"SetPitchMultiplier")audio.pitch=params.value;
    else if(name==L"Activate"){CHECK(params.reset);++test::chirps;audio.playing=true;}
    else if(name==L"ToggleActive"){++test::toggles;audio.active=!audio.active;audio.playing=audio.active;}
    else if(name==L"Play"){CHECK(test::allowSeek || params.value==0);test::lastStart=params.value;++test::plays;audio.playing=true;}
    else {CHECK(name==L"Stop");++test::stops;audio.playing=false;}
    test::Callback(name);return true;}
}
#ifdef DINGUS_REAL_FRAME
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
#endif
#ifndef DINGUS_WRAPPER_FIXTURE_ONLY
int main(){
    using namespace test;namespace M=ue_wrap::dingus_music;
    for(int i=0;i<Count;++i)objects[i].index=i;auto& actor=objects[Actor];auto& audio=objects[Audio];actor.component=At(Audio);
    M::State state{true,true,.05f};CHECK(M::Apply(At(Actor),state) && plays==1 && audio.active && audio.playing && actor.flags==0xA4);
    CHECK(M::Apply(At(Actor),state) && plays==1);audio.playing=false;
    CHECK(M::Apply(At(Actor),state) && plays==2);CHECK(M::StopMusic(At(Actor)) && !audio.playing && stops==1);
    CHECK(M::Apply(At(Actor),state) && plays==3);state.playing=false;CHECK(M::Apply(At(Actor),state) && !audio.playing);
    auto untouched=state;result=false;CHECK(!M::Read(At(Actor),untouched) && untouched.volume==state.volume);result=true;
    fields=false;CHECK(!M::Apply(At(Actor),state));fields=true;
    missing=L"Play";state.playing=true;CHECK(!M::Apply(At(Actor),state));missing.clear();CHECK(M::Apply(At(Actor),state));
    after=L"IsPlaying";callback=[]{++objects[Actor].serial;};auto started=plays;CHECK(!M::Apply(At(Actor),state) && plays==started);
    after=L"IsPlaying";callback=[]{++world;};CHECK(!M::Read(At(Actor),untouched));
    bool current=true;state.volume=.02f;after=L"SetVolumeMultiplier";callback=[&]{current=false;};
    CHECK(!M::Apply(At(Actor),state,[&]{return current;}));current=true;
    callback=[]{objects[Actor].component=nullptr;};CHECK(!M::Apply(At(Actor),state));actor.component=At(Audio);
    CHECK(M::StopMusic(At(Actor)));audio.live=false;CHECK(!M::Apply(At(Actor),state));audio.live=true;
    CHECK(!M::Apply(At(Other),state));state.volume=NAN;CHECK(!M::Apply(At(Actor),state));
    CHECK(M::Squeak(At(Actor),1.7f) && chirps==1 && audio.pitch==1.7f);
    after=L"SetPitchMultiplier";callback=[&]{current=false;};CHECK(!M::Squeak(At(Actor),1.6f,[&]{return current;}) && chirps==1);current=true;
    after=L"SetPitchMultiplier";callback=[]{objects[Actor].component=nullptr;};CHECK(!M::Squeak(At(Actor),1.5f) && chirps==1);actor.component=At(Audio);
    after=L"Activate";callback=[]{++objects[Actor].serial;};CHECK(M::Squeak(At(Actor),1.4f) && chirps==2);
    CHECK(M::Interact(At(Actor),true) && touches==1 && toggles==1);
    missing=L"ToggleActive";CHECK(!M::Interact(At(Actor),true) && touches==1);missing.clear();
    after=L"touch";callback=[&]{current=false;};CHECK(!M::Interact(At(Actor),true,[&]{return current;}) && touches==2 && toggles==1);current=true;
    after=L"touch";callback=[]{objects[Actor].component=nullptr;};CHECK(!M::Interact(At(Actor),true) && toggles==1);actor.component=At(Audio);
    puts("PASS toy music wrapper: actual Play/Stop and volume helper, active-but-stopped recovery, no unchanged restart, packed used bit, metadata retry and actor/component/world/current interruption");
}
#endif
