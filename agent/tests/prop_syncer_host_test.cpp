#ifndef PROP_SYNCER_HOST_SOURCE
#define PROP_SYNCER_HOST_SOURCE "../../src/votv-coop/src/coop/props/prop_syncer_host.cpp"
#endif
#include PROP_SYNCER_HOST_SOURCE
#include "coop/props/drive_generation.h"
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
std::vector<Driven> g_driven;
std::unordered_map<uint32_t,uint8_t> g_genByEid;
uint64_t g_rowRevision=0;
#ifndef PROP_SYNCER_HOST_ROWS_SOURCE
#define PROP_SYNCER_HOST_ROWS_SOURCE "prop_syncer_host_rows.inc"
#endif
#include PROP_SYNCER_HOST_ROWS_SOURCE
}
namespace test {
namespace S=coop::prop_syncer_host;namespace H=coop::prop_drive_host;namespace N=coop::net;
struct Object {int index=0,serial=1,cls=1;bool live=true;uint32_t eid=1;} objects[3];
uint32_t world=1,serial=1;uint64_t now=10000;
bool held=false,readPose=true,readPuppet=true,wireSuppressed=false,drill=true;
ue_wrap::FVector location{10,20,30},puppetLocation{10,20,30};
coop::element::IntentOutcome reach=coop::element::IntentOutcome::Ok;
std::function<void(const std::string&)> hook;
std::vector<std::string> calls;std::vector<N::PropDriveRow> fed;std::vector<N::PropDriveEndPayload> released;
void Call(const std::string& name){calls.push_back(name);auto fn=hook;if(fn)fn(name);}
int Count(const char* name){return int(std::count(calls.begin(),calls.end(),name));}
void Reset(N::Session& s){hook={};H::OnDisconnect();++world;++serial;now=10000;
    for(int i=0;i<3;++i){objects[i].index=i;++objects[i].serial;objects[i].live=true;objects[i].cls=1;objects[i].eid=1;}
    held=false;readPose=readPuppet=drill=true;wireSuppressed=false;reach=coop::element::IntentOutcome::Ok;
    location=puppetLocation={10,20,30};calls.clear();fed.clear();released.clear();s={};s.hook=Call;S::Bind(s);S::g_nextPassMs=now+1000;}
H::Driven Seed(uint32_t eid=1,uint8_t slot=1,bool elected=false,bool admitted=false){H::Driven d;
    d.ref.Set(&objects[0]);d.cls.Set(&objects[1]);d.eid=eid;d.gen=5;d.syncer=slot;d.claimSeq=7;
    d.revision=H::NewRowRevision();d.episode=d.revision;d.ownerGeneration=S::g_session->peerGenerationForSlot(slot);d.ownerDeparture=S::g_departures[slot];
    d.elected=elected;d.throwSeq=elected?7:0;d.startLoc=location;d.admitLoc=location;d.admitRot={};d.admitVel={1,2,3};
    d.startMs=d.electedMs=d.admitMs=now-100;d.admittedEver=admitted;d.key.len=1;d.key.data[0]='p';
    if(auto row=H::FindByEid(eid))*row=d;else H::Rows().push_back(d);return d;}
N::PropSyncClaimPayload Claim(){N::PropSyncClaimPayload p{};p.eid=1;p.seq=7;p.key.len=1;p.key.data[0]='p';return p;}
N::PropDriveRow Row(){N::PropDriveRow p{};p.elementId=1;p.ctx=5;p.x=10;p.y=20;p.z=30;p.sendTimeMs=100;p.key.len=1;p.key.data[0]='x';p.vel[0]=1;return p;}
N::PropDriveEndPayload End(){N::PropDriveEndPayload p{};p.eid=1;p.gen=5;p.x=10;p.y=20;p.z=30;return p;}
void Mark(const char* name){std::printf("PASS %s\n",name);}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return p?static_cast<test::Object*>(p)->index:-1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<3?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLive(void* p){return p&&static_cast<test::Object*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<3&&p==&test::objects[i]&&test::objects[i].live;}
void* ClassOf(void* p){test::Call("class");return p?&test::objects[static_cast<test::Object*>(p)->cls]:nullptr;}
std::wstring ClassNameOf(void*){test::Call("className");return L"Aprop_C";}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace ue_wrap::game_thread {bool IsDefinitelyOffGameThread(){return false;}}
namespace ue_wrap::hot_path {unsigned long long CallerRva(const void*){return 0;}unsigned long CurrentThreadId(){return 0;}}
namespace ue_wrap::log {void Write(Level,const char*,...){}std::string Addr(std::string_view s){return std::string(s);}}
namespace coop::active_drive {uint64_t NowMs(){return test::now;}}
namespace coop::config {bool ResolveFlag(const config_registry::FlagRow&){return false;}std::string ResolveEnum(const config_registry::EnumRow&){return "off";}}
namespace coop::roster_ledger {void RegisterPerSlotClear(PerSlotClearFn,void*){}}
namespace coop::dev::prop_syncer_drill {bool IsEnabled(){return test::drill;}void BeforeGrant(net::Session&,uint8_t){test::Call("beforeGrant");}}
namespace coop::dev::swinger_drill {bool IsEnabled(){return false;}}
namespace coop::element {
IntentTarget IntentTarget::ForClientIntent(net::Session& session,uint8_t slot,float reach){IntentTarget t;t.session_=&session;t.slot_=slot;t.reachUU_=reach;return t;}
IntentSubject IntentTarget::Authorize(void*)const{test::Call("reach");return {test::reach};}
IntentSubject IntentTarget::AuthorizeSegment(const ue_wrap::FVector&,const ue_wrap::FVector&)const{test::Call("segment");return {test::reach};}
}
namespace coop::players {Registry& Registry::Get(){static Registry r;return r;}RemotePlayer* Registry::Puppet(int)const{static RemotePlayer p;return &p;}}
namespace coop {bool RemotePlayer::TryGetLocation(ue_wrap::FVector& out)const{out=test::puppetLocation;test::Call("puppet");return test::readPuppet;}}
namespace ue_wrap::engine {bool TryGetActorLocation(void*,FVector& out){out=test::location;test::Call("location");return test::readPose;}}
namespace ue_wrap::prop {bool IsDescendantOfProp(void*){test::Call("type");return true;}std::wstring GetInteractableKeyString(void*){test::Call("key");return L"prop";}}
namespace coop::prop_element_tracker {element::ElementId GetPropElementIdForActor(void* p){test::Call("eid");return p?static_cast<test::Object*>(p)->eid:element::kInvalidId;}}
namespace coop::prop_lifecycle {bool IsWireSuppressedPropClass(const std::wstring&){return test::wireSuppressed;}}
namespace coop::remote_prop {void* ResolveLiveActorByEid(uint32_t){test::Call("resolve");return &test::objects[0];}}
namespace coop::prop_throw {bool HasMarks(){return false;}bool TakeMark(const std::wstring&,Mark*){return false;}}
namespace coop::prop_drive_stream {void FeedAdmitted(const net::PropDriveRow& p){test::fed.push_back(p);test::Call("feed");}void ReleaseHost(const net::PropDriveEndPayload& p){test::released.push_back(p);test::Call("release");}}
namespace coop::prop_drive_stream {
void FeedAdmitted(const net::PropDriveRow& p,const Source& source){if(!source.current||source.current())FeedAdmitted(p);}
void ReleaseHost(const net::PropDriveEndPayload& p,const Source& source){if(!source.current||source.current())ReleaseHost(p);}
}
namespace coop::prop_wire_parity {uint8_t PhysFlagsOf(void*){test::Call("flags");return 0;}}
namespace coop::prop_drive_host {
std::vector<Driven>& Rows(){return g_driven;}
bool InAHand(void*){test::Call("hand");return test::held;}
void Coast(void*,const char*,bool){test::Call("coast");}
void OnDisconnect(){g_driven.clear();g_genByEid.clear();coop::prop_syncer_host::OnDisconnect();}
}

#ifndef PROP_SYNCER_HOST_NO_MAIN
int main(){using namespace test;N::Session s;
    const char* stages[]={"type","hand","reach","beforeGrant","location","feed","puppet","location","flags","endSend"};
    const char* causes[]={"world","session","sender","actor","class","episode"};
    for(int stage=0;stage<10;++stage)for(int cause=0;cause<6;++cause){
        Reset(s);if(stage>=4)Seed(1,1,stage==4,stage>=6);if(stage==6||stage==7)puppetLocation.X=20000;
        bool fired=false;hook=[&](const std::string& name){if(fired||name!=stages[stage])return;fired=true;
            if(cause==0)++world;if(cause==1)++serial;if(cause==2)++s.generations[1];if(cause==3)++objects[0].serial;if(cause==4)++objects[1].serial;
            if(cause==5){auto next=Seed();next.maxGapMs=99;*H::FindByEid(1)=next;}
        };
        if(stage<=3)S::OnClaim(s,Claim(),1);else if(stage<=5)S::AdmitRow(Row(),1,now);
        else if(stage<=7)S::Pass(now);else S::EndClientRow(*H::FindByEid(1),S::Ending::Range);
        CHECK(fired&&s.answers.empty()&&S::g_admitted==0&&S::g_ended[unsigned(S::Ending::Range)]==0);
        CHECK(s.ends.size()==(stage==9?1:0));if(cause==5)CHECK(H::FindByEid(1)&&H::FindByEid(1)->maxGapMs==99);
        if(stage==8)CHECK(Count("flags")==1); // A cancelled read must stop before touching native flags again.
        std::printf("PASS HS%02d: %s / %s\n",stage*6+cause+1,stages[stage],causes[cause]);
    }
    Reset(s);Seed(1,1,true);s.incoming[1]={Row(),Row()};bool nested=false;
    hook=[&](const std::string& n){if(n=="location"&&!nested){nested=true;S::OnDisconnect();}};S::Admit(s);CHECK(nested&&fed.empty());Mark("PH01 owned drained batches");
    Reset(s);Seed();s.incoming[1]={Row()};bool reentered=false;hook=[&](const std::string& n){if(n=="take"&&!reentered){reentered=true;S::Admit(s);}};S::Admit(s);
    CHECK(Count("take")==1&&fed.size()==1);Mark("PH02 recursive drain guard");
    for(int change=0;change<5;++change){Reset(s);Seed();N::Session other;
        if(change==0)S::Admit(other);if(change==1){++serial;S::Admit(s);}if(change==2){++world;S::Admit(s);}
        if(change==3){++s.stops;S::Admit(s);}if(change==4){s.mode=N::Role::Client;S::Admit(s);}CHECK(H::Rows().empty());}
    Mark("PH03 session world stop role retirement");
    Reset(s);Seed();++s.generations[1];S::AdmitRow(Row(),1,now);CHECK(fed.empty());Mark("PH04 original slot occupant");
    Reset(s);S::RecordHandBack(1,1,now);CHECK(S::IsLateSyncer(1,1,now));++s.generations[1];CHECK(!S::IsLateSyncer(1,1,now));
    Reset(s);S::RecordHandBack(1,1,now);S::OnSlotLeft(1);CHECK(!S::IsLateSyncer(1,1,now));Mark("PH05 owned late syncer history");
    Reset(s);auto previous=Seed();S::Receipt receipt(previous);Seed();CHECK(!receipt.Current());
    Reset(s);previous=Seed();objects[0].cls=2;S::AdmitRow(Row(),1,now);CHECK(fed.empty());Mark("PH06 row and class incarnation");
    Reset(s);auto claim=Claim();hook=[&](const std::string& n){if(n=="reach"){claim.eid=2;claim.seq=8;}};S::OnClaim(s,claim,1);
    CHECK(s.answers.size()==1&&s.answers[0].eid==1&&s.answers[0].seq==7);
    Reset(s);Seed(1,1,true);auto row=Row();hook=[&](const std::string& n){if(n=="location"){row.elementId=2;row.ctx=6;}};S::AdmitRow(row,1,now);
    CHECK(fed.size()==1&&fed[0].elementId==1&&fed[0].key.data[0]=='p'&&S::dev::AdmittedOf(1).rows==1&&S::dev::AdmittedOf(2).rows==0);
    Reset(s);Seed(1,1,false,true);auto end=End();hook=[&](const std::string& n){if(n=="release")end.eid=2;};S::OnClientEnd(s,end,1);
    CHECK(s.ends.size()==1&&s.ends[0].eid==1);Mark("PH07 owned wire input");
    Reset(s);Seed();puppetLocation.X=20000;bool grown=false;hook=[&](const std::string& n){if(n=="puppet"&&!grown){grown=true;for(int i=2;i<500;++i)Seed(i);}};
    S::Pass(now);CHECK(grown&&!H::FindByEid(1)&&H::FindByEid(499));Mark("PH08 owned periodic snapshots");
    Reset(s);Seed(1,1,false,true);bool added=false;hook=[&](const std::string& n){if(n=="flags"&&!added){added=true;Seed();}};
    S::OnSlotLeft(1);CHECK(added&&H::FindByEid(1));auto count=H::Rows().size();S::OnSlotLeft(0);S::OnSlotLeft(255);CHECK(H::Rows().size()==count);Mark("PH09 owned departure snapshots");
    Reset(s);Seed(1,1,false,true);bool detached=false;hook=[&](const std::string& n){if(n=="release"){detached=!H::FindByEid(1);Seed();}};
    S::OnClientEnd(s,End(),1);CHECK(detached&&H::FindByEid(1)&&s.ends.empty());Mark("PH10 detach before native ending");
    Reset(s);bool seen=false;hook=[&](const std::string& n){if(n=="beforeGrant"){seen=H::FindByEid(1)!=nullptr;Seed();}};S::OnClaim(s,Claim(),1);
    CHECK(seen&&s.answers.empty());Mark("PH11 owned grant callback");
    for(int bad=0;bad<3;++bad){Reset(s);Seed();auto p=Row();if(bad==0)p.x=std::numeric_limits<float>::quiet_NaN();if(bad==1)p.y=N::kMaxCoord+1;if(bad==2)p.roll=181;
        S::AdmitRow(p,1,now);CHECK(fed.empty()&&!H::FindByEid(1)->admittedEver);}
    Mark("PH12 finite bounded row poses");
    Reset(s);auto future=Seed(1,1,true);future.startMs=future.electedMs=future.admitMs=now+100;*H::FindByEid(1)=future;
    S::Pass(now);CHECK(H::FindByEid(1)&&s.answers.empty());CHECK(S::Elapsed(now,now+1)==0);Mark("PH13 backwards clock saturation");
    Reset(s);float release[3]={10,20,30};hook=[&](const std::string& n){if(n=="segment")release[0]=9999;};CHECK(S::ElectSyncer(&objects[0],31,7,release,now,false));
    CHECK(H::FindByEid(1)->startLoc.X==10&&H::FindByEid(1)->syncer==31&&s.recipients[0]==31);
    Reset(s);release[0]=std::numeric_limits<float>::infinity();CHECK(!S::ElectSyncer(&objects[0],1,7,release,now,false));
    CHECK(!S::ElectSyncer(&objects[0],0,7,release,now,false)&&!S::ElectSyncer(&objects[0],255,7,release,now,false));Mark("PH14 owned bounded throw election");
    Reset(s);Seed();auto ending=End();ending.gen=4;S::OnClientEnd(s,ending,1);ending.gen=5;S::OnClientEnd(s,ending,2);
    CHECK(released.empty()&&s.ends.empty()&&H::FindByEid(1));++s.generations[1];S::OnClientEnd(s,ending,1);CHECK(released.empty());
    Mark("PH15 exact sender and generation for End");
    Reset(s);Seed();S::AdmitRow(Row(),1,now);CHECK(fed.size()==1&&H::FindByEid(1)->admittedEver&&S::g_admitted==1);
    Reset(s);const auto predecessor=Seed(1,1,false,true);S::EndClientRow(*H::FindByEid(1),S::Ending::HostVerb);CHECK(H::FindByEid(1)&&H::FindByEid(1)->syncer==0&&H::FindByEid(1)->gen==1&&H::FindByEid(1)->handoffFrom==predecessor.revision);
    for(bool admitted:{false,true}){Reset(s);Seed(1,1,false,admitted);++s.generations[1];
        S::EndClientRow(*H::FindByEid(1),S::Ending::HostVerb);
        CHECK(H::FindByEid(1)&&H::FindByEid(1)->syncer==0&&H::FindByEid(1)->claimed&&s.answers.empty()&&S::g_handBack.empty());}
    Reset(s);S::OnClaim(s,Claim(),31);CHECK(s.answers.size()==1&&s.recipients[0]==31&&H::FindByEid(1)->ownerGeneration==s.generations[31]&&S::g_offers.empty());
    Reset(s);hook=[](const std::string& n){if(n=="key")++serial;};S::OnClaim(s,Claim(),1);CHECK(!H::FindByEid(1)&&s.answers.empty()&&S::g_offers.empty());
    Reset(s);Seed();puppetLocation.X=std::numeric_limits<float>::quiet_NaN();S::Pass(now);CHECK(H::FindByEid(1)&&s.answers.empty());
    Reset(s);Seed();hook=[](const std::string& n){if(n=="feed")objects[0].eid=2;};S::AdmitRow(Row(),1,now);CHECK(S::g_admitted==0&&!H::FindByEid(1)->admittedEver);
    Reset(s);Seed();auto invalidEnd=End();invalidEnd.linVelX=std::numeric_limits<float>::quiet_NaN();S::OnClientEnd(s,invalidEnd,1);CHECK(released.empty()&&H::FindByEid(1));
    std::puts("PASS host syncer integration controls");
}
#endif

namespace coop::players {void* Registry::Local()const{return nullptr;}}
namespace coop::gravity_motion {void* Target(){return nullptr;}}
namespace coop::local_streams {void* LastHeldActor(){return nullptr;}}
namespace coop::hand_item {bool IsHandAxisActor(void*){return false;}}
namespace ue_wrap::engine {bool ReadMainPlayerGrabState(void*,MainPlayerGrabState& out){out={};if(test::held)out.grabbingActor=&test::objects[0];test::Call("hand");return true;}}

namespace coop::remote_prop {bool IsActorUnderAnyDrive(void*){return false;}}
