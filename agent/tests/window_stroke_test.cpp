#ifndef WINDOW_STROKE_SOURCE
#define WINDOW_STROKE_SOURCE "../../src/votv-coop/src/coop/interactables/window_stroke_sync.cpp"
#endif
#include WINDOW_STROKE_SOURCE
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>

const char* currentCase="setup";
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %s line %d: %s\n",currentCase,__LINE__,#x);std::exit(1);}}while(0)
namespace test {
namespace S=coop::window_stroke_sync;
struct Object {int id=0,serial=1;bool live=true;} objects[4];
uint32_t world=1,serial=1;
bool resolved=true,brush=true,signal=false,draw=true,install=true,arm=true,stroke=true;
bool prepared=true;
int installs=0,arms=0,releases=0,brushReads=0,draws=0;
float size=32,opac=.4f,col=0,x=1,y=2;
std::vector<ue_wrap::window_canvas::Dab> applied;
std::function<void()> onRead,onDraw,onSignal,onResolve,onInstall,onArm,onCorner,onRelease;
void Run(std::function<void()>& slot){auto callback=std::move(slot);slot={};if(callback)callback();}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p && static_cast<test::Object*>(p)->live;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Object*>(p)->id:-1;}
int32_t SlotSerial(int32_t index){return test::objects[index].serial;}
int32_t AllocateSlotSerial(int32_t index){return SlotSerial(index);}
bool IsLiveByIndex(void* p,int32_t index){return index>=0 && index<4 && p==&test::objects[index] && IsLive(p);}
const FName& NameOf(void*){static FName name{};return name;}
bool NameStartsWith(const FName&,const wchar_t*){return false;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return nullptr;}
}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace coop::element::scan_hub {void Register(const Consumer&) {}}
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* id,const char* feature,const wchar_t* cls,const wchar_t* fn,Mode mode)
    :ident_(id),feature_(feature),cls_(cls),fn_(fn),mode_(mode){}
Resolved Resolve(const Function& row){return Resolved(&test::objects[1],&row);}
}
namespace ue_wrap::ufunction_hook {
bool InstallPostHook(const facts::Resolved&,PostNativeCallback,bool){++test::installs;test::Run(test::onInstall);return test::install;}
bool SetArmed(void*,PostNativeCallback,bool){++test::arms;test::Run(test::onArm);return test::arm;}
}
namespace ue_wrap::window_canvas {
bool EnsureResolved(){test::Run(test::onResolve);return test::resolved;}
bool IsWindow(void* p){return reflection::IsLive(p);}
bool IsStrokeDraw(void*){return test::stroke;}
bool ReadStrokeCorner(void*,float& x,float& y){x=test::x;y=test::y;test::Run(test::onCorner);return true;}
bool ReadHeldBrush(void*,float& size,float& opac,float& col,const std::function<bool()>& current){++test::brushReads;size=test::size;opac=test::opac;col=test::col;test::Run(test::onRead);return test::brush && (!current || current());}
bool IsSignalPlaying(){test::Run(test::onSignal);return test::signal;}
DrawResult DrawDab(void*,const Dab& dab,const std::function<bool()>& current){
    if(!test::prepared || (current && !current()))return DrawResult::Retry;
    ++test::draws;test::applied.push_back(dab);test::Run(test::onDraw);
    return test::draw && (!current || current())?DrawResult::Applied:DrawResult::Attempted;
}
void ReleaseBrush(){++test::releases;test::Run(test::onRelease);}
}
namespace test {
void Index(){S::HubPassBegin(nullptr,true);S::HubMatch(nullptr,&objects[0]);S::HubPassComplete(nullptr,true,world);}
void Reset(coop::net::Session& s){
    onRead=onDraw=onSignal=onResolve=onInstall=onArm=onCorner=onRelease={};
    S::OnDisconnect();++world;++serial;s={};s.ready.fill(true);
    for(int i=0;i<4;++i){objects[i]={};objects[i].id=i;}
    resolved=brush=draw=install=arm=stroke=prepared=true;signal=false;
    size=32;opac=.4f;col=0;x=1;y=2;
    installs=arms=releases=brushReads=draws=0;applied.clear();
    S::g_hookInstalled=S::g_hookArmed=false;S::g_hookIdentity.Reset();
    S::Install(&s);Index();S::Tick();
}
void Observe(float corner=1){x=corner;S::OnDrawPost(nullptr,&objects[0],nullptr);}
coop::net::WindowStrokePayload Packet(float px=1){return {px,2,32,.4f,0};}
void Receive(float px=1){S::OnReliable(Packet(px),1);}
bool Select(int argc,char** argv,const char* name){currentCase=name;return argc<2 || std::string(argv[1])==name;}
}
int main(int argc,char** argv){using namespace test;coop::net::Session s;
    if(Select(argc,argv,"refused-fifo")){
        Reset(s);Observe(1);Observe(2);s.allow=[](const auto&){return false;};S::Tick();
        CHECK(s.sent.empty() && S::g_outbound.size()==2);
        size=99;opac=3;S::Tick();CHECK(brushReads==1);
        s.allow={};S::Tick();CHECK(s.sent.size()==2 && s.sent[0].x==1 && s.sent[1].x==2 && s.sent[0].size==32);
    }
    if(Select(argc,argv,"send-budget")){
        Reset(s);for(int i=0;i<20;++i)Observe(float(i));S::Tick();CHECK(s.sent.size()==8 && S::g_outbound.size()==12);
        S::Tick();S::Tick();CHECK(s.sent.size()==20 && S::g_outbound.empty());
    }
    if(Select(argc,argv,"nested-send")){
        Reset(s);Observe(1);s.allow=[&](const auto& p){CHECK(p.x==1);Observe(2);S::Tick();return true;};S::Tick();
        CHECK(s.sent.size()==1 && S::g_outbound.size()==1);s.allow={};S::Tick();CHECK(s.sent.size()==2 && s.sent.back().x==2);
    }
    if(Select(argc,argv,"send-retirement")){
        for(int cause=0;cause<6;++cause){Reset(s);Observe();Receive();s.allow=[&,cause](const auto&){
            if(cause==0)++world;if(cause==1)++serial;if(cause==2)s.mode=coop::net::Role::Client;
            if(cause==3)s.active=false;if(cause==4)s.linked=false;if(cause==5)S::OnDisconnect();return true;};
            S::Tick();CHECK(draws==0);S::Tick();CHECK(S::g_outbound.empty() && S::g_inbound.empty());}
        Reset(s);Observe();s.allow=[&](const auto&){S::OnDisconnect();S::Install(&s);S::g_armed=true;Observe(77);return true;};
        S::Tick();CHECK(S::g_outbound.size()==1 && S::g_outbound.front().dab.x==77);
    }
    if(Select(argc,argv,"brush-retirement")){
        Reset(s);Observe();onRead=[&]{++serial;};S::Tick();CHECK(s.sent.empty());
        Reset(s);Observe();onRead=[&]{Observe(2);S::Tick();};S::Tick();CHECK(s.sent.size()==1 && S::g_outbound.size()==1);
        Reset(s);Observe();brush=false;S::Tick();CHECK(S::g_outbound.empty() && s.sent.empty());
        Reset(s);Observe();size=NAN;S::Tick();CHECK(s.sent.empty());
    }
    if(Select(argc,argv,"source-reset")){
        for(int cause=0;cause<6;++cause){Reset(s);Observe();Receive();
            if(cause==0)++world;if(cause==1)++serial;if(cause==2)s.mode=coop::net::Role::Client;
            if(cause==3)s.active=false;if(cause==4)s.linked=false;
            coop::net::Session replacement;if(cause==5)S::Install(&replacement);
            S::Tick();CHECK(s.sent.empty() && applied.empty() && S::g_outbound.empty() && S::g_inbound.empty());}
    }
    if(Select(argc,argv,"validation")){
        Reset(s);for(int i=0;i<10;++i){auto p=Packet();if(i==0)p.x=NAN;if(i==1)p.y=INFINITY;if(i==2)p.size=0;
            if(i==3)p.size=513;if(i==4)p.x=-513;if(i==5)p.y=1025;if(i==6)p.opac=-1;if(i==7)p.opac=5;
            if(i==8)p.col=2;if(i==9)p.col=NAN;S::OnReliable(p,1);}CHECK(S::g_inbound.empty());
        S::OnReliable(Packet(),0);S::OnReliable(Packet(),255);s.ready[31]=false;S::OnReliable(Packet(),31);CHECK(S::g_inbound.empty());
        s.mode=coop::net::Role::Client;S::OnReliable(Packet(),31);S::OnReliable(Packet(),0);CHECK(S::g_inbound.size()==2);
        S::Tick();Index();S::Tick();CHECK(draws==2); // relayed origin slots are valid on clients
        Reset(s);s.active=false;Receive();CHECK(S::g_inbound.empty());
        Reset(s);x=NAN;S::OnDrawPost(nullptr,&objects[0],nullptr);CHECK(S::g_outbound.empty());
    }
    if(Select(argc,argv,"queue-cap-expiry")){
        Reset(s);for(int i=0;i<300;++i){Observe(float(i));Receive(float(i));}
        CHECK(S::g_outbound.size()==256 && S::g_inbound.size()==256 && S::g_inbound.front().dab.x==0);
        const auto expired=S::Clock::now()-std::chrono::seconds(1);
        for(auto& row:S::g_outbound)row.deadline=expired;
        for(auto& row:S::g_inbound)row.deadline=expired;
        S::Tick();CHECK(S::g_outbound.empty() && S::g_inbound.empty() && s.sent.empty() && draws==0);
    }
    if(Select(argc,argv,"scan-serial")){
        Reset(s);CHECK(S::ResolveWindow()==&objects[0]);++objects[0].serial;CHECK(!S::ResolveWindow());Receive();S::Tick();CHECK(draws==0);
        Index();S::Tick();CHECK(draws==1);
        Reset(s);S::HubPassBegin(nullptr,true);S::HubMatch(nullptr,&objects[0]);++world;
        CHECK(S::HubPassComplete(nullptr,true,world)==0 && !S::ResolveWindow());
        Reset(s);S::OnDisconnect();S::Install(&s);Receive(3);S::Tick();CHECK(draws==1 && applied[0].x==3);
    }
    if(Select(argc,argv,"replay-ownership")){
        Reset(s);Receive(1);Receive(2);onDraw=[&]{S::Tick();Receive(3);};S::Tick();
        CHECK(draws==3 && applied[0].x==1 && applied[1].x==2 && applied[2].x==3 && S::g_wireApplyDepth==0);
        for(int cause=0;cause<5;++cause){Reset(s);Receive(1);Receive(2);onDraw=[&,cause]{
            if(cause==0)++objects[0].serial;if(cause==1)++world;if(cause==2)++serial;
            if(cause==3)signal=true;if(cause==4){S::OnDisconnect();S::Install(&s);Receive(9);}};
            S::Tick();CHECK(draws==1 && S::g_wireApplyDepth==0);if(cause==4)CHECK(S::g_inbound.front().dab.x==9);}
        Reset(s);Receive(1);Receive(2);draw=false;S::Tick();CHECK(draws==1 && S::g_inbound.size()==1);
        draw=true;S::Tick();CHECK(draws==2 && applied.back().x==2); // uncertain native failure is not replayed
    }
    if(Select(argc,argv,"signal-budget")){
        Reset(s);for(int i=0;i<20;++i)Receive(float(i));signal=true;S::Tick();CHECK(draws==0 && S::g_inbound.size()==20);
        signal=false;S::Tick();CHECK(draws==8 && S::g_inbound.size()==12);
        onSignal=[&]{++serial;};S::Tick();CHECK(draws==8);
    }
    if(Select(argc,argv,"preparation-retry")){
        Reset(s);Receive(1);Receive(2);prepared=false;S::Tick();CHECK(draws==0 && S::g_inbound.size()==2);
        prepared=true;S::Tick();CHECK(draws==2 && S::g_inbound.empty() && applied[0].x==1 && applied[1].x==2);
    }
    if(Select(argc,argv,"hook-recovery")){
        Reset(s);++objects[1].serial;S::Tick();CHECK(installs==2);
        Reset(s);s.linked=false;arm=false;S::Tick();CHECK(S::g_hookArmed);const auto prior=arms;S::Tick();CHECK(arms>prior);
        arm=true;S::Tick();CHECK(!S::g_hookArmed);s.linked=true;S::Tick();CHECK(S::g_hookArmed);
        Reset(s);S::g_hookInstalled=false;install=false;S::Tick();CHECK(!S::g_hookInstalled);
        install=true;S::g_resolveRetry=0;S::Tick();CHECK(S::g_hookInstalled);
        Reset(s);S::g_hookInstalled=false;onInstall=[&]{S::OnDisconnect();};S::Tick();CHECK(!S::g_hookInstalled);
    }
    if(Select(argc,argv,"observation-retirement")){
        Reset(s);onCorner=[&]{++objects[0].serial;};Observe();CHECK(S::g_outbound.empty());
        Reset(s);onCorner=[&]{S::OnDrawPost(nullptr,&objects[0],nullptr);};Observe();CHECK(S::g_outbound.size()==1);
        Reset(s);onCorner=[&]{++serial;};Observe();CHECK(S::g_outbound.empty());
        Reset(s);onRelease=[&]{S::Install(&s);Receive(12);};S::OnDisconnect();CHECK(S::g_inbound.size()==1 && S::g_session.load()==&s);
    }
    puts("PASS window stroke controller: source resets, reliable FIFO/backpressure, bounded queues/drains, native callback retirement, serial scan, replay ownership, relay origins and hook recovery");
}
