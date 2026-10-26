#include "ue_wrap/actors/dingus_music.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/object_index.h"
#include <cmath>
#include <cstring>
#include <string>

namespace ue_wrap::dingus_music {
namespace {
namespace R=reflection;
struct World {
    uint32_t generation=world_identity::Generation();
    void* pointer=world_identity::CurrentWorld();
    bool Current()const{return pointer && generation==world_identity::Generation() && pointer==world_identity::CurrentWorld();}
};
struct Live {
    CachedObjRef object,cls;World world;
    explicit Live(void* p=nullptr){if(p && R::IsLive(p)){object.Set(p);cls.Set(R::ClassOf(p));}}
    bool Current()const{return world.Current() && object.Get() && cls.Get() && R::ClassOf(object.Get())==cls.Get();}
};
bool Typed(const Live& object,const wchar_t* name){
    if(!object.Current())return false;
    CachedObjRef base;base.Set(object_index::ClassByName(name));void* raw=base.Get();
    return raw && R::IsDescendantOfAny(object.cls.Get(),&raw,1) && base.Get() && object.Current();
}
template<class T>struct Field {
    Live source;const wchar_t* name;int offset=-1;
    Field(const Live& object,const wchar_t* property):source(object),name(property){if(source.Current())offset=R::FindPropertyOffset(source.cls.Get(),name);}
    bool Current()const{return offset>=0 && source.Current() && R::FindPropertyOffset(source.cls.Get(),name)==offset && source.Current();}
    bool Read(T& out)const{
        if(!Current())return false;T value{};std::memcpy(&value,static_cast<uint8_t*>(source.object.Get())+offset,sizeof(value));
        if(!Current())return false;out=value;return true;
    }
};
struct Bit {
    Live source;const wchar_t* name;int32_t offset=-1;uint8_t mask=0;
    Bit(const Live& object,const wchar_t* property):source(object),name(property){if(source.Current())R::FindBoolProperty(source.cls.Get(),name,offset,mask);}
    bool Current()const{
        int32_t actual=-1;uint8_t bits=0;
        return offset>=0 && mask && source.Current() && R::FindBoolProperty(source.cls.Get(),name,actual,bits) &&
            actual==offset && bits==mask && source.Current();
    }
    bool Read(bool& out)const{if(!Current())return false;const bool value=(static_cast<uint8_t*>(source.object.Get())[offset]&mask)!=0;
        if(!Current())return false;out=value;return true;}
    bool Write(bool value)const{if(!Current())return false;auto& byte=static_cast<uint8_t*>(source.object.Get())[offset];
        byte=value?uint8_t(byte|mask):uint8_t(byte&~mask);return Current();}
};
struct Binding {
    Live actor;Field<void*> component;Live audio;
    Binding(void* object,bool squeak=false):actor(object),component(actor,squeak?(IsDingus(object)?L"spk":L"qwak"):L"ws1"){
        void* p=nullptr;if((squeak?IsSqueakyToy(object):IsDingus(object)) && component.Read(p))audio=Live(p);
    }
    bool Current()const{void* p=nullptr;return actor.Current() && audio.Current() && Typed(audio,L"AudioComponent") &&
        component.Read(p) && p==audio.object.Raw() && actor.Current() && audio.Current();}
    bool Current(const std::function<bool()>& current)const{return Current() && (!current || current()) && Current();}
};
struct Function {
    Live owner;CachedObjRef fn;const wchar_t* name;
    Function(const Live& object,const wchar_t* verb):owner(object),name(verb){if(owner.Current())fn.Set(R::FindDispatchFunctionCached(owner.cls.Get(),name));}
    bool Current()const{return owner.Current() && fn.Get() && R::FindDispatchFunctionCached(owner.cls.Get(),name)==fn.Get() && owner.Current() && fn.Get();}
};
bool Slot(const ParamFrame& frame,const wchar_t* name,int bytes){
    const int offset=frame.ParamOffset(name),size=frame.FrameSize();
    if(!frame.valid() || bytes<=0 || offset<0 || size<bytes || offset>size-bytes)return false;
    for(const auto& param:R::FunctionParams(frame.function()))
        if(!_wcsicmp(param.name.c_str(),name))return param.offset==offset && param.size==bytes;
    return false;
}
bool Playing(const Binding& binding,bool& out,const std::function<bool()>& current={}){
    Function fn(binding.audio,L"IsPlaying");if(!fn.Current())return false;ParamFrame frame(fn.fn.Get());bool value=false;
    if(!Slot(frame,L"ReturnValue",sizeof(value)) || !fn.Current() || !binding.Current(current) || !fn.Current() ||
        !Call(binding.audio.object.Get(),frame) || !fn.Current() || !binding.Current(current) || !fn.Current() ||
        !frame.GetRaw(L"ReturnValue",&value,sizeof(value)))return false;
    out=value;return true;
}
bool FloatVerb(const Binding& binding,const wchar_t* verb,const wchar_t* parameter,float value,const std::function<bool()>& current){
    Function fn(binding.audio,verb);if(!fn.Current())return false;ParamFrame frame(fn.fn.Get());
    return Slot(frame,parameter,sizeof(value)) && frame.Set<float>(parameter,value) && fn.Current() && binding.Current(current) && fn.Current() &&
        Call(binding.audio.object.Get(),frame) && fn.Current() && binding.Current(current) && fn.Current();
}
bool EmptyVerb(const Binding& binding,const Live& object,const wchar_t* verb,const std::function<bool()>& current){
    Function fn(object,verb);if(!fn.Current())return false;ParamFrame frame(fn.fn.Get());
    return frame.valid() && frame.FrameSize()==0 && fn.Current() && binding.Current(current) && fn.Current() &&
        Call(object.object.Get(),frame) && fn.Current() && binding.Current(current) && fn.Current();
}
bool SetPlaying(const Binding& binding,bool value,const std::function<bool()>& current){
    bool before=false;if(!Playing(binding,before,current))return false;
    if(before==value)return binding.Current(current);
    return value?FloatVerb(binding,L"Play",L"StartTime",0,current):EmptyVerb(binding,binding.audio,L"Stop",current);
}
bool Valid(const State& state){return std::isfinite(state.volume) && state.volume>=0 && state.volume<=10 &&
    std::isfinite(state.pitch) && state.pitch>0 && state.pitch<=10 && std::isfinite(state.elapsed) && state.elapsed>=0 && state.elapsed<=1.e6f &&
    (!state.episode || state.playing) && (state.episode || !state.elapsed);}
uint64_t g_playbackRevision=0;
}

bool IsDingus(void* actor){return Typed(Live(actor),L"prop_dingus_C");}
bool IsSqueakyToy(void* actor){return IsDingus(actor) || Typed(Live(actor),L"prop_abplush_C");}
bool IsAltUse(void* function,const uint8_t* params){
    if(!function || !params)return false;
    const int off=R::FindParamOffset(function,L"player"),size=R::FunctionFrameSize(function);
    if(off<0 || off>size || sizeof(void*)>size-off)return false;
    void* player=nullptr;std::memcpy(&player,params+off,sizeof(player));
    if(!player || !R::IsLive(player))return false;
    int32_t flag=-1;uint8_t mask=0;
    return R::FindBoolProperty(R::ClassOf(player),L"input_alt",flag,mask) && flag>=0 && mask &&
        (static_cast<uint8_t*>(player)[flag]&mask)!=0;
}
size_t Collect(CachedObjRef* out,size_t cap){
    if(!out || !cap)return 0;
    struct Context {CachedObjRef* out;size_t cap,count=0;World world;};Context ctx{out,cap};
    if(!ctx.world.Current())return 0;
    for(const auto* name:{L"prop_dingus_C",L"prop_dingus1_C",L"prop_dingus2_C",L"prop_dingus3_C",L"prop_dingus4ugly_C",L"prop_dingus5erie_C"}){
        CachedObjRef cls;cls.Set(object_index::ClassByName(name));if(!ctx.world.Current())break;if(!cls.Get())continue;
        object_index::ForEachInstance(cls.Get(),[](void* p,void* actor,int32_t){
            auto& c=*static_cast<Context*>(p);if(!c.world.Current() || c.count>=c.cap)return;
            const Live candidate(actor);
            if(!Typed(candidate,L"prop_dingus_C") || world_identity::WorldOf(actor)!=c.world.pointer || !c.world.Current() || !candidate.Current())return;
            for(size_t i=0;i<c.count;++i)if(c.out[i].Is(actor))return;
            c.out[c.count++]=candidate.object;
        },&ctx);
        if(!cls.Get())break;
    }
    if(!ctx.world.Current()){for(size_t i=0;i<ctx.count;++i)out[i].Reset();return 0;}
    return ctx.count;
}
bool Read(void* actor,State& out){
    const Binding binding(actor);if(!binding.Current())return false;
    const Bit used(binding.actor,L"isUsed");const Field<float> volume(binding.audio,L"VolumeMultiplier"),pitch(binding.audio,L"PitchMultiplier");
    State value{};const bool hasPitch=pitch.offset>=0;
    if(!used.Read(value.used) || !volume.Read(value.volume) || (hasPitch && !pitch.Read(value.pitch)) || !Valid(value) || !binding.Current())return false;
    if(!Playing(binding,value.playing))return false;
    bool actualUsed=false;float actualVolume=0,actualPitch=1;
    if(!binding.Current() || !used.Read(actualUsed) || !volume.Read(actualVolume) || (hasPitch && !pitch.Read(actualPitch)) ||
        (!hasPitch && R::FindPropertyOffset(binding.audio.cls.Get(),L"PitchMultiplier")>=0) ||
        actualUsed!=value.used || actualVolume!=value.volume || actualPitch!=value.pitch || !binding.Current())return false;
    out=value;return true;
}
bool ReadClock(void* actor,Clock& out,const std::function<bool()>& current){
    const Binding binding(actor);if(!binding.Current(current))return false;
    const Field<void*> source(binding.audio,L"Sound");const Field<float> pitch(binding.audio,L"PitchMultiplier");
    void* p=nullptr;if(!source.Read(p))return false;const Live sound(p);
    CachedObjRef wave;wave.Set(object_index::ClassByName(L"SoundWave"));
    if(!sound.Current() || !wave.Get() || sound.cls.Get()!=wave.Get())return false;
    const Field<float> duration(sound,L"Duration");const Bit looping(sound,L"bLooping");
    const auto intact=[&]{void* actual=nullptr;return binding.Current() && sound.Current() && wave.Get() && sound.cls.Get()==wave.Get() &&
        source.Read(actual) && actual==sound.object.Raw() && sound.Current() && binding.Current();};
    const auto valid=[&]{return intact() && (!current || current()) && intact();};
    Clock value{};bool loops=false;
    if(!valid() || !duration.Read(value.duration) || !pitch.Read(value.pitch) || !looping.Read(loops) || !loops ||
        !std::isfinite(value.duration) || value.duration<.01f || value.duration>86400 || !std::isfinite(value.pitch) || value.pitch<=0 || value.pitch>10 || !valid())return false;
    float actualDuration=0,actualPitch=0;bool actualLoops=false;
    if(!duration.Read(actualDuration) || !pitch.Read(actualPitch) || !looping.Read(actualLoops) || !actualLoops ||
        actualDuration!=value.duration || actualPitch!=value.pitch || !intact())return false;
    value.audio=binding.audio.object;value.sound=sound.object;out=value;return true;
}
namespace {
bool ApplyState(void* actor,const State& input,Playback* playback,const std::function<bool()>& current){
    const State state=input;if(!Valid(state))return false;
    const uint64_t revision=++g_playbackRevision;if(playback)playback->revision=revision;
    const Binding binding(actor);
    const auto valid=[&]{const auto owned=[&]{return !playback || playback->revision==revision;};
        return owned() && binding.Current() && (!current || current()) && owned() && binding.Current();};
    State before{};if(!valid() || !Read(actor,before) || !valid())return false;
    const Bit used(binding.actor,L"isUsed");if(!valid() || !used.Write(state.used) || !valid())return false;
    if(before.volume!=state.volume && !FloatVerb(binding,L"SetVolumeMultiplier",L"NewVolumeMultiplier",state.volume,valid))return false;
    if(!valid())return false;
    if(playback && before.pitch!=state.pitch && !FloatVerb(binding,L"SetPitchMultiplier",L"NewPitchMultiplier",state.pitch,valid))return false;
    if(playback && state.playing && state.episode){
        Clock clock{};
        if(ReadClock(actor,clock,valid)){
            const bool seek=!before.playing || playback->episode!=state.episode || !playback->audio.Is(clock.audio.Get()) || !playback->sound.Is(clock.sound.Get());
            const auto sameClock=[&]{Clock actual{};return valid() && ReadClock(actor,actual,valid) && actual.audio.Is(clock.audio.Get()) &&
                actual.sound.Is(clock.sound.Get()) && actual.duration==clock.duration && actual.pitch==clock.pitch;};
            if(seek && !FloatVerb(binding,L"Play",L"StartTime",std::fmod(state.elapsed,clock.duration),sameClock))return false;
            if(!sameClock())return false;
            playback->audio=clock.audio;playback->sound=clock.sound;playback->episode=state.episode;return true;
        }
        if(!valid())return false;
    }
    if(!SetPlaying(binding,state.playing,valid) || !valid())return false;
    if(playback){*playback={};playback->revision=revision;}return true;
}
}
bool Apply(void* actor,const State& state,const std::function<bool()>& current){return ApplyState(actor,state,nullptr,current);}
bool ApplyPlayback(void* actor,const State& state,Playback& playback,const std::function<bool()>& current){return ApplyState(actor,state,&playback,current);}
bool StopMusic(void* actor,const std::function<bool()>& current){const Binding binding(actor);return binding.Current(current) && SetPlaying(binding,false,current);}
bool Interact(void* actor,bool toggle,const std::function<bool()>& current){
    const Binding binding(actor);if(!binding.Current(current))return false;
    Function action(binding.audio,L"ToggleActive");if(toggle && !action.Current())return false;
    ParamFrame actionFrame(toggle?action.fn.Get():nullptr);
    if(toggle && (!actionFrame.valid() || actionFrame.FrameSize()!=0 || !action.Current() || !binding.Current(current)))return false;
    if(!EmptyVerb(binding,binding.actor,L"touch",current))return false;
    // touch may replace the component or its dispatch function. Never toggle the successor.
    return !toggle || (action.Current() && binding.Current(current) && action.Current() && Call(binding.audio.object.Get(),actionFrame) &&
        action.Current() && binding.Current(current) && action.Current());
}
bool ReadSqueakPitch(void* actor,float& out){
    const Binding binding(actor,true);const Field<float> field(binding.audio,L"PitchMultiplier");float value=0;
    if(!binding.Current() || !field.Read(value) || !std::isfinite(value) || value<=0 || value>10 || !binding.Current())return false;
    out=value;return true;
}
bool Squeak(void* actor,float pitch,const std::function<bool()>& current){
    if(!std::isfinite(pitch) || pitch<=0 || pitch>10)return false;
    const Binding binding(actor,true);
    if(!binding.Current(current) || !FloatVerb(binding,L"SetPitchMultiplier",L"NewPitchMultiplier",pitch,current))return false;
    Function fn(binding.audio,L"Activate");if(!fn.Current())return false;ParamFrame play(fn.fn.Get());
    if(!Slot(play,L"bReset",sizeof(bool)) || !play.Set<bool>(L"bReset",true) || !fn.Current() || !binding.Current(current) || !fn.Current())return false;
    // Once dispatched, consume the one-shot even if native completion is uncertain.
    // No action6 replay: its counters, damage and achievements belong to gameplay.
    Call(binding.audio.object.Get(),play);return true;
}
}
