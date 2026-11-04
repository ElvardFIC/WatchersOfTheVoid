#define SERVED_PLAYER_FIXTURE
#define KERFUS_COMMAND_FIXTURE
#define KERFUS_STATE_NO_MAIN
#include "kerfus_state_test.cpp"
#ifndef SERVED_PLAYER_SOURCE
#define SERVED_PLAYER_SOURCE "../../src/votv-coop/src/coop/creatures/served_player.cpp"
#endif
#ifndef KERFUS_FOLLOW_SOURCE
#define KERFUS_FOLLOW_SOURCE "../../src/votv-coop/src/coop/creatures/kerfus_follow.cpp"
#endif
#include SERVED_PLAYER_SOURCE
#include KERFUS_FOLLOW_SOURCE
namespace test {
namespace SP=coop::served_player;namespace F=coop::kerfus_follow;
coop::RemotePlayer puppet,otherPuppet;
bool armOK=true,hookOK=true,watchOK=true,unwatchOK=true,mainLive=true,settled=false,insert=true,onGT=true;
int frameSize=16,fieldOffset=0,fieldSize=8;std::wstring fieldType=L"ObjectProperty";
void* nativeCallee=nullptr;void* nativeValue=nullptr;void* resultStorage=&nativeValue;uint8_t* outStorage=nullptr;
bool actualArmed[48]{};uint64_t replacementToken=0;
void ResetService(N::Session& s){hook={};F::OnDisconnect();SP::OnDisconnect();ResetState(s);
 armOK=hookOK=watchOK=unwatchOK=mainLive=insert=onGT=true;settled=false;frameSize=16;fieldOffset=0;fieldSize=8;fieldType=L"ObjectProperty";nativeCallee=&objects[40];nativeValue=&objects[6];resultStorage=&nativeValue;outStorage=nullptr;replacementToken=0;
 SP::g_gpc.fn.Reset();SP::g_gpc.owner.Reset();SP::g_gpc.installed=SP::g_gpc.armed=SP::g_gpc.refused=false;
 SP::g_gpp.fn.Reset();SP::g_gpp.owner.Reset();SP::g_gpp.installed=SP::g_gpp.armed=SP::g_gpp.refused=false;
 SP::g_mainPrimed=SP::g_mainWatched=SP::g_mainRefused=false;
 std::fill(std::begin(actualArmed),std::end(actualArmed),false);
 puppet={};puppet.body=&objects[5];otherPuppet={};otherPuppet.body=&objects[7];
 for(auto& p:coop::players::testPuppets)p=&puppet;
 coop::players::puppetHook=[]{Call("puppetLookup");};coop::service_test::hook=Call;
 SP::Install(&s);F::Install(&s);CHECK(SP::SeamsReady(SP::kAll));calls.clear();}
SP::Lease Service(uint8_t seams=SP::kPlayerCharacter|SP::kPlayerPawn,uint8_t slot=1){return SP::ServeOwned(&objects[0],slot,seams);}
void Follow(){F::OnToggle(&objects[0],1);CHECK(F::g_activator.count(&objects[0]));}
void Answer(){SP::OnCharacterPost(nullptr,&objects[0],nullptr);}
ue_wrap::script_gate::Call Main(uint8_t* locals){ue_wrap::script_gate::Call c{};c.object=&objects[39];c.function=&objects[43];c.callerObject=&objects[0];c.callerFunction=&objects[42];c.callerLocals=locals;return c;}
void ReplaceService(){
 if(auto it=SP::g_records.find(&objects[0]);it!=SP::g_records.end()){
  auto row=std::make_shared<SP::Rec>(*it->second);row->lease.token=++SP::g_token;replacementToken=row->lease.token;SP::g_records[&objects[0]]=row;
 }
 if(auto it=F::g_activator.find(&objects[0]);it!=F::g_activator.end()){
  auto row=std::make_shared<F::Rec>(*it->second);row->attempt=++F::g_attempt;if(replacementToken)row->lease.token=replacementToken;F::g_activator[&objects[0]]=row;
 }
}
void Retire(int cause){if(cause==0)++world;if(cause==1)++serial;if(cause==2)++objects[0].serial;if(cause==3)++objects[1].serial;if(cause==4)ReplaceService();}
}
namespace ue_wrap::game_thread {bool IsGameThread(){return test::onGT;}}
namespace ue_wrap::reflection {
void* FindClass(const wchar_t* name){return std::wstring(name)==L"lib_C"?&test::objects[45]:std::wstring(name)==L"kerfurOmega_C"?&test::objects[46]:&test::objects[44];}
void* OuterOf(void* object){if(object==&test::objects[43])return &test::objects[45];if(object==&test::objects[42])return &test::objects[46];return &test::objects[44];}
void* FindFunction(void* cls,const wchar_t* name){if(cls==&test::objects[45])return &test::objects[43];if(cls==&test::objects[46]){
#ifdef OMEGA_COMMAND_FIXTURE
if(std::wstring(name)==L"startKill")return &test::objects[37];
#endif
return &test::objects[42];}return std::wstring(name)==L"GetPlayerCharacter"?&test::objects[40]:&test::objects[41];}
int32_t FunctionFrameSize(void*){return test::frameSize;}
int32_t FindParamOffset(void*,const wchar_t* name){test::Call(std::wstring(name)==L"AsMain Player"?"mainMeta":"returnMeta");return test::fieldOffset;}
int32_t FindParamSize(void*,const wchar_t*){return test::fieldSize;}
bool PropertyOffsetAndClass(void* owner,const wchar_t* name,int32_t& offset,std::wstring& type){
#ifdef OMEGA_COMMAND_FIXTURE
if(owner==&test::objects[1]){type=L"ObjectProperty";const std::wstring field=name;
 if(field==L"holding_actor")offset=offsetof(test::Object,hold);else if(field==L"grabbing_actor")offset=offsetof(test::Object,grab);else if(field==L"viewer")offset=offsetof(test::Object,viewer);else if(field==L"holding_name"){offset=offsetof(test::Object,handName);type=L"NameProperty";}else return false;return true;}
#endif
offset=test::fieldOffset;type=test::fieldType;return true;}

}
namespace ue_wrap::facts {Resolved ResolveOn(const Function& row,void* cls){return {reflection::FindFunction(cls,row.Name()),&row};}}
namespace ue_wrap::script_gate {
bool WatchName(const facts::Function&,int,PreFn,PostFn){test::Call("watchMain");return test::watchOK;}
bool UnwatchName(const facts::Function&,int,PreFn,PostFn){test::Call("unwatchMain");return test::unwatchOK;}
void ResolvePendingNames(){test::Call("resolveMain");}
bool NameWatchLive(const facts::Function&,int){test::Call("mainLive");return test::mainLive;}
bool NameWatchSettled(const facts::Function&,int){return test::settled;}
uint8_t* OutParamPtr(const Call&,int32_t){test::Call("outPointer");return test::outStorage;}
}
namespace ue_wrap::ufunction_hook {
bool InstallPostHook(const facts::Resolved&,PostNativeCallback,bool armed){CHECK(!armed);test::Call("installHook");return test::hookOK;}
bool SetArmed(void* fn,PostNativeCallback,bool armed){if(test::armOK)test::actualArmed[static_cast<test::Object*>(fn)->index]=armed;test::Call("armNative");return test::armOK;}
void* CurrentNativeFunction(){return test::nativeCallee;}
void* CurrentResult(){test::Call("nativeResult");return test::resultStorage;}
}
namespace ue_wrap::kerfur {
bool IsInsertPlayerReadWhile(void* fn,const uint8_t* locals,const uint8_t* out,const std::function<bool()>& current){if(!current())return false;test::Call("insertSelect");return current()&&test::insert&&fn==&test::objects[42]&&out==locals+8;}
}
#ifndef SERVED_PLAYER_NO_MAIN
int main(){using namespace test;N::Session s,other;
 for(int n=1;n<=25;++n){if(n>=24)continue;ResetService(s);
 switch(n){
 case 1: for(int cause=0;cause<7;++cause){ResetService(s);auto lease=Service();CHECK(lease);if(cause==0)++world;if(cause==1)++serial;if(cause==2)++s.stops;if(cause==3)s.mode=N::Role::Client;if(cause==4)s.linked=false;if(cause==5)SP::g_session.store(&other);if(cause==6)worldObject=&objects[46];CHECK(!SP::Owns(lease));Answer();CHECK(nativeValue==&objects[6]);SP::Tick();CHECK(SP::g_records.empty());}ResetService(s);{auto before=*SP::g_bound;SP::OnDisconnect();SP::Install(&s);CHECK(!before.Current());}break;
 case 2: for(int cause=2;cause<=3;++cause){ResetService(s);auto lease=Service();Retire(cause);CHECK(!SP::Owns(lease));Answer();CHECK(nativeValue==&objects[6]);}break;
 case 3: for(int cause=0;cause<4;++cause){ResetService(s);auto lease=Service();if(cause==0)++s.generations[1];if(cause==1)++s.worldRevisions[1];if(cause==2)s.generations[1]=0;if(cause==3)s.ready[1]=false;CHECK(!SP::Owns(lease));Answer();CHECK(nativeValue==&objects[6]);}break;
 case 4: {auto a=Service(),b=Service();CHECK(a&&b&&a.token!=b.token&&!SP::Owns(a)&&SP::Owns(b)&&!SP::UnserveOwned(a));ResetService(s);bool allowed=true;hook=[&](const std::string& name){if(name=="armNative")allowed=false;};CHECK(!SP::ServeOwned(&objects[0],1,SP::kPlayerCharacter,[&]{return allowed;})&&SP::g_records.empty());}break;
 case 5: Follow();{auto old=F::g_activator.at(&objects[0]);auto newer=Service();F::Drop(old);CHECK(SP::Owns(newer));}break;
 case 6: readable=false;F::OnToggle(&objects[0],1);CHECK(SP::g_records.empty()&&F::g_activator.empty());break;
 case 7: Follow();objects[0].state|=1;{bool fired=false;hook=[&](const std::string& name){if(!fired&&name=="hostActive"){fired=true;ReplaceService();F::g_activator.reserve(4096);}};F::Tick();CHECK(fired&&F::g_activator.size()==1&&!F::g_activator.at(&objects[0])->on);}break;
 case 8: {bool fired=false;hook=[&](const std::string& name){if(!fired&&name=="hostActive"){fired=true;ReplaceService();}};F::OnToggle(&objects[0],1);CHECK(fired&&F::g_activator.size()==1&&SP::g_records.empty());}break;
 case 9: Follow();{auto lease=F::g_activator.at(&objects[0])->lease;readable=false;F::Tick();CHECK(F::g_activator.size()==1&&SP::Owns(lease));readable=true;objects[0].state|=1;F::Tick();CHECK(F::g_activator.at(&objects[0])->on);objects[0].state&=~1;F::Tick();CHECK(F::g_activator.empty()&&!SP::Owns(lease));}break;
 case 10: {auto lease=Service();F::OnToggle(&objects[0],0);CHECK(SP::Owns(lease));}break;
 case 11: {auto lease=Service();CHECK(lease);for(uint8_t seams:{0,8,255})CHECK(!SP::ServeOwned(&objects[0],1,seams));for(uint8_t slot:{0,32,255})CHECK(!SP::ServeOwned(&objects[0],slot,1));CHECK(SP::Owns(lease));}break;
 case 12: Follow();{bool fired=false;hook=[&](const std::string& name){if(!fired&&name=="hostActive"){fired=true;F::OnDisconnect();CHECK(F::g_ticking);F::Tick();CHECK(F::g_ticking);}};F::Tick();CHECK(fired&&!F::g_ticking&&F::g_activator.empty());}ResetService(s);{bool fired=false;hook=[&](const std::string& name){if(!fired&&name=="armNative"){fired=true;++world;}};CHECK(!Service()&&fired);hook={};SP::Tick();CHECK(!actualArmed[40]&&!actualArmed[41]);}break;
 case 13: {auto lease=Service();CHECK(lease);++objects[40].serial;CHECK(!SP::ServedSlot(&objects[0]));SP::Tick();CHECK(SP::g_gpc.fn.Get()==&objects[40]&&SP::g_gpc.installed);ResetService(s);CHECK(Service(SP::kAll));++objects[43].serial;Answer();CHECK(nativeValue==&objects[6]);SP::Tick();Answer();CHECK(nativeValue==&objects[5]);}break;
 case 14: {bool fired=false;hook=[&](const std::string& name){if(!fired&&name=="armNative"){fired=true;ReplaceService();SP::Tick();}};CHECK(!Service()&&fired&&replacementToken&&SP::g_records.at(&objects[0])->lease.token==replacementToken);}break;
 case 15: armOK=false;CHECK(!Service());CHECK(SP::g_records.empty());armOK=true;CHECK(Service());Answer();CHECK(nativeValue==&objects[5]);ResetService(s);mainLive=false;CHECK(!Service(SP::kAll)&&SP::g_records.empty());mainLive=true;CHECK(Service(SP::kAll));break;
 case 16: CHECK(Service());nativeCallee=&objects[41];Answer();CHECK(nativeValue==&objects[6]);nativeCallee=&objects[40];Answer();CHECK(nativeValue==&objects[5]);break;
 case 17: CHECK(Service());{bool fired=false;hook=[&](const std::string& name){if(!fired&&name=="nativeResult"){fired=true;++objects[5].serial;}};Answer();CHECK(fired&&nativeValue==&objects[6]);}ResetService(s);CHECK(Service());{bool fired=false;hook=[&](const std::string& name){if(!fired&&name=="puppetValid"){fired=true;coop::players::testPuppets[1]=&otherPuppet;}};Answer();CHECK(fired&&nativeValue==&objects[6]&&!Count("puppetBody")&&!Count("puppetPose"));}break;
 case 18: for(int fault=0;fault<6;++fault){ResetService(s);CHECK(Service(SP::kMainPlayer));uint8_t locals[32]{};outStorage=locals+8;void* original=&objects[6];std::memcpy(outStorage,&original,8);if(fault==0)frameSize=0;if(fault==1)fieldOffset=-1;if(fault==2)fieldOffset=12;if(fault==3)fieldSize=4;if(fault==4)fieldType=L"Int64Property";if(fault==5)outStorage=nullptr;CHECK(SP::OnMainPlayerPre(Main(locals))==ue_wrap::script_gate::Verdict::Run);}break;
 case 19: CHECK(Service(SP::kMainPlayer));{uint8_t locals[32]{};outStorage=locals+8;insert=false;CHECK(SP::OnMainPlayerPre(Main(locals))==ue_wrap::script_gate::Verdict::Run);insert=true;outStorage=locals+16;CHECK(SP::OnMainPlayerPre(Main(locals))==ue_wrap::script_gate::Verdict::Run);outStorage=locals+8;CHECK(SP::OnMainPlayerPre(Main(locals))==ue_wrap::script_gate::Verdict::Cancel);}break;
 case 20: CHECK(Service(SP::kAll));puppet.posed=false;Answer();CHECK(nativeValue==&objects[6]);{uint8_t locals[32]{};outStorage=locals+8;void* value=&objects[6];std::memcpy(outStorage,&value,8);CHECK(SP::OnMainPlayerPre(Main(locals))==ue_wrap::script_gate::Verdict::Cancel);std::memcpy(&value,outStorage,8);CHECK(value==nullptr);}break;
 case 21: CHECK(SP::Serve(&objects[0],31,SP::kPlayerPawn));CHECK(SP::ServedSlot(&objects[0])==31);SP::Unserve(&objects[0]);CHECK(SP::ServedSlot(&objects[0])==0);break;
 case 22: {auto lease=Service();bool fired=false;hook=[&](const std::string& name){if(!fired&&name=="armNative"){fired=true;auto fresh=Service(SP::kPlayerCharacter,31);CHECK(fresh);}};SP::OnPeerLeft(1);CHECK(fired&&!SP::Owns(lease)&&SP::ServedSlot(&objects[0])==31);}break;
 case 23: Follow();{bool fired=false;hook=[&](const std::string& name){if(!fired&&name=="armNative"){fired=true;F::Install(&s);SP::Install(&s);F::OnToggle(&objects[0],31);}};F::OnDisconnect();CHECK(fired&&F::g_activator.count(&objects[0])&&F::g_activator.at(&objects[0])->slot==31);}break;
 }Pass("SF",n);}
 const char* stages[]={"hostActive","hostActive","armNative","watchMain","puppetLookup","puppetBody","nativeResult","mainMeta","outPointer","insertSelect"};
 for(int stage=0;stage<10;++stage)for(int cause=0;cause<5;++cause){ResetService(s);if(stage==1){Follow();objects[0].state|=1;}if(stage>=4)CHECK(Service(stage>=7?SP::kMainPlayer:SP::kPlayerCharacter));bool fired=false;hook=[&](const std::string& name){if(!fired&&name==stages[stage]){fired=true;Retire(cause);}};
 if(stage==0)F::OnToggle(&objects[0],1);else if(stage==1)F::Tick();else if(stage==2)Service();else if(stage==3)Service(SP::kMainPlayer);else if(stage<7)Answer();else {uint8_t locals[32]{};outStorage=locals+8;void* before=&objects[6];std::memcpy(outStorage,&before,8);CHECK(SP::OnMainPlayerPre(Main(locals))==ue_wrap::script_gate::Verdict::Run);void* after=nullptr;std::memcpy(&after,outStorage,8);CHECK(after==before);}
 CHECK(fired);hook={};if(stage>=4&&stage<7)CHECK(nativeValue==&objects[6]);if(stage==1&&cause==4)CHECK(!F::g_activator.at(&objects[0])->on);if(cause==4&&stage>=2)CHECK(SP::g_records.at(&objects[0])->lease.token==replacementToken);Pass("SM",stage*5+cause+1);
 }
 ResetService(s);F::OnDisconnect();SP::OnDisconnect();H::OnDisconnect();puts("PASS follow/services: 23 base cases and 50 replacement cells; actual controllers, Registry and leases");
}

#endif
