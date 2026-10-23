#include "coop/world/event_location_sync.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/props/active_drive.h"
#include "coop/session/teleport_client.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/facts/function_row.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/world/event_location.h"
#include <array>
#include <cmath>
#include <cstring>
namespace coop::event_location_sync {
namespace {
namespace L=ue_wrap::event_location;
namespace R=ue_wrap::reflection;
namespace SG=ue_wrap::script_gate;
UE_FACT_FN(kRowBoxOverlapActors, "event_location_sync", L"KismetSystemLibrary", L"BoxOverlapActors", Dispatch);
constexpr int kTag=0x454C4F43;
struct Rule{const char* name;const wchar_t* field;};
// Only known shared location rows. Runtime association verifies the actual
// activator/box; remote entrants can activate it without relocating other players.
constexpr Rule rules[]={
    {"falseEnter",L"event_falseEnter"},{"vent",L"event_vent"},
    {"obelisk",L"event_obelisk"},{"arirShip",L"event_arirShip"},
    {"piramid",L"event_piramid"},{"wisps",L"event_wisps"},
    {"call0",L"event_bigmRoar"},{"crys",L"event_crys"},
    {"cookier",L"event_cookier"},{"arirEgg",L"event_arirEgg"}};
struct Row{ue_wrap::CachedObjRef controller,box;bool fired=false;};
std::array<Row,std::size(rules)> rows{};
net::Session* session=nullptr;uint32_t world=0,sessionId=0;uint64_t nextPoll=0,epoch=0,nextHook=0;
bool ticking=false;
ue_wrap::CachedObjRef agrav;bool agravFired=false,hooked=false;
struct Entrant{bool pending=false;uint8_t slot=0;ue_wrap::FVector pos{};float yaw=0;};
bool Host(){return session&&session->connected()&&session->role()==net::Role::Host;}
bool Current(){return session&&session->connected()&&world==ue_wrap::world_identity::Generation()&&sessionId==net::session_serial::Current();}
bool Pose(int slot,ue_wrap::FVector& p,float& yaw) {
    if(slot<0||slot>=net::kMaxPeers||!Current())return false;
    const auto stamp=epoch;
    auto& registry=players::Registry::Get();
    if(slot==registry.LocalPeerId()) {
        void* actor=registry.Local();ue_wrap::FRotator rot{};
        if(!actor)return false;
        ue_wrap::CachedObjRef ref;ref.Set(actor);
        if(!ue_wrap::engine::TryGetActorLocation(actor,p)||stamp!=epoch||!Current()||!ref.Is(actor)||
           !ue_wrap::engine::TryGetActorRotation(actor,rot)||stamp!=epoch||!Current()||!ref.Is(actor))return false;
        yaw=rot.Yaw;return std::isfinite(p.X)&&std::isfinite(p.Y)&&std::isfinite(p.Z)&&std::isfinite(yaw);
    }
    if(!session->IsSlotWorldReady(slot))return false;
    auto* puppet=registry.Puppet(static_cast<uint8_t>(slot));
    if(!puppet||!puppet->valid()||!puppet->HasPose()||!puppet->TryGetLocation(p))return false;
    yaw=puppet->GetSyncedAimRotation().Yaw;return std::isfinite(p.X)&&std::isfinite(p.Y)&&std::isfinite(p.Z)&&std::isfinite(yaw);
}
bool FindInside(const L::Box& box,int preferred,Entrant& out) {
    const auto stamp=epoch;
    for(int i=-1;i<net::kMaxPeers;++i) {
        const int slot=i<0?preferred:i;if(slot<0||slot>=net::kMaxPeers||(i>=0&&slot==preferred))continue;
        ue_wrap::FVector pos{};float yaw=0;
        const bool posed=Pose(slot,pos,yaw);
        if(stamp!=epoch||!Current())return false;
        if(posed&&L::Contains(box,pos)) {out={true,static_cast<uint8_t>(slot),pos,yaw};return true;}
    }
    return false;
}
void Bind(size_t i) {
    void* controller=L::Controller(rules[i].field);void* box=L::TriggerBox(controller);
    if(!box){rows[i]={};return;}
    if(!rows[i].box.Is(box)||!rows[i].controller.Is(controller)) {
        rows[i]={};rows[i].controller.Set(controller);rows[i].box.Set(box);
    }
}
SG::Verdict OnArm(const SG::Call& c) {
    if(!Current()||!Host())return SG::Verdict::Run;
    if(c.object==L::Controller(L"event_agrav")){agrav.Set(c.object);agravFired=false;}
    for(size_t i=0;i<rows.size();++i) {
        Bind(i);if(rows[i].controller.Is(c.object))rows[i].fired=false;
    }
    return SG::Verdict::Run;
}
SG::Verdict OnRunBox(const SG::Call& c) {
    if(!Current()||!Host()||c.fromOurCode||!c.object)return SG::Verdict::Run;
    const auto stamp=epoch;
    // Only the actual overlap path; runAll is also a generic trigger utility.
    if(!c.callerFunction||!c.callerLocals||
       !R::NameEquals(R::NameOf(c.callerFunction),L"ExecuteUbergraph_trigger_box"))return SG::Verdict::Run;
    void* owner=R::OuterOf(c.callerFunction);
    if(!owner||!R::NameEquals(R::NameOf(owner),L"trigger_box_C"))return SG::Verdict::Run;
    const int off=R::FindPropertyOffset(c.callerFunction,L"K2Node_ComponentBoundEvent_OtherActor");
    const int size=R::FunctionFrameSize(c.callerFunction);
    if(off<0||off>size||sizeof(void*)>size-off)return SG::Verdict::Run;
    void* entrant=nullptr;std::memcpy(&entrant,c.callerLocals+off,sizeof(entrant));
    const auto slot=players::Registry::Get().PeerIdOfActor(entrant);
    if(slot>=net::kMaxPeers)return SG::Verdict::Run;
    for(size_t i=0;i<rows.size();++i) {
        Bind(i);auto& row=rows[i];
        if(!row.box.Is(c.object)||!L::Armed(c.object))continue;
        if(row.fired)return SG::Verdict::Cancel; // duplicate overlap
        Entrant g{};g.slot=slot;g.pending=Pose(slot,g.pos,g.yaw);
        if(!g.pending||stamp!=epoch||!Current()||!Host()||!row.box.Is(c.object))return SG::Verdict::Run;
        row.fired=true; // native callbacks can re-enter this overlap
        break;
    }
    return SG::Verdict::Run;
}
void OnAgravOverlap(void*,void* source,void*) {
    if(!Current()||!Host()||!source)return;
    const auto stamp=epoch;
    ue_wrap::CachedObjRef ref;ref.Set(source);
    const auto caller=ue_wrap::ufunction_hook::CurrentCallerFrame();
    L::Box box{};if(!L::ReadAgravQuery(caller.function,caller.locals,box))return;
    if(source!=L::Controller(L"event_agrav"))return;
    if(!agrav.Is(source)){agrav.Set(source);agravFired=false;}
    if(agravFired)return;
    Entrant found{};
    if(!FindInside(box,players::Registry::Get().LocalPeerId(),found))return;
    if(stamp!=epoch||!Current()||!Host()||!ref.Is(source)||!agrav.Is(source))return;
    // Puppets deliberately have collision disabled. The native Pawn overlap may
    // miss a client standing inside the box: include its registered world pose.
    // This graph only branches on ReturnValue; it never reads OutActors afterward.
    auto* result=static_cast<bool*>(ue_wrap::ufunction_hook::CurrentResult());
    if(!result)return;
    *result=true;agravFired=true;
    // The host runs the sequence without relocating other participants.
}
}
bool IsLocationRow(const char* name) {
    if(!name)return false;if(!std::strcmp(name,"agrav"))return true;
    for(auto& rule:rules)if(!std::strcmp(name,rule.name))return L::TriggerBox(L::Controller(rule.field))!=nullptr;
    return false;
}
void Tick(net::Session& s) {
    if(ticking)return;
    ticking=true;
    struct TickScope{~TickScope(){ticking=false;}}tickScope;
    const auto generation=ue_wrap::world_identity::Generation();
    const auto serial=net::session_serial::Current();
    if(session!=&s||world!=generation||sessionId!=serial){OnDisconnect();session=&s;world=generation;sessionId=serial;}
    if(!Current())return;
    const auto stamp=epoch;
    SG::WatchClassName(L"triggerBase_C",L"runAll",kTag,&OnRunBox,nullptr);
    SG::WatchClassName(L"trigger_TBoxActivator_C",L"runTrigger",kTag,&OnArm,nullptr);
    SG::WatchClassName(L"trigger_agrav_C",L"runTrigger",kTag,&OnArm,nullptr);
    SG::ResolvePendingNames();
    const auto now=active_drive::NowMs();
    if(!hooked && now>=nextHook) {
        nextHook=now+2000;
        auto fn=ue_wrap::facts::Resolve(kRowBoxOverlapActors);
        if(fn)hooked=ue_wrap::ufunction_hook::InstallPostHook(fn,&OnAgravOverlap);
    }
    teleport_client::TickGather();
    if(epoch!=stamp||!Current()||!Host()||!s.AnyWorldReadyPeer())return;
    if(now<nextPoll)return;nextPoll=now+200;
    // Bounded to ten explicitly associated event boxes and the ready peers. No
    // GUObjectArray walk, no synthetic overlap for personal scares/general zones.
    for(size_t i=0;i<rows.size();++i) {
        Bind(i);auto& row=rows[i];void* box=row.box.Get();
        if(!box)continue;
        if(!L::Armed(box)){row.fired=false;continue;}
        if(row.fired)continue;
        L::Box bounds{};if(!L::ReadBox(box,bounds))continue;
        if(epoch!=stamp||!Current()||!Host()||!row.box.Is(box))return;
        Entrant found{};if(!FindInside(bounds,players::Registry::Get().LocalPeerId(),found))continue;
        if(epoch!=stamp||!Current()||!Host()||!row.box.Is(box))return;
        row.fired=true;
        if(epoch!=stamp||!Current()||!row.box.Is(box))return;
        if(!L::RunBox(box)&&epoch==stamp&&row.box.Is(box))row.fired=false;
        if(epoch!=stamp||!Current())return;
    }
}
void OnDisconnect(){++epoch;session=nullptr;world=sessionId=0;nextPoll=0;rows={};agrav.Reset();agravFired=false;teleport_client::ClearGather();}
}
