// coop/creatures/kerfur_command.cpp -- see coop/creatures/kerfur_command.h for the design + RE
// ground truth.

#include "coop/creatures/kerfur_command.h"

#include "coop/creatures/npc_sync.h"          // GetNpcIdForActor; NoteKerfurHolding so a take is not a throw
#include "coop/creatures/served_player.h"
#include "coop/element/mirror_manager.h"
#include "coop/element/npc.h"
#include "coop/element/registry.h"
#include "coop/net/intent_queue.h"
#include "coop/net/refusal_tally.h"
#include "coop/net/net_clock.h"
#include "coop/net/protocol.h"
#include "coop/net/wire_key_util.h"
#include "coop/props/prop_drive_host.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_wire_parity.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include <unordered_map>
#include <memory>
#include <optional>
#include <cstring>
#include <tuple>
#include <cmath>
#include "coop/player/hand_item.h"
#include "coop/player/local_streams.h"
#include "coop/player/players_registry.h"  // Local() / kPeerIdHost
#include "coop/player/remote_player.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/remote_prop.h"
#include "ue_wrap/actors/inventory.h"
#include "ue_wrap/actors/kerfur.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflected_offset.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/engine/engine_attach.h"
#include "ue_wrap/engine/engine_mainplayer.h"
#include "ue_wrap/engine/engine_pawn.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include <deque>

namespace coop::kerfur_command {
namespace {

namespace R  = ue_wrap::reflection;
namespace K  = ue_wrap::kerfur;
namespace P  = ue_wrap::profile;
namespace SP = coop::served_player;
namespace sg = ue_wrap::script_gate;

using coop::element::ElementId;
using coop::element::kInvalidId;

UE_FACT_FN(kRowStartKill, "kerfur_command", L"kerfurOmega_C", L"startKill", Declared);
constexpr int kTagStartKill = 0x4B434B01;  // 'KCK' 1

std::atomic<coop::net::Session*> g_session{nullptr};
coop::net::Session* LoadSession() { return g_session.load(std::memory_order_acquire); }

// The startKill watch: an Omega serves a client only while it is live, since murder mode's chase would
// otherwise read that client; refused or dead, it is said once and every command runs for the host.
bool g_killWatched = false;
bool g_killLive = false;
bool g_killRefused = false;
bool g_saidUnserved = false;
ue_wrap::CachedObjRef g_killClass,g_killFn;

// Its follow, patrol, hand-back and turn read GetPlayerPawn, its disc insert getMainPlayer.
constexpr uint8_t kOmegaSeams = SP::kPlayerPawn | SP::kMainPlayer;

// A sender's requests run in arrival order from a bounded queue at a bounded rate. The head waits, with its
// token given back, while the host cannot yet run it for the sender: no posed body for it (the seconds between a
// joiner's curtain and its first pose, a mid-join window rather than a verdict), or the Omega's seams and its
// startKill watch still settling -- since the verb's own reads would take the host. A request past the cap, or
// one that can never run (an unknown sender or command, an eid outside the host range), is refused into the
// tally. Per-slot state: a leaver's queue goes with its occupant. Game thread.
constexpr coop::net::IntentBudget kBudget{4.0f, 2.0f};
constexpr size_t kMaxPending = 8;

struct QueuedCmd {coop::net::KerfurCommandPayload packet{};coop::element::Registry::IdentitySnapshot identity;ue_wrap::CachedObjRef actor,cls;};
struct Slot {
    coop::net::IntentQueue<QueuedCmd> queue;
    uint32_t generation=0;uint64_t ready=0,epoch=0;
    bool waitSaid = false;  // the head's wait, said once a streak
};
coop::roster_ledger::PerSlotState<Slot> g_slots;
coop::net::RefusalTally g_tally{"kerfur_command"};

// Instruments (the kerfur_flood drill reads them against a baseline), per sender.
coop::roster_ledger::PerSlotState<uint32_t> g_runs;
coop::roster_ledger::PerSlotState<uint32_t> g_refused;

// Re-entrancy guard: the host executes a verb by re-invoking actionName via ProcessEvent (HostRunVerb),
// which re-enters the actionName interceptor. The thread-local flag makes TryRecordMenuCommand pass that
// re-entry through instead of recording it (else infinite loop). Game thread; the PE dispatch is
// synchronous on the same thread, so thread_local suffices.
thread_local bool t_inHostExec = false;

// ---- recorded actions (interceptor -> Tick) ----------------------------------------------------
struct PendingCmd {
    void*   actor = nullptr;
    int32_t internalIdx = -1;
    Command command = Command::Invalid;
    bool    isClient = false;
    uint32_t serial=0,world=0,actorSerial=0;uint64_t epoch=0,stops=0;
    void* worldObject=nullptr;coop::net::Session* session=nullptr;
};
std::mutex g_qMutex;
std::vector<PendingCmd> g_queue;

std::atomic<uint64_t> g_execEpoch{1};uint64_t g_commandAttempt=0,g_stageAttempt=0;bool g_pumping=false,g_registeringKill=false;
struct Scope {
    coop::net::Session* session=nullptr;uint32_t serial=0,world=0;void* object=nullptr;uint64_t stops=0,epoch=0;coop::net::Role role=coop::net::Role::Host;bool linked=false;
    bool Current()const{return session&&session==LoadSession()&&epoch==g_execEpoch.load()&&serial==coop::net::session_serial::Current()&&
        world==ue_wrap::world_identity::Generation()&&object&&object==ue_wrap::world_identity::CurrentWorld()&&session->StopCount()==stops&&
        session->running()&&session->role()==role&&session->connected()==linked;}
};
std::optional<Scope> g_boundExecution;
struct Object {ue_wrap::CachedObjRef actor,cls;
    bool Capture(void* value){actor.Set(value);if(!actor.Get())return false;cls.Set(R::ClassOf(value));return Current();}
    bool Current()const{if(!actor.Get()||!cls.Get())return false;void* type=R::ClassOf(actor.Raw());return actor.Get()&&cls.Get()&&type==cls.Raw();}
};
struct TakenNotice;
struct Execution {
    Scope scope;Object robot;coop::element::Registry::IdentitySnapshot identity;uint8_t slot=0;uint32_t generation=0;uint64_t ready=0,attempt=0;
    SP::Assignment before;SP::Lease service;bool captured=false,dispatched=false,assigning=false;
    std::function<bool()> caller;
    std::shared_ptr<TakenNotice> notice;
};
std::unordered_map<void*,std::shared_ptr<Execution>> g_executions;
bool Sender(const Execution& e){return !e.slot||(e.slot<coop::players::kMaxPeers&&e.scope.linked&&e.generation&&e.scope.session->IsSlotConnected(e.slot)&&
    e.scope.session->peerGenerationForSlot(e.slot)==e.generation&&e.scope.session->SlotWorldReadyRevision(e.slot)==e.ready&&e.scope.session->IsSlotWorldReady(e.slot));}
bool Base(const Execution& e){
    if(!e.scope.Current()||!Sender(e)||(e.caller&&!e.caller())||!e.robot.Current())return false;
    coop::element::Registry::IdentitySnapshot now;
    return coop::element::Registry::IsAllowedHostAllocatedEid(e.identity.id)&&coop::element::Registry::Get().ReadIdentity(e.identity.id,now)&&now.incarnation==e.identity.incarnation&&
        now.type==coop::element::ElementType::Npc&&!now.mirror&&!now.deleting&&now.typeName==e.identity.typeName&&
        now.actor==e.robot.actor.Raw()&&now.internalIdx==e.robot.actor.Idx()&&coop::element::Registry::Get().EidForActor(now.actor)==e.identity.id&&e.scope.Current()&&Sender(e);
}
bool Current(const std::shared_ptr<Execution>& e){
    bool own=false;{auto it=g_executions.find(e->robot.actor.Raw());own=it!=g_executions.end()&&it->second==e;}
    if(!own||!Base(*e))return false;
    if(!e->assigning){if(e->service){if(!SP::Owns(e->service))return false;}else if(e->captured&&!SP::AssignmentCurrent(e->before))return false;}
    {auto it=g_executions.find(e->robot.actor.Raw());return it!=g_executions.end()&&it->second==e&&e->scope.Current()&&Sender(*e);}
}
void ClearStaging();
void ClearTakenWork();
void RetireExecutionContext(){
    ++g_execEpoch;g_boundExecution.reset();g_executions.clear();ClearStaging();ClearTakenWork();g_killLive=g_killWatched=g_killRefused=false;g_killClass.Reset();g_killFn.Reset();
    {std::lock_guard<std::mutex> lock(g_qMutex);g_queue.clear();}
    for(int slot=0;slot<coop::players::kMaxPeers;++slot){g_slots[slot]=Slot{};g_runs[slot]=g_refused[slot]=0;}
    g_tally.Clear();
}
void BindExecution(){
    if(g_boundExecution&&!g_boundExecution->Current())RetireExecutionContext();auto* s=LoadSession();void* world=ue_wrap::world_identity::CurrentWorld();
    if(!g_boundExecution&&s&&s->running()&&world)g_boundExecution=Scope{s,coop::net::session_serial::Current(),ue_wrap::world_identity::Generation(),world,s->StopCount(),g_execEpoch.load(),s->role(),s->connected()};
}

const wchar_t* VerbName(Command c) {
    switch (c) {
        case Command::Follow:          return L"follow";
        case Command::Idle:            return L"idle";
        case Command::Patrol:          return L"patrol";
        case Command::FixServers:      return L"fix_servers";
        case Command::GetReports:      return L"get_reports";
        case Command::FixTransformers: return L"fix_transformers";
        case Command::TakeObject:      return L"take_object";
        case Command::Pat:             return L"pat";
        case Command::Equipment:       return L"equipment";
        default:                       return nullptr;
    }
}

// Resolve the WIRE eid (the host's id) off the client's adopted/mirror kerfur Element.
ElementId FindWireEidForNpcActor(void* actor){
    const auto id=coop::element::Registry::Get().EidForActor(actor);coop::element::Registry::IdentitySnapshot row;
    return coop::element::Registry::IsAllowedHostAllocatedEid(id)&&coop::element::Registry::Get().ReadIdentity(id,row)&&row.type==coop::element::ElementType::Npc&&row.mirror&&!row.deleting? id:kInvalidId;
}

bool HostRunVerb(void* actor,void* player,const wchar_t* name,const std::function<bool()>& current,bool* dispatched){
    const bool previous=t_inHostExec;t_inHostExec=true;
    struct Restore{bool previous;~Restore(){t_inHostExec=previous;}} restore{previous};
    return K::RunActionNameWhile(actor,player,name,current,dispatched);
}

bool HasBody(uint8_t slot) {
    coop::RemotePlayer* rp = coop::players::Registry::Get().Puppet(slot);
    return rp && rp->valid() && rp->HasPose();
}

// Settling, not refused: a refusal is final, and its commands run for the host instead of waiting.
bool Settling() {
    return (!g_killLive && !g_killRefused) || (!SP::SeamsReady(kOmegaSeams) && !SP::SeamsRefused(kOmegaSeams));
}

bool CanRunFor(uint8_t slot) { return HasBody(slot) && !Settling(); }

// One refusal into the tally, and into the sender's count when the slot is a client's (1 and up).
void Refuse(uint8_t slot, const char* why) {
    if (slot >= 1 && slot < coop::players::kMaxPeers) ++g_refused[slot];
    g_tally.Note(slot, why, coop::net::RefusalTally::Clock::now());
}

// The game's own guard on every relayed verb (bytecode @20950-@22108): a kerfur on a job -- fixing
// servers, getting reports, fixing transformers, states 3 to 5 -- refuses the command. Mirrored, as the kill
// guard is, so that a refused command hands the kerfur to nobody.
bool OnAJob(void* actor) {
    uint8_t state = 0, face = 0;
    bool spooky = false;
    return K::ReadKerfurState(actor, state, spooky, face) && state >= 3 && state <= 5;
}

void* PlayerBody(uint8_t slot);

// kerfurOmega_C::viewer, the object equipment assigns. Null when the field is absent or empty.
void* KerfurViewer(void* actor,const std::function<bool()>& current){
    void* cls=R::ClassOf(actor);if(!current())return nullptr;const auto size=R::StructSize(cls);if(!current())return nullptr;
    int32_t off=-1;std::wstring type;if(!R::PropertyOffsetAndClass(cls,L"viewer",off,type)||!current()||type!=L"ObjectProperty"||off<0||off>size||sizeof(void*)>size-off)return nullptr;
    void* value=nullptr;std::memcpy(&value,static_cast<uint8_t*>(actor)+off,sizeof value);return current()&&value&&R::IsLive(value)?value:nullptr;
}
// Temporary puppet fields are restored only while their staging receipt still owns them.
struct StagedHand {
    Scope scope;uint8_t slot=0;uint32_t generation=0;uint64_t ready=0,attempt=0;Object pawn;coop::RemotePlayer* puppet=nullptr;
    int32_t holdOff=-1,grabOff=-1,nameOff=-1;void* prevHold=nullptr;void* prevGrab=nullptr;R::FName prevName{},stagedName{};
    ue_wrap::CachedObjRef previousHold,previousGrab,held,grabbed,mirror,heldClass,grabbedClass;
    void* stagedHold=nullptr;void* stagedGrab=nullptr;bool wroteHold=false,wroteGrab=false,wroteName=false,wrote=false;char cls[64]{};
};
std::unordered_map<void*,std::shared_ptr<StagedHand>> g_stages;
void ClearStaging(){g_stages.clear();}
void WritePtr(void* pawn,int32_t off,void* value){if(pawn&&off>=0)std::memcpy(static_cast<uint8_t*>(pawn)+off,&value,sizeof value);}
void* ReadPtr(void* pawn,int32_t off){void* value=nullptr;if(pawn&&off>=0)std::memcpy(&value,static_cast<uint8_t*>(pawn)+off,sizeof value);return value;}
bool HandCurrent(const std::shared_ptr<StagedHand>& hand){
    bool own=false;{auto it=g_stages.find(hand->pawn.actor.Raw());own=it!=g_stages.end()&&it->second==hand;}
    if(!own||!hand->scope.Current()||!hand->pawn.Current())return false;
    auto* s=hand->scope.session;if(!hand->generation||s->peerGenerationForSlot(hand->slot)!=hand->generation||
        !s->IsSlotConnected(hand->slot)||!s->IsSlotWorldReady(hand->slot)||s->SlotWorldReadyRevision(hand->slot)!=hand->ready)return false;
    auto* puppet=coop::players::Registry::Get().Puppet(hand->slot);
    if(puppet!=hand->puppet||!hand->scope.Current())return false;
    bool posed=puppet&&puppet->valid()&&puppet->HasPose();if(!posed||!hand->scope.Current())return false;
    void* actor=puppet->GetActor();
    const bool valid=actor==hand->pawn.actor.Raw()&&hand->pawn.Current()&&hand->scope.Current()&&s->peerGenerationForSlot(hand->slot)==hand->generation&&
        s->SlotWorldReadyRevision(hand->slot)==hand->ready&&s->IsSlotWorldReady(hand->slot);
    {auto it=g_stages.find(hand->pawn.actor.Raw());return valid&&it!=g_stages.end()&&it->second==hand;}
}
bool HandLayout(const std::shared_ptr<StagedHand>& h,const std::function<bool()>& current){
    const auto cls=h->pawn.cls.Raw();const auto size=R::StructSize(cls);if(!current()||size<=0)return false;
    for(auto [name,offset,width,type]:{std::tuple{L"holding_actor",&h->holdOff,int32_t(sizeof(void*)),L"ObjectProperty"},
        std::tuple{L"grabbing_actor",&h->grabOff,int32_t(sizeof(void*)),L"ObjectProperty"},std::tuple{L"holding_name",&h->nameOff,int32_t(sizeof(R::FName)),L"NameProperty"}}){
        std::wstring found;if(!R::PropertyOffsetAndClass(cls,name,*offset,found)||!current()||found!=type||*offset<0||*offset>size||width>size-*offset)return false;
        const auto fields=R::EnumerateStructFields(cls);if(!current())return false;bool scalar=false;
        for(const auto& field:fields)if(_wcsicmp(field.name.c_str(),name)==0){scalar=field.offset==*offset&&field.size==width;break;}
        if(!scalar)return false;
    }
    const auto overlaps=[](int32_t a,int32_t aw,int32_t b,int32_t bw){return a<b+bw&&b<a+aw;};
    return !overlaps(h->holdOff,sizeof(void*),h->grabOff,sizeof(void*))&&!overlaps(h->holdOff,sizeof(void*),h->nameOff,sizeof(R::FName))&&!overlaps(h->grabOff,sizeof(void*),h->nameOff,sizeof(R::FName));
}
void RememberClass(StagedHand& hand,void* actor){
    const auto name=R::ClassNameOf(actor);if(name.empty()||name.size()>=sizeof hand.cls)return;
    for(wchar_t c:name)if(c<=0||c>=128)return;for(size_t i=0;i<name.size();++i)hand.cls[i]=char(name[i]);
}
std::shared_ptr<StagedHand> StageHand(const std::shared_ptr<Execution>& e,bool preferWorld){
    if(!e->slot)return {};auto hand=std::make_shared<StagedHand>();hand->scope=e->scope;hand->slot=e->slot;hand->generation=e->generation;hand->ready=e->ready;hand->attempt=++g_stageAttempt;
    hand->puppet=coop::players::Registry::Get().Puppet(e->slot);if(!Current(e)||!hand->puppet)return {};
    if(!hand->puppet->valid()||!Current(e)||!hand->puppet->HasPose()||!Current(e))return {};
    void* pawn=hand->puppet->GetActor();if(!Current(e)||!pawn||!hand->pawn.Capture(pawn)||!Current(e))return {};
    g_stages[pawn]=hand;const auto current=[&]{return Current(e)&&HandCurrent(hand);};
    if(!HandLayout(hand,current)||!current())return hand;
    hand->prevHold=ReadPtr(pawn,hand->holdOff);hand->prevGrab=ReadPtr(pawn,hand->grabOff);std::memcpy(&hand->prevName,static_cast<uint8_t*>(pawn)+hand->nameOff,sizeof hand->prevName);
    if(hand->prevHold&&R::IsLive(hand->prevHold))hand->previousHold.Set(hand->prevHold);
    if(hand->prevGrab&&R::IsLive(hand->prevGrab))hand->previousGrab.Set(hand->prevGrab);if(!current())return hand;
    void* mirror=coop::hand_item::MirrorActorForSlot(e->slot);if(!current())return hand;
    void* world=coop::remote_prop::DrivenActor(e->slot);if(!current())return hand;
    void* local=coop::local_streams::LastHeldActor();if(!current())return hand;if(world==local)world=nullptr;
    Object selected,worldRef,mirrorRef;bool haveWorld=world&&worldRef.Capture(world);if(!current())return hand;
    const bool haveMirror=mirror&&mirrorRef.Capture(mirror);if(!current())return hand;
    void* chosen=preferWorld&&haveWorld?world:haveMirror?mirror:haveWorld?world:nullptr;
    if(chosen&&!selected.Capture(chosen))return hand;if(!current())return hand;
    if(chosen){
        RememberClass(*hand,chosen);if(!current()||!selected.Current())return hand;
        hand->held=selected.actor;hand->heldClass=selected.cls;hand->stagedHold=chosen;WritePtr(pawn,hand->holdOff,chosen);hand->wroteHold=true;
        if(chosen==mirror){hand->mirror=selected.actor;const auto item=coop::hand_item::HeldItem(e->slot);if(!current()||!selected.Current())return hand;
            if(!item.empty()){const auto name=ue_wrap::fname_utils::StringToFName(item);if(!current()||!selected.Current())return hand;hand->stagedName=name;std::memcpy(static_cast<uint8_t*>(pawn)+hand->nameOff,&name,sizeof name);hand->wroteName=true;}}
    }
    if(haveWorld&&world!=mirror){if(!current()||!worldRef.Current())return hand;hand->grabbed=worldRef.actor;hand->grabbedClass=worldRef.cls;hand->stagedGrab=world;WritePtr(pawn,hand->grabOff,world);hand->wroteGrab=true;}
    if(current())hand->wrote=true;return hand;
}
void RestoreHand(const std::shared_ptr<StagedHand>& hand){
    if(!hand)return;const auto forget=[&]{auto it=g_stages.find(hand->pawn.actor.Raw());if(it!=g_stages.end()&&it->second==hand)g_stages.erase(it);};
    const auto current=[&]{return HandCurrent(hand);};if(!current()){forget();return;}
    void* pawn=hand->pawn.actor.Raw();
    if(hand->wroteHold&&ReadPtr(pawn,hand->holdOff)==hand->stagedHold)WritePtr(pawn,hand->holdOff,hand->previousHold.Get());
    if(!current()){forget();return;}if(hand->wroteGrab&&ReadPtr(pawn,hand->grabOff)==hand->stagedGrab)WritePtr(pawn,hand->grabOff,hand->previousGrab.Get());
    if(!current()){forget();return;}
    if(hand->wroteName){R::FName name{};std::memcpy(&name,static_cast<uint8_t*>(pawn)+hand->nameOff,sizeof name);
        if(std::memcmp(&name,&hand->stagedName,sizeof name)==0)std::memcpy(static_cast<uint8_t*>(pawn)+hand->nameOff,&hand->prevName,sizeof name);}
    forget();
}
// The host telling every peer that the robot took `actor`. The holder's own let-go may still
// be in flight with a zero pad; receivers that have seen this one leave physics off either way.
// Velocities stay zero: this is the take, not a throw. holdGen 0 does not close the host's own
// hold counter -- the receiver ends whichever drive still has this prop.
void PublishKerfurTake(const std::shared_ptr<Execution>& e,const Object& held){
    const auto current=[&]{return Current(e)&&held.Current();};if(!current())return;
    coop::net::PropReleasePayload rel{};void* actor=held.actor.Raw();const auto key=ue_wrap::prop::GetInteractableKeyString(actor);if(!current())return;
    if(!key.empty()&&key!=L"None")coop::net::WireKeyFromString(key,rel.key);
    const auto eid=coop::prop_element_tracker::GetPropElementIdForActor(actor);if(!current())return;
    if(eid!=coop::element::kInvalidId)rel.elementId=eid;if(!rel.key.len&&!rel.elementId)return;
    rel.physFlags=coop::prop_wire_parity::PhysFlagsOf(actor);if(!current())return;
    ue_wrap::FVector loc{};ue_wrap::FRotator rot{};
    const bool haveLoc=ue_wrap::engine::TryGetActorLocation(actor,loc);if(!current())return;
    const bool haveRot=ue_wrap::engine::TryGetActorRotation(actor,rot);if(!current())return;
    if(haveLoc&&haveRot&&std::isfinite(loc.X)&&std::isfinite(loc.Y)&&std::isfinite(loc.Z)&&std::isfinite(rot.Pitch)&&std::isfinite(rot.Yaw)&&std::isfinite(rot.Roll)){
        rel.locX=loc.X;rel.locY=loc.Y;rel.locZ=loc.Z;rel.rotPitch=rot.Pitch;rel.rotYaw=rot.Yaw;rel.rotRoll=rot.Roll;rel.hasPose=1;
    }
    rel._pad=1;if(current())e->scope.session->SendPropRelease(rel);
}
struct TakenNotice {
    std::shared_ptr<StagedHand> hand;coop::net::HandTakenPayload packet{};bool committed=false;
};
constexpr size_t kMaxTakenNotices=64;
std::deque<std::shared_ptr<TakenNotice>> g_notices[coop::players::kMaxPeers];bool g_sendingTaken=false;
void ForgetNotice(const std::shared_ptr<TakenNotice>& notice){
    if(!notice)return;auto& queue=g_notices[notice->hand->slot];
    for(auto it=queue.begin();it!=queue.end();++it)if(*it==notice){queue.erase(it);return;}
}
bool NoticeCurrent(const std::shared_ptr<TakenNotice>& notice){
    auto h=notice->hand;const auto owned=[&]{for(const auto& p:g_notices[h->slot])if(p==notice)return true;return false;};
    if(!owned()||!h->scope.Current()||!h->pawn.Current())return false;
    auto* s=h->scope.session;const auto recipient=[&]{return h->generation&&s->peerGenerationForSlot(h->slot)==h->generation&&
        s->IsSlotConnected(h->slot)&&s->IsSlotWorldReady(h->slot)&&s->SlotWorldReadyRevision(h->slot)==h->ready;};
    if(!recipient())return false;auto* puppet=coop::players::Registry::Get().Puppet(h->slot);
    if(!h->scope.Current()||puppet!=h->puppet||!puppet)return false;
    if(!puppet->valid()||!h->scope.Current()||!puppet->HasPose()||!h->scope.Current())return false;
    void* body=puppet->GetActor();return body==h->pawn.actor.Raw()&&h->pawn.Current()&&h->scope.Current()&&recipient()&&owned();
}
void PumpTakenNotices(){
    if(g_sendingTaken)return;g_sendingTaken=true;struct Busy{~Busy(){g_sendingTaken=false;}} busy;
    for(int slot=1;slot<coop::players::kMaxPeers;++slot){
        size_t budget=kMaxTakenNotices;
        while(budget--&&!g_notices[slot].empty()){
            auto notice=g_notices[slot].front();if(!NoticeCurrent(notice)){ForgetNotice(notice);continue;}
            if(!notice->committed)break;
            const auto packet=notice->packet;auto* s=notice->hand->scope.session;
            const bool sent=s->SendReliableToSlot(slot,coop::net::ReliableKind::HandTaken,&packet,sizeof packet);
            if(!NoticeCurrent(notice)){ForgetNotice(notice);continue;}
            if(!sent)break;ForgetNotice(notice);
        }
    }
}
bool ReserveTakenNotice(const std::shared_ptr<Execution>& e,const std::shared_ptr<StagedHand>& hand){
    if(!Current(e)||!HandCurrent(hand)||!hand->cls[0])return false;
    auto& queue=g_notices[e->slot];if(queue.size()>=kMaxTakenNotices)return false;
    auto notice=std::make_shared<TakenNotice>();notice->hand=hand;
    for(size_t i=0;i<sizeof(notice->packet.cls.data)&&hand->cls[i];++i){notice->packet.cls.data[i]=hand->cls[i];notice->packet.cls.len=uint8_t(i+1);}
    queue.push_back(notice);e->notice=notice;return true;
}
void NotifyHandTaken(const std::shared_ptr<Execution>& e,const std::shared_ptr<StagedHand>& hand){
    if(!e->notice||!Current(e)||!HandCurrent(hand)||!NoticeCurrent(e->notice))return;
    e->notice->committed=true;PumpTakenNotices();
}
// The body the verb's Player argument should name. The host is its own pawn. A client is the puppet
// the host already has for that slot: take_object and equipment read that argument, not player 0.
void* PlayerBody(uint8_t slot) {
    if (slot == coop::players::kPeerIdHost) return coop::players::Registry::Get().Local();
    coop::RemotePlayer* rp = coop::players::Registry::Get().Puppet(slot);
    return (rp && rp->valid()) ? rp->actor() : nullptr;
}

// Apply a command to the host's real kerfur (eid already resolved + validated): the game's own verb, with
// the kerfur serving its requester -- its player-0 reads answered with that player's puppet by
// served_player, the host's own command handing it back to player 0. take_object, pat and equipment
// are also handed that body as Player.
bool KillCurrent();
void ExecuteHostCommand(ElementId eid,void* actor,Command command,uint8_t slot,const std::function<bool()>& caller={}){
    BindExecution();if(!g_boundExecution||g_boundExecution->role!=coop::net::Role::Host||!g_boundExecution->Current()||!VerbName(command)||slot>=coop::players::kMaxPeers)return;
    if(g_executions.contains(actor))return;
    auto e=std::make_shared<Execution>();e->scope=*g_boundExecution;e->slot=slot;e->attempt=++g_commandAttempt;e->caller=caller;
    if(!coop::element::Registry::Get().ReadIdentity(eid,e->identity)||e->identity.actor!=actor||!e->robot.Capture(actor)||!e->scope.Current())return;
    if(slot){e->generation=e->scope.session->peerGenerationForSlot(slot);e->ready=e->scope.session->SlotWorldReadyRevision(slot);}
    if(!Base(*e))return;g_executions[actor]=e;
    std::shared_ptr<StagedHand> staged;
    struct Finish {
        std::shared_ptr<Execution> e;std::shared_ptr<StagedHand>& staged;
        ~Finish(){if(e->notice&&!e->notice->committed)ForgetNotice(e->notice);RestoreHand(staged);if(!e->dispatched&&e->service)SP::RestoreOwned(e->service,e->before,[&]{return Base(*e);});
            auto it=g_executions.find(e->robot.actor.Raw());if(it!=g_executions.end()&&it->second==e)g_executions.erase(it);}
    } finish{e,staged};
    const auto current=[&]{return Current(e);};
    e->before=SP::CaptureAssignment(actor);if(!e->before||!current())return;e->captured=true;if(!current())return;
    if(command!=Command::Equipment){uint8_t state=0,face=0;bool spooky=false;
        if(!K::ReadKerfurState(actor,state,spooky,face)||!current()||(state>=3&&state<=5))return;}
    const uint8_t serves=slot&&KillCurrent()?slot:0;if(!current())return;
    if(slot&&!serves&&(command==Command::GetReports||command==Command::TakeObject||command==Command::Equipment))return;
    e->assigning=true;e->service=SP::AssignOwned(e->before,serves,kOmegaSeams,current);e->assigning=false;if(!e->service||!current())return;
    void* player=coop::players::Registry::Get().Local();if(!current())return;
    if(slot&&(command==Command::TakeObject||command==Command::Pat||command==Command::Equipment)){
        auto* puppet=coop::players::Registry::Get().Puppet(slot);if(!current()||!puppet)return;
        if(!puppet->valid()||!current()||!puppet->HasPose()||!current())return;
        player=puppet->GetActor();if(!current())return;
    }
    Object playerRef;if(player&&(!playerRef.Capture(player)||!current()))return;
    bool hadFloppy=false;if(command==Command::GetReports&&(!K::ReadHasFloppy(actor,hadFloppy)||!current()))return;
    void* heldBefore=nullptr;if(command==Command::TakeObject){heldBefore=K::ReadHoldObject(actor);if(!current())return;}
    void* viewerBefore=nullptr;if(command==Command::Equipment){viewerBefore=KerfurViewer(actor,current);if(!current())return;}
    const bool hand=command==Command::TakeObject||command==Command::Equipment||command==Command::GetReports;
    if(hand&&slot){staged=StageHand(e,command==Command::TakeObject);if(!current()||!staged||!staged->wrote||!ReserveTakenNotice(e,staged))return;}
    const auto sourceCurrent=[](const ue_wrap::CachedObjRef& actor,const ue_wrap::CachedObjRef& cls){return !actor.Raw()||(actor.Get()&&cls.Get()&&R::ClassOf(actor.Raw())==cls.Raw()&&actor.Get()&&cls.Get());};
    const auto nativeCurrent=[&]{return current()&&(!player||playerRef.Current())&&
        (e->dispatched||!staged||(sourceCurrent(staged->held,staged->heldClass)&&sourceCurrent(staged->grabbed,staged->grabbedClass)));};
    const bool ran=HostRunVerb(actor,player,VerbName(command),nativeCurrent,&e->dispatched);
    if(!current()||!ran)return;
    bool took=false;
    if(command==Command::TakeObject){void* heldNow=K::ReadHoldObject(actor);if(!current())return;
        if(heldNow&&heldNow!=heldBefore){Object held;if(!held.Capture(heldNow)||!current())return;
            const auto holdCurrent=[&]{return current()&&held.Current();};
            if(!ue_wrap::engine::SetActorSimulatePhysics(heldNow,false)||!holdCurrent())return;
            coop::npc_sync::NoteKerfurHolding(heldNow);if(!holdCurrent())return;
            if(staged&&staged->wrote&&serves==slot){coop::remote_prop::EndAnyHoldOn(heldNow);if(!holdCurrent())return;
                PublishKerfurTake(e,held);if(!holdCurrent())return;took=true;}
            (void)coop::prop_drive_host::ClaimOwned(heldNow,coop::prop_drive_host::ClaimSource::Kerfur,"kerfur-take",holdCurrent);if(!holdCurrent())return;
        }
    }
    if(staged&&staged->wrote&&serves==slot){
        if(staged->mirror.Raw()&&!staged->mirror.Get())took=true;
        if(command==Command::GetReports){bool has=false;if(!K::ReadHasFloppy(actor,has)||!current())return;if(has&&!hadFloppy)took=true;}
        if(command==Command::Equipment){void* viewer=KerfurViewer(actor,current);if(!current())return;if(viewer&&viewer!=viewerBefore)took=true;}
    }
    if(took)NotifyHandTaken(e,staged);
}
sg::Verdict OnStartKillPre(const sg::Call& call){
    BindExecution();if(!g_boundExecution||g_boundExecution->role!=coop::net::Role::Host||!g_boundExecution->Current())return sg::Verdict::Run;
    const auto scope=*g_boundExecution;Object actor;ue_wrap::CachedObjRef fn;
    if(!call.object||!call.function||!actor.Capture(call.object))return sg::Verdict::Cancel;fn.Set(call.function);
    const auto current=[&]{if(!scope.Current()||!actor.Current()||!fn.Get())return false;
        void* selected=R::FindDispatchFunctionCached(actor.cls.Raw(),L"startKill");return scope.Current()&&actor.Current()&&fn.Get()&&selected==fn.Raw();};
    auto before=SP::CaptureAssignment(call.object);if(!before||!current())return sg::Verdict::Cancel;
    auto host=SP::AssignOwned(before,0,0,current);return host&&current()?sg::Verdict::Run:sg::Verdict::Cancel;
}
// The watch settles at once where it can, so the first command runs with it live: the name resolve is a
// game-thread engine call.
bool KillCurrent(){
    if(!g_killLive||!g_killClass.Get()||!g_killFn.Get())return false;
    void* fn=R::FindFunction(g_killClass.Raw(),L"startKill");return g_killClass.Get()&&g_killFn.Get()&&fn==g_killFn.Raw();
}
void SettleKillWatch(){
    if(!g_boundExecution||!g_boundExecution->Current()||g_boundExecution->role!=coop::net::Role::Host||!g_killWatched||g_killRefused)return;
    if(KillCurrent())return;const auto scope=*g_boundExecution;
    sg::ResolvePendingNames();if(!scope.Current())return;
    const bool live=sg::NameWatchLive(kRowStartKill,kTagStartKill);if(!scope.Current())return;
    if(live){ue_wrap::CachedObjRef cls,fn;cls.Set(R::FindClass(L"kerfurOmega_C"));if(!scope.Current()||!cls.Get())return;
        fn.Set(R::FindFunction(cls.Raw(),L"startKill"));if(!scope.Current()||!cls.Get()||!fn.Get())return;
        g_killClass=cls;g_killFn=fn;g_killLive=true;
    }else {const bool settled=sg::NameWatchSettled(kRowStartKill,kTagStartKill);if(scope.Current()&&settled)g_killRefused=true;}
}
// A refusal at the serve, said through the tally. It is not in RefusedFor: the request entered RunRequest, so it is
// a run, and each request is a run or a refusal, never both, which is what the drill's accounting holds.
void RefuseAtServe(uint8_t slot, const char* why) {
    g_tally.Note(slot, why, coop::net::RefusalTally::Clock::now());
}

// A request whose sender has a body: validated against the kerfur as it is now, then run.
void RunRequest(const QueuedCmd& queued,uint8_t slot,const std::function<bool()>& sender){
    ++g_runs[slot];const auto& payload=queued.packet;
    const auto current=[&]{coop::element::Registry::IdentitySnapshot now;
        return sender()&&coop::element::Registry::Get().ReadIdentity(payload.elementId,now)&&now.incarnation==queued.identity.incarnation&&now.type==coop::element::ElementType::Npc&&
            !now.mirror&&!now.deleting&&queued.actor.Get()&&queued.cls.Get()&&now.actor==queued.actor.Raw()&&R::ClassOf(now.actor)==queued.cls.Raw()&&queued.actor.Get()&&queued.cls.Get()&&sender();};
    if(!current()){
        RefuseAtServe(slot,"no current kerfur identity");return;
    }
    const bool kind=K::IsKerfurActor(queued.actor.Raw());if(!current()||!kind)return;
    const bool kill=K::ReadKill(queued.actor.Raw());if(!current()||kill)return;
    ExecuteHostCommand(payload.elementId,queued.actor.Raw(),static_cast<Command>(payload.command),slot,current);
}
void ServeQueues(){
    if(!g_boundExecution||!g_boundExecution->Current())return;const auto scope=*g_boundExecution;
    for(uint8_t slot=1;slot<coop::players::kMaxPeers;++slot){
        if(!scope.Current())return;Slot& queue=g_slots[slot];const auto generation=queue.generation;const auto revision=queue.ready;const auto epoch=queue.epoch;
        const auto current=[&]{return scope.Current()&&epoch==g_execEpoch.load()&&generation&&scope.session->peerGenerationForSlot(slot)==generation&&
            scope.session->SlotWorldReadyRevision(slot)==revision&&scope.session->IsSlotConnected(slot)&&scope.session->IsSlotWorldReady(slot);};
        if(!current()){queue=Slot{};continue;}
        queue.queue.Serve(kBudget,coop::net::NowMs,[&](const QueuedCmd& request){
            if(!current())return coop::net::Served::Done;
            const bool canRun=CanRunFor(slot);if(!current())return coop::net::Served::Done;
            if(!canRun)return coop::net::Served::Wait;
            RunRequest(request,slot,current);return coop::net::Served::Done;
        });
    }
}


struct TakenCleanup {
    Scope scope;std::shared_ptr<StagedHand> layout;Object held,grabbed;
    void* expectedHold=nullptr;void* expectedGrab=nullptr;R::FName name{};
    bool holdMatch=false,grabMatch=false;unsigned phase=0;
};
std::shared_ptr<TakenCleanup> g_takenCleanup;bool g_cleaningTaken=false;
void ClearTakenWork(){for(auto& queue:g_notices)queue.clear();g_takenCleanup.reset();}
bool CleanupBody(const std::shared_ptr<TakenCleanup>& work){
    if(g_takenCleanup!=work||!work->scope.Current()||!work->layout->pawn.Current())return false;
    void* local=coop::players::Registry::Get().Local();
    return local==work->layout->pawn.actor.Raw()&&g_takenCleanup==work&&work->scope.Current()&&work->layout->pawn.Current();
}
bool CleanupFields(const std::shared_ptr<TakenCleanup>& work){
    if(!CleanupBody(work))return false;auto h=work->layout;void* body=h->pawn.actor.Raw();
    return ReadPtr(body,h->holdOff)==work->expectedHold&&ReadPtr(body,h->grabOff)==work->expectedGrab;
}
void ForgetCleanup(const std::shared_ptr<TakenCleanup>& work){if(g_takenCleanup==work)g_takenCleanup.reset();}
void PumpTakenCleanup(){
    if(g_cleaningTaken)return;auto work=g_takenCleanup;if(!work||!work->layout)return;
    g_cleaningTaken=true;struct Busy{~Busy(){g_cleaningTaken=false;}} busy;
    const auto current=[&]{return CleanupFields(work);};
    const auto fail=[&]{ForgetCleanup(work);};
    if(!current()){fail();return;}auto h=work->layout;void* body=h->pawn.actor.Raw();
    const bool distinct=work->holdMatch&&work->held.actor.Raw()!=work->grabbed.actor.Raw();
    if(work->phase==0){work->phase=1;
        if(work->grabMatch){if(!work->grabbed.Current()||!current()){fail();return;}
            coop::local_streams::ArmKerfurTakeRelease(work->grabbed.actor.Raw());if(!current()||!work->grabbed.Current()){fail();return;}}
    }
    if(work->phase==1){
        if(distinct){R::FName name{};std::memcpy(&name,static_cast<uint8_t*>(body)+h->nameOff,sizeof name);
            if(!work->held.Current()||std::memcmp(&name,&work->name,sizeof name)||!current()){fail();return;}
            const bool wrote=ue_wrap::inventory::WriteHeldItemWhile(L"None",nullptr,[&]{
                if(!current()||!work->held.Current())return false;R::FName now{};std::memcpy(&now,static_cast<uint8_t*>(body)+h->nameOff,sizeof now);
                return std::memcmp(&now,&work->name,sizeof now)==0;});
            if(!current()){fail();return;}if(!wrote)return;
        }else if(work->holdMatch){if(!work->held.Current()||!current()){fail();return;}
            WritePtr(body,h->holdOff,nullptr);work->expectedHold=nullptr;}
        work->phase=2;
    }
    if(work->phase==2){
        if(distinct){
            bool dispatched=false;
            const auto refreshCurrent=[&]{if(!CleanupBody(work))return false;void* hold=ReadPtr(body,h->holdOff);
                if(!dispatched){R::FName name{};std::memcpy(&name,static_cast<uint8_t*>(body)+h->nameOff,sizeof name);if(std::memcmp(&name,&work->name,sizeof name))return false;}
                return (hold==work->expectedHold||!hold)&&ReadPtr(body,h->grabOff)==work->expectedGrab;};
            ue_wrap::engine::CallMainPlayerUpdateHoldWhile(body,refreshCurrent,&dispatched);
            if(!refreshCurrent()){fail();return;}if(!dispatched)return;
            work->expectedHold=ReadPtr(body,h->holdOff);
            if(work->expectedHold){R::FName name{};std::memcpy(&name,static_cast<uint8_t*>(body)+h->nameOff,sizeof name);
                if(std::memcmp(&name,&work->name,sizeof name)){fail();return;}}
        }
        work->phase=3;
    }
    if(!current()){fail();return;}
    if(work->phase==3){work->phase=4;
        if(distinct&&work->held.Current()){
            // Clear a surviving pointer before destruction; never delete the shared grabbed prop.
            if(work->expectedHold==work->held.actor.Raw()){WritePtr(body,h->holdOff,nullptr);work->expectedHold=nullptr;}
            if(!current()||!work->held.Current()){fail();return;}
            ue_wrap::engine::DestroyActor(work->held.actor.Raw());if(!current()){fail();return;}
        }
    }
    if(work->phase==4){
        if(work->grabMatch){if(!current()||!work->grabbed.Current()){fail();return;}
            const auto releaseCurrent=[&]{return CleanupBody(work)&&ReadPtr(body,h->holdOff)==work->expectedHold&&
                (ReadPtr(body,h->grabOff)==work->expectedGrab||!ReadPtr(body,h->grabOff));};
            bool dispatched=false;
            ue_wrap::engine::ReleaseMainPlayerGrabIfHoldingWhile(body,work->grabbed.actor.Raw(),releaseCurrent,&dispatched);
            if(!releaseCurrent()){fail();return;}
            if(ReadPtr(body,h->grabOff)){if(!dispatched)return;fail();return;}work->expectedGrab=nullptr;
        }
        work->phase=5;
    }
    if(work->phase==5){work->phase=6;
        if(work->grabMatch){if(!current()||!work->grabbed.Current()){fail();return;}
            ue_wrap::engine::SetActorSimulatePhysics(work->grabbed.actor.Raw(),false);
        }
    }
    fail();
}
}  // namespace

Command CommandFromActionName(const std::wstring& name) {
    if (name == L"follow")           return Command::Follow;
    if (name == L"idle")             return Command::Idle;
    if (name == L"patrol")           return Command::Patrol;
    if (name == L"fix_servers")      return Command::FixServers;
    if (name == L"get_reports")      return Command::GetReports;
    if (name == L"fix_transformers") return Command::FixTransformers;
    if (name == L"take_object")      return Command::TakeObject;
    if (name == L"pat")              return Command::Pat;
    if (name == L"equipment")        return Command::Equipment;
    return Command::Invalid;  // turn_off (kerfur_convert)
}

bool TryRecordMenuCommand(void* self, const std::wstring& name, bool isClient) {
    if (t_inHostExec) return false;  // our own host actionName replay -- pass through
    if (CommandFromActionName(name) == Command::Invalid) return false;  // not a relayed verb
    auto* s = LoadSession();
    if (!self || !s || !s->running() || !s->connected() || isClient!=(s->role()==coop::net::Role::Client)) return false;  // SP / not connected -> leave it
    PendingCmd pc;
    pc.actor = self;
    pc.internalIdx = R::InternalIndexOf(self);
    pc.command = CommandFromActionName(name);
    pc.isClient = isClient;pc.session=s;pc.serial=coop::net::session_serial::Current();pc.world=ue_wrap::world_identity::Generation();pc.worldObject=ue_wrap::world_identity::CurrentWorld();pc.stops=s->StopCount();pc.epoch=g_execEpoch.load();pc.actorSerial=R::SlotSerial(pc.internalIdx);
    {
        std::lock_guard<std::mutex> lk(g_qMutex);
        // The radial confirm records the verb, then actionName's interceptor records it again
        // in the same press. One entry: the host would otherwise pat or take the object twice.
        if (!g_queue.empty()) {
            const PendingCmd& last = g_queue.back();
            if (last.actor == pc.actor && last.command == pc.command && last.isClient == pc.isClient)
                return true;
        }
        if(g_queue.size()>=256)return true;
        g_queue.push_back(pc);
    }
    return true;  // cancel the local dispatch on BOTH roles -- the host runs it authoritatively
}

void Install(coop::net::Session* session){
    g_session.store(session,std::memory_order_release);BindExecution();
    if(g_registeringKill||!g_boundExecution||g_boundExecution->role!=coop::net::Role::Host||!g_boundExecution->Current())return;
    const auto scope=*g_boundExecution;g_registeringKill=true;struct Busy{~Busy(){g_registeringKill=false;}} busy;
    if(g_killLive&&!KillCurrent()){g_killLive=g_killWatched=g_killRefused=false;}
    if(!g_killWatched&&!g_killRefused){const bool okay=sg::WatchName(kRowStartKill,kTagStartKill,&OnStartKillPre,nullptr);if(!scope.Current())return;g_killWatched=okay;g_killRefused=!okay;}
    SettleKillWatch();
}

void OnCommandRequest(const coop::net::KerfurCommandPayload& payload, uint8_t senderPeerSlot) {
    BindExecution();auto* s=LoadSession();
    if(!g_boundExecution||!g_boundExecution->Current()||g_boundExecution->role!=coop::net::Role::Host||!g_boundExecution->linked)return;
    if(!senderPeerSlot||senderPeerSlot>=coop::players::kMaxPeers){Refuse(senderPeerSlot,"unknown sender");return;}
    if(payload.command>static_cast<uint8_t>(Command::Equipment)||payload._pad[0]||payload._pad[1]||payload._pad[2]){Refuse(senderPeerSlot,"unknown command or padding");return;}
    if(!coop::element::Registry::IsAllowedHostAllocatedEid(payload.elementId)){Refuse(senderPeerSlot,"eid outside host range");return;}
    const auto scope=*g_boundExecution;const auto generation=s->peerGenerationForSlot(senderPeerSlot);const auto ready=s->SlotWorldReadyRevision(senderPeerSlot);
    if(!generation||!s->IsSlotConnected(senderPeerSlot)||!s->IsSlotWorldReady(senderPeerSlot))return;
    auto& slot=g_slots[senderPeerSlot];if(slot.epoch!=scope.epoch||slot.generation!=generation||slot.ready!=ready){slot=Slot{};slot.epoch=scope.epoch;slot.generation=generation;slot.ready=ready;}
    QueuedCmd queued;queued.packet=payload;
    if(coop::element::Registry::Get().ReadIdentity(payload.elementId,queued.identity)&&queued.identity.actor&&R::IsLiveByIndex(queued.identity.actor,queued.identity.internalIdx)){
        queued.actor.Set(queued.identity.actor);queued.cls.Set(R::ClassOf(queued.identity.actor));
    }
    if(!scope.Current()||s->peerGenerationForSlot(senderPeerSlot)!=generation||s->SlotWorldReadyRevision(senderPeerSlot)!=ready)return;
    if(!slot.queue.Push(std::move(queued),kMaxPending))Refuse(senderPeerSlot,"queue full");
}

void NoteRefused(uint8_t senderPeerSlot, const char* why) { Refuse(senderPeerSlot, why); }

void Tick() {
    if(g_pumping)return;g_pumping=true;struct Busy{~Busy(){g_pumping=false;}} busy;
    BindExecution();auto* s=LoadSession();if(!g_boundExecution||!g_boundExecution->Current()||!s)return;const auto scope=*g_boundExecution;
    if(scope.role==coop::net::Role::Host)PumpTakenNotices();else PumpTakenCleanup();if(!scope.Current())return;

    // 1) drain recorded actions (interceptor cancelled the local dispatch; we act here).
    std::vector<PendingCmd> ready;
    { std::lock_guard<std::mutex> lk(g_qMutex); if (!g_queue.empty()) ready.swap(g_queue); }
    for (const PendingCmd& pc : ready) {
        if(!scope.Current())return;
        if(pc.session!=s||pc.epoch!=scope.epoch||pc.serial!=scope.serial||pc.world!=scope.world||pc.worldObject!=scope.object||pc.stops!=scope.stops||pc.isClient!=(scope.role==coop::net::Role::Client)||R::SlotSerial(pc.internalIdx)!=pc.actorSerial)continue;
        if (!pc.actor || !R::IsLiveByIndex(pc.actor, pc.internalIdx)) continue;  // died before drain
        if (pc.isClient) {
            const ElementId eid = FindWireEidForNpcActor(pc.actor);
            if (eid == kInvalidId) {
                UE_LOGW("kerfur_command[client]: command %u on an UNTRACKED kerfur -- no request "
                        "(not adopted yet / rogue local)", static_cast<unsigned>(pc.command));
                continue;
            }
            coop::net::KerfurCommandPayload p{};
            p.elementId = static_cast<uint32_t>(eid);
            p.command   = static_cast<uint8_t>(pc.command);
            if (s->connected())
                s->SendReliable(coop::net::ReliableKind::KerfurCommand, &p, sizeof(p));
            UE_LOGI("kerfur_command[client]: sent command=%u eid=%u",
                    static_cast<unsigned>(pc.command), eid);
        } else {
            // The host's own menu use: requester = host; execute locally.
            const ElementId eid = coop::npc_sync::GetNpcIdForActor(pc.actor);
            if (eid == kInvalidId) {
                UE_LOGW("kerfur_command[host]: own command on an UNTRACKED kerfur -- skipped");
                continue;
            }
            ExecuteHostCommand(eid, pc.actor, pc.command, coop::players::kPeerIdHost);
        }
    }

    // 2) each sender's queued request, once its pace and its body allow; 3) the refusals' summed line once its
    // window has passed, both only on a running host: the queues are the host's, and the roster's bringup resets them
    // off this thread, so no Tick touches them outside a running host session; 4) the startKill watch, until settled.
    if (s->running() && s->role() == coop::net::Role::Host) {
        ServeQueues();
        g_tally.Flush(coop::net::RefusalTally::Clock::now());
    }
    if(scope.Current())Install(s);
}

uint32_t RunsFor(int slot) { return slot >= 1 && slot < coop::players::kMaxPeers ? g_runs[slot] : 0; }
uint32_t RefusedFor(int slot) { return slot >= 1 && slot < coop::players::kMaxPeers ? g_refused[slot] : 0; }

void OnDisconnect(){g_session.store(nullptr,std::memory_order_release);RetireExecutionContext();}

void OnHandTaken(const coop::net::HandTakenPayload& payload) {
    BindExecution();
    if(!g_boundExecution||!g_boundExecution->Current()||g_boundExecution->role!=coop::net::Role::Client||!g_boundExecution->linked)return;
    if(!payload.cls.len||payload.cls.len>sizeof(payload.cls.data))return;
    std::wstring want;for(size_t i=0;i<payload.cls.len;++i){const auto c=static_cast<unsigned char>(payload.cls.data[i]);if(!c||c>=128)return;want.push_back(wchar_t(c));}
    auto work=std::make_shared<TakenCleanup>();work->scope=*g_boundExecution;work->layout=std::make_shared<StagedHand>();g_takenCleanup=work;
    const auto bodyCurrent=[&]{return g_takenCleanup==work&&work->scope.Current();};
    void* body=coop::players::Registry::Get().Local();
    if(!bodyCurrent()||!body||!work->layout->pawn.Capture(body)||!bodyCurrent()){ForgetCleanup(work);return;}
    if(!HandLayout(work->layout,[&]{return CleanupBody(work);})||!CleanupBody(work)){ForgetCleanup(work);return;}
    auto h=work->layout;work->expectedHold=ReadPtr(body,h->holdOff);work->expectedGrab=ReadPtr(body,h->grabOff);
    std::memcpy(&work->name,static_cast<uint8_t*>(body)+h->nameOff,sizeof work->name);
    const auto matches=[&](void* value,Object& object){
        if(!value)return false;if(!object.Capture(value)||!CleanupFields(work))return false;
        const auto name=R::ClassNameOf(value);return CleanupFields(work)&&object.Current()&&name==want;
    };
    work->holdMatch=matches(work->expectedHold,work->held);if(!CleanupFields(work)){ForgetCleanup(work);return;}
    work->grabMatch=matches(work->expectedGrab,work->grabbed);if(!CleanupFields(work)){ForgetCleanup(work);return;}
    if(!work->holdMatch&&!work->grabMatch){ForgetCleanup(work);return;}
    PumpTakenCleanup();
}

}  // namespace coop::kerfur_command
