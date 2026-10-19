#include "coop/creatures/grayboar_actions.h"
#include "coop/element/registry.h"
#include "coop/net/session.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/fname_utils.h"
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
struct Cue {Packet packet{};uint64_t expires=0;uint32_t peers=0;uint64_t sampled=0;};
struct Join {
    int slot=0;size_t next=0;uint64_t expires=0;
    std::vector<element::Registry::ActorIdPair> actors;
    std::vector<ue_wrap::CachedObjRef> live;
};
std::deque<Join> g_joins;
std::vector<Observation> g_observations;
std::deque<Cue> g_out,g_in;
uint64_t Now() {return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count());}
bool Current() {return g_session && g_session->running() && g_session->connected() &&
    g_world==ue_wrap::world_identity::Generation();}
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
bool Silence(void* actor) {
    const auto epoch=g_epoch;ue_wrap::CachedObjRef owner;owner.Set(actor);
    for(const auto* name:{L"audio_idle",L"audio_attack",L"audio_roar"}) {
        if(void* audio=Component(actor,name)) {
            ue_wrap::ParamFrame frame(R::FindDispatchFunctionCached(R::ClassOf(audio),L"Stop"));
            if(frame.valid())ue_wrap::Call(audio,frame);
            if(!owner.Get() || epoch!=g_epoch || !Current())return false;
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
bool Play(void* actor,const Packet& packet,float waited=0.f) {
    if(!Boar(actor))return false;
    const auto epoch=g_epoch;
    ue_wrap::CachedObjRef owner;owner.Set(actor);
    void* mesh=Component(actor,L"SkeletalMesh");
    void* getAnim=mesh?R::FindDispatchFunctionCached(R::ClassOf(mesh),L"GetAnimInstance"):nullptr;
    if(!getAnim)return false;
    ue_wrap::ParamFrame get(getAnim);void* anim=nullptr;
    if(!get.valid() || !ue_wrap::Call(mesh,get) || !owner.Get() || epoch!=g_epoch || !Current() ||
       !get.GetRaw(L"ReturnValue",&anim,sizeof(anim)) || !anim || !R::IsLive(anim))return false;
    ue_wrap::CachedObjRef animation;animation.Set(anim);
    void* montage=R::FindObject(L"grayboar_Skeleton_Montage",L"AnimMontage");
    void* play=R::FindDispatchFunctionCached(R::ClassOf(anim),L"Montage_Play");
    void* jump=R::FindDispatchFunctionCached(R::ClassOf(anim),L"Montage_JumpToSection");
    if(!montage || !play || !jump)return false;
    ue_wrap::CachedObjRef asset;asset.Set(montage);
    const auto section=ue_wrap::fname_utils::StringToFName(Section(packet.action));
    if(!owner.Get() || !animation.Get() || !asset.Get() || epoch!=g_epoch || !Current())return false;
    ue_wrap::ParamFrame start(play),seek(jump);
    if(!start.valid() || !seek.valid() ||
       !start.Set<void*>(L"MontageToPlay",montage) || !start.Set<float>(L"InPlayRate",packet.rate) ||
       !seek.SetRaw(L"SectionName",&section,sizeof(section)) || !seek.Set<void*>(L"Montage",montage))return false;
    const bool resume=packet.position>=0;
    void* stop=resume?R::FindDispatchFunctionCached(R::ClassOf(anim),L"Montage_Stop"):nullptr;
    ue_wrap::ParamFrame end(stop);
    if(resume && (!stop || !R::FindDispatchFunctionCached(R::ClassOf(anim),L"Montage_GetCurrentSection") ||
       !end.valid() || !end.Set<void*>(L"Montage",montage) || !end.Set<float>(L"InBlendOutTime",0.f) ||
       !start.Set<float>(L"InTimeToStartMontageAt",packet.position+waited*packet.rate)))return false;
    if(!ue_wrap::Call(anim,start) || !owner.Get() || !animation.Get() || !asset.Get() || epoch!=g_epoch || !Current())return false;
    float duration=0;
    if(!start.GetRaw(L"ReturnValue",&duration,sizeof(duration)) || !std::isfinite(duration) || duration<=0)return false;
    if(packet.action==3 && !Silence(actor))return true;
    if(resume) {
        R::FName current{};
        if(!MontageValue(anim,montage,L"Montage_GetCurrentSection",current) || !owner.Get() ||
           !animation.Get() || !asset.Get() || epoch!=g_epoch || !Current())return false;
        if(!R::NameEquals(current,Section(packet.action))) {
            // Waiting crossed the section boundary: consume the stale snapshot,
            // and stop only this montage, without exposing its next action.
            if(!ue_wrap::Call(anim,end))return false;
        }
        // A join snapshot never restarts a one-shot roar/attack sound at zero.
        return true;
    }
    if(!ue_wrap::Call(anim,seek) || !owner.Get() || !animation.Get() || epoch!=g_epoch || !Current())return false;
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
               owner.Get() && voice.Get() && epoch==g_epoch && Current()) {
                ue_wrap::ParamFrame startSound(R::FindDispatchFunctionCached(R::ClassOf(audio),L"Play"));
                if(startSound.valid() && startSound.Set<float>(L"StartTime",0.f))ue_wrap::Call(audio,startSound);
            }
        }
        if(!owner.Get() || epoch!=g_epoch || !Current())return true;
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
    if(!eid || eid>=element::kHostRangeSize || !element || element->IsMirror() || element->LiveActor()!=actor ||
       !ValidRate(packet.rate,packet.action) || g_out.size()>=32)return;
    uint32_t peers=0;
    for(int slot=1;slot<net::kMaxPeers;++slot)if(g_session->IsSlotWorldReady(slot))peers|=1u<<slot;
    if(!peers)return;
    if(!++g_sequence)++g_sequence;
    packet.eid=eid;packet.sequence=g_sequence;
    g_out.push_back({packet,Now()+1500,peers});
}
}
void Install(net::Session* session) {
    const auto world=ue_wrap::world_identity::Generation();
    if(session!=g_session || world!=g_world){OnDisconnect();g_session=session;g_world=world;}
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
    const auto rateOff=R::FindPropertyOffset(R::ClassOf(call.object),L"animationSpeed");float rate=0;
    if(rateOff<0)return;
    std::memcpy(&rate,static_cast<uint8_t*>(call.object)+rateOff,sizeof(rate));
    if(action==3)rate*=0.5f;
    Queue(call.object,Packet{0,0,rate,action});
}
void Receive(const Packet& packet,int sender) {
    if(!Current() || g_session->role()!=net::Role::Client || sender!=0 ||
       !packet.eid || packet.eid>=element::kHostRangeSize || !packet.sequence ||
       packet.action<1 || packet.action>7 || !ValidRate(packet.rate,packet.action) ||
       !std::isfinite(packet.position) || (packet.position<0 && packet.position!=-1.f) || packet.position>3600.f ||
       (g_seen && int32_t(packet.sequence-g_seen)<=0) || g_in.size()>=32)return;
    const auto now=Now();g_seen=packet.sequence;g_in.push_back({packet,now+1500,0,now});
}
void Tick() {
    if(!Current() || g_ticking)return;
    struct Guard {Guard(){g_ticking=true;}~Guard(){g_ticking=false;}} guard;
    const auto epoch=g_epoch,now=Now();
    if(g_session->role()==net::Role::Host) {
        int budget=4;
        while(budget>0 && !g_joins.empty() && g_out.size()<32) {
            auto& join=g_joins.front();
            if(now>join.expires || !g_session->IsSlotWorldReady(join.slot) || join.next>=join.actors.size()){g_joins.pop_front();continue;}
            const auto pair=join.actors[join.next];const auto live=join.live[join.next++];const int slot=join.slot;--budget;
            if(!live.Get() || pair.mirror || !pair.id || pair.id>=element::kHostRangeSize)continue;
            auto* element=element::Registry::Get().Get(pair.id);
            if(!element || element->IsMirror() || element->LiveActor()!=pair.actor)continue;
            Packet packet{};packet.eid=pair.id;
            const bool current=ReadCurrent(pair.actor,packet);
            if(epoch!=g_epoch || !Current())return;
            if(current) {
                if(!++g_sequence)++g_sequence;packet.sequence=g_sequence;
                g_out.push_back({packet,Now()+1500,1u<<slot,Now()});
            }
        }
        while(!g_out.empty()) {
            auto cue=g_out.front();g_out.pop_front();
            if(now>cue.expires)continue;
            for(int slot=1;slot<net::kMaxPeers;++slot)if(cue.peers&(1u<<slot)) {
                auto packet=cue.packet;
                if(packet.position>=0)packet.position+=float(Now()-cue.sampled)*0.001f*packet.rate;
                if(!g_session->IsSlotWorldReady(slot) || g_session->SendReliableToSlot(slot,net::ReliableKind::GrayboarAction,&packet,sizeof(packet)))
                    cue.peers&=~(1u<<slot);
            }
            if(cue.peers){g_out.push_front(cue);break;}
        }
        return;
    }
    // Keep FIFO order when a mirror or AnimInstance is not ready yet.
    for(int budget=0;budget<4 && !g_in.empty();++budget) {
        const auto cue=g_in.front();
        if(now>cue.expires){g_in.pop_front();continue;}
        auto* element=element::Registry::Get().Get(cue.packet.eid);
        void* actor=element && element->IsMirror()?element->LiveActor():nullptr;
        const bool played=actor && Play(actor,cue.packet,float(now-cue.sampled)*0.001f);
        if(epoch!=g_epoch || !Current())return;
        if(!played)break;
        g_in.pop_front();
    }
}
void OnDisconnect() {++g_epoch;g_session=nullptr;g_world=g_sequence=g_seen=0;g_observations.clear();g_out.clear();g_in.clear();g_joins.clear();}
void QueueConnect(int slot) {
    if(!Current() || g_session->role()!=net::Role::Host || slot<=0 || slot>=net::kMaxPeers)return;
    for(const auto& join:g_joins)if(join.slot==slot)return;
    Join join{};join.slot=slot;join.expires=Now()+15000;
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
    for(auto& cue:g_out)cue.peers&=~(1u<<slot);
    for(size_t i=0;i<g_joins.size();) {
        if(g_joins[i].slot==slot)g_joins.erase(g_joins.begin()+i);else ++i;
    }
}
}
