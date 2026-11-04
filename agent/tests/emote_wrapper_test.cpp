#ifndef EMOTE_WHEEL_SOURCE
#define EMOTE_WHEEL_SOURCE "../../src/votv-coop/src/ue_wrap/actors/emote_wheel.cpp"
#endif
#include EMOTE_WHEEL_SOURCE
#include <cassert>
#include <cstdio>
#include <deque>
#include <map>
#include <limits>

namespace test {
namespace R=ue_wrap::reflection;
struct Obj {
    int id=0,serial=1;bool live=true,playing=false;uint8_t flags=0;
    R::FName name{};std::wstring text,type=L"Object";
    void* outer=nullptr;void* mesh=nullptr;void* visible=nullptr;void* instance=nullptr;void* root=nullptr;
    void* staticMesh=nullptr;ue_wrap::FTransform transform{};
    float position=0,duration=10;
};
std::deque<Obj> objects;
Obj* Make(std::wstring text=L"object",std::wstring type=L"Object"){
    objects.emplace_back();auto& o=objects.back();o.id=int(objects.size())-1;o.text=text;o.type=type;o.name={o.id,0};return &o;
}
Obj *body,*mesh,*visible,*first,*second,*root,*source,*sound,*audio,*replacement;
std::map<std::wstring,Obj*> functions,assets;
std::map<void*,ue_wrap::ParamFrame::Metadata> metadata;
std::map<std::wstring,ue_wrap::game_thread::ProcessEventObserverFn> observers;
uint32_t world=1;bool installed=true,allowed=true;int loads=0,plays=0,stops=0,seeks=0,sounds=0,audioStops=0;
float lastStart=-1,lastMusic=-1;void* lastStopped=nullptr;
std::function<void()> callback;
void Callback(){auto once=std::move(callback);callback={};if(once)once();}
Obj* Fn(const wchar_t* name){auto& ptr=functions[name];if(!ptr)ptr=Make(name,L"Function");return ptr;}
Obj* Asset(uint16_t id,bool isSound){
    const auto* part=ue_wrap::emote_wheel::PartAt(id);assert(part && (!isSound || part->sound));
    const std::wstring name=isSound?part->sound:part->animation;auto& ptr=assets[name];
    if(!ptr){ptr=Make(name,isSound?L"SoundWave":L"AnimSequence");ptr->outer=Make(L"/Game/Mods/EmoteWheel/"+name,L"Package");}
    return ptr;
}
std::vector<R::ParamInfo> Schema(void* raw){
    std::vector<R::ParamInfo> fields;const auto name=static_cast<Obj*>(raw)->text;
    auto add=[&](const wchar_t* key,int bytes){const int offset=fields.empty()?0:fields.back().offset+fields.back().size;fields.push_back({key,offset,bytes,0x80});};
    if(name==L"GetAnimInstance")add(L"ReturnValue",8);
    else if(name==L"PlaySlotAnimationAsDynamicMontage"){
        add(L"Asset",8);add(L"SlotNodeName",8);add(L"BlendInTime",4);add(L"BlendOutTime",4);add(L"InPlayRate",4);
        add(L"LoopCount",4);add(L"BlendOutTriggerTime",4);add(L"InTimeToStartMontageAt",4);add(L"ReturnValue",8);
    }else if(name==L"Montage_IsPlaying"){add(L"Montage",8);add(L"ReturnValue",1);}
    else if(name==L"Montage_GetPosition"){add(L"Montage",8);add(L"ReturnValue",4);}
    else if(name==L"Montage_SetPosition"){add(L"Montage",8);add(L"NewPosition",4);}
    else if(name==L"Montage_Stop"){add(L"InBlendOutTime",4);add(L"Montage",8);}
    else if(name==L"IsPlaying")add(L"ReturnValue",1);
    else if(name==L"SpawnSound2D"){
        add(L"WorldContextObject",8);add(L"Sound",8);add(L"StartTime",4);add(L"ReturnValue",8);
    }else if(name==L"SpawnSoundAttached"){
        add(L"Sound",8);add(L"AttachToComponent",8);add(L"Location",12);add(L"LocationType",1);
        add(L"bStopWhenAttachedToDestroyed",1);add(L"VolumeMultiplier",4);add(L"PitchMultiplier",4);add(L"StartTime",4);
        add(L"AttenuationSettings",8);add(L"bAutoDestroy",1);add(L"ReturnValue",8);
    }else if(name==L"K2_GetComponentToWorld")add(L"ReturnValue",48);
    else if(name==L"GetSocketTransform"){add(L"InSocketName",8);add(L"TransformSpace",1);add(L"ReturnValue",48);}
    else if(name==L"MakeRelativeTransform"){add(L"A",48);add(L"RelativeTo",48);add(L"ReturnValue",48);}
    else if(name==L"InvertTransform"){add(L"T",48);add(L"ReturnValue",48);}
    else if(name==L"ComposeTransforms"){add(L"A",48);add(L"B",48);add(L"ReturnValue",48);}
    else if(name==L"K2_SetActorTransform"){add(L"NewTransform",48);add(L"bSweep",1);add(L"bTeleport",1);add(L"ReturnValue",1);}
    else assert(name==L"Stop");
    return fields;
}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){for(auto& o:test::objects)if(&o==p)return o.live;return false;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<int(test::objects.size())?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<int(test::objects.size())&&&test::objects[i]==p&&test::objects[i].live;}
void* ClassOf(void* p){return p;}
void* OuterOf(void* p){return static_cast<test::Obj*>(p)->outer;}
const FName& NameOf(void* p){return static_cast<test::Obj*>(p)->name;}
std::wstring ToString(const FName& name){return test::objects.at(name.ComparisonIndex).text;}
std::wstring ClassNameOf(void* p){return static_cast<test::Obj*>(p)->type;}
void* FindDispatchFunctionCached(void*,const wchar_t* name){return test::Fn(name);}
void* FindClassDefaultObject(const wchar_t*){static auto* gs=test::Make(L"gs");return gs;}
int32_t FindPropertyOffset(void*,const wchar_t* name){
    const std::wstring key=name;
    if(key==L"Mesh")return offsetof(test::Obj,mesh);if(key==L"mesh_playerVisible")return offsetof(test::Obj,visible);
    if(key==L"RootComponent")return offsetof(test::Obj,root);if(key==L"Duration")return offsetof(test::Obj,duration);
    if(key==L"StaticMesh")return offsetof(test::Obj,staticMesh);
    return -1;
}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& offset,uint8_t& mask){
    offset=offsetof(test::Obj,flags);mask=!wcscmp(name,L"bLooping")?1:2;return true;
}
std::vector<ParamInfo> FunctionParams(void* fn){return test::Schema(fn);}
int32_t FindParamOffset(void* fn,const wchar_t* key){for(const auto& p:test::Schema(fn))if(!_wcsicmp(p.name.c_str(),key))return p.offset;return -1;}
int32_t FunctionFrameSize(void*){return 256;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* WorldOf(void*){return nullptr;}void* CurrentWorld(){return nullptr;}}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring& name){return test::Make(name)->name;}}
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char*,const char*,const wchar_t*,const wchar_t* fn,Mode):fn_(fn){}
Resolved Resolve(const Function& row){return Resolved(test::Fn(row.Name()),&row);}
}
namespace ue_wrap::game_thread {
bool IsGameThread(){return true;}
bool RegisterPostObserver(const facts::Resolved& fn,ProcessEventObserverFn cb){test::observers[static_cast<test::Obj*>(fn.Fn())->text]=cb;return true;}
}
namespace ue_wrap::asset_load {void* LoadObjectByPath(const wchar_t* path){
    ++test::loads;if(!test::installed)return nullptr;std::wstring full=path;const auto name=full.substr(full.find_last_of(L'.')+1);
    auto& existing=test::assets[name];if(existing)return existing;
    for(size_t i=0;i<emote_wheel::kPartCount;++i){const auto& part=emote_wheel::kParts[i];if(name==part.animation)return test::Asset(uint16_t(i+1),false);
        if(part.sound && name==part.sound)return test::Asset(uint16_t(i+1),true);}
    return nullptr;
}}
namespace ue_wrap::engine {void* SpawnSoundAttenuation(const SoundAttenuationConfig&){return test::Make(L"attenuation");}}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn){if(!fn)return;auto& meta=test::metadata[fn];meta.frameSize=256;meta.offsets.clear();
    for(const auto& p:test::Schema(fn))meta.offsets.emplace_back(p.name,p.offset);meta_=&meta;buf_.resize(256);}
bool ParamFrame::SetRaw(const wchar_t* name,const void* value,int32_t size){
    if(!fn_)return false;for(const auto& field:test::Schema(fn_))if(!_wcsicmp(field.name.c_str(),name)){
        if(size!=field.size)return false;std::memcpy(buf_.data()+field.offset,value,size);return true;
    }return false;
}
bool ParamFrame::GetRaw(const wchar_t* name,void* out,int32_t size)const{
    if(!fn_)return false;for(const auto& field:test::Schema(fn_))if(!_wcsicmp(field.name.c_str(),name)){
        if(size!=field.size)return false;std::memcpy(out,buf_.data()+field.offset,size);return true;
    }return false;
}
bool Call(void* object,ParamFrame& frame){
    using namespace test;const auto name=static_cast<Obj*>(frame.function())->text;auto* obj=static_cast<Obj*>(object);assert(obj&&obj->live);
    if(name==L"GetAnimInstance")frame.Set(L"ReturnValue",obj->instance);
    else if(name==L"PlaySlotAnimationAsDynamicMontage"){
        ++plays;lastStart=frame.Get<float>(L"InTimeToStartMontageAt");auto* montage=Make(L"montage");montage->position=lastStart;montage->playing=true;
        frame.Set(L"ReturnValue",static_cast<void*>(montage));
    }else if(name==L"Montage_IsPlaying")frame.Set(L"ReturnValue",static_cast<Obj*>(frame.Get<void*>(L"Montage"))->playing);
    else if(name==L"Montage_GetPosition")frame.Set(L"ReturnValue",static_cast<Obj*>(frame.Get<void*>(L"Montage"))->position);
    else if(name==L"Montage_SetPosition"){++seeks;static_cast<Obj*>(frame.Get<void*>(L"Montage"))->position=frame.Get<float>(L"NewPosition");}
    else if(name==L"Montage_Stop"){++stops;lastStopped=frame.Get<void*>(L"Montage");static_cast<Obj*>(lastStopped)->playing=false;}
    else if(name==L"IsPlaying")frame.Set(L"ReturnValue",obj->playing);
    else if(name==L"SpawnSoundAttached"){
        ++sounds;lastMusic=frame.Get<float>(L"StartTime");auto* component=Make(L"remoteAudio");component->playing=true;
        frame.Set(L"ReturnValue",static_cast<void*>(component));
    }else if(name==L"Stop"){++audioStops;obj->playing=false;}
    else if(name==L"K2_GetComponentToWorld")frame.Set(L"ReturnValue",obj->transform);
    else if(name==L"GetSocketTransform"){
        const auto bone=objects.at(frame.Get<R::FName>(L"InSocketName").ComparisonIndex).text;
        FTransform t;t.TX=bone==L"hand_L"?10.f:bone==L"hand_R"?20.f:100.f;frame.Set(L"ReturnValue",t);
    }else if(name==L"MakeRelativeTransform"){
        auto a=frame.Get<FTransform>(L"A"),b=frame.Get<FTransform>(L"RelativeTo");a.TX-=b.TX;a.TY-=b.TY;a.TZ-=b.TZ;frame.Set(L"ReturnValue",a);
    }else if(name==L"InvertTransform"){
        auto a=frame.Get<FTransform>(L"T");a.TX=-a.TX;a.TY=-a.TY;a.TZ=-a.TZ;frame.Set(L"ReturnValue",a);
    }else if(name==L"ComposeTransforms"){
        auto a=frame.Get<FTransform>(L"A"),b=frame.Get<FTransform>(L"B");a.TX+=b.TX;a.TY+=b.TY;a.TZ+=b.TZ;frame.Set(L"ReturnValue",a);
    }else if(name==L"K2_SetActorTransform"){
        auto* root=static_cast<Obj*>(obj->root);auto* mesh=static_cast<Obj*>(obj->staticMesh);const auto t=frame.Get<FTransform>(L"NewTransform");
        mesh->transform.TX+=t.TX-root->transform.TX;root->transform=t;frame.Set(L"ReturnValue",true);
    }
    else assert(false);
    if(auto it=observers.find(name);it!=observers.end())it->second(object,frame.function(),frame.data());
    Callback();return true;
}
}
int main(){
    namespace E=ue_wrap::emote_wheel;using namespace test;using ue_wrap::ParamFrame;
    body=Make(L"local");mesh=Make(L"Mesh");visible=Make(L"visible");first=Make(L"first");second=Make(L"second");root=Make(L"root");
    body->mesh=mesh;body->visible=visible;body->root=root;mesh->instance=first;visible->instance=second;
    auto* remote=Make(L"remote");auto* remoteMesh=Make(L"remoteMesh");auto* remoteInst=Make(L"remoteInst");
    remote->mesh=remote->visible=remoteMesh;remote->root=root;remoteMesh->instance=remoteInst;
    uint16_t loop=0,variant=0;for(size_t i=0;i<E::kPartCount;++i){if(E::kParts[i].loop&&E::kParts[i].sound&&!loop)loop=uint16_t(i+1);
        if(std::wstring(E::kParts[i].animation).find(L"_v2")!=std::wstring::npos&&!variant)variant=uint16_t(i+1);}
    E::Observe(body);source=Asset(loop,false);sound=Asset(loop,true);audio=Make(L"localAudio");audio->playing=true;
    auto play=[&](Obj* instance,Obj* asset){ParamFrame f(Fn(L"PlaySlotAnimationAsDynamicMontage"));f.Set(L"Asset",static_cast<void*>(asset));ue_wrap::Call(instance,f);return static_cast<Obj*>(f.Get<void*>(L"ReturnValue"));};
    auto* wrong=play(second,source);assert(!E::Sample().part);
    auto* montage=play(first,source);montage->position=1.5f;auto state=E::Sample();assert(state.part==loop && state.position==1.5f);
    play(remoteInst,Asset(variant,false));assert(E::Sample().episode==state.episode);
    ParamFrame music(Fn(L"SpawnSound2D"));music.Set(L"WorldContextObject",static_cast<void*>(body));music.Set(L"Sound",static_cast<void*>(sound));
    music.Set(L"ReturnValue",static_cast<void*>(audio));music.Set(L"StartTime",.5f);E::OnSound(nullptr,music.function(),music.data());
    state=E::Sample();assert(state.music==loop && state.musicPosition>=.5f);
    montage->playing=false;assert(!E::Sample().part && E::Sample().music==loop);audio->playing=false;assert(!E::Sample().music);
    // Foreign packages and preview instances cannot capture emotes.
    auto* forged=Make(source->text,L"AnimSequence");forged->outer=Make(L"/Game/Foreign/"+source->text,L"Package");
    play(first,forged);assert(!E::Sample().part);
    E::Playback playback;E::State desired{11,12,1.f,3.f,loop,loop};auto current=[]{return allowed;};
    const auto before=plays;assert(E::Apply(remote,desired,playback,current));assert(plays==before+2 && sounds==1 && lastStart==1.f && lastMusic==3.f);
    const auto once=plays;assert(E::Apply(remote,desired,playback,current) && plays==once && sounds==1);
    desired.position=2;assert(E::Apply(remote,desired,playback,current) && seeks>=1 && plays==once);
    desired.episode++;desired.part=variant;desired.position=.1f;assert(E::Apply(remote,desired,playback,current) && plays==once+2 && sounds==1);
    auto owned=playback.montages[1].Get();E::Stop(playback);assert(lastStopped==owned && !playback.actor.Get() && audioStops==1);
    // Missing optional pak is probed once in this world, then skipped silently.
    ++world;E::Observe(body);installed=false;const auto attempts=loads;
    for(int i=0;i<10;++i)assert(!E::Apply(remote,desired,playback,current));assert(loads==attempts+1);
    ++world;installed=true;E::Observe(body);assert(E::Apply(remote,desired,playback,current));
    // Refuse malformed state before any native play/load.
    auto bad=desired;bad.position=std::numeric_limits<float>::quiet_NaN();const auto baseline=plays;
    assert(!E::Apply(remote,bad,playback,current) && plays==baseline);
    bad=desired;bad.part=65535;assert(!E::Apply(remote,bad,playback,current));
    // Body and world replacement invalidate the continuation of an in-flight native call.
    desired.episode++;callback=[]{allowed=false;};assert(!E::Apply(remote,desired,playback,current));E::Stop(playback);allowed=true;
    callback=[]{++world;};assert(!E::Apply(remote,desired,playback,current));E::Stop(playback);
    E::Observe(body);assert(E::Apply(remote,desired,playback,current));++remoteInst->serial;
    assert(E::Apply(remote,desired,playback,current));E::Stop(playback);
    // A missing animation cannot repeatedly restart a valid song during retries.
    E::g_anims[variant].Reset();E::g_missingAnim[variant]=true;
    const auto songs=sounds;assert(!E::Apply(remote,desired,playback,current));
    assert(!E::Apply(remote,desired,playback,current) && sounds==songs+1);
    E::Stop(playback);E::g_missingAnim[variant]=false;
    E::g_sounds[loop].Reset();E::g_missingSound[loop]=true;
    assert(!E::Apply(remote,desired,playback,current) && playback.state.part==variant && !playback.audio.Get());
    E::Stop(playback);E::g_missingSound[loop]=false;
    // The visible held component can move independently of its actor root.
    E::Observe(body);auto* held=Make(L"held");auto* heldRoot=Make(L"heldRoot");auto* shown=Make(L"heldMesh");
    held->root=heldRoot;held->staticMesh=shown;shown->transform.TX=24;
    const auto hand=E::CaptureHand(body,held,99);assert(hand.generation==99 && hand.bone==2 && hand.pose[0]==4);
    auto* mirror=Make(L"mirror");auto* mirrorRoot=Make(L"mirrorRoot");auto* mirrorMesh=Make(L"mirrorMesh");
    mirror->root=mirrorRoot;mirror->staticMesh=mirrorMesh;mirrorMesh->transform.TX=4;
    assert(E::PlaceHand(remote,mirror,hand,current));assert(mirrorRoot->transform.TX==20 && mirrorMesh->transform.TX==24);
    assert(E::PlaceHand(remote,mirror,hand,current));assert(mirrorMesh->transform.TX-mirrorRoot->transform.TX==4);
    auto badHand=hand;badHand.pose[6]=0;assert(!E::PlaceHand(remote,mirror,badHand,current));
    callback=[]{allowed=false;};assert(!E::PlaceHand(remote,mirror,hand,current));allowed=true;
    ++world;assert(!E::CaptureHand(body,held,99).generation);E::Observe(body);
    std::puts("PASS actual EmoteWheel native observation, exact variant/phase/position, music clock, seek, owned cleanup, missing pak cache and retired native continuations");
}
