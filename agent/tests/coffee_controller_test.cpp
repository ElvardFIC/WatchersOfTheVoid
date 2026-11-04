#ifndef COFFEE_STATE_SOURCE
#define COFFEE_STATE_SOURCE "../../src/votv-coop/src/coop/interactables/coffee_state.cpp"
#endif
#include COFFEE_STATE_SOURCE
#ifndef COFFEE_INTENT_SOURCE
#define COFFEE_INTENT_SOURCE "../../src/votv-coop/src/coop/interactables/coffee_intent.cpp"
#endif
#include COFFEE_INTENT_SOURCE
#include "facts_test_support.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <set>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int id=0,serial=1;bool live=true;void* cls=nullptr;ue_wrap::coffee::MakerFlags flags{};int amounts=4;std::wstring key;} obj[512];
coop::net::Session session, replacement;
coop::players::RemotePlayer puppet;
uint32_t world=1,serial=1;
int offAction=0,offOther=8,frame=16,records=0,legacyRecords=0,actions=0,reads=0,watchCalls=0,gateCalls=0,keyReads=0,pubAttempts=0;
bool sendOk=true,awaited=true,watchOk=true;
void* indexed=nullptr;
coop::element::IntentOutcome reach=coop::element::IntentOutcome::Ok;
std::function<void()> onKey,onFlags,onAmounts,onFind,onReach,onPrepare,onAction,onPublish,onAwait,onMeta;
std::set<uint64_t> active;
void Fire(std::function<void()>& f){auto cb=std::move(f);f={};if(cb)cb();}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<512&&p==&test::obj[i]&&IsLive(p);}
int32_t SlotSerial(int32_t i){return i>=0&&i<512?test::obj[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void* p){return p?static_cast<test::Obj*>(p)->cls:nullptr;}
std::wstring ClassNameOf(void* p){return L"class"+std::to_wstring(static_cast<test::Obj*>(ClassOf(p))->serial);}
int32_t ParamOffset::Of(void*){test::Fire(test::onMeta);return !wcscmp(param_,L"action")?test::offAction:test::offOther;}
int32_t FunctionFrameSize(void*){test::Fire(test::onMeta);return test::frame;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* p){return static_cast<test::Obj*>(p)->id<250?CurrentWorld():nullptr;}
}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}uint32_t Next(){return ++test::serial;}}
namespace coop::config_registry::detail {
struct RegistryDef {static constexpr auto Key(){return RegistryCtorKey{};}};
}
namespace coop::config_registry::rows {
const EnumRow coffee_red{nullptr,detail::RegistryDef::Key()};
const FlagRow coffee_drill{nullptr,detail::RegistryDef::Key()};
}
namespace coop::config {
std::string ResolveEnum(const config_registry::EnumRow&){return "";}
bool ResolveFlag(const config_registry::FlagRow&){return true;}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::script_gate {
bool WatchName(const facts::Function&,int,PreFn,PostFn){++test::watchCalls;return test::watchOk;}
void ResolvePendingNames(){}
bool NameWatchLive(const facts::Function&,int){return test::watchOk;}
int PendingNameCount(){return 0;}
bool IsInvocationActive(uint64_t i){return test::active.count(i)!=0;}
}
namespace coop::roster_ledger {void RegisterPerSlotClear(PerSlotClearFn,void*){}}
namespace coop::prop_record_gates {void RegisterIntentGate(const wchar_t*,IntentGate){++test::gateCalls;}}
namespace ue_wrap::prop {
std::wstring GetKeyString(void* p){++test::keyReads;auto key=static_cast<test::Obj*>(p)->key;test::Fire(test::onKey);return key;}
bool IsDescendantOfProp(void*){return true;}
}
namespace ue_wrap::coffee {
Device DeviceOf(void* p){return p==&test::obj[1]?Device::Maker:Device::None;}
bool IsBag(void* p){return p==&test::obj[2];}
bool ReadMakerFlags(void* p,MakerFlags& out){++test::reads;out=static_cast<test::Obj*>(p)->flags;test::Fire(test::onFlags);return true;}
bool ReadBagAmounts(void* p,int32_t& out){++test::reads;out=static_cast<test::Obj*>(p)->amounts;test::Fire(test::onAmounts);return true;}
}
namespace coop::prop_key_index {void* Find(const std::wstring&){auto* p=test::indexed;test::Fire(test::onFind);return p;}}
namespace coop::element {
IntentTarget IntentTarget::ForClientIntent(net::Session& s,uint8_t slot,float r){IntentTarget t;t.session_=&s;t.slot_=slot;t.reachUU_=r;return t;}
IntentSubject IntentTarget::Authorize(void* p)const{test::Fire(test::onReach);return {test::reach,p,slot_};}
const char* OutcomeName(IntentOutcome){return "fixture";}
}
namespace ue_wrap::appliance {
bool CallActionWhile(void* actor,void* player,uint8_t action,const std::function<bool()>& current){
    CHECK(actor==&test::obj[1]&&player==&test::obj[3]&&action==4);
    test::Fire(test::onPrepare);if(!current())return false;
    ++test::actions;test::Fire(test::onAction);return current();
}
}
namespace coop::prop_save_data {
bool Publish(net::Session*,void*,const std::wstring&){++test::legacyRecords;return true;}
bool PublishWhile(net::Session*,void*,const std::wstring&,const std::function<bool()>& current){
    ++test::pubAttempts;if(!current())return false;test::Fire(test::onPublish);if(!current())return false;
    if(!test::sendOk)return false;++test::records;return true;
}
bool AwaitsRecordFrom(const std::wstring&,uint8_t){test::Fire(test::onAwait);return test::awaited;}
}
namespace test {
namespace S=coop::coffee_state;namespace I=coop::coffee_intent;namespace SG=ue_wrap::script_gate;
uint8_t frameData[32];
SG::Call Call(uint64_t invocation=1,bool hit=false){
    SG::Call c{};c.object=&obj[1];c.function=&obj[hit?251:250];c.locals=frameData;c.invocation=invocation;
    c.tag=hit?S::kTagHit:I::kWatches[1].tag;return c;
}
void Reset(){
    S::OnDisconnect();I::OnDisconnect();++world;++serial;
    for(int i=0;i<512;++i){obj[i].id=i;++obj[i].serial;obj[i].live=true;obj[i].cls=&obj[300];obj[i].flags={};obj[i].key=L"K"+std::to_wstring(i);obj[i].amounts=4;}
    session={};replacement={};session.generations.fill(1);session.online.fill(true);session.ready.fill(true);
    replacement=session;puppet.actor=&obj[3];puppet.posed=true;
    coop::players::Registry::Get().puppets.fill(&puppet);coop::players::Registry::Get().life.fill(coop::players::PuppetLife::Alive);
    records=legacyRecords=actions=reads=keyReads=pubAttempts=0;offAction=0;offOther=8;frame=16;
    sendOk=awaited=true;indexed=&obj[1];reach=coop::element::IntentOutcome::Ok;
    onKey=onFlags=onAmounts=onFind=onReach=onPrepare=onAction=onPublish=onAwait=onMeta={};active.clear();
    std::fill(std::begin(frameData),std::end(frameData),uint8_t(0));frameData[0]=4;
    void* bag=&obj[2];std::memcpy(frameData+8,&bag,sizeof(bag));
    S::Install(&session);I::Install(&session);
}
void Pre(SG::Call c,bool hit=false){active.insert(c.invocation);if(hit)S::OnHitPre(c);else S::OnBrewPre(c);}
void Post(SG::Call c,bool hit=false){if(hit)S::OnHitPost(c);else S::OnBrewPost(c);active.erase(c.invocation);}
void Changed(){obj[1].flags.hasCup=!obj[1].flags.hasCup;}
void Brew(uint64_t id=1){auto c=Call(id);Pre(c);Changed();Post(c);}
void Queue(int slot=1){coop::net::CoffeeActionIntentPayload p{};coop::net::WireKeyFromString(obj[1].key,p.key);I::OnAction(session,p,uint8_t(slot));}
size_t Debts(){return std::count_if(S::g_debt.begin(),S::g_debt.end(),[](const auto& d){return d.revision!=0;});}
void Retry(){for(auto& d:S::g_debt)d.nextMs=0;S::Tick();}
void StateTests(){
    Reset();auto outer=Call(1),inner=Call(2);Pre(outer);Pre(inner);Changed();Post(inner);Post(outer);CHECK(records==2&&legacyRecords==0);
    Reset();auto c=Call();Pre(c);Changed();auto wrong=c;wrong.invocation=99;Post(wrong);CHECK(records==0);Post(c);CHECK(records==1);Post(c);CHECK(records==1);
    for(int kind=0;kind<7;++kind){Reset();c=Call();Pre(c);Changed();switch(kind){
        case 0:++obj[1].serial;break;case 1:++obj[250].serial;break;
        case 2:S::Install(&replacement);break;case 3:++serial;break;case 4:++world;break;
        case 5:session.mode=coop::net::Role::Client;break;case 6:++session.stops;break;}
        const int beforeReads=reads;Post(c);CHECK(records==0&&reads==beforeReads);
    }
    Reset();onFlags=[] {++obj[1].serial;};Brew();CHECK(records==0);
    Reset();c=Call();Pre(c);Changed();onFlags=[] {++obj[1].serial;};Post(c);CHECK(records==0);
    Reset();c=Call();Pre(c);Changed();onKey=[] {++obj[1].serial;};Post(c);CHECK(records==0&&Debts()==0);
    Reset();c=Call();Pre(c);Changed();onPublish=[c] {Post(c);};Post(c);CHECK(records==1);
    Reset();c=Call(1,true);Pre(c,true);Changed();--obj[2].amounts;onPublish=[] {S::Install(&replacement);};Post(c,true);CHECK(records==0&&S::g_madeMaker==0&&S::g_madeBag==0);
    Reset();c=Call(1,true);onAmounts=[] {++obj[2].serial;};Pre(c,true);--obj[2].amounts;Post(c,true);CHECK(records==0);
    Reset();c=Call(1,true);Pre(c,true);--obj[2].amounts;Post(c,true);CHECK(records==1&&S::g_madeBag==1);
    Reset();c=Call(1,true);Pre(c,true);Changed();--obj[2].amounts;
    onPublish=[] {auto nested=Call(2,true);Pre(nested,true);Changed();--obj[2].amounts;Post(nested,true);};
    Post(c,true);CHECK(Debts()>0);Retry();CHECK(records>=2&&Debts()==0);
    Reset();sendOk=false;Brew();CHECK(records==0&&Debts()==1&&legacyRecords==0);sendOk=true;Retry();CHECK(records==1&&Debts()==0);
    Reset();onPublish=[] {obj[1].key=L"rekeyed";};Brew();CHECK(records==0&&Debts()==0);
    Reset();onPublish=[] {++world;};Brew();CHECK(records==0&&S::g_madeMaker==0);Retry();CHECK(Debts()==0);
    for(int kind=0;kind<5;++kind){Reset();sendOk=false;Brew();sendOk=true;switch(kind){
        case 0:++obj[1].serial;break;case 1:obj[1].key=L"new";break;case 2:++serial;break;case 3:++world;break;case 4:S::Install(&replacement);break;}
        Retry();CHECK(records==0&&Debts()==0);
    }
    Reset();sendOk=false;Brew();S::OnDisconnect();Brew(2);Retry();CHECK(records==0&&Debts()==0&&S::g_session.load()==nullptr);
    Reset();offOther=frame;auto before=reads;c=Call(1,true);Pre(c,true);CHECK(reads==before+1);Changed();Post(c,true);CHECK(records==1); // maker only
    Reset();c=Call();c.locals=nullptr;Pre(c);Changed();Post(c);CHECK(records==0);
    Reset();offAction=frame;Brew();CHECK(records==0);
    Reset();CHECK(S::AcceptsClientRecord(&obj[1],31));CHECK(!S::AcceptsClientRecord(&obj[1],32));CHECK(!S::AcceptsClientRecord(&obj[1],0));
    for(int kind=0;kind<6;++kind){Reset();switch(kind){case 0:session.ready[1]=false;break;case 1:session.online[1]=false;break;case 2:session.generations[1]=0;break;case 3:session.mode=coop::net::Role::Client;break;case 4:onKey=[] {++obj[1].serial;};break;case 5:onAwait=[] {++session.generations[1];};break;}CHECK(!S::AcceptsClientRecord(&obj[1],1));}
    // Refused records retain debt, refuse overflow without evicting an owner, and retry at most eight per tick.
    Reset();sendOk=false;auto owner=S::Capture(Call(),S::kTagBrew);
    for(int i=1;i<=129;++i){ue_wrap::CachedObjRef actor;actor.Set(&obj[i]);S::PublishRecord(owner,actor,false);}
    CHECK(Debts()==128&&pubAttempts==128&&S::g_publishFailed==129);
    sendOk=true;Retry();CHECK(records==8&&Debts()==120);++serial;Retry();CHECK(Debts()==0);
    // Cancelled PRE notes are reclaimable; active ancestor notes survive a later PRE.
    Reset();for(int i=1;i<200;++i){auto cancelled=Call(i);Pre(cancelled);active.erase(i);}
    CHECK(std::count_if(S::g_notes.begin(),S::g_notes.end(),[](auto& n){return n.invocation!=0;})==1);
    Reset();session.mode=coop::net::Role::Client;S::Install(&session);
    for(int i=1;i<240;++i){auto hit=Call(i,true);hit.object=&obj[i];obj[2].cls=&obj[300+(i%100)];S::OnHitPre(hit);}
    CHECK(S::ReadForDrill().byClass.size()==64&&S::g_cancelled==239&&S::g_tallyHits==239);
    auto hit=Call(240,true);hit.object=&obj[1];++obj[1].serial;S::OnHitPre(hit);
    CHECK(std::count_if(S::g_cancelSay.begin(),S::g_cancelSay.end(),[](auto& s){return s.maker.Is(&obj[1]);})==1);
    CHECK(std::find_if(S::g_cancelSay.begin(),S::g_cancelSay.end(),[](auto& s){return s.maker.Is(&obj[1]);})->n==1);
    // Reused UClass addresses get a fresh name/count instead of inheriting the old tally.
    Reset();session.mode=coop::net::Role::Client;S::Install(&session);hit=Call(1,true);S::OnHitPre(hit);
    ++obj[300].serial;S::OnHitPre(hit);auto view=S::ReadForDrill();CHECK(view.byClass.size()==1&&view.byClass[0].n==1);
    Reset();S::g_brewAsked=S::g_hitAsked=S::g_settled=false;watchOk=false;auto watches=watchCalls;
    for(int i=0;i<5;++i){S::Install(&session);S::Tick();}CHECK(watchCalls==watches+2&&S::g_settled);watchOk=true;
    puts("PASS coffee state S01-S27: nested invocation ownership, retirement, guarded retries, diagnostics and sender gate");
}
void IntentTests(){
    Reset();session.mode=coop::net::Role::Client;I::Install(&session);auto c=Call();c.function=nullptr;CHECK(I::OnPressPre(c)==SG::Verdict::Cancel&&session.sends==0);
    c=Call();c.locals=nullptr;CHECK(I::OnPressPre(c)==SG::Verdict::Cancel);c=Call();offAction=frame;CHECK(I::OnPressPre(c)==SG::Verdict::Cancel&&session.sends==0);
    for(int kind=0;kind<3;++kind){Reset();session.mode=coop::net::Role::Client;I::Install(&session);onKey=[kind]{if(kind==0)++obj[1].serial;if(kind==1)++serial;if(kind==2)++world;};CHECK(I::OnPressPre(Call())==SG::Verdict::Cancel&&session.sends==0);}
    for(const auto& key:{std::wstring{},std::wstring(L"None"),std::wstring(32,L'a'),std::wstring(L"\u0401"),std::wstring(L"a\0b",3)}){
        Reset();session.mode=coop::net::Role::Client;I::Install(&session);obj[1].key=key;I::OnPressPre(Call());CHECK(session.sends==0);
    }
    Reset();session.mode=coop::net::Role::Client;I::Install(&session);I::OnPressPre(Call());CHECK(session.sends==1);
    Reset();session.mode=coop::net::Role::Client;I::Install(&session);onMeta=[] {++obj[250].serial;};I::OnPressPre(Call());CHECK(session.sends==0);
    Reset();session.mode=coop::net::Role::Client;I::Install(&session);session.onSend=[] {I::Install(&replacement);};I::OnPressPre(Call());CHECK(session.sends==1&&I::g_sent==0);
    for(int kind=0;kind<8;++kind){Reset();coop::net::CoffeeActionIntentPayload p{};coop::net::WireKeyFromString(obj[1].key,p.key);switch(kind){
        case 0:p.key.len=32;break;case 1:p.key.len=0;break;case 2:p.key.data[0]=char(255);break;case 3:session.ready[1]=false;break;case 4:session.online[1]=false;break;case 5:session.generations[1]=0;break;case 6:session.live=false;break;case 7:session.mode=coop::net::Role::Client;break;}
        I::OnAction(session,p,1);I::Tick(session);CHECK(actions==0);
    }
    Reset();Queue(31);I::Tick(session);CHECK(actions==1);Queue(32);I::Tick(session);CHECK(actions==1);
    for(int kind=0;kind<7;++kind){Reset();Queue();switch(kind){case 0:++session.generations[1];break;case 1:I::Install(&replacement);break;case 2:++serial;break;case 3:++world;break;case 4:I::OnDisconnect();break;case 5:++session.stops;break;case 6:session.live=false;break;}I::Tick(session);CHECK(actions==0);}
    Reset();Queue();I::Tick(replacement);CHECK(actions==0);I::Tick(session);CHECK(actions==1);
    for(int kind=0;kind<5;++kind){Reset();onReach=[kind]{if(kind==0)++obj[1].serial;if(kind==1)++obj[3].serial;if(kind==2)puppet.actor=&obj[4];if(kind==3)obj[1].key=L"changed";if(kind==4)indexed=&obj[4];};Queue();I::Tick(session);CHECK(actions==0);}
    Reset();onPrepare=[] {++obj[3].serial;};Queue();I::Tick(session);CHECK(actions==0);
    Reset();Queue();Queue(2);onAction=[] {I::Install(&replacement);};I::Tick(session);CHECK(actions==1&&I::g_ran==0);
    Reset();reach=coop::element::IntentOutcome::NoBody;Queue();I::Tick(session);CHECK(I::g_slots[1].waitSaid&&I::g_slots[1].queue.size()==1);
    I::g_slots[1].queue={};I::Queued expired{};expired.deadline=0;coop::net::WireKeyFromString(obj[1].key,expired.payload.key);I::g_slots[1].queue.Push(std::move(expired),8);I::Tick(session);CHECK(I::g_slots[1].queue.empty()&&!I::g_slots[1].waitSaid);
    Reset();Queue();onAction=[] {++session.generations[1];Queue();I::g_slots[1].waitSaid=true;};I::Tick(session);CHECK(I::g_slots[1].waitSaid&&I::g_slots[1].queue.size()==1);
    // The final key-index callback is an authority boundary too.
    Reset();Queue();onKey=[] {onFind=[] {++obj[3].serial;};};I::Tick(session);CHECK(actions==0);
    puts("PASS coffee intent C01-C23: malformed frames/keys, 32 slots, occupant generations, reach/body/rekey, wait expiry and reentry");
}
}
int main(){test::StateTests();test::IntentTests();}
