#include "ue_wrap/world/madness.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/component_calls.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_audio.h"
#include "ue_wrap/engine/world_identity.h"
#include <cmath>
#include <cstring>
#include <string>
namespace ue_wrap::madness {
namespace {
namespace R=reflection;
template<class T> bool ReadField(void* object,const wchar_t* name,T& value) {
    if(!object || !R::IsLive(object))return false;
    const int off=R::FindPropertyOffset(R::ClassOf(object),name);if(off<0)return false;
    std::memcpy(&value,static_cast<uint8_t*>(object)+off,sizeof(T));return true;
}
void* Object(void* object,const wchar_t* name) {
    void* result=nullptr;return ReadField(object,name,result) && result && R::IsLive(result)?result:nullptr;
}
bool Verb(void* object,const wchar_t* name){return object && component_calls::CallParamlessNamed(object,name);}
void* MakeWidget() {
    void* lib=R::FindClassDefaultObject(L"WidgetBlueprintLibrary");
    void* cls=object_index::ClassByName(L"ui_madnessCombat_C");
    if(!lib || !cls)return nullptr;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(lib),L"Create"));
    void* ctx=engine::GetWorldContext();void* owner=nullptr;
    if(!ctx || !f.valid() || !f.Set(L"WorldContextObject",ctx) || !f.Set(L"WidgetType",cls) ||
       !f.Set(L"OwningPlayer",owner) || !Call(lib,f))return nullptr;
    void* widget=f.Get<void*>(L"ReturnValue");if(!widget || !R::IsLive(widget))return nullptr;
    ParamFrame show(R::FindDispatchFunctionCached(R::ClassOf(widget),L"AddToViewport"));
    if(!show.valid() || !show.Set<int32_t>(L"ZOrder",0) || !Call(widget,show))return nullptr;
    return widget;
}
void* StartMusic(float elapsed) {
    void* lib=R::FindClassDefaultObject(L"GameplayStatics");
    void* sound=engine::FindSound(L"SoundWave /Game/audio/music/MC_music.MC_music");
    if(!lib || !sound)return nullptr;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(lib),L"SpawnSound2D"));
    void* ctx=engine::GetWorldContext();void* concurrency=nullptr;
    if(!ctx || !f.valid() || !f.Set(L"WorldContextObject",ctx) || !f.Set(L"Sound",sound) ||
       !f.Set<float>(L"VolumeMultiplier",.5f) || !f.Set<float>(L"PitchMultiplier",1.f) ||
       !f.Set<float>(L"StartTime",elapsed) || !f.Set(L"ConcurrencySettings",concurrency) ||
       !f.Set<bool>(L"bPersistAcrossLevelTransition",false) || !f.Set<bool>(L"bAutoDestroy",true) || !Call(lib,f))return nullptr;
    return f.Get<void*>(L"ReturnValue");
}
}
void* FindHost() {
    void* cls=object_index::ClassByName(L"madnessCombatMaster_C");if(!cls)return nullptr;
    void* result=nullptr;
    object_index::ForEachInstance(cls,[](void* ctx,void* actor,int32_t){
        if(!*static_cast<void**>(ctx) && R::IsLive(actor) && world_identity::CurrentWorld() &&
           world_identity::WorldOf(actor)==world_identity::CurrentWorld())*static_cast<void**>(ctx)=actor;
    },&result);
    return result;
}
bool Read(void* actor,State& out) {
    if(!actor || !R::IsLive(actor) || !R::NameEquals(R::NameOf(R::ClassOf(actor)),L"madnessCombatMaster_C"))return false;
    CachedObjRef live;live.Set(actor);State state{};state.active=true;
    void* hud=Object(actor,L"killcount");state.hud=hud!=nullptr;
    if(hud && (!ReadField(hud,L"count",state.kills) || state.kills<0))return false;
    void* audio=Object(actor,L"MC_music");if(!audio)return false;
    ParamFrame playing(R::FindDispatchFunctionCached(R::ClassOf(audio),L"IsPlaying"));
    if(!playing.valid() || !Call(audio,playing) || !live.Get())return false;
    state.playing=playing.Get<bool>(L"ReturnValue");
    ParamFrame age(R::FindDispatchFunctionCached(R::ClassOf(actor),L"GetGameTimeSinceCreation"));
    if(!age.valid() || !Call(actor,age) || !live.Get())return false;
    state.elapsed=age.Get<float>(L"ReturnValue");
    if(!std::isfinite(state.elapsed) || state.elapsed<0)return false;
    out=state;return true;
}
void Clear(View& view) {
    const auto old=view;view={};
    if(void* audio=old.audio.Get())Verb(audio,L"Stop");
    if(void* widget=old.widget.Get())Verb(widget,L"RemoveFromParent");
}
bool Apply(View& view,const State& state) {
    if(!state.active){Clear(view);return true;}
    bool ok=true;
    if(state.playing && !view.started) {
        void* audio=StartMusic(state.elapsed);view.audio.Set(audio);view.started=view.audio.Get()!=nullptr;
        if(!view.started)ok=false;
    } else if(!state.playing) {
        auto old=view.audio;view.audio={};view.started=false;
        if(void* audio=old.Get())Verb(audio,L"Stop");
    }
    if(state.hud) {
        if(!view.widget.Get()){view.widget.Set(MakeWidget());view.kills=-1;}
        void* widget=view.widget.Get();if(!widget)return false;
        if(view.kills!=state.kills) {
            const auto text=L"KILLS: "+std::to_wstring(state.kills);
            if(!component_calls::SetText(Object(widget,L"text_count"),text.c_str()))return false;
            // Native addCount also calls master.killed; update the label directly.
            view.kills=state.kills;
        }
    } else {
        auto old=view.widget;view.widget={};view.kills=-1;
        if(void* widget=old.Get())Verb(widget,L"RemoveFromParent");
    }
    return ok;
}
}
