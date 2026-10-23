// Actual controller, native wrapper and ParamFrame. Engine objects/VM/transport
// edges are controlled; no rendered gameplay is claimed.
#define main WallWrapperBaseline
#include "wall_wrapper_2166_test.cpp"
#undef main
#include "coop/net/session.h"
#include <map>
#include <set>
#include <memory>
#ifndef WALL_DELIVERY_SOURCE
#define WALL_DELIVERY_SOURCE "../../src/votv-coop/src/coop/world/wall_build_sync.cpp"
#endif
#include WALL_DELIVERY_SOURCE

namespace delivery {
namespace S=coop::wall_build_sync;
namespace SG=ue_wrap::script_gate;
using V=SG::Verdict;
const char* scenario="setup";
uint32_t serial=1;
uint64_t invocation=0;
std::set<uint64_t> active;
coop::net::Session session,other,third;
std::function<void()> onTrack,onBody,onInstall,onChanged,onPreDispatch;
bool bodyOK=true,post=true;
int tracked=0,changed=0;
std::map<std::wstring,int> attempts;
std::set<std::wstring> refused;
uint8_t* external=nullptr;
void Fire(std::function<void()>& slot){auto callback=std::move(slot);slot={};if(callback)callback();}
void Fail(int line,const char* expr){std::fprintf(stderr,"FAIL %s line %d: %s\n",scenario,line,expr);std::exit(1);}
}
#undef CHECK
#define CHECK(x) do{if(!(x))delivery::Fail(__LINE__,#x);}while(0)
namespace coop::net::session_serial {uint32_t Current(){return delivery::serial;}}
namespace ue_wrap::script_gate {
bool IsInvocationActive(uint64_t id){return delivery::active.count(id)!=0;}
bool WatchClassName(const wchar_t*,const wchar_t* fn,int,PreFn,PostFn){++delivery::attempts[fn];return !delivery::refused.count(fn);}
uint8_t* OutParamPtr(const Call& call,int32_t off){return delivery::external?delivery::external:call.locals?call.locals+off:nullptr;}
}
namespace ue_wrap::reflection {
int32_t ParamOffset::Of(void*){return 0;}
int32_t FindParamOffset(void*,const wchar_t* n){return !wcscmp(n,L"isFine")?8:0;}
bool FindBoolProperty(void*,const wchar_t*,int32_t& off,uint8_t& mask){off=8;mask=1;return true;}
}
namespace coop::wall_creative_sync {
void Install(net::Session*){delivery::Fire(delivery::onInstall);}
void Tick(){}void OnDisconnect(){}
bool NativeCurrent(void*){return true;}
ue_wrap::script_gate::Verdict BeforeBuild(const ue_wrap::script_gate::Call&){return ue_wrap::script_gate::Verdict::Cancel;}
}
namespace coop::placed_save_sync {
void TrackWall(void*){++delivery::tracked;delivery::Fire(delivery::onTrack);}
void MarkChanged(void*){++delivery::changed;delivery::Fire(delivery::onChanged);}
}
namespace delivery {
using namespace test;
struct Frame{alignas(16) uint8_t args[16]{};SG::Call call{};
 Frame(void* wall=&objs[0],void* material=&objs[1]){call.object=wall;call.function=&objs[4];call.locals=args;call.invocation=++invocation;std::memcpy(args,&material,8);active.insert(call.invocation);}
 ~Frame(){active.erase(call.invocation);}
};
V Queue(void* wall=&objs[0],void* material=&objs[1],bool ours=false){Frame frame(wall,material);frame.call.fromOurCode=ours;return S::BeforeOverlap(frame.call);}
void Ready(){for(auto& p:S::g_pending)p.next=0;}
void Reset(){
 onTrack=onBody=onInstall=onChanged=onPreDispatch={};metadata={};nativeFeed={};S::OnDisconnect();active.clear();
 for(int i=0;i<400;++i){const int next=objs[i].serial+1;objs[i]={};objs[i].id=i;objs[i].serial=next;}
 ++serial;++world;touches=callOk=fields=materialOK=locationOK=damageFields=true;kill=solid=false;
 flexible=true;feeds=queries=tracked=changed=0;overlapCallback=nameCallback=locCallback=nullptr;external=nullptr;
 bodyOK=post=true;session={};other={};third={};objs[0].overlap=&objs[2];attempts.clear();refused.clear();
 std::fill(std::begin(S::g_registered),std::end(S::g_registered),false);S::g_watched=false;S::g_bodies.clear();
 nativeFeed=[](void* wall,void* material,void* raw){
  Frame frame(wall,material);frame.call.locals=static_cast<uint8_t*>(raw);frame.call.fromOurCode=true;
  Fire(onPreDispatch);
  if(S::BeforeOverlap(frame.call)!=V::Run)return true;
  ++feeds;Fire(onBody);if(bodyOK&&post)S::Changed(frame.call);return bodyOK;
 };
 S::Install(&session);
}
void NativeOnce(){Reset();CHECK(Queue()==V::Cancel);S::Tick();S::Tick();CHECK(feeds==1&&queries==1&&changed==1&&S::g_pending.empty());}
void EnteredFailure(){Reset();bodyOK=false;CHECK(Queue()==V::Cancel);S::Tick();Ready();S::Tick();CHECK(feeds==1&&changed==1&&S::g_pending.empty());}
void RetryBeforeEntry(){Reset();callOk=false;Queue();S::Tick();CHECK(feeds==0&&queries==1&&S::g_pending.size()==1);S::Tick();CHECK(queries==1);callOk=true;Ready();S::Tick();CHECK(feeds==1&&S::g_pending.empty());}
void RetryFairness(){Reset();callOk=false;for(int i=10;i<22;++i)Queue(&objs[0],&objs[i]);S::Tick();CHECK(queries==8);S::Tick();CHECK(queries==12);callOk=true;Ready();S::Tick();CHECK(feeds==8);S::Tick();CHECK(feeds==12&&S::g_pending.empty());}
void ContextRetirement(){for(int mode=0;mode<6;++mode){Reset();Queue();if(mode==0)++serial;if(mode==1)++world;if(mode==2)session.mode=coop::net::Role::Client;if(mode==3)S::Install(&other);if(mode==4){++serial;S::Install(&session);}if(mode==5)session.active=false;S::Tick();if(mode==5){session.active=true;S::Tick();}CHECK(feeds==0&&queries==0&&S::g_pending.empty());}}
int queryMode=0;
void QueryChange(){switch(queryMode){case 0:++serial;break;case 1:++world;break;case 2:session.mode=coop::net::Role::Client;break;case 3:session.active=false;break;case 4:S::OnDisconnect();break;case 5:S::OnDisconnect();S::Install(&session);break;case 6:S::Install(&other);break;case 7:++objs[0].serial;break;case 8:++objs[1].serial;break;case 9:objs[0].cls=&objs[60];break;case 10:objs[1].cls=&objs[61];break;case 11:++objs[2].serial;break;case 12:objs[2].cls=&objs[62];break;case 13:++objs[3].serial;break;}}
void QueryRetirement(){for(queryMode=0;queryMode<14;++queryMode){Reset();Queue();overlapCallback=QueryChange;S::Tick();CHECK(feeds==0);}}
void EnrollmentRetirement(){for(int mode=0;mode<5;++mode){Reset();onTrack=[=]{if(mode==0)++serial;if(mode==1)++world;if(mode==2)++objs[1].serial;if(mode==3)objs[0].cls=&objs[60];if(mode==4)S::Install(&other);};CHECK(Queue()==V::Cancel);S::Tick();CHECK(feeds==0&&queries==0&&S::g_pending.empty());}}
void EnrollmentReentry(){Reset();Queue();onTrack=[]{CHECK(Queue(&objs[0],&objs[1],true)==V::Cancel);S::Tick();};S::Tick();S::Tick();CHECK(feeds==1&&S::g_pending.empty());Reset();Queue();onTrack=[]{CHECK(Queue(&objs[0],&objs[10],true)==V::Cancel);};S::Tick();CHECK(feeds==1&&S::g_pending.size()==1);S::Tick();CHECK(feeds==2&&S::g_pending.empty());}
void BodyReentry(){Reset();Queue();onBody=[]{CHECK(Queue()==V::Cancel);S::Tick();Queue(&objs[0],&objs[10]);};S::Tick();CHECK(feeds==1&&S::g_pending.size()==1);S::Tick();CHECK(feeds==2&&S::g_pending.empty());}
void MetadataRetirement(){for(const char* stage:{"offset","find-query","query-params","find-feed","feed-params"})for(int mode=0;mode<4;++mode){Reset();Queue();bool fired=false;metadata=[&](const char* name){if(fired||std::string(name)!=stage)return;fired=true;if(mode==0)++serial;if(mode==1)++world;if(mode==2)objs[0].cls=&objs[60];if(mode==3)session.mode=coop::net::Role::Client;};S::Tick();CHECK(fired&&feeds==0);CHECK(queries==(std::string(stage)=="find-feed"||std::string(stage)=="feed-params"?1:0));}}
void PreDispatchRetirement(){for(int mode=0;mode<4;++mode){Reset();Queue();onPreDispatch=[=]{if(mode==0)++objs[0].serial;if(mode==1)++objs[1].serial;if(mode==2)objs[0].cls=&objs[60];if(mode==3)objs[1].cls=&objs[61];};S::Tick();S::Tick();CHECK(feeds==0&&S::g_pending.empty());}}
void ComponentRetry(){Reset();Queue();overlapCallback=[]{++objs[2].serial;};S::Tick();CHECK(feeds==0&&S::g_pending.size()==1);overlapCallback=nullptr;Ready();S::Tick();CHECK(feeds==1&&queries==2&&S::g_pending.empty());}
void BodyCapacity(){Reset();std::vector<std::unique_ptr<Frame>> frames;for(int i=0;i<64;++i){frames.push_back(std::make_unique<Frame>());CHECK(S::BeforeMutation(frames.back()->call)==V::Run);}Frame repair;repair.args[8]=0x80;CHECK(S::BeforeRepair(repair.call)==V::Cancel&&repair.args[8]==0x81);frames.clear();S::Tick();CHECK(S::g_bodies.empty());Frame fresh;CHECK(S::BeforeRepair(fresh.call)==V::Run);S::Changed(fresh.call);CHECK(changed==1);}
void RegistrationRetry(){Reset();std::fill(std::begin(S::g_registered),std::end(S::g_registered),false);S::g_watched=false;attempts.clear();refused.insert(L"buildWall");S::Install(&session);CHECK(attempts.size()==6&&attempts[L"fix"]==1&&!S::g_watched);refused.clear();S::Install(&session);CHECK(S::g_watched&&attempts[L"buildWall"]==2&&attempts[L"fix"]==1);}
void StalePost(){for(int mode=0;mode<5;++mode){Reset();Frame body;CHECK(S::BeforeMutation(body.call)==V::Run);if(mode==0)++serial;if(mode==1)++world;if(mode==2)S::Install(&other);if(mode==3)++objs[0].serial;if(mode==4)objs[0].cls=&objs[60];S::Changed(body.call);CHECK(changed==0);}}
void NoSpendRepair(){Reset();session.mode=coop::net::Role::Client;Frame body;body.args[8]=0x80;uint8_t result=0x80;external=&result;CHECK(S::BeforeRepair(body.call)==V::Cancel&&body.args[8]==0x81&&result==0x81);external=nullptr;Reset();Frame current;onTrack=[]{S::OnDisconnect();};CHECK(S::BeforeRepair(current.call)==V::Cancel&&current.args[8]==1);}
void PartialPublication(){Reset();post=false;Queue();onBody=[]{objs[1].live=false;};S::Tick();CHECK(feeds==1&&changed==1&&S::g_pending.empty());}
void NewSessionQueue(){Reset();Queue();onBody=[]{S::OnDisconnect();S::Install(&other);Queue(&objs[0],&objs[10]);};S::Tick();CHECK(feeds==1&&changed==0&&S::g_pending.size()==1);S::Tick();CHECK(feeds==2&&changed==1&&S::g_pending.empty());}
void NestedInstall(){Reset();onInstall=[]{S::Install(&third);};S::Install(&other);CHECK(S::g_session==&third);}
void SoloHost(){Reset();Queue();session.linked=false;S::Tick();CHECK(feeds==1&&S::g_pending.empty());Frame direct;CHECK(S::BeforeOverlap(direct.call)==V::Run);S::Changed(direct.call);CHECK(changed==2);}
}
int main(int argc,char** argv){using namespace delivery;struct Case{const char* name;void(*run)();};const Case cases[]={
 {"native-once",NativeOnce},{"entered-failure",EnteredFailure},{"retry-before-entry",RetryBeforeEntry},{"retry-fairness",RetryFairness},
 {"context-retirement",ContextRetirement},{"query-retirement",QueryRetirement},{"enrollment-retirement",EnrollmentRetirement},
 {"enrollment-reentry",EnrollmentReentry},{"body-reentry",BodyReentry},{"metadata-retirement",MetadataRetirement},
 {"registration-retry",RegistrationRetry},{"stale-post",StalePost},{"no-spend-repair",NoSpendRepair},
 {"partial-publication",PartialPublication},{"new-session-queue",NewSessionQueue},{"nested-install",NestedInstall},{"solo-host",SoloHost},
 {"pre-dispatch-retirement",PreDispatchRetirement},{"component-retry",ComponentRetry},{"body-capacity",BodyCapacity}};
 bool ran=false;for(const auto& item:cases)if(argc<2||std::string(argv[1])==item.name){scenario=item.name;item.run();std::printf("PASS wall delivery %s\n",scenario);ran=true;}
 CHECK(ran);onTrack=onBody=onInstall=onChanged=onPreDispatch={};test::metadata={};test::nativeFeed={};S::OnDisconnect();std::puts("PASS actual wall controller, native wrapper and ParamFrame");return 0;}
