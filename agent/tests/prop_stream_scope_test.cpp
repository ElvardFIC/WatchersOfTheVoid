#include "../../src/votv-coop/src/coop/element/bind_wait.cpp"
#ifndef STREAM_SCOPE_SOURCE
#define STREAM_SCOPE_SOURCE "../../src/votv-coop/src/coop/props/prop_drive_stream.cpp"
#endif
#include STREAM_SCOPE_SOURCE
#ifndef STREAM_SCOPE_PARK_SOURCE
#define STREAM_SCOPE_PARK_SOURCE "../../src/votv-coop/src/coop/props/prop_park.cpp"
#endif
#include STREAM_SCOPE_PARK_SOURCE
#ifndef STREAM_SCOPE_PARITY_SOURCE
#define STREAM_SCOPE_PARITY_SOURCE "../../src/votv-coop/src/coop/props/prop_wire_parity.cpp"
#endif
#include STREAM_SCOPE_PARITY_SOURCE
#ifndef STREAM_SCOPE_CONTROLLER_SOURCE
#define STREAM_SCOPE_CONTROLLER_SOURCE "../../src/votv-coop/src/coop/props/prop_syncer_host.cpp"
#endif
#include STREAM_SCOPE_CONTROLLER_SOURCE
#include "coop/props/drive_generation.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
#include <optional>
#include "coop/player/local_prop_hold.h"
#include "coop/player/local_streams.h"
#include "coop/player/hand_item.h"
namespace coop::prop_drive_host {
auto& g_session=coop::prop_syncer_host::g_session;
auto& g_epoch=coop::prop_syncer_host::g_epoch;
auto& g_world=coop::prop_syncer_host::g_world;
auto& g_serial=coop::prop_syncer_host::g_serial;
auto& g_stops=coop::prop_syncer_host::g_stops;
constexpr bool g_host=true;
namespace R=ue_wrap::reflection;namespace PR=ue_wrap::prop;
std::vector<Driven> g_driven;std::unordered_map<uint32_t,uint8_t> g_genByEid;uint64_t g_rowRevision=0;
#include "stream_scope_rows.inc"
}
namespace test {
namespace D=coop::prop_drive_stream;namespace H=coop::prop_drive_host;namespace S=coop::prop_syncer_host;namespace N=coop::net;namespace P=coop::prop_park;
struct Object {int index=0,serial=1,cls=1;bool live=true;uint32_t eid=1;} objects[10];
uint32_t world=1,serial=1;uint64_t now=10000;
bool present=true,held=false,simulates=true,frozen=false,sleeping=false,drill=true,linearOK=true,angularOK=true;
void* resolved=&objects[0];
void* mesh=&objects[2];
int taken=0,given=0;uint64_t nextToken=0;std::unordered_map<uint64_t,void*> tokens;
ue_wrap::FVector location{},linear{},angular{};ue_wrap::FRotator rotation{};
std::function<void(const std::string&)> hook;std::vector<std::string> calls;
void Call(const std::string& name){calls.push_back(name);auto fn=hook;if(fn)fn(name);}
int Count(const char* name){return int(std::count(calls.begin(),calls.end(),name));}
void Reset(N::Session& s){hook={};D::OnDisconnect();P::OnDisconnect();H::OnDisconnect();coop::element::bind_wait::Clear("reset");CHECK(tokens.empty());
    ++world;++serial;now=10000;for(int i=0;i<10;++i){objects[i].index=i;++objects[i].serial;objects[i].cls=(i==2||i==4)?3:1;objects[i].live=true;objects[i].eid=1;}
    mesh=&objects[2];resolved=&objects[0];present=simulates=drill=linearOK=angularOK=true;held=frozen=sleeping=false;location=linear=angular={};rotation={};taken=given=0;calls.clear();
    s={};S::Bind(s);CHECK(D::BindSession(s));}
H::Driven Seed(){H::Driven row;row.ref.Set(&objects[0]);row.cls.Set(&objects[1]);row.eid=1;row.gen=5;row.syncer=1;row.claimSeq=7;
    row.revision=H::NewRowRevision();row.episode=row.revision;row.ownerGeneration=S::g_session->peerGenerationForSlot(1);row.ownerDeparture=S::g_departures[1];row.admitMs=now;
    row.key.len=1;row.key.data[0]='p';if(auto live=H::FindByEid(1))*live=row;else H::Rows().push_back(row);return row;}
N::PropDriveRow Row(){N::PropDriveRow row{};row.elementId=1;row.ctx=5;row.x=10;row.sendTimeMs=100;row.vel[0]=1;return row;}
N::PropDriveEndPayload End(){N::PropDriveEndPayload end{};end.eid=1;end.gen=5;end.x=20;end.linVelX=10;end.angVelZ=20;end.physFlags=N::propspawn_flags::kLiveState;return end;}
D::Source Source(){S::Receipt receipt(*H::FindByEid(1));return {receipt.context.session,[receipt]{return receipt.Current();},[receipt]{return receipt.Alive();},receipt.value.ref,receipt.value.cls,receipt.value.episode};}
void Mark(const char* name){std::printf("PASS %s\n",name);}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return p?static_cast<test::Object*>(p)->index:-1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<10?test::objects[i].serial:0;}int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLive(void* p){return p&&static_cast<test::Object*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<10&&p==&test::objects[i]&&test::objects[i].live;}
void* ClassOf(void* p){return p?&test::objects[static_cast<test::Object*>(p)->cls]:nullptr;}
std::wstring ClassNameOf(void*){return L"Aprop_C";}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace ue_wrap::game_thread {bool IsDefinitelyOffGameThread(){return false;}}
namespace ue_wrap::hot_path {unsigned long long CallerRva(const void*){return 0;}unsigned long CurrentThreadId(){return 0;}}
namespace ue_wrap::log {void Write(Level,const char*,...){}std::string Addr(std::string_view s){return std::string(s);}}
namespace coop::active_drive {uint64_t NowMs(){return test::now;}}
namespace coop::config {bool ResolveFlag(const config_registry::FlagRow&){return false;}std::string ResolveEnum(const config_registry::EnumRow&){return "off";}}
namespace coop::config {long ResolveInt(const config_registry::IntRow&){return 0;}}
namespace coop::dev::prop_syncer_drill {bool IsEnabled(){return test::drill;}void BeforeGrant(net::Session&,uint8_t){}}
namespace coop::dev::swinger_drill {bool IsEnabled(){return false;}}
namespace coop::roster_ledger {void RegisterPerSlotClear(PerSlotClearFn,void*){}}
namespace coop::element {
Registry::Registry(){}Registry::~Registry(){}Registry& Registry::Get(){static Registry registry;return registry;}Element* Registry::Get(ElementId)const{return nullptr;}
IntentTarget IntentTarget::ForClientIntent(net::Session& session,uint8_t slot,float reach){IntentTarget target;target.session_=&session;target.slot_=slot;target.reachUU_=reach;return target;}
IntentSubject IntentTarget::Authorize(void*)const{return {IntentOutcome::Ok};}IntentSubject IntentTarget::AuthorizeSegment(const ue_wrap::FVector&,const ue_wrap::FVector&)const{return {IntentOutcome::Ok};}
}
namespace coop::players {Registry& Registry::Get(){static Registry registry;return registry;}RemotePlayer* Registry::Puppet(int)const{static RemotePlayer player;return &player;}void* Registry::Local()const{return nullptr;}}
namespace coop {bool RemotePlayer::TryGetLocation(ue_wrap::FVector& out)const{out={};return true;}}
namespace ue_wrap::prop {
bool IsDescendantOfProp(void*){return true;}std::wstring GetInteractableKeyString(void*){return L"prop";}
void* GetStaticMesh(void*){test::Call("mesh");return test::mesh;}
bool IsFrozen(void*){test::Call("readFrozen");return test::frozen;}bool IsStatic(void*){return false;}bool IsSleeping(void*){return test::sleeping;}
bool IsHeavy(void*){return false;}bool ReadRemoveWOrespawn(void*){return false;}
bool ForceRestoreDefaultCollision(void*){return true;}
bool CallAwakeUnfreeze(void*){test::frozen=test::sleeping=false;test::Call("frozen");return true;}
bool CallBaseSetPropProps(void*,bool,bool frozen,bool sleep){test::frozen=frozen;test::sleeping=sleep;test::Call("frozen");return true;}
}
namespace ue_wrap::engine {
bool TryGetActorLocation(void*,FVector& out){out=test::location;test::Call("read");return true;}
bool SetActorLocation(void*,const FVector& at){test::location=at;test::Call("location");return true;}
bool SetActorRotation(void*,const FRotator& at){test::rotation=at;test::Call("rotation");return true;}
bool SetComponentSimulatePhysics(void*,bool on){test::simulates=on;test::Call(on?"restore":"park");return true;}
bool SetActorSimulatePhysics(void*,bool on){test::simulates=on;test::Call(on?"restore":"park");return true;}
int AttachedCharactersOf(void*,AttachedCharacter* out,int max){if(max<1)return 0;out[0].updated=&test::objects[6];out[0].movement=&test::objects[7];return 1;}
bool IsComponentSimulatingPhysics(void*){return test::simulates;}
bool IsActorRootBodyAtRest(void*){return false;}
bool SetComponentLinearVelocity(void*,float x,float y,float z){test::linear={x,y,z};test::Call("linear");return test::linearOK;}
bool SetComponentAngularVelocity(void*,float x,float y,float z){test::angular={x,y,z};test::Call("angular");return test::angularOK;}
bool ReadMainPlayerGrabState(void*,MainPlayerGrabState& out){out={};if(test::held)out.grabbingActor=&test::objects[0];return true;}
}
namespace coop::rider_hold {
Token Take(void* movement){const auto token=++test::nextToken;test::tokens[token]=movement;++test::taken;test::Call("takeRider");return token;}
void Give(void* movement,Token token){CHECK(test::tokens.contains(token)&&test::tokens.at(token)==movement);test::tokens.erase(token);++test::given;test::Call("unpark");}
}
namespace coop::remote_prop {void* ResolveLiveActorByEid(uint32_t){test::Call("resolve");return test::present?test::resolved:nullptr;}std::wstring KeyToWString(const net::WireKey&){return L"prop";}}
namespace coop::prop_key_index {void* Find(const std::wstring&){test::Call("keyResolve");return nullptr;}}
namespace coop::prop_element_tracker {element::ElementId GetPropElementIdForActor(void* p){return p?static_cast<test::Object*>(p)->eid:element::kInvalidId;}}
namespace coop::prop_lifecycle {bool IsWireSuppressedPropClass(const std::wstring&){return false;}}
namespace coop::prop_throw {bool HasMarks(){return false;}bool TakeMark(const std::wstring&,Mark*){return false;}}
namespace coop::prop_drive_host {std::vector<Driven>& Rows(){return g_driven;}bool InAHand(void*){test::Call("hand");return test::held;}void Coast(void*,const char*,bool){}void OnDisconnect(){g_driven.clear();g_genByEid.clear();coop::prop_syncer_host::OnDisconnect();}}
namespace coop::local_streams {void* LastHeldActor(){return nullptr;}}
namespace coop::hand_item {bool IsHandAxisActor(void*){return false;}}
namespace coop::gravity_motion {void* Target(){return nullptr;}}
namespace coop::prop_syncer {bool SkipsHostPose(uint32_t){return false;}}
namespace coop::prop_child_release {void OnParentReleased(void*){test::Call("child");}void Tick(uint64_t){}void OnDisconnect(){}}

int main(){using namespace test;N::Session session;
    const char* stages[]={"resolve","mesh","read","park","location","location","rotation","frozen","linear","unpark"};
    const char* causes[]={"world","session","sender","actor","class","episode"};
    for(int stage=0;stage<10;++stage)for(int cause=0;cause<6;++cause){
        Reset(session);Seed();if(stage>=5){S::AdmitRow(Row(),1,now);CHECK(D::IsParked(&objects[0]));calls.clear();if(stage==7)frozen=true;}
        bool fired=false;hook=[&](const std::string& name){if(fired||name!=stages[stage])return;fired=true;
            if(cause==0)++world;if(cause==1)++serial;if(cause==2)++session.generations[1];if(cause==3)++objects[0].serial;if(cause==4)++objects[1].serial;if(cause==5)Seed();
        };
        if(stage<5)S::AdmitRow(Row(),1,now);else S::OnClientEnd(session,End(),1);
        CHECK(fired);
        if(stage==3)CHECK(taken==0);
        if(stage<4)CHECK(!Count("location"));if(stage==4||stage==5)CHECK(!Count("rotation"));
        if(stage==6)CHECK(!Count("restore"));if(stage==7)CHECK(!Count("linear"));if(stage==8)CHECK(!Count("angular"));if(stage==9)CHECK(!Count("child"));
        CHECK(D::g_endCount==0);std::printf("PASS SX%02d: %s / %s\n",stage*6+cause+1,stages[stage],causes[cause]);
    }
    Reset(session);Seed();auto source=Source();source.current=[] {return false;};D::FeedAdmitted(Row(),source);CHECK(!Count("park")&&!Count("location"));Mark("SG01 guarded admission API");
    Reset(session);Seed();source=Source();source.current=[] {return false;};D::ReleaseHost(End(),source);CHECK(!Count("location"));Mark("SG02 guarded release API");
    Reset(session);Seed();S::AdmitRow(Row(),1,now);CHECK(D::IsParked(&objects[0])&&Count("location")==1&&S::g_admitted==1);Mark("SG03 actual admission receipt");
    S::OnClientEnd(session,End(),1);CHECK(!H::FindByEid(1)&&D::g_endCount==1&&Count("child")==1);Mark("SG04 rest retirement proof");
    Reset(session);Seed();S::AdmitRow(Row(),1,now);S::EndClientRow(*H::FindByEid(1),S::Ending::HostVerb);CHECK(H::FindByEid(1)&&H::FindByEid(1)->syncer==0&&Count("child")==1);Mark("SG05 takeback retirement proof");
    Reset(session);auto before=Seed();S::AdmitRow(Row(),1,now);CHECK(H::FindByEid(1)->revision!=before.revision&&D::LifetimeCurrent(D::g_drives.at(1)));
    D::TickApplyAndDrive(session);CHECK(D::LifetimeCurrent(D::g_drives.at(1)));Mark("SG06 persistent episode lifetime");
    for(int change=0;change<5;++change){Reset(session);Seed();S::AdmitRow(Row(),1,now);N::Session other;
        if(change==0)D::TickApplyAndDrive(other);if(change==1){++serial;D::TickApplyAndDrive(session);}if(change==2){++session.stops;D::TickApplyAndDrive(session);}
        if(change==3){++world;D::TickApplyAndDrive(session);}if(change==4){session.mode=N::Role::Client;D::TickApplyAndDrive(session);}CHECK(D::g_drives.empty());}
    Mark("SG07 receiver session retirement");
    Reset(session);Seed();hook=[](const std::string& n){if(n=="mesh")objects[0].cls=9;};S::AdmitRow(Row(),1,now);CHECK(!Count("park")&&!Count("location"));Mark("SG08 actor class and mesh incarnation");
    Reset(session);Seed();source=Source();present=false;coop::element::bind_wait::MarkDeferred(1,"fixture");D::ReleaseHost(End(),source);
    CHECK(!coop::element::bind_wait::Holds(1,uint16_t(N::ReliableKind::PropDriveEnd)));Mark("SG09 no borrowed host End replay");
    Reset(session);Seed();S::AdmitRow(Row(),1,now);hook=[](const std::string& n){if(n=="linear")++serial;};S::OnClientEnd(session,End(),1);
    CHECK(D::g_endCount==0&&!D::g_endOnActor.contains(1));Mark("SG10 End bookkeeping after valid completion");
    Reset(session);Seed();hook=[](const std::string& n){if(n=="park")++serial;};S::AdmitRow(Row(),1,now);CHECK(taken==0&&!Count("location"));Mark("SG11 Park Unpark source composition");
    Reset(session);Seed();S::AdmitRow(Row(),1,now);hook=[](const std::string& n){if(n=="unpark")++serial;};S::OnClientEnd(session,End(),1);CHECK(!Count("child")&&tokens.empty());Mark("SG12 child notification and cleanup boundary");
    Reset(session);Seed();auto row=Row();source=Source();hook=[&](const std::string& n){if(n=="resolve"){row.x=999;source.current=[] {return false;};}};D::FeedAdmitted(row,source);
    CHECK(location.X==10);Mark("SG13 owned payload and callbacks");
    Reset(session);Seed();S::AdmitRow(Row(),1,now);now+=100;auto newer=Row();newer.x=30;newer.sendTimeMs=200;S::AdmitRow(newer,1,now);
    ++session.generations[1];now+=50;calls.clear();D::TickApplyAndDrive(session);
    CHECK(D::g_drives.contains(1)&&D::g_drives.at(1).physicsParked&&!Count("location")&&!Count("rotation")&&!Count("park"));Mark("SG14 invalid lifetime retains park debt");
    for(bool angularFailure:{false,true}){Reset(session);Seed();S::AdmitRow(Row(),1,now);calls.clear();if(angularFailure)angularOK=false;else linearOK=false;S::OnClientEnd(session,End(),1);
        CHECK(!Count("child")&&D::g_endCount==0);if(!angularFailure)CHECK(!Count("angular"));}
    Mark("SG15 refused velocity stops later effects");
    Reset(session);Seed();source=Source();resolved=&objects[8];D::FeedAdmitted(Row(),source);CHECK(!Count("park")&&!Count("location"));
    Reset(session);Seed();source=Source();source.current=source.lifetime=[] {return true;};objects[0].cls=9;D::FeedAdmitted(Row(),source);CHECK(!Count("park")&&!Count("location"));
    Reset(session);Seed();S::AdmitRow(Row(),1,now);calls.clear();CHECK(!P::Unpark(&objects[0],mesh,true,[]{return false;}));CHECK(tokens.size()==1&&!Count("restore"));
    Reset(session);Seed();S::AdmitRow(Row(),1,now);calls.clear();frozen=true;hook=[&](const std::string& n){if(n=="readFrozen")++session.generations[1];};S::OnClientEnd(session,End(),1);CHECK(!Count("frozen"));
    Reset(session);Seed();S::AdmitRow(Row(),1,now);hook=[&](const std::string& n){if(n=="child")++session.generations[1];};S::OnClientEnd(session,End(),1);CHECK(D::g_endCount==0);
    Reset(session);Seed();S::AdmitRow(Row(),1,now);calls.clear();hook=[&](const std::string& n){if(n=="location")++session.generations[1];};S::EndClientRow(*H::FindByEid(1),S::Ending::HostVerb);CHECK(!Count("rotation")&&!Count("child"));
    Reset(session);Seed();S::AdmitRow(Row(),1,now);N::Session other;auto input=Row();source=Source();source.session=&other;source.current=source.lifetime=[] {return true;};
    hook=[&](const std::string& n){if(n=="restore"){source.current=[]{return false;};input.x=999;}};D::FeedAdmitted(input,source);CHECK(location.X==10&&D::g_drives.contains(1));
    Reset(session);Seed();S::AdmitRow(Row(),1,now);auto originalEpisode=H::FindByEid(1)->episode;S::OnClientEnd(session,End(),1);Seed();CHECK(H::FindByEid(1)->episode!=originalEpisode);
    S::AdmitRow(Row(),1,now);CHECK(D::IsParked(&objects[0])&&D::g_drives.at(1).gen==5);
    Reset(session);Seed();S::AdmitRow(Row(),1,now);calls.clear();int handQueries=0;
    hook=[&](const std::string& n){if(n=="hand"&&++handQueries==2)++session.generations[1];};S::OnClientEnd(session,End(),1);CHECK(!Count("location")&&D::g_endCount==0);
    Reset(session);Seed();S::AdmitRow(Row(),1,now);calls.clear();bool replaced=false;
    hook=[&](const std::string& n){if(n=="location"&&!replaced){replaced=true;auto next=H::Open(&objects[0],true,"new episode");CHECK(next);H::EraseRow(*next);}};
    S::OnClientEnd(session,End(),1);CHECK(replaced&&!Count("rotation")&&!Count("child")&&D::g_endCount==0);
    std::puts("PASS stream scope integration controls");
}

namespace coop::remote_prop {bool IsActorUnderAnyDrive(void*){return false;}}
