#ifndef ARIRAL_SHIP_SOURCE
#define ARIRAL_SHIP_SOURCE "../../src/votv-coop/src/coop/world/ariral_ship_sync.cpp"
#endif
#include ARIRAL_SHIP_SOURCE
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace S=coop::ariral_ship_sync;
namespace test {
struct Obj {int id=0,serial=1;bool live=true;ue_wrap::ariral_ship::Appearance appearance;int off=12,frame=32;} objects[100];
uint32_t world=1,serial=1;
bool watches=true,readOK=true,paintOK=true,bindOK=true,activityOK=true,airOK=true;
int paints=0,binds=0,activities=0,airs=0,deadReads=0;
std::vector<int> scene{0};
std::function<void()> duringRead,duringPaint,duringBind,duringActivity,duringAir;
coop::net::Session session;
void Callback(std::function<void()>& source){auto fn=std::move(source);source={};if(fn)fn();}
ue_wrap::script_gate::Call Call(int object=0) {ue_wrap::script_gate::Call c{};c.object=&objects[object];c.function=&objects[99];return c;}
bool Graph(int entry,int actor=0) {
    uint8_t bytes[64]{};std::memcpy(bytes+objects[99].off,&entry,sizeof(entry));
    auto c=Call(actor);c.locals=bytes;return S::Graph(c)==ue_wrap::script_gate::Verdict::Run;
}
void Reset(coop::net::Role role=coop::net::Role::Client) {
    S::OnDisconnect();world=serial=1;session={};session.mode=role;scene={0};
    watches=readOK=paintOK=bindOK=activityOK=airOK=true;paints=binds=activities=airs=deadReads=0;
    duringRead=duringPaint=duringBind=duringActivity=duringAir={};
    for(int i=0;i<100;++i) {objects[i]={};objects[i].id=i;objects[i].appearance.name=L"arirShipAppear_C_"+std::to_wstring(i);objects[i].appearance.at={100,200,300};}
    S::Install(&session);
}
coop::net::AriralShipStatePayload Packet(uint32_t revision=1,uint32_t cue=1,uint8_t flags=1) {
    coop::net::AriralShipStatePayload p{};S::Name(objects[0].appearance.name,p.name);
    p.generation=1;p.revision=revision;p.cue=cue;p.flags=flags;p.x=100;p.y=200;p.z=300;p.alpha=.5f;return p;
}
void Peer(int slot=1) {session.peers[slot]=session.ready[slot]=true;++session.generations[slot];}
void Pump(){S::g_sample={};S::Tick();}
}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* p){return static_cast<test::Obj*>(p)->id<90?CurrentWorld():nullptr;}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
bool IsLiveByIndex(void* p,int32_t id){return id>=0&&id<100&&p==&test::objects[id]&&test::objects[id].live;}
int32_t SlotSerial(int32_t id){return test::objects[id].serial;}
int32_t AllocateSlotSerial(int32_t id){return SlotSerial(id);}
int32_t FindParamOffset(void* p,const wchar_t*){return static_cast<test::Obj*>(p)->off;}
int32_t FunctionFrameSize(void* p){return static_cast<test::Obj*>(p)->frame;}
}
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn){return true;}
bool ClassNameWatchLive(const wchar_t*,const wchar_t*,int){return test::watches;}
void ResolvePendingNames(){}
}
namespace ue_wrap::ariral_ship {
std::vector<void*> Actors(){std::vector<void*> out;for(int i:test::scene)if(test::objects[i].live)out.push_back(&test::objects[i]);return out;}
bool Read(void* p,Appearance& out){if(!static_cast<test::Obj*>(p)->live){++test::deadReads;return false;}test::Callback(test::duringRead);if(!test::readOK)return false;out=static_cast<test::Obj*>(p)->appearance;return true;}
bool Paint(void* p,const Appearance& state,const std::function<bool()>& current) {
    ++test::paints;if(!test::paintOK||!current())return false;
    auto& out=static_cast<test::Obj*>(p)->appearance;out.alpha=state.alpha;out.visible=state.visible;
    test::Callback(test::duringPaint);return current();
}
bool PhotoSubscription(void* p,bool,const std::function<bool()>& current) {
    ++test::binds;auto c=test::Call(static_cast<test::Obj*>(p)->id);c.fromOurCode=true;
    CHECK(S::Trigger(c)==script_gate::Verdict::Run && test::Graph(1398,static_cast<test::Obj*>(p)->id));
    CHECK(S::BlockClient(c)==script_gate::Verdict::Cancel && !test::Graph(908));
    auto other=test::Call(static_cast<test::Obj*>(p)->id==1?2:1);CHECK(S::Trigger(other)==script_gate::Verdict::Cancel);
    test::Callback(test::duringBind);return test::bindOK&&current();
}
bool Activity(void*,bool,const std::function<bool()>& current){++test::activities;test::Callback(test::duringActivity);return test::activityOK&&current();}
bool Air(void*,const std::function<bool()>& current){++test::airs;test::Callback(test::duringAir);return test::airOK&&current();}
}
int main() {
    using namespace test;using V=ue_wrap::script_gate::Verdict;
    Reset();auto c=Call();c.fromOurCode=true;
    CHECK(S::Trigger(c)==V::Cancel&&S::Photo(c)==V::Cancel&&S::BlockClient(c)==V::Cancel);
    for(int entry:{15,31,383,604,908,1546,2585,2631,3075,1398,2686})CHECK(!Graph(entry));
    CHECK(Graph(1197));objects[99].off=60;objects[99].frame=32;CHECK(!Graph(1197));objects[99].off=12;
    Reset();auto p=Packet();S::Receive(p,1);CHECK(S::g_rows.empty());
    auto bad=p;bad.alpha=std::numeric_limits<float>::quiet_NaN();S::Receive(bad,0);
    bad=p;bad.reserved[1]=1;S::Receive(bad,0);bad=p;bad.name.len=64;S::Receive(bad,0);
    bad=p;bad.name.data[62]='x';S::Receive(bad,0);bad=p;bad.flags=8;S::Receive(bad,0);
    bad=p;bad.generation=0;S::Receive(bad,0);bad=p;bad.revision=0;S::Receive(bad,0);
    bad=p;bad.x=2000001;S::Receive(bad,0);bad=p;bad.flags=4;S::Receive(bad,0);
    CHECK(S::g_rows.empty());
    S::Receive(p,0);watches=false;Pump();CHECK(!paints);watches=true;Pump();
    CHECK(objects[0].appearance.visible&&objects[0].appearance.alpha==.5f&&paints==1&&binds==1&&activities==1&&airs==0);
    CHECK(S::Photo(c)==V::Run&&Graph(2686)&&!Graph(1398));Pump();CHECK(paints==1);
    S::Receive(p,0);bad=Packet(0);S::Receive(bad,0);CHECK(S::g_rows.begin()->second.state.revision==1);
    p=Packet(2,2);S::Receive(p,0);Pump();CHECK(airs==1&&paints==2);Pump();CHECK(airs==1);
    p=Packet(3,3,0);p.alpha=0;S::Receive(p,0);Pump();CHECK(airs==2&&!objects[0].appearance.visible&&S::Photo(c)==V::Cancel);
    p=Packet(4,3,4);p.alpha=0;S::Receive(p,0);Pump();bad=Packet(3,4);S::Receive(bad,0);Pump();CHECK(!objects[0].appearance.visible);
    bad=Packet(99,4);bad.generation=0;S::Receive(bad,0);CHECK(S::g_rows.begin()->second.state.revision==4);
    p=Packet(1,20);p.generation=2;S::Receive(p,0);Pump();CHECK(objects[0].appearance.visible&&airs==2);
    bad=Packet(100,22);S::Receive(bad,0);CHECK(S::g_rows.begin()->second.state.generation==2);
    puts("PASS host-only validated state, ordered incarnations/tombstones, local photo admission and quiet seeds");

    for(int stage=0;stage<3;++stage) {
        Reset();p=Packet();S::Receive(p,0);
        if(stage==0)paintOK=false;if(stage==1)bindOK=false;if(stage==2)activityOK=false;
        Pump();CHECK(S::g_rows.begin()->second.applied==0);
        paintOK=bindOK=activityOK=true;Pump();CHECK(S::g_rows.begin()->second.applied==1);
    }
    Reset();p=Packet();S::Receive(p,0);Pump();airOK=false;p=Packet(2,2);S::Receive(p,0);Pump();CHECK(airs==1);
    p=Packet(3,2);p.alpha=.75f;S::Receive(p,0);airOK=true;Pump();CHECK(airs==2);Pump();CHECK(airs==2);
    p=Packet(4,3);S::Receive(p,0);S::g_rows.begin()->second.cueUntil=S::Clock::now()-std::chrono::seconds(1);Pump();CHECK(airs==2);
    p=Packet(5,4,3);S::Receive(p,0);Pump();CHECK(airs==2); // quiet resync suppresses old cue
    Reset();S::Receive(Packet(),0);duringPaint=[]{S::Receive(Packet(2,2),0);};Pump();
    CHECK(binds==0&&S::g_rows.begin()->second.applied==0);Pump();CHECK(S::g_rows.begin()->second.applied==2&&airs==1);
    Reset();S::Receive(Packet(),0);Pump();S::Receive(Packet(2,2),0);duringAir=[]{auto next=Packet(3,2);next.alpha=.8f;S::Receive(next,0);};Pump();Pump();CHECK(airs==1);
    Reset();readOK=false;S::Receive(Packet(),0);Pump();CHECK(!paints);readOK=true;Pump();CHECK(paints==1);
    Reset();S::Receive(Packet(),0);duringRead=[]{auto next=Packet();next.generation=2;next.x=2000;S::Receive(next,0);};Pump();
    CHECK(!S::g_rows.begin()->second.actor.Raw()&&S::g_rows.begin()->second.applied==0);Pump();CHECK(S::g_rows.begin()->second.applied==0);
    Reset();objects[0].appearance.name=L"different";scene={0,1};S::Receive(Packet(),0); // host name now different
    S::g_rows.begin()->second.state.name={};S::Name(L"host_unique",S::g_rows.begin()->second.state.name);
    Pump();CHECK(S::g_rows.begin()->second.applied==0);scene={0};Pump();CHECK(S::g_rows.begin()->second.applied==1);
    Reset();S::Receive(Packet(),0);objects[0].appearance.at.X=1000;Pump();CHECK(S::g_rows.begin()->second.applied==0);
    Reset();objects[0].appearance.visible=true;objects[0].appearance.alpha=1;Pump();CHECK(!objects[0].appearance.visible&&objects[0].appearance.alpha==0&&binds==1);
    Pump();CHECK(binds==1);++objects[0].serial;Pump();CHECK(binds==2);
    puts("PASS readiness/apply/cue retries, deadlines, reentrant newer state, unique scene matching and unowned actor quieting");

    for(int stage=0;stage<5;++stage)for(int change=0;change<4;++change) {
        Reset();S::Receive(Packet(),0);
        auto callback=[change]{if(change==0)++world;if(change==1)++serial;if(change==2)++objects[0].serial;if(change==3)S::OnDisconnect();};
        if(stage==0)duringRead=callback;if(stage==1)duringPaint=callback;if(stage==2)duringBind=callback;
        if(stage==3)duringActivity=callback;
        if(stage==4){Pump();S::Receive(Packet(2,2),0);duringAir=callback;}
        Pump();CHECK(!S::g_binding.actor.Raw());
        if(stage<3)CHECK(activities==0);
        if(change!=3) {S::Context();for(auto& [key,row]:S::g_rows)CHECK(row.applied!=row.state.revision || change==2);}
    }
    Reset(coop::net::Role::Host);Pump();CHECK(session.sent.empty()&&S::g_rows.size()==1);Peer();Pump();
    CHECK(session.sent.size()==1&&(session.sent.back().state.flags&2));Pump();CHECK(session.sent.size()==1);
    objects[0].appearance.visible=true;objects[0].appearance.alpha=.4f;S::Appeared(Call());Pump();
    CHECK(session.sent.size()==2&&session.sent.back().state.cue==1&&!(session.sent.back().state.flags&2));
    session.sendOK=false;objects[0].appearance.alpha=.6f;Pump();CHECK(session.sent.size()==2);
    objects[0].appearance.alpha=.8f;Pump();session.sendOK=true;Pump();CHECK(session.sent.size()==3&&session.sent.back().state.alpha==.8f);
    Peer(2);Pump();CHECK(session.sent.back().slot==2&&(session.sent.back().state.flags&2));
    ++session.generations[1];Pump();CHECK(session.sent.back().slot==1&&(session.sent.back().state.flags&2));
    session.duringSend=[]{++session.generations[1];};objects[0].appearance.alpha=.9f;Pump();
    CHECK(S::g_rows.begin()->second.delivered[1].peer!=session.generations[1]);Pump();CHECK(session.sent.back().state.flags&2);
    S::QueueConnectBroadcastForSlot(1);Pump();CHECK(session.sent.back().state.flags&2);
    S::QueueConnectBroadcastForSlot(1);session.duringSend=[]{S::QueueConnectBroadcastForSlot(1);};Pump();
    CHECK(S::g_rows.begin()->second.delivered[1].revision==0);Pump();CHECK(S::g_rows.begin()->second.delivered[1].revision!=0);
    objects[0].live=false;Pump();CHECK(session.sent.back().state.flags&4);
    const auto gen=session.sent.back().state.generation;objects[0].live=true;++objects[0].serial;Pump();CHECK(session.sent.back().state.generation!=gen);
    Reset(coop::net::Role::Host);Peer();session.duringSend=[]{S::OnDisconnect();};Pump();CHECK(S::g_rows.empty());
    Reset(coop::net::Role::Host);Peer();session.duringSend=[]{++world;};Pump();S::Context();CHECK(S::g_rows.empty());
    Reset(coop::net::Role::Host);Peer();session.duringSend=[]{objects[0].appearance.alpha=.9f;S::Appeared(Call());};Pump();
    CHECK(S::g_rows.begin()->second.delivered[1].revision==0);Pump();CHECK(session.sent.back().state.alpha==.9f);
    Reset(coop::net::Role::Host);Peer();scene.clear();for(int i=0;i<32;++i)scene.push_back(i);
    Pump();CHECK(session.sent.size()==16);Pump();CHECK(session.sent.size()==32);Pump();CHECK(session.sent.size()==32);
    Reset(coop::net::Role::Host);scene={0,1};duringRead=[]{objects[1].live=false;};Pump();CHECK(deadReads==0&&S::g_rows.size()==1);
    Reset(coop::net::Role::Host);scene={0,1};duringRead=[]{++objects[1].serial;};Pump();CHECK(S::g_rows.size()==1);
    Pump();CHECK(S::g_rows.size()==2);
    puts("PASS host phases, host-alone late joins, retained latest sends, recipient replacement, retirement and teardown");
}
