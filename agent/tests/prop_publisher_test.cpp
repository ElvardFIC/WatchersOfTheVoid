#ifndef PROP_PUBLISHER_SOURCE
#define PROP_PUBLISHER_SOURCE "../../src/votv-coop/src/coop/props/prop_drive_host.cpp"
#endif
#include PROP_PUBLISHER_SOURCE
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <cstdarg>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
namespace H=coop::prop_drive_host;namespace N=coop::net;
struct Object {int index=0,serial=1,cls=1;bool live=true;uint32_t eid=1;
#ifdef KERFUS_MANAGER_FIXTURE
uint8_t state=0x80;float stateEnergy=10;
#ifdef OMEGA_COMMAND_FIXTURE
void* hold=nullptr;void* grab=nullptr;void* viewer=nullptr;ue_wrap::reflection::FName handName{};
#endif
#endif
} objects[48];
uint32_t world=1,serial=1;uint64_t now=10000;
bool held=false,locationOK=true,rotationOK=true,velocityOK=true;
void* root=&objects[2];
ue_wrap::FVector location{10,20,30},velocity{1,2,3};ue_wrap::FRotator rotation{0,10,0};
std::function<void(const std::string&)> hook;
std::vector<std::string> calls;std::vector<coop::prop_syncer_host::Ending> clientEnds;
#ifdef PROP_VERB_FIXTURE
std::wstring nativeKey=L"prop";
bool propClass=true,suppressed=false,remoteHand=false,hotbarHand=false,robotHold=false;
void* lastHand=nullptr;void* worldObject=nullptr;
std::vector<std::string> logLines;
std::function<void(const H::Driven&)> transition;
#endif
void Call(const std::string& name){calls.push_back(name);auto fn=hook;if(fn)fn(name);}
int Count(const char* name){return int(std::count(calls.begin(),calls.end(),name));}
void Reset(N::Session& s){hook={};H::OnDisconnect();++world;++serial;now=10000;
    for(int i=0;i<48;++i){objects[i].index=i;++objects[i].serial;objects[i].live=true;objects[i].cls=(i==2||i==4)?3:1;objects[i].eid=1;}
    held=false;locationOK=rotationOK=velocityOK=true;root=&objects[2];location={10,20,30};rotation={0,10,0};velocity={1,2,3};
    calls.clear();clientEnds.clear();s={};s.hook=Call;CHECK(H::BindSession(s,true));}
H::Driven Seed(uint32_t eid=1,bool admitted=false,bool rest=false){H::Driven d;
    auto* actor=eid==1?&objects[0]:&objects[5];actor->eid=eid;
    d.ref.Set(actor);d.cls.Set(&objects[1]);d.eid=eid;d.gen=5;d.revision=H::NewRowRevision();d.episode=d.revision;d.key.len=1;d.key.data[0]='p';
    d.syncer=admitted?1:0;d.ownerGeneration=admitted?H::g_session->peerGenerationForSlot(1):0;
    d.claimed=!rest;d.everSent=rest;d.sentLoc=location;d.sentRot=rotation;d.openMs=now-1000;
    d.lastMoveMs=now-(rest?600:0);d.lastSendMs=now;d.admitLoc=location;d.admitRot=rotation;d.admitVel=velocity;
    d.admittedEver=d.freshAdmit=admitted;d.admitMs=now;
    if(auto row=H::FindByEid(eid))*row=d;else H::Rows().push_back(d);return d;}
void Mark(const char* name){std::printf("PASS %s\n",name);}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){
#ifdef PROP_VERB_FIXTURE
if(p){bool known=false;for(auto& object:test::objects)if(p==&object)known=true;if(!known){test::Call("foreignCapture");return -1;}}
#endif
return p?static_cast<test::Object*>(p)->index:-1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<48?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLive(void* p){return p&&static_cast<test::Object*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<48&&p==&test::objects[i]&&test::objects[i].live;}
void* ClassOf(void* p){test::Call("class");return p?&test::objects[static_cast<test::Object*>(p)->cls]:nullptr;}
std::wstring ClassNameOf(void*){
#if defined(PROP_VERB_FIXTURE) || defined(HANDTAKEN_FIXTURE)
test::Call("className");
#endif
return L"Aprop_C";}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){
#ifdef PROP_VERB_FIXTURE
return test::worldObject;
#else
return nullptr;
#endif
}void* WorldOf(void*){return nullptr;}}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace ue_wrap::game_thread {bool IsDefinitelyOffGameThread(){return false;}}
namespace ue_wrap::hot_path {unsigned long long CallerRva(const void*){return 0;}unsigned long CurrentThreadId(){return 0;}}
namespace ue_wrap::log {void Write(Level,const char* format,...){
#ifdef PROP_VERB_FIXTURE
char text[2048];va_list args;va_start(args,format);std::vsnprintf(text,sizeof text,format,args);va_end(args);test::logLines.emplace_back(text);
#endif
}std::string Addr(std::string_view s){return std::string(s);}}
namespace coop::active_drive {uint64_t NowMs(){
#ifdef PROP_VERB_FIXTURE
test::Call("clock");
#endif
return test::now;}}
namespace coop::config {std::string ResolveEnum(const config_registry::EnumRow&){return "off";}}
namespace ue_wrap::prop {
bool IsDescendantOfProp(void*){
#ifdef PROP_VERB_FIXTURE
test::Call("descendant");return test::propClass;
#else
return true;
#endif
}std::wstring GetInteractableKeyString(void*){
#ifdef PROP_VERB_FIXTURE
const auto key=test::nativeKey;test::Call("key");return key;
#else
return L"prop";
#endif
}
VelocityState GetPhysicsVelocity(void*,bool){test::Call("endVelocity");return {test::velocity,{},true};}
}
namespace ue_wrap::engine {
bool TryGetActorLocation(void*,FVector& out){out=test::location;test::Call("location");return test::locationOK;}
bool TryGetActorRotation(void*,FRotator& out){out=test::rotation;test::Call("rotation");return test::rotationOK;}
void* RootComponentOf(void*){test::Call("root");return test::root;}
bool GetActorRootLinearVelocity(CachedObjRef& cache,void*,FVector& out){if(!cache.Get())cache.Set(test::root);out=test::velocity;test::Call("velocity");return test::velocityOK;}
bool ReadMainPlayerGrabState(void*,MainPlayerGrabState& out){out={};if(test::held)out.grabbingActor=&test::objects[0];test::Call("hand");return true;}
}
namespace coop::prop_knock {void Tick(net::Session&){test::Call("contacts");}void OnDisconnect(){test::Call("knockStop");}}
namespace coop::prop_syncer {void Tick(net::Session&,bool){test::Call("syncer");}void OnDisconnect(){test::Call("clientStop");}}
namespace coop::prop_syncer_host {
#ifndef PROP_END_JOINT
void OnRowOpened(uint32_t eid){coop::prop_end_delivery::Opened(eid);
#ifdef PROP_VERB_FIXTURE
test::Call("opened");
#endif
}
void Admit(net::Session&){test::Call("admit");}void OnDisconnect(){test::Call("hostStop");}
void EndClientRow(prop_drive_host::Driven& row,Ending why){const auto before=row;const auto eid=row.eid;const auto revision=row.revision;test::clientEnds.push_back(why);test::Call("clientEnd");
    auto live=prop_drive_host::FindByEid(eid);if(live&&live->revision==revision){prop_drive_host::EraseRow(*live);
#ifdef PROP_VERB_FIXTURE
if(test::transition)test::transition(before);
#endif
}}
#endif
}
namespace coop::prop_drive_stream {void TickApplyAndDrive(net::Session&){test::Call("apply");}}
namespace coop::prop_wire_parity {uint8_t PhysFlagsOf(void*){test::Call("flags");return 0;}}
namespace coop::prop_element_tracker {element::ElementId GetPropElementIdForActor(void* p){test::Call("eid");return p?static_cast<test::Object*>(p)->eid:element::kInvalidId;}}
namespace coop::prop_lifecycle {bool IsWireSuppressedPropClass(const std::wstring&){
#ifdef PROP_VERB_FIXTURE
test::Call("suppression");return test::suppressed;
#else
return false;
#endif
}}
#ifndef PROP_LEASE_NATIVE
namespace coop::npc_sync {bool KerfurIsHolding(void*){
#ifdef PROP_VERB_FIXTURE
test::Call("robot");return test::robotHold;
#else
return false;
#endif
}}
#endif
namespace coop::gravity_motion {void* Target(){return nullptr;}}
namespace coop::local_streams {void* LastHeldActor(){
#ifdef PROP_VERB_FIXTURE
test::Call("lastHand");return test::lastHand;
#else
return nullptr;
#endif
}}
namespace coop::remote_prop {bool IsActorUnderAnyDrive(void*){
#ifdef PROP_VERB_FIXTURE
test::Call("remoteHand");return test::remoteHand;
#else
return false;
#endif
}}
namespace coop::hand_item {bool IsHandAxisActor(void*){
#ifdef PROP_VERB_FIXTURE
test::Call("hotbarHand");return test::hotbarHand;
#else
return false;
#endif
}}

#ifndef PROP_PUBLISHER_NO_MAIN
int main(){using namespace test;N::Session s;
    const char* stages[]={"hand","turn","location","rotation","velocity","flags","sendEnd","turn","publish","publish"};
    const char* causes[]={"world","session","actor","class","episode"};
    for(int stage=0;stage<10;++stage)for(int cause=0;cause<5;++cause){
        Reset(s);Seed(1,stage==7||stage==8,stage==5||stage==6);bool fired=false;
        hook=[&](const std::string& name){if(fired||name!=stages[stage])return;fired=true;
            if(cause==0)++world;if(cause==1)++serial;if(cause==2)++objects[0].serial;if(cause==3)++objects[1].serial;
            if(cause==4){auto next=Seed(1,stage==7||stage==8);next.maxGapMs=99;*H::FindByEid(1)=next;}
        };
        H::Tick(s);CHECK(fired&&s.poses.size()==((stage==8||stage==9)?1:0)&&s.ends.size()==(stage==6?1:0));
        CHECK(H::g_published==0);if(cause==4)CHECK(H::FindByEid(1)&&H::FindByEid(1)->maxGapMs==99);
        if(stage==6&&(cause==2||cause==3))CHECK(H::FindByEid(1));
        std::printf("PASS HT%02d: %s / %s\n",stage*5+cause+1,stages[stage],causes[cause]);
    }
    Reset(s);Seed();bool grown=false;hook=[&](const std::string& n){if(n=="location"&&!grown){grown=true;for(int i=2;i<600;++i)Seed(i);}};H::Tick(s);
    CHECK(grown&&s.poses.size()==1&&H::FindByEid(1)->everSent);Mark("HP01 owned Tick receipts");
    Reset(s);Seed();hook=[](const std::string& n){if(n=="location")Seed();};H::Tick(s);CHECK(s.poses.empty()&&!H::FindByEid(1)->everSent);Mark("HP02 exact revision commit");
    Reset(s);Seed();bool entered=false;hook=[&](const std::string& n){if(n=="location"&&!entered){entered=true;H::Tick(s);}};H::Tick(s);
    CHECK(Count("location")==1&&s.poses.size()==1);Mark("HP03 recursive Tick guard");
    Reset(s);bool pumped=false;hook=[&](const std::string& n){if(n=="syncer"&&!pumped){pumped=true;H::Pump(s,true);}};H::Pump(s,true);
    CHECK(Count("syncer")==1&&Count("apply")==1);Mark("HP04 recursive Pump guard");
    Reset(s);Seed();N::Session other;H::Tick(other);CHECK(H::Rows().empty());Mark("HP05 session pointer retirement");
    Reset(s);Seed();++serial;H::Tick(s);CHECK(H::Rows().empty());Mark("HP06 same-pointer session retirement");
    Reset(s);Seed();++s.stops;H::Tick(s);CHECK(H::Rows().empty());Mark("HP07 stopped-session retirement");
    Reset(s);Seed();++world;H::Tick(s);CHECK(H::Rows().empty());Mark("HP08 replaced-world retirement");
    Reset(s);Seed();s.mode=N::Role::Client;H::Pump(s,false);CHECK(H::Rows().empty());Mark("HP09 changed-role retirement");
    Reset(s);Seed();hook=[](const std::string& n){if(n=="contacts")++serial;};H::Tick(s);CHECK(s.poses.empty()&&!Count("hand"));Mark("HP10 contact drain completion");
    Reset(s);Seed();hook=[](const std::string& n){if(n=="admit")++serial;};H::Pump(s,true);CHECK(!Count("contacts")&&!Count("apply"));Mark("HP11 remaining Pump stages cancelled");
    Reset(s);Seed(1,true);++s.generations[1];H::Tick(s);CHECK(s.poses.empty()&&clientEnds.size()==1&&clientEnds[0]==coop::prop_syncer_host::Ending::SlotLeft);Mark("HP12 original client slot owner");
    Reset(s);Seed();bool added=false;hook=[&](const std::string& n){if(n=="location"&&!added){added=true;Seed(2);}};H::Tick(s);CHECK(s.poses.size()==1&&!H::FindByEid(2)->everSent);H::Tick(s);CHECK(s.poses.size()==2&&H::FindByEid(2)->everSent);Mark("HP13 callback arrivals deferred");
    Reset(s);Seed();auto dead=Seed(2);dead.dead=true;H::Rows()[1]=dead;H::g_turn=999;H::Tick(s);CHECK(H::Rows().size()==1&&H::g_turn<H::Rows().size());Mark("HP14 fair current-table rotation");
    Reset(s);Seed(1,true);hook=[](const std::string& n){if(n=="publish")H::OnPeerWorldReady();};H::Tick(s);CHECK(H::FindByEid(1)->resend&&H::FindByEid(1)->freshAdmit);Mark("HP15 callback resend preserved");
    Reset(s);Seed(1,false,true);hook=[](const std::string& n){if(n=="rotation")H::Coast(&objects[0],"keepalive");};H::Tick(s);CHECK(s.ends.empty()&&H::FindByEid(1)->lastMoveMs==now);Mark("HP16 callback coast preserved");
    Reset(s);Seed();location.X=std::numeric_limits<float>::quiet_NaN();H::Tick(s);CHECK(s.poses.empty()&&s.ends.empty());Mark("HP17 nonfinite pose rejected");
    Reset(s);Seed();location.Z=N::kMaxCoord+1;H::Tick(s);CHECK(s.poses.empty()&&s.ends.empty());Mark("HP18 out-of-world pose rejected");
    Reset(s);Seed();velocity.X=std::numeric_limits<float>::infinity();H::Tick(s);CHECK(s.poses.empty());Mark("HP19 nonfinite velocity rejected");
    Reset(s);Seed();H::FindByEid(1)->rootComp.Set(root);root=&objects[4];H::Tick(s);CHECK(s.poses.size()==1&&H::FindByEid(1)->rootComp.Is(root));Mark("HP20 actual primitive root refresh");
    Reset(s);Seed();hook=[](const std::string& n){if(n=="velocity")root=&objects[4];};H::Tick(s);CHECK(s.poses.empty());Mark("HP21 root incarnation guard");
    Reset(s);Seed();hook=[](const std::string& n){if(n=="velocity")held=true;};H::Tick(s);CHECK(s.poses.empty());Mark("HP22 final hand ownership");
    Reset(s);Seed(1,false,true);auto future=H::FindByEid(1);future->lastMoveMs=future->lastSendMs=future->openMs=now+100;H::Tick(s);CHECK(s.poses.empty()&&s.ends.empty()&&H::FindByEid(1));Mark("HP23 backwards clock saturation");
    Reset(s);Seed(1,false,true);s.sendOk=false;H::Tick(s);CHECK(!H::FindByEid(1)&&s.ends.empty()&&coop::prop_end_delivery::g_pending.count(1));s.sendOk=true;now+=250;H::Tick(s);CHECK(!H::FindByEid(1)&&s.ends.size()==1);Mark("HP24 refused host End retry");
    Reset(s);Seed(1,false,true);H::Tick(s);H::Tick(s);CHECK(H::Rows().empty()&&s.ends.size()==1);Mark("HP25 accepted End retired once");
    Reset(s);Seed();hook=[](const std::string& n){if(n=="location")objects[0].cls=5;};H::Tick(s);CHECK(s.poses.empty());
    Reset(s);Seed(1,false,true);hook=[](const std::string& n){if(n=="flags")root=&objects[4];};H::Tick(s);CHECK(s.ends.empty()&&!Count("endVelocity"));
    Reset(s);Seed(1,false,true);H::FindByEid(1)->lastSendMs=now-2000;H::Tick(s);CHECK(s.poses.size()==1&&s.ends.size()==1&&H::Rows().empty());
    Reset(s);Seed(1,false,true);locationOK=false;H::Tick(s);CHECK(s.ends.size()==1&&H::Rows().empty());
    Reset(s);Seed();locationOK=false;H::FindByEid(1)->lastMoveMs=now-600;H::Tick(s);CHECK(s.ends.empty()&&H::FindByEid(1)->nextProbeMs==now+H::kRestProbeMs);
    Reset(s);Seed(1,true);H::FindByEid(1)->syncer=31;H::FindByEid(1)->ownerGeneration=s.generations[31];H::Tick(s);CHECK(s.poses.size()==1&&!Count("location")&&!H::FindByEid(1)->freshAdmit);
    Reset(s);Seed();hook=[](const std::string& n){if(n=="velocity")objects[0].eid=2;};H::Tick(s);CHECK(s.poses.empty());
    Reset(s);Seed();bool replaced=false;hook=[&](const std::string& n){if(n=="location"&&!replaced){replaced=true;H::Tick(other);Seed();}};
    H::Tick(s);CHECK(replaced&&s.poses.empty()&&H::g_session==&other&&H::FindByEid(1)&&!H::FindByEid(1)->everSent);
    Reset(s);hook=[&](const std::string& n){if(n=="knockStop"){H::Tick(other);Seed();}};H::OnDisconnect();
    CHECK(H::g_session==&other&&H::FindByEid(1)&&!Count("hostStop"));
    for(const char* stage:{"turn","publish"}){Reset(s);Seed(1,true);hook=[&](const std::string& n){if(n==stage)++s.generations[1];};
        H::Tick(s);CHECK(H::FindByEid(1)->freshAdmit&&s.poses.size()==(std::string(stage)=="publish"?1:0));}
    Reset(s);Seed();hook=[](const std::string& n){if(n=="publish")root=&objects[4];};H::Tick(s);CHECK(s.poses.size()==1&&!H::FindByEid(1)->everSent&&H::g_published==0);
    Reset(s);Seed(1,false,true);hook=[](const std::string& n){if(n=="sendEnd")root=&objects[4];};H::Tick(s);CHECK(s.ends.size()==1&&H::FindByEid(1));
    Reset(s);Seed(1,false,true);s.sendOk=false;hook=[](const std::string& n){if(n=="sendEnd"){Seed();H::FindByEid(1)->dead=true;}};
    H::Tick(s);CHECK(s.ends.empty()&&H::Rows().empty());
    std::puts("PASS host publisher integration controls");
}
#endif
