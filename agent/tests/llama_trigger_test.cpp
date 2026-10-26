#include "../../src/votv-coop/src/coop/props/llama_trigger.cpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <unordered_set>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL controller %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
namespace T=coop::llama_trigger;namespace M=ue_wrap::llama_trigger;namespace P=coop::players;namespace R=ue_wrap::reflection;
struct Obj {int id=0,serial=1;bool live=true;void* cls=nullptr;} objects[600];
Obj& O(void* p){CHECK(p);return *static_cast<Obj*>(p);}void* At(int i){return &objects[i];}
uint64_t now=1000,timer=123;uint32_t generation=1,serial=1;uintptr_t world=100;uint16_t people[32]{};
coop::net::Session session;coop::RemotePlayer players[32];bool hand=false,source=true,captureOk=true,overlapOk=true,admit=true,complete=true,clear=true;
bool kerfur=false,coastAccept=true;std::unordered_set<void*> driven;
int captures=0,launches=0,coasts=0,selected=-1;ue_wrap::FVector aim{};std::vector<int> blocked,traceOrder;std::vector<void*> bodies;
std::function<void()> onCapture,onTrace,onLaunch,onCoast;
void Fire(std::function<void()>& action){auto f=std::move(action);action={};if(f)f();}
void Add(int slot,float x=100){players[slot]={};players[slot].actor=At(10+slot);players[slot].body={x,0,0};players[slot].eye={x,0,100};
    P::Registry::Get().peers[slot]=&players[slot];P::Registry::Get().lives[slot]=P::PuppetLife::Alive;bodies.push_back(At(10+slot));}
void Reset(){T::Reset();++generation;++serial;++world;now=1000;timer=123;
    for(int i=0;i<600;++i){objects[i]={i,int(generation*1000)+i,true,At(500)};}session={};session.perSlot=true;
    session.links.fill(true);session.lanes.fill(true);session.worlds.fill(true);session.generations.fill(1);
    for(int i=0;i<32;++i)people[i]=uint16_t(100+i);P::Registry::Get()={};for(auto& player:players)player={};
    source=captureOk=overlapOk=admit=complete=clear=coastAccept=true;hand=kerfur=false;driven.clear();captures=launches=coasts=0;selected=-1;
    onCapture=onTrace=onLaunch=onCoast={};bodies.clear();blocked.clear();traceOrder.clear();Add(1);}
bool Run(){return T::TryRemote(session,At(1),[]{return source;});}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&test::O(p).live;}void* ClassOf(void* p){return p?test::O(p).cls:nullptr;}
int32_t InternalIndexOf(void* p){return p?test::O(p).id:-1;}int32_t SlotSerial(int32_t i){return i>=0&&i<600?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}bool IsLiveByIndex(void* p,int32_t i){return p&&i>=0&&i<600&&p==test::At(i)&&IsLive(p);}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::generation;}void* CurrentWorld(){return reinterpret_cast<void*>(test::world);}void* WorldOf(void*){return CurrentWorld();}}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace coop::roster_ledger {const Row& Get(int slot){static Row rows[32];rows[slot].playerNo=test::people[slot];return rows[slot];}}
namespace coop::prop_drive_host {bool InAHand(void*){return test::hand;}bool IsDriven(void* actor){return test::driven.contains(actor);}
void Coast(void* actor,const char*,bool){++test::coasts;if(test::coastAccept)test::driven.insert(actor);test::Fire(test::onCoast);}}
namespace coop::npc_sync {bool KerfurIsHolding(void*){return test::kerfur;}}
namespace ue_wrap::llama_trigger {
bool ReadTimer(void*,uint64_t& out){out=test::timer;return true;}
bool Capture(void* actor,Probe& out,const std::function<bool()>& current){++test::captures;test::Fire(test::onCapture);if(!test::captureOk||!test::timer||!current())return false;
    Probe p;p.actor.Set(actor);p.cls.Set(reflection::ClassOf(actor));p.timer=test::timer;p.generation=test::generation;p.world=world_identity::CurrentWorld();p.radius=200;out=p;return true;}
bool Overlaps(const Probe&,std::vector<CachedObjRef>& out,const std::function<bool()>& current){if(!test::overlapOk||!current())return false;
    for(void* p:test::bodies){CachedObjRef ref;ref.Set(p);out.push_back(ref);}return current();}
bool ClearLine(const Probe&,const FVector& eye,const std::function<bool()>& current){test::traceOrder.push_back(int(eye.X));test::Fire(test::onTrace);
    return test::clear&&std::find(test::blocked.begin(),test::blocked.end(),int(eye.X))==test::blocked.end()&&current();}
bool Launch(const Probe&,const FVector& eye,bool& admitted,const std::function<bool()>& current,const std::function<bool()>& sourceCurrent){
    ++test::launches;test::aim=eye;test::selected=int(eye.X);admitted=test::admit&&current();test::Fire(test::onLaunch);
    if(admitted)test::timer=0;return admitted&&test::complete&&(!sourceCurrent||sourceCurrent());}
}
namespace test {
void Group(int id){Reset();switch(id){
    case 1:session.mode=coop::net::Role::Client;CHECK(!Run()&&!captures);break;
    case 2:for(int cause=0;cause<2;++cause){Reset();if(cause)session.linked=false;else session.active=false;CHECK(!Run()&&!captures);}break;
    case 3:session.worlds[1]=false;CHECK(!Run()&&!launches);break;
    case 4:session.lanes[1]=false;CHECK(!Run()&&!launches);break;
    case 5:P::Registry::Get().peers[1]=nullptr;CHECK(!Run()&&!launches);break;
    case 6:P::Registry::Get().lives[1]=P::PuppetLife::Dead;CHECK(!Run()&&!launches);break;
    case 7:players[1].posed=false;CHECK(!Run()&&!launches);break;
    case 8:players[1].ragdoll=true;CHECK(!Run()&&!launches);break;
    case 9:bodies.clear();CHECK(!Run()&&!launches);break;
    case 10:P::Registry::Get().peers[1]=nullptr;Add(0);CHECK(!Run()&&!launches);P::Registry::Get().local=At(10);
        CHECK(T::AllowLocal(session,At(1),[]{return source;}));bodies={At(11)};CHECK(!T::AllowLocal(session,At(1),[]{return source;}));break;
    case 11:bodies={At(80)};CHECK(!Run()&&!launches);break;
    case 12:players[1].body.X=std::numeric_limits<float>::infinity();CHECK(!Run()&&!launches);break;
    case 13:players[1].eye.Y=std::numeric_limits<float>::quiet_NaN();CHECK(!Run()&&!launches);break;
    case 14:players[1].eye.Z=601;CHECK(!Run()&&!launches);break;
    case 15:Add(2,50);CHECK(Run()&&selected==50);break;
    case 16:Add(2,100);CHECK(Run()&&traceOrder.size()==1&&players[1].eye.X==aim.X);break;
    case 17:blocked={100};CHECK(!Run()&&!launches);break;
    case 18:clear=false;CHECK(!Run()&&!launches);break;
    case 19:Add(2,50);blocked={50};CHECK(Run()&&selected==100&&traceOrder==std::vector<int>({50,100}));break;
    case 20:onTrace=[]{++people[1];};CHECK(!Run()&&!launches);Reset();people[1]=0;CHECK(!Run()&&!launches);break;
    case 21:onTrace=[]{++session.generations[1];};CHECK(!Run()&&!launches);Reset();session.generations[1]=0;CHECK(!Run()&&!launches);break;
    case 22:onTrace=[]{++objects[11].serial;};CHECK(!Run()&&!launches);break;
    case 23:onTrace=[]{++objects[500].serial;};CHECK(!Run()&&!launches);break;
    case 24:for(int cause=0;cause<4;++cause){Reset();onTrace=[cause]{if(cause==0)players[1].eye.X+=2;if(cause==1)P::Registry::Get().peers[1]=nullptr;
        if(cause==2)players[1].ragdoll=true;if(cause==3)session.links[1]=false;};CHECK(!Run()&&!launches);}break;
    case 25:hand=true;CHECK(!Run()&&!captures);break;
    case 49:for(int slot=2;slot<32;++slot)Add(slot,float(200-slot));CHECK(Run()&&launches==1&&traceOrder.size()==1);break;
    case 67:CHECK(Run()&&coasts==1);break;
    case 69:complete=false;CHECK(Run()&&launches==1);now+=500;timer=123;CHECK(!Run()&&launches==1&&T::BlocksNative(At(1)));break;
    case 70:captureOk=false;CHECK(!Run()&&!launches);now+=500;captureOk=true;CHECK(Run()&&launches==1);break;
    case 71:CHECK(Run());now+=500;timer=456;CHECK(!T::BlocksNative(At(1)));CHECK(Run()&&launches==2);break;
    case 72:onCapture=[]{CHECK(!Run());CHECK(T::BlocksNative(At(1)));};onLaunch=[]{CHECK(!Run());};CHECK(Run()&&captures==1&&launches==1);
        Reset();CHECK(!T::TryRemote(session,At(1),[]{CHECK(!Run());return false;}));break;
    case 73:CHECK(Run()&&T::BlocksNative(At(1)));CHECK(!T::BlocksNative(At(2)));++objects[1].serial;CHECK(!T::BlocksNative(At(1)));break;
    case 74:CHECK(Run());for(int cause=0;cause<3;++cause){Reset();CHECK(Run());if(cause==0)++serial;if(cause==1)++world;if(cause==2)++generation;timer=123;CHECK(Run()&&launches==2);}
        for(int cause=0;cause<4;++cause){Reset();onCapture=[cause]{if(cause==0)T::Reset();if(cause==1)++world;if(cause==2)++generation;if(cause==3)++serial;};CHECK(!Run()&&!launches);}
        Reset();P::Registry::Get().peers[1]=nullptr;for(int i=1;i<=257;++i){T::TryRemote(session,At(i),[]{return true;});}CHECK(T::g_rows.size()==256);
        objects[1].live=false;CHECK(!T::TryRemote(session,At(258),[]{return true;})&&T::g_rows.size()==256);T::Reset();CHECK(T::g_rows.empty());break;
    case 75:clear=false;CHECK(!Run()&&captures==1);now+=499;CHECK(!Run()&&captures==1);++now;CHECK(!Run()&&captures==2);now=0;CHECK(!Run()&&captures==3);break;
    default:CHECK(false);}
}
void Recovery(int id){Reset();const auto queue=[] {T::QueueNativeMotion(session,At(1),[]{return source;});};switch(id){
    case 59:CHECK(Run()&&coasts==1&&driven.contains(At(1))&&T::g_motion.empty());break;
    case 60:queue();CHECK(coasts==1&&T::g_motion.empty());break;
    case 61:coastAccept=false;queue();CHECK(coasts==1&&T::g_motion.size()==1&&!driven.contains(At(1)));break;
    case 62:coastAccept=false;queue();now+=100;coastAccept=true;T::TickMotion(session);CHECK(coasts==2&&T::g_motion.empty()&&driven.contains(At(1)));break;
    case 63:coastAccept=false;queue();now+=99;T::TickMotion(session);CHECK(coasts==1);++now;T::TickMotion(session);CHECK(coasts==2);break;
    case 64:coastAccept=false;for(int i=1;i<=257;++i)T::QueueNativeMotion(session,At(i),[]{return true;});CHECK(T::g_motion.size()==256&&coasts==256);break;
    case 65:for(int cause=0;cause<3;++cause){Reset();coastAccept=false;queue();if(cause==0)source=false;if(cause==1)++objects[500].serial;if(cause==2)objects[1].cls=At(501);
        now+=100;T::TickMotion(session);CHECK(coasts==1&&T::g_motion.empty());}break;
    case 66:for(int cause=0;cause<4;++cause){Reset();coastAccept=false;queue();if(cause==0)++serial;if(cause==1)++world;if(cause==2)++generation;if(cause==3)session.active=false;
        now+=100;T::TickMotion(session);CHECK(coasts==1&&T::g_motion.empty());}break;
    case 67:coastAccept=false;queue();hand=true;now+=100;T::TickMotion(session);CHECK(coasts==1&&T::g_motion.empty());break;
    case 68:kerfur=true;CHECK(!Run()&&!captures);kerfur=false;coastAccept=false;queue();kerfur=true;now+=100;T::TickMotion(session);CHECK(coasts==1&&T::g_motion.empty());break;
    case 69:coastAccept=false;queue();T::Reset();now+=100;T::TickMotion(session);CHECK(coasts==1&&T::g_motion.empty());break;
    case 70:coastAccept=false;queue();now+=10000;T::TickMotion(session);CHECK(coasts==1&&T::g_motion.empty());Reset();coastAccept=false;queue();now+=100;T::TickMotion(session);
        now=0;T::TickMotion(session);CHECK(coasts==3&&T::g_motion.size()==1);now=10000;T::TickMotion(session);CHECK(coasts==3&&T::g_motion.empty());break;
    case 71:captureOk=false;for(int i=1;i<=257;++i)T::TryRemote(session,At(i),[]{return true;});CHECK(T::g_rows.empty());captureOk=true;CHECK(Run()&&T::g_rows.size()==1);break;
    case 72:CHECK(Run());timer=456;P::Registry::Get().peers[1]=nullptr;for(int i=2;i<=257;++i)T::TryRemote(session,At(i),[]{return true;});
        CHECK(T::g_rows.size()==256&&T::g_rows.at(At(1)).spent==123);break;
    case 73:P::Registry::Get().peers[1]=nullptr;for(int i=1;i<=257;++i)T::TryRemote(session,At(i),[]{return true;});CHECK(T::g_rows.size()==256&&T::g_rows.contains(At(257)));
        Add(1);now+=500;CHECK(T::TryRemote(session,At(257),[]{return true;})&&coasts==1);break;
    case 74:onCoast=[]{T::TickMotion(session);T::QueueNativeMotion(session,At(1),[]{return source;});CHECK(coasts==1);};queue();CHECK(coasts==1&&T::g_motion.size()==1);
        now+=100;T::TickMotion(session);CHECK(coasts==2&&T::g_motion.empty());break;
    case 75:{bool old=true;onCoast=[&]{old=false;coastAccept=false;T::QueueNativeMotion(session,At(1),[]{return source;});};
        T::QueueNativeMotion(session,At(1),[&]{return old;});CHECK(coasts==1&&T::g_motion.size()==1);now+=100;coastAccept=true;T::TickMotion(session);CHECK(coasts==2&&T::g_motion.empty());}break;
    default:CHECK(false);}
}
}
int main(){for(int id=1;id<=75;++id){if(id>25&&id!=49&&id!=67&&id<69)continue;test::Group(id);std::printf("PASS llama trigger %d\n",id);}
    for(int id=59;id<=75;++id){test::Recovery(id);std::printf("PASS llama recovery %d\n",id);}return 0;}
