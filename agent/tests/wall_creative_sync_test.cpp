#include "support_2135.h"
#include "../../src/votv-coop/src/coop/world/wall_creative_sync.cpp"
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
namespace test {
namespace S=coop::wall_creative_sync;using Packet=coop::net::WallCreativePayload;using Role=coop::net::Role;
bool builder=true,found=true,keyReady=true,outline=true,permissionReady=true,allowed=true,owned=true;
uint32_t generation=1;void* hand=&ui;int builds=0,tracks=0;
int nativeRefusals=0;std::function<void()> finishCallback;
ue_wrap::wall_creative::Build result=ue_wrap::wall_creative::Build::Dispatched;
std::wstring key=L"mvpl_12345678_00000001";
std::array<coop::roster_ledger::Row,4> roster{};
}
namespace ue_wrap::reflection {int32_t FunctionFrameSize(void*){return 32;}}
namespace ue_wrap::wall_creative {
bool IsBuilderCall(void* tool,void* function){return test::builder && tool==&test::ui && function==&test::ui;}
bool Allowed(void*,bool& allowed){test::Callback();allowed=test::allowed;return test::permissionReady;}
Build Finish(void* wall,const std::function<bool()>& current){auto callback=std::move(test::finishCallback);test::finishCallback={};if(callback)callback();
    if(!test::S::NativeCurrent(wall))++test::nativeRefusals;if(!current() || !test::S::NativeCurrent(wall))return Build::Retry;
    if(test::result!=Build::Retry)++test::builds;return test::result;}
}
namespace ue_wrap::placed_save {Kind Type(void*){return test::outline?Kind::WallOutline:Kind::Wall;}}
namespace ue_wrap::wall_build {std::wstring Key(void*){return test::keyReady?test::key:std::wstring{};}}
namespace ue_wrap::keyed_objects {void* Resolve(const wchar_t* key){return test::found && key==test::key?&test::actor:nullptr;}}
namespace coop::placed_save_sync {void TrackWall(void*){++test::tracks;}bool OwnsClientWall(const std::wstring&,void*,uint8_t,uint16_t){return test::owned;}}
namespace coop::hand_item {
void* LocalHandActor(){return test::hand;}uint32_t Generation(uint8_t){return test::generation;}
bool HeldClassIs(uint8_t,const wchar_t* cls){CHECK(!wcscmp(cls,L"prop_wallbuilder_C"));return test::builder;}
}
namespace coop::roster_ledger {const Row& Get(int slot){return test::roster[slot];}}
namespace test {
void Start(coop::net::Session& session,Role role) {
    S::OnDisconnect();session.mode=role;session.linked=session.ready=session.sendOk=true;session.sent.clear();session.allowSend={};
    coop::players::Registry::Get().peer=role==Role::Host?0:1;
    for(int slot=0;slot<4;++slot)roster[slot].playerNo=uint16_t(10+slot);
    generation=1;hand=&ui;builder=found=keyReady=outline=permissionReady=allowed=owned=true;
    result=ue_wrap::wall_creative::Build::Dispatched;callback={};finishCallback={};coop::element::reach=true;coop::element::callback={};
    actor.live=ui.live=true;S::Install(&session);
}
Packet Decode(const coop::net::Session::Sent& sent){CHECK(sent.kind==coop::net::ReliableKind::WallCreative);Packet packet{};std::memcpy(&packet,sent.data.data(),sizeof(packet));return packet;}
Packet Request(uint32_t sequence=1){Packet p{};p.sequence=sequence;p.slot=1;p.playerNo=11;p.generation=1;coop::net::WireKeyFromString(key,p.key);return p;}
}
int main() {
    using namespace test;using V=ue_wrap::script_gate::Verdict;coop::net::Session session;
    CHECK(coop::net::LaneForKind(coop::net::ReliableKind::WallCreative)==coop::net::Lane::Normal);
    CHECK(!coop::net::IsClientRelayableReliableKind(coop::net::ReliableKind::WallCreative));
    Start(session,Role::Client);ue_wrap::script_gate::Call call{};uint8_t args[32]{};args[offsetof(Obj,flags)]=1;
    call.object=call.function=&actor;call.callerObject=call.callerFunction=&ui;call.locals=args;
    keyReady=false;generation=0;CHECK(S::BeforeBuild(call)==V::Cancel && S::g_client.size()==1);S::Tick();CHECK(session.sent.empty());
    keyReady=true;generation=1;session.sendOk=false;S::Tick();CHECK(session.sent.empty() && S::g_client.size()==1);
    session.sendOk=true;now+=500;S::Tick();CHECK(session.sent.size()==1);auto request=Decode(session.sent.back());
    CHECK(request.generation==1 && request.playerNo==11 && request.op==0);S::BeforeBuild(call);CHECK(S::g_client.size()==1);
    auto reply=request;reply.op=1;S::Receive(reply,2);CHECK(S::g_client.size()==1);S::Receive(reply,0);CHECK(S::g_client.empty());
    args[offsetof(Obj,flags)]=0;S::BeforeBuild(call);CHECK(S::g_client.empty());args[offsetof(Obj,flags)]=1;
    builder=false;S::BeforeBuild(call);CHECK(S::g_client.empty());builder=true;S::BeforeBuild(call);hand=nullptr;S::Tick();CHECK(S::g_client.empty());
    Start(session,Role::Host);found=false;request=Request();S::Receive(request,1);S::Receive(request,1);S::Tick();auto count=builds;
    CHECK(S::g_host[1].size()==1 && builds==count && session.sent.empty());found=true;permissionReady=false;now+=200;S::Tick();CHECK(session.sent.empty());
    permissionReady=true;allowed=false;now+=200;S::Tick();CHECK(Decode(session.sent.back()).op==2 && builds==count);
    S::Receive(request,1);S::Tick();CHECK(Decode(session.sent.back()).op==2 && builds==count);
    Start(session,Role::Host);request=Request();generation=0;S::Receive(request,1);S::Tick();CHECK(session.sent.empty());
    generation=1;now+=200;session.sendOk=false;S::Tick();CHECK(builds==count+1 && S::g_host[1].at(1).pending);
    session.sendOk=true;S::Tick();CHECK(Decode(session.sent.back()).op==1 && builds==count+1);
    S::Receive(request,1);S::Tick();CHECK(builds==count+1);request.sequence++;S::Receive(request,1);S::Tick();CHECK(Decode(session.sent.back()).op==2 && builds==count+1);
    Start(session,Role::Host);request=Request();coop::element::reach=false;S::Receive(request,1);S::Tick();CHECK(Decode(session.sent.back()).op==2 && builds==count+1);
    Start(session,Role::Host);request=Request();session.ready=false;S::Receive(request,1);CHECK(S::g_host[1].empty());session.ready=true;
    auto invalid=request;invalid.playerNo++;S::Receive(invalid,1);CHECK(S::g_host[1].empty());invalid=request;invalid.key.len=32;S::Receive(invalid,1);CHECK(S::g_host[1].empty());
    Start(session,Role::Host);request=Request();found=false;S::Receive(request,1);now+=10001;found=true;S::Tick();CHECK(Decode(session.sent.back()).op==2 && builds==count+1);
    Start(session,Role::Host);request=Request();S::Receive(request,1);roster[1].playerNo++;S::Tick();CHECK(S::g_host[1].empty() && builds==count+1);
    Start(session,Role::Host);request=Request();S::Receive(request,1);callback=[]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session && builds==count+1);
    Start(session,Role::Host);request=Request();S::Receive(request,1);coop::element::callback=[]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session && builds==count+1);
    Start(session,Role::Host);request=Request();result=ue_wrap::wall_creative::Build::Failed;S::Receive(request,1);S::Tick();CHECK(Decode(session.sent.back()).op==3 && builds==count+2);
    S::Receive(request,1);S::Tick();CHECK(builds==count+2);request.sequence++;S::Receive(request,1);S::Tick();CHECK(builds==count+2);
    Start(session,Role::Host);request=Request();found=false;S::Receive(request,1);++world;S::Tick();CHECK(S::g_host[1].empty() && S::g_claimed.empty());
    Start(session,Role::Host);request=Request();owned=false;S::Receive(request,1);S::Tick();CHECK(session.sent.empty());
    owned=true;now+=200;S::Tick();CHECK(Decode(session.sent.back()).op==1);
    Start(session,Role::Client);S::BeforeBuild(call);S::Tick();auto immutable=Decode(session.sent.back());
    hand=nullptr;generation++;now+=500;S::Tick();CHECK(S::g_client.size()==1 && Decode(session.sent.back()).generation==immutable.generation);
    immutable.op=1;S::Receive(immutable,0);CHECK(S::g_client.empty());
    Start(session,Role::Host);request=Request();found=false;S::Receive(request,1);generation++;builder=false;S::Tick();count=builds;
    found=true;now+=200;S::Tick();CHECK(builds==count+1 && Decode(session.sent.back()).op==1); // validated source survives later stow
    Start(session,Role::Host);request=Request();S::Receive(request,1);count=builds;auto refused=nativeRefusals;
    finishCallback=[]{S::OnDisconnect();};S::Tick();CHECK(builds==count && nativeRefusals==refused+1 && !S::g_session);
    Start(session,Role::Host);count=builds;
    for(uint32_t sequence=1;sequence<=16;++sequence){auto waiting=Request(sequence);coop::net::WireKeyFromString(L"mvpl_waiting_contour",waiting.key);S::Receive(waiting,1);}
    request=Request();request.slot=2;request.playerNo=12;S::Receive(request,2);
    for(int tick=0;tick<5;++tick){now+=200;S::Tick();}
    CHECK(builds==count+1 && S::g_host[2].at(request.sequence).reply==1);
    puts("PASS creative walls: exact builder capture, key/hand wait, host custom-content/reach checks, request/reply retry, at-most-once native dispatch, late outline, refusal/fault/expiry, roster/world/reentrant fences");
}
