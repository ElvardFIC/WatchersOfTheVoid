#ifndef GEOM_SYNC_SOURCE
#define GEOM_SYNC_SOURCE "../../src/votv-coop/src/coop/creatures/geom_octa_sync.cpp"
#endif
#include GEOM_SYNC_SOURCE
#include "../../src/votv-coop/src/coop/creatures/geom_octa_relay.cpp"
#include <cassert>
#include <functional>
#include <cwchar>

namespace test {
struct Object { void* target=nullptr; bool busy=false; } objects[32];
int serials[32]; bool alive[32]; uint32_t world=1;
uint32_t sessionSerial=1;
coop::net::Session session; coop::net::Session* active=&session;
int frameSize=8, paramOffset=0, overlaps=0, finishes=0, hooks=0, arms=0, disarms=0;
bool marked=false, installOk=true, armOk=true, hookArmed=false, sphereReady=true, graphReady=true;
bool gameThread=true;
void* native=nullptr; void* result=nullptr;
ue_wrap::ufunction_hook::CallerFrame caller{};
uint8_t locals[8]; const uint8_t* passFrame=nullptr;
std::function<void()> onAggro,onFinish;
std::function<void()> onPose,onSaveKey;
bool poseReadable=true,saveKey=false;
uint8_t lastAnswer=255;
namespace S=coop::geom_octa_sync;
namespace L=coop::geom_octa_relay;
namespace N=coop::net;
namespace E=coop::element;
void Reset(bool client=false) {
    S::OnDisconnect();
    ++sessionSerial;
    for(int i=0;i<32;++i){objects[i]={};serials[i]=1;alive[i]=true;}
    session={};session.mode=client?N::Role::Client:N::Role::Host;active=&session;world=1;
    marked=client;installOk=armOk=sphereReady=graphReady=gameThread=true;
    hookArmed=false;overlaps=finishes=hooks=arms=disarms=0;
    native=result=nullptr;caller={};passFrame=nullptr;lastAnswer=255;
    onAggro={};onFinish={};onPose={};onSaveKey={};poseReadable=true;saveKey=false;N::testNow=100;frameSize=8;paramOffset=0;
    E::Registry::Get().rows={};
    E::Registry::Get().rows[0]={1,&objects[0],client};
    E::Registry::Get().rows[1]={2,&objects[1],client};
    E::Registry::Get().rows[2]={3,&objects[2],client};
    coop::players::Registry::Get()={};
    coop::players::Registry::Get().local=&objects[5];
    coop::players::Registry::Get().peers[31].actor=&objects[6];
    L::g_overlapFn.Reset();L::g_ow={};
    S::Install(&session);
    if(client)L::Tick();
    else {S::NoteSent(1);S::NoteSent(2);}
}
ue_wrap::script_gate::Call Call(int actor,void** target,int depth=1,uint64_t invocation=1) {
    ue_wrap::script_gate::Call c{};c.object=&objects[actor];c.function=&objects[12];
    c.locals=reinterpret_cast<uint8_t*>(target);c.stack=&objects[15+depth];c.depth=depth;c.invocation=invocation;
    return c;
}
N::GeomOctaEventPayload Aggro(uint32_t eid=1,uint32_t base=0) {
    N::GeomOctaEventPayload p{};p.octaEid=eid;p.op=N::kGeomOctaOpAggro;
    p.targetKind=N::kGeomOctaTargetSlot;p.target=31;p.verdictNo=base;return p;
}
void Start() {L::OnEvent(Aggro());L::Tick();assert(overlaps==1);}
void Notify() {auto c=Call(0,nullptr);L::OnFinishPre(c);}
void Verdict(uint32_t n=1,uint8_t refire=1) {
    N::GeomOctaEventPayload p{};p.octaEid=1;p.op=N::kGeomOctaOpVerdict;p.verdictNo=n;p.refire=refire;L::OnEvent(p);
}
void Answer(void* context=&objects[10],void* source=&objects[0]) {
    uint8_t answer=0;result=&answer;native=&objects[13];caller={&objects[11],locals,1227};
    L::OnOverlapPost(context,source,nullptr);lastAnswer=answer;result=nullptr;
}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){for(int i=0;i<32;++i)if(p==&test::objects[i])return i;return -1;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0 && i<32 && p==&test::objects[i] && test::alive[i];}
bool IsLive(void* p){return IsLiveByIndex(p,InternalIndexOf(p));}
int32_t SlotSerial(int32_t i){return i>=0 && i<32?test::serials[i]:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void*){return &test::objects[14];}
std::wstring ClassNameOf(void*){return L"geomOcta_C";}
int32_t ParamOffset::Of(void*){return test::paramOffset;}
int32_t FunctionFrameSize(void*){return test::frameSize;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return &test::world;}
void* WorldOf(void*){return &test::world;}
}
namespace coop::net::session_serial {uint32_t Current(){return test::sessionSerial;}}
namespace ue_wrap::engine {
bool TryGetActorTransform(void*,FVector& location,FRotator& rotation,FVector& scale,const std::function<bool()>& current){
    if(test::onPose)test::onPose();
    if(!test::poseReadable||!current())return false;
    location={10,20,30};rotation={1,2,3};scale={2,3,4};return true;
}}
namespace ue_wrap::kerfur {bool HasSaveKey(void*){if(test::onSaveKey)test::onSaveKey();return test::saveKey;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::game_thread {bool IsGameThread(){return test::gameThread;}}
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* ident,const char* feature,const wchar_t* cls,const wchar_t* fn,Mode mode)
 :ident_(ident),feature_(feature),cls_(cls),fn_(fn),mode_(mode),next_(nullptr){}
void Function::NoteUse(Use,bool)const{}
Resolved Resolve(const Function& row){return Resolved(&test::objects[13],&row);}
}
namespace ue_wrap::script_gate {bool WatchName(const facts::Function&,int,PreFn,PostFn){return true;}}
namespace ue_wrap::ufunction_hook {
bool InstallPostHook(const facts::Resolved&,PostNativeCallback,bool){++test::hooks;return test::installOk;}
bool SetArmed(void*,PostNativeCallback,bool value){
    if(value){++test::arms;if(!test::armOk)return false;}else ++test::disarms;
    test::hookArmed=value;return true;
}
CallerFrame CurrentCallerFrame(){return test::caller;}
void* CurrentResult(){return test::result;}
void* CurrentNativeFunction(){return test::native;}
}
namespace coop::npc_sync {net::Session* GetSession(){return test::active;}}
namespace coop::npc_mirror_brain {
bool IsMarkedMirror(void*){return test::marked;}
const EntryTable* TableNamed(const wchar_t*){static EntryTable t{};return &t;}
bool ChainHasTable(void*,const EntryTable&){return true;}
ScopedEntryPass::ScopedEntryPass(void*,std::initializer_list<int32_t>){test::passFrame=test::locals;}
ScopedEntryPass::~ScopedEntryPass(){test::passFrame=nullptr;}
const uint8_t* ScopedEntryPass::Frame(){return test::passFrame;}
}
namespace ue_wrap::geom_octa {
void* TargetActor(void* p){return static_cast<test::Object*>(p)->target;}
bool IsAnim(void* p){return static_cast<test::Object*>(p)->busy;}
void* MinSphere(void*){return test::sphereReady?&test::objects[10]:nullptr;}
void* GraphFunction(void*){return test::graphReady?&test::objects[11]:nullptr;}
bool IsFinishNotify(const script_gate::Call&){return true;}
void* LaserOwner(void*){return &test::objects[0];}
bool CallMinSphereBeginOverlap(void*,void* target){
    assert(target==&test::objects[6]);++test::overlaps;
    if(test::onAggro)test::onAggro();return true;
}
bool CallFireFinishNotify(void*){
    assert(test::hookArmed);++test::finishes;
    if(test::onFinish)test::onFinish();else test::Answer();return true;
}
}

int main() {
    using namespace test;
    // Each nested invocation keeps its own guard and original argument.
    Reset();void* a=&objects[5];void* b=&objects[6];
    auto outer=Call(0,&a);auto inner=Call(1,&b,2,2);
    S::OnAggroPre(outer);S::OnAggroPre(inner);S::OnAggroPost(inner);
    a=&objects[6];S::OnAggroPost(outer);
    assert(session.sent.size()==2 && session.sent[0].target==31 && session.sent[1].target==0);
    S::OnAggroPost(outer);assert(session.sent.size()==2);
    // Same actor recursion, cancellation/reused frames and a mismatched POST.
    Reset();a=&objects[5];outer=Call(0,&a);inner=Call(0,&b,2,2);
    S::OnAggroPre(outer);objects[0].target=&objects[6];S::OnAggroPre(inner);
    S::OnAggroPost(inner);S::OnAggroPost(outer);assert(session.sent.size()==1);
    Reset();S::OnAggroPre(outer);auto successor=outer;successor.invocation=2;S::OnAggroPre(successor);
    S::OnAggroPost(outer);assert(session.sent.empty());S::OnAggroPost(successor);assert(session.sent.size()==1);
    for(int mode=0;mode<6;++mode){
        Reset();S::OnAggroPre(outer);
        if(mode==0)++serials[0];if(mode==1)++serials[12];if(mode==2)++serials[5];
        if(mode==3)++world;if(mode==4)S::OnDisconnect();if(mode==5)session.active=false;
        S::OnAggroPost(outer);assert(session.sent.empty());
    }
    for(int size: {-1,0,7}){Reset();frameSize=size;S::OnAggroPre(outer);S::OnAggroPost(outer);assert(session.sent.empty());}
    Reset();paramOffset=1;S::OnAggroPre(outer);S::OnAggroPost(outer);assert(session.sent.empty());
    Reset();session.sendHook=[] {S::OnDisconnect();};S::OnAggroPre(outer);S::OnAggroPost(outer);
    assert(S::GetCounters().hostAggro==0 && S::GetCounters().sendFailed==0);
    Reset();S::OnFireFinishPre(outer);S::OnFireFinishPre(inner);
    S::OnFireFinishPost(inner);S::OnFireFinishPost(outer);assert(S::GetCounters().hostVerdict==2);
    S::OnFireFinishPost(outer);assert(S::GetCounters().hostVerdict==2);
    for(int mode=0;mode<4;++mode){Reset();S::OnFireFinishPre(outer);
        if(mode==0)++serials[0];if(mode==1)++serials[12];if(mode==2)++world;
        if(mode==3){S::OnDisconnect();S::Install(&session);}
        S::OnFireFinishPost(outer);assert(S::GetCounters().hostVerdict==0);
    }
    // Client ready path uses the host answer, not the local false overlap.
    Reset(true);Start();Notify();Verdict();L::Tick();
    assert(finishes==1 && lastAnswer==1 && !hookArmed && L::GetCounters().overwriteWrites==1);
    // Failed install retries without consuming the verdict or dispatching native work.
    Reset(true);Start();Notify();Verdict();installOk=false;L::Tick();L::Tick();
    assert(hooks==1 && finishes==0 && L::Find(1)->waitCount==1 && L::Find(1)->keptCount==1);
    installOk=true;N::testNow+=1000;L::Tick();assert(hooks==2 && finishes==1);
    // Persistent failure times out without running local overlap or losing isolation.
    Reset(true);Start();Notify();Verdict();installOk=false;L::Tick();N::testNow+=5000;L::Tick();
    assert(finishes==0 && L::Find(1)->waitCount==0 && L::GetCounters().verdictDropped==1);
    Reset(true);Start();Notify();N::testNow+=5000;L::Tick();
    assert(finishes==1 && lastAnswer==0 && L::GetCounters().verdictTimeout==1);
    // Failure to arm also refuses native replay; missing components retain work.
    Reset(true);Start();Notify();Verdict();armOk=false;L::Tick();assert(finishes==0 && !hookArmed);
    assert(L::Find(1)->waitCount==1 && L::Find(1)->keptCount==1);
    armOk=true;L::Tick();assert(finishes==1);
    Reset(true);Start();Notify();Verdict();armOk=false;L::Tick();N::testNow+=5000;L::Tick();
    assert(finishes==0 && L::Find(1)->waitCount==0);
    Reset(true);Start();Notify();Verdict();L::Tick();assert(hooks==1);
    ++serials[13];Notify();Verdict(2);N::testNow+=1000;L::Tick();assert(hooks==2 && finishes==2);
    for(int mode=0;mode<2;++mode){Reset(true);Start();Notify();Verdict();
        if(mode==0)sphereReady=false;else graphReady=false;
        L::Tick();assert(finishes==0 && L::Find(1)->waitCount==1);
        sphereReady=graphReady=true;L::Tick();assert(finishes==1);
    }
    // An eid cannot carry an old attack onto a new actor or recycled object slot.
    Reset(true);Start();Notify();Verdict();++serials[0];L::Tick();assert(finishes==0 && !L::Find(1));
    L::OnEvent(Aggro());L::Tick();assert(overlaps==2 && L::Find(1)->waitCount==0);
    Reset(true);Start();Notify();Verdict();E::Registry::Get().rows[0].actor=&objects[3];L::Tick();assert(finishes==0);
    // Reentrant native callbacks may replace the lane or actor. No old continuation commits.
    for(int mode=0;mode<5;++mode){Reset(true);Start();Notify();Verdict();
        onFinish=[mode] {
            if(mode==0)L::OnDisconnect();if(mode==1)++world;if(mode==2)++serials[0];
            if(mode==3)L::OnEvent(Aggro(1,9));if(mode==4){L::Tick();L::Tick();}
            Answer();
        };
        L::Tick();assert(finishes==1 && !hookArmed);
        if(mode!=4)assert(lastAnswer==0 && L::GetCounters().verdictApplied==0);
        if(mode==3)assert(L::Find(1)->aggroPending && L::Find(1)->aggroBase==9);
    }
    Reset(true);onAggro=[] {L::OnDisconnect();L::OnEvent(Aggro());L::Tick();};
    L::OnEvent(Aggro());L::Tick();assert(overlaps==1 && L::Find(1)->aggroPending);
    // Source, callee, graph, frame, component, thread, and function lifetime all match.
    for(int mode=0;mode<10;++mode){Reset(true);Start();Notify();Verdict();
        onFinish=[mode] {
            uint8_t answer=0;result=&answer;native=&objects[13];caller={&objects[11],locals,1227};
            void* source=&objects[0];void* context=&objects[10];
            if(mode==0)source=&objects[1];if(mode==1)context=&objects[9];if(mode==2)native=&objects[12];
            if(mode==3)caller.function=&objects[12];if(mode==4)caller.locals=nullptr;
            if(mode==5)++serials[13];if(mode==6)gameThread=false;
            if(mode==7){L::ArmScope nested(*L::Find(1),0);assert(!nested.Ready());}
            if(mode==8)caller.codeOffset=1228;
            if(mode==9){L::OnOverlapPost(context,source,nullptr);answer=0;}
            L::OnOverlapPost(context,source,nullptr);lastAnswer=answer;result=nullptr;
        };
        L::Tick();assert(lastAnswer==(mode==7?1:0));
        assert(L::GetCounters().verdictApplied==((mode==7||mode==9)?1:0));
    }
    // Session/world change clears retained state even without an explicit disconnect callback.
    Reset(true);Start();Notify();Verdict();++world;L::Tick();assert(finishes==0 && !L::Find(1));
    Reset(true);Start();Notify();Verdict();session.mode=N::Role::Host;L::Tick();assert(finishes==0);
    Reset();session.sendOk=false;assert(S::SendEvent(Aggro())&&S::g_decisions.size()==1);
    for(int i=0;i<8;++i){N::testNow+=250;S::DrainDecisions();}assert(S::g_decisions.size()==1&&session.sent.empty());
    session.sendOk=true;N::testNow+=250;S::DrainDecisions();assert(S::g_decisions.empty()&&session.sent.size()==1);
    Reset();session.online[1]=true;session.allowSlot=[](int slot){return slot!=31;};assert(S::SendEvent(Aggro()));
    assert(session.slots.size()==1&&session.slots[0]==1&&S::g_decisions.size()==1);
    session.allowSlot={};N::testNow+=250;S::DrainDecisions();assert(session.slots.size()==2&&session.slots[1]==31&&S::g_decisions.empty());
    Reset();session.ready=false;auto event=Aggro();assert(S::SendEvent(event));event.op=N::kGeomOctaOpVerdict;
    event.targetKind=N::kGeomOctaTargetNone;event.target=0;event.verdictNo=1;assert(S::SendEvent(event));
    assert(S::g_decisions.size()==2&&session.sent.empty());session.ready=true;N::testNow+=250;S::DrainDecisions();
    assert(session.sent.size()==2&&session.sent[0].op==N::kGeomOctaOpAggro&&session.sent[1].op==N::kGeomOctaOpVerdict);
    Reset();session.sendOk=false;assert(S::SendEvent(Aggro()));assert(S::SendEvent(Aggro(1,2)));
    assert(S::g_decisions.size()==1&&S::g_decisions[0]->p.verdictNo==2);
    ++session.generations[31];session.sendOk=true;N::testNow+=250;S::DrainDecisions();assert(S::g_decisions.empty()&&session.sent.empty());
    Reset();session.sendOk=false;objects[0].target=&objects[6];assert(S::SendEvent(Aggro()));
    objects[0].target=nullptr;session.sendOk=true;N::testNow+=250;S::DrainDecisions();assert(S::g_decisions.empty()&&session.sent.empty());
    for(int cause=0;cause<3;++cause){Reset(true);Start();Notify();Verdict();
        if(cause==0)++sessionSerial;if(cause==1)++session.stops;if(cause==2)++serials[0];
        L::Tick();assert(finishes==0&&!L::Find(1));}
    Reset();session.birthOk=false;assert(S::SendEvent(Aggro()));
    for(int i=0;i<9;++i){N::testNow+=250;S::DrainDecisions();}
    assert(session.births.empty()&&session.sent.empty()&&S::g_decisions.size()==1);
    session.birthOk=true;N::testNow+=250;S::DrainDecisions();
    assert(session.births.size()==1&&session.sent.size()==1&&S::g_decisions.empty());
    assert(session.wireKinds.size()==2&&session.wireKinds[0]==N::ReliableKind::EntitySpawn&&session.wireKinds[1]==N::ReliableKind::GeomOctaEvent);
    const auto birth=session.births[0];
    assert(birth.elementId==1&&birth.locX==10&&birth.rotYaw==2&&birth.scaleZ==4&&birth.className.len==10);
    Reset();session.allowSlot=[](int){return session.births.empty();};assert(S::SendEvent(Aggro()));
    assert(session.births.size()==1&&session.sent.empty());session.allowSlot={};N::testNow+=250;S::DrainDecisions();
    assert(session.births.size()==1&&session.sent.size()==1);
    Reset();session.online[1]=true;session.allowSlot=[](int slot){return slot==1;};assert(S::SendEvent(Aggro()));
    assert(session.birthSlots.size()==1&&session.birthSlots[0]==1&&session.slots[0]==1);
    session.allowSlot={};N::testNow+=250;S::DrainDecisions();
    assert(session.birthSlots.size()==2&&session.birthSlots[1]==31&&session.slots[1]==31);
    for(int cause=0;cause<5;++cause){Reset();assert(S::SendEvent(Aggro()));assert(session.births.size()==1);
        if(cause==0)++session.generations[31];if(cause==1)++serials[0];if(cause==2)++serials[14];
        if(cause==3)E::Registry::Get().rows[0].actor=&objects[3];
        if(cause==4){++sessionSerial;S::Install(&session);}
        assert(S::SendEvent(Aggro(1,2)));assert(session.births.size()==2&&session.sent.size()==2);
    }
    Reset();assert(S::SendEvent(Aggro()));assert(S::SendEvent(Aggro(1,2)));
    assert(session.births.size()==1&&session.sent.size()==2);
    Reset();onPose=[] {session.ready=false;};assert(S::SendEvent(Aggro()));
    assert(session.births.empty()&&session.sent.empty()&&S::g_decisions.size()==1);
    onPose={};session.ready=true;N::testNow+=250;S::DrainDecisions();assert(session.sent.size()==1);
    Reset();poseReadable=false;assert(S::SendEvent(Aggro()));
    assert(session.births.empty()&&session.sent.empty()&&S::g_decisions.size()==1);
    poseReadable=true;N::testNow+=250;S::DrainDecisions();assert(session.sent.size()==1);
    for(int cause=0;cause<8;++cause){Reset();onPose=[cause]{
        if(cause==0)++world;if(cause==1)++sessionSerial;if(cause==2)++session.stops;
        if(cause==3)++serials[0];if(cause==4)++serials[14];
        if(cause==5)S::OnDisconnect();if(cause==6)session.mode=N::Role::Client;
        if(cause==7)E::Registry::Get().rows[0].typeName="replacement_C";
    };S::SendEvent(Aggro());assert(session.births.empty()&&session.sent.empty());}
    for(int cause=0;cause<3;++cause){Reset();onSaveKey=[cause]{
        if(cause==0)++world;if(cause==1)++serials[0];if(cause==2)S::OnDisconnect();
    };S::SendEvent(Aggro());assert(session.births.empty()&&session.sent.empty());}
    Reset();saveKey=true;E::Registry::Get().rows[0].typeName="geomOcta_variant_C";
    assert(S::SendEvent(Aggro()));assert(session.births[0].savePersisted==1&&session.births[0].className.len==18);
    for(int cause=0;cause<3;++cause){Reset();session.sendHook=[cause]{
        if(cause==0)++session.generations[31];if(cause==1)session.online[31]=false;if(cause==2)++session.stops;
    };S::SendEvent(Aggro());assert(session.births.size()==1&&session.sent.empty()&&S::g_births.empty());}
    Reset();bool nested=false;int saveReads=0;onSaveKey=[&]{++saveReads;};onPose=[&]{if(!nested){nested=true;assert(S::SendEvent(Aggro(1,8)));}};
    S::SendEvent(Aggro());assert(session.births.empty()&&session.sent.empty()&&S::g_decisions.size()==1);
    assert(saveReads==0);
    onPose={};N::testNow+=250;S::DrainDecisions();assert(session.births.size()==1&&session.sent.size()==1&&session.sent[0].verdictNo==8);
    Reset();nested=false;session.sendHook=[&]{if(!nested){nested=true;assert(S::SendEvent(Aggro(1,9)));}};
    S::SendEvent(Aggro());assert(session.births.size()==1&&session.sent.empty()&&S::g_births.empty());
    session.sendHook={};N::testNow+=250;S::DrainDecisions();assert(session.births.size()==2&&session.sent.size()==1&&session.sent[0].verdictNo==9);
    Reset();assert(S::SendEvent(Aggro()));session.MarkSlotWorldReady(31,false);session.MarkSlotWorldReady(31,true);
    assert(S::SendEvent(Aggro(1,2)));assert(session.births.size()==2&&session.sent.size()==2);
    Reset();session.online[1]=true;assert(S::SendEvent(Aggro()));
    session.MarkSlotWorldReady(31,false);session.MarkSlotWorldReady(31,true);assert(S::SendEvent(Aggro(1,2)));
    assert(session.birthSlots.size()==3&&session.birthSlots.back()==31&&session.sent.size()==4);
    for(int boundary=0;boundary<4;++boundary){Reset();auto resetWorld=[]{session.MarkSlotWorldReady(31,false);session.MarkSlotWorldReady(31,true);};
        if(boundary==0)onPose=resetWorld;if(boundary==1)onSaveKey=resetWorld;
        if(boundary>=2)session.sendHook=[=]{if((session.wireKinds.back()==N::ReliableKind::EntitySpawn)==(boundary==2))resetWorld();};
        S::SendEvent(Aggro());assert(session.sent.size()==(boundary==3?1u:0u)&&S::g_decisions.size()==1);
        if(boundary<2)assert(session.births.empty());
        onPose=onSaveKey={};session.sendHook={};N::testNow+=250;S::DrainDecisions();
        assert(S::g_decisions.empty()&&session.births.size()==(boundary<2?1u:2u)&&session.sent.size()==(boundary==3?2u:1u));
    }
    puts("PASS UI06 birth world-ready revisions: reset between pumps and during pose/save-key/birth/event continuations; unaffected slots retain receipts");
    puts("PASS UI06 recipient birth: refusal beyond five attempts, exact slot/generation/actor/class/session ownership, ordered birth/event, native reads, idempotent retry and nested successors");
    puts("PASS UI06 retained decisions: per-recipient refusal/readiness/FIFO, supersession, departed target/peer and session reconstruction");
    puts("PASS Ramiel: nested host capture, malformed frames, serial/world/session cancellation, hook retry/refusal, exact overwrite and replay reentrancy");
}
