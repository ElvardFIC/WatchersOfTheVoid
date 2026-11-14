#ifndef TOWER_ROWS_SOURCE
#define TOWER_ROWS_SOURCE "../../src/votv-coop/src/coop/world/fuse_tower_rows.cpp"
#endif
#ifndef TOWER_KIND_SOURCE
#define TOWER_KIND_SOURCE "../../src/votv-coop/src/coop/world/fuse_tower_kind.cpp"
#endif
#include TOWER_ROWS_SOURCE
#include TOWER_KIND_SOURCE
#include "facts_test_support.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
namespace CT=ue_wrap::coord_tower;namespace RT=ue_wrap::radio_tower;
struct Obj{int id=0,serial=1;bool live=true;}objects[16];
CT::State coord[16];RT::State radio;
uint32_t world=1,serial=1;uint64_t classes=1;
int coords=3,discoveryCount=-1;bool haveRadio=true,red=false;
int repairFuse0=-1;
bool prediction=false,clearPredictionOnAck=false;
std::vector<std::string> calls;
std::string refused;
std::function<void(const std::string&)> hook;
coop::net::Session session,other;
bool Do(const std::string& name){calls.push_back(name);auto cb=hook;if(cb)cb(name);return name!=refused;}
int Count(const std::string& name){return int(std::count(calls.begin(),calls.end(),name));}
int Index(void* p){return static_cast<Obj*>(p)->id;}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return p?test::Index(p):-1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<16?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<16&&p==&test::objects[i]&&test::objects[i].live;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return CurrentWorld();}
}
namespace ue_wrap::object_index {uint64_t ClassSetVersion(){return test::classes;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}uint32_t Next(){return ++test::serial;}}
namespace coop::roster_ledger {void RegisterPerSlotClear(PerSlotClearFn,void*) {}}
namespace coop::fuse_tower_ops {
constexpr size_t kRefusedCap=33;
#include "tower_refused_fifo.inc"
roster_ledger::PerSlotState<RefusedFifo> g_refused;
std::array<uint16_t,32> ack{};
void FillAcks(net::FuseTowerPayload& p,int slot){for(int i=0;i<32;++i)p.ack[i]=ack[i];if(slot>0&&g_refused[slot].count)p.refused[slot]=g_refused[slot].At(0);}
#ifndef TOWER_TOLD_SOURCE
#define TOWER_TOLD_SOURCE "tower_refused_told.inc"
#endif
#include TOWER_TOLD_SOURCE
bool RefusedPending(uint8_t slot){return g_refused[slot].count!=0;}
void OnAcks(uint16_t,uint16_t){test::Do("acks");if(test::clearPredictionOnAck){test::prediction=false;test::clearPredictionOnAck=false;}}
void Overlay(net::FuseTowerRow& row){test::Do("overlay");if(test::prediction&&row.kind==0&&row.id==0)row.fuses[0]=0;}
bool RedOpen(){return test::red;}
}
namespace ue_wrap::coord_tower {
bool EnsureResolved(){return true;}
int32_t ReadAll(void** out,int32_t cap){test::Do("c.discover");for(int i=0;i<std::min(cap,test::coords);++i)out[i]=&test::objects[i==0?0:i+2];return test::discoveryCount>=0?test::discoveryCount:std::min(cap,test::coords);}
int32_t IdOf(void* p){return test::coord[test::Index(p)].id;}
bool Read(void* p,State& out){out=test::coord[test::Index(p)];return test::Do("c.read");}
bool WriteBroken(void* p,bool value){if(!test::Do("c.writeBroken"))return false;test::coord[test::Index(p)].isBroken=value;return true;}
bool WriteLeverMoving(void* p,bool value){if(!test::Do("c.writeMoving"))return false;test::coord[test::Index(p)].leverMoving=value;return true;}
bool WriteFuse(void* p,int32_t i,uint8_t value){if(!test::Do("c.writeFuse"))return false;test::coord[test::Index(p)].fuses[i]=value;return true;}
bool WriteLights(void* p,const bool* values,int32_t n){if(!test::Do("c.writeLights"))return false;std::copy(values,values+n,test::coord[test::Index(p)].lights);return true;}
bool UpdBroken(void*){return test::Do("c.paintBroken");}
bool UpdPuzzle(void*){return test::Do("c.paintPuzzle");}
bool UpdFuses(void*){return test::Do("c.paintFuses");}
bool SolvePuzzle(void* p){const bool ok=test::Do("c.repair");if(ok){auto& state=test::coord[test::Index(p)];state.isBroken=false;std::fill(state.lights,state.lights+state.lightCount,true);}return ok;}
bool Scramble(void*){std::abort();}
bool MoveLever(void* p,bool up){const bool ok=test::Do("c.lever");if(ok)test::coord[test::Index(p)].leverUp=up;return ok;}
bool Play(void*,Sound sound){return test::Do("c.sound"+std::to_string(int(sound)));}
bool ReadLookAt(void*,LookAt&){std::abort();}
bool ReadFuseLook(void*,int32_t&){std::abort();}
bool Use(void* p,void*,const LookAt&){const bool ok=test::Do("c.panel");if(ok)test::coord[test::Index(p)].isAnim=true;return ok;}
bool PartLocation(void*,const LookAt&,FVector&){std::abort();}
bool IsFuse(void*){std::abort();}
void* SpawnFuse(const FVector&){std::abort();}
void* SpawnFuseTracked(const FVector&,bool&,const std::function<bool()>&){std::abort();}
bool IsPulledFuse(void*){std::abort();}
}
namespace ue_wrap::radio_tower {
void* Instance(){test::Do("r.discover");return test::haveRadio?&test::objects[1]:nullptr;}
bool IsTower(void* p){return p==&test::objects[1];}
bool Read(void*,State& out){out=test::radio;return test::Do("r.read");}
bool WriteAnimation(void*,bool value){if(!test::Do("r.writeAnim"))return false;test::radio.animation=value;return true;}
bool WriteBroken(void*,bool value){if(!test::Do("r.writeBroken"))return false;test::radio.isBroken=value;return true;}
bool WriteFuse(void*,int32_t i,uint8_t value){if(!test::Do("r.writeFuse"))return false;test::radio.fuses[i]=value;return true;}
bool WriteCells(void*,const bool* values,int32_t n){if(!test::Do("r.writeCells"))return false;std::copy(values,values+n,test::radio.cells);return true;}
bool UpdPuzzle(void*){return test::Do("r.paintPuzzle");}
bool UpdFuses(void*){return test::Do("r.paintFuses");}
bool SetBroken(void*,bool value,bool){if(!test::Do("r.setBroken"))return false;test::radio.isBroken=value;return true;}
bool CompleteMinigame(void*){const bool ok=test::Do("r.repair");test::repairFuse0=test::radio.fuses[0];
    if(ok){std::fill(test::radio.cells,test::radio.cells+test::radio.cellCount,true);test::radio.isBroken=false;}return ok;}
bool Scramble(void*){std::abort();}
bool PlayLever(void*,Lever lever,bool){return test::Do(lever==Lever::Confirm?"r.confirm":"r.reset");}
bool Play(void*,Sound sound){return test::Do("r.sound"+std::to_string(int(sound)));}
bool Alert(void*,bool damaged){return test::Do(damaged?"r.damage":"r.brokenAlert");}
bool ReadLookAt(void*,LookAt&){std::abort();}
bool ReadFuseLook(void*,int32_t&){std::abort();}
bool Use(void*,void*,const LookAt&){std::abort();}
bool PartLocation(void*,const LookAt&,FVector&){std::abort();}
}
namespace ue_wrap::game_rules {int ReadDifficulty(){return 2;}}
namespace test {
namespace Rows=coop::fuse_tower_rows;namespace Kind=coop::fuse_tower_kind;namespace Ops=coop::fuse_tower_ops;namespace N=coop::net;
void Reset(N::Role role=N::Role::Client){
    Rows::OnDisconnect();++world;++serial;++classes;session={};other={};session.mode=role;
    hook={};refused.clear();calls.clear();haveRadio=true;red=false;coords=3;discoveryCount=-1;repairFuse0=-1;
    prediction=clearPredictionOnAck=false;
    for(int i=0;i<16;++i){objects[i].id=i;++objects[i].serial;objects[i].live=true;coord[i]={};coord[i].id=i==0?0:i-2;coord[i].fuseCount=3;coord[i].lightCount=3;}
    radio={};radio.fuseCount=3;radio.cellCount=4;radio.width=radio.height=2;
    for(int i=0;i<32;++i){Ops::g_refused[i]={};Ops::ack[i]=0;}
    coop::players::Registry::Get().body=&objects[2];coop::players::Registry::Get().peer=31;
    Rows::Install(&session);
}
N::FuseTowerPayload Packet(bool r=false){
    N::FuseTowerPayload p{};p.count=1;p.rows[0].kind=r?N::kFuseTowerKindRadio:N::kFuseTowerKindCoordinate;
    p.rows[0].id=0;p.rows[0].lightCount=r?4:3;p.rows[0].fuseCount=3;return p;
}
void Retry(){Rows::g_waitLookMs=0;Rows::Tick();}
void OneHook(const std::string& at,std::function<void()> cb){hook=[at,cb](const std::string& name){if(name==at){hook={};cb();}};}
void Basic(){
    Reset(N::Role::Host);session.sendOk=false;Rows::HostSendNow();CHECK(!Rows::g_haveSent&&Rows::g_acksDirty);
    session.sendOk=true;Rows::Tick();CHECK(Rows::g_haveSent&&!Rows::g_acksDirty&&session.sent.size()==1);
    Rows::Tick();CHECK(session.sent.size()==1);
    Reset(N::Role::Host);session.onSend=[] {Rows::HostSendNow();};Rows::HostSendNow();CHECK(Rows::g_acksDirty&&!Rows::g_haveSent);
    Rows::Tick();CHECK(!Rows::g_acksDirty&&Rows::g_haveSent&&session.sent.size()==2);
    Reset(N::Role::Host);session.onSend=[] {Rows::OnDisconnect();Rows::Install(&other);};Rows::HostSendNow();CHECK(!Rows::g_haveSent&&Rows::g_broadcasts==0);
    for(int kind=0;kind<5;++kind){Reset();auto p=Packet();Rows::OnRows(p);CHECK(Rows::g_haveRows);
        if(kind==0)Rows::Install(&other);if(kind==1)++serial;if(kind==2)++session.stops;if(kind==3)++world;if(kind==4)session.mode=N::Role::Host;
        N::FuseTowerPayload out{};CHECK(!Rows::LastRows(out)&&Rows::g_towerCount==0);
    }
    Reset();Rows::OnRows(Packet());Rows::OnDisconnect();CHECK(Rows::g_session.load()==nullptr);calls.clear();Rows::OnRows(Packet());Rows::Tick();CHECK(calls.empty());
    for(int kind=0;kind<3;++kind){Reset();if(kind==0)session.live=false;if(kind==1)session.linked=false;if(kind==2)session.mode=N::Role::Host;
        Rows::OnRows(Packet());CHECK(!Rows::g_haveRows);
    }
    for(int kind=0;kind<8;++kind){Reset();auto p=Packet();switch(kind){
        case 0:p.count=5;break;case 1:p.rows[0].kind=2;break;case 2:p.rows[0].flags=0x40;break;
        case 3:p.rows[0].id=-1;break;case 4:p.rows[0].kind=1;p.rows[0].id=1;break;
        case 5:p.rows[0].lights=1u<<12;break;case 6:p.rows[0].fuses[0]=3;break;case 7:p.rows[0].lightCount=26;break;}
        Rows::OnRows(p);CHECK(!Rows::g_haveRows);
    }
    Reset();auto p=Packet();p.count=2;p.rows[1]=p.rows[0];p.rows[1].flags=N::kFuseTowerBroken;Rows::OnRows(p);CHECK(Rows::g_rows.count==1);
    p=Packet(true);Rows::OnRows(p);CHECK(std::count_if(Rows::g_kept.begin(),Rows::g_kept.end(),[](auto& k){return k.used;})==1&&Rows::g_kept[0].row.kind==1);
    Reset();OneHook("c.discover",[] {++world;});Rows::OnRows(Packet());CHECK(Rows::g_towerCount==0);
    Reset(N::Role::Host);OneHook("r.read",[] {++objects[1].serial;});Rows::HostSendNow();CHECK(session.sent.empty());
    Reset(N::Role::Host);OneHook("r.read",[] {Rows::OnDisconnect();});Rows::HostSendNow();CHECK(session.sent.empty()&&Rows::g_towerCount==0);
    Reset(N::Role::Host);discoveryCount=99;Rows::HostSendNow();CHECK(Rows::g_towerCount==4&&session.sent.size()==1);
    Reset();Rows::OnRows(Packet());auto before=Count("c.paintPuzzle");++objects[0].serial;Retry();Retry();CHECK(Count("c.paintPuzzle")>before);
    std::puts("PASS tower rows scope/admission/discovery/broadcast regression");
}
void Delivery(){
    Reset(N::Role::Host);session.ready[31]=false;Rows::QueueConnectBroadcastForSlot(31);CHECK(Rows::g_join[31].owed&&session.sent.empty());
    session.ready[31]=true;Rows::Tick();CHECK(!Rows::g_join[31].owed&&session.sent.front().slot==31);
    Reset(N::Role::Host);session.ready[31]=false;Rows::QueueConnectBroadcastForSlot(31);++session.generations[31];session.ready[31]=true;Rows::Tick();
    CHECK(!Rows::g_join[31].owed&&std::none_of(session.sent.begin(),session.sent.end(),[](auto& s){return s.slot==31;}));
    Reset(N::Role::Host);session.onSend=[] {Rows::QueueConnectBroadcastForSlot(31);};Rows::QueueConnectBroadcastForSlot(31);CHECK(Rows::g_join[31].owed);
    Reset(N::Role::Host);Ops::g_refused[31].seq[0]=10;Ops::g_refused[31].count=1;Rows::HostAnswerOwed(31);
    session.onSend=[] {Ops::g_refused[31].seq[0]=20;Rows::HostAnswerOwed(31);};Rows::ServeDeliveries(true);
    CHECK(Ops::g_refused[31].count==1&&Ops::g_refused[31].At(0)==20&&Rows::g_answer[31].owed);
    Rows::ServeDeliveries(true);CHECK(!Ops::g_refused[31].count&&!Rows::g_answer[31].owed);
    Reset(N::Role::Host);for(int i=1;i<32;++i){Ops::g_refused[i].count=1;Ops::g_refused[i].seq[0]=1;Rows::HostAnswerOwed(uint8_t(i));}
    Rows::ServeDeliveries(true);CHECK(session.sent.size()==8);Rows::ServeDeliveries(true);CHECK(session.sent.size()==16);
    Reset(N::Role::Host);Rows::HostAnswerOwed(31);session.onSend=[] {++session.generations[31];};Rows::ServeDeliveries(true);
    CHECK(Rows::g_answer[31].owed);Rows::ServeDeliveries(true);CHECK(!Rows::g_answer[31].owed);
    std::puts("PASS tower generation-bound slot-31 join/refusal debt, exact sequence receipts and bounded fair delivery");
}
void Apply(){
    Reset();auto canonical=Packet();canonical.rows[0].fuses[0]=2;Rows::OnRows(canonical);
    coord[0].fuses[0]=0;prediction=true;Rows::OnRows(canonical);CHECK(coord[0].fuses[0]==0);
    clearPredictionOnAck=true;Rows::OnRows(canonical);CHECK(coord[0].fuses[0]==2&&!Rows::Pending());
    Reset();auto p=Packet();refused="c.read";Rows::OnRows(p);CHECK(Rows::Pending());refused.clear();Retry();CHECK(!Rows::Pending());
    Reset();refused="c.paintPuzzle";Rows::OnRows(p);CHECK(Rows::Pending());refused.clear();Retry();CHECK(!Rows::Pending()&&Count("c.paintPuzzle")==2);
    Reset();Rows::OnRows(Packet());calls.clear();p=Packet();p.rows[0].lights=1;refused="c.paintPuzzle";
    Rows::OnRows(p);CHECK(Rows::Pending()&&Count("c.sound0")==0);refused.clear();Retry();
    CHECK(!Rows::Pending()&&Count("c.paintPuzzle")==2&&Count("c.sound0")==1);
    Reset();p=Packet();p.rows[0].lights=1;refused="c.writeLights";Rows::OnRows(p);CHECK(Rows::Pending()&&!coord[0].lights[0]);refused.clear();Retry();CHECK(coord[0].lights[0]&&!Rows::Pending());
    Reset();p=Packet();OneHook("c.paintPuzzle",[p]{auto next=p;next.rows[0].lights=2;Rows::OnRows(next);});Rows::OnRows(p);
    CHECK(Rows::Pending());Retry();CHECK(!Rows::Pending()&&coord[0].lights[1]);
    Reset();OneHook("c.paintPuzzle",[] {Rows::OnDisconnect();Rows::Install(&other);});Rows::OnRows(Packet());CHECK(Rows::g_applied==0&&!Rows::Applying());
    Reset();OneHook("c.paintPuzzle",[] {throw std::runtime_error("native");});try{Rows::OnRows(Packet());CHECK(false);}catch(const std::runtime_error&){}
    CHECK(!Rows::Applying()&&Rows::Pending());hook={};Retry();CHECK(!Rows::Pending());
    Reset();auto newer=Packet();newer.rows[0].lights=4;OneHook("acks",[newer]{Rows::OnRows(newer);});Rows::OnRows(Packet());CHECK(coord[0].lights[2]);
    Reset();Rows::OnRows(Packet());p=Packet();p.rows[0].lights=1;p.rows[0].flags=N::kFuseTowerLeverUp;
    refused="c.writeMoving";Rows::OnRows(p);CHECK(Rows::Pending());const int lever=Count("c.lever"),click=Count("c.sound0");
    refused.clear();Retry();CHECK(!Rows::Pending()&&Count("c.lever")==lever&&Count("c.sound0")==click);
    // Same-row acknowledgements cannot re-admit a native effect or discard its retry debt.
    Rows::OnRows(p);CHECK(Count("c.lever")==lever&&Count("c.sound0")==click);
    Reset();Rows::OnRows(Packet());p=Packet();p.rows[0].flags=N::kFuseTowerLeverUp;refused="c.lever";
    Rows::OnRows(p);Rows::OnRows(p);CHECK(Count("c.lever")==1&&!Rows::Pending());
    Reset();p=Packet();p.rows[0].flags=N::kFuseTowerBroken;Rows::OnRows(p);p.rows[0].flags=0;p.rows[0].lights=1;Rows::OnRows(p);
    CHECK(Count("c.repair")==1&&coord[0].lights[0]&&!coord[0].lights[1]&&!coord[0].lights[2]);
    Reset();p=Packet(true);p.rows[0].flags=N::kFuseTowerBroken;p.rows[0].fuses[0]=2;Rows::OnRows(p);
    p.rows[0].flags=0;p.rows[0].lights=4;for(int i=0;i<3;++i)p.rows[0].fuses[i]=1;Rows::OnRows(p);
    CHECK(Count("r.repair")==1&&repairFuse0==1&&!radio.isBroken&&radio.cells[2]&&!radio.cells[0]);
    Reset();Rows::OnRows(Packet(true));CHECK(Count("r.setBroken")==0);auto r=Packet(true);r.rows[0].flags=N::kFuseTowerBroken|N::kFuseTowerLeverMoving|N::kFuseTowerLeverUp|N::kFuseTowerResetMoving;
    r.rows[0].fuses[0]=2;refused="r.writeAnim";Rows::OnRows(r);CHECK(Rows::Pending());
    auto confirm=Count("r.confirm"),reset=Count("r.reset"),alert=Count("r.brokenAlert"),stateEvent=Count("r.setBroken");refused.clear();Retry();
    CHECK(!Rows::Pending()&&Count("r.confirm")==confirm&&Count("r.reset")==reset&&Count("r.brokenAlert")==alert&&Count("r.setBroken")==stateEvent);
    Reset();Rows::OnRows(Packet());auto open=Packet();open.rows[0].flags=N::kFuseTowerOpened;Rows::OnRows(open);Rows::ConvergePanels();
    auto asked=Rows::g_kept[0].panelAskedMs;CHECK(asked>0&&Count("c.panel")==1);Rows::ConvergePanels();CHECK(Count("c.panel")==1);
    Reset();Rows::OnRows(Packet());open=Packet();open.rows[0].flags=N::kFuseTowerOpened;Rows::OnRows(open);
    OneHook("c.panel",[] {++objects[2].serial;});Rows::ConvergePanels();CHECK(Rows::g_kept[0].panelAskedMs==0&&Rows::g_edges[0].panels==0);
    Reset();p=Packet();p.count=2;p.rows[1]=p.rows[0];p.rows[1].id=1;p.rows[0].flags=N::kFuseTowerOpened;
    Rows::OnRows(p);Rows::ConvergePanels();CHECK(Count("c.panel")==1);coord[0].isAnim=false;Rows::g_kept[0].panelAskedMs=Rows::NowMs()+20000;
    std::swap(p.rows[0],p.rows[1]);Rows::OnRows(p);Rows::ConvergePanels();CHECK(Count("c.panel")==1);
    std::puts("PASS tower owned apply/reentry, retryable paint debt, once-only effects and identity-bound panel retries");
}
void KindGuards(){
    for(bool r:{false,true})for(const auto& at:r?std::vector<std::string>{"r.read","r.writeCells","r.setBroken","r.paintPuzzle","r.writeFuse","r.paintFuses","r.sound3","r.writeAnim"}:
        std::vector<std::string>{"c.read","c.writeLights","c.writeBroken","c.paintPuzzle","c.paintBroken","c.writeFuse","c.paintFuses","c.sound3","c.lever","c.writeMoving"}){
        Reset();auto base=Packet(r);Rows::OnRows(base);calls.clear();auto change=base;change.rows[0].lights=1;change.rows[0].flags=N::kFuseTowerBroken|N::kFuseTowerLeverUp;
        if(r)change.rows[0].flags|=N::kFuseTowerLeverMoving;change.rows[0].fuses[0]=0;
        if(r)radio.fuses[0]=2;else coord[0].fuses[0]=2;
        bool hit=false;OneHook(at,[&hit]{hit=true;++serial;});Rows::OnRows(change);CHECK(hit&&calls.back()==at);
    }
    Reset();auto invalid=Packet();invalid.rows[0].lightCount=25;Rows::OnRows(invalid);CHECK(Rows::Pending());
    Reset();radio.cellCount=26;Rows::OnRows(Packet(true));CHECK(Rows::Pending());
    std::puts("PASS actual tower kind coordinator guards around coordinate/radio native steps and count bounds");
}
}
namespace test {
void KindOperations(){
    for(bool radioKind:{false,true})for(int op=0;op<3;++op){
        Reset();calls.clear();bool current=false;
        const auto kind=radioKind?Kind::Kind::Radio:Kind::Kind::Coordinate;
        void* actor=&objects[radioKind?1:0];
        const auto run=[&]{
            if(op==0)Kind::TakePull(kind,actor,0,[&]{return current;});
            if(op==1)Kind::TakeInsert(kind,actor,0,[&]{return current;});
            if(op==2)Kind::TakeEasyInsert(kind,actor,0,[&]{return current;});
        };
        run();CHECK(calls.empty());current=true;
        hook=[&](const std::string&){current=false;};run();CHECK(calls.size()==1);
        if(op<2 || radioKind){
            calls.clear();hook={};current=true;refused=radioKind?"r.writeFuse":"c.writeFuse";
            run();CHECK(calls.size()==1);refused.clear();
        }
    }
    std::puts("PASS tower operation helpers stop later native writes/effects after caller cancellation");
}
}
int main(){test::Basic();test::Delivery();test::Apply();test::KindGuards();test::KindOperations();}
