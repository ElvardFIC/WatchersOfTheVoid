#define PROP_VERB_FIXTURE
#define PROP_PUBLISHER_NO_MAIN
#define PROP_LEASE_NATIVE
#include "prop_publisher_test.cpp"
#include "ue_wrap/actors/watoy.h"
#include "coop/net/wire_key_util.h"
#ifndef CLAIM_NPC_SOURCE
#define CLAIM_NPC_SOURCE "../../src/votv-coop/src/coop/creatures/npc_pose_host.cpp"
#endif
#ifndef CLAIM_KERFUS_SOURCE
#define CLAIM_KERFUS_SOURCE "../../src/votv-coop/src/coop/creatures/kerfus_state.cpp"
#endif
#ifndef CLAIM_HOOK_SOURCE
#define CLAIM_HOOK_SOURCE "../../src/votv-coop/src/coop/items/hook_prop_claim.cpp"
#endif
#include CLAIM_NPC_SOURCE
#include CLAIM_KERFUS_SOURCE
#include CLAIM_HOOK_SOURCE
#include "../../src/votv-coop/src/coop/element/element.cpp"
#include "../../src/votv-coop/src/coop/element/registry.cpp"
namespace actual_handoff {
using Driven=coop::prop_drive_host::Driven;using Ending=coop::prop_syncer_host::Ending;
struct Receipt{Driven value;};struct Retirement{bool Current()const{return true;}};
void OnRowOpened(uint32_t eid){coop::prop_syncer_host::OnRowOpened(eid);}
void Complete(Driven d,const Receipt& receipt,bool toHost,const Retirement& retiring,uint64_t nowMs,Ending why){
#include "claim_handoff.inc"
}
}
namespace test {
namespace E=coop::element;namespace NPC=coop::npc_sync;namespace KF=coop::kerfus_state;namespace HK=coop::hook_sync::detail;
using Source=H::ClaimSource;using Lease=H::ClaimLease;
N::Session* leaseSession=nullptr;std::vector<std::unique_ptr<E::Element>> owners;
std::vector<HK::Owned> ownedHooks;std::vector<HK::Adopted> adoptedHooks;std::unordered_map<HK::Key,HK::Mirror> mirrorHooks;
void* attached=nullptr;bool kerfusOn=true;float kerfusEnergy=10;int thaws=0;
ue_wrap::watoy::State watoyState{};
E::Element* Register(int object,E::ElementType type){auto p=std::make_unique<E::Element>(type);p->SetTypeName("test_C");CHECK(E::Registry::Get().AllocHostId(p.get())!=E::kInvalidId);p->SetActor(&objects[object],object);objects[object].eid=p->GetId();auto* result=p.get();owners.push_back(std::move(p));return result;}
void ResetClaims(N::Session& s){hook={};NPC::DropKerfurHolds();KF::OnDisconnect();HK::ResetPropClaims();owners.clear();ownedHooks.clear();adoptedHooks.clear();mirrorHooks.clear();Reset(s);worldObject=&objects[47];leaseSession=&s;attached=nullptr;kerfusOn=true;kerfusEnergy=10;thaws=0;nativeKey=L"prop";propClass=true;suppressed=remoteHand=hotbarHand=robotHold=false;lastHand=nullptr;logLines.clear();KF::g_session.store(&s);transition=[](const H::Driven& row){actual_handoff::Complete(row,{row},true,{},now,coop::prop_syncer_host::Ending::HostVerb);};}
Lease Acquire(Source source){return H::ClaimOwned(&objects[0],source,"lease");}
H::Driven* Row(const Lease& lease){return H::FindByEid(lease.eid);}
void Pass(const char* family,int n){std::printf("PASS %s%02d\n",family,n);}
}
namespace coop::npc_sync {net::Session* GetSession(){return test::leaseSession;}}
namespace coop::hook_sync::detail {
coop::net::Session* Session(){return test::leaseSession;}
std::vector<Owned>& OwnedHooks(){return test::ownedHooks;}
std::vector<Adopted>& AdoptedHooks(){return test::adoptedHooks;}
std::unordered_map<Key,Mirror>& Mirrors(){return test::mirrorHooks;}
}
namespace ue_wrap::hook {bool AttachedActors(void*,void*& a,void*& b){a=test::attached;b=nullptr;test::Call("hookAttached");return true;}}
#ifndef OMEGA_COMMAND_FIXTURE
namespace ue_wrap::engine {bool SetActorSimulatePhysics(void*,bool simulate){if(simulate)++test::thaws;test::Call("npcPhysics");return true;}}
#endif
#ifndef KERFUS_MANAGER_FIXTURE
namespace ue_wrap::kerfus {
bool ReadActive(void*,bool& value){value=test::kerfusOn;test::Call("kerfusActive");return true;}
bool ReadCharging(void*,bool& value){value=false;test::Call("kerfusCharging");return true;}
bool ReadEnergy(void*,float& value){value=test::kerfusEnergy;test::Call("kerfusEnergy");return true;}
bool ReadActiveWhile(void* actor,bool& value,const std::function<bool()>& current){return (!current||current())&&ReadActive(actor,value)&&(!current||current());}
bool ReadChargingWhile(void* actor,bool& value,const std::function<bool()>& current){return (!current||current())&&ReadCharging(actor,value)&&(!current||current());}
bool ReadEnergyWhile(void* actor,float& value,const std::function<bool()>& current){return (!current||current())&&ReadEnergy(actor,value)&&(!current||current());}
}
#endif
// Unused native routes fail if reached; they are linked because the fixture
// includes complete production translation units, including registration paths.
namespace coop::npc_sync {void SyncDestroyedNpcByEid(uint32_t,void*){CHECK(false);}}
namespace ue_wrap::reflection {
void* ObjectAt(int32_t index){return index>=0&&index<48?&test::objects[index]:nullptr;}
const FName& NameOf(void*){CHECK(false);static FName name{};return name;}
bool NameStartsWith(const FName&,const wchar_t*){CHECK(false);return false;}
}
namespace ue_wrap::engine {
bool TryGetActorTransform(void*,FVector&,FRotator&,FVector&,const std::function<bool()>&){CHECK(false);return false;}
FVector GetActorVelocity(void*){CHECK(false);return {};}
}
namespace ue_wrap::facts {Function::Function(detail::MacroToken,const char* ident,const char* feature,const wchar_t* cls,const wchar_t* fn,Mode mode):ident_(ident),feature_(feature),cls_(cls),fn_(fn),mode_(mode),next_(nullptr){}}
namespace ue_wrap::script_gate {
#ifndef KERFUS_MANAGER_FIXTURE
bool WatchName(const facts::Function&,int,PreFn,PostFn){CHECK(false);return false;}
void ResolvePendingNames(){CHECK(false);}
bool NameWatchLive(const facts::Function&,int){CHECK(false);return false;}
#endif
}
namespace coop::npc_join_delivery {void Queue(int){CHECK(false);}void Tick(){CHECK(false);}}
namespace coop::npc_world_enum {void DrainPendingExSpawns(){CHECK(false);}}
namespace coop::kerfur_census {void Tick(){CHECK(false);}}
namespace ue_wrap::fossilhound {bool ReadBodyYaw(void*,float&){CHECK(false);return false;}}
namespace ue_wrap::rufus {bool ReadBodyYaw(void*,float&){CHECK(false);return false;}}
namespace ue_wrap::kerfur {
#ifndef OMEGA_COMMAND_FIXTURE
bool ReadKerfurState(void*,uint8_t&,bool&,uint8_t&){CHECK(false);return false;}
bool ReadKerfurFloppy(void*,bool&,uint8_t&){CHECK(false);return false;}
void* ReadHoldObject(void*){CHECK(false);return nullptr;}
#endif
}
namespace ue_wrap::puppet {
bool ReadCharacterIsFalling(void*){CHECK(false);return false;}
bool ReadKerfurLookAt(void*,FVector&){CHECK(false);return false;}
bool ReadKerfurBodyYaw(void*,float&){CHECK(false);return false;}
}
namespace ue_wrap::sit_pose {bool ReadIsOnAtv(void*){CHECK(false);return false;}}
namespace coop::element::bind_wait {
#ifndef KERFUS_MANAGER_FIXTURE
bool Hold(uint32_t,uint16_t,int,Replay,const void*,size_t,bool){CHECK(false);return false;}
void NoteNeverBound(uint16_t,uint32_t){CHECK(false);}
#endif
}
#ifndef KERFUS_MANAGER_FIXTURE
namespace ue_wrap::kerfus {
bool IsKerfusWhile(void*,const std::function<bool()>&){CHECK(false);return false;}
bool WriteActiveWhile(void*,bool,const std::function<bool()>&,bool*){CHECK(false);return false;}
bool WriteChargingWhile(void*,bool,const std::function<bool()>&){CHECK(false);return false;}
bool WriteEnergyWhile(void*,float,const std::function<bool()>&){CHECK(false);return false;}
bool RunUpdWhile(void*,bool,const std::function<bool()>&,bool*){CHECK(false);return false;}
bool IsKerfus(void*){CHECK(false);return false;}
bool WriteActive(void*,bool){CHECK(false);return false;}
bool WriteCharging(void*,bool){CHECK(false);return false;}
bool WriteEnergy(void*,float){CHECK(false);return false;}
bool RunUpd(void*,bool){CHECK(false);return false;}
}
#endif
namespace ue_wrap::hook {bool EnsureResolved(){CHECK(false);return false;}bool IsHookFamily(void*){CHECK(false);return false;}}
namespace coop::hook_retrieve {void Note(void*,bool){CHECK(false);}}
namespace coop::element::scan_hub {void Register(const Consumer&){CHECK(false);}}
namespace actual_watoy {
namespace net=coop::net;namespace prop_drive_host=coop::prop_drive_host;namespace R=ue_wrap::reflection;
using ue_wrap::CachedObjRef;using Packet=net::WatoyPayload;
struct Targets{};struct Row{Packet state{};CachedObjRef actor;uint64_t at=0;uint32_t pending=0;bool authorized=false;Targets targets;prop_drive_host::ClaimLease lease;};
uint64_t g_epoch=0;uint32_t g_sequence=0;std::unordered_map<std::wstring,Row> g_worldRows;
bool Eligible(void*){return true;}bool Current(uint64_t epoch){return epoch==g_epoch;}
std::wstring Key(void* actor){return ue_wrap::prop::GetInteractableKeyString(actor);}
namespace W {using State=ue_wrap::watoy::State;bool Read(void*,State& value){value=test::watoyState;test::Call("watoyRead");return true;}}
bool Changed(const Row& row,const W::State& value){return !row.state.sequence||bool(row.state.active)!=value.active;}
void Encode(const W::State& value,Packet& packet){packet.active=value.active;}
uint32_t Next(uint32_t& value){return ++value;}uint32_t Mask(){return 0;}void Bind(Targets&,uint32_t){}
#include "claim_watoy.inc"
}
#ifndef PROP_CLAIM_NO_MAIN
int main(){using namespace test;N::Session s,other;
 for(int n=1;n<=25;++n){ResetClaims(s);
  switch(n){
  case 1: for(auto bad:{Source::Count,static_cast<Source>(255)}){CHECK(!Acquire(bad));Lease fake;fake.eid=1;fake.token=1;fake.source=bad;CHECK(!H::ReleaseOwned(fake));}CHECK(!Count("descendant"));break;
  case 2: {auto first=Acquire(Source::Hook);CHECK(first);H::OnDisconnect();CHECK(H::BindSession(s,true));auto next=Acquire(Source::Hook);CHECK(next&&next.token>first.token&&!H::OwnsClaim(first));}break;
  case 3: {auto a=Acquire(Source::Hook);auto revision=Row(a)->revision;auto b=Acquire(Source::Hook);CHECK(a.token==b.token&&Row(a)->revision==revision);}break;
  case 4: {uint32_t mask=0;for(size_t i=0;i<H::kClaimSourceCount;++i){auto lease=Acquire(static_cast<Source>(i));CHECK(lease&&H::OwnsClaim(lease));mask|=1u<<i;}CHECK(H::FindByEid(1)->claimMask==mask);}break;
  case 5: for(int cause=0;cause<6;++cause){ResetClaims(s);auto lease=Acquire(Source::Hook);if(cause==0)++world;if(cause==1)worldObject=&objects[46];if(cause==2)++serial;if(cause==3)++s.stops;if(cause==4)s.mode=N::Role::Client;if(cause==5)CHECK(H::BindSession(other,true));CHECK(!H::OwnsClaim(lease)&&!H::ReleaseOwned(lease));if(cause==0||cause==2||cause==3)CHECK(!Acquire(Source::Rocket));}break;
  case 6: for(int cause=0;cause<3;++cause){ResetClaims(s);auto lease=Acquire(Source::Hook);if(cause==0)++objects[0].serial;if(cause==1)++objects[1].serial;if(cause==2)objects[0].eid=2;CHECK(!H::OwnsClaim(lease)&&!H::ReleaseOwned(lease));}break;
  case 7: for(auto stage:{"key","opened","clientEnd"}){ResetClaims(s);bool valid=true;if(std::string(stage)=="clientEnd")Seed(1,true);hook=[&](const std::string& name){if(name==stage)valid=false;};auto lease=H::ClaimOwned(&objects[0],Source::Hook,"cancel",[&]{return valid;});CHECK(!lease&&!H::HasClaims(&objects[0]));}break;
  case 8: {auto lease=Acquire(Source::Hook);for(int forged=0;forged<8;++forged){auto bad=lease;if(forged==0)++bad.token;if(forged==1)++bad.eid;if(forged==2)bad.source=Source::Watoy;if(forged==3)bad.token=0;if(forged==4)++bad.epoch;if(forged==5)++bad.world;if(forged==6)++bad.serial;if(forged==7)bad.worldObject=&objects[46];CHECK(!H::ReleaseOwned(bad)&&H::OwnsClaim(lease));}}break;
  case 9: {auto a=Acquire(Source::Hook),b=Acquire(Source::Watoy);calls.clear();CHECK(H::ReleaseOwned(a)&&H::OwnsClaim(b)&&Row(b)->claimed&&!Count("clock"));}break;
  case 10: {auto a=Acquire(Source::Hook);Lease b;bool fired=false;hook=[&](const std::string& name){if(name=="clock"&&!fired){fired=true;b=Acquire(Source::Watoy);}};CHECK(!H::ReleaseOwned(a)&&fired&&H::OwnsClaim(a)&&H::OwnsClaim(b));hook={};CHECK(H::ReleaseOwned(a)&&H::OwnsClaim(b));}break;
  case 11: {auto a=Acquire(Source::Hook);H::Claim(&objects[0],"legacy");H::Release(&objects[0]);CHECK(H::OwnsClaim(a)&&Row(a)->claimed);H::Release(&objects[0]);CHECK(H::OwnsClaim(a));}break;
  case 12: for(int cause=0;cause<3;++cause){ResetClaims(s);if(cause==0)held=true;if(cause==1)propClass=false;if(cause==2)nativeKey=L"None";CHECK(!Acquire(Source::Hook)&&H::Rows().empty());}break;
  case 13: hook=[](const std::string& name){if(name=="opened"){auto next=Seed();next.maxGapMs=99;*H::FindByEid(1)=next;}};CHECK(!Acquire(Source::Hook)&&H::FindByEid(1)->maxGapMs==99);break;
  case 14: {auto a=Acquire(Source::Hook);Lease nested;bool fired=false;hook=[&](const std::string& name){if(name=="hand"&&!fired){fired=true;nested=Acquire(Source::Rocket);}};CHECK(!Acquire(Source::Watoy)&&fired&&H::OwnsClaim(a)&&H::OwnsClaim(nested));}break;
  case 15: {auto a=Acquire(Source::Hook);Lease fresh;bool fired=false;hook=[&](const std::string& name){if(name=="clock"&&!fired){fired=true;CHECK(H::ReleaseOwned(a));fresh=Acquire(Source::Hook);}};CHECK(!H::ReleaseOwned(a)&&fresh&&fresh.token!=a.token&&!H::OwnsClaim(a)&&H::OwnsClaim(fresh));}break;
  case 16: {auto a=Acquire(Source::Hook);H::Coast(&objects[0],"hand",true);CHECK(!H::OwnsClaim(a)&&!H::HasClaims(&objects[0]));auto b=Acquire(Source::Hook);CHECK(b&&b.token!=a.token&&!H::ReleaseOwned(a));}break;
  case 17: {auto a=Acquire(Source::Hook);H::OnDisconnect();CHECK(H::BindSession(s,true));auto b=Acquire(Source::Hook);CHECK(!H::ReleaseOwned(a)&&H::OwnsClaim(b)&&b.eid==a.eid);ResetClaims(s);H::OnDisconnect();auto offline=Acquire(Source::Hook);CHECK(offline&&H::HasClaims(&objects[0]));CHECK(H::BindSession(s,true));CHECK(!H::OwnsClaim(offline)&&!H::HasClaims(&objects[0]));auto current=Acquire(Source::Watoy);CHECK(current&&Row(current)->claimMask==H::SourceBit(Source::Watoy));}break;
  case 18: {auto client=Seed(1,true);H::FindByEid(1)->claimTokens[1]=123;auto a=Acquire(Source::Hook);CHECK(a&&Row(a)->claimMask==H::SourceBit(Source::Hook)&&!Row(a)->claimTokens[0]);ResetClaims(s);client=Seed(1,true);H::EraseRow(*H::FindByEid(1));actual_handoff::Complete(client,{client},true,{},now,coop::prop_syncer_host::Ending::HostVerb);CHECK(H::FindByEid(1)->claimMask==0&&std::all_of(H::FindByEid(1)->claimTokens.begin(),H::FindByEid(1)->claimTokens.end(),[](uint64_t token){return token==0;}));H::Tick(s);CHECK(!H::FindByEid(1)->claimed&&!H::FindByEid(1)->handoffFrom);}break;
  case 19: {auto a=Acquire(Source::Hook);CHECK(!H::ElectClientRow(&objects[0],2,"blocked"));CHECK(H::ReleaseOwned(a));auto row=H::ElectClientRow(&objects[0],2,"elect");CHECK(row&&row->syncer==2&&!row->claimMask&&!H::ReleaseOwned(a));}break;
  case 20: {HK::Owned hookRow;hookRow.ref.Set(&objects[5]);ownedHooks.push_back(hookRow);attached=&objects[0];HK::TickPropClaims();auto hookLease=HK::g_claimLeases.at(&objects[0]);auto rocket=Acquire(Source::Rocket);ownedHooks.clear();HK::TickPropClaims();CHECK(!H::OwnsClaim(hookLease)&&H::OwnsClaim(rocket));ResetClaims(s);hookRow.ref.Set(&objects[5]);ownedHooks.push_back(hookRow);attached=&objects[0];hook=[](const std::string& name){if(name=="hookAttached"){HK::ResetPropClaims();HK::TickPropClaims();}};HK::TickPropClaims();CHECK(H::Rows().empty()&&HK::g_tied.empty()&&!HK::g_claimTicking);}break;
  case 21: {Register(0,E::ElementType::Prop);auto* owner=Register(5,E::ElementType::Npc);auto carry=Acquire(Source::Kerfur),hookLease=Acquire(Source::Hook);auto hold=std::make_shared<NPC::Hold>();hold->context={};CHECK(E::Registry::Get().ReadIdentity(owner->GetId(),hold->owner.identity));CHECK(hold->owner.actor.Capture(&objects[5])&&hold->prop.Capture(&objects[0]));hold->lease=carry;hold->active=false;NPC::g_holds.push_back(hold);NPC::SweepKerfurHolds(hold->context);CHECK(!H::OwnsClaim(carry)&&H::OwnsClaim(hookLease)&&thaws==0&&NPC::g_holds.empty());}break;
  case 22: {Register(0,E::ElementType::Prop);ue_wrap::script_gate::Call call{};call.object=&objects[0];KF::OnTickPost(call);auto lease=KF::g_host.at(&objects[0]).lease;CHECK(lease&&H::OwnsClaim(lease));auto hookLease=Acquire(Source::Hook);kerfusOn=false;KF::OnTickPost(call);CHECK(!H::OwnsClaim(lease)&&H::OwnsClaim(hookLease)&&!KF::g_host.at(&objects[0]).lease);ResetClaims(s);Register(0,E::ElementType::Prop);call.object=&objects[0];hook=[](const std::string& name){if(name=="kerfusEnergy")KF::OnDisconnect();};KF::OnTickPost(call);CHECK(KF::g_host.empty()&&!H::HasClaims(&objects[0]));ResetClaims(s);Register(0,E::ElementType::Prop);call.object=&objects[0];hook=[](const std::string& name){if(name=="kerfusEnergy"){auto& newer=KF::g_host.at(&objects[0]);newer.revision=++KF::g_hostRevision;newer.energy=99;}};KF::OnTickPost(call);CHECK(KF::g_host.at(&objects[0]).energy==99&&!H::HasClaims(&objects[0]));}break;
  case 23: {namespace W=actual_watoy;++W::g_epoch;W::g_worldRows.clear();watoyState.active=true;W::SampleWorld(&objects[0],now);auto lease=W::g_worldRows.at(nativeKey).lease;CHECK(lease&&H::OwnsClaim(lease));auto hookLease=Acquire(Source::Hook);watoyState.active=false;W::SampleWorld(&objects[0],now+1);CHECK(!H::OwnsClaim(lease)&&H::OwnsClaim(hookLease));ResetClaims(s);++W::g_epoch;W::g_worldRows.clear();watoyState.active=false;W::SampleWorld(&objects[0],now);watoyState.active=true;hook=[](const std::string& name){if(name=="watoyRead")++W::g_worldRows.at(nativeKey).state.sequence;};W::SampleWorld(&objects[0],now+1);CHECK(!H::HasClaims(&objects[0]));}break;
  case 24: for(auto source:{Source::Kerfur,Source::Rocket,Source::Grayboar,Source::ClientFeeder}){auto a=Acquire(source);CHECK(a&&a.source==source&&H::OwnsClaim(a));}break;
  case 25: {auto a=Acquire(Source::Hook),b=Acquire(Source::Watoy);auto generation=Row(a)->gen;H::Coast(&objects[0],"impulse");CHECK(H::OwnsClaim(a)&&H::OwnsClaim(b)&&Row(a)->claimed&&Row(a)->gen==generation);}break;
  }Pass("LS",n);
 }
 const Source pairs[][2]={{Source::Hook,Source::Kerfur},{Source::Hook,Source::Kerfus},{Source::Hook,Source::Watoy},{Source::Hook,Source::Rocket},{Source::Hook,Source::Grayboar},{Source::Kerfur,Source::Kerfus},{Source::Kerfur,Source::Watoy},{Source::Kerfus,Source::Watoy},{Source::Rocket,Source::Watoy},{Source::Grayboar,Source::Watoy}};
 for(int pair=0;pair<10;++pair)for(int scenario=0;scenario<5;++scenario){ResetClaims(s);auto a=Acquire(pairs[pair][0]),b=Acquire(pairs[pair][1]);CHECK(a&&b);
  if(scenario==0)CHECK(H::ReleaseOwned(a)&&!H::OwnsClaim(a)&&H::OwnsClaim(b)&&Row(b)->claimed);
  if(scenario==1)CHECK(H::ReleaseOwned(b)&&!H::OwnsClaim(b)&&H::OwnsClaim(a)&&Row(a)->claimed);
  if(scenario==2){CHECK(H::ReleaseOwned(a));auto fresh=Acquire(pairs[pair][0]);CHECK(fresh.token!=a.token&&!H::ReleaseOwned(a)&&H::OwnsClaim(fresh)&&H::OwnsClaim(b));}
  if(scenario==3){H::Release(&objects[0]);CHECK(H::OwnsClaim(a)&&H::OwnsClaim(b));}
  if(scenario==4){H::Coast(&objects[0],"hand episode",true);auto fresh=Acquire(pairs[pair][1]);CHECK(fresh&&!H::ReleaseOwned(a)&&!H::ReleaseOwned(b)&&H::OwnsClaim(fresh));}
  Pass("LX",pair*5+scenario+1);
 }
 ResetClaims(s);H::OnDisconnect();puts("PASS independent drive claims: 75 named fixes; actual core, hook, NPC cleanup, Kerfus and watoy sample bodies");
}

#endif
