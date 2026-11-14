// Host NPC pose publication and robot-held prop lifetime, on the game thread.
#include "coop/creatures/npc_sync.h"
#include "coop/creatures/npc_join_delivery.h"
#include "coop/creatures/npc_world_enum.h"
#include "coop/dev/kerfur_census.h"
#include "coop/element/registry.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/props/prop_drive_host.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_attach.h"
#include "ue_wrap/actors/fossilhound.h"
#include "ue_wrap/actors/kerfur.h"
#include "ue_wrap/actors/puppet.h"
#include "ue_wrap/actors/rufus.h"
#include "ue_wrap/actors/sit_pose.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

namespace coop::npc_sync {
namespace {
namespace N=coop::net;
namespace E=coop::element;
namespace R=ue_wrap::reflection;
namespace U=ue_wrap::engine;
uint64_t g_epoch=1;
bool g_ticking=false;
struct Context {
    N::Session* session=GetSession();
    uint32_t serial=N::session_serial::Current(),world=ue_wrap::world_identity::Generation();
    void* worldObject=ue_wrap::world_identity::CurrentWorld();
    uint64_t stops=session?session->StopCount():0,epoch=g_epoch;
    N::Role role=session?session->role():N::Role::Host;
    bool connected=session&&session->connected();
    bool Current()const {
        return epoch==g_epoch&&session&&session==GetSession()&&
            serial==N::session_serial::Current()&&world==ue_wrap::world_identity::Generation()&&
            worldObject&&worldObject==ue_wrap::world_identity::CurrentWorld()&&
            stops==session->StopCount()&&session->running()&&session->role()==role;
    }
    bool Publishing()const{return Current()&&connected&&session->connected();}
};
std::optional<Context> g_rotationContext;
size_t g_turn=0;
bool SameContext(const Context& a,const Context& b){
    return a.session==b.session&&a.serial==b.serial&&a.world==b.world&&a.worldObject==b.worldObject&&
        a.stops==b.stops&&a.epoch==b.epoch&&a.role==b.role;
}
struct Actor {
    ue_wrap::CachedObjRef ref,cls;
    bool Capture(void* value){
        ref.Set(value);if(!ref.Get())return false;
        cls.Set(R::ClassOf(value));return Current();
    }
    bool Current()const{return ref.Get()&&cls.Get()&&R::ClassOf(ref.Raw())==cls.Raw();}
};
struct Row {
    E::Registry::IdentitySnapshot identity;
    Actor actor;
    bool Bound()const {
        E::Registry::IdentitySnapshot now;
        return E::Registry::Get().ReadIdentity(identity.id,now)&&now.incarnation==identity.incarnation&&
            now.type==E::ElementType::Npc&&!now.mirror&&!now.deleting&&now.typeName==identity.typeName&&
            now.actor==identity.actor&&now.internalIdx==identity.internalIdx;
    }
    bool Current()const {
        return Bound()&&actor.Current()&&E::Registry::Get().EidForActor(actor.ref.Raw())==identity.id;
    }
};
struct Hold {Context context;Row owner;Actor prop;coop::prop_drive_host::ClaimLease lease;bool active=true,released=false;};
struct Note {Context context;Actor prop;};
std::vector<std::shared_ptr<Hold>> g_holds;
std::vector<Note> g_notes;
bool Installed(const std::shared_ptr<Hold>& hold){
    return hold->context.Current()&&std::find(g_holds.begin(),g_holds.end(),hold)!=g_holds.end();
}
bool Selected(const std::shared_ptr<Hold>& hold){
    return Installed(hold)&&hold->active&&hold->owner.Current()&&hold->prop.Current();
}
bool SelectionCurrent(const std::shared_ptr<Hold>& hold){
    if(!Selected(hold))return false;
    void* selected=ue_wrap::kerfur::ReadHoldObject(hold->owner.actor.ref.Raw());
    return Selected(hold)&&selected==hold->prop.ref.Raw();
}
bool OtherHold(const std::shared_ptr<Hold>& hold){
    for(const auto& other:g_holds)
        if(other!=hold&&Selected(other)&&other->prop.ref.Raw()==hold->prop.ref.Raw())return true;
    return false;
}
void Erase(const std::shared_ptr<Hold>& hold){
    g_holds.erase(std::remove(g_holds.begin(),g_holds.end(),hold),g_holds.end());
}
void SyncKerfurHold(const Context& context,const Row& owner){
    const auto current=[&]{return context.Current()&&owner.Current();};
    if(!current())return;
    void* selected=ue_wrap::kerfur::ReadHoldObject(owner.actor.ref.Raw());
    if(!current())return;
    std::shared_ptr<Hold> hold;
    for(const auto& old:g_holds){
        if(!old->active||old->owner.identity.id!=owner.identity.id)continue;
        if(old->owner.identity.incarnation==owner.identity.incarnation&&Selected(old)&&
            selected==old->prop.ref.Raw())hold=old;
        else old->active=false;
    }
    if(!selected)return;
    if(!hold){
        auto next=std::make_shared<Hold>();next->context=context;next->owner=owner;
        if(!next->prop.Capture(selected)||!current())return;
        g_holds.push_back(next);hold=std::move(next);
    }
    if(!SelectionCurrent(hold))return;
    const bool inHand=coop::prop_drive_host::InAHand(selected);
    if(!SelectionCurrent(hold)||inHand)return;
    const bool frozen=U::SetActorSimulatePhysics(selected,false);
    if(!frozen||!SelectionCurrent(hold))return;
    const bool handAfterFreeze=coop::prop_drive_host::InAHand(selected);
    if(!SelectionCurrent(hold)||handAfterFreeze)return;
    const auto lease=coop::prop_drive_host::ClaimOwned(selected,coop::prop_drive_host::ClaimSource::Kerfur,"kerfur",
        [&]{return context.Current()&&owner.Current();});
    if(Selected(hold)&&lease)hold->lease=lease;
    // No borrowed vector row survives Claim's native work.
}
void SweepKerfurHolds(const Context& context){
    const auto snapshot=g_holds;
    for(const auto& hold:snapshot){
        if(!context.Current())return;
        if(!Installed(hold)||!hold->prop.Current()){Erase(hold);continue;}
        if(hold->active&&hold->owner.Current())continue;
        hold->active=false;
        if(OtherHold(hold)){Erase(hold);continue;}
        const auto current=[&]{return context.Current()&&Installed(hold)&&!hold->active&&
            hold->prop.Current()&&!OtherHold(hold);};
        if(!current())continue;
        const bool inHand=coop::prop_drive_host::InAHand(hold->prop.ref.Raw());
        if(!current())continue;
        if(inHand){Erase(hold);continue;}
        if(!hold->released){
            if(hold->lease&&coop::prop_drive_host::OwnsClaim(hold->lease)) {
                if(!coop::prop_drive_host::ReleaseOwned(hold->lease,current))continue;
                if(!current())continue;
            }
            hold->released=true;
        }
        if(coop::prop_drive_host::HasClaims(hold->prop.ref.Raw())){Erase(hold);continue;}
        const bool handAfterRelease=coop::prop_drive_host::InAHand(hold->prop.ref.Raw());
        if(!current())continue;
        if(handAfterRelease){Erase(hold);continue;}
        if(coop::prop_drive_host::HasClaims(hold->prop.ref.Raw())){Erase(hold);continue;}
        const bool thawed=U::SetActorSimulatePhysics(hold->prop.ref.Raw(),true);
        if(current()&&thawed)Erase(hold);
    }
}
bool VectorOkay(const ue_wrap::FVector& value,float limit){
    return std::isfinite(value.X)&&std::isfinite(value.Y)&&std::isfinite(value.Z)&&
        std::fabs(value.X)<=limit&&std::fabs(value.Y)<=limit&&std::fabs(value.Z)<=limit;
}
bool ReadPose(const Context& context,const Row& row,N::EntityPoseSnapshot& snap){
    const auto current=[&]{return context.Publishing()&&row.Current();};
    if(!current())return false;
    void* actor=row.actor.ref.Raw();
    ue_wrap::FVector location{},scale{};ue_wrap::FRotator rotation{};
    if(!U::TryGetActorTransform(actor,location,rotation,scale,current)||!current())return false;
    if(!VectorOkay(location,N::kMaxCoord)||!std::isfinite(rotation.Pitch)||
        !std::isfinite(rotation.Yaw)||!std::isfinite(rotation.Roll))return false;
    const auto velocity=U::GetActorVelocity(actor);
    if(!current()||!VectorOkay(velocity,N::kMaxSpeed))return false;
    snap.elementId=row.identity.id;snap.x=location.X;snap.y=location.Y;snap.z=location.Z;
    snap.yaw=ue_wrap::NormalizeAxis(rotation.Yaw);snap.pitch=ue_wrap::NormalizeAxis(rotation.Pitch);
    snap.roll=ue_wrap::NormalizeAxis(rotation.Roll);
    snap.speed=std::hypot(velocity.X,velocity.Y);
    if(!std::isfinite(snap.speed)||snap.speed>N::kMaxSpeed)return false;
    const bool falling=ue_wrap::puppet::ReadCharacterIsFalling(actor);
    if(!current())return false;
    snap.stateBits=falling?N::kStateBitInAir:0;
    ue_wrap::FVector look{};
    const bool hasLook=ue_wrap::puppet::ReadKerfurLookAt(actor,look);
    if(!current())return false;
    if(hasLook&&VectorOkay(look,N::kMaxCoord)){
        snap.lookAtX=look.X;snap.lookAtY=look.Y;snap.lookAtZ=look.Z;snap.stateBits|=N::kEntityPoseBitHasLookAt;
    }
    float yaw=0;
    bool hasYaw=ue_wrap::puppet::ReadKerfurBodyYaw(actor,yaw);
    if(!current())return false;
    if(!hasYaw){hasYaw=ue_wrap::fossilhound::ReadBodyYaw(actor,yaw);if(!current())return false;}
    if(!hasYaw){hasYaw=ue_wrap::rufus::ReadBodyYaw(actor,yaw);if(!current())return false;}
    if(hasYaw&&std::isfinite(yaw)){snap.bodyYaw=ue_wrap::NormalizeAxis(yaw);snap.stateBits|=N::kEntityPoseBitHasBodyYaw;}
    uint8_t state=0,face=0;bool spooky=false;
    const bool hasState=ue_wrap::kerfur::ReadKerfurState(actor,state,spooky,face);
    if(!current())return false;
    if(hasState){
        snap.kerfState=state;snap.kerfFace=face;snap.stateBits|=N::kEntityPoseBitHasKerfurState;
        if(spooky)snap.stateBits|=N::kEntityPoseBitKerfurSpooky;
        bool hasDisc=false;uint8_t disc=0;
        const bool readable=ue_wrap::kerfur::ReadKerfurFloppy(actor,hasDisc,disc);
        if(!current())return false;
        if(readable&&hasDisc){snap.floppyType=disc;snap.stateBits|=N::kEntityPoseBitKerfurHasFloppy;}
    }
    const bool onAtv=ue_wrap::sit_pose::ReadIsOnAtv(actor);
    if(!current())return false;
    if(onAtv)snap.stateBits|=N::kEntityPoseBitKerfurOnAtv;
    return true;
}
} // namespace

void DropKerfurHolds(){
    ++g_epoch;g_holds.clear();g_notes.clear();g_turn=0;
    // Drive teardown owns its rows. Keep the stack recursion guard set until Tick returns.
}
bool KerfurIsHolding(void* actor){
    if(!actor)return false;
    for(const auto& hold:g_holds)if(Selected(hold)&&hold->prop.ref.Raw()==actor)return true;
    for(const auto& note:g_notes)if(note.context.Current()&&note.prop.Current()&&note.prop.ref.Raw()==actor)return true;
    return false;
}
void NoteKerfurHolding(void* actor){
    Context context;if(!context.Current()||!actor)return;
    Note note;note.context=context;
    if(!note.prop.Capture(actor)||!context.Current())return;
    g_notes.erase(std::remove_if(g_notes.begin(),g_notes.end(),[&](const Note& old){
        return !old.context.Current()||!old.prop.Current()||old.prop.ref.Raw()==actor;
    }),g_notes.end());
    g_notes.push_back(std::move(note));
}
void ForgetKerfurHolding(void* actor){
    auto* session=GetSession();if(!actor||!session||session->role()!=N::Role::Client)return;
    g_notes.erase(std::remove_if(g_notes.begin(),g_notes.end(),[&](const Note& note){return note.prop.ref.Raw()==actor;}),g_notes.end());
}
void QueueConnectBroadcastForSlot(int slot){coop::npc_join_delivery::Queue(slot);}
void TickPoseStream(){
    if(g_ticking)return;
    struct Busy{Busy(){g_ticking=true;}~Busy(){g_ticking=false;}} busy;
    coop::npc_join_delivery::Tick();
    Context context;if(context.role!=N::Role::Host||!context.Current())return;
    if(!g_rotationContext||!SameContext(context,*g_rotationContext)){
        g_rotationContext=context;g_turn=0;
        g_holds.clear();g_notes.clear();
    }
    coop::npc_world_enum::DrainPendingExSpawns();
    if(!context.Current())return;
    coop::kerfur_census::Tick();
    if(!context.Current())return;
    std::vector<E::Registry::IdentitySnapshot> identities;
    E::Registry::Get().SnapshotIdentitiesByType(E::ElementType::Npc,identities);
    struct Pending{Row row;N::EntityPoseSnapshot pose;};
    std::vector<Pending> pending;
    const size_t total=identities.size();
    const size_t start=total>static_cast<size_t>(N::kMaxNpcBatchEntries)?g_turn++%total:0;
    for(size_t k=0;k<total;++k){
        if(!context.Current())return;
        Row row;row.identity=identities[(start+k)%total];
        if(!row.identity.actor||row.identity.mirror||row.identity.deleting||!row.Bound())continue;
        if(!R::IsLiveByIndex(row.identity.actor,row.identity.internalIdx)){
            // No callback between the identity check and its EID-keyed retirement.
            if(context.Current()&&row.Bound())SyncDestroyedNpcByEid(row.identity.id,row.identity.actor);
            continue;
        }
        if(!row.actor.Capture(row.identity.actor)||!row.Current())continue;
        SyncKerfurHold(context,row);
        if(!context.Current())return;
        if(!context.Publishing()||!row.Current()||pending.size()>=N::kMaxNpcBatchEntries)continue;
        N::EntityPoseSnapshot pose{};
        if(ReadPose(context,row,pose))pending.push_back({row,pose});
    }
    SweepKerfurHolds(context);
    if(!context.Publishing())return;
    std::vector<N::EntityPoseSnapshot> batch;batch.reserve(pending.size());
    for(const auto& item:pending)if(item.row.Current())batch.push_back(item.pose);
    if(!context.Publishing())return;
    context.session->SetLocalNpcPoseBatch(batch);
}
} // namespace coop::npc_sync
