#include "../../src/votv-coop/src/coop/player/blood_sync.cpp"
#include "../../src/votv-coop/src/ue_wrap/actors/player_blood.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int index,serial=1;bool live=true;std::wstring cls;void* world=nullptr;};
Obj objects[]={{0,1,true,L"bloodDecalMaster_C"},{1,1,true,L"effect_bloodLoss_C"},{2,1,true,L"Function"}};
struct Params {ue_wrap::FVector location{10,20,30},velocity{1,2,3};float size=1;int32_t type=0;} params;
uint32_t world=1,serial=1;bool thread=true,assets=true;int plays=0,frameBytes=sizeof(Params),missing=-1;
std::function<void()> playCallback;
ue_wrap::script_gate::PostFn watch=nullptr;
int Offset(const wchar_t* name){
    const wchar_t* names[]={L"location",L"velocity",L"size",L"type"};
    const int offsets[]={offsetof(Params,location),offsetof(Params,velocity),offsetof(Params,size),offsetof(Params,type)};
    for(int i=0;i<4;++i)if(!wcscmp(name,names[i]))return missing==i?-1:offsets[i];return -1;
}
ue_wrap::script_gate::Call Call(){ue_wrap::script_gate::Call c{};c.object=&objects[0];c.callerObject=&objects[1];c.function=&objects[2];c.locals=reinterpret_cast<uint8_t*>(&params);return c;}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->index:-1;}
void* ObjectAt(int32_t i){return i>=0&&i<3?&test::objects[i]:nullptr;}
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
bool IsLiveByIndex(void* a,int32_t i){return i>=0&&i<3&&a==&test::objects[i]&&IsLive(a);}
int32_t SlotSerial(int32_t i){return i>=0&&i<3?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void* a){return a;}
std::wstring ClassNameOf(void* a){return static_cast<test::Obj*>(a)->cls;}
void* FindFunction(void*,const wchar_t* name){CHECK(!wcscmp(name,L"addParticle"));return test::assets?&test::objects[2]:nullptr;}
int32_t FunctionFrameSize(void*){return test::frameBytes;}
int32_t FindParamOffset(void*,const wchar_t* n){return test::Offset(n);}
int32_t FindParamSize(void*,const wchar_t* n){return !wcscmp(n,L"size")||!wcscmp(n,L"type")?4:12;}
std::vector<ParamInfo> FunctionParams(void*){std::vector<ParamInfo> out;for(const auto* n:{L"location",L"velocity",L"size",L"type"})out.push_back({n,test::Offset(n),FindParamSize(nullptr,n),0});return out;}
bool CallFunction(void*,void*,void* raw){auto& p=*static_cast<test::Params*>(raw);CHECK(p.type==0&&p.size==1&&p.location.X==10);++test::plays;auto fn=std::move(test::playCallback);test::playCallback={};if(fn)fn();return true;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* a){return a==&test::objects[2]?nullptr:CurrentWorld();}
}
namespace ue_wrap::world_singleton {void* Find(const wchar_t* name){CHECK(!wcscmp(name,L"bloodDecalMaster_C"));return test::assets?&test::objects[0]:nullptr;}}
namespace ue_wrap::game_thread {bool IsGameThread(){return test::thread;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* i,const char* f,const wchar_t* c,const wchar_t* n,Mode m):ident_(i),feature_(f),cls_(c),fn_(n),mode_(m),next_(nullptr){}
}
namespace ue_wrap::script_gate {bool WatchName(const facts::Function&,int,PreFn,PostFn post){test::watch=post;return true;}}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
int main(){
    namespace S=coop::blood_sync;namespace FX=ue_wrap::player_blood;using namespace test;using coop::net::Role;
    auto& registry=coop::players::Registry::Get();auto& roster=coop::roster_ledger::rows;
    for(int i=0;i<coop::net::kMaxPeers;++i)roster[i].playerNo=uint16_t(i+1);
    auto call=Call();FX::Drop drop{};CHECK(FX::ReadDrop(call,drop));
    call.fromOurCode=true;CHECK(!FX::ReadDrop(call,drop));call=Call();
    objects[1].cls=L"npc_zombie_C";CHECK(!FX::ReadDrop(call,drop));objects[1].cls=L"effect_bloodLoss_C";
    params.type=1;CHECK(!FX::ReadDrop(call,drop));params.type=0;
    frameBytes=12;CHECK(!FX::ReadDrop(call,drop));frameBytes=sizeof(Params);
    missing=1;CHECK(!FX::ReadDrop(call,drop));missing=-1;
    params.size=std::numeric_limits<float>::quiet_NaN();CHECK(!FX::ReadDrop(call,drop));params.size=1;
    coop::net::Session session;S::Install(&session);CHECK(watch);
    session.ready[3]=false;watch(call);S::Tick();CHECK(session.sent.size()==30&&plays==0);
    for(auto& s:session.sent)CHECK(s.slot!=0&&s.slot!=3&&s.packet.slot==0);
    session.sent.clear();session.blocked=1;watch(call);S::Tick();CHECK(S::g_peers[1].queue.size()==1);
    session.blocked=-1;S::Tick();CHECK(S::g_peers[1].queue.empty());
    // A client's drop reaches host and other observers, once, excluding its owner.
    coop::net::PlayerBloodPayload p{10,10,20,30,1,2,3,1,2,1,0};
    session.sent.clear();S::Receive(p,1);S::Tick();CHECK(plays==1);
    for(auto& s:session.sent)CHECK(s.slot!=1&&s.slot!=0&&s.packet.slot==1);
    S::Receive(p,1);S::Tick();CHECK(plays==1);
    ++p.sequence;S::Receive(p,2);S::Tick();CHECK(plays==1);
    coop::element::bloodAllowed=false;S::Receive(p,1);S::Tick();CHECK(plays==1);coop::element::bloodAllowed=true;
    p.reserved=1;S::Receive(p,1);S::Tick();CHECK(plays==1);p.reserved=0;
    p.playerNo=3;S::Receive(p,1);S::Tick();CHECK(plays==1);p.playerNo=2;
    // Dependency readiness retries without consuming or duplicating native calls.
    assets=false;S::Receive(p,1);S::Tick();CHECK(plays==1&&S::g_in.size()==1);
    assets=true;S::Tick();S::Tick();CHECK(plays==2&&S::g_in.empty());
    ++p.sequence;assets=false;S::Receive(p,1);S::Tick();++world;assets=true;S::Tick();CHECK(plays==2);
    // A recipient replacement never inherits a blocked drop.
    session.blocked=2;watch(call);S::Tick();CHECK(!S::g_peers[2].queue.empty());
    ++session.generations[2];++roster[2].playerNo;session.blocked=-1;session.sent.clear();S::Tick();CHECK(session.sent.empty());
    // Late join receives subsequent drops, not old completed ones.
    session.ready[3]=true;watch(call);session.sent.clear();S::Tick();bool joined=false;
    for(auto& s:session.sent)joined|=s.slot==3;CHECK(joined);
    // Client publishes only to host; canonical host drops play on the observer.
    S::OnDisconnect();session.mode=Role::Client;registry.slot=1;session.sent.clear();S::Install(&session);
    watch(call);S::Tick();CHECK(session.sent.size()==1&&session.sent[0].slot==0);
    p.slot=0;p.playerNo=1;p.sequence=42;const int before=plays;
    S::Receive(p,2);S::Tick();CHECK(plays==before);S::Receive(p,0);S::Tick();CHECK(plays==before+1);
    p.slot=1;p.playerNo=2;++p.sequence;S::Receive(p,0);S::Tick();CHECK(plays==before+1);
    // Callback teardown clears owned work; no references survive native dispatch.
    p.slot=0;p.playerNo=1;++p.sequence;S::Receive(p,0);playCallback=[]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session);
    session.mode=Role::Host;registry.slot=0;S::Install(&session);watch(call);
    session.callback=[]{S::OnDisconnect();};S::Tick();CHECK(!S::g_session);
    // Disconnecting a source during native authorization invalidates its request
    // even before the roster has been updated by the outer departure handler.
    S::Install(&session);p.slot=1;p.playerNo=2;p.sequence=200;
    coop::element::bloodAuthorize=[]{S::OnPeerLeft(1);};S::Receive(p,1);CHECK(S::g_in.empty());
    // A newer recursive receive wins over the older request being authorized.
    coop::element::bloodAuthorize=[&]{auto newer=p;++newer.sequence;S::Receive(newer,1);};
    S::Receive(p,1);CHECK(S::g_in.size()==1&&S::g_in.front().packet.sequence==201);S::Tick();
    // Requests from the previous session/world must never reach the native body.
    ++p.sequence;p.sequence+=2;assets=false;S::Receive(p,1);S::Tick();const int prior=plays;
    ++serial;assets=true;S::Tick();CHECK(plays==prior&&S::g_in.empty());
    // A blocked recipient has bounded storage; old visual drops expire.
    session.blocked=1;for(int i=0;i<100;++i)watch(call);CHECK(S::g_peers[1].queue.size()==32);
    for(auto& row:S::g_peers[1].queue)row.expires=0;
    session.sent.clear();for(int i=0;i<4;++i)S::Tick();CHECK(S::g_peers[1].queue.empty());
    // Slot 31 uses its own identity and receives ordinary host drops.
    bool lastSlot=false;for(auto& row:session.sent)lastSlot|=row.slot==31;CHECK(lastSlot);
    thread=false;watch(call);S::Receive(p,1);thread=true;
    CHECK(S::g_in.empty());S::OnDisconnect();
    std::puts("PASS blood: native provenance, host/client routing, retries, duplicates, malformed requests, join, occupancy and callback teardown");
}

// Reflection names used by ParamFrame missing-field diagnostics.
namespace ue_wrap::reflection {
std::wstring ToString(const FName&){return L"fixture_function";}
const FName& NameOf(void*){static const FName name{};return name;}
}
