// Kerfus shared commands: owned admission, FIFO retry and guarded native dispatch.
#include "coop/creatures/kerfus_intent.h"
#include "coop/creatures/kerfus_follow.h"
#include "coop/element/intent_authority.h"
#include "coop/element/registry.h"
#include "coop/net/intent_queue.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/players_registry.h"
#include "coop/props/active_drive.h"
#include "ue_wrap/actors/kerfus.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/facts/function_row.h"
#include <array>
#include <atomic>
#include <cstring>
#include <deque>
#include <memory>
#include <optional>
#include <string>

namespace coop::kerfus_intent {
namespace {
namespace EL=coop::element;namespace R=ue_wrap::reflection;namespace UK=ue_wrap::kerfus;
namespace sg=ue_wrap::script_gate;namespace N=coop::net;
using Packet=N::KerfusIntentPayload;
UE_FACT_FN(kRowActionOptionIndex, "kerfus_intent", L"p_kerfus_C", L"actionOptionIndex", Declared);
UE_FACT_FN(kRowActionName, "kerfus_intent", L"p_kerfus_C", L"actionName", Declared);
constexpr int kTagAction=0x4B464901,kTagName=0x4B464902;
constexpr float kReach=400.f;constexpr N::IntentBudget kBudget{10.f,10.f};
constexpr size_t kCapacity=16,kClientDrain=8;constexpr uint64_t kRetryMs=250;
std::atomic<N::Session*> g_session{nullptr};
bool g_actionWatched=false,g_nameWatched=false,g_saidLive=false,g_pumping=false,g_registering=false;
uint64_t g_epoch=1,g_nextCommand=0,g_sent=0,g_run=0,g_denied=0;
struct Scope {
    N::Session* session=nullptr;void* worldObject=nullptr;uint32_t world=0,serial=0;
    uint64_t stops=0,epoch=0;N::Role role=N::Role::Host;bool linked=false;
    bool Current()const{return session&&session==g_session.load(std::memory_order_acquire)&&epoch==g_epoch&&
        worldObject&&worldObject==ue_wrap::world_identity::CurrentWorld()&&world==ue_wrap::world_identity::Generation()&&
        serial==N::session_serial::Current()&&session->StopCount()==stops&&session->running()&&session->role()==role&&session->connected()==linked;}
};
std::optional<Scope> g_bound;
struct Target {
    EL::Registry::IdentitySnapshot identity;ue_wrap::CachedObjRef actor,cls;
    bool Bound(bool mirror)const{
        EL::Registry::IdentitySnapshot now;
        return EL::Registry::Get().ReadIdentity(identity.id,now)&&now.incarnation==identity.incarnation&&
            now.type==EL::ElementType::Prop&&now.typeName==identity.typeName&&now.mirror==mirror&&!now.deleting&&
            actor.Get()&&cls.Get()&&now.actor==actor.Raw()&&now.internalIdx==actor.Idx()&&
            R::ClassOf(actor.Raw())==cls.Raw()&&EL::Registry::Get().EidForActor(now.actor)==identity.id;
    }
};
struct Command {Scope scope;Packet packet{};Target target;uint64_t sequence=0,at=0;bool attempted=false,dispatched=false;};
struct Slot {Scope scope;int slot=0;uint32_t generation=0;uint64_t ready=0;N::IntentQueue<std::shared_ptr<Command>> queue;};
std::array<std::shared_ptr<Slot>,N::kMaxPeers> g_slots;
std::deque<std::shared_ptr<Command>> g_client;
struct Running {std::shared_ptr<Slot> owner;std::shared_ptr<Command> command;};
const Running* g_running=nullptr;
struct Busy {Busy(){g_pumping=true;}~Busy(){g_pumping=false;}};
bool Verb(uint8_t action){return action==4||action==6||action==8;}
bool Valid(const Packet& p){return EL::Registry::IsAllowedHostAllocatedEid(p.elementId)&&Verb(p.action)&&!p._pad[0]&&!p._pad[1]&&!p._pad[2];}
void Clear(){++g_epoch;g_bound.reset();for(auto& slot:g_slots)slot.reset();g_client.clear();g_running=nullptr;g_sent=g_run=g_denied=0;}
void Bind(){
    auto* s=g_session.load(std::memory_order_acquire);void* object=ue_wrap::world_identity::CurrentWorld();
    const auto world=ue_wrap::world_identity::Generation(),serial=N::session_serial::Current();
    if(g_bound&&(!s||g_bound->session!=s||g_bound->worldObject!=object||g_bound->world!=world||g_bound->serial!=serial||
        g_bound->stops!=s->StopCount()||g_bound->role!=s->role()||g_bound->linked!=s->connected()||!s->running()))Clear();
    if(!g_bound&&s&&s->running()&&object)g_bound=Scope{s,object,world,serial,s->StopCount(),g_epoch,s->role(),s->connected()};
}
bool Capture(Target& target,uint32_t id,bool mirror,const std::function<bool()>& current){
    if(!current()||!EL::Registry::IsAllowedHostAllocatedEid(id)||!EL::Registry::Get().ReadIdentity(id,target.identity)||
        target.identity.type!=EL::ElementType::Prop||target.identity.mirror!=mirror||target.identity.deleting||!target.identity.actor||
        !R::IsLiveByIndex(target.identity.actor,target.identity.internalIdx))return false;
    target.actor.Set(target.identity.actor);if(!current()||!target.actor.Get())return false;
    target.cls.Set(R::ClassOf(target.actor.Raw()));return current()&&target.Bound(mirror)&&current();
}
bool Sender(const std::shared_ptr<Slot>& row){auto* s=row->scope.session;return row->scope.Current()&&row->scope.linked&&
    row->generation&&s->IsSlotConnected(row->slot)&&s->peerGenerationForSlot(row->slot)==row->generation&&
    s->SlotWorldReadyRevision(row->slot)==row->ready&&s->IsSlotWorldReady(row->slot);}
bool Installed(const std::shared_ptr<Slot>& row){return row&&row->slot>0&&row->slot<N::kMaxPeers&&g_slots[row->slot]==row&&Sender(row);}
bool Current(const std::shared_ptr<Slot>& row,const std::shared_ptr<Command>& command){return Installed(row)&&command->scope.Current()&&command->target.Bound(false)&&Installed(row);}
std::shared_ptr<Slot> SenderSlot(const Scope& scope,int slot){
    if(slot<=0||slot>=N::kMaxPeers||scope.role!=N::Role::Host||!scope.Current()||!scope.linked)return {};
    auto* s=scope.session;const auto gen=s->peerGenerationForSlot(slot);const auto ready=s->SlotWorldReadyRevision(slot);
    if(!gen||!s->IsSlotConnected(slot)||!s->IsSlotWorldReady(slot)){g_slots[slot].reset();return {};}
    auto row=g_slots[slot];if(!row||!Installed(row)){
        row=std::make_shared<Slot>();row->scope=scope;row->slot=slot;row->generation=gen;row->ready=ready;g_slots[slot]=row;
    }
    return row;
}
bool ClientCurrent(const std::shared_ptr<Command>& command){return command->scope.Current()&&command->scope.role==N::Role::Client&&
    command->scope.linked&&command->target.Bound(true)&&command->scope.Current();}
void PumpClient(const Scope& scope){
    const size_t count=std::min(kClientDrain,g_client.size());
    for(size_t i=0;i<count&&!g_client.empty();++i){
        if(!scope.Current())return;const auto command=g_client.front();
        const auto installed=[&]{return !g_client.empty()&&g_client.front()==command&&ClientCurrent(command);};
        if(!ClientCurrent(command)){if(!g_client.empty()&&g_client.front()==command)g_client.pop_front();continue;}
        const auto now=coop::active_drive::NowMs();if(!installed())return;
        if(command->attempted&&now>=command->at&&now-command->at<kRetryMs)return;
        command->attempted=true;command->at=now;
        const bool accepted=scope.session->SendReliable(N::ReliableKind::KerfusIntent,&command->packet,sizeof command->packet);
        if(!installed()||!accepted)return;
        g_client.pop_front();++g_sent;
    }
}
void Send(const Scope& scope,void* actor,uint8_t action,const std::function<bool()>& current){
    if(!scope.linked||!scope.Current()||!current()||g_client.size()>=kCapacity)return;
    const auto id=EL::Registry::Get().EidForActor(actor);auto command=std::make_shared<Command>();
    command->scope=scope;command->packet.elementId=id;command->packet.action=action;
    if(!Valid(command->packet)||!Capture(command->target,id,true,current)||!current())return;
    command->sequence=++g_nextCommand;g_client.push_back(command);
    if(!g_pumping){Busy busy;PumpClient(scope);}
}
bool Copy(const void* source,void* target,size_t bytes){
    if(!source||!target)return false;
    __try{std::memcpy(target,source,bytes);return true;}__except(1){return false;}
}
bool Span(int32_t off,int32_t width,int32_t size){return off>=0&&width>0&&size>0&&off<=size&&width<=size-off;}
struct Pre {
    Scope scope;ue_wrap::CachedObjRef actor,cls,fn;const wchar_t* name=nullptr;
    Target target;bool tracked=false;
    bool Current()const{
        if(!scope.Current()||!actor.Get()||!cls.Get()||!fn.Get()||R::ClassOf(actor.Raw())!=cls.Raw())return false;
        void* resolved=R::FindDispatchFunctionCached(cls.Raw(),name);
        return scope.Current()&&actor.Get()&&cls.Get()&&fn.Get()&&resolved==fn.Raw()&&
            (!tracked||target.Bound(scope.role==N::Role::Client))&&scope.Current();
    }
    bool Capture(const sg::Call& c,const Scope& s,const wchar_t* field){
        scope=s;name=field;actor.Set(c.object);fn.Set(c.function);if(!scope.Current()||!actor.Get()||!fn.Get())return false;
        const auto id=EL::Registry::Get().EidForActor(c.object);
        tracked=EL::Registry::IsAllowedHostAllocatedEid(id)&&EL::Registry::Get().ReadIdentity(id,target.identity);
        if(scope.role==N::Role::Client&&!tracked)return false;
        cls.Set(R::ClassOf(c.object));target.actor=actor;target.cls=cls;return Current();
    }
    bool Param(const wchar_t* field,int32_t width,int32_t& offset,const wchar_t* expected){
        const auto size=R::FunctionFrameSize(fn.Raw());if(!Current()||size<=0||size>4096)return false;
        offset=R::FindParamOffset(fn.Raw(),field);if(!Current()||!Span(offset,width,size))return false;
        const auto bytes=R::FindParamSize(fn.Raw(),field);if(!Current()||bytes!=width)return false;
        int32_t typedOffset=-1;std::wstring type;
        return R::PropertyOffsetAndClass(fn.Raw(),field,typedOffset,type)&&Current()&&typedOffset==offset&&
            (type==expected||(type==L"EnumProperty"&&std::wstring(expected)==L"ByteProperty"));
    }
};
bool FString(const uint8_t* locals,int32_t offset,std::wstring& name){
    struct Header{const wchar_t* data;int32_t num,max;} header{};static_assert(sizeof(Header)==16);
    if(!locals||!Copy(locals+offset,&header,sizeof header)||header.num<0||header.max<header.num)return false;
    if(!header.num){name.clear();return true;}
    if(!header.data||header.num>64)return false;
    std::array<wchar_t,64> chars{};
    if(!Copy(header.data,chars.data(),size_t(header.num)*sizeof(wchar_t))||chars[header.num-1]!=0)return false;
    name.assign(chars.data(),size_t(header.num-1));return true;
}
sg::Verdict Gate(const sg::Call& c,bool named){
    Bind();if(!g_bound||!g_bound->Current()){
        auto* s=g_session.load(std::memory_order_acquire);
        return s&&s->running()&&s->role()==N::Role::Client?sg::Verdict::Cancel:sg::Verdict::Run;
    }
    const auto scope=*g_bound;Pre proof;const wchar_t* fn=named?L"actionName":L"actionOptionIndex";
    if(!c.object||!c.function||!proof.Capture(c,scope,fn))return sg::Verdict::Cancel;
    const auto current=[&]{return proof.Current();};int32_t offset=-1;uint8_t action=0;
    if(named){
        std::wstring name;if(!proof.Param(L"name",16,offset,L"StrProperty")||!FString(c.locals,offset,name)||!current())return sg::Verdict::Cancel;
        if(name==L"activate")action=8;else if(name==L"use")action=4;else if(name==L"pat")action=6;else return sg::Verdict::Run;
    }else if(!proof.Param(L"action",1,offset,L"ByteProperty")||!c.locals||!Copy(c.locals+offset,&action,1)||!current())return sg::Verdict::Cancel;
    if(!Verb(action))return sg::Verdict::Run;
    if(scope.role==N::Role::Client){if(!c.fromOurCode)Send(scope,c.object,action,current);return sg::Verdict::Cancel;}
    if(action==8){
        const Running* running=g_running;uint8_t slot=0;
        if(running&&running->command->target.actor.Raw()!=c.object)running=nullptr;
        if(running&&running->command->target.actor.Raw()==c.object){if(!Current(running->owner,running->command))return sg::Verdict::Cancel;slot=uint8_t(running->owner->slot);}
        coop::kerfus_follow::OnToggle(c.object,slot);
        if(!current()||(running&&!Current(running->owner,running->command)))return sg::Verdict::Cancel;
    }
    return sg::Verdict::Run;
}
sg::Verdict OnActionPre(const sg::Call& call){return Gate(call,false);}
sg::Verdict OnNamePre(const sg::Call& call){return Gate(call,true);}
N::Served Execute(const std::shared_ptr<Slot>& owner,const std::shared_ptr<Command>& command){
    const auto current=[&]{return Current(owner,command);};if(!current())return N::Served::Done;
    const auto token=EL::IntentTarget::ForClientIntent(*owner->scope.session,uint8_t(owner->slot),kReach);
    const auto subject=token.Authorize(command->target.actor.Raw());if(!current())return N::Served::Done;
    if(subject.outcome==EL::IntentOutcome::NoBody)return N::Served::Wait;
    if(!subject||subject.actor!=command->target.actor.Raw()){++g_denied;return N::Served::Done;}
    const bool kind=UK::IsKerfusWhile(subject.actor,current);if(!current())return N::Served::Done;
    if(!kind){++g_denied;return N::Served::Done;}
    void* player=coop::players::Registry::Get().Local();if(!current())return N::Served::Done;
    Running invocation{owner,command};const Running* previous=g_running;g_running=&invocation;
    struct Restore {const Running* invocation;const Running* previous;~Restore(){if(g_running==invocation)g_running=previous;}} restore{&invocation,previous};
    const bool ran=UK::RunActionOptionIndexWhile(subject.actor,player,command->packet.action,current,&command->dispatched);
    if(!current())return N::Served::Done;
    if(!command->dispatched)return N::Served::Wait;
    if(ran)++g_run;else ++g_denied;
    return N::Served::Done;
}
void Register(const Scope& scope){
    if(g_registering)return;g_registering=true;
    struct Registering {~Registering(){g_registering=false;}} registering;
    if(!g_actionWatched){const bool okay=sg::WatchName(kRowActionOptionIndex,kTagAction,&OnActionPre,nullptr);if(!scope.Current())return;g_actionWatched=okay;}
    if(!g_nameWatched){const bool okay=sg::WatchName(kRowActionName,kTagName,&OnNamePre,nullptr);if(!scope.Current())return;g_nameWatched=okay;}
}
} // namespace
void Install(N::Session* session){g_session.store(session,std::memory_order_release);Bind();if(g_bound){const auto scope=*g_bound;Register(scope);}}
void Tick(N::Session& session){
    if(g_pumping)return;Bind();if(!g_bound||g_bound->session!=&session||!g_bound->Current())return;
    Busy busy;const auto scope=*g_bound;Register(scope);if(!scope.Current())return;
    if(!g_saidLive&&g_actionWatched&&g_nameWatched){
        sg::ResolvePendingNames();if(!scope.Current())return;
        const bool first=sg::NameWatchLive(kRowActionOptionIndex,kTagAction);if(!scope.Current())return;
        const bool second=sg::NameWatchLive(kRowActionName,kTagName);if(!scope.Current())return;g_saidLive=first&&second;
    }
    if(scope.role==N::Role::Client){if(scope.linked)PumpClient(scope);return;}
    for(int slot=1;slot<N::kMaxPeers;++slot){
        if(!scope.Current())return;auto row=SenderSlot(scope,slot);if(!row)continue;
        row->queue.Serve(kBudget,coop::active_drive::NowMs,[&](const std::shared_ptr<Command>& command){return Execute(row,command);});
    }
}
void OnIntent(N::Session& session,const Packet& payload,uint8_t slot){
    Bind();if(!g_bound||g_bound->session!=&session||!g_bound->Current()||!Valid(payload))return;
    const auto scope=*g_bound;auto owner=SenderSlot(scope,slot);if(!owner)return;
    auto command=std::make_shared<Command>();command->scope=scope;command->packet=payload;command->sequence=++g_nextCommand;
    const auto current=[&]{return Installed(owner);};
    if(!Capture(command->target,payload.elementId,false,current)||!current())return;
    if(!owner->queue.Push(std::move(command),kCapacity)&&current())++g_denied;
}
void OnDisconnect(){Clear();}
} // namespace coop::kerfus_intent
