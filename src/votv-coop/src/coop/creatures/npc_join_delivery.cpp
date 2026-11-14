#include "coop/creatures/npc_join_delivery.h"
#include "coop/creatures/npc_sync.h"
#include "coop/creatures/geom_octa_sync.h"
#include "coop/creatures/kerfur_entity.h"
#include "coop/element/registry.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/net/net_clock.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/actors/kerfur.h"
#include <array>
#include <memory>
#include <string>
#include <cmath>
#include <cstring>
#include <utility>

namespace coop::npc_join_delivery {
namespace {
namespace N=coop::net;
namespace E=coop::element;
namespace R=ue_wrap::reflection;
struct Context {
    N::Session* session=nullptr;
    uint32_t serial=0,world=0,generation=0;
    uint64_t stops=0,ready=0,epoch=0;
    void* worldObject=nullptr;
    int slot=0;
    bool Current(bool requireReady=false)const;
};
struct Row {
    E::Registry::IdentitySnapshot identity;
    ue_wrap::CachedObjRef actor,cls;
    bool done=false,attempted=false;
    uint64_t at=0;
};
struct Join {Context context;std::vector<Row> rows;size_t cursor=0,remaining=0;bool building=true;};
struct Snapshot {Context context;uint64_t token=0;};
struct Completion {Context context;uint64_t token=0;std::vector<uint8_t> payload;CompletionFn sent,invalid;std::function<bool()> valid;bool attempted=false;uint64_t at=0;};
std::array<std::shared_ptr<Join>,N::kMaxPeers> g_joins;
std::array<std::shared_ptr<Completion>,N::kMaxPeers> g_completions;
std::array<std::shared_ptr<Snapshot>,N::kMaxPeers> g_snapshots;
uint64_t g_nextSnapshot=1;
N::Session* g_session=nullptr;
uint32_t g_serial=0,g_world=0;
uint64_t g_stops=0,g_epoch=1;
void* g_worldObject=nullptr;
bool g_draining=false;
int g_cursor=1;
int g_completionCursor=1;
constexpr size_t kNativeBudget=16,kScanBudget=64;
constexpr uint64_t kRetryMs=250;
bool ValidSlot(int slot){return slot>0&&slot<N::kMaxPeers;}
void Clear(){++g_epoch;g_joins.fill({});g_completions.fill({});g_snapshots.fill({});g_session=nullptr;g_cursor=g_completionCursor=1;}
void Bind(){
    auto* session=coop::npc_sync::GetSession();
    const auto serial=N::session_serial::Current(),world=ue_wrap::world_identity::Generation();
    void* worldObject=ue_wrap::world_identity::CurrentWorld();
    if(session!=g_session||serial!=g_serial||world!=g_world||worldObject!=g_worldObject||
        (session&&(session->StopCount()!=g_stops||session->role()!=N::Role::Host||!session->running())))Clear();
    g_session=session;g_serial=serial;g_world=world;g_worldObject=worldObject;g_stops=session?session->StopCount():0;
}
bool Context::Current(bool requireReady)const{
    return epoch==g_epoch&&session&&session==g_session&&session==coop::npc_sync::GetSession()&&
        serial==N::session_serial::Current()&&world==ue_wrap::world_identity::Generation()&&
        worldObject&&worldObject==ue_wrap::world_identity::CurrentWorld()&&
        session->StopCount()==stops&&session->running()&&session->connected()&&session->role()==N::Role::Host&&
        generation&&session->peerGenerationForSlot(slot)==generation&&session->IsSlotConnected(slot)&&
        session->SlotWorldReadyRevision(slot)==ready&&(!requireReady||session->IsSlotWorldReady(slot));
}
Context Capture(int slot){
    Context result;result.session=g_session;result.serial=g_serial;result.world=g_world;
    result.worldObject=g_worldObject;result.stops=g_stops;result.epoch=g_epoch;result.slot=slot;
    if(g_session){result.generation=g_session->peerGenerationForSlot(slot);result.ready=g_session->SlotWorldReadyRevision(slot);}
    return result;
}
bool Installed(const std::shared_ptr<Join>& join){return g_joins[join->context.slot]==join&&join->context.Current();}
bool Identity(const Row& row,E::Registry::IdentitySnapshot& now){
    return E::Registry::Get().ReadIdentity(row.identity.id,now)&&now.incarnation==row.identity.incarnation&&
        now.type==E::ElementType::Npc&&!now.mirror&&!now.deleting&&now.typeName==row.identity.typeName;
}
bool ActorBinding(const E::Registry::IdentitySnapshot& now){return E::Registry::Get().EidForActor(now.actor)==now.id;}
bool Owned(const std::shared_ptr<Join>& join,const Row& row){
    if(!Installed(join)||!row.actor.Get()||!row.cls.Get()||R::ClassOf(row.actor.Raw())!=row.cls.Raw())return false;
    E::Registry::IdentitySnapshot now;
    return Identity(row,now)&&now.actor==row.actor.Raw()&&R::IsLiveByIndex(now.actor,now.internalIdx)&&ActorBinding(now);
}
void Finish(const std::shared_ptr<Join>& join,Row& row){if(Installed(join)&&!row.done){row.done=true;--join->remaining;}}
bool Paced(bool attempted,uint64_t at,uint64_t now){return attempted&&now>=at&&now-at<kRetryMs;}
bool NameValid(const std::string& name){
    if(name.empty()||name.size()>63)return false;
    for(unsigned char ch:name)if(ch<33||ch>126)return false;
    return true;
}
void Attempt(const std::shared_ptr<Join>& join,Row& row){
    if(!Installed(join)||!join->context.Current(true))return;
    E::Registry::IdentitySnapshot now;
    if(!Identity(row,now)){Finish(join,row);return;}
    if(!row.actor.Raw()){
        if(!now.actor)return;
        if(!R::IsLiveByIndex(now.actor,now.internalIdx)){Finish(join,row);return;}
        if(!ActorBinding(now)){
            if(E::Registry::Get().EidForActor(now.actor)!=E::kInvalidId)Finish(join,row);
            return;
        }
        row.actor.Set(now.actor);row.cls.Set(R::ClassOf(now.actor));
    }
    if(!Owned(join,row)||!NameValid(row.identity.typeName)){Finish(join,row);return;}
    const auto current=[&]{return Owned(join,row)&&join->context.Current(true)&&!row.done;};
    ue_wrap::FVector location{},scale{};ue_wrap::FRotator rotation{};
    const bool pose=ue_wrap::engine::TryGetActorTransform(row.actor.Raw(),location,rotation,scale,current);
    if(!Owned(join,row)){Finish(join,row);return;}
    if(!pose||!current())return;
    const float values[]={location.X,location.Y,location.Z,rotation.Pitch,rotation.Yaw,rotation.Roll,scale.X,scale.Y,scale.Z};
    for(float value:values)if(!std::isfinite(value))return;
    if(std::fabs(location.X)>N::kMaxCoord||std::fabs(location.Y)>N::kMaxCoord||std::fabs(location.Z)>N::kMaxCoord)return;
    bool saved=false;
    const bool readable=ue_wrap::kerfur::TryHasSaveKey(row.actor.Raw(),saved,current);
    if(!Owned(join,row)){Finish(join,row);return;}
    if(!readable||!current())return;
    const auto off=coop::kerfur_entity::GetOriginOffEidForEid(row.identity.id);
    if(!current())return;
    N::EntitySpawnPayload packet{};packet.elementId=row.identity.id;
    packet.className.len=static_cast<uint8_t>(row.identity.typeName.size());
    std::memcpy(packet.className.data,row.identity.typeName.data(),row.identity.typeName.size());
    packet.locX=location.X;packet.locY=location.Y;packet.locZ=location.Z;
    packet.rotPitch=rotation.Pitch;packet.rotYaw=rotation.Yaw;packet.rotRoll=rotation.Roll;
    packet.scaleX=scale.X;packet.scaleY=scale.Y;packet.scaleZ=scale.Z;
    packet.savePersisted=saved?1:0;packet.retireOffEid=off==E::kInvalidId?0:off;
    const bool accepted=join->context.session->SendReliableToSlot(join->context.slot,N::ReliableKind::EntitySpawn,&packet,sizeof packet);
    if(!Owned(join,row)||!join->context.Current(true))return;
    if(!accepted)return;
    Finish(join,row);
    coop::geom_octa_sync::OnNpcSpawnSent(packet.elementId,row.actor.Raw(),row.identity.typeName.data(),row.identity.typeName.size(),true,join->context.slot);
}
void DrainCompletions(size_t& budget){
    for(int visited=0;visited<N::kMaxPeers-1&&budget;++visited){
        const int slot=g_completionCursor;g_completionCursor=slot+1==N::kMaxPeers?1:slot+1;
        auto completion=g_completions[slot];if(!completion)continue;
        const auto installed=[&]{return g_completions[slot]==completion&&completion->context.Current()&&SnapshotCurrent(slot,completion->token);};
        if(!installed()){if(g_completions[slot]==completion)g_completions[slot].reset();continue;}
        if(!completion->context.Current(true)||Paced(completion->attempted,completion->at,N::NowMs()))continue;
        const bool coherent=!completion->valid||completion->valid();
        if(!installed())continue;
        if(!coherent){g_completions[slot].reset();g_snapshots[slot].reset();if(completion->invalid)completion->invalid(completion->context.session,slot);continue;}
        if(HasPending(slot))continue;
        --budget;completion->attempted=true;completion->at=N::NowMs();
        const bool accepted=completion->context.session->SendReliableToSlot(slot,N::ReliableKind::SnapshotComplete,completion->payload.data(),static_cast<int>(completion->payload.size()));
        if(!installed()||!completion->context.Current(true)||!accepted)continue;
        g_completions[slot].reset();
        g_snapshots[slot].reset();
        if(completion->sent)completion->sent(completion->context.session,slot);
    }
}
}

void Queue(int slot){
    if(!ValidSlot(slot))return;
    Bind();
    auto join=std::make_shared<Join>();join->context=Capture(slot);
    if(!join->context.Current())return;
    g_completions[slot].reset();
    g_joins[slot]=join;
    std::vector<E::Registry::IdentitySnapshot> identities;
    E::Registry::Get().SnapshotIdentitiesByType(E::ElementType::Npc,identities);
    if(!Installed(join))return;
    join->rows.reserve(identities.size());
    for(auto& identity:identities){
        if(identity.id==0||!E::Registry::IsAllowedHostAllocatedEid(identity.id)||identity.mirror||identity.deleting||!identity.incarnation)continue;
        Row row;row.identity=std::move(identity);
        if(row.identity.actor){
            if(!R::IsLiveByIndex(row.identity.actor,row.identity.internalIdx))continue;
            if(ActorBinding(row.identity)){row.actor.Set(row.identity.actor);row.cls.Set(R::ClassOf(row.identity.actor));}
        }
        join->rows.push_back(std::move(row));
    }
    join->remaining=join->rows.size();join->building=false;
    if(!join->remaining&&g_joins[slot]==join)g_joins[slot].reset();
}
void Tick(){
    Bind();if(g_draining)return;
    struct Busy{Busy(){g_draining=true;}~Busy(){g_draining=false;}}busy;
    size_t budget=kNativeBudget-4,scans=kScanBudget,empty=0;
    while(budget&&scans&&empty<N::kMaxPeers-1){
        const int slot=g_cursor;g_cursor=slot+1==N::kMaxPeers?1:slot+1;
        auto join=g_joins[slot];
        if(!join){++empty;continue;}
        if(!Installed(join)){if(g_joins[slot]==join)g_joins[slot].reset();++empty;continue;}
        if(join->building||!join->context.Current(true)){++empty;continue;}
        Row* candidate=nullptr;
        for(size_t n=0;n<join->rows.size()&&scans;++n){
            --scans;auto& row=join->rows[join->cursor++%join->rows.size()];
            if(!row.done&&!Paced(row.attempted,row.at,N::NowMs())){candidate=&row;break;}
        }
        if(!candidate){++empty;continue;}
        empty=0;--budget;candidate->attempted=true;candidate->at=N::NowMs();
        Attempt(join,*candidate);
        if(Installed(join)&&!join->remaining)g_joins[slot].reset();
    }
    budget+=4;DrainCompletions(budget);
}
bool HasPending(int slot){
    if(!ValidSlot(slot))return false;
    const auto join=g_joins[slot];return join&&Installed(join)&&(join->building||join->remaining);
}
uint64_t BeginSnapshot(int slot,N::Session* expected){
    if(!ValidSlot(slot))return 0;
    Bind();auto snapshot=std::make_shared<Snapshot>();snapshot->context=Capture(slot);
    if(expected&&expected!=g_session)return 0;
    if(!snapshot->context.Current(true))return 0;
    snapshot->token=g_nextSnapshot++;g_snapshots[slot]=snapshot;g_completions[slot].reset();return snapshot->token;
}
bool SnapshotCurrent(int slot,uint64_t token){return ValidSlot(slot)&&token&&g_snapshots[slot]&&g_snapshots[slot]->token==token&&g_snapshots[slot]->context.Current();}
bool CompleteSnapshot(int slot,uint64_t token,const std::vector<uint8_t>& payload,CompletionFn sent,std::function<bool()> valid,CompletionFn invalid){
    if(!ValidSlot(slot)||payload.size()<sizeof(N::SnapshotEndPayload)||payload.size()>N::kMaxReliablePayload)return false;
    if(!SnapshotCurrent(slot,token))return false;
    auto completion=std::make_shared<Completion>();completion->context=g_snapshots[slot]->context;completion->token=token;
    completion->payload=payload;completion->sent=std::move(sent);completion->valid=std::move(valid);completion->invalid=std::move(invalid);
    g_completions[slot]=completion;return true;
}
void CancelCompletion(int slot){if(ValidSlot(slot)){g_completions[slot].reset();g_snapshots[slot].reset();}}
bool HasCompletion(int slot){return ValidSlot(slot)&&g_completions[slot]&&g_completions[slot]->context.Current();}
void OnDisconnect(){Clear();}
}
