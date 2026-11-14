#define TEST_CUSTOM_ACTION_REFS
#define TEST_CUSTOM_WORLD_REFS
#include "support_2135.h"
#include "../../src/votv-coop/src/coop/props/llama_sync.cpp"
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include "../../src/votv-coop/src/coop/net/client_authorship.h"
#include <set>
namespace test {
bool active=false,found=true,readOk=true,hand=false,nativeReady=true,parkOk=true,entryOk=true,visual=false,foreignWorld=false,jumpSetting=true;
int applies=0,cues=0,parks=0;uint32_t serial=1;uintptr_t worldPointer=100;std::wstring key=L"llama";
bool remoteJump=false,nativeBlocked=false;int coasts=0;std::function<void()> afterCoast,afterTimerStop;
Obj extras[100]{},graph{103};void* canonical=&actor;void* lastApplied=nullptr;std::unordered_map<void*,std::wstring> actorKeys;
bool restoreOk=true;int restores=0;
bool beginReady=true,eventReady=true;int beginFinishes=0;uint64_t nextBeginTicket=0;
uint16_t people[coop::net::kMaxPeers]{};std::set<uint64_t> invocations;std::function<void()> keyCallback,afterCue,afterPark;
void Fire(std::function<void()>& f){auto copy=std::move(f);f={};if(copy)copy();}
}
namespace ue_wrap::reflection {
int32_t SlotSerial(int32_t i){return i==1?test::actor.serial:i==103?test::graph.serial:i>=3&&i<103?test::extras[i-3].serial:test::ui.serial;}
void* ClassOf(void* p){return p?&test::ui:nullptr;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}void* CurrentWorld(){return test::worldUnavailable?nullptr:reinterpret_cast<void*>(test::worldPointer);}
void* WorldOf(void* p){return p==&test::actor&&test::foreignWorld?reinterpret_cast<void*>(999):CurrentWorld();}
}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace coop::roster_ledger {const Row& Get(int slot){static Row rows[net::kMaxPeers];rows[slot].playerNo=test::people[slot];return rows[slot];}}
namespace coop::hand_item {bool IsHandAxisActor(void*){return test::hand;}}
namespace coop::llama_timers {void Install(net::Session*){}void Tick(){}void OnDisconnect(){test::Fire(test::afterTimerStop);}}
namespace coop::llama_events {void Install(net::Session*){}void Tick(){}void OnDisconnect(){}
bool Prepare(void*,const std::function<bool()>& current){return test::eventReady&&current();}}
namespace coop::llama_begin_restore {void Install(net::Session*){}void Tick(){}void OnDisconnect(){}void Consumed(void*){}
bool Prepare(void*,bool original,uint64_t,const std::function<bool()>& current,uint64_t& ticket){ticket=original?++test::nextBeginTicket:0;return test::beginReady&&current();}
void Commit(uint64_t){}void Cancel(uint64_t){}void Finished(uint64_t){++test::beginFinishes;}}
namespace coop::llama_trigger {void Reset(){} bool BlocksNative(void*){return test::nativeBlocked;}
bool AllowLocal(net::Session&,void*,const std::function<bool()>& current){return current();}
void TickMotion(net::Session&){}
void QueueNativeMotion(net::Session&,void*,const std::function<bool()>& current){if(current()){++test::coasts;test::Fire(test::afterCoast);}}
bool TryRemote(net::Session&,void*,const std::function<bool()>& current){if(!test::remoteJump||!current())return false;test::remoteJump=false;test::active=true;return true;}}
namespace coop::prop_drive_host {void Coast(void*,const char*,bool){++test::coasts;test::Fire(test::afterCoast);}}
namespace coop::prop_element_tracker {void* FindLiveActorByKey(const std::wstring& key){return test::found&&key==test::key?test::canonical:nullptr;}}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void* actor){const auto it=test::actorKeys.find(actor);const auto key=it==test::actorKeys.end()?test::key:it->second;test::Fire(test::keyCallback);return key;}}
namespace ue_wrap::script_gate {bool IsInvocationActive(uint64_t id){return test::invocations.contains(id);}}
namespace ue_wrap::llama {
bool IsLlama(void* p){if(p==&test::actor)return test::actor.live;for(const auto& extra:test::extras)if(p==&extra)return extra.live;return false;}
size_t Collect(CachedObjRef* out,size_t capacity){if(!test::found||!capacity)return 0;out[0].Set(&test::actor);return 1;}
bool Read(void*,bool& out){out=test::active;test::Callback();return test::readOk;}
bool Entry(void*,const uint8_t* p,int32_t& entry){if(!p||!test::entryOk)return false;std::memcpy(&entry,p,4);return true;}
bool ParkBegin(void*,bool& original,const std::function<bool()>& current){++test::parks;test::Callback();if(!test::parkOk||!current())return false;
    original=test::jumpSetting;test::jumpSetting=false;test::Fire(test::afterPark);return true;}
bool RestoreBegin(void*,bool original,const std::function<bool()>& current){++test::restores;if(!test::restoreOk||!current())return false;test::jumpSetting=original;return true;}
bool Apply(void* actor,bool active,bool cue,bool& dispatched,const std::function<bool()>& current){
    dispatched=false;if(!current()||!test::nativeReady)return false;++test::applies;test::lastApplied=actor;test::visual=active;test::Callback();if(!current())return false;
    if(cue){++test::cues;dispatched=true;test::Fire(test::afterCue);}return current();
}
}
namespace test {
namespace S=coop::llama_sync;using coop::net::Role;using Packet=coop::net::LlamaStatePayload;
coop::net::Session session;int32_t entry=1264;ue_wrap::script_gate::Call call{};
void Reset(Role role=Role::Host){S::OnDisconnect();++world;++serial;worldPointer=100+world;worldUnavailable=false;
    session={};session.mode=role;active=visual=hand=foreignWorld=false;found=readOk=nativeReady=parkOk=entryOk=announced=jumpSetting=true;
    actor.live=ui.live=graph.live=true;++actor.serial;++ui.serial;++graph.serial;callback=keyCallback=afterCue=afterPark=afterCoast=afterTimerStop={};key=L"llama";applies=cues=parks=coasts=restores=0;invocations.clear();remoteJump=nativeBlocked=false;
    actorKeys.clear();for(int i=0;i<100;++i){extras[i].id=3+i;extras[i].live=true;++extras[i].serial;}now=1000;canonical=&actor;lastApplied=nullptr;restoreOk=true;beginReady=eventReady=true;beginFinishes=0;
    for(int i=0;i<coop::net::kMaxPeers;++i)people[i]=uint16_t(10+i);S::Install(&session);entry=1264;call={};
    call.object=&actor;call.function=&graph;call.locals=reinterpret_cast<uint8_t*>(&entry);call.stack=&call;call.invocation=500;invocations.insert(500);
}
Packet State(uint32_t sequence=1,uint32_t pulse=1,bool cue=true){Packet p{};coop::net::WireKeyFromString(key,p.key);p.sequence=sequence;p.pulse=pulse;p.active=1;p.cue=uint8_t(cue);p.ttlMs=cue?1500:0;return p;}
Packet Sent(size_t index=0){Packet p{};std::memcpy(&p,session.sent.at(index).data.data(),sizeof(p));return p;}
void Host(){Reset();S::Tick();CHECK(!session.sent.empty()&&!Sent().cue&&!Sent().active);Reset(Role::Client);S::Sample(&actor,true);CHECK(S::g_rows.empty());}
void Authority(){Reset(Role::Client);for(int value:{1264,1269,15,1214}){entry=value;CHECK(S::BeforeGraph(call)==ue_wrap::script_gate::Verdict::Cancel);}
    entryOk=false;CHECK(S::BeforeGraph(call)==ue_wrap::script_gate::Verdict::Cancel);Reset();CHECK(S::BeforeGraph(call)==ue_wrap::script_gate::Verdict::Run);}
void Begin(){Reset(Role::Client);entry=1146;CHECK(S::BeforeGraph(call)==ue_wrap::script_gate::Verdict::Run&&parks==1&&!jumpSetting);
    auto wrong=call;wrong.stack=&ui;S::AfterGraph(wrong);CHECK(!jumpSetting);S::AfterGraph(call);CHECK(jumpSetting&&S::g_captures.empty());
    parkOk=false;CHECK(S::BeforeGraph(call)==ue_wrap::script_gate::Verdict::Cancel);
    Reset(Role::Client);entry=1146;afterPark=[]{hand=true;};CHECK(S::BeforeGraph(call)==ue_wrap::script_gate::Verdict::Cancel&&jumpSetting);
    Reset(Role::Client);entry=1146;beginReady=false;CHECK(S::BeforeGraph(call)==ue_wrap::script_gate::Verdict::Cancel&&jumpSetting&&S::g_captures.empty());
    beginReady=true;CHECK(S::BeforeGraph(call)==ue_wrap::script_gate::Verdict::Run);S::AfterGraph(call);CHECK(jumpSetting&&beginFinishes==1);}
void Sources(){for(int cause=0;cause<6;++cause){Reset();if(cause==0)hand=true;if(cause==1)foreignWorld=true;if(cause==2)key=L"None";
    if(cause==3)key=L"bad key";if(cause==4)key=std::wstring(40,L'x');if(cause==5)actor.live=false;S::Sample(&actor,true);CHECK(S::g_rows.empty());}}
void Capture(){Reset();CHECK(S::BeforeGraph(call)==ue_wrap::script_gate::Verdict::Run);auto wrong=call;wrong.stack=&ui;S::AfterGraph(wrong);CHECK(S::g_captures.size()==1);
    wrong=call;wrong.locals=reinterpret_cast<uint8_t*>(&actor);S::AfterGraph(wrong);CHECK(S::g_captures.size()==1);
    active=true;S::AfterGraph(call);CHECK(S::g_captures.empty()&&S::g_rows.at(key).state.cue);S::AfterGraph(call);CHECK(S::g_rows.at(key).state.sequence==1);
    Reset();keyCallback=[]{CHECK(S::g_captures.size()==1&&!S::g_captures[0].ready);S::BeforeGraph(call);};S::BeforeGraph(call);CHECK(S::g_captures.size()==1);
    key=L"renamed";active=true;S::AfterGraph(call);CHECK(S::g_rows.empty());
    Reset();S::BeforeGraph(call);++serial;active=true;S::AfterGraph(call);CHECK(S::g_rows.empty());}
void Bounded(){Reset();for(int i=0;i<257;++i){key=L"llama_"+std::to_wstring(i);S::Sample(&actor);}CHECK(S::g_rows.size()==256&&S::g_samples.size()==256);
    S::Tick();CHECK(S::g_rows.size()<=1);Reset();S::Sample(&actor);S::Sample(&actor);CHECK(S::g_rows.size()==1&&S::g_rows.at(key).state.sequence==1);}
void SessionScope(){for(int cause=0;cause<4;++cause){Reset();S::Sample(&actor,true);found=false;
    if(cause==0)++serial;if(cause==1)session.active=false;if(cause==2)session.mode=Role::Client;if(cause==3)session.linked=false;
    S::Tick();CHECK(S::g_rows.empty()&&S::g_captures.empty());}
    Reset(Role::Client);S::Receive(State(),0);++session.peerEpoch;found=false;S::Tick();CHECK(S::g_rows.empty());}
void WorldScope(){for(int cause=0;cause<3;++cause){Reset();S::Sample(&actor,true);found=false;
    if(cause==0)++world;if(cause==1)++worldPointer;if(cause==2)worldUnavailable=true;S::Tick();CHECK(S::g_rows.empty());}}
void NestedSample(){Reset();callback=[]{active=true;S::Sample(&actor);};S::Sample(&actor);CHECK(S::g_rows.at(key).state.active);
    Reset();callback=[]{++ui.serial;};S::Sample(&actor);CHECK(S::g_rows.empty()&&S::g_samples.empty());}
void Shape(){Reset();auto p=State();S::Receive(p,0);CHECK(S::g_rows.empty());
    for(int cause=0;cause<10;++cause){Reset(Role::Client);p=State();if(cause==0)p.sequence=0;if(cause==1)p.pulse=0;
        if(cause==2)p.active=2;if(cause==3)p.cue=2;if(cause==4)p.ttlMs=1501;if(cause==5)p.active=0;
        if(cause==6)p.cue=0;if(cause==7)p.key.len=32;if(cause==8)p.key.data[0]=' ';if(cause==9)p.ttlMs=0;S::Receive(p,0);CHECK(S::g_rows.empty());}
    Reset(Role::Client);p=State();S::Receive(p,1);CHECK(S::g_rows.empty());S::Receive(p,0);p.active=0;p.cue=0;p.ttlMs=0;S::Tick();CHECK(visual&&cues==1);}
void Recipients(){for(bool connection:{false,true}){Reset();active=true;S::Sample(&actor,true);found=false;
    if(connection)++session.peerEpoch;else ++people[31];S::Tick();for(const auto& message:session.sent)CHECK(message.slot!=31);}
    Reset();S::Sample(&actor);S::OnPeerLeft(31);CHECK(!(S::g_rows.at(key).pending&(1u<<31)));S::QueueConnect(32);}
void Readiness(){Reset();active=true;S::Sample(&actor,true);session.perSlot=true;session.links.fill(true);session.generations.fill(1);
    session.lanes.fill(false);session.worlds.fill(false);session.sent.clear();S::Tick();CHECK(session.sent.empty()&&S::g_rows.at(key).pending);
    session.lanes.fill(true);session.worlds.fill(true);S::Tick();CHECK(session.sent.size()==31&&!S::g_rows.at(key).pending);}
void Sending(){Reset();active=true;S::Sample(&actor,true);bool nested=false;session.allowSend=[&](int slot,auto,const void*,int){
    if(slot==1&&!nested){nested=true;active=false;S::Sample(&actor);}return true;};S::Tick();CHECK(nested&&Sent().active&&!S::g_rows.at(key).state.active&&S::g_rows.at(key).pending);
    Reset();S::Sample(&actor);bool requeue=false;session.allowSend=[&](int slot,auto,const void*,int){if(slot==31&&!requeue){requeue=true;S::QueueConnect(31);}return true;};
    S::Tick();CHECK(S::g_rows.at(key).pending&(1u<<31));session.allowSend={};S::Tick();CHECK(!(S::g_rows.at(key).pending&(1u<<31)));}
void ActorWait(){Reset(Role::Client);auto p=State();S::Receive(p,0);found=false;S::Tick();CHECK(!cues&&S::g_rows.size()==1);
    found=true;now+=100;announced=false;S::Tick();CHECK(!cues);announced=true;now+=100;S::Tick();CHECK(cues==1&&visual);
    Reset(Role::Client);S::Receive(State(),0);nativeReady=false;S::Tick();CHECK(!cues);nativeReady=true;now+=100;S::Tick();CHECK(cues==1);}
void Ordering(){Reset(Role::Client);auto p=State(UINT32_MAX,UINT32_MAX);S::Receive(p,0);p.sequence=1;p.pulse=1;S::Receive(p,0);
    CHECK(S::g_rows.at(key).state.sequence==1);auto old=p;old.sequence=UINT32_MAX;S::Receive(old,0);CHECK(S::g_rows.at(key).state.sequence==1);
    p.sequence=2;p.pulse=UINT32_MAX;S::Receive(p,0);CHECK(S::g_rows.at(key).state.sequence==1);S::Tick();now+=100;S::Tick();CHECK(cues==1);
    Reset(Role::Client);p=State();S::Receive(p,0);afterCue=[&]{p.sequence=2;S::Receive(p,0);};S::Tick();S::Tick();CHECK(cues==1&&S::g_rows.at(key).heard==1);}
void Expiry(){Reset();active=true;S::Sample(&actor,true);S::QueueConnect(31);S::Tick();for(size_t i=0;i<session.sent.size();++i)if(session.sent[i].slot==31)CHECK(!Sent(i).cue&&Sent(i).active);
    Reset();active=true;S::Sample(&actor,true);session.allowSend=[](int slot,auto,const void*,int){if(slot==1)now+=1600;return true;};S::Tick();
    for(size_t i=0;i<session.sent.size();++i)if(session.sent[i].slot==2)CHECK(!Sent(i).cue);
    Reset(Role::Client);S::Receive(State(),0);found=false;S::Tick();now+=1600;found=true;S::Tick();CHECK(!cues&&visual);
    Reset(Role::Client);S::Receive(State(),0);callback=[]{now+=1600;};S::Tick();CHECK(!cues);
    Reset(Role::Client);S::Receive(State(),0);found=false;S::Tick();--now;found=true;S::Tick();CHECK(!cues&&visual);}
void Wiring(){CHECK(coop::net::LaneForKind(coop::net::ReliableKind::LlamaState)==coop::net::Lane::Normal);
    CHECK(coop::net::AuthorshipOf(coop::net::ReliableKind::LlamaState)==coop::net::ClientAuthorship::Host);
    CHECK(!coop::net::IsClientRelayableReliableKind(coop::net::ReliableKind::LlamaState));Reset();S::Sample(&actor);S::OnDisconnect();CHECK(!S::g_session&&S::g_rows.empty());}
void RemoteState(){Reset();remoteJump=true;S::Tick();CHECK(!session.sent.empty()&&Sent().active&&Sent().cue);const auto state=Sent();
    Reset(Role::Client);S::Receive(state,0);S::Tick();CHECK(visual&&cues==1);now+=100;S::Tick();CHECK(cues==1);
    Reset(Role::Client);eventReady=false;S::Receive(state,0);S::Tick();CHECK(!visual&&!cues&&S::g_rows.size()==1);
    eventReady=true;now+=100;S::Tick();CHECK(visual&&cues==1);}
void NativeMotion(){Reset();S::BeforeGraph(call);active=true;S::AfterGraph(call);CHECK(coasts==1&&S::g_rows.at(key).state.cue);
    Reset();nativeBlocked=true;CHECK(S::BeforeGraph(call)==ue_wrap::script_gate::Verdict::Cancel);entry=1269;CHECK(S::BeforeGraph(call)==ue_wrap::script_gate::Verdict::Cancel);
    entry=212;CHECK(S::BeforeGraph(call)==ue_wrap::script_gate::Verdict::Run);
    Reset();S::BeforeGraph(call);active=true;afterCoast=[]{key=L"replacement";};S::AfterGraph(call);CHECK(coasts==1&&S::g_rows.empty());}
void ReadySlots(){session.perSlot=true;session.links.fill(true);session.lanes.fill(true);session.worlds.fill(true);session.generations.fill(1);}
void Recovery(int id){Reset();switch(id){
    case 26:ReadySlots();session.lanes.fill(false);active=true;S::Sample(&actor,true);now+=1100;S::Sample(&actor);
        CHECK(S::g_rows.at(key).state.cue);session.lanes.fill(true);found=false;S::Tick();CHECK(Sent().cue&&Sent().ttlMs==400);break;
    case 27:active=true;S::Sample(&actor,true);{const auto original=S::g_rows.at(key).cueAt;now+=1100;S::Sample(&actor);CHECK(S::g_rows.at(key).cueAt==original);
        now=original+1500;found=false;S::Tick();for(size_t i=0;i<session.sent.size();++i)CHECK(!Sent(i).cue);}break;
    case 28:active=true;S::Sample(&actor,true);S::QueueConnect(31);now+=1100;S::Sample(&actor);found=false;S::Tick();
        for(size_t i=0;i<session.sent.size();++i)CHECK(Sent(i).cue==uint8_t(session.sent[i].slot!=31));break;
    case 29:active=true;S::Sample(&actor,true);found=false;S::Tick();CHECK(session.sent.size()==31);session.sent.clear();now+=1100;S::Sample(&actor);S::Tick();
        CHECK(session.sent.size()==31);for(size_t i=0;i<session.sent.size();++i)CHECK(!Sent(i).cue);
        Reset();active=true;S::Sample(&actor,true);found=false;{bool changed=false;session.allowSend=[&](int slot,auto,const void*,int){
            if(slot==1&&!changed){changed=true;now+=1000;S::Sample(&actor);}return true;};S::Tick();
            CHECK(changed&&S::g_rows.at(key).recipients[1].delivered==S::g_rows.at(key).state.pulse&&(S::g_rows.at(key).pending&(1u<<1)));
            session.allowSend={};session.sent.clear();S::Tick();for(size_t i=0;i<session.sent.size();++i)if(session.sent[i].slot==1)CHECK(!Sent(i).cue);}break;
    case 30:Reset(Role::Client);{auto p=State();S::Receive(p,0);const auto deadline=S::g_rows.at(key).expires;now+=400;++p.sequence;S::Receive(p,0);
        CHECK(S::g_rows.at(key).expires==deadline);now=deadline+1;S::Tick();CHECK(!cues&&visual);}break;
    case 31:Reset(Role::Client);S::Receive(State(1,1,false),0);S::Receive(State(2,1,true),0);S::Tick();CHECK(!cues&&S::g_rows.at(key).heard==1);break;
    case 32:Reset(Role::Client);S::Receive(State(10,10),0);found=false;now+=10001;S::Tick();CHECK(S::g_rows.at(key).expired);
        S::Receive(State(9,9),0);CHECK(S::g_rows.at(key).expired&&S::g_rows.at(key).state.sequence==10);
        Reset(Role::Client);found=false;for(int i=0;i<256;++i){key=L"key_"+std::to_wstring(i);S::Receive(State(uint32_t(i+1),uint32_t(i+1),false),0);}
        CHECK(S::g_rows.size()==256);now+=10001;S::Tick();key=L"new_key";S::Receive(State(300,300,false),0);CHECK(S::g_rows.size()==256&&S::g_rows.contains(key));break;
    case 33:Reset(Role::Client);S::Receive(State(),0);S::Tick();CHECK(cues==1);now+=10001;S::Tick();CHECK(S::g_rows.at(key).expired&&S::g_rows.at(key).heard==1);
        S::Receive(State(2,1),0);S::Tick();CHECK(cues==1&&S::g_rows.at(key).heard==1);break;
    case 34:Reset(Role::Client);S::Receive(State(1,1,false),0);S::Tick();--now;S::Receive(State(2,2),0);S::Tick();CHECK(cues==1&&visual);break;
    case 35:Reset(Role::Client);now=std::numeric_limits<uint64_t>::max()-100;S::Receive(State(),0);CHECK(S::g_rows.at(key).expires==std::numeric_limits<uint64_t>::max());S::Tick();CHECK(cues==1);break;
    case 36:Reset(Role::Client);now=std::numeric_limits<uint64_t>::max()-50;S::Receive(State(1,1,false),0);S::Tick();CHECK(applies==1);++now;S::Tick();CHECK(applies==1);break;
    case 37:Reset(Role::Client);S::Receive(State(1,1,false),0);S::Tick();CHECK(lastApplied==&actor);canonical=&extras[0];now+=100;S::Tick();CHECK(lastApplied==&extras[0]);break;
    case 38:Reset(Role::Client);S::Receive(State(),0);callback=[]{announced=false;};S::Tick();CHECK(!cues);
        Reset(Role::Client);S::Receive(State(),0);callback=[]{canonical=&extras[0];};S::Tick();CHECK(!cues);break;
    case 39:ReadySlots();active=true;S::Sample(&actor,true);found=false;{int reads=0;std::function<void()> nested;nested=[&]{if(++reads==2)++people[1];else keyCallback=nested;};keyCallback=nested;
        S::Tick();for(const auto& sent:session.sent)CHECK(sent.slot!=1);}break;
    case 40:ReadySlots();active=true;S::Sample(&actor,true);found=false;{int reads=0;std::function<void()> nested;nested=[&]{if(++reads==2)session.worlds[1]=false;else keyCallback=nested;};keyCallback=nested;
        S::Tick();for(const auto& sent:session.sent)CHECK(sent.slot!=1);CHECK(S::g_rows.at(key).pending&(1u<<1));}break;
    case 41:active=true;S::Sample(&actor,true);found=false;{int reads=0;std::function<void()> nested;nested=[&]{if(++reads==2){active=false;S::Sample(&actor);}else keyCallback=nested;};keyCallback=nested;
        S::Tick();CHECK(session.sent.empty()&&!S::g_rows.at(key).state.active);}
        Reset();active=true;S::Sample(&actor,true);found=false;{int reads=0;std::function<void()> nested;nested=[&]{if(++reads==2)S::QueueConnect(1);else keyCallback=nested;};keyCallback=nested;
        S::Tick();CHECK(session.sent.empty()&&(S::g_rows.at(key).pending&(1u<<1)));}break;
    case 42:for(int i=0;i<3;++i){actorKeys[&extras[i]]=L"row_"+std::to_wstring(i);S::Sample(&extras[i]);}found=false;S::Tick();CHECK(session.sent.size()==64);session.sent.clear();
        for(int i=0;i<3;++i)S::Queue(S::g_rows.at(actorKeys[&extras[i]]),31,false);S::Tick();{bool thirdHigh=false;for(size_t i=0;i<session.sent.size();++i)if(coop::net::StringFromWireKey(Sent(i).key)==L"row_2"&&session.sent[i].slot==31)thirdHigh=true;CHECK(thirdHigh);}break;
    case 43:S::Sample(&actor);found=false;{std::vector<int> attempts;session.allowSend=[&](int slot,auto,const void*,int){attempts.push_back(slot);return false;};S::Tick();CHECK(attempts==std::vector<int>({1,2}));
        S::Tick();CHECK(attempts==std::vector<int>({1,2,3,4}));for(int i=0;i<14;++i)S::Tick();CHECK(std::find(attempts.begin(),attempts.end(),31)!=attempts.end());}break;
    case 44:for(int i=0;i<10;++i){actorKeys[&extras[i]]=L"row_"+std::to_wstring(i);S::Sample(&extras[i]);}found=false;{std::set<std::wstring> seen;int attempts=0;
        session.allowSend=[&](int,auto,const void* bytes,int){++attempts;seen.insert(coop::net::StringFromWireKey(static_cast<const Packet*>(bytes)->key));return false;};S::Tick();CHECK(attempts==20&&seen.size()==10);}break;
    case 45:Reset(Role::Client);entry=1146;S::BeforeGraph(call);CHECK(!jumpSetting);invocations.erase(call.invocation);S::Tick();CHECK(jumpSetting&&S::g_captures.empty()&&S::g_restores.empty());break;
    case 46:Reset(Role::Client);entry=1146;S::BeforeGraph(call);S::OnDisconnect();CHECK(!jumpSetting&&S::g_restores.size()==1);S::AfterGraph(call);CHECK(jumpSetting&&S::g_restores.empty());break;
    case 47:Reset(Role::Client);entry=1146;afterPark=[]{S::OnDisconnect();};CHECK(S::BeforeGraph(call)==ue_wrap::script_gate::Verdict::Cancel&&jumpSetting&&S::g_restores.empty());break;
    case 48:Reset(Role::Client);entry=1146;S::BeforeGraph(call);++graph.serial;S::AfterGraph(call);CHECK(jumpSetting&&S::g_restores.empty());break;
    case 49:Reset(Role::Client);entry=1146;S::BeforeGraph(call);restoreOk=false;S::AfterGraph(call);CHECK(!jumpSetting&&S::g_restores.size()==1);
        restoreOk=true;S::Tick();CHECK(jumpSetting&&S::g_restores.empty());break;
    case 50:Reset(Role::Client);entry=1146;S::BeforeGraph(call);{auto inner=call;inner.invocation=501;inner.stack=&inner;invocations.insert(501);
        CHECK(S::BeforeGraph(inner)==ue_wrap::script_gate::Verdict::Run);S::AfterGraph(inner);CHECK(!jumpSetting&&S::g_restores.size()==1);restoreOk=false;
        S::OnDisconnect();S::AfterGraph(call);CHECK(!jumpSetting&&S::g_restores.size()==2);restoreOk=true;S::Tick();CHECK(jumpSetting&&S::g_restores.empty());}
        Reset(Role::Client);entry=1146;S::BeforeGraph(call);session.mode=Role::Host;++serial;S::Install(&session);CHECK(!jumpSetting);S::AfterGraph(call);CHECK(jumpSetting);
        Reset(Role::Client);{coop::net::Session replacement;afterTimerStop=[&]{S::Install(&replacement);};++serial;S::Install(&session);
            CHECK(S::g_session==&replacement&&S::g_scope.role==Role::Host);S::OnDisconnect();}break;
    default:CHECK(false);}
}
}
int main(){using namespace test;struct Group{int id;void(*run)();};Group groups[]={{1,Host},{2,Authority},{3,Begin},{4,Sources},{6,Capture},{14,Bounded},{15,SessionScope},
    {16,WorldScope},{17,NestedSample},{18,Shape},{19,Recipients},{20,Readiness},{21,Sending},{22,ActorWait},{23,Ordering},{24,Expiry},{25,Wiring}};
    for(const auto& g:groups){g.run();std::printf("PASS llama %d\n",g.id);}
    RemoteState();std::printf("PASS llama trigger 66\n");NativeMotion();std::printf("PASS llama trigger 68\n");
    for(int id=26;id<=50;++id){Recovery(id);std::printf("PASS llama recovery %d\n",id);}return 0;}
