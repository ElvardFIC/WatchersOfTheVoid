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
    CachedObjRef live;live.Set(actor);int32_t off=-1;uint8_t mask=0;
    if(!Used(actor,off,mask))return false;
    State value{};value.used=(static_cast<uint8_t*>(actor)[off]&mask)!=0;
    static R::InstanceOffset volume{L"VolumeMultiplier"};const auto vo=volume.Of(audio);
    if(vo<0)return false;
    std::memcpy(&value.volume,static_cast<uint8_t*>(audio)+vo,sizeof(float));
    if(!std::isfinite(value.volume) || value.volume<0 || value.volume>10)return false;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(audio),L"IsPlaying"));
    if(!f.valid() || !Call(audio,f) || !live.Get())return false;
    value.playing=f.Get<bool>(L"ReturnValue");out=value;return true;
}
bool Apply(void* actor,const State& state) {
    if(!std::isfinite(state.volume) || state.volume<0 || state.volume>10)return false;
    State current{};if(!Read(actor,current))return false;
    CachedObjRef live;live.Set(actor);CachedObjRef audio;audio.Set(Audio(actor));
    int32_t off=-1;uint8_t mask=0;if(!Used(actor,off,mask))return false;
    auto& byte=static_cast<uint8_t*>(actor)[off];
    byte=state.used?uint8_t(byte|mask):uint8_t(byte&~mask);
    if(current.volume!=state.volume && !component_calls::SetVolumeMultiplier(audio.Get(),state.volume))return false;
    if(!live.Get() || !audio.Get())return false;
    // Compare actual playback every time: a later native loadData may silence the
    // component after a successful network apply. Do not restart unchanged music.
    return current.playing==state.playing || component_calls::SetActive(audio.Get(),state.playing,false);
}
bool Interact(void* actor,bool toggle) {
    if(!Audio(actor))return false;
    CachedObjRef live;live.Set(actor);
    if(!component_calls::CallParamlessNamed(actor,L"touch") || !live.Get())return false;
    return !toggle || component_calls::CallParamlessNamed(Audio(actor),L"ToggleActive");
}
bool ReadSqueakPitch(void* actor,float& pitch) {
    void* audio=Audio(actor,true);if(!audio)return false;
    static R::InstanceOffset field{L"PitchMultiplier"};const auto off=field.Of(audio);
    if(off<0)return false;
    std::memcpy(&pitch,static_cast<uint8_t*>(audio)+off,sizeof(float));
    return std::isfinite(pitch) && pitch>0 && pitch<=10;
}
bool Squeak(void* actor,float pitch) {
    if(!std::isfinite(pitch) || pitch<=0 || pitch>10)return false;
    void* audio=Audio(actor,true);if(!audio)return false;
    CachedObjRef live;live.Set(actor);CachedObjRef sound;sound.Set(audio);
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(audio),L"SetPitchMultiplier"));
    if(!f.valid() || !f.Set<float>(L"NewPitchMultiplier",pitch) || !Call(audio,f) || !live.Get() || !sound.Get())return false;
    // Only the native audio component: no action6 replay, b increments, damage,
    // inventory effects or achievement changes on a remote presentation copy.
    return component_calls::Activate(sound.Get());
}
}
