#include "coop/props/llama_sync.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/net/wire_key_util.h"
#include "coop/player/hand_item.h"
#include "coop/player/roster_ledger.h"
#include "coop/props/active_drive.h"
#include "coop/props/llama_trigger.h"
#include "coop/props/llama_timers.h"
#include "coop/props/llama_events.h"
#include "coop/props/llama_begin_restore.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/session/net_pump.h"
#include "ue_wrap/actors/llama.h"
#include "ue_wrap/actors/prop.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>
namespace coop::llama_sync {
namespace {
namespace M=ue_wrap::llama;namespace R=ue_wrap::reflection;namespace SG=ue_wrap::script_gate;
using Packet=net::LlamaStatePayload;
struct Scope {
    net::Session* session=nullptr;net::Role role{};uint32_t serial=0,generation=0,hostConnection=0;void* world=nullptr;bool active=false,linked=false;
    bool operator==(const Scope&)const=default;
};
struct Recipient {uint16_t person=0;uint32_t connection=0;uint64_t revision=0;bool quiet=false;uint32_t delivered=0;};
struct Row {
    Packet state{};ue_wrap::CachedObjRef actor,cls;uint64_t at=0,cueAt=0,expires=0;uint32_t pending=0,heard=0;
    std::array<Recipient,net::kMaxPeers> recipients{};bool expired=false;uint8_t sendSlot=1;
};
struct Capture {ue_wrap::CachedObjRef actor,cls,function;void* stack=nullptr;uint8_t* parameters=nullptr;std::wstring key;uint64_t invocation=0,epoch=0,order=0;
    void* world=nullptr;uint32_t generation=0;uint64_t beginTicket=0;bool ready=false,begin=false,original=false,parked=false,finished=false;};
net::Session* g_session=nullptr;Scope g_scope;uint64_t g_epoch=0,g_revision=0,g_poll=0,g_now=0;
uint32_t g_sequence=0,g_pulse=0;bool g_ticking=false,g_restoring=false,g_clientPolled=false;uint64_t g_clientAt=0;size_t g_sendRow=0;
std::unordered_map<std::wstring,Row> g_rows;std::unordered_map<std::wstring,uint64_t> g_samples;
std::vector<Capture> g_captures,g_restores;
constexpr wchar_t kClass[]=L"prop_llama_C",kGraph[]=L"ExecuteUbergraph_prop_llama";constexpr int kTag=0x4C4C414D;
uint32_t Next(uint32_t& n){if(!++n)++n;return n;}
bool After(uint32_t a,uint32_t b){return a&&(!b||int32_t(a-b)>0);}
Scope CaptureScope(net::Session* s){Scope scope;scope.session=s;scope.serial=net::session_serial::Current();
    scope.generation=ue_wrap::world_identity::Generation();scope.world=ue_wrap::world_identity::CurrentWorld();
    if(s){scope.role=s->role();scope.hostConnection=scope.role==net::Role::Client?s->peerGenerationForSlot(0):0;
        scope.linked=s->connected();scope.active=s->running()&&scope.world;}return scope;}
bool Current(){return g_scope.active&&g_scope==CaptureScope(g_session);}
bool Refresh(){if(!(g_scope==CaptureScope(g_session)))Install(g_session);return Current();}
bool KeyValid(const std::wstring& key){return !key.empty()&&key!=L"None"&&key.size()<=sizeof(Packet{}.key.data)&&
    std::all_of(key.begin(),key.end(),[](wchar_t c){return c>=33&&c<=126;});}
std::wstring Key(void* actor){auto key=ue_wrap::prop::GetInteractableKeyString(actor);return KeyValid(key)?key:std::wstring{};}
bool Eligible(void* actor){return M::IsLlama(actor)&&!hand_item::IsHandAxisActor(actor)&&ue_wrap::world_identity::CurrentWorld()&&
    ue_wrap::world_identity::WorldOf(actor)==ue_wrap::world_identity::CurrentWorld();}
bool Source(const ue_wrap::CachedObjRef& actor,const ue_wrap::CachedObjRef& cls,const std::wstring& key,uint64_t epoch){
    if(epoch!=g_epoch||!Current()||!actor.Get()||!cls.Get()||R::ClassOf(actor.Get())!=cls.Get()||!Eligible(actor.Get()))return false;
    return Key(actor.Get())==key&&epoch==g_epoch&&Current()&&actor.Get()&&cls.Get()&&R::ClassOf(actor.Get())==cls.Get()&&Eligible(actor.Get());
}
bool SameOwner(const Capture& a,const Capture& b){return a.actor.Raw()==b.actor.Raw()&&a.actor.Is(b.actor.Get())&&a.cls.Raw()==b.cls.Raw()&&a.cls.Is(b.cls.Get())&&a.world==b.world&&a.generation==b.generation;}
bool RestoreOwner(const Capture& c){return c.world&&c.world==ue_wrap::world_identity::CurrentWorld()&&c.generation==ue_wrap::world_identity::Generation()&&
    c.actor.Get()&&c.cls.Get()&&R::ClassOf(c.actor.Get())==c.cls.Get();}
bool RetiredOwner(const Capture& c){const auto world=ue_wrap::world_identity::CurrentWorld();return !c.actor.Get()||!c.cls.Get()||R::ClassOf(c.actor.Get())!=c.cls.Get()||
    c.generation!=ue_wrap::world_identity::Generation()||(world&&c.world!=world);}
void QueueRestore(Capture capture,bool finished=false){
    if(!capture.begin||!capture.parked)return;capture.finished=finished;
    const auto found=std::find_if(g_restores.begin(),g_restores.end(),[&](const Capture& c){return c.order==capture.order;});
    if(found==g_restores.end())g_restores.push_back(capture);else found->finished=found->finished||finished;
}
void RetryRestores(){
    if(g_restoring)return;struct Guard{Guard(){g_restoring=true;}~Guard(){g_restoring=false;}}guard;
    std::erase_if(g_restores,[](const Capture& c){return RetiredOwner(c);});const auto debts=g_restores;
    for(const auto& debt:debts){
        if(!RestoreOwner(debt))continue;auto owner=debt;uint64_t last=debt.order;bool blocked=false,found=false;
        for(const auto& c:g_restores)if(SameOwner(c,debt)){found=true;last=std::max(last,c.order);if(c.order<owner.order)owner=c;
            if(!c.finished&&SG::IsInvocationActive(c.invocation))blocked=true;}
        for(const auto& c:g_captures)if(c.begin&&SameOwner(c,debt)){if(c.order<owner.order)owner=c;blocked=true;}
        if(!found||blocked||!RestoreOwner(owner))continue;
        const auto current=[owner]{return RestoreOwner(owner);};
        if(M::RestoreBegin(owner.actor.Get(),owner.original,current)&&current())
            std::erase_if(g_restores,[&](const Capture& c){return c.order<=last&&SameOwner(c,owner);});
    }
}
void SweepCaptures(){
    std::vector<Capture> retired;
    std::erase_if(g_captures,[&](const Capture& c){const bool drop=c.epoch!=g_epoch||!c.actor.Get()||!c.cls.Get()||
        (!c.begin&&!c.function.Get())||!SG::IsInvocationActive(c.invocation);if(drop)retired.push_back(c);return drop;});
    for(const auto& c:retired)QueueRestore(c,!SG::IsInvocationActive(c.invocation));RetryRestores();
}
void ObserveClock(uint64_t now){
    if(now<g_now){g_poll=0;g_clientPolled=false;for(auto& [key,row]:g_rows){row.at=now;row.expires=now;row.state.cue=0;row.state.ttlMs=0;row.heard=row.state.pulse;}}
    g_now=now;
}
uint64_t Deadline(uint64_t now,uint16_t duration){return now>std::numeric_limits<uint64_t>::max()-duration?std::numeric_limits<uint64_t>::max():now+duration;}
bool RecipientCurrent(int slot,const Recipient& r){return r.person&&g_session->IsSlotConnected(slot)&&r.person==roster_ledger::Get(slot).playerNo&&r.connection&&r.connection==g_session->peerGenerationForSlot(slot);}
void Queue(Row& row,int slot,bool quiet,bool preserve=false){
    if(slot<=0||slot>=net::kMaxPeers||!g_session->IsSlotConnected(slot))return;
    const auto person=roster_ledger::Get(slot).playerNo;const auto connection=g_session->peerGenerationForSlot(slot);if(!person||!connection)return;
    const auto previous=row.recipients[slot];const bool same=previous.person==person&&previous.connection==connection;
    row.recipients[slot]={person,connection,++g_revision,quiet||(preserve&&same&&previous.quiet),same?previous.delivered:0};row.pending|=uint32_t{1}<<slot;
}
void Sample(void* actor,bool jump=false){
    if(!Current()||g_scope.role!=net::Role::Host||!Eligible(actor))return;const auto epoch=g_epoch;
    if(jump)llama_begin_restore::Consumed(actor);
    ue_wrap::CachedObjRef live,cls;live.Set(actor);cls.Set(R::ClassOf(actor));const auto key=Key(actor);
    if(key.empty()||!Source(live,cls,key,epoch)||(g_rows.size()>=256&&!g_rows.contains(key)))return;
    const uint64_t revision=++g_revision;g_samples[key]=revision;bool active=false;
    const bool read=M::Read(actor,active);
    if(!Source(live,cls,key,epoch)){
        if(epoch==g_epoch){auto it=g_samples.find(key);if(it!=g_samples.end()&&it->second==revision)g_samples.erase(it);}return;
    }
    const auto sample=g_samples.find(key);if(sample==g_samples.end()||sample->second!=revision)return;
    if(!read){g_samples.erase(key);return;}
    const auto now=active_drive::NowMs();auto it=g_rows.find(key);
    const bool fresh=it==g_rows.end()||!it->second.actor.Is(actor)||!it->second.cls.Is(cls.Get());
    if(!jump&&!fresh&&it->second.state.active==uint8_t(active)&&now>=it->second.at&&now-it->second.at<1000)return;
    Row row=it==g_rows.end()?Row{}:it->second;row.actor=live;row.cls=cls;row.at=now;
    row.state.sequence=Next(g_sequence);
    net::WireKeyFromString(key,row.state.key);
    if(fresh||jump)row.state.pulse=Next(g_pulse);
    const bool retain=!fresh&&!jump&&active&&row.state.cue&&now>=row.cueAt&&now-row.cueAt<1500;
    row.state.active=uint8_t(active);row.state.cue=uint8_t((jump&&active)||retain);row.state.ttlMs=row.state.cue?1500:0;
    if(fresh||jump)row.cueAt=now;row.pending=0;if(fresh||jump)row.recipients={};
    for(int slot=1;slot<net::kMaxPeers;++slot)Queue(row,slot,false,!fresh&&!jump);
    g_rows[key]=row;
}
bool Valid(const Packet& p){return p.sequence&&p.pulse&&p.active<=1&&p.cue<=1&&p.ttlMs<=1500&&
    (p.cue?(p.active&&p.ttlMs):!p.ttlMs)&&p.key.len&&p.key.len<=sizeof(p.key.data)&&KeyValid(net::StringFromWireKey(p.key));}
void SendRows(uint64_t epoch){
    std::vector<std::wstring> keys;keys.reserve(g_rows.size());for(const auto& [key,row]:g_rows)keys.push_back(key);std::sort(keys.begin(),keys.end());
    if(keys.empty())return;const size_t start=g_sendRow%keys.size();g_sendRow=(start+1)%keys.size();size_t budget=64;
    for(size_t offset=0;offset<keys.size()&&budget;++offset){const size_t index=(start+offset)%keys.size();const auto& key=keys[index];
        auto it=g_rows.find(key);if(it==g_rows.end())continue;const auto row=it->second;
        if(!Source(row.actor,row.cls,key,epoch)){if(epoch!=g_epoch||!Current())return;it=g_rows.find(key);if(it!=g_rows.end()&&it->second.state.sequence==row.state.sequence){g_rows.erase(it);g_samples.erase(key);}continue;}
        int refused=0;const int first=std::clamp<int>(row.sendSlot,1,net::kMaxPeers-1);
        for(int turn=0;turn<net::kMaxPeers-1&&budget&&refused<2;++turn){const int slot=1+(first-1+turn)%(net::kMaxPeers-1);const auto bit=uint32_t{1}<<slot;
            if(!(row.pending&bit))continue;const auto target=row.recipients[slot];
            const auto debtCurrent=[&](const Row& live){return live.state.sequence==row.state.sequence&&(live.pending&bit)&&live.recipients[slot].revision==target.revision;};
            it=g_rows.find(key);if(it==g_rows.end()||!debtCurrent(it->second))break;
            if(!RecipientCurrent(slot,target)){it->second.pending&=~bit;continue;}
            if(!g_session->IsSlotWorldReady(slot)||!g_session->IsSlotReady(slot))continue;
            if(!Source(row.actor,row.cls,key,epoch)){if(epoch!=g_epoch||!Current())return;break;}
            // Source/key reads can run callbacks. Revalidate the complete recipient/debt admission afterward.
            it=g_rows.find(key);if(it==g_rows.end()||!debtCurrent(it->second))break;
            if(!RecipientCurrent(slot,target)){it->second.pending&=~bit;continue;}
            if(!g_session->IsSlotWorldReady(slot)||!g_session->IsSlotReady(slot))continue;
            Packet outgoing=row.state;const auto sendNow=active_drive::NowMs();
            const uint64_t age=sendNow>=row.cueAt&&sendNow>=g_now?sendNow-row.cueAt:1500;
            if(target.quiet||target.delivered==outgoing.pulse||!outgoing.cue||age>=1500){outgoing.cue=0;outgoing.ttlMs=0;}else outgoing.ttlMs=uint16_t(1500-age);
            --budget;const bool sent=g_session->SendReliableToSlot(slot,net::ReliableKind::LlamaState,&outgoing,sizeof(outgoing));
            if(epoch!=g_epoch||!Current())return;it=g_rows.find(key);
            // A nested same-pulse heartbeat can replace the state revision while
            // transport admits these bytes. Remember its delivered pulse without
            // acknowledging the newer state/debt revision.
            if(sent&&it!=g_rows.end()&&it->second.state.pulse==row.state.pulse&&RecipientCurrent(slot,target)){
                auto& recipient=it->second.recipients[slot];
                if(recipient.person==target.person&&recipient.connection==target.connection)recipient.delivered=row.state.pulse;
            }
            if(it!=g_rows.end()&&it->second.state.sequence==row.state.sequence){it->second.sendSlot=uint8_t(slot%(net::kMaxPeers-1)+1);
                if(sent&&debtCurrent(it->second)&&RecipientCurrent(slot,target)){it->second.pending&=~bit;it->second.recipients[slot].delivered=row.state.pulse;}}
            if(!sent)++refused;
        }
    }
}
}
void Install(net::Session* session){
    const auto scope=CaptureScope(session);if(!(g_scope==scope)||g_session!=session){
        const auto epoch=g_epoch;OnDisconnect();
        // Native restoration can install a newer session while teardown unwinds.
        if(g_epoch!=epoch+1||g_session)return;g_session=session;g_scope=CaptureScope(session);
    }
    const auto epoch=g_epoch;llama_begin_restore::Install(session);
    if(epoch!=g_epoch||g_session!=session)return;llama_events::Install(session);
    if(epoch!=g_epoch||g_session!=session)return;llama_timers::Install(session);
    if(epoch!=g_epoch||g_session!=session)return;
    SG::WatchClassName(kClass,kGraph,kTag,&BeforeGraph,&AfterGraph);SG::ResolvePendingNames();
}
SG::Verdict BeforeGraph(const SG::Call& call){
    SweepCaptures();
    if(!Refresh()||!call.object||!Eligible(call.object))return SG::Verdict::Run;
    const auto epoch=g_epoch;ue_wrap::CachedObjRef actor,cls;actor.Set(call.object);cls.Set(R::ClassOf(call.object));
    const auto current=[&]{return epoch==g_epoch&&Current()&&actor.Get()&&cls.Get()&&R::ClassOf(actor.Get())==cls.Get()&&Eligible(actor.Get());};
    int32_t entry=0;const bool known=M::Entry(call.function,call.locals,entry);
    if(!current())return SG::Verdict::Cancel;
    if(g_scope.role==net::Role::Client){
        // Keep prop_C.ReceiveBeginPlay/key/physics initialization, but do not arm this copy's own jump timer.
        if(!known||entry!=1146||!call.invocation||!call.function||g_captures.size()>=32||g_restores.size()>=128||
            std::any_of(g_restores.begin(),g_restores.end(),[&](const Capture& c){return c.actor.Is(call.object)&&c.cls.Is(cls.Get());})||
            std::any_of(g_captures.begin(),g_captures.end(),[&](const Capture& c){return c.invocation==call.invocation;}))return SG::Verdict::Cancel;
        Capture capture;capture.actor=actor;capture.cls=cls;capture.function.Set(call.function);capture.stack=call.stack;capture.parameters=call.locals;
        capture.invocation=call.invocation;capture.epoch=epoch;capture.order=++g_revision;capture.world=ue_wrap::world_identity::CurrentWorld();
        capture.generation=ue_wrap::world_identity::Generation();capture.begin=true;g_captures.push_back(capture);
        capture.parked=M::ParkBegin(call.object,capture.original,current);capture.ready=capture.parked&&current();
        if(capture.ready)capture.ready=llama_begin_restore::Prepare(call.object,capture.original,capture.invocation,current,capture.beginTicket)&&current();
        const auto found=std::find_if(g_captures.begin(),g_captures.end(),[&](const Capture& c){return c.invocation==capture.invocation&&c.epoch==epoch;});
        if(found==g_captures.end()){llama_begin_restore::Cancel(capture.beginTicket);QueueRestore(capture,true);RetryRestores();return SG::Verdict::Cancel;}
        if(!capture.ready){g_captures.erase(found);
            llama_begin_restore::Cancel(capture.beginTicket);
            QueueRestore(capture,true);RetryRestores();
            return SG::Verdict::Cancel;
        }*found=capture;llama_begin_restore::Commit(capture.beginTicket);return SG::Verdict::Run;
    }
    if(known&&(entry==1264||entry==1269)){
        if(llama_trigger::BlocksNative(call.object)||!current())return SG::Verdict::Cancel;
        if(entry==1269&&(!llama_trigger::AllowLocal(*g_session,call.object,current)||!current()))return SG::Verdict::Cancel;
        if(entry==1264)llama_begin_restore::Consumed(call.object);
    }
    if(!known||entry!=1264||call.fromOurCode||!call.invocation||!call.function)return SG::Verdict::Run;
    if(g_captures.size()>=32||std::any_of(g_captures.begin(),g_captures.end(),[&](const Capture& c){return c.invocation==call.invocation;}))return SG::Verdict::Run;
    Capture capture;capture.actor=actor;capture.cls=cls;capture.function.Set(call.function);capture.stack=call.stack;capture.parameters=call.locals;
    capture.invocation=call.invocation;capture.epoch=epoch;g_captures.push_back(capture);
    capture.key=Key(call.object);capture.ready=current()&&KeyValid(capture.key)&&Source(actor,cls,capture.key,epoch);
    const auto found=std::find_if(g_captures.begin(),g_captures.end(),[&](const Capture& c){return c.invocation==capture.invocation&&c.epoch==epoch;});
    if(found!=g_captures.end()){if(capture.ready)*found=capture;else g_captures.erase(found);}return SG::Verdict::Run;
}
void AfterGraph(const SG::Call& call){
    const auto matches=[&](const Capture& c){return c.invocation==call.invocation&&c.stack==call.stack&&
        c.parameters==call.locals&&c.function.Raw()==call.function&&c.actor.Raw()==call.object;};
    const auto found=std::find_if(g_captures.begin(),g_captures.end(),matches);
    if(found==g_captures.end()){
        const auto debt=std::find_if(g_restores.begin(),g_restores.end(),matches);const bool begin=debt!=g_restores.end()&&debt->begin;
        if(debt!=g_restores.end())debt->finished=true;RetryRestores();if(begin)llama_begin_restore::Finished(call.invocation);return;
    }const auto capture=*found;g_captures.erase(found);
    if(capture.begin){
        QueueRestore(capture,true);RetryRestores();llama_begin_restore::Finished(call.invocation);return;
    }
    if(capture.ready&&g_scope.role==net::Role::Host&&SG::IsInvocationActive(capture.invocation)&&capture.function.Get()&&
        Source(capture.actor,capture.cls,capture.key,capture.epoch)){
        const auto current=[actor=capture.actor,cls=capture.cls,key=capture.key,epoch=capture.epoch]{return Source(actor,cls,key,epoch);};
        llama_trigger::QueueNativeMotion(*g_session,call.object,current);
        if(Source(capture.actor,capture.cls,capture.key,capture.epoch))Sample(call.object,true);
    }
}
void Tick(){
    if(g_ticking)return;struct Guard{Guard(){g_ticking=true;}~Guard(){g_ticking=false;}}guard;SweepCaptures();
    const bool active=Refresh();llama_timers::Tick();llama_events::Tick();llama_begin_restore::Tick();if(!active||!Current())return;
    const auto epoch=g_epoch,now=active_drive::NowMs();
    if(g_scope.role==net::Role::Host){llama_trigger::TickMotion(*g_session);if(epoch!=g_epoch||!Current())return;}
    ObserveClock(now);
    if(g_scope.role==net::Role::Client){if(g_clientPolled&&now>=g_clientAt&&now-g_clientAt<100)return;g_clientAt=now;g_clientPolled=true;}
    if(g_scope.role==net::Role::Host&&now>=g_poll){
        g_poll=now+100;std::array<ue_wrap::CachedObjRef,256> actors{};const auto count=M::Collect(actors.data(),actors.size());
        if(epoch!=g_epoch||!Current())return;for(size_t i=0;i<count;++i){
            void* actor=actors[i].Get();ue_wrap::CachedObjRef cls;cls.Set(actor?R::ClassOf(actor):nullptr);const auto key=actor?Key(actor):std::wstring{};
            const auto current=[actorRef=actors[i],cls,key,epoch]{return !key.empty()&&Source(actorRef,cls,key,epoch);};
            const bool jumped=current()&&llama_trigger::TryRemote(*g_session,actor,current);
            if(epoch!=g_epoch||!Current())return;if(current())Sample(actor,jumped);if(epoch!=g_epoch||!Current())return;
        }
    }
    if(g_scope.role==net::Role::Host){SendRows(epoch);return;}
    const auto rows=g_rows;
    for(const auto& [key,row]:rows){
            if(row.expired)continue;
            if(now>=row.at&&now-row.at>10000){auto it=g_rows.find(key);if(it!=g_rows.end()&&it->second.state.sequence==row.state.sequence){
                it->second.expired=true;it->second.actor.Reset();it->second.cls.Reset();it->second.state.cue=0;it->second.state.ttlMs=0;it->second.heard=row.state.pulse;}continue;}
            if(!g_session->IsSlotWorldReady(0)||!g_session->IsSlotReady(0))continue;
            void* actor=prop_element_tracker::FindLiveActorByKey(key);
            if(epoch!=g_epoch||!Current())return;if(!Eligible(actor)||!net_pump::IsInAnnouncedWorld(actor))continue;
            ue_wrap::CachedObjRef live,cls;live.Set(actor);cls.Set(R::ClassOf(actor));
            const auto valid=[&]{if(!Source(live,cls,key,epoch)||prop_element_tracker::FindLiveActorByKey(key)!=live.Get()||!net_pump::IsInAnnouncedWorld(live.Get()))return false;
                const auto it=g_rows.find(key);return epoch==g_epoch&&Current()&&it!=g_rows.end()&&it->second.state.sequence==row.state.sequence;};
            if(!valid()||!llama_events::Prepare(actor,valid))continue;const bool cue=row.state.cue&&row.state.active&&row.heard!=row.state.pulse&&now<row.expires;
            const auto applyCurrent=[&]{return valid()&&(!cue||(active_drive::NowMs()>=g_now&&active_drive::NowMs()<row.expires));};
            bool dispatched=false;M::Apply(actor,row.state.active!=0,cue,dispatched,applyCurrent);
            if(epoch!=g_epoch||!Current())return;auto it=g_rows.find(key);if(it==g_rows.end())continue;
            if(dispatched&&it->second.state.pulse==row.state.pulse)it->second.heard=row.state.pulse;
            if(it->second.state.sequence==row.state.sequence){it->second.actor=live;it->second.cls=cls;}
    }
}
void Receive(const Packet& incoming,uint8_t sender){
    const Packet p=incoming;if(!Refresh()||g_scope.role!=net::Role::Client||!g_session->connected()||sender!=0||!Valid(p))return;
    const auto now=active_drive::NowMs();ObserveClock(now);const auto key=net::StringFromWireKey(p.key);auto it=g_rows.find(key);
    if(it==g_rows.end()&&g_rows.size()>=256){
        auto oldest=g_rows.end();for(auto candidate=g_rows.begin();candidate!=g_rows.end();++candidate)if(candidate->second.expired&&
            (oldest==g_rows.end()||candidate->second.at<oldest->second.at))oldest=candidate;
        if(oldest==g_rows.end())return;g_rows.erase(oldest);
    }
    if(it!=g_rows.end()&&(!After(p.sequence,it->second.state.sequence)||(p.pulse!=it->second.state.pulse&&!After(p.pulse,it->second.state.pulse))))return;
    auto& row=g_rows[key];const bool same=row.state.pulse==p.pulse;const auto expires=Deadline(now,p.ttlMs);
    row.expires=same?std::min(row.expires,expires):expires;row.state=p;row.at=now;row.expired=false;
    if(!p.cue)row.heard=p.pulse;g_clientPolled=false;g_poll=0;
}
void QueueConnect(int slot){if(!Refresh()||g_scope.role!=net::Role::Host)return;for(auto& [key,row]:g_rows)Queue(row,slot,true);g_poll=0;}
void OnPeerLeft(int slot){if(slot<=0||slot>=net::kMaxPeers)return;for(auto& [key,row]:g_rows){row.pending&=~(uint32_t{1}<<slot);row.recipients[slot]={};}++g_revision;}
void OnDisconnect(){++g_epoch;llama_trigger::Reset();const auto captures=g_captures;g_captures.clear();for(const auto& c:captures)QueueRestore(c);
    g_session=nullptr;g_scope={};g_rows.clear();g_samples.clear();g_sequence=g_pulse=0;g_poll=g_now=g_clientAt=0;g_sendRow=0;g_clientPolled=false;RetryRestores();
    llama_events::OnDisconnect();llama_timers::OnDisconnect();llama_begin_restore::OnDisconnect();}
}
