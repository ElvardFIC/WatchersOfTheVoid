#ifndef PROP_SYNCER_CLIENT_SOURCE
#define PROP_SYNCER_CLIENT_SOURCE "../../src/votv-coop/src/coop/props/prop_syncer_client.cpp"
#endif
#include PROP_SYNCER_CLIENT_SOURCE
#include "coop/props/prop_wire_parity.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
namespace S=coop::prop_syncer;namespace N=coop::net;
struct Object {int index=0,serial=1,cls=1;bool live=true;uint32_t eid=1;} objects[6];
uint32_t world=1,serial=1;uint64_t now=10000;
bool held=false,ready=true,takeThrow=true,haveActor=true,readPose=true,readVelocity=true;
void* root=&objects[2];
ue_wrap::FVector location{10,20,30},velocity{1,2,3};ue_wrap::FRotator rotation{0,10,0};
std::function<void(const std::string&)> hook;
std::vector<std::string> calls;std::vector<std::pair<uint32_t,uint8_t>> closed;
void Call(const std::string& name){calls.push_back(name);auto fn=hook;if(fn)fn(name);}
int Count(const char* name){return int(std::count(calls.begin(),calls.end(),name));}
void Reset(N::Session& s){hook={};S::OnDisconnect();++world;++serial;now=10000;
    for(int i=0;i<6;++i){objects[i].index=i;++objects[i].serial;objects[i].live=true;objects[i].cls=(i==2||i==4)?3:1;objects[i].eid=1;}
    held=false;ready=takeThrow=haveActor=readPose=readVelocity=true;root=&objects[2];
    location={10,20,30};rotation={0,10,0};velocity={1,2,3};calls.clear();closed.clear();
    s={};s.hook=[](const std::string& name){Call(name);};S::Tick(s,false);calls.clear();}
S::Entry Entry(uint32_t eid=1,bool syncing=true,bool rest=false){S::Entry e;
    e.ref.Set(&objects[0]);e.cls.Set(&objects[1]);e.revision=++S::g_revision;e.seq=7;e.gen=syncing?5:0;
    e.state=syncing?S::State::Syncing:S::State::Pending;e.key.len=1;e.key.data[0]='p';
    e.everSent=rest;e.sentLoc=location;e.sentRot=rotation;e.lastMoveMs=now-(rest?600:0);
    e.lastInteractMs=now-300;e.lastSendMs=now-30;e.claimSentMs=now;e.epochMs=now-1000;
    S::g_table[eid]=e;return e;}
N::PropSyncAnswerPayload Answer(uint8_t gen=5,bool thrown=false){N::PropSyncAnswerPayload p{};
    p.eid=1;p.seq=7;p.gen=gen;p.result=uint8_t(N::PropSyncResult::Granted);p.kind=uint8_t(thrown?N::PropSyncKind::Throw:N::PropSyncKind::Claim);return p;}
void Mark(const char* label){std::printf("PASS %s\n",label);}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return p?static_cast<test::Object*>(p)->index:-1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<6?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLive(void* p){return p && static_cast<test::Object*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<6&&p==&test::objects[i]&&test::objects[i].live;}
void* ClassOf(void* p){return p?&test::objects[static_cast<test::Object*>(p)->cls]:nullptr;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return nullptr;}
}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace ue_wrap::game_thread {bool IsDefinitelyOffGameThread(){return false;}}
namespace ue_wrap::hot_path {unsigned long long CallerRva(const void*){return 0;}unsigned long CurrentThreadId(){return 0;}}
namespace ue_wrap::log {void Write(Level,const char*,...){}std::string Addr(std::string_view s){return std::string(s);}}
namespace coop::active_drive {uint64_t NowMs(){return test::now;}}
namespace coop::config {bool ResolveFlag(const config_registry::FlagRow&){return false;}}
namespace coop::element {
Registry::Registry(){}Registry::~Registry(){}Registry& Registry::Get(){static Registry r;return r;}
ElementId Registry::EidForActor(void* p)const{return p?static_cast<test::Object*>(p)->eid:kInvalidId;}
}
namespace coop::net_pump {bool HasAnnouncedWorldReady(){return test::ready;}}
namespace coop::dev::prop_syncer_drill {bool IsEnabled(){return false;}}
namespace coop::remote_prop {void* ResolveLiveActorByEid(uint32_t){test::Call("resolve");return test::haveActor?&test::objects[0]:nullptr;}}
namespace ue_wrap::prop {
std::wstring GetInteractableKeyString(void*){test::Call("key");return L"prop";}
VelocityState GetPhysicsVelocity(void*,bool){test::Call("endVelocity");return {test::velocity,{},true};}
}
namespace coop::prop_wire_parity {uint8_t PhysFlagsOf(void*){test::Call("flags");return 0;}}
namespace coop::prop_drive_host {
namespace PR=ue_wrap::prop;
constexpr float kRestVelCmS=coop::prop_drive_rules::kRestVelCmS;
#ifndef PROP_SYNCER_END_SOURCE
#define PROP_SYNCER_END_SOURCE "prop_syncer_end.inc"
#endif
#include PROP_SYNCER_END_SOURCE
bool InAHand(void*){test::Call("hand");return test::held;}
void Claim(void*,const char*){test::Call("hostClaim");}
ClaimLease ClaimOwned(void*,ClaimSource source,const char*,const std::function<bool()>& current){if(current&&!current())return {};test::Call("hostClaim");ClaimLease lease;lease.eid=1;lease.token=1;lease.source=source;return lease;}
}
namespace ue_wrap::engine {
bool TryGetActorLocation(void*,FVector& out){out=test::location;test::Call("location");return test::readPose;}
bool TryGetActorRotation(void*,FRotator& out){out=test::rotation;test::Call("rotation");return test::readPose;}
void* RootComponentOf(void*){test::Call("root");return test::root;}
bool GetActorRootLinearVelocity(CachedObjRef& root,void*,FVector& out){
    if(!root.Get())root.Set(test::root);out=test::velocity;test::Call("velocity");return test::readVelocity;}
}
namespace coop::prop_throw {
bool TakeForGrant(uint16_t,void** actor,uint64_t* start){*actor=&test::objects[0];*start=test::now-1000;test::Call("throw");return test::takeThrow;}
void Ended(void*){test::Call("ended");}
}
namespace coop::props::prop_damage_gate {void ReplayAtGrant(void*){test::Call("replay");}}
namespace coop::prop_drive_stream {void CloseGen(uint32_t eid,uint8_t gen){test::closed.emplace_back(eid,gen);test::Call("close");}}

int main(){using namespace test;
    N::Session s;
    const char* stages[]={"key","sendClaim","hand","location","rotation","velocity","flags","sendEnd","key","replay"};
    const char* causes[]={"world","session","actor","class","episode"};
    for(int stage=0;stage<10;++stage)for(int cause=0;cause<5;++cause){
        Reset(s);if(stage>=2 && stage<=7)Entry(1,true,stage>=6);
        bool fired=false;
        hook=[&](const std::string& name){if(!fired && name==stages[stage]){
            fired=true;if(cause==0)++world;if(cause==1)++serial;if(cause==2)++objects[0].serial;
            if(cause==3)++objects[1].serial;
            if(cause==4){auto replacement=Entry();replacement.rowsSent=99;S::g_table[1]=replacement;}
        }};
        if(stage<=1)S::Claim(&objects[0],N::PropSyncReason::DevDrill);
        else if(stage>=8)S::OnAnswer(Answer(5,true));else S::Tick(s,false);
        CHECK(fired && s.poses.empty() && closed.empty() && S::g_orphanEnds.empty());
        CHECK(s.ends.size()==(stage==7?1:0));
        if(cause==4)CHECK(S::g_table.at(1).rowsSent==99);
        std::printf("PASS PS%02d: %s / %s\n",stage*5+cause+1,stages[stage],causes[cause]);
    }
    Reset(s);Entry();bool grown=false;hook=[&](const std::string& n){if(n=="location"&&!grown){grown=true;for(unsigned i=2;i<700;++i)Entry(i,false);}};
    S::Tick(s,false);CHECK(grown && s.poses.size()==1 && S::g_table.at(1).rowsSent==1);Mark("PC01 owned tick receipts through table growth");
    Reset(s);Entry();hook=[](const std::string& n){if(n=="location")Entry();};S::Tick(s,false);
    CHECK(s.poses.empty() && S::g_table.at(1).rowsSent==0);Mark("PC02 exact episode commit");
    Reset(s);Entry();bool nested=false;hook=[&](const std::string& n){if(n=="location"&&!nested){nested=true;S::Tick(s,false);}};S::Tick(s,false);
    CHECK(Count("location")==1 && s.poses.size()==1);Mark("PC03 recursive tick refused");
    Reset(s);Entry();N::Session other;S::Tick(other,false);CHECK(S::g_table.empty());Mark("PC04 session pointer replacement");
    Reset(s);Entry();++serial;CHECK(!S::SkipsHostPose(1));S::Tick(s,false);CHECK(S::g_table.empty());Mark("PC05 same-pointer session serial");
    Reset(s);Entry();++s.stops;S::Tick(s,false);CHECK(S::g_table.empty());Mark("PC06 stop retirement");
    Reset(s);Entry();++world;CHECK(!S::AuthorsActorHere(&objects[0]));S::Tick(s,false);CHECK(S::g_table.empty());Mark("PC07 world retirement");
    Reset(s);Entry();s.mode=N::Role::Host;S::Tick(s,true);CHECK(S::g_table.empty());Mark("PC08 role retirement");
    Reset(s);bool registered=false;hook=[&](const std::string& n){if(n=="sendClaim")registered=S::g_table.contains(1);};
    S::Claim(&objects[0],N::PropSyncReason::DevDrill);CHECK(registered && s.claims.size()==1);Mark("PC09 Pending precedes send");
    Reset(s);hook=[&](const std::string& n){if(n=="sendClaim"){auto answer=Answer();answer.seq=s.claims.back().seq;S::OnAnswer(answer);}};
    S::Claim(&objects[0],N::PropSyncReason::DevDrill);CHECK(S::g_table.at(1).state==S::State::Syncing && S::CountRef(S::Count::ClaimsSent)==1);Mark("PC10 early grant preserved");
    Reset(s);s.sendOk=false;hook=[](const std::string& n){if(n=="sendClaim")Entry();};S::Claim(&objects[0],N::PropSyncReason::DevDrill);
    CHECK(S::g_table.at(1).state==S::State::Syncing);Mark("PC11 refused send preserves replacement");
    Reset(s);Entry(2);++objects[1].serial;auto answer=Answer(7,true);
    hook=[&](const std::string& n){if(n=="key"){answer.eid=2;answer.gen=9;}};S::OnAnswer(answer);
    CHECK(S::g_table.contains(1) && S::g_table.at(1).gen==7 && Count("replay")==1);Mark("PC12 owned answer payload");
    Reset(s);Entry();S::g_seenGrants[1]={5,7,uint8_t(N::PropSyncKind::Claim)};S::OnAnswer(Answer());CHECK(s.ends.empty() && closed.empty());Mark("PC13 duplicate live grant");
    Reset(s);Entry();S::OnAnswer(Answer(4));CHECK(s.ends.empty() && S::g_table.at(1).gen==5);Mark("PC14 older live generation");
    Reset(s);hook=[](const std::string& n){if(n=="key")++objects[1].serial;};S::OnAnswer(Answer(5,true));
    CHECK(S::g_orphanEnds.empty() && s.ends.empty());Mark("PC15 cancelled throw has no orphan fallback");
    Reset(s);Entry();hook=[](const std::string& n){if(n=="ended")Entry();};S::OnHostEnd(1,5);
    CHECK(S::g_table.contains(1));Mark("PC16 detach before throw cleanup");
    Reset(s);S::SetBackoff(1,now-2000,true);hook=[](const std::string& n){if(n=="hand")S::SetBackoff(1,now,true);};
    S::Claim(&objects[0],N::PropSyncReason::DevDrill);CHECK(s.claims.empty() && S::g_backoff.at(1).untilMs>now);Mark("PC17 owned backoff query");
    Reset(s);auto pending=Entry(1,false);pending.claimSentMs=now+100;S::g_table[1]=pending;S::Tick(s,false);
    CHECK(S::g_table.contains(1) && S::CountRef(S::Count::PendingExpired)==0);Mark("PC18 backwards pending clock");
    Reset(s);auto resting=Entry(1,true,true);resting.lastMoveMs=resting.lastInteractMs=resting.epochMs=now+100;S::g_table[1]=resting;S::Tick(s,false);
    CHECK(s.ends.empty());Mark("PC19 backwards rest and age clocks");
    Reset(s);Entry();location.X=std::numeric_limits<float>::quiet_NaN();S::Tick(s,false);CHECK(s.poses.empty() && s.ends.empty());Mark("PC20 finite pose");
    Reset(s);Entry();velocity.X=std::numeric_limits<float>::infinity();S::Tick(s,false);CHECK(s.poses.empty());Mark("PC21 finite velocity");
    Reset(s);auto moving=Entry();moving.rootComp.Set(&objects[2]);S::g_table[1]=moving;root=&objects[4];S::Tick(s,false);
    CHECK(s.poses.size()==1 && S::g_table.at(1).rootComp.Is(&objects[4]));Mark("PC22 actual root component refresh");
    Reset(s);Entry();hook=[](const std::string& n){if(n=="velocity")held=true;};S::Tick(s,false);
    CHECK(s.poses.empty() && S::g_table.empty() && S::g_backoff.at(1).inHand);Mark("PC23 final hand ownership");
    Reset(s);Entry(1,true,true);hook=[](const std::string& n){if(n=="rotation")S::Claim(&objects[0],N::PropSyncReason::DevDrill);};S::Tick(s,false);
    CHECK(s.ends.empty() && S::g_table.at(1).lastInteractMs==now);Mark("PC24 callback keepalive");
    Reset(s);Entry(1,true,true);s.sendOk=false;S::Tick(s,false);CHECK(closed.empty() && S::g_table.contains(1));
    s.sendOk=true;S::Tick(s,false);CHECK(S::g_table.empty() && s.ends.size()==1 && closed.size()==1);S::Tick(s,false);CHECK(closed.size()==1);
    Reset(s);takeThrow=false;s.sendOk=false;S::OnAnswer(Answer(8,true));CHECK(S::g_orphanEnds.size()==1 && closed.empty());
    s.sendOk=true;S::Tick(s,false);CHECK(S::g_orphanEnds.empty() && closed.size()==1);S::OnAnswer(Answer(8,true));CHECK(closed.size()==1);
    Reset(s);takeThrow=false;s.sendOk=false;S::OnAnswer(Answer(8,true));objects[0].eid=2;
    s.sendOk=true;S::Tick(s,false);CHECK(S::g_orphanEnds.empty() && s.ends.empty() && closed.empty());
    Reset(s);takeThrow=false;hook=[](const std::string& name){if(name=="location")objects[0].eid=2;};
    S::OnAnswer(Answer(8,true));CHECK(s.ends.empty() && closed.empty());hook={};S::Tick(s,false);CHECK(S::g_orphanEnds.empty());
    Reset(s);takeThrow=false;s.sendOk=false;S::OnAnswer(Answer(8,true));S::OnHostEnd(1,9);CHECK(S::g_orphanEnds.contains(1));
    S::OnHostEnd(1,8);s.sendOk=true;S::Tick(s,false);CHECK(S::g_orphanEnds.empty() && s.ends.empty() && closed.empty());
    Mark("PC25 accepted End retry and once-only close");
    Reset(s);Entry();hook=[](const std::string& name){if(name=="location")objects[0].cls=5;};S::Tick(s,false);
    CHECK(s.poses.empty());
    Reset(s);Entry();hook=[](const std::string& name){if(name=="velocity")root=&objects[4];};S::Tick(s,false);
    CHECK(s.poses.empty());hook={};S::Tick(s,false);CHECK(s.poses.size()==1 && S::g_table.at(1).rootComp.Is(&objects[4]));
    Reset(s);auto wrapped=Entry();wrapped.gen=255;S::g_table[1]=wrapped;S::g_seenGrants[1]={255,7,uint8_t(N::PropSyncKind::Claim)};
    S::OnAnswer(Answer(1,true));CHECK(S::g_table.at(1).gen==1 && Count("replay")==1);
    Reset(s);Entry();S::g_table.at(1).nextProbeMs=now+100;S::Tick(s,false);CHECK(!Count("location") && s.poses.empty());
    Reset(s);Entry();s.turn=N::PoseTurn::Wait;S::Tick(s,false);CHECK(!Count("location") && s.poses.empty());
    Reset(s);Entry();objects[0].eid=2;CHECK(!S::SkipsHostPose(1));S::Tick(s,false);CHECK(S::g_table.empty() && s.poses.empty());
    Reset(s);Entry(1,true,true);hook=[](const std::string& name){if(name=="flags")root=&objects[4];};S::Tick(s,false);
    CHECK(s.ends.empty() && closed.empty() && !Count("endVelocity"));
    Reset(s);Entry(1,true,true);hook=[](const std::string& name){if(name=="flags")held=true;};S::Tick(s,false);
    CHECK(s.ends.empty() && S::g_table.empty());
    Reset(s);Entry();hook=[&](const std::string& name){if(name=="location"){S::Tick(other,false);Entry();}};
    S::Tick(s,false);CHECK(s.poses.empty() && S::g_session==&other && S::g_table.at(1).rowsSent==0);
    Reset(s);takeThrow=false;bool replaced=false;hook=[&](const std::string& name){if(name=="sendEnd"&&!replaced){replaced=true;S::OnAnswer(Answer(9,true));}};
    S::OnAnswer(Answer(8,true));CHECK(S::g_orphanEnds.at(1).answer.gen==9 && closed.empty());
    hook={};S::Tick(s,false);CHECK(S::g_orphanEnds.empty() && closed.size()==1 && closed[0].second==9);
    Reset(s);Entry();++objects[1].serial;CHECK(!S::AuthorsActorHere(&objects[0]));S::Tick(s,false);
    CHECK(S::g_table.empty() && Count("ended")==1);
    Reset(s);Entry();auto revoked=Answer();revoked.result=uint8_t(N::PropSyncResult::Revoked);revoked.seq=8;
    S::OnAnswer(revoked);CHECK(S::g_table.contains(1));revoked.seq=7;revoked.kind=uint8_t(N::PropSyncKind::Throw);
    S::OnAnswer(revoked);CHECK(S::g_table.contains(1));revoked.kind=uint8_t(N::PropSyncKind::Claim);
    S::OnAnswer(revoked);CHECK(S::g_table.empty() && Count("ended")==1);
    Reset(s);Entry(1,false);auto refusal=Answer(0,true);refusal.result=uint8_t(N::PropSyncResult::Refused);
    S::OnAnswer(refusal);CHECK(S::g_table.contains(1));refusal.kind=uint8_t(N::PropSyncKind::Claim);
    S::OnAnswer(refusal);CHECK(S::g_table.empty() && S::g_backoff.contains(1));
    Reset(s);S::g_seenGrants[1]={5,1,uint8_t(N::PropSyncKind::Claim)};Entry(1,false);S::OnAnswer(Answer(250));
    CHECK(S::g_table.at(1).gen==250);
    Reset(s);S::g_seenGrants[1]={5,1,uint8_t(N::PropSyncKind::Claim)};Entry(1,false);S::OnAnswer(Answer(5));
    CHECK(S::g_table.at(1).state==S::State::Syncing && S::g_table.at(1).gen==5);
    Reset(s);S::g_seenGrants[1]={5,7,uint8_t(N::PropSyncKind::Throw)};S::OnAnswer(Answer(5,true));
    CHECK(S::g_table.at(1).thrown && Count("replay")==1);
    Reset(s);Entry();takeThrow=false;S::OnAnswer(Answer(4,true));
    CHECK(S::g_table.at(1).gen==5 && !Count("replay") && s.ends.empty());
    Reset(s);Entry();S::OnAnswer(Answer(4,true));CHECK(S::g_table.at(1).thrown && Count("replay")==1);
    Reset(s);const auto original=Entry();objects[0].eid=2;S::OnAnswer(Answer(8,true));
    CHECK(S::g_table.at(1).revision==original.revision && !Count("replay") && S::g_orphanEnds.empty());
    std::puts("PASS client syncer integration: live class/root replacement, generation wrap, probe/send turn and accepted early-grant accounting");
}
