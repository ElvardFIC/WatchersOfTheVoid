#include "ue_wrap/actors/dingus_music.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/component_calls.h"
#include "ue_wrap/core/object_index.h"
#include <cmath>
#include <cstring>
namespace ue_wrap::dingus_music {
namespace {
namespace R=reflection;
void* Audio(void* actor,bool squeak=false) {
    if(!(squeak?IsSqueakyToy(actor):IsDingus(actor)))return nullptr;
    static R::InstanceOffset music{L"ws1"},voice{L"spk"},qwak{L"qwak"};
    const auto off=(squeak?(IsDingus(actor)?voice:qwak):music).Of(actor);
    if(off<0)return nullptr;
    void* audio=nullptr;std::memcpy(&audio,static_cast<uint8_t*>(actor)+off,sizeof(audio));
    return audio && R::IsLive(audio)?audio:nullptr;
}
bool Used(void* actor,int32_t& off,uint8_t& mask) {
    return R::FindBoolProperty(R::ClassOf(actor),L"isUsed",off,mask) && off>=0 && mask;
}
}
bool IsDingus(void* actor) {
    if(!actor || !R::IsLive(actor))return false;
    void* base=object_index::ClassByName(L"prop_dingus_C");
    return base && R::IsDescendantOfAny(R::ClassOf(actor),&base,1);
}
bool IsSqueakyToy(void* actor) {
    if(IsDingus(actor))return true;
    if(!actor || !R::IsLive(actor))return false;
    void* base=object_index::ClassByName(L"prop_abplush_C");
    return base && R::IsDescendantOfAny(R::ClassOf(actor),&base,1);
}
bool IsAltUse(void* function,const uint8_t* params) {
    if(!function || !params)return false;
    const int off=R::FindParamOffset(function,L"player"),size=R::FunctionFrameSize(function);
    if(off<0 || off>size || sizeof(void*)>size-off)return false;
    void* player=nullptr;std::memcpy(&player,params+off,sizeof(player));
    if(!player || !R::IsLive(player))return false;
    int32_t flag=-1;uint8_t mask=0;
    return R::FindBoolProperty(R::ClassOf(player),L"input_alt",flag,mask) && flag>=0 && mask &&
        (static_cast<uint8_t*>(player)[flag]&mask)!=0;
}
size_t Collect(CachedObjRef* out,size_t cap) {
    struct Context {CachedObjRef* out;size_t cap,count=0;void* world;};
    Context ctx{out,cap,0,world_identity::CurrentWorld()};if(!ctx.world)return 0;
    // Exact indexed classes, never a full UObject walk. The last two are variant
    // names from the game's package; ancestry still excludes unrelated props.
    for(const auto* name:{L"prop_dingus_C",L"prop_dingus1_C",L"prop_dingus2_C",L"prop_dingus3_C",
                         L"prop_dingus4ugly_C",L"prop_dingus5erie_C"}) {
        void* cls=object_index::ClassByName(name);if(!cls)continue;
        object_index::ForEachInstance(cls,[](void* p,void* actor,int32_t){
            auto& c=*static_cast<Context*>(p);
            if(c.count<c.cap && IsDingus(actor) && world_identity::WorldOf(actor)==c.world)
                c.out[c.count++].Set(actor);
        },&ctx);
    }
    return ctx.count;
}
bool Read(void* actor,State& out) {
    void* audio=Audio(actor);if(!audio)return false;
    CachedObjRef live,sound;live.Set(actor);sound.Set(audio);const auto world=world_identity::Generation();int32_t off=-1;uint8_t mask=0;
    if(!Used(actor,off,mask))return false;
    State value{};value.used=(static_cast<uint8_t*>(actor)[off]&mask)!=0;
    static R::InstanceOffset volume{L"VolumeMultiplier"},pitch{L"PitchMultiplier"};const auto vo=volume.Of(audio),po=pitch.Of(audio);
    if(vo<0 || !live.Get() || !sound.Get())return false;
    std::memcpy(&value.volume,static_cast<uint8_t*>(audio)+vo,sizeof(float));
    if(po>=0)std::memcpy(&value.pitch,static_cast<uint8_t*>(audio)+po,sizeof(float));
    if(!std::isfinite(value.pitch) || value.pitch<=0 || value.pitch>10)value.pitch=1;
    if(!std::isfinite(value.volume) || value.volume<0 || value.volume>10)return false;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(audio),L"IsPlaying"));
    if(!f.valid() || !Call(audio,f) || !live.Get() || !sound.Get() || world!=world_identity::Generation() ||
        Audio(actor)!=audio || !f.GetRaw(L"ReturnValue",&value.playing,sizeof(value.playing)))return false;
    out=value;return true;
}
bool ReadClock(void* actor,Clock& out,const std::function<bool()>& current){
    if(current && !current())return false;CachedObjRef live;live.Set(actor);Clock value{};value.audio.Set(Audio(actor));
    const auto world=world_identity::Generation();if(!live.Get() || !value.audio.Get())return false;
    static R::InstanceOffset source{L"Sound"},pitch{L"PitchMultiplier"};const int sourceOff=source.Of(value.audio.Get()),pitchOff=pitch.Of(value.audio.Get());
    const auto valid=[&]{return live.Get() && value.audio.Get() && world==world_identity::Generation() && Audio(actor)==value.audio.Raw() && (!current || current());};
    if(sourceOff<0 || pitchOff<0 || !valid())return false;
    void* sound=nullptr;std::memcpy(&sound,static_cast<uint8_t*>(value.audio.Get())+sourceOff,sizeof(sound));value.sound.Set(sound);
    CachedObjRef wave;wave.Set(object_index::ClassByName(L"SoundWave"));
    if(!valid() || !value.sound.Get() || !wave.Get() || R::ClassOf(sound)!=wave.Raw())return false;
    const int duration=R::FindPropertyOffset(wave.Get(),L"Duration");int32_t loop=-1;uint8_t mask=0;
    if(duration<0 || !R::FindBoolProperty(wave.Get(),L"bLooping",loop,mask) || loop<0 || !mask || !valid() || !value.sound.Get() || !wave.Get() || R::ClassOf(sound)!=wave.Raw())return false;
    void* actual=nullptr;std::memcpy(&actual,static_cast<uint8_t*>(value.audio.Get())+sourceOff,sizeof(actual));if(actual!=sound)return false;
    std::memcpy(&value.duration,static_cast<uint8_t*>(sound)+duration,sizeof(float));
    std::memcpy(&value.pitch,static_cast<uint8_t*>(value.audio.Get())+pitchOff,sizeof(float));
    if(!(static_cast<uint8_t*>(sound)[loop]&mask) || !std::isfinite(value.duration) || value.duration<.01f || value.duration>86400 ||
       !std::isfinite(value.pitch) || value.pitch<=0 || value.pitch>10 || !valid() || !value.sound.Get() || !wave.Get())return false;
    std::memcpy(&actual,static_cast<uint8_t*>(value.audio.Get())+sourceOff,sizeof(actual));if(actual!=sound)return false;
    out=value;return true;
}
namespace {
bool ApplyState(void* actor,const State& state,Playback* playback,const std::function<bool()>& current) {
    if(!std::isfinite(state.volume) || state.volume<0 || state.volume>10 || !IsDingus(actor))return false;
    if(playback && (!std::isfinite(state.pitch) || state.pitch<=0 || state.pitch>10 || !std::isfinite(state.elapsed) || state.elapsed<0 || state.elapsed>1.e6f ||
        (state.episode && !state.playing) || (!state.episode && state.elapsed)))return false;
    if(current && !current())return false;
    CachedObjRef live;live.Set(actor);CachedObjRef audio;audio.Set(Audio(actor));const auto world=world_identity::Generation();
    const auto valid=[&]{return live.Get() && audio.Get() && world==world_identity::Generation() &&
        Audio(actor)==audio.Raw() && (!current || current());};
    State before{};if(!valid() || !Read(actor,before) || !valid())return false;
    int32_t off=-1;uint8_t mask=0;if(!Used(actor,off,mask))return false;
    auto& byte=static_cast<uint8_t*>(actor)[off];
    byte=state.used?uint8_t(byte|mask):uint8_t(byte&~mask);
    if(before.volume!=state.volume && !component_calls::SetVolumeMultiplier(audio.Get(),state.volume))return false;
    if(!valid())return false;
    if(playback && before.pitch!=state.pitch){
        CachedObjRef fn;fn.Set(R::FindDispatchFunctionCached(R::ClassOf(audio.Get()),L"SetPitchMultiplier"));ParamFrame pitch(fn.Get());
        if(!pitch.valid() || !pitch.Set<float>(L"NewPitchMultiplier",state.pitch) || !valid() || !fn.Get() || !Call(audio.Get(),pitch) || !valid())return false;
    }
    if(playback && state.playing && state.episode){
        Clock clock{};
        if(ReadClock(actor,clock,valid)){
            if(!valid())return false;
            const bool seek=!before.playing || playback->episode!=state.episode || !playback->audio.Is(clock.audio.Get()) || !playback->sound.Is(clock.sound.Get());
            if(seek){
                const auto sameClock=[&]{Clock actual{};return valid() && ReadClock(actor,actual,valid) && actual.audio.Is(clock.audio.Get()) && actual.sound.Is(clock.sound.Get());};
                CachedObjRef fn;fn.Set(R::FindDispatchFunctionCached(R::ClassOf(audio.Get()),L"Play"));ParamFrame play(fn.Get());
                const float position=std::fmod(state.elapsed,clock.duration);
                if(!play.valid() || !play.Set<float>(L"StartTime",position) || !sameClock() || !fn.Get() || !Call(audio.Get(),play) || !sameClock())return false;
            }
            playback->audio=clock.audio;playback->sound=clock.sound;playback->episode=state.episode;
            return true;
        }
        if(!valid())return false;
    }
    if(playback)*playback={};
    // Compare actual playback every time: a later native loadData may silence the
    // component after a successful network apply. Do not restart unchanged music.
    return component_calls::SetAudioPlaying(audio.Get(),state.playing,valid);
}
}
bool Apply(void* actor,const State& state,const std::function<bool()>& current){return ApplyState(actor,state,nullptr,current);}
bool ApplyPlayback(void* actor,const State& state,Playback& playback,const std::function<bool()>& current){return ApplyState(actor,state,&playback,current);}
bool StopMusic(void* actor,const std::function<bool()>& current) {
    if((current && !current()) || !IsDingus(actor))return false;
    CachedObjRef live,audio;live.Set(actor);audio.Set(Audio(actor));const auto world=world_identity::Generation();
    return live.Get() && audio.Get() && component_calls::SetAudioPlaying(audio.Get(),false,[&]{return live.Get() &&
        audio.Get() && world==world_identity::Generation() && Audio(actor)==audio.Raw() && (!current || current());});
}
bool Interact(void* actor,bool toggle,const std::function<bool()>& current) {
    if(current && !current())return false;void* component=Audio(actor);if(!component)return false;
    CachedObjRef live,audio;live.Set(actor);audio.Set(component);const auto world=world_identity::Generation();
    const auto valid=[&]{return live.Get() && audio.Get() && world==world_identity::Generation() &&
        Audio(actor)==audio.Raw() && (!current || current());};
    ParamFrame touch(R::FindDispatchFunctionCached(R::ClassOf(actor),L"touch"));
    ParamFrame action(toggle?R::FindDispatchFunctionCached(R::ClassOf(component),L"ToggleActive"):nullptr);
    if(!valid() || !touch.valid() || (toggle && !action.valid()))return false;
    if(!Call(actor,touch) || !valid())return false;
    return !toggle || (Call(audio.Get(),action) && valid());
}
bool ReadSqueakPitch(void* actor,float& pitch) {
    void* audio=Audio(actor,true);if(!audio)return false;
    static R::InstanceOffset field{L"PitchMultiplier"};const auto off=field.Of(audio);
    if(off<0)return false;
    std::memcpy(&pitch,static_cast<uint8_t*>(audio)+off,sizeof(float));
    return std::isfinite(pitch) && pitch>0 && pitch<=10;
}
bool Squeak(void* actor,float pitch,const std::function<bool()>& current) {
    if(!std::isfinite(pitch) || pitch<=0 || pitch>10 || (current && !current()))return false;
    void* audio=Audio(actor,true);if(!audio)return false;
    CachedObjRef live;live.Set(actor);CachedObjRef sound;sound.Set(audio);const auto world=world_identity::Generation();
    const auto valid=[&]{return live.Get() && sound.Get() && world==world_identity::Generation() &&
        Audio(actor,true)==sound.Raw() && (!current || current());};
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(audio),L"SetPitchMultiplier"));
    if(!valid() || !f.valid() || !f.Set<float>(L"NewPitchMultiplier",pitch) || !Call(audio,f) || !valid())return false;
    // Only the native audio component: no action6 replay, b increments, damage,
    // inventory effects or achievement changes on a remote presentation copy.
    ParamFrame play(R::FindDispatchFunctionCached(R::ClassOf(sound.Get()),L"Activate"));
    if(!valid() || !play.valid() || !play.Set<bool>(L"bReset",true))return false;
    Call(sound.Get(),play);return true;
}
}
