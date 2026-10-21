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
bool MakeWidget(View& view,const std::function<bool()>& current) {
    void* lib=R::FindClassDefaultObject(L"WidgetBlueprintLibrary");
    void* cls=object_index::ClassByName(L"ui_madnessCombat_C");
    if(!lib || !cls || !current())return false;
    if(!view.widget.Get()) {
        ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(lib),L"Create"));
        void* ctx=engine::GetWorldContext();void* owner=nullptr;
        if(!ctx || !f.valid() || !f.Set(L"WorldContextObject",ctx) || !f.Set(L"WidgetType",cls) ||
           !f.Set(L"OwningPlayer",owner) || !current() || !Call(lib,f))return false;
        view.widget.Set(f.Get<void*>(L"ReturnValue"));view.kills=-1;view.shown=false;
    }
    void* widget=view.widget.Get();
    if(!view.widget.Get() || !current())return false;
    ParamFrame show(R::FindDispatchFunctionCached(R::ClassOf(widget),L"AddToViewport"));
    if(!show.valid() || !show.Set<int32_t>(L"ZOrder",0) || !current() || !view.widget.Get() || !Call(widget,show))return false;
    view.shown=current() && view.widget.Get();return view.shown;
}
bool StartMusic(View& view,float elapsed,const std::function<bool()>& current) {
    void* lib=R::FindClassDefaultObject(L"GameplayStatics");
    void* sound=engine::FindSound(L"SoundWave /Game/audio/music/MC_music.MC_music");
    if(!lib || !sound || !current())return false;
    ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(lib),L"SpawnSound2D"));
    void* ctx=engine::GetWorldContext();void* concurrency=nullptr;
    if(!ctx || !f.valid() || !f.Set(L"WorldContextObject",ctx) || !f.Set(L"Sound",sound) ||
       !f.Set<float>(L"VolumeMultiplier",.5f) || !f.Set<float>(L"PitchMultiplier",1.f) ||
       !f.Set<float>(L"StartTime",elapsed) || !f.Set(L"ConcurrencySettings",concurrency) ||
       !f.Set<bool>(L"bPersistAcrossLevelTransition",false) || !f.Set<bool>(L"bAutoDestroy",true) || !current() || !Call(lib,f))return false;
    view.audio.Set(f.Get<void*>(L"ReturnValue"));view.started=view.audio.Get()!=nullptr;
    return current() && view.started;
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
    CachedObjRef live;live.Set(actor);const auto world=world_identity::Generation();State state{};state.active=true;
    void* hud=Object(actor,L"killcount");state.hud=hud!=nullptr;
    if(hud && (!ReadField(hud,L"count",state.kills) || state.kills<0))return false;
    void* audio=Object(actor,L"MC_music");CachedObjRef voice;voice.Set(audio);
    const auto valid=[&]{return live.Get() && voice.Get() && world==world_identity::Generation() && Object(actor,L"MC_music")==audio;};
    if(!valid())return false;
    ParamFrame playing(R::FindDispatchFunctionCached(R::ClassOf(audio),L"IsPlaying"));
    if(!playing.valid() || !valid() || !Call(audio,playing) || !valid() ||
       !playing.GetRaw(L"ReturnValue",&state.playing,sizeof(state.playing)))return false;
    ParamFrame age(R::FindDispatchFunctionCached(R::ClassOf(actor),L"GetGameTimeSinceCreation"));
    if(!age.valid() || !valid() || !Call(actor,age) || !valid() ||
       !age.GetRaw(L"ReturnValue",&state.elapsed,sizeof(state.elapsed)))return false;
    if(!std::isfinite(state.elapsed) || state.elapsed<0)return false;
    out=state;return true;
}
void Clear(View& view) {
    const auto old=view;view={};
    if(old.world==world_identity::Generation())if(void* audio=old.audio.Get())Verb(audio,L"Stop");
    // Owned UMG widgets can outlive the level through their game instance. Remove
    // that exact widget even after travel; audio remains scoped to its old world.
    if(void* widget=old.widget.Get())Verb(widget,L"RemoveFromParent");
}
bool Apply(View& view,const State& state,const std::function<bool()>& current) {
    if(current && !current())return false;
    const auto world=world_identity::Generation();
    if(view.world!=world){Clear(view);if(world!=world_identity::Generation() || (current && !current()))return false;view.world=world;}
    const auto valid=[&]{return world==world_identity::Generation() && (!current || current());};
    if(state.kills<0 || !std::isfinite(state.elapsed) || state.elapsed<0 || state.elapsed>86400)return false;
    if(!state.active){Clear(view);return true;}
    bool ok=true;
    if(state.playing && view.started) {
        void* audio=view.audio.Get();
        if(!audio)view.started=false;
        else {
            ParamFrame f(R::FindDispatchFunctionCached(R::ClassOf(audio),L"IsPlaying"));
            const auto before=view.audio;bool playing=false;
            if(!f.valid() || !valid() || !before.Get() || !Call(audio,f) || !valid() || !before.Get())return false;
            if(!f.GetRaw(L"ReturnValue",&playing,sizeof(playing)))ok=false;
            else if(!playing){view.audio={};view.started=false;}
        }
    }
    if(state.playing && !view.started) {
        if(!StartMusic(view,state.elapsed,valid))ok=false;
    } else if(!state.playing) {
        auto old=view.audio;view.audio={};view.started=false;
        if(void* audio=old.Get())Verb(audio,L"Stop");
    }
    if(!valid())return false;
    if(state.hud) {
        if((!view.widget.Get() || !view.shown) && !MakeWidget(view,valid))return false;
        if(!valid())return false;
        void* widget=view.widget.Get();if(!widget)return false;
        if(view.kills!=state.kills) {
            const auto text=L"KILLS: "+std::to_wstring(state.kills);
            const auto before=view.widget;CachedObjRef label;label.Set(Object(widget,L"text_count"));
            if(!label.Get() || !valid() || !before.Get() || !component_calls::SetText(label.Get(),text.c_str()) ||
               !valid() || !before.Get() || !label.Get())return false;
            // Native addCount also calls master.killed; update the label directly.
            view.kills=state.kills;
        }
    } else {
        auto old=view.widget;view.widget={};view.kills=-1;view.shown=false;
        if(void* widget=old.Get())Verb(widget,L"RemoveFromParent");
    }
    return ok && valid();
}
}
