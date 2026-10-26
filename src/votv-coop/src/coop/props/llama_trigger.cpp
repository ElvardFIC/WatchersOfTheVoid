#include "coop/props/llama_trigger.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/creatures/npc_sync.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/active_drive.h"
#include "coop/props/prop_drive_host.h"
#include "ue_wrap/actors/llama_trigger.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>
namespace coop::llama_trigger {
namespace {
namespace M=ue_wrap::llama_trigger;namespace R=ue_wrap::reflection;
struct Scope {net::Session* session=nullptr;uint32_t serial=0,generation=0;void* world=nullptr;bool operator==(const Scope&)const=default;};
struct Row {ue_wrap::CachedObjRef actor,cls;uint64_t checked=0,spent=0;bool polled=false;};
struct Target {RemotePlayer* player=nullptr;ue_wrap::CachedObjRef actor,cls;uint8_t slot=0;uint16_t person=0;uint32_t connection=0;ue_wrap::FVector body{},eye{};double distance=0;};
struct Motion {ue_wrap::CachedObjRef actor,cls;Scope scope;std::function<bool()> current;uint64_t epoch=0,revision=0,born=0,last=0,next=0;};
Scope g_scope;uint64_t g_epoch=0;bool g_busy=false;ue_wrap::CachedObjRef g_launching;
std::unordered_map<void*,Row> g_rows;
std::unordered_map<void*,Motion> g_motion;uint64_t g_motionRevision=0;size_t g_motionTurn=0;bool g_motionBusy=false;
bool Finite(const ue_wrap::FVector& p){return std::isfinite(p.X)&&std::isfinite(p.Y)&&std::isfinite(p.Z);}
double Distance(const ue_wrap::FVector& a,const ue_wrap::FVector& b){const double x=double(a.X)-b.X,y=double(a.Y)-b.Y,z=double(a.Z)-b.Z;return x*x+y*y+z*z;}
Scope Capture(net::Session& s){return {&s,net::session_serial::Current(),ue_wrap::world_identity::Generation(),ue_wrap::world_identity::CurrentWorld()};}
bool Host(net::Session& s){return s.role()==net::Role::Host&&s.running()&&s.connected()&&ue_wrap::world_identity::CurrentWorld();}
bool Held(void* actor){return prop_drive_host::InAHand(actor)||npc_sync::KerfurIsHolding(actor);}
bool Peer(net::Session& s,const Target& t){return t.slot>0&&t.slot<net::kMaxPeers&&t.person&&t.connection&&s.IsSlotConnected(t.slot)&&s.IsSlotReady(t.slot)&&
    s.IsSlotWorldReady(t.slot)&&roster_ledger::Get(t.slot).playerNo==t.person&&s.peerGenerationForSlot(t.slot)==t.connection;}
bool Player(net::Session& s,const Target& t){auto& registry=players::Registry::Get();
    return Peer(s,t)&&registry.Puppet(t.slot)==t.player&&t.player&&t.actor.Get()&&t.cls.Get()&&R::ClassOf(t.actor.Get())==t.cls.Get()&&
        ue_wrap::world_identity::WorldOf(t.actor.Get())==g_scope.world&&t.player->valid()&&t.player->GetActor()==t.actor.Get()&&
        t.player->HasPose()&&!t.player->IsRagdollDisplayed()&&registry.LifeOf(t.slot)==players::PuppetLife::Alive&&
        registry.PeerIdOfActor(t.actor.Get())==t.slot&&Peer(s,t);
}
bool Pose(net::Session& s,const Target& t,ue_wrap::FVector& body,ue_wrap::FVector& eye){
    return Player(s,t)&&t.player->TryGetLocation(body)&&Player(s,t)&&t.player->TryGetEyePosition(eye)&&Player(s,t)&&
        Finite(body)&&Finite(eye)&&Distance(body,eye)<=600.0*600.0;
}
bool MotionOwned(const Motion& row){
    const auto identity=[&]{return row.epoch==g_epoch&&row.scope==g_scope&&row.scope.session&&Host(*row.scope.session)&&row.scope==Capture(*row.scope.session)&&
        row.actor.Get()&&row.cls.Get()&&R::ClassOf(row.actor.Get())==row.cls.Get()&&ue_wrap::world_identity::WorldOf(row.actor.Get())==row.scope.world;};
    return identity()&&row.current&&row.current()&&identity();
}
void EraseMotion(const Motion& row){auto it=g_motion.find(row.actor.Raw());if(it!=g_motion.end()&&it->second.revision==row.revision)g_motion.erase(it);}
void AttemptMotion(Motion row,uint64_t now){
    if(!MotionOwned(row)||Held(row.actor.Get())){EraseMotion(row);return;}
    if(now<row.last){row.born=now;row.next=now;}
    if(now>=row.born&&now-row.born>=10000){EraseMotion(row);return;}
    if(now<row.next)return;row.last=now;row.next=now+100;
    auto it=g_motion.find(row.actor.Raw());if(it==g_motion.end()||it->second.revision!=row.revision)return;it->second=row;
    prop_drive_host::Coast(row.actor.Get(),"llama jump");
    // A Coast callback may requeue newer source debt. Only acknowledge our revision.
    if(!MotionOwned(row)||Held(row.actor.Get())||prop_drive_host::IsDriven(row.actor.Get()))EraseMotion(row);
}
}
void Reset(){++g_epoch;g_scope={};g_rows.clear();g_motion.clear();g_motionTurn=0;g_launching.Reset();}
void QueueNativeMotion(net::Session& s,void* actor,const std::function<bool()>& current){
    if(!Host(s)||!actor||!R::IsLive(actor)||!current)return;const auto scope=Capture(s);
    if(!(scope==g_scope)){Reset();g_scope=scope;}
    std::erase_if(g_motion,[](const auto& pair){const auto& row=pair.second;return !row.actor.Get()||!row.cls.Get()||R::ClassOf(row.actor.Get())!=row.cls.Get();});
    if(g_motion.size()>=256&&!g_motion.contains(actor))return;
    Motion row;row.actor.Set(actor);row.cls.Set(R::ClassOf(actor));row.scope=scope;row.current=current;row.epoch=g_epoch;row.revision=++g_motionRevision;
    row.born=row.last=active_drive::NowMs();g_motion[actor]=row;
    if(g_motionBusy)return;struct Guard{Guard(){g_motionBusy=true;}~Guard(){g_motionBusy=false;}}guard;AttemptMotion(row,row.born);
}
void TickMotion(net::Session& s){
    if(g_motionBusy)return;if(!Host(s)){if(g_scope.session==&s)Reset();return;}
    if(!(g_scope==Capture(s))){Reset();g_scope=Capture(s);return;}if(g_motion.empty())return;
    struct Guard{Guard(){g_motionBusy=true;}~Guard(){g_motionBusy=false;}}guard;
    std::vector<Motion> rows;rows.reserve(g_motion.size());for(const auto& [actor,row]:g_motion)rows.push_back(row);
    const size_t count=std::min(size_t(32),rows.size()),start=g_motionTurn%rows.size();g_motionTurn=(start+count)%rows.size();
    const auto epoch=g_epoch;for(size_t i=0;i<count;++i){AttemptMotion(rows[(start+i)%rows.size()],active_drive::NowMs());if(epoch!=g_epoch)return;}
}
bool BlocksNative(void* actor){
    if(g_launching.Is(actor))return true;const auto it=g_rows.find(actor);if(it==g_rows.end())return false;const auto row=it->second;const auto epoch=g_epoch;
    if(!row.spent||!row.actor.Is(actor)||!row.cls.Get()||R::ClassOf(actor)!=row.cls.Get())return false;
    uint64_t timer=0;const bool read=M::ReadTimer(actor,timer);
    return epoch==g_epoch&&row.actor.Is(actor)&&row.cls.Get()&&R::ClassOf(actor)==row.cls.Get()&&(!read||!timer||timer==row.spent);
}
bool AllowLocal(net::Session& s,void* actor,const std::function<bool()>& current){
    if(!Host(s)||!current||g_busy||!actor||!R::IsLive(actor))return false;const auto scope=Capture(s);const auto epoch=g_epoch;
    struct Guard{Guard(){g_busy=true;}~Guard(){g_busy=false;g_launching.Reset();}}guard;g_launching.Set(actor);
    auto& registry=players::Registry::Get();ue_wrap::CachedObjRef local,cls;local.Set(registry.Local());cls.Set(local.Get()?R::ClassOf(local.Get()):nullptr);
    const auto valid=[&]{return epoch==g_epoch&&scope==Capture(s)&&Host(s)&&current()&&local.Get()&&cls.Get()&&registry.Local()==local.Get()&&
        R::ClassOf(local.Get())==cls.Get()&&ue_wrap::world_identity::WorldOf(local.Get())==scope.world&&!Held(actor)&&epoch==g_epoch&&scope==Capture(s);};
    if(!valid())return false;
    M::Probe probe;std::vector<ue_wrap::CachedObjRef> overlaps;
    return M::Capture(actor,probe,valid)&&M::Overlaps(probe,overlaps,valid)&&valid()&&
        std::any_of(overlaps.begin(),overlaps.end(),[&](const auto& ref){return local.Is(ref.Get());});
}
bool TryRemote(net::Session& s,void* actor,const std::function<bool()>& current){
    if(g_busy||!Host(s)||!current||!actor||!R::IsLive(actor))return false;
    const auto scope=Capture(s);if(!(scope==g_scope)){Reset();g_scope=scope;}
    struct Guard{Guard(){g_busy=true;}~Guard(){g_busy=false;g_launching.Reset();}}guard;g_launching.Set(actor);
    const auto epoch=g_epoch;const auto valid=[&]{return epoch==g_epoch&&g_scope==scope&&scope==Capture(s)&&Host(s)&&current()&&epoch==g_epoch&&
        scope==Capture(s)&&Host(s)&&!Held(actor);};
    if(!valid())return false;
    std::erase_if(g_rows,[](const auto& pair){const auto& row=pair.second;return !row.actor.Get()||!row.cls.Get()||R::ClassOf(row.actor.Get())!=row.cls.Get();});
    auto rowIt=g_rows.find(actor);Row row=rowIt==g_rows.end()?Row{}:rowIt->second;const auto now=active_drive::NowMs();
    if(row.polled&&now>=row.checked&&now-row.checked<500)return false;
    M::Probe probe;if(!M::Capture(actor,probe,valid)||!valid()){
        if(epoch==g_epoch){auto it=g_rows.find(actor);if(it!=g_rows.end()&&!it->second.spent)g_rows.erase(it);}return false;
    }
    if(rowIt==g_rows.end()){
        if(g_rows.size()>=256){auto oldest=g_rows.end();for(auto it=g_rows.begin();it!=g_rows.end();++it)if(!it->second.spent&&(oldest==g_rows.end()||it->second.checked<oldest->second.checked))oldest=it;
            if(oldest==g_rows.end())return false;g_rows.erase(oldest);}
        row.actor=probe.actor;row.cls=probe.cls;
    }
    row.checked=now;row.polled=true;g_rows[actor]=row;
    if(row.spent==probe.timer)return false;
    // A new native timer is a new activation. Old timer delegates remain blocked until then.
    if(row.spent){row.spent=0;g_rows[actor]=row;}
    std::vector<ue_wrap::CachedObjRef> overlaps;if(!M::Overlaps(probe,overlaps,valid)||!valid())return false;
    std::vector<Target> targets;targets.reserve(net::kMaxPeers-1);auto& registry=players::Registry::Get();
    for(int slot=1;slot<net::kMaxPeers;++slot){Target target;target.slot=uint8_t(slot);target.person=roster_ledger::Get(slot).playerNo;target.connection=s.peerGenerationForSlot(slot);
        if(!Peer(s,target))continue;target.player=registry.Puppet(uint8_t(slot));if(!target.player||!target.player->valid())continue;
        void* body=target.player->GetActor();if(!body||!R::IsLive(body)||!std::any_of(overlaps.begin(),overlaps.end(),[&](const auto& ref){return ref.Is(body);}))continue;
        target.actor.Set(body);target.cls.Set(R::ClassOf(body));if(!valid())return false;
        if(!Pose(s,target,target.body,target.eye))continue;if(!valid())return false;
        target.distance=Distance(target.eye,probe.origin);targets.push_back(target);
    }
    std::sort(targets.begin(),targets.end(),[](const auto& a,const auto& b){return a.distance<b.distance||(a.distance==b.distance&&a.slot<b.slot);});
    for(const auto& target:targets){const auto selected=[&]{if(!valid()||!Player(s,target))return false;ue_wrap::FVector body{},eye{};
            return Pose(s,target,body,eye)&&Distance(body,target.body)<=1.0&&Distance(eye,target.eye)<=1.0&&valid();};
        if(!selected())continue;if(!M::ClearLine(probe,target.eye,selected)){if(!valid())return false;continue;}
        const auto completion=[epoch,scope,current,source=probe.actor,cls=probe.cls]{
            return epoch==g_epoch&&g_scope==scope&&scope.session&&scope==Capture(*scope.session)&&Host(*scope.session)&&source.Get()&&cls.Get()&&
                R::ClassOf(source.Get())==cls.Get()&&current()&&epoch==g_epoch&&scope==Capture(*scope.session)&&!Held(source.Get());};
        if(!selected())continue;bool admitted=false;M::Launch(probe,target.eye,admitted,selected,completion);
        if(!admitted){if(!valid())return false;continue;}
        if(epoch==g_epoch&&scope==Capture(s)&&Host(s)&&probe.actor.Get()&&probe.cls.Get()&&R::ClassOf(probe.actor.Get())==probe.cls.Get()){
            row.spent=probe.timer;g_rows[actor]=row;
            QueueNativeMotion(s,actor,completion);
        }return true;
    }return false;
}
}
