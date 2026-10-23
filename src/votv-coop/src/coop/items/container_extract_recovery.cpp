#include "coop/items/container_extract_recovery.h"

#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/net/net_clock.h"
#include "coop/player/players_registry.h"
#include "coop/props/container_contents_sync.h"
#include "coop/props/prop_lifecycle.h"
#include "ue_wrap/actors/container_inventory.h"
#include "ue_wrap/actors/inventory.h"
#include "ue_wrap/actors/save_record.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/world_identity.h"
#include "ue_wrap/facts/function_row.h"

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <map>
#include <memory>
#include <vector>

namespace coop::container_extract_recovery {
namespace {
namespace SG=ue_wrap::script_gate;
namespace R=ue_wrap::reflection;
namespace SR=ue_wrap::save_record;
namespace CI=ue_wrap::container_inventory;
namespace WI=ue_wrap::world_identity;
UE_FACT_FN(kExtract,"container_extract_recovery",L"prop_container_C",L"extract",Declared);
UE_FACT_FN(kTake,"container_extract_recovery",L"propInventory_C",L"takeObj",Declared);
constexpr int kExtractTag=0x58435245,kTakeTag=0x54435245;
constexpr size_t kDebtCap=64,kMaxContents=16384;
constexpr uint64_t kRetryMs=250;
net::Session* g_session=nullptr;
uint64_t g_epoch=0,g_sequence=0,g_cursor=0;
uint32_t g_world=0,g_serial=0;
net::Role g_role=net::Role::Host;
bool g_watched=false,g_draining=false;
net::Session* g_settlingSession=nullptr;
uint64_t g_settlingEpoch=0;

struct Layout {
    ue_wrap::CachedObjRef function;
    int32_t index=-1,remove=-1,success=-1,record=-1,actor=-1,begun=-1,pocket=-1,taken=-1;
};
Layout g_extract,g_take;
struct Source {
    net::Session* session=nullptr;
    uint64_t epoch=0;
    uint32_t world=0,serial=0;
    net::Role role=net::Role::Host;
    ue_wrap::CachedObjRef container,inventory,save,player,function;
    ue_wrap::CachedObjRef containerType,inventoryType,saveType,playerType;
    uint8_t* slot=nullptr;
};
struct Debt {
    uint64_t id=0,extractInvocation=0,takeInvocation=0,pocketInvocation=0,next=0;
    Source source;
    SR::SaveRecord record;
    int32_t index=-1;
    ue_wrap::CachedObjRef actor,actorType,takeFunction,pocketFunction;
    bool debit=false,restored=false,refund=false,finished=false,absent=false,pocketSeen=false,pocketDone=false;
    bool busy=false,uncertain=false,saidUncertain=false;
};
std::map<uint64_t,std::shared_ptr<Debt>> g_debts;

bool Context() {
    return ue_wrap::game_thread::IsGameThread() && g_session &&
        ((g_settlingSession==g_session && g_settlingEpoch==g_epoch) || (g_session->running() && g_session->connected())) &&
        g_session->role()==g_role && g_world==WI::Generation() &&
        g_serial==net::session_serial::Current();
}
bool Owned(const std::shared_ptr<Debt>& row) {
    const auto it=g_debts.find(row->id);
    return it!=g_debts.end() && it->second==row;
}
bool Current(const Source& s) {
    return Context() && s.session==g_session && s.epoch==g_epoch && s.world==g_world &&
        s.serial==g_serial && s.role==g_role && s.container.Get() && s.inventory.Get() && s.save.Get() &&
        s.player.Get() && players::Registry::Get().Local()==s.player.Raw() &&
        s.containerType.Get() && R::ClassOf(s.container.Raw())==s.containerType.Raw() &&
        s.inventoryType.Get() && R::ClassOf(s.inventory.Raw())==s.inventoryType.Raw() &&
        s.saveType.Get() && R::ClassOf(s.save.Raw())==s.saveType.Raw() &&
        s.playerType.Get() && R::ClassOf(s.player.Raw())==s.playerType.Raw() &&
        ue_wrap::inventory::ResolveSaveSlot()==s.save.Raw() &&
        CI::InventoryOf(s.container.Raw())==s.inventory.Raw() && CI::OwnerOf(s.inventory.Raw())==s.container.Raw() &&
        CI::WorldContentsSlot(s.inventory.Raw())==s.slot;
}
bool Current(const std::shared_ptr<Debt>& row) {return Owned(row) && Current(row->source);}
bool Same(const SR::SaveRecord& a,const SR::SaveRecord& b) {
    return a.className==b.className && a.xform==b.xform && a.key==b.key && a.bools==b.bools &&
        a.floats==b.floats && a.ints==b.ints && a.strings==b.strings && a.signals==b.signals &&
        a.classes==b.classes && a.vectors==b.vectors && a.rotators==b.rotators &&
        a.transforms==b.transforms && a.bytes==b.bytes && a.names==b.names;
}
bool Fits(int32_t offset,int32_t size,int32_t frame) {
    return offset>=0 && size>=0 && offset<=frame && size<=frame-offset;
}
Layout ExtractLayout(void* function) {
    if(!g_extract.function.Is(function)) {
        g_extract={};g_extract.function.Set(function);
        const auto frame=R::FunctionFrameSize(function);
        const auto index=R::FindParamOffset(function,L"index");
        if(Fits(index,4,frame))g_extract.index=index;
        for(const auto& f:R::EnumerateStructFields(function)) {
            if(!Fits(f.offset,f.size,frame))continue;
            if(!::_wcsicmp(f.name.c_str(),L"CallFunc_takeObj_Output") && f.size>=SR::kSaveRecordBytes)g_extract.record=f.offset;
            if(!::_wcsicmp(f.name.c_str(),L"CallFunc_FinishSpawningActor_ReturnValue") && f.size==sizeof(void*))g_extract.actor=f.offset;
            if(!::_wcsicmp(f.name.c_str(),L"CallFunc_BeginDeferredActorSpawnFromClass_ReturnValue") && f.size==sizeof(void*))g_extract.begun=f.offset;
            if(!::_wcsicmp(f.name.c_str(),L"CallFunc_takeObj_return") && f.size==1)g_extract.taken=f.offset;
            if(!::_wcsicmp(f.name.c_str(),L"CallFunc_putObjectInventory2_return") && f.size==1)g_extract.pocket=f.offset;
        }
    }
    return g_extract;
}
Layout TakeLayout(void* function) {
    if(!g_take.function.Is(function)) {
        g_take={};g_take.function.Set(function);
        const auto frame=R::FunctionFrameSize(function);
        const auto field=[&](const wchar_t* name,int bytes){const auto off=R::FindParamOffset(function,name);return Fits(off,bytes,frame)?off:-1;};
        g_take.index=field(L"index",4);g_take.remove=field(L"removeVol",1);g_take.success=field(L"return",1);
        g_take.record=field(L"Output",SR::kSaveRecordBytes);g_take.actor=field(L"object",sizeof(void*));
    }
    return g_take;
}
const uint8_t* Output(const SG::Call& call,int32_t offset) {
    if(offset<0 || !call.locals)return nullptr;
    const auto* output=SG::OutParamPtr(call,offset);
    return output?output:call.locals+offset;
}
void RefuseTake(const SG::Call& call,const Layout& layout) {
    if(layout.success>=0) {
        call.locals[layout.success]=0;
        if(auto* out=SG::OutParamPtr(call,layout.success))*out=0;
    }
    if(layout.actor>=0) {
        void* none=nullptr;std::memcpy(call.locals+layout.actor,&none,sizeof(none));
        if(auto* out=SG::OutParamPtr(call,layout.actor))std::memcpy(out,&none,sizeof(none));
    }
}
std::shared_ptr<Debt> ExtractFor(void* object,void* function) {
    for(auto it=g_debts.rbegin();it!=g_debts.rend();++it) {
        const auto& row=it->second;
        if(row->source.container.Is(object) && row->source.function.Is(function) &&
           SG::IsInvocationActive(row->extractInvocation))return row;
    }
    return {};
}
void Complete(const std::shared_ptr<Debt>& row) {
    if(!Owned(row))return;
    g_debts.erase(row->id);
}
bool Attach(const std::shared_ptr<Debt>& row,void* actor) {
    if(!actor){row->absent=true;return true;}
    if(row->actor.Raw()==actor && !row->actor.Get()){row->absent=true;return true;}
    if(!R::IsLive(actor) || R::ClassNameOf(actor)!=row->record.className)return false;
    if(row->actor.Raw()) {
        if(!row->actor.Is(actor) || !row->actorType.Get() || R::ClassOf(actor)!=row->actorType.Raw())return false;
        row->absent=false;return true;
    }
    row->actor.Set(actor);row->actorType.Set(R::ClassOf(actor));row->absent=false;return true;
}

void Recover(const std::shared_ptr<Debt>& row) {
    if(!Current(row) || row->busy || !row->debit || !row->refund || row->uncertain)return;
    row->busy=true;struct Guard{Debt& row;~Guard(){row.busy=false;}} guard{*row};
    row->next=net::NowMs()+kRetryMs;
    const auto current=[&]{return Current(row);};
    if(void* actor=row->actor.Get()) {
        if(!row->actorType.Get() || R::ClassOf(actor)!=row->actorType.Raw()){row->uncertain=true;return;}
        // No saved-row credit while its carrier can still be collected. The
        // existing destroy helper suppresses network echo and validates serials.
        prop_lifecycle::DestroyLocalProp(actor,false);
        if(!current() || row->actor.Get())return;
    } else if(!row->actor.Raw() && !row->absent) {
        return; // a skipped outer POST did not prove whether a carrier was born
    }
    if(!current())return;
    const bool committed=CI::RefundRecordWhile(row->source.inventory.Raw(),row->record,current);
    if(!committed)return;
    // True is a committed credit even when allocator cleanup reentered and
    // invalidated its source. Retire before any derived-state callback.
    Complete(row);
    if(Current(row->source)) {
        props::container_contents_sync::NoteLocalContentsChange(row->source.container.Raw());
        if(Current(row->source))CI::RederiveShownStateWhile(row->source.container.Raw(),row->source.inventory.Raw(),[source=row->source]{return Current(source);});
    }
}

SG::Verdict BeforeExtract(const SG::Call& call) {
    if(!Context())return SG::Verdict::Run;
    if(g_settlingSession==g_session && g_settlingEpoch==g_epoch)return SG::Verdict::Cancel;
    if(!g_watched)return SG::Verdict::Cancel;
    if(!call.object || !call.function || !call.locals || !call.invocation)return SG::Verdict::Cancel;
    if(!CI::IsContainer(call.object))return SG::Verdict::Run;
    auto* inv=CI::InventoryOf(call.object);
    if(!inv)return SG::Verdict::Cancel;
    bool personal=false;
    if(!CI::ReadPersonalFlag(inv,personal))return SG::Verdict::Cancel;
    if(personal)return SG::Verdict::Run;
    const auto layout=ExtractLayout(call.function);
    if(layout.index<0 || layout.record<0 || layout.actor<0 || layout.begun<0 || layout.pocket<0 || layout.taken<0 || g_debts.size()>=kDebtCap)return SG::Verdict::Cancel;
    auto row=std::make_shared<Debt>();row->id=++g_sequence;row->extractInvocation=call.invocation;
    auto& source=row->source;
    source.session=g_session;source.epoch=g_epoch;source.world=g_world;source.serial=g_serial;source.role=g_role;
    source.container.Set(call.object);source.inventory.Set(inv);source.function.Set(call.function);
    source.save.Set(ue_wrap::inventory::ResolveSaveSlot());source.player.Set(players::Registry::Get().Local());
    if(!source.container.Get() || !source.inventory.Get() || !source.save.Get() || !source.player.Get())return SG::Verdict::Cancel;
    source.containerType.Set(R::ClassOf(source.container.Raw()));source.inventoryType.Set(R::ClassOf(source.inventory.Raw()));
    source.saveType.Set(R::ClassOf(source.save.Raw()));source.playerType.Set(R::ClassOf(source.player.Raw()));
    source.slot=CI::WorldContentsSlot(inv);
    if(!source.slot || !Current(source))return SG::Verdict::Cancel;
    std::memcpy(&row->index,call.locals+layout.index,4);
    std::vector<SR::SaveRecord> records;
    if(!CI::ReadContents(inv,records,kMaxContents) || !Current(source))return SG::Verdict::Cancel;
    if(row->index<0 || size_t(row->index)>=records.size())return SG::Verdict::Run;
    row->record=std::move(records[size_t(row->index)]);
    if(row->record.className.empty() || row->record.key.empty())return SG::Verdict::Cancel;
    call.locals[layout.taken]=call.locals[layout.pocket]=0;
    g_debts.emplace(row->id,std::move(row));
    return SG::Verdict::Run;
}
SG::Verdict BeforeTake(const SG::Call& call) {
    auto row=ExtractFor(call.callerObject,call.callerFunction);
    if(!row || !row->source.inventory.Is(call.object))return SG::Verdict::Run;
    const auto layout=TakeLayout(call.function);
    const auto refuse=[&]{
        if(call.locals)RefuseTake(call,layout);
        const auto parent=ExtractLayout(call.callerFunction);
        if(call.callerLocals && parent.taken>=0)call.callerLocals[parent.taken]=0;
        return SG::Verdict::Cancel;
    };
    if(!Current(row) || !call.locals || !call.invocation || layout.index<0 || layout.remove<0 ||
       layout.success<0 || layout.record<0 || layout.actor<0 || row->debit || row->takeInvocation)return refuse();
    int32_t index=-1;std::memcpy(&index,call.locals+layout.index,4);
    if(index!=row->index || call.locals[layout.remove])return refuse();
    std::vector<SR::SaveRecord> contents;
    if(!CI::ReadContents(call.object,contents,kMaxContents) || !Current(row) || index<0 ||
       size_t(index)>=contents.size() || !Same(contents[size_t(index)],row->record))return refuse();
    row->takeInvocation=call.invocation;row->takeFunction.Set(call.function);
    return SG::Verdict::Run;
}
void AfterTake(const SG::Call& call) {
    std::shared_ptr<Debt> row;
    for(const auto& [id,candidate]:g_debts)if(candidate->takeInvocation==call.invocation && candidate->source.inventory.Is(call.object)){row=candidate;break;}
    if(!row || !row->takeFunction.Is(call.function))return;
    row->takeInvocation=0;
    const auto layout=TakeLayout(call.function);
    const auto* success=Output(call,layout.success);
    if(!success || !*success)return;
    row->debit=true;
    const auto* output=Output(call,layout.record);
    if(!output){row->uncertain=true;return;}
    SR::SaveRecord actual;SR::ReadSaveRecord(output,actual);
    if(actual.className.empty() || actual.key.empty()){row->uncertain=true;return;}
    if(!Same(actual,row->record))row->refund=true;
    row->record=std::move(actual);
}
void AfterExtract(const SG::Call& call) {
    std::shared_ptr<Debt> row;
    for(const auto& [id,candidate]:g_debts)if(candidate->extractInvocation==call.invocation && candidate->source.container.Is(call.object)){row=candidate;break;}
    if(!row)return;
    row->finished=true;
    if(!row->debit){if(row->takeInvocation)row->uncertain=true;else Complete(row);return;}
    if(!Current(row) || !row->source.function.Is(call.function))return;
    const auto layout=ExtractLayout(call.function);
    if(!call.locals || layout.actor<0){row->uncertain=true;return;}
    if(row->pocketSeen && row->restored && layout.pocket>=0 && call.locals[layout.pocket]){Complete(row);return;}
    if(row->pocketSeen && row->restored && !row->pocketDone){row->uncertain=true;return;}
    void* actor=nullptr;std::memcpy(&actor,call.locals+layout.actor,sizeof(actor));
    if(!actor && layout.begun>=0) {
        void* begun=nullptr;std::memcpy(&begun,call.locals+layout.begun,sizeof(begun));
        if(begun && R::IsLive(begun))actor=begun;
    }
    if(!Attach(row,actor)){row->uncertain=true;return;}
    row->refund=true;Recover(row);
}
void Drain() {
    if(g_draining || !Context())return;
    g_draining=true;struct Guard{~Guard(){g_draining=false;}} guard;
    const auto epoch=g_epoch,now=net::NowMs();
    std::vector<std::shared_ptr<Debt>> work;
    for(size_t visited=0,total=g_debts.size();visited<total && work.size()<8;++visited) {
        auto it=g_debts.upper_bound(g_cursor);if(it==g_debts.end())it=g_debts.begin();
        if(it==g_debts.end())break;g_cursor=it->first;
        if(it->second->next<=now)work.push_back(it->second);
    }
    for(const auto& row:work) {
        if(epoch!=g_epoch || !Context())return;
        if(!Owned(row) || row->busy)continue;
        if(SG::IsInvocationActive(row->extractInvocation))continue;
        if(!row->debit){if(row->takeInvocation)row->uncertain=true;else Complete(row);continue;}
        if(!row->finished && !row->actor.Raw())row->uncertain=true;
        row->refund=true;
        if(row->uncertain && !row->saidUncertain){row->saidUncertain=true;UE_LOGW("container recovery: retained unresolved extraction %llu",static_cast<unsigned long long>(row->id));}
        Recover(row);
    }
}
}

bool InFlight() {
    if(!Context())return false;
    for(const auto& [id,row]:g_debts)
        if(row->source.epoch==g_epoch && SG::IsInvocationActive(row->extractInvocation))return true;
    return false;
}
bool Pending(void* actor) {
    if(!actor || !Context())return false;
    for(const auto& [id,row]:g_debts)if(row->source.epoch==g_epoch && row->actor.Is(actor))return true;
    return false;
}
RestoreResult RestoreForPickup(const SG::Call& call,void* actor) {
    if(!Context())return RestoreResult::Unrelated;
    for(const auto& [id,row]:g_debts)if(row->actor.Is(actor))return RestoreResult::Failed;
    auto row=ExtractFor(call.callerObject,call.callerFunction);
    if(!row)return RestoreResult::Unrelated;
    if(!Current(row) || !row->debit || row->busy || !call.callerLocals || !call.invocation)return RestoreResult::Invalidated;
    const auto layout=ExtractLayout(call.callerFunction);
    if(layout.actor<0 || layout.record<0)return RestoreResult::Failed;
    void* selected=nullptr;std::memcpy(&selected,call.callerLocals+layout.actor,sizeof(selected));
    if(selected!=actor || !Attach(row,actor))return RestoreResult::Invalidated;
    row->pocketInvocation=call.invocation;row->pocketFunction.Set(call.function);row->pocketSeen=true;
    SR::SaveRecord reported;SR::ReadSaveRecord(call.callerLocals+layout.record,reported);
    if(!Current(row) || !Same(reported,row->record)){row->refund=true;return RestoreResult::Failed;}
    if(row->refund || row->uncertain)return RestoreResult::Failed;
    row->busy=true;struct Guard{Debt& row;~Guard(){row.busy=false;}} guard{*row};
    const auto current=[&]{
        if(!Current(row) || !row->actor.Is(actor) || !row->actorType.Get() || R::ClassOf(actor)!=row->actorType.Raw() || !SG::IsInvocationActive(call.invocation) ||
           !row->source.function.Is(call.callerFunction) || !row->pocketFunction.Is(call.function))return false;
        void* candidate=nullptr;std::memcpy(&candidate,call.callerLocals+layout.actor,sizeof(candidate));
        return candidate==actor;
    };
    if(!current())return RestoreResult::Invalidated;
    auto* load=R::FindDispatchFunctionCached(R::ClassOf(actor),L"loadData");
    const bool restored=load && SR::ApplyRecordViaWhile(actor,load,row->record,current);
    if(!current()){row->refund=true;return RestoreResult::Invalidated;}
    row->restored=restored;row->refund=!restored;
    return restored?RestoreResult::Restored:RestoreResult::Failed;
}
void OnPocketPost(const SG::Call& call) {
    std::shared_ptr<Debt> row;
    for(const auto& [id,candidate]:g_debts)if(candidate->pocketInvocation==call.invocation){row=candidate;break;}
    if(!row || !row->pocketFunction.Is(call.function))return;
    row->pocketInvocation=0;
    row->pocketDone=true;
    const auto off=R::FindParamOffset(call.function,L"return");
    if(!Fits(off,1,R::FunctionFrameSize(call.function))){row->uncertain=true;return;}
    const auto* accepted=Output(call,off);
    if(accepted && *accepted && row->restored){Complete(row);return;}
    row->refund=true;
}
void OnDisconnect() {
    // A local refund can still finish while this exact session is stopping.
    // A nested install changes epoch/session and makes every old callback fail.
    const auto epoch=g_epoch;auto* session=g_session;
    if(g_settlingSession!=session || g_settlingEpoch!=epoch) {
        auto* previous=g_settlingSession;const auto previousEpoch=g_settlingEpoch;
        g_settlingSession=session;g_settlingEpoch=epoch;
        struct Guard{net::Session* session;uint64_t epoch;~Guard(){g_settlingSession=session;g_settlingEpoch=epoch;}} guard{previous,previousEpoch};
        std::vector<std::shared_ptr<Debt>> work;
        for(const auto& [id,row]:g_debts)if(row->debit && row->refund && row->finished && !row->uncertain)work.push_back(row);
        for(const auto& row:work) {
            if(g_epoch!=epoch || g_session!=session)break;
            Recover(row);
        }
    }
    if(g_epoch!=epoch || g_session!=session)return;
    ++g_epoch;g_session=nullptr;g_world=g_serial=0;
    g_debts.clear();g_cursor=0;g_extract={};g_take={};
}
void Install(net::Session* session) {
    if(!ue_wrap::game_thread::IsGameThread())return;
    if(session!=g_session || g_world!=WI::Generation() || g_serial!=net::session_serial::Current() || (session && session->role()!=g_role)) {
        const auto epoch=g_epoch;OnDisconnect();
        if(g_epoch!=epoch+1 || g_session)return;
        g_session=session;g_world=WI::Generation();g_serial=net::session_serial::Current();if(session)g_role=session->role();
    }
    if(!g_watched) {
        const bool extract=SG::WatchName(kExtract,kExtractTag,BeforeExtract,AfterExtract);
        const bool take=SG::WatchName(kTake,kTakeTag,BeforeTake,AfterTake);
        if(extract && take)g_watched=true;
        else {
            if(extract)SG::UnwatchName(kExtract,kExtractTag,BeforeExtract,AfterExtract);
            if(take)SG::UnwatchName(kTake,kTakeTag,BeforeTake,AfterTake);
        }
    }
    Drain();
}
}
