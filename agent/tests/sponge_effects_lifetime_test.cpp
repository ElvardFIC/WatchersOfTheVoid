#define TEST_CUSTOM_ACTION_REFS
#define main PriorSpongeMain
#include "sponge_effects_sync_test.cpp"
#undef main
#include <string>
const char* spongeCase="setup";
#undef CHECK
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %s line %d: %s\n",spongeCase,__LINE__,#x);std::exit(1);}}while(0)
namespace test {
Obj actorClass{3},alternateClass{4};bool differentClass=false;
}
namespace ue_wrap::reflection {
int32_t SlotSerial(int32_t id){
    return id==1?test::actor.serial:id==2?test::ui.serial:id==3?test::actorClass.serial:test::alternateClass.serial;
}
void* ClassOf(void* object){return object==&test::actor?(test::differentClass?&test::alternateClass:&test::actorClass):object;}
}
namespace test {
ue_wrap::script_gate::Call Call(uint64_t id=1){ue_wrap::script_gate::Call c{};c.object=&actor;c.function=&ui;c.stack=&ui;c.depth=1;c.invocation=id;return c;}
void Reset(coop::net::Session& s,Role role=Role::Host){
    differentClass=false;actor.live=ui.live=actorClass.live=alternateClass.live=true;
    key=L"sponge";now=1000;surfaces=bursts=0;Start(s,role);
}
bool Select(int argc,char** argv,const char* name){spongeCase=name;return argc<2 || std::string(argv[1])==name;}
}
int main(int argc,char** argv){using namespace test;coop::net::Session s;
    if(Select(argc,argv,"session-scope")){
        for(int cause=0;cause<5;++cause){Reset(s,Role::Client);auto c=Call();NativeUse(c);CHECK(S::g_events.size()==1);
            if(cause==0)++sessionSerial;if(cause==1)++world;if(cause==2)s.mode=Role::Host;if(cause==3)s.active=false;if(cause==4)s.linked=false;
            S::Tick();CHECK(S::g_events.empty() && s.sent.empty());}
        Reset(s);auto c=Call();NativeUse(c);coop::net::Session replacement;
        s.allowSend=[&](int,auto,const void*,int){S::Install(&replacement);NativeUse(c);return false;};S::Tick();
        CHECK(S::g_session==&replacement && S::g_events.size()==1);
    }
    if(Select(argc,argv,"invocation-post")){
        Reset(s);auto c=Call();S::Before(c);soap-=5;auto wrong=c;wrong.invocation=2;S::AfterUse(wrong);
        CHECK(S::g_frames.size()==1 && S::g_events.empty());S::AfterUse(c);CHECK(S::g_frames.empty() && S::g_events.size()==1);
        S::AfterUse(c);CHECK(S::g_events.size()==1);
        Reset(s);S::Before(c);inactiveInvocations.insert(1);S::Tick();CHECK(S::g_frames.empty());
        Reset(s);c.invocation=0;S::Before(c);CHECK(S::g_frames.empty());
    }
    if(Select(argc,argv,"reserved-pre")){
        Reset(s);std::function<void(int)> nested;
        nested=[&](int depth){auto c=Call(uint64_t(depth));c.depth=depth;
            callback=[&,depth]{CHECK(S::g_frames.size()==size_t(depth));if(depth<20)nested(depth+1);};S::Before(c);};
        nested(1);callback={};CHECK(S::g_frames.size()==16);
        Reset(s);auto c=Call();callback=[&]{S::OnDisconnect();S::Install(&s);S::Before(Call(2));};S::Before(c);
        CHECK(S::g_frames.size()==1 && S::g_frames[0].invocation==2);
    }
    if(Select(argc,argv,"frozen-address")){
        Reset(s,Role::Client);auto c=Call();S::Before(c);soap-=5;key=L"retagged";S::AfterUse(c);CHECK(S::g_events.empty());
        Reset(s,Role::Client);callback=[]{key=L"retagged";};S::Before(c);CHECK(S::g_frames.empty());
        Reset(s,Role::Client);callback=[]{coop::players::Registry::Get().local=&actor;};S::Before(c);CHECK(S::g_frames.empty());
        Reset(s,Role::Client);localHand=&actor;generation=0;NativeUse(c);generation=2;S::Tick();CHECK(S::g_events.empty() && s.sent.empty());
        Reset(s);soap=2;NativeUse(c);CHECK(soap==-3 && S::g_events.size()==1);
    }
    if(Select(argc,argv,"target-currency")){
        for(int cause=0;cause<3;++cause){Reset(s);S::Receive(Request(1),1);
            coop::element::callback=[cause]{if(cause==0)++actorClass.serial;if(cause==1)differentClass=true;if(cause==2)key=L"retagged";};
            S::Tick();CHECK(s.sent.empty() && surfaces==0 && bursts==0);}
        Reset(s,Role::Client);auto c=Call();NativeUse(c);differentClass=true;S::Tick();CHECK(s.sent.empty() && S::g_events.empty());
        Reset(s);c=Call();NativeUse(c);s.sendOk=false;S::Tick();key=L"retagged";s.sendOk=true;S::Tick();CHECK(S::g_events.empty() && s.sent.empty());
    }
    if(Select(argc,argv,"source-occupancy")){
        Reset(s);S::Receive(Request(1),1);callback=[] {S::OnPeerLeft(1);};S::Tick();
        CHECK(S::g_events.empty() && surfaces==0 && s.sent.empty());
        Reset(s);S::Receive(Request(1),1);callback=[&]{++s.generations[1];};S::Tick();CHECK(S::g_events.empty() && surfaces==0 && s.sent.empty());
        S::Receive(Request(1),1);S::Tick();CHECK(surfaces==1);
    }
    if(Select(argc,argv,"recipient-occupancy")){
        Reset(s);auto c=Call();NativeUse(c);s.allowSend=[&](int slot,auto,const void*,int){if(slot==2){++s.generations[2];return false;}return true;};
        S::Tick();for(const auto& row:s.sent)CHECK(row.slot!=2);CHECK(S::g_events.empty());
        Reset(s);NativeUse(c);s.allowSend=[](int slot,auto,const void*,int){return slot!=2;};S::Tick();CHECK(S::g_events.size()==1);
        S::OnPeerLeft(2);s.allowSend={};s.sent.clear();S::Tick();CHECK(s.sent.empty() && S::g_events.empty());
    }
    if(Select(argc,argv,"overflow-admission")){
        Reset(s,Role::Client);coop::players::Registry::Get().peer=2;
        for(uint32_t i=1;i<=128;++i){auto p=Request(i);p.op=1;S::Receive(p,0);}CHECK(S::g_events.size()==128);
        auto refused=Request(129);refused.op=1;S::Receive(refused,0);CHECK(S::g_lastHost==128);
        S::g_events.pop_front();S::Receive(refused,0);CHECK(S::g_lastHost==129 && S::g_events.size()==128);
    }
    if(Select(argc,argv,"budget-clock")){
        Reset(s);
        for(int slot=1;slot<=8;++slot)for(uint32_t i=1;i<=16;++i){auto p=Request(i);p.slot=uint8_t(slot);p.playerNo=uint16_t(10+slot);S::Receive(p,uint8_t(slot));}
        CHECK(S::g_events.size()==128);S::Tick();CHECK(surfaces==8 && bursts==8 && s.sent.size()==64);
        S::Tick();CHECK(surfaces==16 && bursts==16 && s.sent.size()<=128);
        std::array<uint32_t,32> last{};for(const auto& row:s.sent){const auto p=Decode(row);CHECK(p.sequence>last[row.slot]);last[row.slot]=p.sequence;}
        Reset(s);auto c=Call();NativeUse(c);now+=1500;S::Tick();CHECK(S::g_events.empty() && s.sent.empty());
        Reset(s);NativeUse(c);--now;S::Tick();CHECK(S::g_events.empty() && s.sent.empty());
        Reset(s);S::Receive(Request(1),1);callback=[]{now+=1500;};S::Tick();CHECK(surfaces==0 && bursts==0 && s.sent.empty() && S::g_events.empty());
        Reset(s);
        for(uint32_t i=1;i<=8;++i)S::Receive(Request(i,true),1);
        for(uint32_t i=9;i<=16;++i)S::Receive(Request(i),1);
        S::Tick();CHECK(surfaces==0);S::Tick();CHECK(surfaces==8 && bursts==8);
    }
    if(Select(argc,argv,"post-class")){
        Reset(s);auto c=Call();S::Before(c);soap-=5;++actorClass.serial;S::AfterUse(c);CHECK(S::g_events.empty() && S::g_frames.empty());
        Reset(s);S::Before(c);soap-=5;++ui.serial;S::AfterUse(c);CHECK(S::g_events.empty() && S::g_frames.empty());
    }
    puts("PASS sponge lifetime: exact invocations/reservation, frozen addresses, class/source/recipient currency, capacity admission, bounded work/FIFO/expiry and serial/world/role/link retirement");
}
