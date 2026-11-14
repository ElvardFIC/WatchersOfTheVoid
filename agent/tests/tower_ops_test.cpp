#ifndef TOWER_OPS_SOURCE
#define TOWER_OPS_SOURCE "../../src/votv-coop/src/coop/world/fuse_tower_ops.cpp"
#endif
namespace coop::net { struct Config; }
#include TOWER_OPS_SOURCE
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
namespace S=coop::fuse_tower_ops;
namespace F=coop::fuse_tower_kind;
namespace N=coop::net;
struct Object {int index=0,serial=1;bool live=true;} tower{0},cls{1},fuse{2},body{3},output{4},otherOutput{5};
uint32_t world=1,serial=1;
bool resolved=true,located=true,viewed=true,easy=false,useOk=true,spawnOk=true,haveTower=true;
bool writeOk=true,spawnAttempted=true;
bool grabReadable=false,pulledFuse=false;
void* grabActor=nullptr;void* lastDestroyed=nullptr;
int32_t paramOffset=-1,paramBytes=16,lookSlot=0;
F::Look look=F::Look::None;
F::View view;
coop::RemotePlayer puppet;
std::vector<std::string> calls;
std::function<void(const std::string&)> hook;
coop::element::IntentOutcome reach=coop::element::IntentOutcome::Ok;
const char* refusal=nullptr;
void Call(const char* name){calls.emplace_back(name);auto fn=hook;if(fn)fn(name);}
int Count(const char* name){return int(std::count(calls.begin(),calls.end(),name));}
N::FuseTowerPayload Op(uint16_t seq,uint8_t op=N::kFuseTowerOpButton){N::FuseTowerPayload p{};
    p.seq=seq;p.op=op;p.kind=N::kFuseTowerKindCoordinate;p.tower=0;p.index=0;return p;}
void Reset(N::Session& session){hook={};S::OnDisconnect();++world;++serial;
    for(auto* object:{&tower,&cls,&fuse,&body,&output,&otherOutput}){++object->serial;object->live=true;}
    session={};resolved=located=viewed=useOk=spawnOk=haveTower=true;easy=false;refusal=nullptr;
    reach=coop::element::IntentOutcome::Ok;view={};view.broken=true;view.fuseCount=1;view.lightCount=1;view.fuses[0]=2;
    paramOffset=-1;paramBytes=16;lookSlot=0;look=F::Look::None;writeOk=true;spawnAttempted=true;
    grabReadable=pulledFuse=false;grabActor=lastDestroyed=nullptr;coop::players::Registry::Get().local=nullptr;
    puppet.actor=&body;coop::players::Registry::Get().bodies.fill(&puppet);
    coop::players::Registry::Get().lives.fill(coop::players::PuppetLife::Alive);
    calls.clear();S::Install(&session);S::BindPeer(&session,1);}
void Credit(int slot=1){S::OnDestroyHeard(slot,&fuse);CHECK(S::FindSpend(uint8_t(slot),F::Kind::Coordinate,0,&tower)!=0);calls.clear();}
void Queue(N::Session& session,uint16_t seq,int slot=1,uint8_t op=N::kFuseTowerOpButton){
    CHECK(S::BindPeer(&session,uint8_t(slot)));
    S::g_waiting[slot].push_back({Op(seq,op),S::NowMs(),S::Capture(&session,uint8_t(slot))});}
void Mark(const char* id){std::printf("PASS %s\n",id);}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return p?static_cast<test::Object*>(p)->index:-1;}
int32_t SlotSerial(int32_t i){return i==0?test::tower.serial:i==1?test::cls.serial:i==2?test::fuse.serial:i==3?test::body.serial:i==4?test::output.serial:i==5?test::otherOutput.serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLive(void* p){return p && static_cast<test::Object*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t i){return p && i>=0 && i<6 && static_cast<test::Object*>(p)->index==i && IsLive(p);}
void* ClassOf(void*){return &test::cls;}
int32_t FindParamOffset(void*,const wchar_t*){return test::paramOffset;}
int32_t FunctionFrameSize(void*){return test::paramBytes;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return nullptr;}
}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}std::string Addr(std::string_view value){return std::string(value);}}
namespace ue_wrap::script_gate {
bool WatchName(const facts::Function&,int,PreFn,PostFn){return true;}
void ResolvePendingNames(){}
bool NameWatchLive(const facts::Function&,int){return true;}
bool NameWatchSettled(const facts::Function&,int){return true;}
}
namespace coop::roster_ledger {void RegisterPerSlotClear(PerSlotClearFn,void*) {}}
namespace coop::element {
IntentTarget IntentTarget::ForClientIntent(net::Session& s,uint8_t slot,float reach){IntentTarget t;t.session_=&s;t.slot_=slot;t.reachUU_=reach;return t;}
IntentSubject IntentTarget::AuthorizeSegment(const ue_wrap::FVector&,const ue_wrap::FVector&)const{
    test::Call("reach");return {test::reach,nullptr,slot_,reachUU_,10.f};}
const char* OutcomeName(IntentOutcome){return "reach refused";}
}
namespace coop::fuse_tower_kind {
bool Resolved(Kind){test::Call("resolved");return test::resolved;}
void* Find(Kind,int32_t id){test::Call("find");return test::haveTower && id==0?&test::tower:nullptr;}
int32_t ReadAll(Tower* out,int32_t cap){if(cap){out[0]={Kind::Coordinate,0,&test::tower};return 1;}return 0;}
bool IsFuse(void* p){return p==&test::fuse;}
bool IsPulledFuse(void* actor){return test::pulledFuse && (actor==&test::output || actor==&test::otherOutput);}
bool PartLocation(Kind,void*,uint8_t,int32_t,ue_wrap::FVector& out){out={};test::Call("part");return test::located;}
bool EasyInsert(){test::Call("difficulty");return test::easy;}
bool ReadView(Kind,void*,View& out){out=test::view;test::Call("view");return test::viewed;}
const char* CanPull(Kind,void*,const View&,int32_t){test::Call("canPull");return test::refusal;}
const char* CanInsert(Kind,void*,const View&,int32_t){test::Call("canInsert");return test::refusal;}
const char* CanUse(Kind,void*,uint8_t){test::Call("canUse");return test::refusal;}
bool Use(Kind,void*,void*,uint8_t,int32_t){test::Call("use");return test::useOk;}
bool TakePull(Kind,void*,int32_t slot,const std::function<bool()>& current){CHECK(current());if(test::writeOk)test::view.fuses[slot]=0;test::Call("pull");return test::writeOk;}
bool TakeInsert(Kind,void*,int32_t slot,const std::function<bool()>& current){CHECK(current());if(test::writeOk)test::view.fuses[slot]=1;test::Call("insert");return test::writeOk;}
void TakeEasyInsert(Kind,void*,int32_t,const std::function<bool()>& current){CHECK(current());if(test::writeOk)test::view.broken=false;test::Call("easy");}
void* SpawnFuse(const ue_wrap::FVector&){test::Call("spawn");return test::spawnOk?&test::fuse:nullptr;}
void* SpawnFuseTracked(const ue_wrap::FVector&,bool& attempted,const std::function<bool()>& current){if(current&&!current()){attempted=false;return nullptr;}test::Call("spawn");attempted=test::spawnAttempted;return test::spawnOk?&test::fuse:nullptr;}
bool Identify(void* p,Kind& kind,int32_t& id){kind=Kind::Coordinate;id=0;return p==&test::tower;}
Look ReadUse(Kind,void*,uint8_t& op,int32_t& index){op=coop::net::kFuseTowerOpPull;index=test::lookSlot;test::Call("look");return test::look;}
bool ReadFuseLook(Kind,void*,int32_t& slot){slot=test::lookSlot;test::Call("fuseLook");return true;}
const WatchRows& WatchesOf(Kind){static WatchRows rows{};return rows;}
}
namespace coop::fuse_tower_rows {
void HostAnswerOwed(uint8_t){test::Call("answer");}
void HostSendNow(){test::Call("publish");}
bool Applying(){return false;}
void ReapplyKept(uint8_t,int32_t){}
}
namespace coop::remote_prop {void AddDestroyListener(DestroyHeardFn){}}
namespace coop::prop_lifecycle {void DestroyLocalProp(void* actor,bool){test::lastDestroyed=actor;test::Call("destroyLocal");}}
namespace coop::net_pump {bool IsInAnnouncedWorld(void*){return true;}}
namespace coop::config {std::string ResolveString(const config_registry::StringRow&){return {};}}
namespace ue_wrap::engine {
bool TryGetActorLocation(void*,FVector& out){out={};test::Call("body");return true;}
bool ReadMainPlayerGrabState(void*,MainPlayerGrabState& state){test::Call("grab");state.holdingActor=test::grabActor;return test::grabReadable;}
}

#ifndef TOWER_OPS_NO_MAIN
int main(){using namespace test;
    N::Session session;
    constexpr const char* stages[]={"part","reach","difficulty","view","canPull","canInsert","canUse","use","pull","insert","easy"};
    constexpr const char* causes[]={"world","session","sender","tower","class"};
    for(int stage=0;stage<11;++stage)for(int cause=0;cause<5;++cause){
        Reset(session);
        const bool inserting=stage==2 || stage==5 || stage>=9;
        const uint8_t op=inserting?N::kFuseTowerOpInsert:(stage==4 || stage==8)?N::kFuseTowerOpPull:N::kFuseTowerOpButton;
        if(inserting && stage!=10)Credit();easy=stage==10;
        bool fired=false;
        hook=[&](const std::string& name){if(!fired && name==stages[stage]){
            fired=true;if(cause==0)++world;if(cause==1)++serial;if(cause==2)++session.generations[1];
            if(cause==3)++tower.serial;if(cause==4)++cls.serial;
        }};
        S::HostOffer(&session,Op(1,op),1);
        CHECK(fired && !S::g_taken && !Count("publish") && !Count("answer"));
        if(stage<7)CHECK(!Count("use") && !Count("pull") && !Count("insert") && !Count("easy"));
        CHECK(S::g_waiting[1].empty() && !S::g_hostExecuting);
        std::printf("PASS TO%02d: %s / %s\n",stage*5+cause+1,stages[stage],causes[cause]);
    }
    Reset(session);Queue(session,1);hook=[&](const std::string& n){if(n=="use"){CHECK(S::g_waiting[1].empty());S::OnDisconnect();}};
    S::HostDrainWaiting(&session);CHECK(S::g_waiting[1].empty());Mark("TQ01 detach before native callback");
    Reset(session);Queue(session,1);resolved=false;bool nested=false;
    hook=[&](const std::string& n){if(n=="resolved" && !nested){nested=true;S::HostOffer(&session,Op(2),1);}};
    S::HostDrainWaiting(&session);CHECK(S::g_waiting[1].size()==2 && S::g_waiting[1][0].p.seq==1 && S::g_waiting[1][1].p.seq==2);Mark("TQ02 restore Wait before nested arrival");
    Reset(session);Queue(session,1);resolved=false;hook=[](const std::string& n){if(n=="resolved")S::OnPeerLeft(1);};
    S::HostDrainWaiting(&session);CHECK(S::g_waiting[1].empty());Mark("TQ03 departure cancels Wait");
    Reset(session);Queue(session,1);++session.generations[1];S::HostDrainWaiting(&session);
    CHECK(!Count("use") && S::g_waiting[1].empty());Mark("TQ04 queued occupant stamp");
    Reset(session);session.ready[1]=false;S::HostOffer(&session,Op(1),1);CHECK(calls.empty() && S::g_waiting[1].empty());
    session.ready[1]=true;session.online[1]=false;S::HostOffer(&session,Op(1),1);CHECK(calls.empty());Mark("TQ05 ready connected sender");
    Reset(session);Queue(session,1);resolved=false;S::g_waiting[1][0].arrivedMs=S::NowMs()+100000;
    S::HostDrainWaiting(&session);CHECK(S::g_waiting[1].size()==1 && !S::g_ack[1]);Mark("TQ06 no backwards timeout");
    Reset(session);Queue(session,1);resolved=false;S::HostDrainWaiting(&session);
    int tokens=0;while(S::g_rate[1].Take(S::kOpBudget,S::NowMs()))++tokens;CHECK(tokens==8);Mark("TQ07 current Wait returns rate token");
    Reset(session);for(int slot=1;slot<=6;++slot)for(int seq=1;seq<=8;++seq)Queue(session,uint16_t(seq),slot);
    S::HostDrainWaiting(&session);CHECK(Count("use")==33 && S::g_drainSlot==6);
    S::HostDrainWaiting(&session);CHECK(S::g_ack[6]==8);Mark("TQ08 bounded rotating drain");
    Reset(session);Queue(session,1);hook=[&](const std::string& n){if(n=="use")S::HostDrainWaiting(&session);};
    S::HostDrainWaiting(&session);CHECK(Count("use")==1 && S::g_taken==1);Mark("TQ09 recursive drain refused");
    Reset(session);nested=false;hook=[&](const std::string& n){if(n=="use" && !nested){nested=true;S::HostOffer(&session,Op(2),1);}};
    S::HostOffer(&session,Op(1),1);CHECK(Count("use")==1 && S::g_waiting[1].size()==1);Mark("TQ10 nested offer queued");
    Reset(session);for(int seq=1;seq<=32;++seq)Queue(session,uint16_t(seq),1,N::kFuseTowerOpPull);
    S::HostOffer(&session,Op(33,N::kFuseTowerOpPull),1);CHECK(S::g_rejections[1].empty() && S::g_refused[1].count==33 && S::g_ack[1]==33);Mark("TQ11 detached overflow records");
    Reset(session);for(int seq=1;seq<=32;++seq)Queue(session,uint16_t(seq),1,N::kFuseTowerOpPull);
    hook=[](const std::string& n){if(n=="answer")S::OnPeerLeft(1);};S::HostOffer(&session,Op(33,N::kFuseTowerOpPull),1);
    CHECK(Count("answer")==1 && S::g_rejections[1].empty() && S::g_refused[1].count==0);Mark("TQ12 overflow ownership cancellation");
    Reset(session);for(int seq=1;seq<=32;++seq)Queue(session,uint16_t(seq),1,N::kFuseTowerOpPull);nested=false;
    hook=[&](const std::string& n){if(n=="answer"&&!nested){nested=true;S::HostOffer(&session,Op(34),1);}};
    S::HostOffer(&session,Op(33,N::kFuseTowerOpPull),1);CHECK(S::g_waiting[1].size()==1 && S::g_waiting[1][0].p.seq==34);Mark("TQ13 preserve overflow callback arrivals");
    Reset(session);resolved=false;S::HostOffer(&session,Op(1),1);S::HostOffer(&session,Op(1),1);
    CHECK(S::g_waiting[1].size()==1);Mark("TQ14 duplicate queued sequences");
    Reset(session);auto bad=Op(1,N::kFuseTowerOpReset);S::HostOffer(&session,bad,1);bad.kind=255;S::HostOffer(&session,bad,1);
    CHECK(!Count("resolved") && S::g_waiting[1].empty());Mark("TQ15 operation-kind validation");
    Reset(session);Credit();S::g_ack[1]=5;S::PushRefused(1,5);Queue(session,6);S::OnPeerLeft(1);
    CHECK(!S::g_ack[1] && !S::g_refused[1].count && S::g_waiting[1].empty() && !S::FindSpend(1,F::Kind::Coordinate,0,&tower));Mark("TQ16 departure state retirement");
    Reset(session);Credit();Queue(session,1);++world;S::Install(&session);
    CHECK(S::g_waiting[1].empty() && !S::g_ack[1] && !S::FindSpend(1,F::Kind::Coordinate,0,&tower));Mark("TQ17 world reconstruction");
    Reset(session);Credit();++session.generations[1];CHECK(!S::FindSpend(1,F::Kind::Coordinate,0,&tower));Mark("TQ18 spend sender ownership");
    Reset(session);Credit();++tower.serial;CHECK(!S::FindSpend(1,F::Kind::Coordinate,0,&tower));Mark("TQ19 spend tower identity");
    Reset(session);Credit();const auto receipt=S::FindSpend(1,F::Kind::Coordinate,0,&tower);
    hook=[](const std::string& n){if(n=="canInsert")++test::cls.serial;};S::HostOffer(&session,Op(1,N::kFuseTowerOpInsert),1);
    CHECK(S::g_spends[1].ring[0].receipt==receipt && !Count("insert"));Mark("TQ20 spend consumed after validation only");
    for(uint8_t op:{N::kFuseTowerOpButton,N::kFuseTowerOpLever,N::kFuseTowerOpRetract,N::kFuseTowerOpReset,N::kFuseTowerOpPull,N::kFuseTowerOpInsert}){
        Reset(session);if(op==N::kFuseTowerOpInsert)Credit();auto packet=Op(1,op);
        if(op==N::kFuseTowerOpReset)packet.kind=N::kFuseTowerKindRadio;
        S::HostOffer(&session,packet,1);CHECK(S::g_taken==1 && S::g_ack[1]==1 && Count("publish")==1);
    }
    Reset(session);easy=true;S::HostOffer(&session,Op(1,N::kFuseTowerOpInsert),1);
    CHECK(S::g_taken==1 && Count("easy")==1 && !Count("insert"));
    for(int cause=0;cause<4;++cause){
        Reset(session);N::Session other;
        hook=[&](const std::string& name){if(name=="view"){
            if(cause==0)S::OnPeerLeft(1);if(cause==1)++session.stops;
            if(cause==2)session.mode=N::Role::Client;if(cause==3)S::Install(&other);
        }};
        S::HostOffer(&session,Op(1),1);CHECK(!Count("use") && !S::g_taken);
    }
    Reset(session);resolved=false;nested=false;
    hook=[&](const std::string& name){if(name=="resolved"&&!nested){nested=true;S::HostOffer(&session,Op(1),1);}};
    S::HostOffer(&session,Op(1),1);CHECK(S::g_waiting[1].size()==1);
    Reset(session);resolved=false;nested=false;
    hook=[&](const std::string& name){if(name=="resolved"&&!nested){nested=true;
        for(int seq=2;seq<=33;++seq)S::HostOffer(&session,Op(uint16_t(seq),N::kFuseTowerOpPull),1);
    }};
    S::HostOffer(&session,Op(1,N::kFuseTowerOpPull),1);
    CHECK(S::g_rejections[1].size()==33 && S::g_rejections[1][0].p.seq==1);
    hook={};S::HostDrainWaiting(&session);CHECK(S::g_rejections[1].empty() && S::g_ack[1]==33);
    Reset(session);S::HostOffer(&session,Op(0),1);CHECK(calls.empty() && !S::g_taken);
    CHECK(!S::FindSpend(255,F::Kind::Coordinate,0,&tower) && !S::CommitSpend(255,1));
    Reset(session);Queue(session,1,31);S::HostDrainWaiting(&session);CHECK(S::g_ack[31]==1 && S::g_taken==1);
    Reset(session);Credit();const auto firstCredit=S::FindSpend(1,F::Kind::Coordinate,0,&tower);
    Credit();CHECK(S::FindSpend(1,F::Kind::Coordinate,0,&tower)>firstCredit);
    CHECK(S::TakeSpend(1,F::Kind::Coordinate,0,&tower));CHECK(S::FindSpend(1,F::Kind::Coordinate,0,&tower)==firstCredit);
    Reset(session);Credit();const auto pinned=S::g_spends[1].ring[0].receipt;
    hook=[](const std::string& name){if(name=="canInsert")for(int i=0;i<8;++i)S::OnDestroyHeard(1,&fuse);};
    S::HostOffer(&session,Op(1,N::kFuseTowerOpInsert),1);
    CHECK(Count("insert")==1 && S::g_taken==1 && S::g_spends[1].ring[0].receipt==0);
    for(const auto& item:S::g_spends[1].ring)CHECK(item.receipt!=pinned && !item.reserved);
    std::puts("PASS tower op integration: every legal action, peer/session cancellation, active duplicate and overflow during Wait");
}
#endif
