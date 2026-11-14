// Owned robot/player substitution and guarded native/script seams.
#include "coop/creatures/served_player.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "ue_wrap/actors/kerfur.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/facts/function_row.h"
#include <atomic>
#include <cstring>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace coop::served_player {
namespace {
namespace R=ue_wrap::reflection;namespace sg=ue_wrap::script_gate;namespace UH=ue_wrap::ufunction_hook;
namespace N=coop::net;
UE_FACT_FN(kRowGetMainPlayer, "served_player", L"lib_C", L"getMainPlayer", Declared);
UE_FACT_FN(kRowGetPlayerCharacter, "served_player", L"GameplayStatics", L"GetPlayerCharacter", Declared);
UE_FACT_FN(kRowGetPlayerPawn, "served_player", L"GameplayStatics", L"GetPlayerPawn", Declared);
constexpr int kTagMainPlayer=0x53505001;
constexpr uint8_t kAll=kPlayerCharacter|kPlayerPawn|kMainPlayer;
std::atomic<N::Session*> g_session{nullptr};
uint64_t g_epoch=1,g_token=0,g_seamRevision=0,g_answered=0;
bool g_arming=false,g_dirty=true,g_installing=false,g_ticking=false;
struct Scope {
    N::Session* session=nullptr;uint32_t world=0,serial=0;void* object=nullptr;uint64_t stops=0,epoch=0;bool linked=false;
    bool Current()const{return session&&g_session.load(std::memory_order_acquire)==session&&epoch==g_epoch&&
        world==ue_wrap::world_identity::Generation()&&object&&object==ue_wrap::world_identity::CurrentWorld()&&
        serial==N::session_serial::Current()&&session->running()&&session->role()==N::Role::Host&&session->StopCount()==stops&&session->connected()==linked;}
};
std::optional<Scope> g_bound;
struct Rec {
    Scope scope;ue_wrap::CachedObjRef actor,cls;uint8_t slot=0,seams=0;uint32_t generation=0;uint64_t ready=0;Lease lease;
};
std::unordered_map<void*,std::shared_ptr<Rec>> g_records;
struct NativeSeam {
    const ue_wrap::facts::Function* row;uint8_t bit;UH::PostNativeCallback cb;
    ue_wrap::CachedObjRef fn,owner;uint64_t revision=0;bool installed=false,refused=false,armed=false,armKnown=false;
};
void OnCharacterPost(void*,void*,void*);void OnPawnPost(void*,void*,void*);
sg::Verdict OnMainPlayerPre(const sg::Call&);
NativeSeam g_gpc{&kRowGetPlayerCharacter,kPlayerCharacter,&OnCharacterPost};
NativeSeam g_gpp{&kRowGetPlayerPawn,kPlayerPawn,&OnPawnPost};
bool g_mainPrimed=false,g_mainWatched=false,g_mainRefused=false;
ue_wrap::CachedObjRef g_mainFn,g_mainOwner;
void Clear(){++g_epoch;g_records.clear();g_bound.reset();g_answered=0;g_dirty=true;g_mainPrimed=g_mainRefused=false;}
void Bind(){
    auto* s=g_session.load(std::memory_order_acquire);const auto world=ue_wrap::world_identity::Generation(),serial=N::session_serial::Current();void* object=ue_wrap::world_identity::CurrentWorld();
    if(g_bound&&(!s||g_bound->session!=s||g_bound->world!=world||g_bound->object!=object||g_bound->serial!=serial||
        g_bound->stops!=s->StopCount()||!s->running()||s->role()!=N::Role::Host||g_bound->linked!=s->connected()))Clear();
    if(!g_bound&&s&&s->running()&&s->role()==N::Role::Host&&object)g_bound=Scope{s,world,serial,object,s->StopCount(),g_epoch,s->connected()};
}
bool Mask(uint8_t seams){return seams&&!(seams&~kAll);}
bool Current(const std::shared_ptr<Rec>& row){
    if(!row||!row->scope.Current()||!row->actor.Get()||!row->cls.Get())return false;
    auto* s=row->scope.session;
    if(!row->slot){if(row->seams)return false;}
    else if(!row->scope.linked||row->slot>=N::kMaxPeers||!row->generation||!s->IsSlotConnected(row->slot)||
        s->peerGenerationForSlot(row->slot)!=row->generation||s->SlotWorldReadyRevision(row->slot)!=row->ready||!s->IsSlotWorldReady(row->slot))return false;
    const auto cls=R::ClassOf(row->actor.Raw());
    return row->scope.Current()&&row->actor.Get()&&row->cls.Get()&&cls==row->cls.Raw();
}
bool Installed(const std::shared_ptr<Rec>& row){
    bool exact=false;{auto it=g_records.find(row->actor.Raw());exact=it!=g_records.end()&&it->second==row;}
    if(!exact||!Current(row))return false;
    {auto it=g_records.find(row->actor.Raw());return it!=g_records.end()&&it->second==row;}
}
std::vector<std::shared_ptr<Rec>> Snapshot(){std::vector<std::shared_ptr<Rec>> rows;for(const auto& [actor,row]:g_records)rows.push_back(row);return rows;}
bool Remove(const Lease& lease){
    auto it=g_records.find(lease.robot);if(!lease||it==g_records.end()||it->second->lease.token!=lease.token)return false;
    g_records.erase(it);g_dirty=true;return true;
}
bool SeamCurrent(const NativeSeam& seam){
    if(!seam.fn.Get()||!seam.owner.Get()||R::OuterOf(seam.fn.Raw())!=seam.owner.Raw())return false;
    void* fn=R::FindFunction(seam.owner.Raw(),seam.row->Name());return seam.fn.Get()&&seam.owner.Get()&&fn==seam.fn.Raw();
}
bool MainCurrent(){
    if(!g_mainFn.Get()||!g_mainOwner.Get()||R::OuterOf(g_mainFn.Raw())!=g_mainOwner.Raw())return false;
    void* fn=R::FindFunction(g_mainOwner.Raw(),kRowGetMainPlayer.Name());return g_mainFn.Get()&&g_mainOwner.Get()&&fn==g_mainFn.Raw();
}
bool Active(uint8_t seams){return Mask(seams)&&(!(seams&kPlayerCharacter)||(g_gpc.installed&&g_gpc.armKnown&&g_gpc.armed&&SeamCurrent(g_gpc)))&&
    (!(seams&kPlayerPawn)||(g_gpp.installed&&g_gpp.armKnown&&g_gpp.armed&&SeamCurrent(g_gpp)))&&(!(seams&kMainPlayer)||(g_mainPrimed&&g_mainWatched&&!g_mainRefused&&MainCurrent()));}
void DropNeeding(uint8_t seams){for(const auto& row:Snapshot())if(row->seams&seams)Remove(row->lease);}
void UpdateArm(){
    g_dirty=true;if(g_arming)return;g_arming=true;struct Busy{~Busy(){g_arming=false;}} busy;
    for(int pass=0;pass<3&&g_dirty;++pass){
        g_dirty=false;const auto epoch=g_epoch;const auto scope=g_bound;
        const auto current=[&]{return epoch==g_epoch&&(!scope?!g_bound:(scope->Current()&&g_bound.has_value()));};
        uint8_t want=0;for(const auto& row:Snapshot())if(Installed(row))want|=row->seams;
        if(!current()){g_dirty=true;return;}
        const bool main=(want&kMainPlayer)!=0;
        if((g_mainPrimed||(!main&&g_mainWatched))&&g_mainWatched!=main){
            bool okay=true;if(main)okay=sg::WatchName(kRowGetMainPlayer,kTagMainPlayer,&OnMainPlayerPre,nullptr);
            else okay=sg::UnwatchName(kRowGetMainPlayer,kTagMainPlayer,&OnMainPlayerPre,nullptr);
            if(!current()){g_dirty=true;return;}
            if(okay)g_mainWatched=main;
            else {g_dirty=true;if(main)DropNeeding(kMainPlayer);}
            if(main&&okay){
                const bool live=sg::NameWatchLive(kRowGetMainPlayer,kTagMainPlayer);if(!current()){g_dirty=true;return;}
                if(!live){g_mainPrimed=false;DropNeeding(kMainPlayer);g_dirty=true;}
            }
        }
        for(auto* seam:{&g_gpc,&g_gpp}){
            if(!current()){g_dirty=true;return;}
            if(!seam->installed)continue;
            if(!SeamCurrent(*seam)){seam->installed=seam->armed=seam->refused=seam->armKnown=false;seam->fn.Reset();seam->owner.Reset();seam->revision=++g_seamRevision;g_dirty=true;continue;}
            const bool arm=(want&seam->bit)!=0;if(seam->armKnown&&seam->armed==arm)continue;
            const auto before=*seam;const bool okay=UH::SetArmed(before.fn.Raw(),before.cb,arm);
            if(!current()){if(seam->revision==before.revision)seam->armKnown=false;g_dirty=true;return;}
            if(seam->revision!=before.revision||!SeamCurrent(before)){g_dirty=true;continue;}
            if(okay){seam->armed=arm;seam->armKnown=true;}else g_dirty=true;
        }
    }
}
bool FrameField(void* fn,const wchar_t* name,int32_t& offset,const std::function<bool()>& current){
    const auto size=R::FunctionFrameSize(fn);if(!current()||size<=0||size>1024*1024)return false;
    offset=R::FindParamOffset(fn,name);if(!current()||offset<0||offset>size||sizeof(void*)>size-offset)return false;
    const auto width=R::FindParamSize(fn,name);if(!current()||width!=sizeof(void*))return false;
    int32_t typed=-1;std::wstring type;
    return R::PropertyOffsetAndClass(fn,name,typed,type)&&current()&&typed==offset&&type==L"ObjectProperty";
}
void InstallSeam(NativeSeam& seam,const Scope& scope){
    if(seam.installed&&SeamCurrent(seam))return;if(!scope.Current())return;
    ue_wrap::CachedObjRef owner;owner.Set(R::FindClass(seam.row->Class()));if(!scope.Current()||!owner.Get())return;
    const auto resolved=ue_wrap::facts::ResolveOn(*seam.row,owner.Raw());if(!scope.Current()||!owner.Get())return;
    ue_wrap::CachedObjRef fn;fn.Set(resolved.Fn());if(!fn.Get())return;
    const auto current=[&]{return scope.Current()&&owner.Get()&&fn.Get()&&R::OuterOf(fn.Raw())==owner.Raw();};
    int32_t result=-1;if(!FrameField(fn.Raw(),L"ReturnValue",result,current)||!current())return;
    if(seam.refused&&seam.fn.Is(fn.Raw())&&seam.owner.Is(owner.Raw()))return;
    const auto revision=seam.revision;const bool okay=UH::InstallPostHook(resolved,seam.cb,false);
    if(!current()||seam.revision!=revision)return;
    seam.fn=fn;seam.owner=owner;seam.revision=++g_seamRevision;seam.installed=okay;seam.refused=!okay;seam.armed=false;seam.armKnown=okay;
    if(!okay)DropNeeding(seam.bit);
}
void PrimeMain(const Scope& scope){
    if(g_mainPrimed&&MainCurrent())return;if(g_mainPrimed)g_mainPrimed=false;if(g_mainRefused)return;
    {const bool okay=sg::WatchName(kRowGetMainPlayer,kTagMainPlayer,&OnMainPlayerPre,nullptr);if(!scope.Current())return;g_mainWatched=okay;if(!okay)return;}
    sg::ResolvePendingNames();if(!scope.Current())return;
    const bool live=sg::NameWatchLive(kRowGetMainPlayer,kTagMainPlayer);if(!scope.Current())return;
    if(live){
        ue_wrap::CachedObjRef owner,fn;owner.Set(R::FindClass(kRowGetMainPlayer.Class()));if(!scope.Current()||!owner.Get())return;
        fn.Set(R::FindFunction(owner.Raw(),kRowGetMainPlayer.Name()));if(!scope.Current()||!owner.Get()||!fn.Get()||R::OuterOf(fn.Raw())!=owner.Raw())return;
        if(!scope.Current()||!owner.Get()||!fn.Get())return;g_mainOwner=owner;g_mainFn=fn;g_mainPrimed=true;return;
    }
    const bool settled=sg::NameWatchSettled(kRowGetMainPlayer,kTagMainPlayer);if(!scope.Current())return;
    if(settled){g_mainRefused=true;DropNeeding(kMainPlayer);}
}
void Prepare(){
    Bind();if(g_installing||!g_bound)return;const auto scope=*g_bound;
    g_installing=true;struct Busy{~Busy(){g_installing=false;}} busy;
    PrimeMain(scope);if(!scope.Current())return;InstallSeam(g_gpc,scope);if(!scope.Current())return;InstallSeam(g_gpp,scope);
}
struct Body {
    std::shared_ptr<Rec> service;coop::RemotePlayer* puppet=nullptr;ue_wrap::CachedObjRef actor,cls;
    bool Current()const{
        const auto fresh=[&]{return Installed(service)&&coop::players::Registry::Get().Puppet(service->slot)==puppet&&Installed(service);};
        if(!fresh())return false;
        if(!puppet)return !actor.Raw();
        const bool valid=puppet->valid();if(!fresh())return false;
        if(!valid)return !actor.Raw();
        const bool posed=puppet->HasPose();if(!fresh())return false;
        if(!posed)return !actor.Raw();
        void* body=puppet->GetActor();if(!fresh())return false;
        if(!actor.Raw())return body==nullptr;
        if(body!=actor.Raw()||!actor.Get()||!cls.Get())return false;
        const auto type=R::ClassOf(body);return fresh()&&actor.Get()&&cls.Get()&&type==cls.Raw();
    }
};
bool PawnOf(const std::shared_ptr<Rec>& row,Body& result){
    result.service=row;if(!Installed(row))return false;
    result.puppet=coop::players::Registry::Get().Puppet(row->slot);if(!Installed(row))return false;
    const auto fresh=[&]{return Installed(row)&&coop::players::Registry::Get().Puppet(row->slot)==result.puppet&&Installed(row);};
    if(!result.puppet)return true;
    if(!fresh())return false;
    const bool valid=result.puppet->valid();if(!fresh())return false;if(!valid)return true;
    const bool posed=result.puppet->HasPose();if(!fresh())return false;if(!posed)return true;
    void* pawn=result.puppet->GetActor();if(!fresh())return false;if(!pawn)return true;
    result.actor.Set(pawn);if(!result.actor.Get())return false;result.cls.Set(R::ClassOf(pawn));return result.Current();
}
std::shared_ptr<Rec> Served(void* robot,uint8_t seam){
    auto it=g_records.find(robot);auto row=it==g_records.end()?nullptr:it->second;
    return row&&(row->seams&seam)&&Installed(row)&&Active(row->seams)?row:nullptr;
}
void AnswerNative(void* source,NativeSeam& seam){
    if(!source||!ue_wrap::game_thread::IsGameThread())return;Bind();const auto before=seam;
    if(!before.installed||!before.armed||!SeamCurrent(before))return;
    void* callee=UH::CurrentNativeFunction();if(callee!=before.fn.Raw()||!SeamCurrent(before))return;
    auto row=Served(source,seam.bit);if(!row)return;Body body;if(!PawnOf(row,body)||!body.actor.Raw())return;
    void* result=UH::CurrentResult();if(!result||!body.Current()||!Active(row->seams)||!SeamCurrent(before)||seam.revision!=before.revision)return;
    void* pawn=body.actor.Raw();std::memcpy(result,&pawn,sizeof pawn);++g_answered;
}
void OnCharacterPost(void*,void* source,void*){AnswerNative(source,g_gpc);}
void OnPawnPost(void*,void* source,void*){AnswerNative(source,g_gpp);}
sg::Verdict OnMainPlayerPre(const sg::Call& c){
    Bind();auto row=Served(c.callerObject,kMainPlayer);if(!row||!c.function||!c.callerFunction||!c.callerLocals)return sg::Verdict::Run;
    ue_wrap::CachedObjRef fn,owner,caller,receiver;fn.Set(c.function);caller.Set(c.callerFunction);receiver.Set(c.object);
    if(!fn.Get()||!caller.Get()||!receiver.Get()||!Installed(row))return sg::Verdict::Run;
    owner.Set(R::FindClass(kRowGetMainPlayer.Class()));
    const auto current=[&]{
        if(!Installed(row)||!Active(row->seams)||!fn.Get()||!owner.Get()||!caller.Get()||!receiver.Get())return false;
        void* selected=R::FindFunction(owner.Raw(),L"getMainPlayer");
        return Installed(row)&&fn.Get()&&owner.Get()&&caller.Get()&&receiver.Get()&&selected==fn.Raw()&&R::OuterOf(fn.Raw())==owner.Raw();
    };
    if(!current())return sg::Verdict::Run;
    int32_t offset=-1;if(!FrameField(fn.Raw(),L"AsMain Player",offset,current)||!current())return sg::Verdict::Run;
    uint8_t* out=sg::OutParamPtr(c,offset);if(!out||!current())return sg::Verdict::Run;
    if(!ue_wrap::kerfur::IsInsertPlayerReadWhile(c.callerFunction,c.callerLocals,out,current)||!current())return sg::Verdict::Run;
    Body body;if(!PawnOf(row,body)||!body.Current()||!current())return sg::Verdict::Run;
    void* pawn=body.actor.Raw();std::memcpy(out,&pawn,sizeof pawn);++g_answered;return sg::Verdict::Cancel;
}
} // namespace
struct AssignmentState {Scope scope;void* robot=nullptr;std::shared_ptr<Rec> row;};
bool MatchesAssignment(const AssignmentState& before){
    if(!before.scope.Current())return false;
    std::shared_ptr<Rec> row;{auto it=g_records.find(before.robot);if(it!=g_records.end())row=it->second;}
    return row==before.row&&(!row||Installed(row));
}
Assignment CaptureAssignment(void* robot){
    Bind();if(!robot||!g_bound||!g_bound->Current())return {};
    auto state=std::make_shared<AssignmentState>();state->scope=*g_bound;state->robot=robot;
    {auto it=g_records.find(robot);if(it!=g_records.end())state->row=it->second;}
    if(state->row&&!Installed(state->row))return {};
    if(!state->scope.Current())return {};return {state};
}
bool AssignmentCurrent(const Assignment& before){return before.state&&MatchesAssignment(*before.state);}
void Install(N::Session* session){g_session.store(session,std::memory_order_release);Prepare();UpdateArm();}
bool SeamsReady(uint8_t seams){Prepare();return Mask(seams)&&g_bound&&g_bound->Current()&&
    (!(seams&kPlayerCharacter)||(g_gpc.installed&&SeamCurrent(g_gpc)))&&(!(seams&kPlayerPawn)||(g_gpp.installed&&SeamCurrent(g_gpp)))&&
    (!(seams&kMainPlayer)||(g_mainPrimed&&!g_mainRefused&&MainCurrent()));}
bool SeamsRefused(uint8_t seams){return !Mask(seams)||((seams&kPlayerCharacter)&&g_gpc.refused)||((seams&kPlayerPawn)&&g_gpp.refused)||((seams&kMainPlayer)&&g_mainRefused);}
Lease ServeWithExpectation(void* robot,uint8_t slot,uint8_t seams,const std::function<bool()>& caller,const AssignmentState* expected){
    Bind();if(!g_bound||!robot||!Mask(seams)||slot<=0||slot>=N::kMaxPeers)return {};
    const auto scope=*g_bound;const auto current=[&]{return scope.Current()&&(!caller||caller())&&scope.Current();};
    const auto beforeCurrent=[&]{return current()&&(!expected||MatchesAssignment(*expected))&&current();};
    if(!beforeCurrent())return {};auto row=std::make_shared<Rec>();row->scope=scope;row->slot=slot;row->seams=seams;
    row->actor.Set(robot);if(!current()||!row->actor.Get())return {};row->cls.Set(R::ClassOf(robot));
    row->generation=scope.session->peerGenerationForSlot(slot);row->ready=scope.session->SlotWorldReadyRevision(slot);
    if(!beforeCurrent()||!Current(row)||!SeamsReady(seams)||!beforeCurrent())return {};
    std::shared_ptr<Rec> previous;if(auto it=g_records.find(robot);it!=g_records.end())previous=it->second;
    if(!++g_token)++g_token;row->lease={robot,g_token};g_records[robot]=row;UpdateArm();
    if(current()&&Installed(row)&&Active(seams)&&current()&&Installed(row))return row->lease;
    if(Remove(row->lease)&&previous&&Current(previous)&&!g_records.contains(robot))g_records.emplace(robot,previous);
    UpdateArm();return {};
}
Lease ServeOwned(void* robot,uint8_t slot,uint8_t seams,const std::function<bool()>& current){return ServeWithExpectation(robot,slot,seams,current,nullptr);}
Lease AssignOwned(const Assignment& before,uint8_t slot,uint8_t seams,const std::function<bool()>& caller){
    if(!before.state)return {};const auto state=before.state;
    if(slot)return ServeWithExpectation(state->robot,slot,seams,caller,state.get());
    Bind();if(!g_bound)return {};const auto scope=*g_bound;
    const auto current=[&]{return scope.Current()&&(!caller||caller())&&scope.Current();};
    if(!current()||!MatchesAssignment(*state)||!current())return {};
    auto row=std::make_shared<Rec>();row->scope=scope;row->actor.Set(state->robot);
    if(!current()||!row->actor.Get())return {};row->cls.Set(R::ClassOf(state->robot));
    if(!current()||!Current(row)||!MatchesAssignment(*state)||!current())return {};
    if(!++g_token)++g_token;row->lease={state->robot,g_token};g_records[state->robot]=row;UpdateArm();
    if(current()&&Installed(row))return row->lease;
    if(Remove(row->lease)&&state->row&&Current(state->row)&&!g_records.contains(state->robot))g_records.emplace(state->robot,state->row);
    UpdateArm();return {};
}
bool RestoreOwned(const Lease& attempt,const Assignment& before,const std::function<bool()>& current){
    if(!before.state||!before.state->scope.Current()||before.state->robot!=attempt.robot||!Owns(attempt)||(current&&!current()))return false;
    const auto previous=before.state->row;const bool valid=!previous||Current(previous);
    if((current&&!current())||!Owns(attempt)||!Remove(attempt))return false;
    if(valid&&previous&&!g_records.contains(attempt.robot))g_records.emplace(attempt.robot,previous);
    UpdateArm();return true;
}
bool Serve(void* robot,uint8_t slot,uint8_t seams){if(!slot){Unserve(robot);return false;}return bool(ServeOwned(robot,slot,seams));}
bool Owns(const Lease& lease){std::shared_ptr<Rec> row;{auto it=g_records.find(lease.robot);if(it!=g_records.end())row=it->second;}return lease&&row&&row->lease.token==lease.token&&Installed(row);}
bool UnserveOwned(const Lease& lease){if(!Remove(lease))return false;UpdateArm();return true;}
void Unserve(void* robot){auto it=g_records.find(robot);if(it!=g_records.end()){const auto lease=it->second->lease;UnserveOwned(lease);}}
uint8_t ServedSlot(void* robot){Bind();auto it=g_records.find(robot);const auto row=it==g_records.end()?nullptr:it->second;return row&&Installed(row)&&Active(row->seams)?row->slot:0;}
void Tick(){
    if(g_ticking)return;g_ticking=true;struct Busy{~Busy(){g_ticking=false;}} busy;
    Prepare();for(const auto& row:Snapshot())if(!Installed(row))Remove(row->lease);UpdateArm();
}
void OnPeerLeft(uint8_t slot){if(!slot||slot>=N::kMaxPeers)return;for(const auto& row:Snapshot())if(row->slot==slot)Remove(row->lease);UpdateArm();}
void OnDisconnect(){g_session.store(nullptr,std::memory_order_release);Clear();UpdateArm();}
} // namespace coop::served_player
