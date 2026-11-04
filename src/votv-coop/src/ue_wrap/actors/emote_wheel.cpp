#include "ue_wrap/actors/emote_wheel.h"
#include "ue_wrap/core/asset_load.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/types.h"
#include "ue_wrap/engine/engine_audio.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>

UE_FACT_FN(kEmotePlay, "emote_wheel", L"AnimInstance", L"PlaySlotAnimationAsDynamicMontage", Declared);
UE_FACT_FN(kEmoteSound, "emote_wheel", L"GameplayStatics", L"SpawnSound2D", Declared);

namespace ue_wrap::emote_wheel {
namespace {
namespace R=reflection;
CachedObjRef g_local, g_instance, g_montage, g_audio;
State g_source{};
uint32_t g_episode=0, g_world=0;
uint64_t g_musicAt=0;
float g_musicStart=0;
bool g_playHook=false,g_soundHook=false;
std::array<CachedObjRef,kPartCount+1> g_anims{},g_sounds{};
std::array<bool,kPartCount+1> g_missingAnim{},g_missingSound{};
CachedObjRef g_attenuation;
bool g_checkedPak=false,g_hasPak=false;
uint64_t Now(){using namespace std::chrono;return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();}
uint32_t Next(){if(!++g_episode)++g_episode;return g_episode;}

template<class T>bool Field(void* object,const wchar_t* name,T& value){
    if(!object || !R::IsLive(object))return false;
    const int offset=R::FindPropertyOffset(R::ClassOf(object),name);
    if(offset<0)return false;
    std::memcpy(&value,static_cast<uint8_t*>(object)+offset,sizeof(value));return true;
}
void* Pointer(void* object,const wchar_t* name){void* value=nullptr;return Field(object,name,value) && value && R::IsLive(value)?value:nullptr;}
bool Flag(void* object,const wchar_t* name){
    int32_t offset=-1;uint8_t mask=0;
    return object && R::IsLive(object) && R::FindBoolProperty(R::ClassOf(object),name,offset,mask) &&
        offset>=0 && mask && (static_cast<uint8_t*>(object)[offset]&mask)!=0;
}
void* Function(void* object,const wchar_t* verb){return object && R::IsLive(object)?R::FindDispatchFunctionCached(R::ClassOf(object),verb):nullptr;}
template<class T>bool Param(void* fn,void* params,const wchar_t* name,T& out){
    if(!fn || !params)return false;
    const int offset=R::FindParamOffset(fn,name),size=R::FunctionFrameSize(fn);
    if(offset<0 || size<int(sizeof(T)) || offset>size-int(sizeof(T)))return false;
    for(const auto& p:R::FunctionParams(fn))if(!_wcsicmp(p.name.c_str(),name)){
        if(p.size!=sizeof(T))return false;std::memcpy(&out,static_cast<uint8_t*>(params)+offset,sizeof(T));return true;
    }
    return false;
}
uint16_t Identify(void* asset,bool sound){
    if(!asset || !R::IsLive(asset))return 0;
    const auto name=R::ToString(R::NameOf(asset));
    void* outer=R::OuterOf(asset);
    if(!outer || !R::IsLive(outer) || R::ToString(R::NameOf(outer))!=L"/Game/Mods/EmoteWheel/"+name)return 0;
    if(R::ClassNameOf(asset)!=(sound?L"SoundWave":L"AnimSequence"))return 0;
    for(size_t i=0;i<kPartCount;++i)if(sound?kParts[i].sound && name==kParts[i].sound:name==kParts[i].animation)return uint16_t(i+1);
    return 0;
}
void* Instance(void* pawn,const wchar_t* field){
    CachedObjRef component;component.Set(Pointer(pawn,field));
    if(!component.Get())return nullptr;ParamFrame f(Function(component.Get(),L"GetAnimInstance"));
    if(!f.valid() || !Call(component.Get(),f) || !component.Get())return nullptr;
    return f.Get<void*>(L"ReturnValue");
}
bool Playing(void* object,void* montage=nullptr){
    if(!object)return false;CachedObjRef live;live.Set(object);
    ParamFrame f(Function(object,montage?L"Montage_IsPlaying":L"IsPlaying"));
    if(!f.valid() || (montage && !f.Set(L"Montage",montage)) || !live.Get() || !Call(live.Get(),f) || !live.Get())return false;
    return f.Get<bool>(L"ReturnValue");
}
void OnPlay(void* self,void* fn,void* params){
    if(!game_thread::IsGameThread() || !g_local.Get() || !g_instance.Is(self) || g_world!=world_identity::Generation())return;
    void* asset=nullptr;void* montage=nullptr;
    if(!Param(fn,params,L"Asset",asset) || !Param(fn,params,L"ReturnValue",montage) || !montage || !R::IsLive(montage))return;
    const auto part=Identify(asset,false);if(!part)return;
    g_montage.Set(montage);g_source.part=part;g_source.episode=Next();
}
void OnSound(void*,void* fn,void* params){
    if(!game_thread::IsGameThread() || !g_local.Get() || g_world!=world_identity::Generation())return;
    void* context=nullptr;void* sound=nullptr;void* audio=nullptr;float start=0;
    if(!Param(fn,params,L"WorldContextObject",context) || !g_local.Is(context) ||
        !Param(fn,params,L"Sound",sound) || !Param(fn,params,L"ReturnValue",audio) || !audio || !R::IsLive(audio))return;
    const auto part=Identify(sound,true);if(!part)return;
    Param(fn,params,L"StartTime",start);if(!std::isfinite(start) || start<0)return;
    g_audio.Set(audio);g_source.music=part;g_source.musicEpisode=Next();g_musicAt=Now();g_musicStart=start;
}
float Position(void* instance,void* montage){
    CachedObjRef live;live.Set(instance);ParamFrame f(Function(instance,L"Montage_GetPosition"));
    if(!f.valid() || !f.Set(L"Montage",montage) || !live.Get() || !Call(live.Get(),f) || !live.Get())return -1;
    return f.Get<float>(L"ReturnValue");
}
bool Seek(void* instance,void* montage,float position){
    CachedObjRef live;live.Set(instance);ParamFrame f(Function(instance,L"Montage_SetPosition"));
    return f.valid() && f.Set(L"Montage",montage) && f.Set(L"NewPosition",position) && live.Get() && Call(live.Get(),f) && live.Get();
}
void StopMontage(const CachedObjRef& instance,const CachedObjRef& montage){
    if(!instance.Get() || !montage.Get())return;
    ParamFrame f(Function(instance.Get(),L"Montage_Stop"));
    if(f.valid() && f.Set(L"Montage",montage.Get()) && f.Set(L"InBlendOutTime",.2f) && instance.Get() && montage.Get())Call(instance.Get(),f);
}
void StopAudio(CachedObjRef audio){
    if(!audio.Get())return;ParamFrame f(Function(audio.Get(),L"Stop"));if(f.valid() && audio.Get())Call(audio.Get(),f);
}
void* Asset(uint16_t id,bool sound){
    const auto* p=PartAt(id);if(!p || (sound && !p->sound))return nullptr;
    auto& cached=sound?g_sounds[id]:g_anims[id];auto& missing=sound?g_missingSound[id]:g_missingAnim[id];
    if(cached.Get())return cached.Get();if(missing)return nullptr;
    const std::wstring name=sound?p->sound:p->animation;
    void* loaded=asset_load::LoadObjectByPath((L"/Game/Mods/EmoteWheel/"+name+L"."+name).c_str());
    if(Identify(loaded,sound)!=id){missing=true;return nullptr;}
    cached.Set(loaded);return cached.Get();
}
bool Available(){
    if(!g_checkedPak){g_checkedPak=true;g_hasPak=Asset(1,false)!=nullptr;}
    return g_hasPak;
}
float MusicPosition(void* sound,float position){
    float duration=0;if(Field(sound,L"Duration",duration) && std::isfinite(duration) && duration>.01f && Flag(sound,L"bLooping"))return std::fmod(position,duration);
    return position;
}
}

bool Eligible(void* pawn){
    return pawn && R::IsLive(pawn) && !Flag(pawn,L"isRagdoll") && !Flag(pawn,L"dead") && !Flag(pawn,L"sleeping") &&
        !Flag(pawn,L"inWater") && !Pointer(pawn,L"sittingOn") && !Pointer(pawn,L"atv");
}
void ResetObservation(){g_local.Reset();g_instance.Reset();g_montage.Reset();g_audio.Reset();g_source={};}
void Observe(void* pawn){
    const auto world=world_identity::Generation();
    if(world!=g_world){ResetObservation();g_world=world;g_anims={};g_sounds={};g_missingAnim={};g_missingSound={};g_checkedPak=g_hasPak=false;}
    if(!g_local.Is(pawn)){ResetObservation();if(pawn)g_local.Set(pawn);}
    if(!g_local.Get())return;
    // EmoteWheel carries its orbit-camera items on Mesh. The first-person
    // mesh_playerVisible can stop ticking while the orbit camera is active.
    void* inst=Instance(pawn,L"Mesh");if(!inst)inst=Instance(pawn,L"mesh_playerVisible");
    if(!g_instance.Is(inst)){g_instance.Set(inst);g_montage.Reset();g_source.part=0;g_source.episode=0;}
    if(!g_playHook)g_playHook=game_thread::RegisterPostObserver(facts::Resolve(kEmotePlay),OnPlay);
    if(!g_soundHook)g_soundHook=game_thread::RegisterPostObserver(facts::Resolve(kEmoteSound),OnSound);
}
State Sample(){
    State value=g_source;
    if(!g_local.Get() || !Eligible(g_local.Get()))return {};
    if(!g_instance.Get() || !g_montage.Get() || !Playing(g_instance.Get(),g_montage.Get())){value.part=0;value.episode=0;}
    else {
        value.position=Position(g_instance.Get(),g_montage.Get());
        const auto* part=PartAt(value.part);
        if(!part || !std::isfinite(value.position) || value.position<0){value.part=0;value.episode=0;value.position=0;}
        else if(part->loop)value.position=std::fmod(value.position,part->duration);
    }
    if(!g_audio.Get() || !Playing(g_audio.Get())){value.music=0;value.musicEpisode=0;}
    else value.musicPosition=g_musicStart+float(Now()-g_musicAt)/1000.f;
    return Valid(value)?value:State{};
}
void Stop(Playback& playback){
    const auto old=playback;playback={};
    if(old.world!=world_identity::Generation())return;
    for(size_t i=0;i<2;++i)StopMontage(old.instances[i],old.montages[i]);
    StopAudio(old.audio);
}
bool Apply(void* pawn,const State& input,Playback& playback,const std::function<bool()>& current){
    const State state=input;if(!Valid(state) || !current() || !Eligible(pawn))return false;
    CachedObjRef body;body.Set(pawn);const auto world=world_identity::Generation();
    const auto valid=[&]{return world==world_identity::Generation() && body.Get() && current() && body.Get();};
    if(!playback.actor.Is(pawn) || playback.world!=world){Stop(playback);if(!valid())return false;playback.actor=body;playback.world=world;}
    if((state.part || state.music) && !Available())return false;
    if(!valid())return false;
    bool complete=true;
    const wchar_t* names[]={L"Mesh",L"mesh_playerVisible"};
    for(size_t i=0;i<2;++i){
        CachedObjRef inst;inst.Set(Instance(pawn,names[i]));if(!valid())return false;
        if(!inst.Get()){if(Pointer(pawn,names[i]))complete=false;continue;}
        const bool restart=playback.episodes[i]!=state.episode || playback.parts[i]!=state.part || !playback.instances[i].Is(inst.Get()) || (state.part && !playback.montages[i].Get());
        if(restart){
            StopMontage(playback.instances[i],playback.montages[i]);if(!valid())return false;
            playback.instances[i]=inst;playback.montages[i].Reset();
            if(state.part){
                void* sequence=Asset(state.part,false);if(!valid())return false;
                if(!sequence){complete=false;continue;}
                const auto slot=fname_utils::StringToFName(L"DefaultSlot");if(!valid() || !inst.Get())return false;
                ParamFrame f(Function(inst.Get(),L"PlaySlotAnimationAsDynamicMontage"));
                const auto* part=PartAt(state.part);
                if(!f.valid() || !f.Set(L"Asset",sequence) || !f.Set(L"SlotNodeName",slot) || !f.Set(L"BlendInTime",.1f) ||
                    !f.Set(L"BlendOutTime",part->loop?.0f:.25f) || !f.Set(L"InPlayRate",1.f) ||
                    !f.Set(L"LoopCount",part->loop?1000000:1) || !f.Set(L"BlendOutTriggerTime",-1.f) ||
                    !f.Set(L"InTimeToStartMontageAt",state.position) || !valid() || !inst.Get() || !Call(inst.Get(),f)){
                    complete=false;continue;
                }
                playback.montages[i].Set(f.Get<void*>(L"ReturnValue"));
                if(!valid() || !inst.Get())return false;
                if(!playback.montages[i].Get())complete=false;
            }
            if(!state.part || playback.montages[i].Get()){playback.episodes[i]=state.episode;playback.parts[i]=state.part;}
        }else if(state.part){
            const float position=Position(inst.Get(),playback.montages[i].Get());if(!valid())return false;
            const auto* part=PartAt(state.part);float drift=std::abs(position-state.position);
            if(part->loop)drift=std::min(drift,std::abs(part->duration-drift));
            if(drift>.3f && !Seek(inst.Get(),playback.montages[i].Get(),state.position))complete=false;
            if(!valid())return false;
        }
    }
    if(complete){
        playback.state.part=state.part;playback.state.episode=state.episode;playback.state.position=state.position;
    }
    const float musicDrift=std::abs(state.musicPosition-(playback.musicStart+float(Now()-playback.musicAt)/1000.f));
    if(state.music!=playback.state.music || state.musicEpisode!=playback.state.musicEpisode ||
       (state.music && (!playback.audio.Get() || musicDrift>.3f))){
        StopAudio(playback.audio);playback.audio.Reset();if(!valid())return false;
        if(state.music){
            void* sound=Asset(state.music,true);if(!valid())return false;
            if(!sound)complete=false;
            else {
                CachedObjRef root;root.Set(Pointer(pawn,L"RootComponent"));
                CachedObjRef gs;gs.Set(R::FindClassDefaultObject(L"GameplayStatics"));
                if(!g_attenuation.Get()){
                    engine::SoundAttenuationConfig config;config.extents[0]=150;config.falloffDistance=2200;
                    g_attenuation.Set(engine::SpawnSoundAttenuation(config));
                }
                if(!valid() || !root.Get() || !gs.Get())return false;
                ParamFrame f(Function(gs.Get(),L"SpawnSoundAttached"));
                const float position=MusicPosition(sound,state.musicPosition);
                if(!f.valid() || !g_attenuation.Get() || !f.Set(L"Sound",sound) || !f.Set(L"AttachToComponent",root.Get()) ||
                    !f.Set(L"Location",FVector{}) || !f.Set(L"LocationType",uint8_t(0)) || !f.Set(L"bStopWhenAttachedToDestroyed",true) ||
                    !f.Set(L"VolumeMultiplier",.6f) || !f.Set(L"PitchMultiplier",1.f) || !f.Set(L"StartTime",position) ||
                    !f.Set(L"AttenuationSettings",g_attenuation.Get()) || !f.Set(L"bAutoDestroy",true) ||
                    !valid() || !root.Get() || !gs.Get() || !Call(gs.Get(),f))complete=false;
                else {playback.audio.Set(f.Get<void*>(L"ReturnValue"));if(!valid())return false;if(!playback.audio.Get())complete=false;}
            }
        }
        // An unavailable animation must not restart successfully playing music
        // on every retry; the two presentation channels commit independently.
        if(!state.music || playback.audio.Get()){
            playback.state.music=state.music;playback.state.musicEpisode=state.musicEpisode;
            playback.musicAt=Now();playback.musicStart=state.musicPosition;
        }
    }
    if(!valid())return false;
    if(complete)playback.state=state;
    return complete;
}

namespace {
const wchar_t* Bone(uint8_t bone){return bone==1?L"hand_L":bone==2?L"hand_R":L"head";}
bool Transform(void* component,FTransform& out,const std::function<bool()>& current){
    CachedObjRef live;live.Set(component);if(!live.Get())return false;
    ParamFrame f(Function(live.Get(),L"K2_GetComponentToWorld"));
    return f.valid() && current() && live.Get() && Call(live.Get(),f) && current() && live.Get() && f.GetRaw(L"ReturnValue",&out,sizeof(out));
}
bool BoneTransform(void* mesh,uint8_t bone,FTransform& out,const std::function<bool()>& current){
    CachedObjRef live;live.Set(mesh);if(!live.Get())return false;
    const auto name=fname_utils::StringToFName(Bone(bone));if(!current() || !live.Get())return false;
    ParamFrame f(Function(live.Get(),L"GetSocketTransform"));
    return f.valid() && f.Set(L"InSocketName",name) && f.Set(L"TransformSpace",uint8_t(0)) && current() && live.Get() &&
        Call(live.Get(),f) && current() && live.Get() && f.GetRaw(L"ReturnValue",&out,sizeof(out));
}
bool MathTransform(const wchar_t* verb,const FTransform& a,const FTransform* b,FTransform& out,const std::function<bool()>& current){
    CachedObjRef library;library.Set(R::FindClassDefaultObject(L"KismetMathLibrary"));if(!library.Get())return false;
    ParamFrame f(Function(library.Get(),verb));
    return f.valid() && f.Set(b?L"A":L"T",a) && (!b || f.Set(!wcscmp(verb,L"MakeRelativeTransform")?L"RelativeTo":L"B",*b)) &&
        current() && library.Get() && Call(library.Get(),f) && current() && library.Get() && f.GetRaw(L"ReturnValue",&out,sizeof(out));
}
}
Hand CaptureHand(void* pawn,void* item,uint32_t generation){
    if(g_world!=world_identity::Generation() || !generation || !g_local.Is(pawn) || !g_local.Get() || !item || !R::IsLive(item))return {};
    CachedObjRef actor,mesh,visual;actor.Set(item);mesh.Set(Pointer(pawn,L"Mesh"));visual.Set(Pointer(item,L"StaticMesh"));
    const auto world=world_identity::Generation();
    const auto current=[&]{return world==world_identity::Generation() && g_local.Get() && g_local.Is(pawn) && actor.Get() && mesh.Get() && visual.Get() &&
        mesh.Is(Pointer(pawn,L"Mesh")) && visual.Is(Pointer(item,L"StaticMesh"));};
    FTransform shown{},best{};if(!current() || !Transform(visual.Get(),shown,current))return {};
    uint8_t selected=0;float distance=1.e20f;
    for(uint8_t bone=1;bone<=3;++bone){
        FTransform candidate{};if(!BoneTransform(mesh.Get(),bone,candidate,current))return {};
        const float dx=shown.TX-candidate.TX,dy=shown.TY-candidate.TY,dz=shown.TZ-candidate.TZ;
        const float d=dx*dx+dy*dy+dz*dz;if(d<distance){distance=d;selected=bone;best=candidate;}
    }
    FTransform relative{};if(!selected || !MathTransform(L"MakeRelativeTransform",shown,&best,relative,current))return {};
    Hand hand{};hand.generation=generation;hand.bone=selected;
    const float pose[]={relative.TX,relative.TY,relative.TZ,relative.RotX,relative.RotY,relative.RotZ,relative.RotW};
    std::copy_n(pose,7,hand.pose);return current() && Valid(hand)?hand:Hand{};
}
bool PlaceHand(void* pawn,void* item,const Hand& input,const std::function<bool()>& permitted){
    const Hand hand=input;if(!Valid(hand) || !hand.generation || !permitted())return false;
    CachedObjRef actor,body,mesh,visual,root;actor.Set(item);body.Set(pawn);
    if(!actor.Get() || !body.Get())return false;
    mesh.Set(Pointer(pawn,L"Mesh"));visual.Set(Pointer(item,L"StaticMesh"));root.Set(Pointer(item,L"RootComponent"));
    const auto world=world_identity::Generation();
    const auto current=[&]{return world==world_identity::Generation() && body.Get() && actor.Get() && mesh.Get() && visual.Get() && root.Get() &&
        permitted() && body.Get() && actor.Get() && mesh.Get() && visual.Get() && root.Get() &&
        mesh.Is(Pointer(pawn,L"Mesh")) && visual.Is(Pointer(item,L"StaticMesh")) && root.Is(Pointer(item,L"RootComponent"));};
    if(!current())return false;
    FTransform bone{},shown{},base{};
    if(!BoneTransform(mesh.Get(),hand.bone,bone,current) || !Transform(visual.Get(),shown,current) || !Transform(root.Get(),base,current))return false;
    FTransform relative{},inverse{},local{},target{},actorTarget{};
    local.TX=hand.pose[0];local.TY=hand.pose[1];local.TZ=hand.pose[2];
    local.RotX=hand.pose[3];local.RotY=hand.pose[4];local.RotZ=hand.pose[5];local.RotW=hand.pose[6];
    if(!MathTransform(L"MakeRelativeTransform",shown,&base,relative,current) ||
       !MathTransform(L"InvertTransform",relative,nullptr,inverse,current) ||
       !MathTransform(L"ComposeTransforms",local,&bone,target,current))return false;
    target.SX=shown.SX;target.SY=shown.SY;target.SZ=shown.SZ;
    // Move the display actor so its mesh reaches the bone pose. Its original
    // component-relative transform stays intact for the ordinary hold driver.
    if(!MathTransform(L"ComposeTransforms",inverse,&target,actorTarget,current))return false;
    ParamFrame f(Function(actor.Get(),L"K2_SetActorTransform"));
    return f.valid() && f.Set(L"NewTransform",actorTarget) && f.Set(L"bSweep",false) && f.Set(L"bTeleport",true) &&
        current() && actor.Get() && Call(actor.Get(),f) && current() && f.Get<bool>(L"ReturnValue");
}
}
