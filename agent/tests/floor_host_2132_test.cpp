#define NDEBUG
#include <intrin.h>
#include "../../src/votv-coop/src/coop/props/host_spawn_watcher.cpp"
#include "facts_test_support.h"
namespace ue_wrap::facts {
Resolved ResolveOn(const Function& row,void* cls){
    void* fn=cls?reflection::FindFunction(cls,row.Name()):nullptr;
    return fn?Resolved(fn,&row):Resolved{};
}
Resolved Resolve(const Function& row){return ResolveOn(row,object_index::ClassByName(row.Class()));}
}
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace H=coop::host_spawn_watcher;
namespace test {
struct Actor { int id=0,serial=1,mints=0,expressed=0,unmarks=0;bool live=true,floorActor=true,tracked=false,mirror=false,key=false,processed=false; } actors[260];
int world=1,otherWorld=2;
void* current=&world;
Actor* hand=nullptr;
bool authored=true,mintOk=true,elementMirror=false;
bool gameplayCaller=false,wrongCallerOwner=false,enrollOnExpress=false;
bool waterPour=false;
void* fixed=nullptr;void* frozen=nullptr;void* remoteHand=nullptr;
std::vector<void*> coasted;
int publishes=0;float charge=1.f,publishedCharge=-1.f;
std::function<void()> duringReadKey;
std::function<void()> duringKey,duringExpress;
coop::net::Session session;
bool engineClass=false,ambientClasses=false,beginFunction=true,finishHookOK=true,observerOK=true;
int keyedHooks=0,ambientHooks=0,returnOffset=0;
ue_wrap::ufunction_hook::PostNativeCallback finishCallback=nullptr;
void Callback(std::function<void()>& f){auto cb=std::move(f);f={};if(cb)cb();}
void Enqueue(int i=0){H::OnFinishSpawnFunc(nullptr,nullptr,&actors[i]);}
void Drain(){H::DrainPendingSpawns(&session);}
void Reset(){H::OnDisconnect();H::SetSession(&session);session.linked=true;session.mode=coop::net::Role::Host;
    for(int i=0;i<260;++i){actors[i]=Actor{};actors[i].id=i;}
    publishes=0;charge=1.f;publishedCharge=-1.f;elementMirror=false;duringReadKey={};
    gameplayCaller=wrongCallerOwner=enrollOnExpress=waterPour=false;fixed=frozen=remoteHand=nullptr;coasted.clear();
    current=&world;hand=nullptr;authored=mintOk=true;duringKey={};duringExpress={};}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::hot_path {unsigned long CurrentThreadId(){return 0;}unsigned long long CallerRva(const void*){return 0;}}
namespace ue_wrap::game_thread {
bool IsGameThread(){return true;}bool IsDefinitelyOffGameThread(){return false;}
bool RegisterPostObserver(const facts::Resolved&,void(*)(void*,void*,void*)){++test::ambientHooks;return test::observerOK;}
}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a && static_cast<test::Actor*>(a)->live;}
int32_t InternalIndexOf(void* a){return static_cast<test::Actor*>(a)->id;}
int32_t SlotSerial(int32_t i){return test::actors[i].serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* a,int32_t i){return a==&test::actors[i] && IsLive(a);}
void* FindClass(const wchar_t*){std::abort();}
void* FindFunction(void*,const wchar_t* n){return !wcscmp(n,profile::name::FinishSpawningActorFn)?&test::actors[257]:test::beginFunction?&test::actors[258]:nullptr;}
int32_t FindParamOffset(void*,const wchar_t* n){return !wcscmp(n,L"ReturnValue")?test::returnOffset:0;}
int32_t FunctionFrameSize(void*){return 8;}
std::wstring ClassNameOf(void*){std::abort();}
void* OuterOf(void*){return &test::actors[255];}
const FName& NameOf(void* a){static FName n{};n.ComparisonIndex=a==&test::actors[255]?1:2;return n;}
bool NameEquals(const FName& n,const wchar_t* value){return n.ComparisonIndex==1?
    (!test::wrongCallerOwner&&!wcscmp(value,profile::name::GamemodeClass)):
    !wcscmp(value,L"spawnPropThroughGamemode");}
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* n){return !wcscmp(n,profile::name::GameplayStaticsClass)?
    (test::engineClass?&test::actors[259]:nullptr):(test::ambientClasses?&test::actors[259]:nullptr);}
}
namespace ue_wrap::world_identity {void* WorldOf(void*){return &test::world;}void* CurrentWorld(){return test::current;}}
namespace ue_wrap::prop {bool IsDescendantOfProp(void*){return true;}
bool IsStatic(void* a){return a==test::fixed;}bool IsFrozen(void* a){return a==test::frozen;}
std::wstring GetInteractableKeyString(void* a){test::Callback(test::duringReadKey);return static_cast<test::Actor*>(a)->key?L"item":L"";}}
namespace ue_wrap::floor_bed {
bool IsFloorBed(void* a){return static_cast<test::Actor*>(a)->floorActor;}
bool EnsureKey(void* a){auto& actor=*static_cast<test::Actor*>(a);++actor.mints;
    test::Callback(test::duringKey);if(test::mintOk)actor.key=true;return test::mintOk;}
}
namespace ue_wrap::water_pour {bool IsPour(void* a){return test::waterPour && a==&test::actors[0];}bool IsProjectile(void* a){return IsPour(a);}}
namespace coop::water_pour_sync {void ObserveHostBirth(void*,void*){}}
namespace ue_wrap::ufunction_hook {
CallerFrame CurrentCallerFrame(){return {test::gameplayCaller?&test::actors[254]:nullptr,nullptr};}
bool InstallPostHook(void*,PostNativeCallback cb,bool){++test::keyedHooks;test::finishCallback=cb;return test::finishHookOK;}}
namespace ue_wrap::ufunction_hook {bool InstallPostHook(const facts::Resolved& fn,PostNativeCallback cb,bool flag){return InstallPostHook(fn.Fn(),cb,flag);}}
namespace coop::element {
Element::Element(ElementType type):m_type(type){} Element::~Element()=default;
Element* Registry::Get(ElementId id)const {static Element e(ElementType::Prop);e.SetMirror_(test::elementMirror);return id==kInvalidId?nullptr:&e;}
Registry::Registry()=default;Registry::~Registry()=default;
Registry& Registry::Get(){static Registry r;return r;}
ElementId Registry::EidForActor(void* a)const{return static_cast<test::Actor*>(a)->tracked?1:kInvalidId;}
}
namespace coop::prop_element_tracker {
coop::element::ElementId GetPropElementIdForActor(void* a){return static_cast<test::Actor*>(a)->tracked?1:coop::element::kInvalidId;}
void MarkProcessedInit(void*){std::abort();}
void UnmarkProcessedInit(void* a){auto& actor=*static_cast<test::Actor*>(a);actor.processed=false;++actor.unmarks;}
std::wstring MarkPropElement(void*,const std::wstring&,const std::wstring&,EnrollSource){std::abort();}
}
namespace coop::prop_echo_suppress {
bool IsMirrorSpawn(void* a){return static_cast<test::Actor*>(a)->mirror;}
bool InMirrorSpawnScope(){return false;}
}
namespace coop::hand_item {void* LocalHandActor(){return test::hand;}
bool IsHandAxisActor(void* a){return a==test::hand||a==test::remoteHand;}}
namespace coop::prop_drive_host {void Coast(void* a,const char*,bool){
    CHECK(static_cast<test::Actor*>(a)->tracked);test::coasted.push_back(a);}}
namespace coop::prop_spawn_authoring {bool BirthIsPlayerAuthored(void*){return test::authored;}}
namespace coop::join_membership_sweep {void RecordClaimIfTracking(void*){std::abort();}}
namespace coop::prop_lifecycle {void ExpressSpawnedProp(void* a){auto& actor=*static_cast<test::Actor*>(a);
    CHECK(actor.live);if(actor.key&&!actor.processed){++actor.expressed;actor.processed=true;if(test::enrollOnExpress)actor.tracked=true;}test::Callback(test::duringExpress);}}
namespace coop::prop_save_data {bool Publish(net::Session*,void*,const std::wstring& key){CHECK(key==L"item");++test::publishes;test::publishedCharge=test::charge;return true;}}
int main(){using namespace test;
    Reset();Enqueue();Drain();CHECK(actors[0].mints==1 && actors[0].expressed==1);Drain();CHECK(actors[0].expressed==1);
    for(int gate=0;gate<5;++gate){Reset();
        if(gate==0)authored=false;if(gate==1)actors[0].mirror=true;if(gate==2)actors[0].tracked=true;
        if(gate==3)hand=&actors[0];if(gate==4)actors[0].floorActor=false;
        Enqueue();Drain();CHECK(actors[0].mints==0 && actors[0].expressed==0);}
    Reset();authored=false;Enqueue();actors[0].key=true;Drain();CHECK(actors[0].mints==0 && actors[0].expressed==1);
    Reset();mintOk=false;Enqueue();Drain();CHECK(H::g_pendingFinished.size()==1 && actors[0].expressed==0);
    mintOk=true;Drain();CHECK(actors[0].expressed==1 && actors[0].mints==2);
    Reset();mintOk=false;Enqueue();for(int i=0;i<20;++i)Drain();CHECK(actors[0].mints==9 && H::g_pendingFinished.empty());
    for(int cause=0;cause<3;++cause){Reset();Enqueue();
        if(cause==0)++actors[0].serial;if(cause==1)current=&otherWorld;if(cause==2)actors[0].live=false;
        Drain();CHECK(actors[0].mints==0 && actors[0].expressed==0);}
    for(bool keyPhase:{false,true})for(int cause=0;cause<6;++cause){
        Reset();Enqueue();Enqueue(1);
        auto& cb=keyPhase?duringKey:duringExpress;
        cb=[cause]{if(cause==0)++actors[0].serial;if(cause==1)current=&otherWorld;
                   if(cause==2)H::OnDisconnect();if(cause==3)session.linked=false;
                   if(cause==4)actors[0].mirror=true;if(cause==5)actors[0].tracked=true;};
        Drain();CHECK(actors[0].expressed==(keyPhase?0:1));
        if(cause==1 || cause==2 || cause==3)CHECK(actors[1].expressed==0 && actors[1].mints==0);
    }
    for(bool keyPhase:{false,true}){
        Reset();Enqueue();auto& cb=keyPhase?duringKey:duringExpress;
        cb=[]{Enqueue(1);Drain();};Drain();CHECK(actors[0].expressed==1 && actors[1].expressed==0 && H::g_pendingFinished.size()==1);
        Drain();CHECK(actors[1].expressed==1);
        Reset();Enqueue();auto& reset=keyPhase?duringKey:duringExpress;
        reset=[]{H::OnDisconnect();H::SetSession(&session);Enqueue(1);};Drain();
        CHECK(H::g_pendingFinished.size()==1);Drain();CHECK(actors[1].expressed==1);
    }
    Reset();mintOk=false;Enqueue();duringKey=[]{for(int i=1;i<150;++i)Enqueue(i);};Drain();
    CHECK(H::g_pendingFinished.size()==128);
    Reset();session.mode=coop::net::Role::Client;Enqueue();CHECK(H::g_pendingFinished.empty());
    // b2146: init already owns the actor before finish returns; the leaf restores charge LATER.
    Reset();actors[0].tracked=actors[0].key=true;Enqueue();CHECK(publishes==0);
    charge=.17f;Drain();CHECK(publishes==1 && publishedCharge==.17f && actors[0].expressed==0);
    Drain();CHECK(publishes==1);
    Reset();actors[0].key=true;Enqueue();actors[0].tracked=true;charge=0.f;Drain();CHECK(publishes==1 && publishedCharge==0.f);
    for(int cause=0;cause<7;++cause){Reset();actors[0].tracked=actors[0].key=true;Enqueue();
        if(cause==0)hand=&actors[0];if(cause==1)actors[0].mirror=true;if(cause==2)elementMirror=true;
        if(cause==3)actors[0].live=false;if(cause==4)++actors[0].serial;if(cause==5)current=&otherWorld;
        if(cause==6)H::OnDisconnect();Drain();CHECK(publishes==0);}
    for(int cause=0;cause<4;++cause){Reset();actors[0].tracked=actors[0].key=true;Enqueue();
        duringReadKey=[cause]{if(cause==0)H::OnDisconnect();if(cause==1)actors[0].mirror=true;
                              if(cause==2)hand=&actors[0];if(cause==3)elementMirror=true;};
        Drain();CHECK(publishes==0);}
    // 2150: a premature Init dedupe mark is not a completed birth. Native container
    // break restores the final record after FinishSpawn; wait for its key and re-enter express.
    Reset();actors[0].floorActor=false;actors[0].processed=true;Enqueue();Drain();
    CHECK(actors[0].unmarks==0&&actors[0].expressed==0&&H::g_pendingFinished.size()==1);
    actors[0].key=true;Drain();CHECK(actors[0].unmarks==1&&actors[0].expressed==1&&H::g_pendingFinished.empty());
    Drain();CHECK(actors[0].expressed==1);
    Reset();actors[0].floorActor=false;Enqueue();for(int i=0;i<20;++i)Drain();
    CHECK(H::g_pendingFinished.empty()&&actors[0].unmarks==0); // bounded, no per-frame forever retry
    for(int cause=0;cause<4;++cause){Reset();actors[0].floorActor=false;actors[0].key=true;Enqueue();
        duringReadKey=[cause]{if(cause==0)actors[0].tracked=true;if(cause==1)actors[0].mirror=true;
                              if(cause==2)hand=&actors[0];if(cause==3)++actors[0].serial;};
        Drain();CHECK(actors[0].unmarks==0&&actors[0].expressed==0);}
    // 2158: missing ambient assets/signature must never disable ordinary keyed births.
    Reset();H::Install(&session);CHECK(keyedHooks==0);
    engineClass=true;beginFunction=false;
    for(int i=0;i<65;++i)H::Install(&session);
    CHECK(keyedHooks==1&&finishCallback&&ambientHooks==0&&H::g_observerRegistered);
    actors[0].floorActor=false;actors[0].key=true;
    finishCallback(nullptr,nullptr,&actors[0]);Drain();CHECK(actors[0].expressed==1);
    H::g_observerRegistered=false;beginFunction=true;
    for(int i=0;i<65;++i)H::Install(&session);
    CHECK(keyedHooks==1&&ambientHooks==0&&!H::g_classesResolved);
    ambientClasses=true;
    for(int i=0;i<65;++i)H::Install(&session);
    CHECK(keyedHooks==1&&ambientHooks==1&&H::g_classesResolved);
    H::g_observerRegistered=false;observerOK=false;
    for(int i=0;i<65;++i)H::Install(&session);
    CHECK(keyedHooks==1&&ambientHooks==2); // POST table failure is independent too
    H::g_finishHookSettled=false;returnOffset=8;H::InstallKeyedFinish();
    CHECK(keyedHooks==1&&!H::g_finishSpawnFn); // refuses out-of-frame return
    H::g_finishHookSettled=false;returnOffset=0;finishHookOK=false;H::InstallKeyedFinish();
    H::InstallKeyedFinish();CHECK(keyedHooks==2); // bounded failure, no registration/log storm
    // 2159: seven ordinary cubicle gibs all enroll independently; no surviving
    // source actor is required. Motion begins after final native state is filled.
    Reset();gameplayCaller=enrollOnExpress=true;
    for(int i=0;i<7;++i){actors[i].floorActor=false;actors[i].key=true;Enqueue(i);}
    gameplayCaller=false; // provenance must survive the factory frame returning
    CHECK(coasted.empty());actors[20].live=false;charge=.42f;Drain();
    CHECK(coasted.size()==7&&publishes==7&&publishedCharge==.42f);
    for(int i=0;i<7;++i)CHECK(actors[i].expressed==1&&coasted[i]==&actors[i]);
    Drain();CHECK(coasted.size()==7&&publishes==7);
    // Save/mirror/hand/static births must not start a host motion stream.
    for(int cause=0;cause<10;++cause){Reset();gameplayCaller=enrollOnExpress=true;
        actors[0].floorActor=false;actors[0].key=true;
        if(cause==0)gameplayCaller=false;if(cause==1)wrongCallerOwner=true;
        Enqueue();
        if(cause==2)actors[0].mirror=true;if(cause==3)hand=&actors[0];
        if(cause==4)fixed=&actors[0];if(cause==5)frozen=&actors[0];
        if(cause==6)remoteHand=&actors[0];if(cause==7)actors[0].live=false;
        if(cause==8)++actors[0].serial;if(cause==9)current=&otherWorld;
        Drain();CHECK(coasted.empty());}
    Reset();gameplayCaller=true;actors[0].tracked=actors[0].key=true;Enqueue();Drain();
    CHECK(coasted.size()==1&&actors[0].expressed==0); // early enrollment still coasts
    Reset();gameplayCaller=enrollOnExpress=true;actors[0].floorActor=false;Enqueue();Drain();CHECK(coasted.empty());
    actors[0].key=true;Drain();CHECK(coasted.size()==1); // delayed key
    Reset();gameplayCaller=enrollOnExpress=true;actors[0].key=true;Enqueue();
    duringExpress=[]{H::OnDisconnect();};Drain();CHECK(coasted.empty());
    // Bucket water uses its own native factory, outside spawnPropThroughGamemode.
    Reset();waterPour=enrollOnExpress=true;actors[0].floorActor=false;actors[0].key=true;
    Enqueue();waterPour=false;CHECK(coasted.empty());charge=.73f;Drain();
    CHECK(coasted.size()==1 && publishes==1 && publishedCharge==.73f);
    Drain();CHECK(coasted.size()==1); // final velocity/record is read on the next pump
    puts("PASS: keyed install, seven-part gameplay births and coasting, final state and lifetime/exclusion guards");
}
