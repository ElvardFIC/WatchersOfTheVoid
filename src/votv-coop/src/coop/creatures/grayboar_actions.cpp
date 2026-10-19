#include "coop/creatures/grayboar_actions.h"
#include "coop/element/registry.h"
#include "coop/net/session.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/fname_utils.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <vector>

namespace coop::grayboar_actions {
namespace {
namespace R=ue_wrap::reflection;
using Packet=net::GrayboarActionPayload;
net::Session* g_session=nullptr;
uint32_t g_world=0,g_sequence=0,g_seen=0;
uint64_t g_epoch=0;
std::array<uint64_t,net::kMaxPeers> g_peerEpoch{};
bool g_ticking=false;
struct Observation {
    ue_wrap::CachedObjRef actor;
    void* stack=nullptr;
    uint8_t action=0;
    bool wasActive=false,valid=false;
    bool hitEligible=false;
    float health=0;
    uint64_t epoch=0;
};
enum class PlaybackPhase {Start,Verify,Stop};
struct Playback {
    PlaybackPhase phase=PlaybackPhase::Start;
    ue_wrap::CachedObjRef mesh,animation,asset;
};
struct Cue {
    Packet packet{};uint64_t expires=0;uint32_t peers=0;uint64_t sampled=0;
    ue_wrap::CachedObjRef actor;
    Playback playback;
};
struct Join {
    int slot=0;size_t next=0;uint64_t expires=0,peerEpoch=0;
    std::vector<element::Registry::ActorIdPair> actors;
    std::vector<ue_wrap::CachedObjRef> live;
};
std::deque<Join> g_joins;
std::vector<Observation> g_observations;
std::deque<Cue> g_out,g_in;
std::vector<uint32_t> g_retryActors;
uint64_t Now() {return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count());}
bool Current() {return g_session && g_session->running() && g_session->connected() &&
    g_world==ue_wrap::world_identity::Generation();}
uint32_t QueueablePeers() {
    const auto now=Now();
    std::erase_if(g_out,[=](const Cue& cue){return !cue.peers || now>cue.expires;});
    std::array<size_t,net::kMaxPeers> pending{};
    for(const auto& cue:g_out)
        for(int slot=1;slot<net::kMaxPeers;++slot)if(cue.peers&(1u<<slot))++pending[slot];
    uint32_t peers=0;
    for(int slot=1;slot<net::kMaxPeers;++slot)
        if(pending[slot]<32 && g_session->IsSlotWorldReady(slot))peers|=1u<<slot;
    return peers;
}
bool Boar(void* actor) {
    if(!actor || !R::IsLive(actor))return false;
    void* cls=R::ClassOf(actor);
    for(int i=0;cls && i<64;++i,cls=R::SuperStructOf(cls))
        if(R::NameEquals(R::NameOf(cls),L"grayboar_C"))return true;
    return false;
}
bool Flag(void* actor,uint8_t action,bool& value) {
    int32_t off=-1;uint8_t mask=0;
    if(!R::FindBoolProperty(R::ClassOf(actor),action==1?L"biting":action==2?L"roaring":L"dying",off,mask) || off<0 || !mask)return false;
    value=(static_cast<uint8_t*>(actor)[off]&mask)!=0;return true;
}
const wchar_t* Section(uint8_t action){
    switch(action) {
    case 1:return L"atttack";case 2:return L"roar";case 3:return L"death";
    case 4:return L"damage1";case 5:return L"damage2";case 6:return L"damage3";case 7:return L"damage4";
    default:return L"";
    }
}
bool ReadHealth(void* actor,float& health) {
    const int off=R::FindPropertyOffset(R::ClassOf(actor),L"health");
    if(off<0)return false;
    std::memcpy(&health,static_cast<uint8_t*>(actor)+off,sizeof(health));
    return std::isfinite(health);
}
bool WantsHitAnimation(const ue_wrap::script_gate::Call& call) {
    if(!call.function || !call.locals)return false;
    const int off=R::FindParamOffset(call.function,L"skipAnim");
    const int size=R::FunctionFrameSize(call.function);
    return off>=0 && off<size && call.locals[off]==0;
}
bool ValidRate(float rate,uint8_t action){return std::isfinite(rate) && rate>=(action==3?0.025f:0.05f) && rate<=10.f;}
void* Component(void* actor,const wchar_t* name) {
    const auto off=R::FindPropertyOffset(R::ClassOf(actor),name);
    void* value=nullptr;
    if(off>=0)std::memcpy(&value,static_cast<uint8_t*>(actor)+off,sizeof(value));
    return value && R::IsLive(value)?value:nullptr;
}
template<class Check> bool Silence(void* actor,Check valid) {
    for(const auto* name:{L"audio_idle",L"audio_attack",L"audio_roar"}) {
        if(void* audio=Component(actor,name)) {
            ue_wrap::ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(audio),L"Stop"));
            if(frame.valid())ue_wrap::Call(audio,frame);
            if(!valid())return false;
        }
    }
    return true;
}
template<class T> bool MontageValue(void* anim,void* montage,const wchar_t* name,T& out) {
    void* fn=R::FindDispatchFunctionCached(R::ClassOf(anim),name);
    ue_wrap::ParamFrame frame(fn);
    return frame.valid() && frame.Set<void*>(L"Montage",montage) && ue_wrap::Call(anim,frame) &&
        frame.GetRaw(L"ReturnValue",&out,sizeof(out));
}
bool ReadCurrent(void* actor,Packet& packet) {
    if(!Boar(actor))return false;
    int32_t dyingOff=-1;uint8_t dyingMask=0;
    if(!R::FindBoolProperty(R::ClassOf(actor),L"dying",dyingOff,dyingMask) || dyingOff<0 || !dyingMask)return false;
    const auto epoch=g_epoch;
    ue_wrap::CachedObjRef owner;owner.Set(actor);
    void* mesh=Component(actor,L"SkeletalMesh");
    void* fn=mesh?R::FindDispatchFunctionCached(R::ClassOf(mesh),L"GetAnimInstance"):nullptr;
    ue_wrap::ParamFrame frame(fn);void* anim=nullptr;
    if(!frame.valid() || !ue_wrap::Call(mesh,frame) || !owner.Get() || epoch!=g_epoch || !Current() ||
       !frame.GetRaw(L"ReturnValue",&anim,sizeof(anim)) || !anim || !R::IsLive(anim))return false;
    ue_wrap::CachedObjRef animation;animation.Set(anim);
    void* montage=R::FindObject(L"grayboar_Skeleton_Montage",L"AnimMontage");
    if(!montage)return false;
    ue_wrap::CachedObjRef asset;asset.Set(montage);
    // Getters have no gameplay callbacks, but every reflected call can tear down
    // the session in a hook; revalidate before issuing another one.
    auto valid=[&]{return owner.Get() && animation.Get() && asset.Get() && epoch==g_epoch && Current();};
    uint8_t playing=0;R::FName section{};
    if(!MontageValue(anim,montage,L"Montage_IsPlaying",playing) || !valid() || !playing)return false;
    if(!MontageValue(anim,montage,L"Montage_GetCurrentSection",section) || !valid())return false;
    packet.action=0;
    for(uint8_t action=1;action<=7;++action)if(R::NameEquals(section,Section(action))){packet.action=action;break;}
    bool active=false;
    if(!packet.action || (packet.action<=3 && (!Flag(actor,packet.action,active) || !active)))return false;
    if(packet.action!=3 && (static_cast<uint8_t*>(actor)[dyingOff]&dyingMask))return false;
    if(!MontageValue(anim,montage,L"Montage_GetPosition",packet.position) || !valid())return false;
    if(!MontageValue(anim,montage,L"Montage_GetPlayRate",packet.rate) || !valid())return false;
    return std::isfinite(packet.position) && packet.position>=0 && packet.position<=3600.f &&
        ValidRate(packet.rate,packet.action);
}
// Asset playback only: no callback proxy, actor event, damage or loot delegates.
bool Play(void* actor,Cue& cue,float waited=0.f) {
    const auto& packet=cue.packet;auto& playback=cue.playback;
    const bool started=playback.phase!=PlaybackPhase::Start,expired=Now()>cue.expires;
    if(expired && !started)return true;
    if(!Boar(actor))return false;
    const auto epoch=g_epoch;
    ue_wrap::CachedObjRef owner;owner.Set(actor);
    void* mesh=Component(actor,L"SkeletalMesh");
    if(started && (!playback.mesh.Get() || playback.mesh.Raw()!=mesh ||
       !playback.animation.Get() || !playback.asset.Get()))return true;
    ue_wrap::CachedObjRef skeletal;skeletal.Set(mesh);
    auto owned=[&] {
        if(!owner.Get() || !skeletal.Get() || epoch!=g_epoch || !Current() ||
           g_session->role()!=net::Role::Client)return false;
        auto* element=element::Registry::Get().Get(packet.eid);
        return element && element->IsMirror() && element->LiveActor()==actor && Component(actor,L"SkeletalMesh")==mesh;
    };
    void* getAnim=mesh?R::FindDispatchFunctionCached(R::ClassOf(mesh),L"GetAnimInstance"):nullptr;
    if(!getAnim)return expired;
    ue_wrap::ParamFrame get(getAnim);void* anim=nullptr;
    if(!get.valid() || !ue_wrap::Call(mesh,get))return expired;
    if(!owned())return true;
    if(!get.GetRaw(L"ReturnValue",&anim,sizeof(anim)) || !anim || !R::IsLive(anim))return expired;
    if(started && (!playback.mesh.Get() || !playback.animation.Get() || !playback.asset.Get() ||
       anim!=playback.animation.Raw()))return true;
    ue_wrap::CachedObjRef animation;animation.Set(anim);
    void* montage=started?playback.asset.Get():R::FindObject(L"grayboar_Skeleton_Montage",L"AnimMontage");
    if(!montage)return expired;
    ue_wrap::CachedObjRef asset;asset.Set(montage);
    auto valid=[&]{return owned() && animation.Get() && asset.Get();};
    const bool resume=packet.position>=0;
    ue_wrap::ParamFrame end(R::FindDispatchFunctionCached(R::ClassOf(anim),L"Montage_Stop"));
    if(!end.valid() || !end.Set<void*>(L"Montage",montage) || !end.Set<float>(L"InBlendOutTime",0.f))return expired;
    if(expired)playback.phase=PlaybackPhase::Stop;
    if(playback.phase==PlaybackPhase::Start) {
        if(!R::FindDispatchFunctionCached(R::ClassOf(anim),L"Montage_GetCurrentSection") ||
           !R::FindDispatchFunctionCached(R::ClassOf(anim),L"Montage_IsPlaying"))return false;
        ue_wrap::ParamFrame start(R::FindDispatchFunctionCached(R::ClassOf(anim),L"Montage_Play"));
        ue_wrap::ParamFrame seek(resume?nullptr:R::FindDispatchFunctionCached(R::ClassOf(anim),L"Montage_JumpToSection"));
        if(!start.valid() || !start.Set<void*>(L"MontageToPlay",montage) || !start.Set<float>(L"InPlayRate",packet.rate))return false;
        if(resume) {
            if(!start.Set<float>(L"InTimeToStartMontageAt",packet.position+waited*packet.rate))return false;
        } else {
            const auto section=ue_wrap::fname_utils::StringToFName(Section(packet.action));
            if(!seek.valid() || !seek.SetRaw(L"SectionName",&section,sizeof(section)) || !seek.Set<void*>(L"Montage",montage))return false;
        }
        float duration=0;
        if(!valid() || !start.GetRaw(L"ReturnValue",&duration,sizeof(duration)))return false;
        if(!ue_wrap::Call(anim,start))return false;
        if(!valid())return true;
        const bool result=start.GetRaw(L"ReturnValue",&duration,sizeof(duration));
        if(result && duration==0)return false; // Native refusal has no playback to resume.
        playback.mesh=skeletal;playback.animation=animation;playback.asset=asset;
        playback.phase=result && std::isfinite(duration) && duration>0?PlaybackPhase::Verify:PlaybackPhase::Stop;
        if(playback.phase==PlaybackPhase::Verify && !resume) {
            const bool jumped=ue_wrap::Call(anim,seek);
            if(!valid())return true;
            if(!jumped)playback.phase=PlaybackPhase::Stop;
        }
    }
    if(playback.phase==PlaybackPhase::Verify) {
        uint8_t playing=0;
        const bool queried=MontageValue(anim,montage,L"Montage_IsPlaying",playing);
        if(!valid())return true;
        if(!queried)return false;
        if(!playing)return true;
        R::FName current{};
        const bool sectionRead=MontageValue(anim,montage,L"Montage_GetCurrentSection",current);
        if(!valid())return true;
        if(!sectionRead)return false;
        if(!R::NameEquals(current,Section(packet.action)))playback.phase=PlaybackPhase::Stop;
    }
    // A failed seek, stale snapshot or uncertain start is cleaned up without
    // issuing Montage_Play again. Stop/query retries retain the same instance.
    if(playback.phase==PlaybackPhase::Stop)return ue_wrap::Call(anim,end) || expired;
    if(packet.action==3 && !Silence(actor,valid))return true;
    if(resume)return true; // Joined playback never restarts one-shot audio.
    if(packet.action==3) {
        void* lib=R::FindClassDefaultObject(L"GameplayStatics");
        void* sound=R::FindObject(L"grayboar_idle4",L"SoundWave");
        void* attenuation=R::FindObject(L"att_default",L"SoundAttenuation");
        void* body=Component(actor,L"StaticMesh");
        if(lib && sound && attenuation && body) {
            ue_wrap::ParamFrame audio(R::FindDispatchFunctionCached(R::ClassOf(lib),L"SpawnSoundAttached"));
            if(audio.valid() && audio.Set<void*>(L"Sound",sound) && audio.Set<void*>(L"AttachToComponent",body) &&
               audio.Set<float>(L"VolumeMultiplier",1.5f) && audio.Set<float>(L"PitchMultiplier",1.35f) &&
               audio.Set<void*>(L"AttenuationSettings",attenuation) && audio.Set<bool>(L"bAutoDestroy",true))ue_wrap::Call(lib,audio);
        }
        return true;
    }
    if(packet.action>=4) {
        // Native damaged chooses one of damage1..4, sets the idle component's
        // damage cue, plays it and stops attack audio. No health/gameplay call.
        void* audio=Component(actor,L"audio_idle");
        void* sound=R::FindObject(L"grayboar_damage_Cue",L"SoundCue");
        if(audio && sound) {
            ue_wrap::CachedObjRef voice;voice.Set(audio);
            ue_wrap::ParamFrame set(R::FindDispatchFunctionCached(R::ClassOf(audio),L"SetSound"));
            if(set.valid() && set.Set<void*>(L"NewSound",sound) && ue_wrap::Call(audio,set) &&
               valid() && voice.Get()) {
                ue_wrap::ParamFrame startSound(R::FindDispatchFunctionCached(R::ClassOf(audio),L"Play"));
                if(startSound.valid() && startSound.Set<float>(L"StartTime",0.f))ue_wrap::Call(audio,startSound);
            }
        }
        if(!valid())return true;
        if(void* attack=Component(actor,L"audio_attack")) {
            ue_wrap::ParamFrame stopSound(R::FindDispatchFunctionCached(R::ClassOf(attack),L"Stop"));
            if(stopSound.valid())ue_wrap::Call(attack,stopSound);
        }
        return true;
    }
    // Audio failure must not restart a montage that has already played.
    if(void* audio=Component(actor,packet.action==1?L"audio_attack":L"audio_roar")) {
        if(void* fn=R::FindDispatchFunctionCached(R::ClassOf(audio),L"Play")) {
            ue_wrap::ParamFrame sound(fn);
            if(sound.valid() && sound.Set<float>(L"StartTime",0.f))ue_wrap::Call(audio,sound);
        }
    }
    return true;
}
void Queue(void* actor,Packet packet) {
    auto& registry=element::Registry::Get();const auto eid=registry.EidForActor(actor);
    auto* element=registry.Get(eid);
       !ValidRate(packet.rate,packet.action))return;
    const uint32_t peers=QueueablePeers();
    if(!peers)return;
    if(!++g_sequence)++g_sequence;
    packet.eid=eid;packet.sequence=g_sequence;
    Cue cue{packet,Now()+1500,peers};cue.actor.Set(actor);
    g_out.push_back(cue);
}
}
void Install(net::Session* session) {
    const auto world=ue_wrap::world_identity::Generation();
    if(session!=g_session || world!=g_world){OnDisconnect();g_session=session;g_world=world;g_retryActors.reserve(32);}
}
void Before(const ue_wrap::script_gate::Call& call,uint8_t action) {
    if(action<1 || action>3 || !Current() || g_session->role()!=net::Role::Host || !Boar(call.object))return;
    // Another gate may cancel after our pre callback, so there is no matching
    // post. A reused frame replaces its abandoned observation; storage is bounded.
    for(size_t i=0;i<g_observations.size();) {
        if(g_observations[i].stack==call.stack)g_observations.erase(g_observations.begin()+i);
        else ++i;
    }
    if(g_observations.size()>=32)g_observations.erase(g_observations.begin());
    Observation row{};row.actor.Set(call.object);row.stack=call.stack;row.action=action;row.epoch=g_epoch;
    row.valid=Flag(call.object,action,row.wasActive);
    if(action==3)row.hitEligible=WantsHitAnimation(call) && ReadHealth(call.object,row.health);
    g_observations.push_back(row);
}
void After(const ue_wrap::script_gate::Call& call,uint8_t action) {
    size_t index=g_observations.size();
    for(size_t i=0;i<g_observations.size();++i) {
        const auto& row=g_observations[i];
        if(row.stack==call.stack && row.action==action && row.actor.Raw()==call.object){index=i;break;}
    }
    if(index==g_observations.size())return;
    const auto row=g_observations[index];g_observations.erase(g_observations.begin()+index);
    if(!Current() || g_session->role()!=net::Role::Host || row.epoch!=g_epoch || !row.valid || row.wasActive || !row.actor.Get())return;
    bool active=false;
    if(!Flag(call.object,action,active))return;
    if(!active) {
        float health=0;
        if(action!=3 || !row.hitEligible || !ReadHealth(call.object,health) || health<=0 || health>=row.health)return;
        Packet hit{};
        // Read the section actually selected by native RNG, never roll again on
        // clients. Health must have fallen and skipAnim must have been false:
        // an old damage montage alone is not evidence of a new hit reaction.
        if(!ReadCurrent(call.object,hit) || hit.action<4 || hit.action>7 ||
           !Current() || g_session->role()!=net::Role::Host || row.epoch!=g_epoch || !row.actor.Get())return;
        hit.position=-1.f;Queue(call.object,hit);return;
    }
    int32_t dyingOff=-1;uint8_t dyingMask=0;
    if(!R::FindBoolProperty(R::ClassOf(call.object),L"dying",dyingOff,dyingMask) || dyingOff<0 || !dyingMask ||
       (action!=3 && (static_cast<uint8_t*>(call.object)[dyingOff]&dyingMask)))return;
    if(action==3) {
        int32_t off=-1;uint8_t mask=0;
        if(!R::FindBoolProperty(R::ClassOf(call.object),L"skipDeathAnimation",off,mask) || off<0 || !mask ||
           (static_cast<uint8_t*>(call.object)[off]&mask))return;
    }
    // Native flags can change even when montage playback fails. Relay only
    // the matching active section, at the rate the AnimInstance actually uses.
    Packet packet{};
    if(!ReadCurrent(call.object,packet) || packet.action!=action ||
       !Current() || g_session->role()!=net::Role::Host || row.epoch!=g_epoch || !row.actor.Get())return;
    packet.position=-1.f;Queue(call.object,packet);
}
void Receive(const Packet& packet,int sender) {
    if(!Current() || g_session->role()!=net::Role::Client || sender!=0 ||
       !packet.eid || packet.eid>=element::kHostRangeSize || !packet.sequence ||
       packet.action<1 || packet.action>7 || !ValidRate(packet.rate,packet.action) ||
       !std::isfinite(packet.position) || (packet.position<0 && packet.position!=-1.f) || packet.position>3600.f ||
       (g_seen && int32_t(packet.sequence-g_seen)<=0) || g_in.size()>=32)return;
    const auto now=Now();g_seen=packet.sequence;
    Cue cue{packet,now+1500,0,now};
    auto* element=element::Registry::Get().Get(packet.eid);
    void* actor=element && element->IsMirror()?element->LiveActor():nullptr;
    if(Boar(actor))cue.actor.Set(actor);
    g_in.push_back(cue);
    if(std::find(g_retryActors.begin(),g_retryActors.end(),packet.eid)==g_retryActors.end())g_retryActors.push_back(packet.eid);
}
void Tick() {
    if(!Current() || g_ticking)return;
    struct Guard {Guard(){g_ticking=true;}~Guard(){g_ticking=false;}} guard;
    const auto epoch=g_epoch,now=Now();
    if(g_session->role()==net::Role::Host) {
        int budget=4;
        size_t blockedJoins=0;
        while(budget>0 && !g_joins.empty()) {
            auto& join=g_joins.front();
            if(now>join.expires || !g_session->IsSlotWorldReady(join.slot) || join.next>=join.actors.size()){g_joins.pop_front();continue;}
            if(!(QueueablePeers()&(1u<<join.slot))) {
                auto blocked=std::move(join);g_joins.pop_front();g_joins.push_back(std::move(blocked));
                if(++blockedJoins>=g_joins.size())break;
                continue;
            }
            blockedJoins=0;
            const auto pair=join.actors[join.next];const auto live=join.live[join.next++];const int slot=join.slot;
            const auto peerEpoch=join.peerEpoch;--budget;
            if(!live.Get() || pair.mirror || !pair.id || pair.id>=element::kHostRangeSize)continue;
            auto* element=element::Registry::Get().Get(pair.id);
            if(!element || element->IsMirror() || element->LiveActor()!=pair.actor)continue;
            Packet packet{};packet.eid=pair.id;
            const bool current=ReadCurrent(pair.actor,packet);
            if(epoch!=g_epoch || !Current() || g_session->role()!=net::Role::Host)return;
            element=element::Registry::Get().Get(pair.id);
            if(peerEpoch!=g_peerEpoch[slot] || !(QueueablePeers()&(1u<<slot)) || !live.Get() ||
               !element || element->IsMirror() || element->LiveActor()!=pair.actor)continue;
            if(current) {
                if(!++g_sequence)++g_sequence;packet.sequence=g_sequence;
                Cue cue{packet,Now()+1500,1u<<slot,Now()};cue.actor=live;
                g_out.push_back(cue);
            }
        }
        // Preserve FIFO separately for each peer. A failed recipient must not
        // hold up later cues for peers that already accepted the earlier one.
        uint32_t blocked=0;
        size_t remaining=g_out.size();
        for(size_t i=0;remaining && i<g_out.size();--remaining) {
            const auto cue=g_out[i];
            auto* element=element::Registry::Get().Get(cue.packet.eid);
            if(now>cue.expires || !cue.actor.Get() || !element || element->IsMirror() ||
               element->LiveActor()!=cue.actor.Raw()) {g_out.erase(g_out.begin()+i);continue;}
            for(int slot=1;slot<net::kMaxPeers;++slot) {
                const uint32_t bit=1u<<slot;
                if(!(g_out[i].peers&bit))continue;
                if(!g_session->IsSlotWorldReady(slot)){g_out[i].peers&=~bit;continue;}
                if(blocked&bit)continue;
                auto packet=cue.packet;
                if(packet.position>=0)packet.position+=float(Now()-cue.sampled)*0.001f*packet.rate;
                const bool sent=g_session->SendReliableToSlot(slot,net::ReliableKind::GrayboarAction,&packet,sizeof(packet));
                if(epoch!=g_epoch || !Current() || g_session->role()!=net::Role::Host)return;
                if(sent)g_out[i].peers&=~bit;
                else blocked|=bit;
            }
            if(!g_out[i].peers)g_out.erase(g_out.begin()+i);
            else ++i;
        }
        return;
    }
    std::erase_if(g_in,[=](const Cue& cue){return now>cue.expires && cue.playback.phase==PlaybackPhase::Start;});
    std::erase_if(g_retryActors,[](uint32_t eid){
        return std::none_of(g_in.begin(),g_in.end(),[=](const Cue& cue){return cue.packet.eid==eid;});
    });
    // Rotate entity heads rather than packet sequences: newly arriving events
    // join the tail and cannot indefinitely postpone an older entity's retry.
    const size_t count=g_retryActors.size();
    for(size_t attempted=0;attempted<count && attempted<4;++attempted) {
        const auto eid=g_retryActors.front();g_retryActors.erase(g_retryActors.begin());
        auto pending=std::find_if(g_in.begin(),g_in.end(),[=](const Cue& row){return row.packet.eid==eid;});
        if(pending==g_in.end())continue;
        auto cue=*pending;
        auto* element=element::Registry::Get().Get(eid);
        void* actor=element && element->IsMirror()?element->LiveActor():nullptr;
        if(pending->actor.Raw() && (!pending->actor.Get() || actor!=pending->actor.Raw())) {
            g_in.erase(pending);
        } else if(actor && Boar(actor)) {
            if(!pending->actor.Raw())pending->actor.Set(actor);
            const bool played=Play(actor,cue,float(now-cue.sampled)*0.001f);
            if(epoch!=g_epoch || !Current() || g_session->role()!=net::Role::Client)return;
            if(played)std::erase_if(g_in,[&](const Cue& row){return row.packet.sequence==cue.packet.sequence;});
            else {
                // Reflected callbacks can append packets and invalidate iterators.
                auto retained=std::find_if(g_in.begin(),g_in.end(),[&](const Cue& row){return row.packet.sequence==cue.packet.sequence;});
                if(retained!=g_in.end())retained->playback=cue.playback;
            }
        } else if(now>cue.expires) {
            // A vanished class cannot be called safely even for cleanup.
            g_in.erase(pending);
        }
        if(std::any_of(g_in.begin(),g_in.end(),[=](const Cue& row){return row.packet.eid==eid;}) &&
           std::find(g_retryActors.begin(),g_retryActors.end(),eid)==g_retryActors.end())g_retryActors.push_back(eid);
    }
}
void OnDisconnect() {++g_epoch;g_session=nullptr;g_world=g_sequence=g_seen=0;g_peerEpoch.fill(0);g_observations.clear();g_out.clear();g_in.clear();g_joins.clear();g_retryActors.clear();}
void QueueConnect(int slot) {
    if(!Current() || g_session->role()!=net::Role::Host || slot<=0 || slot>=net::kMaxPeers)return;
    for(const auto& join:g_joins)if(join.slot==slot)return;
    Join join{};join.slot=slot;join.expires=Now()+15000;join.peerEpoch=g_peerEpoch[slot];
    element::Registry::Get().SnapshotActorsByType(element::ElementType::Prop,join.actors);
    size_t retained=0;
    for(const auto pair:join.actors) {
        if(pair.mirror || !R::IsLiveByIndex(pair.actor,pair.internalIdx) || !Boar(pair.actor))continue;
        join.actors[retained++]=pair;
        ue_wrap::CachedObjRef live;live.Set(pair.actor);join.live.push_back(live);
    }
    join.actors.resize(retained);
    g_joins.push_back(std::move(join));
}
void OnPeerLeft(int slot) {
    if(slot<=0 || slot>=net::kMaxPeers)return;
    ++g_peerEpoch[slot];
    for(auto& cue:g_out)cue.peers&=~(1u<<slot);
    for(size_t i=0;i<g_joins.size();) {
        if(g_joins[i].slot==slot)g_joins.erase(g_joins.begin()+i);else ++i;
    }
}
}
