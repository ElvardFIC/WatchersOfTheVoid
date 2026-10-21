#include "coop/props/water_pour_sync.h"
#include "coop/element/intent_authority.h"
#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/hand_item.h"
#include "coop/player/local_prop_hold.h"
#include "coop/player/local_streams.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/active_drive.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_lifecycle.h"
#include "coop/props/remote_prop.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/actors/building_material.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/water_pour.h"
#include "ue_wrap/actors/pressure_washer.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/core/script_gate.h"
#include <algorithm>
#include <array>
#include <deque>
#include <cstring>
#include <vector>
#include <bitset>
#include <cmath>
namespace coop::water_pour_sync {
namespace {
namespace FX=ue_wrap::water_pour;
namespace R=ue_wrap::reflection;
namespace SG=ue_wrap::script_gate;
using Request=net::WaterPourPayload;
using Reply=net::WaterPourReplyPayload;
using Splash=net::WaterSplashPayload;
using Launch=net::WaterLaunchPayload;
using ue_wrap::CachedObjRef;
constexpr int kTag=0x57504F55;
constexpr size_t kMaxCues=1024;
UE_FACT_FN(kWaterLaunchEmitter,"water_pour_sync",L"GameplayStatics",L"SpawnEmitterAtLocation",Declared);
UE_FACT_FN(kBucketSoundAt,"water_pour_sync",L"GameplayStatics",L"PlaySoundAtLocation",Declared);
UE_FACT_FN(kBucketSound2D,"water_pour_sync",L"GameplayStatics",L"PlaySound2D",Declared);
struct LocalBirth {Request request{};CachedObjRef actor,source;uint64_t expires=0,next=0;bool captured=false;uint32_t world=0;};
struct HostBirth {Request request{};uint64_t expires=0;uint8_t code=0;bool processing=false,owed=false;};
struct Cue {Splash packet{};Launch launch{};uint64_t expires=0;uint32_t pending=0;std::array<uint16_t,net::kMaxPeers> people{};std::array<uint64_t,net::kMaxPeers> epochs{};bool surface=false,sound=false,bucket=false;};
struct Collision {CachedObjRef actor;FX::Impact impact;void* stack=nullptr;int depth=0;uint64_t epoch=0;uint8_t kind=0;};
net::Session* g_session=nullptr;
uint32_t g_world=0,g_sequence=0,g_splashSequence=0,g_lastSplash=0;
uint64_t g_epoch=0,g_retryWatch=0;
bool g_ticking=false,g_watched=false,g_capturing=false;
bool g_launchHookReady=false;
CachedObjRef g_tick,g_throw,g_washerTick,g_shotBegin,g_shotGraph;
struct HostShot {CachedObjRef actor,source;uint64_t epoch=0,expires=0;};
std::deque<HostShot> g_nativeShots;
std::deque<LocalBirth> g_local;
std::deque<HostBirth> g_host;
std::deque<Cue> g_cues;
std::vector<Collision> g_collisions;
std::array<CachedObjRef,128> g_impacted{};
size_t g_impactCursor=0;
size_t g_hostCursor=0;
uint16_t g_people[net::kMaxPeers]{};
uint64_t g_peerEpoch[net::kMaxPeers]{};
uint32_t g_seen[net::kMaxPeers]{};
std::array<std::bitset<256>,net::kMaxPeers> g_seenBirths{};
uint64_t g_rateAt[net::kMaxPeers]{};
uint16_t g_rate[net::kMaxPeers]{};
bool Current(){return g_session && g_session->connected() && g_world==ue_wrap::world_identity::Generation();}
bool Current(uint64_t epoch){return Current() && epoch==g_epoch;}
bool Host(){return g_session->role()==net::Role::Host;}
uint8_t Local(){return players::Registry::Get().LocalPeerId();}
uint32_t Next(uint32_t& value){if(!++value)++value;return value;}
bool After(uint32_t a,uint32_t b){return int32_t(a-b)>0;}
bool Unseen(uint8_t slot,uint32_t sequence){
    const auto high=g_seen[slot];if(!high || After(sequence,high))return true;
    const auto distance=high-sequence;return distance<256 && !g_seenBirths[slot][distance];
}
void Remember(uint8_t slot,uint32_t sequence){
    auto& high=g_seen[slot];auto& births=g_seenBirths[slot];
    if(!high){births.reset();high=sequence;}
    else if(After(sequence,high)){const auto distance=sequence-high;if(distance>=256)births.reset();else births<<=distance;high=sequence;}
    births.set(high-sequence);
}
FX::Birth Birth(const Request& p){return {{p.x,p.y,p.z},{p.vx,p.vy,p.vz},p.soap,FX::Kind(p.kind),p.power,{p.pitch,p.yaw,p.roll}};}
FX::Impact Impact(const Splash& p){return {{p.x,p.y,p.z},{p.nx,p.ny,p.nz},{p.ax,p.ay,p.az},p.kind==2};}
bool SourceType(void* source,uint8_t kind){return kind==1?ue_wrap::pressure_washer::IsWasher(source):ue_wrap::building_material::IsBucket(source);}
bool SourceActive(void* source){return SG::IsObjectBodyActive(g_tick.Get(),source) || SG::IsObjectBodyActive(g_throw.Get(),source) || SG::IsObjectBodyActive(g_washerTick.Get(),source);}
void CaptureHost();
void QueueLaunch(const FX::Birth& birth,int owner,bool hostSound);
bool WasherReady(){
    return g_launchHookReady &&
        SG::ClassNameWatchLive(L"prop_sponge_p_C",L"BndEvt__StaticMesh_K2Node_ComponentBoundEvent_0_ComponentHitSignature__DelegateSignature",kTag+2) &&
        SG::ClassNameWatchLive(L"prop_sponge_p_C",L"ExecuteUbergraph_prop_sponge_p",kTag+2) &&
        SG::ClassNameWatchLive(L"prop_sponge_p_C",L"ReceiveBeginPlay",kTag+2) &&
        SG::ClassNameWatchLive(L"prop_sponge_C",L"BndEvt__prop_sponge_StaticMesh_K2Node_ComponentBoundEvent_0_ComponentHitSignature__DelegateSignature",kTag+3) &&
        SG::ClassNameWatchLive(L"prop_sponge_C",L"ExecuteUbergraph_prop_sponge",kTag+3);
}
bool BucketReady(){
    return SG::ClassNameWatchLive(L"prop_sponge_bucketPour_C",L"collided",kTag) &&
        SG::ClassNameWatchLive(L"prop_sponge_bucketPour_C",L"ExecuteUbergraph_prop_sponge_bucketPour",kTag) &&
        SG::ClassNameWatchLive(L"prop_sponge_C",L"BndEvt__prop_sponge_StaticMesh_K2Node_ComponentBoundEvent_0_ComponentHitSignature__DelegateSignature",kTag+3) &&
        SG::ClassNameWatchLive(L"prop_sponge_C",L"ExecuteUbergraph_prop_sponge",kTag+3);
}
bool Key(const net::WireKey& key){
    if(!key.len || key.len>sizeof(key.data))return false;
    for(unsigned i=0;i<key.len;++i)if(uint8_t(key.data[i])<33 || uint8_t(key.data[i])>126)return false;
    return net::StringFromWireKey(key)!=L"None";
}
void Retire(CachedObjRef actor,uint32_t world){
    if(world!=ue_wrap::world_identity::Generation())return;
    if(void* live=actor.Get())if(!hand_item::IsHandAxisActor(live))prop_lifecycle::DestroyLocalProp(live,true);
}
template<class Packet>bool OwnedSource(const Packet& p,const CachedObjRef& identity){
    if(!Current())return false;const auto epoch=g_epoch;
    void* source=identity.Get();
    if(!source || p.slot!=Local() || (p.playerNo && roster_ledger::Get(p.slot).playerNo!=p.playerNo))return false;
    if(p.source.len)return local_prop_hold::IsHeld(source) && Current(epoch) &&
        (!p.generation || (local_streams::LastHeldActor()==source && local_streams::CurrentHoldGen()==p.generation));
    return hand_item::LocalHandActor()==source && (!p.generation || hand_item::Generation(p.slot)==p.generation);
}
bool LocalSource(const LocalBirth& row){return OwnedSource(row.request,row.source);}
template<class Packet>void* HostSource(const Packet& p){
    if(roster_ledger::Get(p.slot).playerNo!=p.playerNo)return nullptr;
    if(!p.source.len)return hand_item::MirrorGeneration(p.slot)==p.generation?hand_item::MirrorActorForSlot(p.slot):nullptr;
    void* source=prop_element_tracker::FindLiveActorByKey(net::StringFromWireKey(p.source));
    return source && remote_prop::DrivenActor(p.slot)==source && remote_prop::DrivenHoldGeneration(p.slot)==p.generation?source:nullptr;
}
HostBirth* Find(const Request& p){
    for(auto& row:g_host)if(row.request.slot==p.slot && row.request.playerNo==p.playerNo && row.request.sequence==p.sequence)return &row;
    return nullptr;
}
bool Reconcile(){
    const auto epoch=g_epoch;
    for(int slot=0;slot<net::kMaxPeers;++slot){const auto person=roster_ledger::Get(slot).playerNo;
        if(person!=g_people[slot]){if(g_people[slot])OnPeerLeft(slot);if(!Current(epoch))return false;g_people[slot]=person;}}
    return true;
}
void Capture(){
    if(!Current() || Host() || g_capturing)return;
    struct Guard{Guard(){g_capturing=true;}~Guard(){g_capturing=false;}}guard;
    const auto epoch=g_epoch;
    // Native reads may erase or append rows. Process stable identities and re-find
    // after each call rather than retaining deque references across engine entry.
    std::vector<LocalBirth> rows(g_local.begin(),g_local.end());
    for(auto row:rows){
        if(!Current(epoch))return;const bool own=LocalSource(row);if(!Current(epoch))return;if(!own)continue;
        // Another bucket's POST, or a nested pump, can run before this source has
        // assigned velocity. Wait until both native source bodies have returned.
        void* source=row.source.Get();
        if(SourceActive(source))continue;
        auto& p=row.request;const auto person=roster_ledger::Get(p.slot).playerNo;if(!p.playerNo)p.playerNo=person;
        if(!p.generation)p.generation=p.source.len?(local_streams::LastHeldActor()==row.source.Get()?local_streams::CurrentHoldGen():0):hand_item::Generation(p.slot);
        if(!row.captured){
            void* actor=row.actor.Get();if(!actor)continue;FX::Birth value{};
            const auto valid=[&]{return Current(epoch) && LocalSource(row) && Current(epoch) && row.actor.Get()==actor;};
            const bool read=FX::ReadBirth(actor,value,valid);if(!Current(epoch))return;
            if(!read || !valid())continue;
            p.x=value.location.X;p.y=value.location.Y;p.z=value.location.Z;
            p.vx=value.velocity.X;p.vy=value.velocity.Y;p.vz=value.velocity.Z;p.soap=value.soap;row.captured=true;
            p.kind=uint8_t(value.kind);p.power=value.power;p.pitch=value.rotation.Pitch;p.yaw=value.rotation.Yaw;p.roll=value.rotation.Roll;
        }
        const auto it=std::find_if(g_local.begin(),g_local.end(),[&](const LocalBirth& old){return old.request.sequence==p.sequence;});
        if(it==g_local.end())continue;const bool retire=!it->captured;*it=row;
        if(retire)Retire(row.actor,row.world);if(!Current(epoch))return;
    }
}
SG::Verdict BeforeSource(const SG::Call& call){
    if(Current() && !call.fromOurCode){
        if(call.tag==kTag+2)g_washerTick.Set(call.function);
        else if(call.tag==kTag)g_tick.Set(call.function);else g_throw.Set(call.function);
    }
    if(Current() && hand_item::IsMirrorActor(call.object))return SG::Verdict::Cancel;
    if(Current() && call.tag==kTag+2 && !Host()){
        const auto epoch=g_epoch;const bool own=call.object==hand_item::LocalHandActor() || local_prop_hold::IsHeld(call.object);
        if(!Current(epoch) || !own || call.fromOurCode || !WasherReady() || !net_pump::IsInAnnouncedWorld(call.object))return SG::Verdict::Cancel;
    }
    else if(Current() && !Host()){
        const auto epoch=g_epoch;const bool own=call.object==hand_item::LocalHandActor() || local_prop_hold::IsHeld(call.object);
        if(!Current(epoch) || (own && (call.fromOurCode || !BucketReady() || !net_pump::IsInAnnouncedWorld(call.object))) ||
           (!own && call.tag==kTag+1))return SG::Verdict::Cancel;
    }
    return SG::Verdict::Run;
}
SG::Verdict BeforeParentHit(const SG::Call& call){
    return Current() && !Host() && FX::IsProjectile(call.object)?SG::Verdict::Cancel:SG::Verdict::Run;
}
SG::Verdict BeforeParentGraph(const SG::Call& call){
    if(!Current() || Host() || !FX::IsProjectile(call.object))return SG::Verdict::Run;
    const int off=call.function?R::FindParamOffset(call.function,L"EntryPoint"):-1,size=call.function?R::FunctionFrameSize(call.function):0;
    int32_t entry=0;if(off>=0 && off<=size-int(sizeof(entry)) && call.locals)std::memcpy(&entry,call.locals+off,sizeof(entry));
    // The parent's independent hit delegate cleans grime/windows before it
    // dispatches collided(). Refusing only the leaf hit would leave that path live.
    return entry==3875?SG::Verdict::Run:SG::Verdict::Cancel;
}
void AfterSource(const SG::Call& call){if(!call.fromOurCode){if(Current() && Host())CaptureHost();else Capture();}}
SG::Verdict BeforeCollision(const SG::Call& call){
    if(!Current() || !FX::IsProjectile(call.object))return SG::Verdict::Run;
    if(!Host())return SG::Verdict::Cancel;
    if(g_collisions.size()>=16 || g_cues.size()>=kMaxCues || !call.function || !call.locals)return SG::Verdict::Run;
    for(const auto& ref:g_impacted)if(ref.Is(call.object))return SG::Verdict::Run;
    const bool shot=FX::IsShot(call.object);const auto* hitName=shot?L"Hit":L"InputPin5";
    const int off=R::FindParamOffset(call.function,hitName),otherOff=R::FindParamOffset(call.function,shot?L"OtherActor":L"otherac"),size=R::FunctionFrameSize(call.function);
    if(off<0 || off>=size || otherOff<0 || otherOff>size-int(sizeof(void*)))return SG::Verdict::Run;
    void* inner=R::PropertyInnerStruct(call.function,hitName);const int bytes=inner?R::StructSize(inner):0;
    if(bytes<=0 || bytes>size-off)return SG::Verdict::Run;
    void* other=nullptr;std::memcpy(&other,call.locals+otherOff,sizeof(other));
    const void* hit=SG::OutParamPtr(call,off);if(!hit)hit=call.locals+off;
    Collision frame{};frame.actor.Set(call.object);frame.stack=call.stack;frame.depth=call.depth;frame.epoch=g_epoch;
    frame.kind=shot?1:0;
    if(FX::ReadImpact(call.object,other,call.function,hit,frame.impact) && Current(frame.epoch) && frame.actor.Get()){
        g_collisions.push_back(frame);g_impacted[g_impactCursor++%g_impacted.size()]=frame.actor;
    }
    return SG::Verdict::Run;
}
void AfterCollision(const SG::Call& call){
    auto it=std::find_if(g_collisions.rbegin(),g_collisions.rend(),[&](const Collision& row){return row.stack==call.stack && row.depth==call.depth && row.actor.Raw()==call.object;});
    if(it==g_collisions.rend())return;const auto frame=*it;g_collisions.erase(std::next(it).base());
    if(!Current(frame.epoch) || !Host() || g_cues.size()>=kMaxCues)return;
    Cue cue{};auto& p=cue.packet;p.sequence=Next(g_splashSequence);
    p.kind=frame.kind;
    p.x=frame.impact.location.X;p.y=frame.impact.location.Y;p.z=frame.impact.location.Z;
    p.nx=frame.impact.normal.X;p.ny=frame.impact.normal.Y;p.nz=frame.impact.normal.Z;
    p.ax=frame.impact.audioLocation.X;p.ay=frame.impact.audioLocation.Y;p.az=frame.impact.audioLocation.Z;
    cue.expires=active_drive::NowMs()+1500;cue.surface=cue.sound=true;
    for(int slot=1;slot<net::kMaxPeers;++slot)if(g_session->IsSlotWorldReady(slot)){
        cue.pending|=1u<<slot;cue.people[slot]=roster_ledger::Get(slot).playerNo;cue.epochs[slot]=g_peerEpoch[slot];
    }
    if(cue.pending)g_cues.push_back(cue);
}
SG::Verdict BeforeGraph(const SG::Call& call){
    if(!Current() || !FX::IsProjectile(call.object))return SG::Verdict::Run;
    const bool shot=FX::IsShot(call.object);if(shot)g_shotGraph.Set(call.function);
    if(Host())return SG::Verdict::Run;
    const int off=call.function?R::FindParamOffset(call.function,L"EntryPoint"):-1,size=call.function?R::FunctionFrameSize(call.function):0;
    int32_t entry=0;if(off>=0 && off<=size-int(sizeof(entry)) && call.locals)std::memcpy(&entry,call.locals+off,sizeof(entry));
    // BeginPlay's parent initialization remains native. Collision gameplay and a
    // pre-session Delay's destroy continuation belong exclusively to the host.
    return entry==(shot?982:1654)?SG::Verdict::Run:SG::Verdict::Cancel;
}
#include "water_projectile_hooks.inc"
#include "bucket_sound_requests.inc"
SG::Verdict ObserveBucketSound(void* source,const uint8_t* params,bool twoD){
    const auto epoch=g_epoch;
    if(!Current() || R::InCoopDispatch() || !ue_wrap::building_material::IsBucket(source) ||
       !(twoD?SG::IsObjectBodyActive(g_throw.Get(),source):SG::IsObjectBodyActive(g_tick.Get(),source)))return SG::Verdict::Run;
    CachedObjRef identity;identity.Set(source);
    const auto valid=[&]{return Current(epoch) && identity.Get();};
    FX::BucketCue cue{};
    if(!FX::ReadBucketCue(source,twoD?g_bucketSound2D.Get():g_bucketSoundAt.Get(),params,twoD,cue,valid) || !valid())return SG::Verdict::Run;
    Launch p{};p.kind=cue.kind;p.x=cue.location.X;p.y=cue.location.Y;p.z=cue.location.Z;
    if(!CuePoint(p))return SG::Verdict::Run;
    if(Host()){CanonicalBucketSound(p,0,false);return SG::Verdict::Run;}
    const auto slot=Local();if(!slot || slot>=net::kMaxPeers || g_bucketLocal.size()>=128)return SG::Verdict::Run;
    BucketLocal row{};row.source=identity;row.hand=source==hand_item::LocalHandActor();
    p.slot=slot;p.playerNo=roster_ledger::Get(slot).playerNo;p.generation=row.hand?hand_item::Generation(slot):
        (local_streams::LastHeldActor()==source?local_streams::CurrentHoldGen():0);
    const bool own=row.hand || local_prop_hold::IsHeld(source);if(!own || !valid())return SG::Verdict::Run;
    if(!row.hand){const auto key=ue_wrap::prop::GetInteractableKeyString(source);
        if(!valid() || key.size()>31)return SG::Verdict::Run;net::WireKeyFromString(key,p.source);if(!Key(p.source))return SG::Verdict::Run;}
    p.sequence=Next(g_bucketSequence);row.packet=p;row.expires=active_drive::NowMs()+1500;g_bucketLocal.push_back(row);
    return SG::Verdict::Run;
}
void Watch(){
    if(g_watched || active_drive::NowMs()<g_retryWatch)return;g_retryWatch=active_drive::NowMs()+1000;
    const bool tick=SG::WatchClassName(L"prop_bucket_C",L"ReceiveTick",kTag,&BeforeSource,&AfterSource);
    const bool use=SG::WatchClassName(L"prop_bucket_C",L"getFromWater",kTag+1,&BeforeSource,&AfterSource);
    const bool hit=SG::WatchClassName(L"prop_sponge_bucketPour_C",L"collided",kTag,&BeforeCollision,&AfterCollision);
    const bool graph=SG::WatchClassName(L"prop_sponge_bucketPour_C",L"ExecuteUbergraph_prop_sponge_bucketPour",kTag,&BeforeGraph,nullptr);
    const bool washer=SG::WatchClassName(L"prop_waterShooter_C",L"ReceiveTick",kTag+2,&BeforeSource,&AfterSource);
    const bool shotHit=SG::WatchClassName(L"prop_sponge_p_C",L"BndEvt__StaticMesh_K2Node_ComponentBoundEvent_0_ComponentHitSignature__DelegateSignature",kTag+2,&BeforeCollision,&AfterCollision);
    const bool shotGraph=SG::WatchClassName(L"prop_sponge_p_C",L"ExecuteUbergraph_prop_sponge_p",kTag+2,&BeforeGraph,nullptr);
    const bool shotBegin=SG::WatchClassName(L"prop_sponge_p_C",L"ReceiveBeginPlay",kTag+2,&BeforeShotBegin,nullptr);
    const bool parentHit=SG::WatchClassName(L"prop_sponge_C",L"BndEvt__prop_sponge_StaticMesh_K2Node_ComponentBoundEvent_0_ComponentHitSignature__DelegateSignature",kTag+3,&BeforeParentHit,nullptr);
    const bool parentGraph=SG::WatchClassName(L"prop_sponge_C",L"ExecuteUbergraph_prop_sponge",kTag+3,&BeforeParentGraph,nullptr);
    InstallLaunchHook(kWaterLaunchEmitter);
    InstallBucketSoundHooks(kBucketSoundAt,kBucketSound2D);
    g_watched=tick && use && hit && graph && washer && shotHit && shotGraph && shotBegin && parentHit && parentGraph && g_launchHookReady && g_bucketHooksReady;SG::ResolvePendingNames();
}
void DrainHost(uint64_t now){
    const auto epoch=g_epoch;const std::vector<HostBirth> rows(g_host.begin(),g_host.end());unsigned work=8;
    for(size_t visited=0;visited<rows.size();++visited){
        const auto index=g_hostCursor%rows.size();g_hostCursor=(index+1)%rows.size();const auto& copy=rows[index];
        const auto& p=copy.request;auto* row=Find(p);if(!row || row->processing)continue;
        if(!row->code && (now>row->expires || !g_session->IsSlotWorldReady(p.slot)))row->code=2,row->owed=true;
        if(!row->code){
            // Missing mirrors and native fields cost work too. Otherwise an
            // unavailable first page can issue hundreds of engine reads a frame.
            if(!work){g_hostCursor=index;break;}--work;
            void* source=HostSource(p);if(!source)continue;
            if(!SourceType(source,p.kind)){row->code=2;row->owed=true;continue;}
            CachedObjRef live;live.Set(source);
            const auto peerEpoch=g_peerEpoch[p.slot];
            const auto valid=[&]{return Current(epoch) && g_peerEpoch[p.slot]==peerEpoch && live.Get() && HostSource(p)==source && Find(p) && !Find(p)->code;};
            const auto target=element::IntentTarget::ForClientIntent(*g_session,p.slot,600.f);
            bool allowed=bool(target.Authorize(source));if(!Current(epoch))return;
            if(!valid())continue;
            if(allowed)allowed=bool(target.AuthorizeSegment({p.x,p.y,p.z},{p.x,p.y,p.z}));if(!Current(epoch))return;
            if(!valid())continue;row=Find(p);
            if(!allowed){row->code=2;row->owed=true;continue;}
            ue_wrap::FVector origin{};const bool positioned=ue_wrap::engine::TryGetActorLocation(source,origin);
            if(!Current(epoch))return;if(!valid() || !positioned)continue;row=Find(p);
            const float dx=p.x-origin.X,dy=p.y-origin.Y,dz=p.z-origin.Z;
            const float distance=dx*dx+dy*dy+dz*dz;
            if(!std::isfinite(distance) || distance>600.f*600.f){row->code=2;row->owed=true;continue;}
            row->processing=true;const auto result=FX::Author(Birth(p),valid);if(!Current(epoch))return;
            if(g_peerEpoch[p.slot]!=peerEpoch)continue;
            row=Find(p);if(!row)continue;row->processing=false;
            if(result.attempted){row->code=result.actor?1:3;row->owed=true;}
            if(result.actor && p.kind==1)QueueLaunch(Birth(p),p.slot,true);
        }
    }
    for(const auto& copy:rows){
        const auto p=copy.request;auto* row=Find(p);if(!row || !row->owed || !row->code)continue;
        if(roster_ledger::Get(p.slot).playerNo!=p.playerNo)continue;
        const Reply reply{p.sequence,p.playerNo,p.slot,row->code};
        const auto peerEpoch=g_peerEpoch[p.slot];
        const bool sent=g_session->SendReliableToSlot(p.slot,net::ReliableKind::WaterPour,&reply,sizeof(reply));
        if(!Current(epoch))return;row=Find(p);if(sent && row && g_peerEpoch[p.slot]==peerEpoch && row->code==reply.code)row->owed=false;
    }
}
void DrainLocal(uint64_t now){
    const auto epoch=g_epoch;Capture();if(!Current(epoch))return;const std::vector<LocalBirth> rows(g_local.begin(),g_local.end());
    for(const auto& row:rows){const auto& p=row.request;
        const bool own=LocalSource(row);if(!Current(epoch))return;
        if(now>row.expires || !own){
            std::erase_if(g_local,[&](const LocalBirth& old){return old.request.sequence==p.sequence;});Retire(row.actor,row.world);
            if(!Current(epoch))return;continue;
        }
        if(!row.captured || !p.generation || !p.playerNo || now<row.next)continue;
        const auto it=std::find_if(g_local.begin(),g_local.end(),[&](const LocalBirth& old){return old.request.sequence==p.sequence;});
        if(it==g_local.end())continue;it->next=now+150;
        g_session->SendReliableToSlot(0,net::ReliableKind::WaterPour,&p,sizeof(p));if(!Current(epoch))return;
    }
}
void DrainCues(uint64_t now){
    const auto epoch=g_epoch;uint32_t blocked=0;std::deque<Cue> keep;const auto count=g_cues.size();
    bool soundBlocked=false,surfaceBlocked=false;
    for(size_t i=0;i<count && !g_cues.empty();++i){
        auto cue=g_cues.front();g_cues.pop_front();if(now>cue.expires)continue;
        const auto valid=[&]{return Current(epoch);};const auto impact=Impact(cue.packet);
        if(!surfaceBlocked && !cue.surface && FX::Surface(impact,valid))cue.surface=true;if(!Current(epoch))return;
        if(!soundBlocked && !cue.sound && (cue.bucket?FX::BucketSound({cue.launch.kind,{cue.launch.x,cue.launch.y,cue.launch.z}},valid):FX::Sound(impact,valid)))cue.sound=true;if(!Current(epoch))return;
        soundBlocked|=!cue.sound;surfaceBlocked|=!cue.surface;
        for(int slot=1;slot<net::kMaxPeers;++slot)if((cue.pending&~blocked)&(1u<<slot)){
            if(!g_session->IsSlotWorldReady(slot) || roster_ledger::Get(slot).playerNo!=cue.people[slot] || g_peerEpoch[slot]!=cue.epochs[slot]){cue.pending&=~(1u<<slot);continue;}
            const void* payload=cue.bucket?static_cast<const void*>(&cue.launch):static_cast<const void*>(&cue.packet);
            const bool sent=g_session->SendReliableToSlot(slot,cue.bucket?net::ReliableKind::WaterLaunch:net::ReliableKind::WaterSplash,
                payload,cue.bucket?sizeof(cue.launch):sizeof(cue.packet));
            if(!Current(epoch))return;if(sent || roster_ledger::Get(slot).playerNo!=cue.people[slot] || g_peerEpoch[slot]!=cue.epochs[slot])cue.pending&=~(1u<<slot);
        }
        blocked|=cue.pending;if(cue.pending || !cue.surface || !cue.sound)keep.push_back(cue);
    }
    g_cues.insert(g_cues.begin(),keep.begin(),keep.end());
}
}
void Install(net::Session* session){
    const auto world=ue_wrap::world_identity::Generation();
    if(g_session!=session || g_world!=world){const auto epoch=g_epoch;OnDisconnect();if(g_epoch!=epoch+1 || g_session)return;g_session=session;g_world=world;}
    Watch();
}
bool ObserveBirth(void* source,void* actor){
    const auto epoch=g_epoch;
    if(!Current() || Host() || R::InCoopDispatch() || !FX::IsProjectile(actor))return false;
    const bool shot=FX::IsShot(actor);
    if(!SourceType(source,shot?1:0) || (shot?!SG::IsObjectBodyActive(g_washerTick.Get(),source):
       (!SG::IsObjectBodyActive(g_tick.Get(),source) && !SG::IsObjectBodyActive(g_throw.Get(),source))))return false;
    const auto slot=Local();if(slot>=net::kMaxPeers)return false;
    LocalBirth row{};row.actor.Set(actor);row.source.Set(source);row.world=g_world;
    const bool hand=source==hand_item::LocalHandActor();
    auto& p=row.request;p.slot=slot;p.playerNo=roster_ledger::Get(slot).playerNo;p.kind=shot?1:0;p.power=1;
    p.generation=hand?hand_item::Generation(slot):(local_streams::LastHeldActor()==source?local_streams::CurrentHoldGen():0);
    const bool own=hand || local_prop_hold::IsHeld(source);
    if(!Current(epoch) || !row.source.Get() || !row.actor.Get()){Retire(row.actor,row.world);return true;}
    if(!own)return false;
    for(const auto& pending:g_local)if(pending.actor.Is(actor))return true;
    if(g_local.size()>=128){prop_lifecycle::DestroyLocalProp(actor,true);return true;}
    p.sequence=Next(g_sequence);
    if(!hand){const auto key=ue_wrap::prop::GetInteractableKeyString(source);
        if(!Current(epoch) || !row.source.Get() || !row.actor.Get()){Retire(row.actor,row.world);return true;}
        if(key.size()>sizeof(p.source.data)){Retire(row.actor,row.world);return true;}
        net::WireKeyFromString(key,p.source);if(!Key(p.source)){Retire(row.actor,row.world);return true;}}
    row.expires=active_drive::NowMs()+1500;g_local.push_back(row);return true;
}
void ObserveHostBirth(void* source,void* actor){
    const auto epoch=g_epoch;
    if(!Current() || !Host() || R::InCoopDispatch() || !FX::IsShot(actor) || !ue_wrap::pressure_washer::IsWasher(source) ||
       !SG::IsObjectBodyActive(g_washerTick.Get(),source) || g_nativeShots.size()>=128)return;
    for(const auto& row:g_nativeShots)if(row.actor.Is(actor))return;
    HostShot row{};row.actor.Set(actor);row.source.Set(source);row.epoch=epoch;row.expires=active_drive::NowMs()+1500;
    if(Current(epoch))g_nativeShots.push_back(row);
}
void Tick(){
    if(!Current() || g_ticking)return;struct Guard{Guard(){g_ticking=true;}~Guard(){g_ticking=false;}}guard;
    Watch();if(!Reconcile())return;const auto epoch=g_epoch,now=active_drive::NowMs();
    if(Host()){CaptureHost();if(Current(epoch))DrainHost(now);}else DrainLocal(now);
    if(Current(epoch))DrainBucketSounds(now);if(Current(epoch))DrainCues(now);
}
void Receive(const Request& p,uint8_t sender){
    if(!Current() || !Host() || !sender || sender>=net::kMaxPeers || sender!=p.slot || !p.sequence || !p.playerNo || !p.generation ||
       p.kind>1 || !FX::Valid(Birth(p)) || (p.source.len && (!Key(p.source) || p.generation>65535)) || !g_session->IsSlotWorldReady(sender))return;
    if(!Reconcile() || roster_ledger::Get(sender).playerNo!=p.playerNo)return;
    if(auto* old=Find(p)){if(std::memcmp(&old->request,&p,sizeof(p))==0 && old->code)old->owed=true;return;}
    if(!Unseen(sender,p.sequence))return;
    const auto now=active_drive::NowMs();if(now-g_rateAt[sender]>=1000){g_rateAt[sender]=now;g_rate[sender]=0;}
    if(g_rate[sender]>=128)return;
    if(g_host.size()>=512){auto it=std::find_if(g_host.begin(),g_host.end(),[](const HostBirth& row){return row.code && !row.owed && !row.processing;});
        if(it==g_host.end())return;g_host.erase(it);}
    ++g_rate[sender];Remember(sender,p.sequence);g_host.push_back({p,now+1500});
}
void ReceiveReply(const Reply& p,uint8_t sender){
    if(!Current() || Host() || sender || !p.sequence || p.slot!=Local() || p.code<1 || p.code>3 ||
       roster_ledger::Get(p.slot).playerNo!=p.playerNo)return;
    std::erase_if(g_local,[&](const LocalBirth& row){return row.request.sequence==p.sequence && row.request.playerNo==p.playerNo;});
}
void ReceiveSplash(const Splash& p,uint8_t sender){
    if(!Current() || Host() || sender || !p.sequence || p.kind>2 || p.reserved[0] || p.reserved[1] || p.reserved[2] || !FX::Valid(Impact(p)) ||
       (g_lastSplash && !After(p.sequence,g_lastSplash)) || g_cues.size()>=kMaxCues)return;
    g_lastSplash=p.sequence;Cue cue{};cue.packet=p;cue.surface=p.kind==2;cue.expires=active_drive::NowMs()+1500;g_cues.push_back(cue);
}
void ReceiveLaunch(const Launch& p,uint8_t sender){ReceiveBucketSound(p,sender);}
void OnPeerLeft(int slot){
    if(slot<0 || slot>=net::kMaxPeers)return;++g_peerEpoch[slot];g_people[slot]=0;g_seen[slot]=0;g_seenBirths[slot].reset();g_rateAt[slot]=0;g_rate[slot]=0;
    std::erase_if(g_host,[slot](const HostBirth& row){return row.request.slot==slot;});
    ClearBucketSounds(slot);
    for(auto& cue:g_cues)cue.pending&=~(1u<<slot);
    if(slot==Local()){auto local=g_local;g_local.clear();for(const auto& row:local)Retire(row.actor,row.world);}
}
void OnDisconnect(){
    auto local=g_local;++g_epoch;g_session=nullptr;g_world=g_sequence=g_splashSequence=g_lastSplash=0;
    g_local.clear();g_host.clear();g_cues.clear();g_collisions.clear();g_impacted={};g_impactCursor=g_hostCursor=0;g_tick.Reset();g_throw.Reset();
    g_seenBirths={};g_nativeShots.clear();g_washerTick.Reset();g_shotBegin.Reset();g_shotGraph.Reset();
    ClearBucketSounds(-1);
    for(int slot=0;slot<net::kMaxPeers;++slot){g_people[slot]=0;g_seen[slot]=0;g_rateAt[slot]=0;g_rate[slot]=0;}
    for(const auto& row:local)Retire(row.actor,row.world);
}
}
