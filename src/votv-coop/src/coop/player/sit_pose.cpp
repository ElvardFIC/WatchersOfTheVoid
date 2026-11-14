// coop/player/sit_pose.cpp -- chair body/rig presentation; see sit_pose.h.
#include "coop/player/sit_pose.h"
#include "coop/interactables/atv_sync.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/net/wire_key_util.h"
#include "coop/element/intent_authority.h"
#include "coop/props/active_drive.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/player/players_registry.h"
#include "coop/player/puppet_drive.h"
#include "coop/player/remote_player.h"
#include "coop/player/roster_ledger.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/actors/sit_pose.h"
#include "ue_wrap/actors/sleep.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/desk/device_screen.h"
#include "ue_wrap/devices/atv.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/engine_component.h"
#include "ue_wrap/engine/world_identity.h"
#include <algorithm>
#include <atomic>
#include <cmath>

namespace coop::sit_pose {
namespace {
namespace E=ue_wrap::engine;namespace R=ue_wrap::reflection;
using ue_wrap::CachedObjRef;using ue_wrap::FVector;using ue_wrap::FRotator;
std::atomic<coop::net::Session*> g_session{nullptr};
bool g_localSitting=false,g_inTick=false;
CachedObjRef g_lastChair,g_lastPublisher;
CachedObjRef g_localDisplayed,g_displayed[coop::players::kMaxPeers];
uint64_t g_lastAnchorAt=0,g_revision=0;
coop::net::SitAnchorPayload g_sent{};
uint64_t NowMs(){return coop::active_drive::NowMs();}
struct Scope {
    coop::net::Session* session=nullptr;
    coop::net::Role role{};
    uint32_t serial=0,generation=0;void* world=nullptr;bool active=false;
    bool operator==(const Scope&)const=default;
};
Scope g_scope;
Scope CaptureScope(){
    Scope s;s.session=g_session.load(std::memory_order_acquire);
    s.serial=coop::net::session_serial::Current();s.generation=ue_wrap::world_identity::Generation();
    s.world=ue_wrap::world_identity::CurrentWorld();
    if(s.session){s.role=s.session->role();s.active=s.session->running()&&s.session->connected()&&s.world;}
    return s;
}
struct Pose {FVector location{};float yaw=0;FRotator chair{};};
struct Anchor {
    bool on=false,ready=false,wasSitting=false;
    uint16_t person=0;uint32_t link=0;
    uint64_t received=0,lerpAt=0,revision=0;
    coop::net::WireKey key{};
    CachedObjRef chair,chairClass;
    Pose from{},to{};
};
Anchor g_anchor[coop::players::kMaxPeers];
void ClearAnchor(uint8_t slot){g_anchor[slot]={};g_anchor[slot].revision=++g_revision;}
void ResetState(){
    g_localSitting=false;g_lastChair.Reset();g_lastPublisher.Reset();g_lastAnchorAt=0;g_sent={};++g_revision;
    for(uint8_t slot=0;slot<coop::players::kMaxPeers;++slot)ClearAnchor(slot);
    const auto local=g_localDisplayed;g_localDisplayed.Reset();
    CachedObjRef displayed[coop::players::kMaxPeers];
    for(uint8_t slot=0;slot<coop::players::kMaxPeers;++slot){displayed[slot]=g_displayed[slot];g_displayed[slot].Reset();}
    const auto scope=CaptureScope();const uint64_t revision=g_revision;
    const auto current=[&]{return revision==g_revision&&scope==CaptureScope();};
    if(void* body=local.Get()){ue_wrap::sit_pose::DriveSitting(body,false);ue_wrap::sit_pose::ClearMeshShift(body);}
    for(auto& ref:displayed){if(!current())return;if(void* body=ref.Get())ue_wrap::sit_pose::ClearMeshShift(body);}
}
bool SyncScope(){
    const auto now=CaptureScope();
    if(!(now==g_scope)){ResetState();if(!(now==CaptureScope()))return false;g_scope=now;}
    return now.active;
}
bool Current(const Scope& s){return s.active&&s==CaptureScope();}
bool IsChair(void* a){return a&&R::IsLive(a)&&ue_wrap::prop::IsDescendantOfProp(a)&&R::ClassNameOf(a).find(L"prop_sit")==0;}
bool Finite(const FVector& p){return std::isfinite(p.X)&&std::isfinite(p.Y)&&std::isfinite(p.Z);}
bool Finite(const FRotator& r){return std::isfinite(r.Pitch)&&std::isfinite(r.Yaw)&&std::isfinite(r.Roll);}
bool ValidKey(const coop::net::WireKey& key){
    if(!key.len||key.len>sizeof(key.data))return false;
    for(size_t i=0;i<key.len;++i)if(static_cast<unsigned char>(key.data[i])<33||static_cast<unsigned char>(key.data[i])>126)return false;
    return coop::net::StringFromWireKey(key)!=L"None";
}
bool SameKey(const coop::net::WireKey& a,const coop::net::WireKey& b){
    return a.len==b.len&&coop::net::StringFromWireKey(a)==coop::net::StringFromWireKey(b);
}
float Arc(float from,float to,float alpha){return ue_wrap::NormalizeAxis(from+ue_wrap::NormalizeAxis(to-from)*alpha);}
Pose Render(const Anchor& a,uint64_t now){
    const float alpha=now<a.lerpAt?1.f:(std::min)(1.f,float(now-a.lerpAt)/50.f);
    Pose p;p.location={a.from.location.X+(a.to.location.X-a.from.location.X)*alpha,
        a.from.location.Y+(a.to.location.Y-a.from.location.Y)*alpha,a.from.location.Z+(a.to.location.Z-a.from.location.Z)*alpha};
    p.yaw=Arc(a.from.yaw,a.to.yaw,alpha);
    p.chair={Arc(a.from.chair.Pitch,a.to.chair.Pitch,alpha),Arc(a.from.chair.Yaw,a.to.chair.Yaw,alpha),Arc(a.from.chair.Roll,a.to.chair.Roll,alpha)};
    return p;
}
bool PeerCurrent(uint8_t slot,const Anchor& a){
    auto* s=g_scope.session;
    return s&&a.person&&a.person==coop::roster_ledger::Get(slot).playerNo&&a.link==s->peerGenerationForSlot(slot);
}
enum class Admission {Pending,Refused,Ready};
Admission Admit(uint8_t slot,Anchor& a,const Scope& scope){
    if(!Current(scope)||!PeerCurrent(slot,a))return Admission::Refused;
    if(a.chair.Raw()&&(!a.chair.Get()||!a.chairClass.Get()))return Admission::Refused;
    void* chair=a.chair.Get();
    if(!chair){
        chair=coop::prop_element_tracker::ResolveLiveActorByKey(coop::net::StringFromWireKey(a.key),nullptr);
        if(!chair)return Admission::Pending;
        if(!IsChair(chair))return Admission::Refused;
        a.chair.Set(chair);a.chairClass.Set(R::ClassOf(chair));
    }
    const auto valid=[&]{return Current(scope)&&PeerCurrent(slot,a)&&a.chair.Get()&&a.chairClass.Get()&&
        ue_wrap::world_identity::WorldOf(chair)==scope.world&&R::ClassOf(chair)==a.chairClass.Get();};
    if(!valid()||!IsChair(chair)||ue_wrap::prop::GetInteractableKeyString(chair)!=coop::net::StringFromWireKey(a.key)||!valid())
        return Admission::Refused;
    FVector chairPosition{};
    if(!E::TryGetActorLocation(chair,chairPosition)){a.ready=false;return valid()?Admission::Pending:Admission::Refused;}
    if(!valid()||!Finite(chairPosition))return Admission::Refused;
    const double dx=double(a.to.location.X)-chairPosition.X,dy=double(a.to.location.Y)-chairPosition.Y,dz=double(a.to.location.Z)-chairPosition.Z;
    if(dx*dx+dy*dy+dz*dz>450.0*450.0)return Admission::Refused;
    if(scope.role==coop::net::Role::Host){
        const auto result=coop::element::IntentTarget::ForClientIntent(*scope.session,slot,450.f).Authorize(chair);
        if(!valid())return Admission::Refused;
        if(!result){a.ready=false;return result.outcome==coop::element::IntentOutcome::NoBody||
            result.outcome==coop::element::IntentOutcome::NoTarget?Admission::Pending:Admission::Refused;}
    }
    void* local=coop::players::Registry::Get().Local();void* localSeat=nullptr;
    CachedObjRef localBody,localClass;
    if(!local||!R::IsLive(local))return Admission::Pending;
    localBody.Set(local);localClass.Set(R::ClassOf(local));
    const auto localCurrent=[&]{return localBody.Get()&&localClass.Get()&&coop::players::Registry::Get().Local()==local&&R::ClassOf(local)==localClass.Get();};
    if(!E::ReadMainPlayerSittingOn(local,localSeat)||localSeat==chair||!localCurrent()||!valid())return Admission::Refused;
    void* confirmed=nullptr;
    if(!E::ReadMainPlayerSittingOn(local,confirmed)||confirmed!=localSeat||confirmed==chair||!localCurrent()||!valid())return Admission::Refused;
    a.ready=true;return Admission::Ready;
}
bool Sample(void* local,void* chair,coop::net::SitAnchorPayload& p,const Scope& scope){
    CachedObjRef player,seat,playerClass,seatClass;player.Set(local);seat.Set(chair);
    playerClass.Set(R::ClassOf(local));seatClass.Set(R::ClassOf(chair));
    const auto valid=[&]{void* bound=nullptr;return Current(scope)&&player.Get()&&seat.Get()&&playerClass.Get()&&seatClass.Get()&&
        coop::players::Registry::Get().Local()==local&&R::ClassOf(local)==playerClass.Get()&&R::ClassOf(chair)==seatClass.Get()&&
        E::ReadMainPlayerSittingOn(local,bound)&&bound==chair;};
    FVector location{};FRotator body{},rotation{};
    if(!E::TryGetActorLocation(local,location)||!valid()||!E::TryGetActorRotation(local,body)||!valid()||
       !E::TryGetActorRotation(chair,rotation)||!valid()||!Finite(location)||!Finite(body)||!Finite(rotation))return false;
    const auto key=ue_wrap::prop::GetInteractableKeyString(chair);
    if(!valid()||key.empty()||key.size()>sizeof(p.chairKey.data))return false;
    for(wchar_t c:key)if(c<33||c>126)return false;
    coop::net::WireKeyFromString(key,p.chairKey);
    if(!ValidKey(p.chairKey))return false;
    p.x=location.X;p.y=location.Y;p.z=location.Z;p.yaw=ue_wrap::NormalizeAxis(body.Yaw);p.active=1;
    p.chairPitch=ue_wrap::NormalizeAxis(rotation.Pitch);p.chairYaw=ue_wrap::NormalizeAxis(rotation.Yaw);p.chairRoll=ue_wrap::NormalizeAxis(rotation.Roll);
    return valid()&&ue_wrap::prop::GetInteractableKeyString(chair)==key&&valid();
}
bool Changed(const coop::net::SitAnchorPayload& p){
    const float x=p.x-g_sent.x,y=p.y-g_sent.y,z=p.z-g_sent.z;
    const auto turned=[](float a,float b){return std::fabs(ue_wrap::NormalizeAxis(a-b))>=.5f;};
    return !SameKey(p.chairKey,g_sent.chairKey)||x*x+y*y+z*z>=1.f||
        turned(p.yaw,g_sent.yaw)||turned(p.chairPitch,g_sent.chairPitch)||
        turned(p.chairYaw,g_sent.chairYaw)||turned(p.chairRoll,g_sent.chairRoll);
}
} // namespace

void Install(coop::net::Session* session){g_session.store(session,std::memory_order_release);SyncScope();}
bool LocalIsSitting(){return g_localSitting;}
void Tick(){
    if(g_inTick)return;
    g_inTick=true;struct Exit{~Exit(){g_inTick=false;}}exit;
    const bool active=SyncScope();const Scope scope=g_scope;
    void* local=coop::players::Registry::Get().Local();
    if(!active||!local||!R::IsLive(local)){
        if(local){ue_wrap::sit_pose::DriveSitting(local,false);ue_wrap::sit_pose::ClearMeshShift(local);}
        g_localSitting=false;return;
    }
    CachedObjRef player,playerClass;player.Set(local);playerClass.Set(R::ClassOf(local));
    void* seat=nullptr;bool ragdoll=false,dead=false;
    const bool read=E::ReadMainPlayerSittingOn(local,seat)&&E::ReadMainPlayerRagdollState(local,ragdoll,dead);
    const bool hasSeat=read&&seat&&R::IsLive(seat);
    const bool onAtv=hasSeat&&ue_wrap::atv::EnsureResolved()&&ue_wrap::atv::IsAtv(seat);
    const bool chair=hasSeat&&IsChair(seat);
    const bool computer=ue_wrap::device_screen::ClassifyWidgetClaimKey(ue_wrap::device_screen::ReadActiveInterface(local))==L"laptop";
    if(!Current(scope)||!player.Get()||coop::players::Registry::Get().Local()!=local)return;
    g_localSitting=read&&!computer&&!ue_wrap::sleep::IsSleeping()&&!ragdoll&&!dead&&(chair||onAtv||coop::atv_sync::LocalIsDriving());
    ue_wrap::sit_pose::DriveSitting(local,g_localSitting);
    g_localDisplayed=player;
    if(g_localSitting){
        const float head=ue_wrap::sit_pose::ModelHead(local);
        if(onAtv||coop::atv_sync::LocalIsDriving())ue_wrap::sit_pose::ShiftMesh(local,head*1.5f,head*3.f);
        else ue_wrap::sit_pose::ShiftMesh(local,-ue_wrap::sit_pose::ModelHalfLength(local)-head*2.5f,head*4.f);
    }else ue_wrap::sit_pose::ClearMeshShift(local);
    if(!Current(scope)||!player.Get())return;
    coop::net::SitAnchorPayload packet{};
    const bool sitting=chair&&g_localSitting;
    CachedObjRef capturedSeat,capturedClass;if(sitting){capturedSeat.Set(seat);capturedClass.Set(R::ClassOf(seat));}
    if(sitting&&!Sample(local,seat,packet,scope))return;
    if(!sitting&&!g_lastChair.Raw())return;
    const uint64_t now=NowMs();
    const bool first=!g_lastChair.Is(seat)||!g_lastPublisher.Is(local);
    const uint64_t elapsed=now<g_lastAnchorAt?1000:now-g_lastAnchorAt;
    if(sitting&&!first&&(elapsed<50||(!Changed(packet)&&elapsed<1000)))return;
    const uint64_t revision=g_revision;
    if(!scope.session->SendReliable(coop::net::ReliableKind::SitAnchor,&packet,sizeof(packet)))return;
    // A callback can replace a pawn/seat/session or clear the enclosing state.
    void* currentSeat=nullptr;
    if(!Current(scope)||revision!=g_revision||!player.Get()||!playerClass.Get()||R::ClassOf(local)!=playerClass.Get()||
       coop::players::Registry::Get().Local()!=local||!E::ReadMainPlayerSittingOn(local,currentSeat)||
       (sitting&&(!capturedSeat.Get()||!capturedClass.Get()||R::ClassOf(seat)!=capturedClass.Get()||currentSeat!=seat))||
       (!sitting&&currentSeat&&currentSeat!=seat)||!Current(scope)||revision!=g_revision)return;
    if(sitting){g_lastChair=capturedSeat;g_lastPublisher=player;g_sent=packet;g_lastAnchorAt=now;}
    else{g_lastChair.Reset();g_lastPublisher.Reset();g_sent={};g_lastAnchorAt=0;}
}

void OnAnchor(uint8_t slot,const coop::net::SitAnchorPayload& incoming){
    const auto p=incoming;
    if(!SyncScope()||slot>=coop::players::kMaxPeers||slot==coop::players::Registry::Get().LocalPeerId()||
       p.active>1||p.pad[0]||p.pad[1]||p.pad[2]||!Finite(FVector{p.x,p.y,p.z})||!std::isfinite(p.yaw)||
       !Finite(FRotator{p.chairPitch,p.chairYaw,p.chairRoll})||
       std::fabs(p.chairPitch)>180||std::fabs(p.chairYaw)>180||std::fabs(p.chairRoll)>180)return;
    if(!coop::roster_ledger::Get(slot).playerNo)return;
    if(!p.active){ClearAnchor(slot);return;} // OFF never rotates the released chair.
    if(!ValidKey(p.chairKey))return;
    const Scope scope=g_scope;const uint64_t revision=g_anchor[slot].revision;
    const uint64_t now=NowMs();Anchor next;
    const auto& old=g_anchor[slot];
    if(old.on&&old.ready&&PeerCurrent(slot,old)&&SameKey(old.key,p.chairKey)&&now>=old.received&&now-old.received<=3000){
        next=old;next.from=Render(old,now);
    }else next.from={{p.x,p.y,p.z},ue_wrap::NormalizeAxis(p.yaw),{p.chairPitch,p.chairYaw,p.chairRoll}};
    next.on=true;next.key=p.chairKey;next.person=coop::roster_ledger::Get(slot).playerNo;
    next.link=scope.session->peerGenerationForSlot(slot);next.received=next.lerpAt=now;
    next.to={{p.x,p.y,p.z},ue_wrap::NormalizeAxis(p.yaw),{p.chairPitch,p.chairYaw,p.chairRoll}};
    if(Admit(slot,next,scope)==Admission::Refused)return;
    if(!Current(scope)||g_anchor[slot].revision!=revision||!PeerCurrent(slot,next))return;
    next.revision=++g_revision;g_anchor[slot]=next;
}
void DriveRemotes(){
    if(!SyncScope())return;
    const Scope scope=g_scope;const uint8_t localSlot=coop::players::Registry::Get().LocalPeerId();
    for(uint8_t slot=0;slot<coop::players::kMaxPeers;++slot){
        if(slot==localSlot)continue;
        auto& pup=coop::puppet_drive::Puppet(static_cast<int>(slot));void* actor=pup.GetActor();
        if(!actor||!pup.valid())continue;
        if(coop::atv_sync::ActorSeatedBy(slot)){if(g_anchor[slot].on)ClearAnchor(slot);continue;}
        Anchor next=g_anchor[slot];const uint64_t revision=next.revision,now=NowMs();
        if(!next.on||!PeerCurrent(slot,next)||now<next.received||now-next.received>3000){
            if(next.on)ClearAnchor(slot);ue_wrap::sit_pose::ClearMeshShift(actor);continue;
        }
        if(!pup.IsSitting()){
            if(next.wasSitting)ClearAnchor(slot);
            ue_wrap::sit_pose::ClearMeshShift(actor);continue;
        }
        CachedObjRef body;body.Set(actor);
        const auto current=[&]{return Current(scope)&&body.Get()&&pup.valid()&&pup.GetActor()==actor&&pup.IsSitting()&&
            g_anchor[slot].revision==revision&&PeerCurrent(slot,next);};
        const auto admission=Admit(slot,next,scope);
        if(!current())continue;
        if(admission==Admission::Refused){ClearAnchor(slot);ue_wrap::sit_pose::ClearMeshShift(actor);continue;}
        if(admission==Admission::Pending)continue;
        const Pose pose=Render(next,now);
        const auto rigCurrent=[&]{return current()&&next.chair.Get()&&next.chairClass.Get()&&R::ClassOf(next.chair.Get())==next.chairClass.Get();};
        if(!rigCurrent())continue;
        // Run after other prop presentation; chair and body share the same render alpha.
        if(!E::SetActorRotation(next.chair.Get(),pose.chair)||!rigCurrent())continue;
        if(!E::SetActorLocation(actor,pose.location)||!rigCurrent())continue;
        if(!E::SetActorRotation(actor,{0,pose.yaw,0})||!rigCurrent())continue;
        ue_wrap::sit_pose::DriveSitting(actor,true);
        if(!rigCurrent())continue;
        const float head=ue_wrap::sit_pose::ModelHead(actor);
        const float half=ue_wrap::sit_pose::ModelHalfLength(actor);
        if(!rigCurrent())continue;
        ue_wrap::sit_pose::ShiftMesh(actor,-half-head*2.5f,head*4.f);
        if(!rigCurrent())continue;
        pup.RefreshSeatView();
        if(!rigCurrent())continue;
        g_displayed[slot]=body;
        next.wasSitting=true;g_anchor[slot]=next;
    }
}
void OnPeerLeft(uint8_t slot){
    if(slot>=coop::players::kMaxPeers)return;
    ClearAnchor(slot);
    auto& pup=coop::puppet_drive::Puppet(static_cast<int>(slot));
    if(pup.valid())ue_wrap::sit_pose::ClearMeshShift(pup.GetActor());
}

void DriveAtvDrivers() {
    if(!SyncScope())return;
    const Scope scope=g_scope;const uint8_t localSlot=coop::players::Registry::Get().LocalPeerId();
    void* atvs[8]{};uint8_t drivers[8]{};
    const int count=coop::atv_sync::CopyIndexed(atvs,drivers,8);
    for(int i=0;i<count&&i<8;++i){
        const uint8_t slot=drivers[i];
        if(slot==0xFF||slot==localSlot||slot>=coop::players::kMaxPeers)continue;
        auto& pup=coop::puppet_drive::Puppet(int(slot));void* actor=pup.GetActor();void* atv=atvs[i];
        if(!actor||!pup.valid()||!pup.IsSitting()||!atv||!R::IsLive(atv))continue;
        CachedObjRef body,rig,cls;body.Set(actor);rig.Set(atv);cls.Set(R::ClassOf(atv));
        const auto person=coop::roster_ledger::Get(slot).playerNo;
        const auto link=scope.session->peerGenerationForSlot(slot);
        const auto current=[&]{return Current(scope)&&body.Get()&&rig.Get()&&cls.Get()&&R::ClassOf(atv)==cls.Get()&&
            ue_wrap::world_identity::WorldOf(atv)==scope.world&&pup.valid()&&pup.GetActor()==actor&&pup.IsSitting()&&
            person&&coop::roster_ledger::Get(slot).playerNo==person&&scope.session->peerGenerationForSlot(slot)==link&&
            coop::atv_sync::ActorSeatedBy(slot)==atv;};
        if(!current())continue;
        FVector loc{};FRotator rot{};
        void* component=ue_wrap::sit_pose::AtvDriverSeat(atv);
        CachedObjRef seat,seatClass;
        if(component){
            seat.Set(component);seatClass.Set(R::ClassOf(component));
            const auto seatCurrent=[&]{return current()&&seat.Get()&&seatClass.Get()&&R::ClassOf(component)==seatClass.Get()&&
                ue_wrap::sit_pose::AtvDriverSeat(atv)==component;};
            if(!E::TryGetComponentLocation(component,loc)||!seatCurrent()||
               !E::TryGetComponentWorldRotation(component,rot)||!seatCurrent())continue;
        }else{
            if(!E::TryGetActorLocation(atv,loc)||!current()||!E::TryGetActorRotation(atv,rot)||!current())continue;
            const FVector fwd=E::GetActorForwardVector(atv);
            const double length=double(fwd.X)*fwd.X+double(fwd.Y)*fwd.Y+double(fwd.Z)*fwd.Z;
            if(!current()||!Finite(fwd)||length<.01||length>4.0)continue;
            loc.X+=fwd.X*40.f;loc.Y+=fwd.Y*40.f;loc.Z+=45.f;
        }
        const auto ready=[&]{return current()&&(!component||(seat.Get()&&seatClass.Get()&&
            R::ClassOf(component)==seatClass.Get()&&ue_wrap::sit_pose::AtvDriverSeat(atv)==component));};
        if(!Finite(loc)||!Finite(rot)||!ready())continue;
        if(!E::SetActorLocation(actor,loc)||!ready())continue;
        if(!E::SetActorRotation(actor,rot)||!ready())continue;
        ue_wrap::sit_pose::DriveSitting(actor,true);if(!ready())continue;
        const float head=ue_wrap::sit_pose::ModelHead(actor);if(!std::isfinite(head)||!ready())continue;
        ue_wrap::sit_pose::ShiftMesh(actor,head*1.5f,head*3.f);if(!ready())continue;
        pup.RefreshSeatView();if(!ready())continue;
        g_displayed[slot]=body;
    }
}

void OnDisconnect(){
    g_session.store(nullptr,std::memory_order_release);ResetState();
    if(g_session.load(std::memory_order_acquire))return;
    g_scope={};const uint64_t revision=g_revision;
    if(void* local=coop::players::Registry::Get().Local()){
        ue_wrap::sit_pose::DriveSitting(local,false);ue_wrap::sit_pose::ClearMeshShift(local);
    }
    for(uint8_t slot=0;slot<coop::players::kMaxPeers;++slot){
        if(g_session.load(std::memory_order_acquire)||revision!=g_revision)return;
        auto& pup=coop::puppet_drive::Puppet(static_cast<int>(slot));
        if(pup.valid())ue_wrap::sit_pose::ClearMeshShift(pup.GetActor());
    }
}
} // namespace coop::sit_pose
