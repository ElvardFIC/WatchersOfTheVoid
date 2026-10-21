#include "coop/props/drink_spill_sync.h"
#include "coop/interactables/grime_sync.h"
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
#include "coop/props/remote_prop.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/prop_explosion.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/devices/drink_spill.h"
#include "ue_wrap/devices/grime.h"
#include "ue_wrap/engine/engine.h"
#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <map>
#include <set>
#include <vector>
namespace coop::drink_spill_sync {
namespace {
namespace N=ue_wrap::drink_spill;
namespace R=ue_wrap::reflection;
namespace SG=ue_wrap::script_gate;
using ue_wrap::CachedObjRef;
using Packet=net::DrinkSpillPayload;
using LaunchPacket=net::DrinkLaunchPayload;
using PourPacket=net::DrinkPourPayload;
UE_FACT_FN(kFinish,"drink_spill_sync",L"GameplayStatics",L"FinishSpawningActor",Declared);
UE_FACT_FN(kPourSound,"drink_spill_sync",L"GameplayStatics",L"PlaySoundAtLocation",Declared);
constexpr int kTag=0x44524950;
net::Session* g_session=nullptr;
net::Role g_role=net::Role::Host;
uint64_t g_epoch=0,g_nonce=0;
uint32_t g_world=0,g_sequence=0,g_id=0;
bool g_hook=false,g_watch=false,g_ticking=false,g_authoring=false;
bool g_breakWatch=false,g_fireWatch=false,g_flightWatch=false,g_graphWatch=false;
bool g_gasBreakWatch=false,g_gasFireWatch=false;
bool g_coffeeWatch=false;
bool g_scorchWatch=false;
bool g_pourHook=false;
CachedObjRef g_finishFunction;
CachedObjRef g_pourFunction;
std::array<uint64_t,net::kMaxPeers> g_peerEpoch{};
std::array<uint16_t,net::kMaxPeers> g_people{};
std::array<uint32_t,net::kMaxPeers> g_high{};
std::array<std::bitset<256>,net::kMaxPeers> g_seen{};
std::array<uint64_t,net::kMaxPeers> g_rateAt{};
std::array<unsigned,net::kMaxPeers> g_rate{};
struct Call {CachedObjRef source;uint64_t invocation=0;Packet owner{};bool poured=false;};
struct Flight {CachedObjRef actor;Packet owner{};uint64_t expires=0;};
struct Candidate {CachedObjRef actor;Packet owner{};uint64_t expires=0;bool baseline=false;};
struct Local {CachedObjRef actor;LaunchPacket packet{};uint64_t expires=0,next=0,invocation=0;bool captured=false;};
struct Request {LaunchPacket packet{};uint64_t expires=0,peerEpoch=0;bool processing=false,done=false,owed=false,attempted=false;CachedObjRef output;};
struct Record {CachedObjRef actor,adopted;Packet packet{};uint32_t pending=0,joins=0;uint64_t revision=0,soundUntil=0;bool sound=false,retired=false,spawned=false;
    bool failed=false,paint=false,mayAdopt=false;std::array<uint16_t,net::kMaxPeers> people{};std::array<uint64_t,net::kMaxPeers> epochs{};};
struct ActorId {CachedObjRef actor;uint32_t id=0;int32_t serial=0;};
std::vector<Call> g_calls;
struct ScorchCall {CachedObjRef source;uint64_t invocation=0,epoch=0;bool shared=true;};
std::vector<ScorchCall> g_scorchCalls;
struct ScorchOrigin {CachedObjRef actor;bool shared=false;};
std::map<void*,ScorchOrigin> g_scorchOrigins;
uint32_t g_scorchWorld=0;
std::map<void*,Flight> g_flights;
std::deque<Candidate> g_candidates;
std::map<uint32_t,Local> g_local;
std::map<void*,CachedObjRef> g_unassigned,g_baselineSeen;
struct Existing {CachedObjRef actor;N::State state;};
std::vector<Existing> g_existing;
bool g_baselineComplete=false;
std::map<std::pair<uint8_t,uint32_t>,Request> g_requests;
std::map<uint32_t,Record> g_records;
std::set<uint32_t> g_pendingRecords;
std::map<void*,ActorId> g_actorIds;
uint64_t g_revision=0;
uint32_t g_recordCursor=0;
uint32_t g_localCursor=0;
std::pair<uint8_t,uint32_t> g_requestCursor{};
uint64_t Now(){return active_drive::NowMs();}
uint32_t Next(uint32_t& n){if(!++n)++n;return n;}
void EraseTemporary(std::map<void*,CachedObjRef>& rows,const CachedObjRef& ref){
    const auto it=rows.find(ref.Raw());if(it!=rows.end() && (!it->second.Get() || (ref.Get() && it->second.Is(ref.Get()))))rows.erase(it);
}
bool Host(){return g_session && g_session->role()==net::Role::Host;}
uint8_t LocalSlot(){return players::Registry::Get().LocalPeerId();}
bool Current(uint64_t epoch){return g_session && g_session->connected() && g_session->role()==g_role && epoch==g_epoch && g_world==ue_wrap::world_identity::Generation();}
bool Current(){return Current(g_epoch);}
uint64_t Nonce(){auto nonce=uint64_t(std::chrono::steady_clock::now().time_since_epoch().count())^(g_epoch<<32);return nonce?nonce:1;}
void World(){if(g_session && g_world!=ue_wrap::world_identity::Generation()){
    auto* session=g_session;OnDisconnect();g_session=session;g_world=ue_wrap::world_identity::Generation();g_nonce=Nonce();}}
bool Key(const net::WireKey& key){if(key.len>sizeof(key.data))return false;
    for(unsigned i=0;i<key.len;++i)if(uint8_t(key.data[i])<33 || uint8_t(key.data[i])>126)return false;
    return !key.len || net::StringFromWireKey(key)!=L"None";}
N::State Decode(const Packet& p){return {{p.x,p.y,p.z},{p.sx,p.sy,p.sz},{p.pitch,p.yaw,p.roll},{p.dpitch,p.dyaw,p.droll},p.process,p.type,p.kind};}
void Encode(const N::State& s,Packet& p){p.x=s.location.X;p.y=s.location.Y;p.z=s.location.Z;p.sx=s.scale.X;p.sy=s.scale.Y;p.sz=s.scale.Z;
    p.pitch=s.rotation.Pitch;p.yaw=s.rotation.Yaw;p.roll=s.rotation.Roll;p.dpitch=s.decalRotation.Pitch;p.dyaw=s.decalRotation.Yaw;p.droll=s.decalRotation.Roll;
    p.process=s.process;p.type=s.type;p.kind=s.kind;}
bool Valid(const Packet& p){return p.op>=1 && p.op<=3 && !p.reserved[0] && !p.reserved[1] && !p.reserved[2] && Key(p.source) && N::Valid(Decode(p));}
bool SameShape(const Packet& a,const Packet& b){
    return a.x==b.x && a.y==b.y && a.z==b.z && a.sx==b.sx && a.sy==b.sy && a.sz==b.sz &&
        a.pitch==b.pitch && a.yaw==b.yaw && a.roll==b.roll && a.dpitch==b.dpitch && a.dyaw==b.dyaw && a.droll==b.droll && a.type==b.type && a.kind==b.kind;
}
std::wstring DynamicKey(uint64_t nonce,uint32_t id){wchar_t key[32]{};swprintf_s(key,L"ds_%016llx_%08x",static_cast<unsigned long long>(nonce),id);return key;}
bool Own(void* source){return source==hand_item::LocalHandActor() || local_prop_hold::IsHeld(source);}
template<class P>bool SourcePacket(void* source,P& p){
    p.slot=LocalSlot();if(p.slot>=net::kMaxPeers)return false;p.playerNo=roster_ledger::Get(p.slot).playerNo;
    if(source==hand_item::LocalHandActor())p.generation=hand_item::Generation(p.slot);
    else if(local_prop_hold::IsHeld(source) && local_streams::LastHeldActor()==source){
        p.generation=local_streams::CurrentHoldGen();const auto key=ue_wrap::prop::GetInteractableKeyString(source);
        if(key.empty() || key.size()>31)return false;net::WireKeyFromString(key,p.source);
    }
    return p.slot && p.playerNo && p.generation && Key(p.source);
}
template<class P>void* HostSource(const P& p){
    if(p.slot>=net::kMaxPeers || roster_ledger::Get(p.slot).playerNo!=p.playerNo)return nullptr;
    if(!p.source.len)return hand_item::MirrorGeneration(p.slot)==p.generation?hand_item::MirrorActorForSlot(p.slot):nullptr;
    void* actor=prop_element_tracker::FindLiveActorByKey(net::StringFromWireKey(p.source));
    return actor && remote_prop::DrivenActor(p.slot)==actor && remote_prop::DrivenHoldGeneration(p.slot)==p.generation?actor:nullptr;
}
bool Reconcile(){const auto epoch=g_epoch;for(int slot=0;slot<net::kMaxPeers;++slot){const auto person=roster_ledger::Get(slot).playerNo;
    if(g_people[slot]==person)continue;if(g_people[slot])OnPeerLeft(slot);if(!Current(epoch))return false;g_people[slot]=person;}return true;}
void QueueRecord(Record& row,int slot,bool join){
    if(slot<=0 || slot>=net::kMaxPeers || !g_session->IsSlotWorldReady(slot))return;
    row.pending|=1u<<slot;if(join)row.joins|=1u<<slot;else row.joins&=~(1u<<slot);
    row.people[slot]=roster_ledger::Get(slot).playerNo;row.epochs[slot]=g_peerEpoch[slot];row.revision=++g_revision;
    if(row.packet.id)g_pendingRecords.insert(row.packet.id);
}
uint32_t Register(void* actor,Packet p){
    const auto assigned=g_actorIds.find(actor);if(assigned!=g_actorIds.end() && assigned->second.actor.Is(actor))return assigned->second.id;
    if(g_records.size()>=4096)return 0;Record row{};row.actor.Set(actor);const bool baseline=p.op==2;p.id=Next(g_id);p.nonce=g_nonce;p.op=1;
    row.packet=p;row.sound=false;row.soundUntil=Now()+1500;row.revision=++g_revision;
    for(int slot=1;slot<net::kMaxPeers;++slot)QueueRecord(row,slot,baseline);
    g_unassigned.erase(actor);g_actorIds[actor]={row.actor,p.id,R::SlotSerial(row.actor.Idx())};g_records[p.id]=row;grime_sync::RegisterDynamic(actor);return p.id;
}
bool FlightGatesReady(){return SG::ClassNameWatchLive(L"grimeProjectile_C",L"ReceiveTick",kTag+3) &&
    SG::ClassNameWatchLive(L"grimeProjectile_C",L"ExecuteUbergraph_grimeProjectile",kTag+4);}
SG::Verdict BeforeFlight(const SG::Call& call){
    World();if(!Current() || Host())return SG::Verdict::Run;uint8_t kind=0;
    const bool tracked=std::any_of(g_local.begin(),g_local.end(),[&](const auto& row){return row.second.actor.Is(call.object);});
    if(!tracked && !N::ProjectileKind(call.object,kind))return SG::Verdict::Run;
    if(call.tag==kTag+3)return SG::Verdict::Cancel;
    const int off=call.function?R::FindParamOffset(call.function,L"EntryPoint"):-1,size=call.function?R::FunctionFrameSize(call.function):0;
    int32_t entry=-1;if(call.locals && off>=0 && off<=size-int(sizeof(entry)))std::memcpy(&entry,call.locals+off,sizeof(entry));
    // BeginPlay installs native velocity; Tick owns tracing, decal birth and shortCircuit.
    return entry==15?SG::Verdict::Run:SG::Verdict::Cancel;
}
struct PourLocal{PourPacket packet{};uint64_t invocation=0,expires=0,next=0,peerEpoch=0;};
struct PourRequest{PourPacket packet{};uint64_t expires=0,peerEpoch=0;bool done=false,owed=false;};
struct PourCue{PourPacket packet{};uint64_t expires=0;uint32_t pending=0;bool sound=false;
    std::array<uint16_t,net::kMaxPeers> people{};std::array<uint64_t,net::kMaxPeers> epochs{};};
struct PourReplay{uint32_t high=0;std::bitset<256> seen;};
std::map<std::pair<uint8_t,uint16_t>,PourReplay> g_pourCanonicalSeen;
std::map<uint32_t,PourLocal> g_pourLocal;
std::map<std::pair<uint8_t,uint32_t>,PourRequest> g_pourRequests;
std::map<uint32_t,PourCue> g_pours;
std::array<uint32_t,net::kMaxPeers> g_pourHigh{};
std::array<std::bitset<256>,net::kMaxPeers> g_pourSeen{};
std::array<uint64_t,net::kMaxPeers> g_pourRateAt{};
std::array<unsigned,net::kMaxPeers> g_pourRate{};
uint32_t g_pourSequence=0,g_pourId=0,g_pourLocalCursor=0;
std::pair<uint8_t,uint32_t> g_pourRequestCursor{};
bool ValidPour(const PourPacket& p){
    if(p.kind>2 || p.op>2 || p.reserved[0] || p.reserved[1] || p.reserved[2] || !Key(p.source) || (p.kind==2 && p.source.len))return false;
    for(float value:{p.x,p.y,p.z})if(!std::isfinite(value) || std::fabs(value)>1.e7f)return false;return true;
}
bool SamePour(PourPacket a,PourPacket b){a.op=b.op=0;return std::memcmp(&a,&b,sizeof(a))==0;}
bool TakePourSequence(uint32_t& high,std::bitset<256>& seen,uint32_t sequence){
    if(high && int32_t(sequence-high)<=0){const auto age=high-sequence;if(age>=256 || seen[age])return false;}
    if(!high){high=sequence;seen.reset();}else if(int32_t(sequence-high)>0){const auto shift=sequence-high;if(shift>=256)seen.reset();else seen<<=shift;high=sequence;}
    seen.set(high-sequence);return true;
}
bool FreshPour(uint8_t slot,uint32_t sequence){return TakePourSequence(g_pourHigh[slot],g_pourSeen[slot],sequence);}
bool FreshCanonicalPour(const PourPacket& p){
    const auto key=std::make_pair(p.slot,p.playerNo);
    if(!g_pourCanonicalSeen.contains(key) && g_pourCanonicalSeen.size()>=4096)return false;
    auto& row=g_pourCanonicalSeen[key];return TakePourSequence(row.high,row.seen,p.sequence);
}
bool QueuePour(PourPacket p,bool sound){
    if(g_pours.size()>=128)return false;PourCue cue{};cue.packet=p;cue.packet.op=1;cue.sound=sound;cue.expires=Now()+1500;
    if(Host())for(uint8_t slot=1;slot<net::kMaxPeers;++slot)if(slot!=p.slot && g_session->IsSlotWorldReady(slot)){
        cue.pending|=1u<<slot;cue.people[slot]=roster_ledger::Get(slot).playerNo;cue.epochs[slot]=g_peerEpoch[slot];}
    g_pours[Next(g_pourId)]=cue;return true;
}
void ObservePourSound(void*,void* source,void*){
    if(!Current() || g_authoring || !source)return;const auto epoch=g_epoch;
    auto call=std::find_if(g_calls.rbegin(),g_calls.rend(),[&](const Call& row){return !row.poured && row.source.Is(source) && SG::IsInvocationActive(row.invocation);});
    if(call==g_calls.rend())return;const auto captured=*call;
    const auto valid=[&]{return Current(epoch) && captured.source.Get()==source && SG::IsInvocationActive(captured.invocation);};
    uint8_t kind=0;ue_wrap::FVector point{};
    if(!N::ReadPourCue(source,ue_wrap::ufunction_hook::CurrentCallerFrame(),kind,point,valid) || !valid() || kind!=captured.owner.kind)return;
    call=std::find_if(g_calls.rbegin(),g_calls.rend(),[&](const Call& row){return row.invocation==captured.invocation && row.source.Is(source);});
    if(call==g_calls.rend() || call->poured)return;call->poured=true;
    PourPacket p{};p.source=captured.owner.source;p.generation=captured.owner.generation;p.playerNo=captured.owner.playerNo;
    p.slot=captured.owner.slot;p.kind=kind;p.sequence=Next(g_pourSequence);p.x=point.X;p.y=point.Y;p.z=point.Z;
    if(Host()){p.playerNo=roster_ledger::Get(0).playerNo;QueuePour(p,false);}
    else if(g_pourLocal.size()<128)g_pourLocal[p.sequence]={p,captured.invocation,Now()+3000,0,g_peerEpoch[p.slot]};
}
void DrainPourLocal(){
    const auto epoch=g_epoch;const auto pending=g_pourLocal;unsigned work=8;
    auto it=pending.upper_bound(g_pourLocalCursor);if(it==pending.end())it=pending.begin();
    for(size_t visited=0;visited<pending.size() && work;++visited){const auto sequence=it->first;const auto row=it->second;if(++it==pending.end())it=pending.begin();
        if(!Current(epoch))return;
        const auto& p=row.packet;if(Now()>row.expires || p.slot!=LocalSlot() || roster_ledger::Get(p.slot).playerNo!=p.playerNo || g_peerEpoch[p.slot]!=row.peerEpoch){
            g_pourLocal.erase(sequence);--work;g_pourLocalCursor=sequence;continue;}
        if(SG::IsInvocationActive(row.invocation) || Now()<row.next)continue;--work;g_pourLocalCursor=sequence;
        auto live=g_pourLocal.find(sequence);if(live==g_pourLocal.end())continue;live->second.next=Now()+150;
        g_session->SendReliableToSlot(0,net::ReliableKind::DrinkPour,&p,sizeof(p));if(!Current(epoch))return;
    }
}
void DrainPourRequests(){
    const auto epoch=g_epoch;auto* session=g_session;const auto pending=g_pourRequests;unsigned work=8;
    std::array<uint32_t,net::kMaxPeers> first{};
    for(const auto& [key,row]:pending)if(!row.done && (!first[key.first] || int32_t(key.second-first[key.first])<0))first[key.first]=key.second;
    auto it=pending.upper_bound(g_pourRequestCursor);if(it==pending.end())it=pending.begin();
    for(size_t visited=0;visited<pending.size() && work;++visited){const auto key=it->first;const auto row=it->second;if(++it==pending.end())it=pending.begin();
        auto live=g_pourRequests.find(key);if(live==g_pourRequests.end())continue;const auto p=row.packet;
        const auto person=[&]{return Current(epoch) && Host() && g_session==session && g_peerEpoch[p.slot]==row.peerEpoch &&
            roster_ledger::Get(p.slot).playerNo==p.playerNo && session->IsSlotWorldReady(p.slot);};
        if(!person()){if(!Current(epoch))return;g_pourRequests.erase(key);continue;}
        if(!live->second.done && Now()>live->second.expires){live->second.done=live->second.owed=true;live->second.packet.op=2;live->second.expires=Now()+3000;}
        if(!live->second.done){
            if(first[p.slot]!=p.sequence)continue;
            --work;g_pourRequestCursor=key;void* source=HostSource(p);CachedObjRef ref;ref.Set(source);std::wstring retiredClass;
            const bool retired=!p.source.len && !source && hand_item::RecentGenerationClass(p.slot,p.generation,retiredClass);
            const auto valid=[&]{auto request=g_pourRequests.find(key);if(!person() || request==g_pourRequests.end() || request->second.peerEpoch!=row.peerEpoch)return false;
                if(source)return ref.Get()==source && HostSource(p)==source && person();
                std::wstring cls;return retired && hand_item::RecentGenerationClass(p.slot,p.generation,cls) && cls==retiredClass && person();};
            uint8_t kind=0;const bool known=source?N::SourceKind(source,kind):(retired && N::SourceClassKind(retiredClass,kind));
            if(!known || !valid() || kind!=p.kind)continue;
            const auto target=element::IntentTarget::ForClientIntent(*session,p.slot,600.f);
            const bool reach=!source || bool(target.Authorize(source));if(!valid())continue;
            const bool point=reach && bool(target.AuthorizeSegment({p.x,p.y,p.z},{p.x,p.y,p.z}));if(!point || !valid())continue;
            if(source){ue_wrap::FVector location{};if(!ue_wrap::engine::TryGetActorLocation(source,location) || !valid())continue;
                const float x=p.x-location.X,y=p.y-location.Y,z=p.z-location.Z,distance=x*x+y*y+z*z;if(!std::isfinite(distance) || distance>600.f*600.f)continue;}
            if(g_pours.size()>=128)continue;
            live=g_pourRequests.find(key);if(live==g_pourRequests.end() || !valid())continue;
            live->second.done=live->second.owed=true;live->second.packet.op=1;live->second.expires=Now()+3000;
            QueuePour(live->second.packet,true);
        }
        live=g_pourRequests.find(key);if(live==g_pourRequests.end())continue;
        if(live->second.owed && work){--work;g_pourRequestCursor=key;const auto reply=live->second.packet;
            const bool sent=session->SendReliableToSlot(p.slot,net::ReliableKind::DrinkPour,&reply,sizeof(reply));if(!Current(epoch))return;
            live=g_pourRequests.find(key);if(sent && person() && live!=g_pourRequests.end() && SamePour(live->second.packet,reply))live->second.owed=false;}
        live=g_pourRequests.find(key);if(live!=g_pourRequests.end() && live->second.done && Now()>live->second.expires)g_pourRequests.erase(live);
    }
}
void DrainPourCues(){
    const auto epoch=g_epoch;const auto pending=g_pours;unsigned work=8;bool localBlocked=false;uint32_t blocked=0;
    for(const auto& [id,cue]:pending){
        if(!Current(epoch) || !work)return;auto live=g_pours.find(id);if(live==g_pours.end())continue;
        const auto current=[&]{return Current(epoch) && g_pours.contains(id);};
        if(Now()>cue.expires){--work;g_pours.erase(id);continue;}
        if(cue.sound && !localBlocked){--work;const bool played=N::PourSound(cue.packet.kind,{cue.packet.x,cue.packet.y,cue.packet.z},current);if(!Current(epoch))return;
            live=g_pours.find(id);if(played && live!=g_pours.end())live->second.sound=false;else localBlocked=true;}
        for(uint8_t slot=1;slot<net::kMaxPeers && work;++slot)if((cue.pending&(1u<<slot)) && !(blocked&(1u<<slot))){
            const auto recipient=[&]{return current() && g_session->IsSlotWorldReady(slot) && g_peerEpoch[slot]==cue.epochs[slot] && roster_ledger::Get(slot).playerNo==cue.people[slot];};
            if(!current())break;if(!recipient()){g_pours[id].pending&=~(1u<<slot);continue;}--work;
            const bool sent=g_session->SendReliableToSlot(slot,net::ReliableKind::DrinkPour,&cue.packet,sizeof(cue.packet));if(!Current(epoch))return;
            if(sent && recipient())g_pours[id].pending&=~(1u<<slot);else if(recipient())blocked|=1u<<slot;
        }
        live=g_pours.find(id);if(live!=g_pours.end() && !live->second.sound && !live->second.pending)g_pours.erase(live);
    }
}
void ReceivePourPacket(const PourPacket& p,uint8_t sender){
    World();if(!Current() || !ValidPour(p) || p.slot>=net::kMaxPeers || !p.sequence || !Reconcile())return;
    if(!Host()){
        if(sender || !p.op || (!p.playerNo && p.slot))return;
        if(p.slot==LocalSlot()){const auto local=g_pourLocal.find(p.sequence);
            if(p.playerNo==roster_ledger::Get(p.slot).playerNo && local!=g_pourLocal.end() && SamePour(local->second.packet,p))g_pourLocal.erase(local);return;}
        if(p.op!=1 || g_pours.size()>=128 || !FreshCanonicalPour(p))return;QueuePour(p,true);return;
    }
    if(!sender || sender>=net::kMaxPeers || sender!=p.slot || p.op || !p.generation || !p.playerNo ||
       p.playerNo!=roster_ledger::Get(sender).playerNo || !g_session->IsSlotWorldReady(sender))return;
    const auto key=std::make_pair(sender,p.sequence);auto old=g_pourRequests.find(key);
    if(old!=g_pourRequests.end()){if(old->second.done && SamePour(old->second.packet,p))old->second.owed=true;return;}
    if(g_pourRequests.size()>=256 || std::count_if(g_pourRequests.begin(),g_pourRequests.end(),[&](const auto& row){return row.first.first==sender;})>=64)return;
    const auto now=Now();if(now-g_pourRateAt[sender]>=1000){g_pourRateAt[sender]=now;g_pourRate[sender]=0;}
    if(g_pourRate[sender]>=32 || !FreshPour(sender,p.sequence))return;++g_pourRate[sender];
    g_pourRequests[key]={p,now+1500,g_peerEpoch[sender]};
}

SG::Verdict BeforeSourceImpl(const SG::Call& call,bool breaking){
    World();if(!Current() || call.fromOurCode)return SG::Verdict::Run;const auto epoch=g_epoch;
    Packet owner{};if(!(breaking?N::BreakSourceKind(call.object,owner.kind):N::SourceKind(call.object,owner.kind)) || !Current(epoch))return SG::Verdict::Run;
    if(hand_item::IsMirrorActor(call.object))return SG::Verdict::Cancel;
    if(Host()){if(owner.kind!=2)for(uint8_t slot=1;slot<net::kMaxPeers;++slot)if(remote_prop::DrivenActor(slot)==call.object)return SG::Verdict::Cancel;}
    else if((owner.kind>=2 && call.object!=hand_item::LocalHandActor()) || g_local.size()>=128 || !FlightGatesReady() || !Own(call.object) || !SourcePacket(call.object,owner) ||
        !net_pump::IsInAnnouncedWorld(call.object))return SG::Verdict::Cancel;
    if(!Current(epoch) || !g_hook || !g_finishFunction.Get() || g_records.size()>=4096)return SG::Verdict::Cancel;
    CachedObjRef source;source.Set(call.object);
    // Optional source-audio metadata must not stall the authoritative flight path.
    if(g_pourHook && g_pourFunction.Get())N::PourReady(call.object);
    if(!Current(epoch) || !source.Get())return SG::Verdict::Cancel;
    std::erase_if(g_calls,[](const Call& row){return !row.source.Get() || !SG::IsInvocationActive(row.invocation);});
    if(!call.invocation || g_calls.size()>=32)return SG::Verdict::Cancel;
    Call row{};row.source=source;row.invocation=call.invocation;row.owner=owner;g_calls.push_back(row);return SG::Verdict::Run;
}
SG::Verdict BeforeSource(const SG::Call& call){return BeforeSourceImpl(call,false);}
SG::Verdict ObserveHostBreak(const SG::Call& call){
    World();if(!Current() || !Host() || !g_hook || !call.invocation || hand_item::IsMirrorActor(call.object))return SG::Verdict::Run;
    const auto epoch=g_epoch;CachedObjRef ref;ref.Set(call.object);Packet owner{};
    if(!N::BreakSourceKind(call.object,owner.kind) || !Current(epoch) || !ref.Get())return SG::Verdict::Run;
    std::erase_if(g_calls,[](const Call& row){return !row.source.Get() || !SG::IsInvocationActive(row.invocation);});
    if(g_calls.size()<32)g_calls.push_back({ref,call.invocation,owner});return SG::Verdict::Run;
}
SG::Verdict BeforeGasBreak(const SG::Call& call){
    World();if(!Current())return SG::Verdict::Run;const auto epoch=g_epoch;
    uint8_t kind=0;if(!N::BreakSourceKind(call.object,kind) || kind!=3)return SG::Verdict::Run;
    if(!Current(epoch) || hand_item::IsMirrorActor(call.object))return SG::Verdict::Cancel;
    if(Host())return ObserveHostBreak(call);
    if(hand_item::IsHandAxisActor(call.object) && call.object==hand_item::LocalHandActor())return BeforeSourceImpl(call,true);
    return SG::Verdict::Cancel;
}
#include "drink_scorch.inc"
void ObserveFinish(void*,void* source,void* actor){
    if(!Current() || g_authoring || !actor)return;const auto epoch=g_epoch;
    if(ObserveScorchFinish(source,actor))return;
    if(!Current(epoch))return;
    if(N::IsProjectile(actor)){
        const auto call=std::find_if(g_calls.rbegin(),g_calls.rend(),[&](const Call& row){return row.source.Is(source) && SG::IsInvocationActive(row.invocation);});
        if(call==g_calls.rend() || !Current(epoch)){
            uint8_t kind=0;if(Current(epoch) && !Host() && N::ProjectileKind(actor,kind) && kind==3 && Current(epoch))ue_wrap::engine::DestroyActor(actor);
            return;
        }
        if(Host()){if(g_flights.size()>=256)return;Flight row{};row.actor.Set(actor);row.owner=call->owner;row.expires=Now()+60000;g_flights[actor]=row;}
        else {if(g_local.size()>=128 || std::any_of(g_local.begin(),g_local.end(),[&](const auto& row){return row.second.actor.Is(actor);}))return;
            Local row{};row.actor.Set(actor);row.invocation=call->invocation;row.expires=Now()+3000;
            const auto& owner=call->owner;auto& p=row.packet;p.source=owner.source;p.generation=owner.generation;p.playerNo=owner.playerNo;p.slot=owner.slot;p.kind=owner.kind;
            p.sequence=Next(g_sequence);const auto sequence=p.sequence;g_local[sequence]=row;
            const auto valid=[&]{const auto live=g_local.find(sequence);return Current(epoch) && !Host() && row.actor.Get()==actor && live!=g_local.end() && live->second.actor.Is(actor);};
            if(!N::FreezeLaunch(actor,valid) && valid()){g_local.erase(sequence);ue_wrap::engine::DestroyActor(actor);}}
        return;
    }
    if(!Host())return;
    auto flight=g_flights.find(source);if(flight==g_flights.end() || !flight->second.actor.Is(source) || g_candidates.size()>=256)return;
    uint8_t kind=0;if(!N::DecalKind(actor,kind) || kind!=flight->second.owner.kind || !Current(epoch))return;
    const auto assigned=g_actorIds.find(actor);if(assigned!=g_actorIds.end() && assigned->second.actor.Is(actor))return;
    for(auto& pending:g_candidates)if(pending.actor.Is(actor)){
        if(pending.baseline){pending.owner=flight->second.owner;pending.baseline=false;pending.expires=Now()+1500;}return;}
    Candidate row{};row.actor.Set(actor);row.owner=flight->second.owner;row.expires=Now()+1500;g_candidates.push_back(row);g_unassigned[actor]=row.actor;
}
void Capture(){
    const auto epoch=g_epoch;const auto count=g_candidates.size();unsigned work=8;
    for(size_t i=0;i<count && !g_candidates.empty() && work;++i){auto row=g_candidates.front();g_candidates.pop_front();--work;
        void* actor=row.actor.Get();
        // A native host scorch is a lasting world actor, not an expiring prediction.
        if(actor && Host() && row.owner.kind==N::kExplosionScorch)row.expires=0;
        if(!actor || (row.expires && Now()>row.expires)){
            EraseTemporary(g_unassigned,row.actor);
            if(actor && !row.baseline)ue_wrap::engine::DestroyActor(actor);continue;}
        const auto valid=[&]{return Current(epoch) && row.actor.Get()==actor;};N::State state{};
        const bool read=N::Read(actor,state,valid);if(!Current(epoch))return;
        if(!read || !valid()){if(valid())g_candidates.push_back(row);continue;}
        auto p=row.owner;Encode(state,p);
        if(row.baseline && !Host())g_existing.push_back({row.actor,state});
        else if(Host()){if(row.baseline)p.op=2;Register(actor,p);}
    }
}
CachedObjRef ClaimExisting(const N::State& target){
    for(auto it=g_existing.begin();it!=g_existing.end();){
        const auto assigned=g_actorIds.find(it->actor.Raw());
        if(!it->actor.Get() || (assigned!=g_actorIds.end() && assigned->second.actor.Is(it->actor.Raw()))){it=g_existing.erase(it);continue;}
        const auto& state=it->state;const float x=state.location.X-target.location.X,y=state.location.Y-target.location.Y,z=state.location.Z-target.location.Z;
        if(state.kind==target.kind && x*x+y*y+z*z<1 && std::fabs(state.scale.X-target.scale.X)<.02f &&
           std::fabs(state.scale.Y-target.scale.Y)<.02f && std::fabs(state.scale.Z-target.scale.Z)<.02f &&
           std::fabs(ue_wrap::NormalizeAxis(state.rotation.Pitch-target.rotation.Pitch))<.1f &&
           std::fabs(ue_wrap::NormalizeAxis(state.rotation.Yaw-target.rotation.Yaw))<.1f &&
           std::fabs(ue_wrap::NormalizeAxis(state.rotation.Roll-target.rotation.Roll))<.1f){const auto result=it->actor;g_existing.erase(it);return result;}
        ++it;
    }
    return {};
}
// A request describes launch,
// never impact: the host runs native tracing, decal spawning and shortCircuit.
N::Launch DecodeLaunch(const LaunchPacket& p){return {{p.x,p.y,p.z},{p.vx,p.vy,p.vz},{p.sx,p.sy,p.sz},{p.pitch,p.yaw,p.roll},p.size,p.kind};}
void EncodeLaunch(const N::Launch& s,LaunchPacket& p){p.x=s.location.X;p.y=s.location.Y;p.z=s.location.Z;
    p.vx=s.velocity.X;p.vy=s.velocity.Y;p.vz=s.velocity.Z;p.sx=s.scale.X;p.sy=s.scale.Y;p.sz=s.scale.Z;
    p.pitch=s.rotation.Pitch;p.yaw=s.rotation.Yaw;p.roll=s.rotation.Roll;p.size=s.size;p.kind=s.kind;}
bool ValidLaunch(const LaunchPacket& p){
    if(p.kind>3 || p.op>3 || p.reserved[0] || p.reserved[1] || p.reserved[2] || !Key(p.source) || !N::Valid(DecodeLaunch(p)))return false;
    if(p.kind==3)return !p.source.len && std::fabs(p.sx-1.f)<=.001f && std::fabs(p.sy-1.f)<=.001f && std::fabs(p.sz-1.f)<=.001f && std::fabs(p.size-5.f)<=.001f;
    if(p.kind==2)return !p.source.len && p.sx>=.5f && p.sx<=1.f && std::fabs(p.sx-p.sy)<=.001f && std::fabs(p.sx-p.sz)<=.001f && std::fabs(p.size-5.f)<=.001f;
    return std::fabs(p.sx-.25f)<=.001f && std::fabs(p.sy-.25f)<=.001f && std::fabs(p.sz-.25f)<=.001f && std::fabs(p.size-1.f)<=.001f;
}
bool SameLaunch(LaunchPacket a,LaunchPacket b){a.op=b.op=0;return std::memcmp(&a,&b,sizeof(a))==0;}
void DrainRequests(){
    const auto epoch=g_epoch;auto* session=g_session;unsigned work=8;const auto pending=g_requests;
    auto it=pending.upper_bound(g_requestCursor);if(it==pending.end())it=pending.begin();
    for(size_t visited=0;visited<pending.size() && work;++visited){
        const auto key=it->first;const auto sampled=it->second;if(++it==pending.end())it=pending.begin();
        auto live=g_requests.find(key);if(live==g_requests.end() || live->second.processing)continue;const auto p=sampled.packet;
        const auto person=[&]{return Current(epoch) && g_session==session && Host() && g_peerEpoch[p.slot]==sampled.peerEpoch &&
            roster_ledger::Get(p.slot).playerNo==p.playerNo && session->IsSlotWorldReady(p.slot);};
        if(!person()){if(!Current(epoch))return;g_requests.erase(key);continue;}
        if(!live->second.done && (Now()>live->second.expires || g_flights.size()>=256 || g_records.size()>=4096)){
            live->second.done=live->second.owed=true;live->second.packet.op=2;live->second.expires=Now()+3000;}
        if(!live->second.done){
            if(!g_hook || !g_finishFunction.Get())continue;
            --work;g_requestCursor=key;void* source=HostSource(p);CachedObjRef ref;ref.Set(source);std::wstring retiredClass;
            const bool retired=!p.source.len && !source && hand_item::RecentGenerationClass(p.slot,p.generation,retiredClass);
            const auto valid=[&]{auto row=g_requests.find(key);if(!person() || !g_hook || !g_finishFunction.Get() || row==g_requests.end() || row->second.peerEpoch!=sampled.peerEpoch)return false;
                if(source)return ref.Get()==source && HostSource(p)==source && person();
                std::wstring cls;return retired && hand_item::RecentGenerationClass(p.slot,p.generation,cls) && cls==retiredClass && person();};
            uint8_t kind=0;
            const bool known=source?(p.kind==3?N::BreakSourceKind(source,kind):N::SourceKind(source,kind)):(retired && N::SourceClassKind(retiredClass,kind));
            if(!known || !valid() || kind!=p.kind)continue;
            const auto target=element::IntentTarget::ForClientIntent(*session,p.slot,600.f);
            const bool reach=!source || bool(target.Authorize(source));if(!valid())continue;
            const bool point=reach && bool(target.AuthorizeSegment({p.x,p.y,p.z},{p.x,p.y,p.z}));if(!valid())continue;
            if(!point)continue;
            if(source){ue_wrap::FVector at{};if(!ue_wrap::engine::TryGetActorLocation(source,at) || !valid())continue;
                const float x=p.x-at.X,y=p.y-at.Y,z=p.z-at.Z;const float distance=x*x+y*y+z*z;if(!std::isfinite(distance) || distance>600.f*600.f)continue;}
            live=g_requests.find(key);if(live==g_requests.end())continue;live->second.processing=true;
            const bool previous=g_authoring;g_authoring=true;struct Guard{bool old;~Guard(){g_authoring=old;}}guard{previous};
            const auto result=N::AuthorLaunch(DecodeLaunch(p),valid);if(!Current(epoch))return;
            if(!person()){
                live=g_requests.find(key);if(live!=g_requests.end() && live->second.peerEpoch==sampled.peerEpoch && SameLaunch(live->second.packet,p))g_requests.erase(live);
                continue;
            }
            live=g_requests.find(key);if(live==g_requests.end())continue;live->second.processing=false;
            if(!result.attempted)continue;
            live->second.attempted=true;live->second.output.Set(result.actor);
            const bool accepted=valid();live->second.done=live->second.owed=true;live->second.expires=Now()+3000;live->second.packet.op=accepted && result.actor?1:3;
            if(!accepted){if(void* actor=live->second.output.Get())ue_wrap::engine::DestroyActor(actor);continue;}
            if(result.actor){Flight flight{};flight.actor.Set(result.actor);flight.expires=Now()+60000;
                auto& owner=flight.owner;owner.source=p.source;owner.sequence=p.sequence;owner.generation=p.generation;owner.slot=p.slot;owner.playerNo=p.playerNo;owner.kind=p.kind;
                g_flights[result.actor]=flight;}
        }
        live=g_requests.find(key);if(live==g_requests.end())continue;
        if(live->second.owed && work){--work;g_requestCursor=key;const auto reply=live->second.packet;
            const bool sent=session->SendReliableToSlot(p.slot,net::ReliableKind::DrinkLaunch,&reply,sizeof(reply));if(!Current(epoch))return;
            live=g_requests.find(key);if(sent && person() && live!=g_requests.end() && SameLaunch(live->second.packet,reply))live->second.owed=false;}
        live=g_requests.find(key);if(live!=g_requests.end() && live->second.done && Now()>live->second.expires)g_requests.erase(live);
    }
}
void RetireLocal(uint32_t sequence){auto it=g_local.find(sequence);if(it==g_local.end())return;const auto actor=it->second.actor;g_local.erase(it);
    if(void* live=actor.Get())ue_wrap::engine::DestroyActor(live);}
void DrainLocal(){const auto epoch=g_epoch;const auto pending=g_local;unsigned work=8;
    auto it=pending.upper_bound(g_localCursor);if(it==pending.end())it=pending.begin();
    for(size_t visited=0;visited<pending.size() && work;++visited){const auto sequence=it->first;const auto row=it->second;if(++it==pending.end())it=pending.begin();
        if(!Current(epoch))return;
        if(Now()>row.expires){--work;RetireLocal(sequence);continue;}
        if(SG::IsInvocationActive(row.invocation) || Now()<row.next)continue;--work;g_localCursor=sequence;
        const auto valid=[&]{auto live=g_local.find(sequence);return Current(epoch) && !Host() && live!=g_local.end() &&
            live->second.packet.playerNo==roster_ledger::Get(LocalSlot()).playerNo && (!row.actor.Raw() || row.actor.Get());};
        auto packet=row.packet;if(!row.captured){N::Launch state{};void* actor=row.actor.Get();if(!actor){RetireLocal(sequence);continue;}
            if(!N::ReadLaunch(actor,state,valid) || !valid())continue;EncodeLaunch(state,packet);
            if(!ValidLaunch(packet) || packet.kind!=row.packet.kind){RetireLocal(sequence);continue;}
            auto live=g_local.find(sequence);if(live==g_local.end())continue;live->second.packet=packet;live->second.captured=true;live->second.actor.Reset();
            ue_wrap::engine::DestroyActor(actor);if(!Current(epoch))return;}
        auto live=g_local.find(sequence);if(live==g_local.end())continue;live->second.next=Now()+150;
        g_session->SendReliableToSlot(0,net::ReliableKind::DrinkLaunch,&packet,sizeof(packet));if(!Current(epoch))return;
    }
}
void ReceiveFlight(const LaunchPacket& p,uint8_t sender){
    World();if(!Current() || !ValidLaunch(p) || p.slot>=net::kMaxPeers || !Reconcile())return;
    if(!Host()){if(sender || !p.op || p.slot!=LocalSlot() || p.playerNo!=roster_ledger::Get(p.slot).playerNo)return;
        const auto local=g_local.find(p.sequence);if(local!=g_local.end() && local->second.captured && SameLaunch(local->second.packet,p))RetireLocal(p.sequence);return;}
    if(!sender || sender>=net::kMaxPeers || sender!=p.slot || p.op || !p.sequence || !p.playerNo || !p.generation ||
       p.playerNo!=roster_ledger::Get(sender).playerNo || !g_session->IsSlotWorldReady(sender))return;
    const auto key=std::make_pair(sender,p.sequence);auto old=g_requests.find(key);
    if(old!=g_requests.end()){if(old->second.done && SameLaunch(old->second.packet,p))old->second.owed=true;return;}
    if(g_requests.size()>=256 || std::count_if(g_requests.begin(),g_requests.end(),[sender](const auto& row){return row.first.first==sender;})>=64)return;
    auto& high=g_high[sender];auto& seen=g_seen[sender];if(high && int32_t(p.sequence-high)<=0){const auto age=high-p.sequence;if(age>=256 || seen[age])return;}
    const auto now=Now();if(now-g_rateAt[sender]>=1000){g_rateAt[sender]=now;g_rate[sender]=0;}if(g_rate[sender]>=32)return;++g_rate[sender];
    if(!high){high=p.sequence;seen.reset();}else if(int32_t(p.sequence-high)>0){const auto shift=p.sequence-high;if(shift>=256)seen.reset();else seen<<=shift;high=p.sequence;}seen.set(high-p.sequence);
    g_requests[key]={p,now+1500,g_peerEpoch[sender]};
}

void DrainRecords(){
    const auto epoch=g_epoch;const auto pending=g_pendingRecords;unsigned work=16;auto it=pending.upper_bound(g_recordCursor);if(it==pending.end())it=pending.begin();
    for(size_t visited=0;visited<pending.size() && work;++visited){const auto id=*it;if(++it==pending.end())it=pending.begin();
        if(!Current(epoch))return;--work;g_recordCursor=id;auto live=g_records.find(id);if(live==g_records.end()){g_pendingRecords.erase(id);continue;}
        const auto sampled=live->second;
        const auto revision=live->second.revision;
        const auto current=[&]{const auto now=g_records.find(id);return Current(epoch) && now!=g_records.end() && now->second.revision==revision;};
        if(Host()){
            if(void* actor=sampled.actor.Get()){float process=0;const bool read=ue_wrap::grime::ReadProcess(actor,process);if(!Current(epoch))return;
                if(read && current() && sampled.actor.Get() && process<live->second.packet.process)live->second.packet.process=process;}
            auto p=live->second.packet;if(live->second.retired)p.op=3;
            for(int slot=1;slot<net::kMaxPeers;++slot)if(sampled.pending&(1u<<slot)){
                if(!current())break;const bool recipient=g_session->IsSlotWorldReady(slot) && g_peerEpoch[slot]==sampled.epochs[slot] && roster_ledger::Get(slot).playerNo==sampled.people[slot];
                if(!recipient){g_records[id].pending&=~(1u<<slot);continue;}
                auto packet=p;if(packet.op!=3)packet.op=(sampled.joins&(1u<<slot))?2:1;
                const bool sent=g_session->SendReliableToSlot(slot,net::ReliableKind::DrinkSpill,&packet,sizeof(packet));if(!Current(epoch))return;
                if(sent && current() && g_peerEpoch[slot]==sampled.epochs[slot] && roster_ledger::Get(slot).playerNo==sampled.people[slot])g_records[id].pending&=~(1u<<slot);
            }
        }else if(!sampled.retired && !sampled.failed && !sampled.actor.Get()){
            if(!g_baselineComplete || std::any_of(g_candidates.begin(),g_candidates.end(),[](const Candidate& row){return row.baseline;}))continue;
            const bool previous=g_authoring;g_authoring=true;struct Guard{bool old;~Guard(){g_authoring=old;}}guard{previous};
            const auto state=Decode(sampled.packet);auto adopted=sampled.adopted;if(!adopted.Get() && sampled.mayAdopt)adopted=ClaimExisting(state);
            const auto birthCurrent=[&]{const auto row=g_records.find(id);return Current(epoch) && row!=g_records.end() && !row->second.retired && row->second.packet.nonce==sampled.packet.nonce;};
            N::Spawn result{};
            if(void* actor=adopted.Get()){
                g_records[id].adopted=adopted;g_actorIds[actor]={adopted,id,R::SlotSerial(adopted.Idx())};grime_sync::RegisterDynamic(actor);
                if(current() && N::ApplyExisting(actor,state,current) && current())result.actor=actor;
            }else result=N::Author(state,true,birthCurrent);
            if(!Current(epoch))return;
            if(result.actor && birthCurrent()){auto& row=g_records[id];row.actor.Set(result.actor);row.spawned=!adopted.Get();row.paint=true;g_unassigned.erase(result.actor);g_actorIds[result.actor]={row.actor,id,R::SlotSerial(row.actor.Idx())};
                grime_sync::RegisterDynamic(result.actor);if(!Current(epoch))return;
}
            else if(result.attempted && birthCurrent())g_records[id].failed=true;
        }
        if(!current())continue;live=g_records.find(id);if(live==g_records.end())continue;
        if(!Host() && live->second.paint && !live->second.retired){
            const auto ref=live->second.actor;void* actor=ref.Get();float process=0;
            const auto valid=[&]{return current() && ref.Get()==actor && actor;};
            if(actor && ue_wrap::grime::ReadProcess(actor,process) && valid() &&
               ue_wrap::grime::WriteProcessAndApply(actor,std::min(process,live->second.packet.process),valid) && valid())g_records[id].paint=false;
            if(!Current(epoch))return;if(!current())continue;
        }
        live=g_records.find(id);if(live==g_records.end())continue;
        if(live->second.sound && Now()<=sampled.soundUntil){
            const bool played=N::ImpactSound({sampled.packet.x,sampled.packet.y,sampled.packet.z},current);if(!Current(epoch))return;
            if(played && current())g_records[id].sound=false;
        }else if(Now()>sampled.soundUntil)live->second.sound=false;
        live=g_records.find(id);if(live!=g_records.end() && current()){
            const auto& row=live->second;
            if(!row.sound && (Host()?!row.pending:(row.retired || row.failed || (row.actor.Get() && !row.paint))))g_pendingRecords.erase(id);
        }
    }
}
}
void Install(net::Session* session){
    if(g_session!=session || g_world!=ue_wrap::world_identity::Generation() || (session && session->role()!=g_role)){OnDisconnect();g_session=session;g_world=ue_wrap::world_identity::Generation();if(session)g_role=session->role();
        g_nonce=Nonce();}
    if(!g_hook || !g_finishFunction.Get()){g_hook=false;void* cls=ue_wrap::object_index::ClassByName(L"GameplayStatics");if(cls){
        const auto fn=ue_wrap::facts::ResolveOn(kFinish,cls);g_hook=ue_wrap::ufunction_hook::InstallPostHook(fn,&ObserveFinish);if(g_hook)g_finishFunction.Set(fn.Fn());}}
    if(!g_pourHook || !g_pourFunction.Get()){g_pourHook=false;void* cls=ue_wrap::object_index::ClassByName(L"GameplayStatics");if(cls){
        const auto fn=ue_wrap::facts::ResolveOn(kPourSound,cls);g_pourHook=ue_wrap::ufunction_hook::InstallPostHook(fn,&ObservePourSound);if(g_pourHook)g_pourFunction.Set(fn.Fn());}}
    if(!g_watch)g_watch=SG::WatchClassName(L"prop_snack_beer_C",L"ReceiveTick",kTag,&BeforeSource,nullptr);
    if(!g_coffeeWatch)g_coffeeWatch=SG::WatchClassName(L"prop_food_coffeeCup_C",L"ReceiveTick",kTag,&BeforeSource,nullptr);
    if(!g_breakWatch)g_breakWatch=SG::WatchClassName(L"prop_snack_beer_C",L"broken",kTag+1,&ObserveHostBreak,nullptr);
    if(!g_fireWatch)g_fireWatch=SG::WatchClassName(L"prop_snack_beer_C",L"broken_fire",kTag+2,&ObserveHostBreak,nullptr);
    if(!g_gasBreakWatch)g_gasBreakWatch=SG::WatchClassName(L"prop_gasBottle_C",L"broken",kTag+5,&BeforeGasBreak,nullptr);
    if(!g_gasFireWatch)g_gasFireWatch=SG::WatchClassName(L"prop_gasBottle_C",L"broken_fire",kTag+6,&BeforeGasBreak,nullptr);
    if(!g_scorchWatch)g_scorchWatch=SG::WatchClassName(L"explosion_C",L"scorchMark",kTag+7,&BeforeScorch,nullptr);
    if(!g_flightWatch)g_flightWatch=SG::WatchClassName(L"grimeProjectile_C",L"ReceiveTick",kTag+3,&BeforeFlight,nullptr);
    if(!g_graphWatch)g_graphWatch=SG::WatchClassName(L"grimeProjectile_C",L"ExecuteUbergraph_grimeProjectile",kTag+4,&BeforeFlight,nullptr);
}
void Tick(){World();if(!Current() || g_ticking)return;struct Guard{Guard(){g_ticking=true;}~Guard(){g_ticking=false;}}guard;
    if(!Reconcile())return;const auto epoch=g_epoch;
    std::erase_if(g_flights,[](const auto& item){return !item.second.actor.Get() || Now()>item.second.expires;});
    std::erase_if(g_calls,[](const Call& row){return !row.source.Get() || !SG::IsInvocationActive(row.invocation);});
    PruneScorchCalls();
    Capture();if(!Current(epoch))return;
    if(Host()){DrainRequests();if(!Current(epoch))return;DrainPourRequests();}
    else {DrainPourLocal();if(!Current(epoch))return;DrainLocal();}
    if(!Current(epoch))return;DrainPourCues();if(!Current(epoch))return;DrainRecords();
}
void Receive(const Packet& p,uint8_t sender){
    World();if(!Current() || Host() || sender || !Valid(p) || p.slot>=net::kMaxPeers || !Reconcile())return;
    if(!Host()){
        if(sender || !p.op)return;
        if(!p.id || !p.nonce || p.slot>=net::kMaxPeers)return;
        auto it=g_records.find(p.id);if(it!=g_records.end() && it->second.packet.nonce!=p.nonce)return;
        if(it!=g_records.end() && p.op!=3 && !SameShape(it->second.packet,p))return;
        if(it==g_records.end() && g_records.size()>=4096)return;
        auto& row=g_records[p.id];if(row.retired)return;
        const bool fresh=!row.packet.id;const float previous=row.packet.process;row.packet=p;if(!fresh)row.packet.process=std::min(previous,p.process);row.paint=true;row.revision=++g_revision;
        if(p.op==3){row.retired=true;row.sound=false;g_pendingRecords.erase(p.id);auto actor=row.actor.Get()?row.actor:row.adopted;row.actor.Reset();row.adopted.Reset();
            const auto bound=g_actorIds.find(actor.Raw());if(bound!=g_actorIds.end() && bound->second.id==p.id)g_actorIds.erase(bound);
            EraseTemporary(g_unassigned,actor);if(void* live=actor.Get())ue_wrap::engine::DestroyActor(live);return;}
        if(fresh){row.mayAdopt=p.op==2;row.sound=p.op==1 && p.kind!=N::kExplosionScorch;row.soundUntil=Now()+1500;}
        g_pendingRecords.insert(p.id);
        return;
    }
}
void ReceiveLaunch(const LaunchPacket& p,uint8_t sender){ReceiveFlight(p,sender);}
void ReceivePour(const PourPacket& p,uint8_t sender){ReceivePourPacket(p,sender);}
void SendAllToSlot(int slot){World();if(!Current() || !Host() || slot<=0 || slot>=net::kMaxPeers)return;
    for(auto& [id,row]:g_records)QueueRecord(row,slot,true);}
std::wstring KeyFor(void* actor){if(!Current())return {};const auto it=g_actorIds.find(actor);if(it==g_actorIds.end() || !it->second.actor.Is(actor))return {};
    const auto row=g_records.find(it->second.id);return row==g_records.end() || row->second.retired?std::wstring():DynamicKey(row->second.packet.nonce,it->second.id);}
void* FindByKey(const std::wstring& key){
    if(!Current() || key.size()!=28 || key.compare(0,3,L"ds_") || key[19]!=L'_')return nullptr;
    uint64_t nonce=0;uint32_t id=0;
    for(size_t i=3;i<key.size();++i){if(i==19)continue;const auto c=key[i];
        const int value=c>=L'0' && c<=L'9'?int(c-L'0'):c>=L'a' && c<=L'f'?int(c-L'a'+10):-1;
        if(value<0)return nullptr;if(i<19)nonce=(nonce<<4)|uint64_t(value);else id=(id<<4)|uint32_t(value);
    }
    const auto row=g_records.find(id);return row==g_records.end() || row->second.retired || row->second.packet.nonce!=nonce?nullptr:row->second.actor.Get();
}
bool ExcludeFromStaticIndex(void* actor){const auto it=g_unassigned.find(actor);
    return Current() && (PrivateScorch(actor) || (it!=g_unassigned.end() && it->second.Is(actor)));}
void ObserveExisting(void* actor){
    if(!Current() || g_authoring || !actor || g_candidates.size()>=4096)return;
    const auto existing=g_actorIds.find(actor);if(existing!=g_actorIds.end() && existing->second.actor.Is(actor))return;
    const auto seen=g_baselineSeen.find(actor);if(seen!=g_baselineSeen.end() && seen->second.Is(actor))return;
    if(ExcludeFromStaticIndex(actor))return;const auto epoch=g_epoch;uint8_t kind=0;
    if(!N::DecalKind(actor,kind) || !Current(epoch))return;
    if(kind==N::kExplosionScorch){
        ObserveScorchOrigin(actor);
        if(PrivateScorch(actor) || (Host() && !SharedScorch(actor)))return;
    }
    Candidate row{};row.actor.Set(actor);row.baseline=true;row.owner.kind=kind;g_candidates.push_back(row);g_baselineSeen[actor]=row.actor;
    if(kind!=N::kExplosionScorch || Host())g_unassigned[actor]=row.actor;
}
void IndexComplete(uint32_t world){if(Current() && world==g_world)g_baselineComplete=true;}
void OnRemoved(void* actor){if(!Current() || !Host())return;const auto it=g_actorIds.find(actor);if(it==g_actorIds.end())return;
    auto row=g_records.find(it->second.id);if(row==g_records.end() || R::ObjectAt(it->second.actor.Idx())!=actor || R::SlotSerial(it->second.actor.Idx())!=it->second.serial)return;row->second.retired=true;row->second.packet.process=0;
    for(int slot=1;slot<net::kMaxPeers;++slot)QueueRecord(row->second,slot,false);}
void OnPeerLeft(int slot){if(slot<0 || slot>=net::kMaxPeers)return;++g_peerEpoch[slot];g_people[slot]=0;g_high[slot]=0;g_seen[slot].reset();g_rate[slot]=0;g_rateAt[slot]=0;
    g_pourHigh[slot]=0;g_pourSeen[slot].reset();g_pourRate[slot]=0;g_pourRateAt[slot]=0;
    std::erase_if(g_pourRequests,[slot](const auto& row){return row.first.first==slot;});
    std::erase_if(g_pourLocal,[slot](const auto& row){return row.second.packet.slot==slot;});
    for(auto& [id,cue]:g_pours)cue.pending&=~(1u<<slot);
    std::vector<CachedObjRef> cleanup;for(const auto& [key,row]:g_requests)if(key.first==slot && !row.done)cleanup.push_back(row.output);
    std::erase_if(g_requests,[slot](const auto& item){return item.first.first==slot;});for(auto& [id,row]:g_records)row.pending&=~(1u<<slot);
    const auto epoch=g_epoch;const auto world=g_world;for(const auto& ref:cleanup){if(epoch!=g_epoch || world!=ue_wrap::world_identity::Generation())break;if(void* actor=ref.Get())ue_wrap::engine::DestroyActor(actor);}}
void OnDisconnect(){
    const auto world=g_world;const bool mirrors=g_session && g_role==net::Role::Client;std::vector<CachedObjRef> cleanup;
    if(mirrors){for(const auto& [id,row]:g_records)if(row.spawned)cleanup.push_back(row.actor);for(const auto& [sequence,row]:g_local)cleanup.push_back(row.actor);}
    ++g_epoch;g_session=nullptr;g_world=0;g_nonce=0;g_sequence=g_id=g_recordCursor=g_localCursor=0;g_requestCursor={};
    g_calls.clear();g_scorchCalls.clear();g_flights.clear();g_candidates.clear();g_local.clear();g_unassigned.clear();g_baselineSeen.clear();g_existing.clear();g_baselineComplete=false;
    g_pourLocal.clear();g_pourRequests.clear();g_pours.clear();g_pourCanonicalSeen.clear();g_pourHigh={};g_pourSeen={};g_pourRateAt={};g_pourRate={};
    g_pourId=g_pourSequence=g_pourLocalCursor=0;g_pourRequestCursor={};
    g_requests.clear();g_records.clear();g_pendingRecords.clear();g_actorIds.clear();g_people={};g_high={};g_seen={};g_rate={};g_rateAt={};
    grime_sync::ResetDynamic();
    const auto epoch=g_epoch;for(const auto& ref:cleanup){if(g_epoch!=epoch || world!=ue_wrap::world_identity::Generation())break;if(void* actor=ref.Get())ue_wrap::engine::DestroyActor(actor);}
}
}
