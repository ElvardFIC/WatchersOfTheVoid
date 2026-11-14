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
uint64_t g_viewRevision=0;
struct Live {
    CachedObjRef object,cls;uint32_t generation=0;void* world=nullptr;
    explicit Live(void* p=nullptr){
        if(p&&R::IsLive(p)){object.Set(p);cls.Set(R::ClassOf(p));}
        generation=world_identity::Generation();world=world_identity::CurrentWorld();
    }
    bool Current()const{return object.Get()&&cls.Get()&&world&&generation==world_identity::Generation()&&
        world==world_identity::CurrentWorld()&&R::ClassOf(object.Get())==cls.Get();}
};
struct Function {
    Live owner;CachedObjRef fn;std::wstring name;
    Function(void* object,const wchar_t* verb):owner(object),name(verb){
        if(owner.Current())fn.Set(R::FindDispatchFunctionCached(owner.cls.Get(),verb));
    }
    bool Current()const{return owner.Current()&&fn.Get()&&R::FindDispatchFunctionCached(owner.cls.Get(),name.c_str())==fn.Get();}
};
bool Typed(void* p,void* base){return p&&base&&R::IsLive(p)&&R::IsLive(base)&&R::IsDescendantOfAny(R::ClassOf(p),&base,1);}
template<class T>bool ReadField(void* p,const wchar_t* name,T& out){
    const Live source(p);if(!source.Current())return false;
    const int offset=R::FindPropertyOffset(source.cls.Get(),name);
    if(offset<0||!source.Current())return false;
    T value{};std::memcpy(&value,static_cast<uint8_t*>(p)+offset,sizeof(value));
    if(!source.Current()||R::FindPropertyOffset(source.cls.Get(),name)!=offset)return false;
    out=value;return true;
}
void* Object(void* p,const wchar_t* name){void* result=nullptr;return ReadField(p,name,result)&&result&&R::IsLive(result)?result:nullptr;}
bool Owned(const CachedObjRef& object,const CachedObjRef& cls){return object.Get()&&cls.Get()&&R::ClassOf(object.Get())==cls.Get();}
bool Verb(const CachedObjRef& object,const CachedObjRef& cls,const wchar_t* name){
    if(!Owned(object,cls))return !object.Get();
    Function function(object.Get(),name);if(!function.Current())return false;
    ParamFrame frame(function.fn.Get());
    return frame.valid()&&Owned(object,cls)&&function.Current()&&Call(object.Get(),frame)&&
        Owned(object,cls)&&function.Current();
}
bool Empty(const View& v){return !v.audio.Raw()&&!v.widget.Raw();}
bool ReturnSlot(const ParamFrame& frame,int32_t bytes){
    const int offset=frame.ParamOffset(L"ReturnValue"),size=frame.FrameSize();
    if(!frame.valid()||offset<0||bytes<=0||size<bytes||offset>size-bytes)return false;
    for(const auto& param:R::FunctionParams(frame.function()))
        if(!_wcsicmp(param.name.c_str(),L"ReturnValue"))return param.offset==offset&&param.size==bytes;
    return false;
}
bool QueryPlaying(const CachedObjRef& audio,const CachedObjRef& cls,bool& result,const std::function<bool()>& current){
    if(!Owned(audio,cls))return false;
    Function function(audio.Get(),L"IsPlaying");if(!function.Current()||!current())return false;
    ParamFrame frame(function.fn.Get());bool playing=false;
    if(!function.Current()||!ReturnSlot(frame,sizeof(playing))||!current()||!Call(audio.Get(),frame)||!function.Current()||!current()||
       !frame.GetRaw(L"ReturnValue",&playing,sizeof(playing)))return false;
    result=playing;return true;
}
bool MakeWidget(View& view,const std::function<bool()>& current){
    if(!view.widget.Get()){
        Live library(R::FindClassDefaultObject(L"WidgetBlueprintLibrary"));
        CachedObjRef type;type.Set(object_index::ClassByName(L"ui_madnessCombat_C"));
        Live context(engine::GetWorldContext());
        if(!library.Current()||!type.Get()||!context.Current()||!current())return false;
        Function create(library.object.Get(),L"Create");
        const auto ready=[&]{return current()&&library.Current()&&type.Get()&&context.Current()&&create.Current()&&
            R::FindClassDefaultObject(L"WidgetBlueprintLibrary")==library.object.Get()&&
            object_index::ClassByName(L"ui_madnessCombat_C")==type.Get()&&engine::GetWorldContext()==context.object.Get();};
        if(!ready())return false;
        ParamFrame frame(create.fn.Get());void* owner=nullptr;
        if(!create.Current()||!ReturnSlot(frame,sizeof(void*))||!frame.Set(L"WorldContextObject",context.object.Get())||!frame.Set(L"WidgetType",type.Get())||
           !frame.Set(L"OwningPlayer",owner)||!ready())return false;
        const bool called=Call(library.object.Get(),frame);
        void* result=nullptr;
        if(!frame.GetRaw(L"ReturnValue",&result,sizeof(result))||!Typed(result,type.Get()))return false;
        Live candidate(result);
        if(!ready()||!candidate.Current()){
            if(candidate.Current()&&!view.widget.Is(result))Verb(candidate.object,candidate.cls,L"RemoveFromParent");
            return false;
        }
        view.widget=candidate.object;view.widgetClass=candidate.cls;view.kills=-1;view.shown=false;
        if(!called)return false;
    }
    if(view.shown)return current()&&Owned(view.widget,view.widgetClass);
    const auto widget=view.widget,cls=view.widgetClass;
    Function show(widget.Get(),L"AddToViewport");if(!show.Current()||!current())return false;
    ParamFrame frame(show.fn.Get());
    if(!frame.valid()||!frame.Set<int32_t>(L"ZOrder",0)||!show.Current()||!current()||!Owned(widget,cls)||
       !view.widget.Is(widget.Get())||!Call(widget.Get(),frame)||!show.Current()||!current()||
       !Owned(widget,cls)||!view.widget.Is(widget.Get()))return false;
    view.shown=true;return true;
}
bool StartMusic(View& view,float elapsed,const std::function<bool()>& current){
    Live library(R::FindClassDefaultObject(L"GameplayStatics"));
    Live sound(engine::FindSound(L"SoundWave /Game/audio/music/MC_music.MC_music"));
    Live context(engine::GetWorldContext());CachedObjRef type;type.Set(R::FindClass(L"AudioComponent"));
    if(!library.Current()||!sound.Current()||!context.Current()||!type.Get()||!current())return false;
    Function spawn(library.object.Get(),L"SpawnSound2D");
    const auto ready=[&]{return current()&&library.Current()&&sound.Current()&&context.Current()&&type.Get()&&spawn.Current()&&
        R::FindClassDefaultObject(L"GameplayStatics")==library.object.Get()&&engine::GetWorldContext()==context.object.Get()&&
        R::FindClass(L"AudioComponent")==type.Get();};
    if(!ready())return false;
    ParamFrame frame(spawn.fn.Get());void* concurrency=nullptr;
    if(!spawn.Current()||!ReturnSlot(frame,sizeof(void*))||!frame.Set(L"WorldContextObject",context.object.Get())||!frame.Set(L"Sound",sound.object.Get())||
       !frame.Set<float>(L"VolumeMultiplier",.5f)||!frame.Set<float>(L"PitchMultiplier",1.f)||!frame.Set<float>(L"StartTime",elapsed)||
       !frame.Set(L"ConcurrencySettings",concurrency)||!frame.Set<bool>(L"bPersistAcrossLevelTransition",false)||
       !frame.Set<bool>(L"bAutoDestroy",true)||!ready())return false;
    const bool called=Call(library.object.Get(),frame);void* result=nullptr;
    if(!frame.GetRaw(L"ReturnValue",&result,sizeof(result))||!Typed(result,type.Get()))return false;
    Live candidate(result);
    if(!ready()||!candidate.Current()){
        if(candidate.Current()&&!view.audio.Is(result))Verb(candidate.object,candidate.cls,L"Stop");
        return false;
    }
    // A typed returned component is owned even if native completion was uncertain.
    view.audio=candidate.object;view.audioClass=candidate.cls;view.started=true;
    return called;
}
}
void* FindHost(){
    CachedObjRef cls;cls.Set(object_index::ClassByName(L"madnessCombatMaster_C"));if(!cls.Get())return nullptr;
    struct Search{CachedObjRef cls,actor;uint32_t generation;void* world;};
    Search search{cls,{},world_identity::Generation(),world_identity::CurrentWorld()};
    object_index::ForEachInstance(cls.Get(),[](void* data,void* actor,int32_t){
        auto& s=*static_cast<Search*>(data);
        if(!s.actor.Get()&&s.cls.Get()&&R::IsLive(actor)&&s.world&&s.generation==world_identity::Generation()&&
           s.world==world_identity::CurrentWorld()&&world_identity::WorldOf(actor)==s.world&&Typed(actor,s.cls.Get()))s.actor.Set(actor);
    },&search);
    return search.generation==world_identity::Generation()&&search.world==world_identity::CurrentWorld()?search.actor.Get():nullptr;
}
bool Read(void* actor,State& out){
    Live master(actor);CachedObjRef base;base.Set(object_index::ClassByName(L"madnessCombatMaster_C"));
    if(!master.Current()||!Typed(actor,base.Get())||world_identity::WorldOf(actor)!=master.world)return false;
    void* hud=nullptr;void* audio=nullptr;
    if(!ReadField(actor,L"killcount",hud)||!ReadField(actor,L"MC_music",audio))return false;
    Live ui(hud),voice(audio);State state{};state.active=true;state.hud=ui.Current();
    const auto valid=[&]{void* currentHud=nullptr;void* currentAudio=nullptr;
        return master.Current()&&base.Get()&&Typed(actor,base.Get())&&ReadField(actor,L"killcount",currentHud)&&
            ReadField(actor,L"MC_music",currentAudio)&&currentHud==hud&&currentAudio==audio&&
            (!hud||ui.Current())&&(!audio||voice.Current());};
    if(state.hud&&(!ReadField(hud,L"count",state.kills)||state.kills<0))return false;
    if(!valid())return false;
    if(audio&&!QueryPlaying(voice.object,voice.cls,state.playing,valid))return false;
    Function age(actor,L"GetGameTimeSinceCreation");if(!age.Current()||!valid())return false;
    ParamFrame frame(age.fn.Get());
    if(!age.Current()||!ReturnSlot(frame,sizeof(state.elapsed))||!valid()||!Call(actor,frame)||!age.Current()||!valid()||
       !frame.GetRaw(L"ReturnValue",&state.elapsed,sizeof(state.elapsed))||
       !std::isfinite(state.elapsed)||state.elapsed<0||state.elapsed>86400)return false;
    out=state;return true;
}
void Clear(View& view){
    const auto old=view;view={};const uint64_t revision=view.revision=++g_viewRevision;
    const auto audio=old.audio,widget=old.widget;
    if(old.world==world_identity::Generation()&&old.worldObject==world_identity::CurrentWorld()&&Owned(audio,old.audioClass)){
        const bool stopped=Verb(audio,old.audioClass,L"Stop");
        if(!stopped&&view.revision==revision&&Owned(audio,old.audioClass)&&old.world==world_identity::Generation()&&old.worldObject==world_identity::CurrentWorld()){
            view.audio=audio;view.audioClass=old.audioClass;view.started=old.started;view.world=old.world;view.worldObject=old.worldObject;
        }
    }
    // An exact old widget can outlive travel through the game instance. Never remove a fresh nested adoption.
    if(Owned(widget,old.widgetClass)&&!view.widget.Is(widget.Get())){
        const bool removed=Verb(widget,old.widgetClass,L"RemoveFromParent");
        if(!removed&&view.revision==revision&&Owned(widget,old.widgetClass)){
            view.widget=widget;view.widgetClass=old.widgetClass;view.kills=old.kills;view.shown=old.shown;
            view.world=old.world;view.worldObject=old.worldObject;
        }
    }
}
bool Apply(View& view,const State& incoming,const std::function<bool()>& current){
    const State state=incoming;
    if((current&&!current())||state.kills<0||!std::isfinite(state.elapsed)||state.elapsed<0||state.elapsed>86400||
       (!state.active&&(state.hud||state.playing||state.kills)))return false;
    const uint32_t generation=world_identity::Generation();void* const world=world_identity::CurrentWorld();
    if(!world)return false;
    if(!state.active){Clear(view);return Empty(view);}
    if(view.world!=generation||view.worldObject!=world){
        Clear(view);
        if(!Empty(view)||generation!=world_identity::Generation()||world!=world_identity::CurrentWorld()||(current&&!current()))return false;
        view.world=generation;view.worldObject=world;
    }
    const uint64_t revision=view.revision=++g_viewRevision;
    const auto valid=[&]{return view.revision==revision&&generation==world_identity::Generation()&&
        world==world_identity::CurrentWorld()&&(!current||current());};
    bool ok=true;
    if(state.playing&&view.started){
        const auto audio=view.audio,cls=view.audioClass;
        if(!Owned(audio,cls)){view.audio={};view.audioClass={};view.started=false;}
        else{
            bool playing=false;
            if(!QueryPlaying(audio,cls,playing,valid)||!valid()||!view.audio.Is(audio.Get()))return false;
            if(!playing){view.audio={};view.audioClass={};view.started=false;}
        }
    }
    if(state.playing&&!view.started){if(!StartMusic(view,state.elapsed,valid))ok=false;}
    else if(!state.playing&&view.audio.Raw()){
        const auto audio=view.audio,cls=view.audioClass;
        const bool stopped=Verb(audio,cls,L"Stop");
        if(!valid())return false;
        if(stopped||!Owned(audio,cls)){view.audio={};view.audioClass={};view.started=false;}else ok=false;
    }
    if(!valid())return false;
    if(state.hud){
        if((!Owned(view.widget,view.widgetClass)||!view.shown)&&!MakeWidget(view,valid))return false;
        const auto widget=view.widget,cls=view.widgetClass;
        if(!valid()||!Owned(widget,cls))return false;
        if(view.kills!=state.kills){
            const Live label(Object(widget.Get(),L"text_count"));
            const int countOffset=R::FindPropertyOffset(cls.Get(),L"count");
            const auto bound=[&]{return valid()&&Owned(widget,cls)&&view.widget.Is(widget.Get())&&label.Current()&&
                Object(widget.Get(),L"text_count")==label.object.Get()&&R::FindPropertyOffset(cls.Get(),L"count")==countOffset;};
            const auto text=L"KILLS: "+std::to_wstring(state.kills);
            if(countOffset<0||!bound()||!component_calls::SetText(label.object.Get(),text.c_str())||!bound())return false;
            std::memcpy(static_cast<uint8_t*>(widget.Get())+countOffset,&state.kills,sizeof(state.kills));
            view.kills=state.kills;
        }
    }else if(view.widget.Raw()){
        const auto widget=view.widget,cls=view.widgetClass;
        const bool removed=Verb(widget,cls,L"RemoveFromParent");
        if(!valid())return false;
        if(removed||!Owned(widget,cls)){view.widget={};view.widgetClass={};view.kills=-1;view.shown=false;}else ok=false;
    }
    return ok&&valid();
}
}
