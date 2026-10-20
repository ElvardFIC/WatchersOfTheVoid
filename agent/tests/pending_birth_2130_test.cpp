// Actual finish-spawn enqueue and Tick. Engine/object-array/network boundaries are doubled.
#define NDEBUG
#include <intrin.h>
#include "../../src/votv-coop/src/coop/props/prop_drop_intent.cpp"
#include "ue_wrap/core/cached_obj_ref.h"
#include "coop/props/local_birth_protection.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace D=coop::prop_drop_intent;
namespace test {
int actor=0,world=0,nextWorld=0,serial=1,publishes=0,claims=0;
int keyReads=0,readableAfter=100000;
void* currentWorld=&world;
bool live=true,authored=true,extract=false,hand=false,mirror=false,tracked=false,readable=true;
bool floorActor=false,mintOk=true;
bool locationOk=true,rotationOk=true;
int mints=0;
bool foodActor=false,foodReadable=true,foodWritable=true,finished=false;
int destroys=0,foodWrites=0,invalidates=0;
int spawns=0,brought=0;
void* existing=nullptr;
bool spawnOK=true;
std::wstring className=L"prop_test_C";
ue_wrap::food::Condition condition{300.f,0.75f,2};
std::function<void()> duringFinish;
std::function<void()> duringKey;
std::function<void()> duringRead,duringPublish;
coop::net::Session session;
void Enqueue(){D::OnClientFinishSpawn(nullptr,nullptr,&actor);}
void Reset(){D::Reset();session.drops.clear();session.linked=true;D::g_session=&session;
    session.results.clear();session.duringSend={};session.ready=true;session.blockedSlot=-1;
    session.onDrop=[](const auto& p){D::OnPlacementResult(session,{p.requestId,0,{}},0);};
    existing=nullptr;spawnOK=true;spawns=brought=0;className=L"prop_test_C";
    locationOk=rotationOk=true;
    live=authored=readable=true;extract=hand=mirror=tracked=false;serial=1;currentWorld=&world;
    foodActor=false;foodReadable=foodWritable=true;finished=false;destroys=foodWrites=invalidates=0;condition={300.f,0.75f,2};duringFinish={};session.sendOK=true;session.mode=coop::net::Role::Client;
    publishes=claims=keyReads=mints=0;floorActor=false;mintOk=true;duringKey={};
    readableAfter=100000;duringRead={};duringPublish={};}
void Callback(std::function<void()>& slot){auto fn=std::move(slot);slot={};if(fn)fn();}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* ident,const char* feature,const wchar_t* cls,const wchar_t* fn,Mode mode)
    : ident_(ident),feature_(feature),cls_(cls),fn_(fn),mode_(mode),next_(nullptr){}
Resolved Resolve(const Function&){return {};}
}
namespace ue_wrap::ufunction_hook {bool InstallPostHook(const facts::Resolved&,PostNativeCallback,bool){return false;}}
namespace ue_wrap::hot_path {unsigned long CurrentThreadId(){return 0;} unsigned long long CallerRva(const void*){return 0;}}
namespace ue_wrap::game_thread {bool IsGameThread(){return true;}bool IsDefinitelyOffGameThread(){return false;}}
namespace ue_wrap::reflection {
bool IsLive(void*){return test::live;}
int32_t InternalIndexOf(void*){return 5;}
bool IsLiveByIndex(void*,int32_t){return test::live;}
int32_t AllocateSlotSerial(int32_t){return test::serial;}
int32_t SlotSerial(int32_t){return test::serial;}
void* ClassOf(void*){return &test::actor;}
std::wstring ClassNameOf(void*){return test::className;}
}
namespace ue_wrap::world_identity {void* WorldOf(void*){return &test::world;}void* CurrentWorld(){return test::currentWorld;}}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t*){return &test::actor;}}
namespace coop::prop_key_index {void* Find(const std::wstring&){return test::existing;}}
namespace ue_wrap::engine {
bool TryGetActorLocation(void*,FVector& out){test::Callback(test::duringRead);out={1,2,3};return test::locationOk;}
bool TryGetActorRotation(void*,FRotator& out){out={};return test::rotationOk;}
FVector GetActorScale3D(void*){return {1,1,1};}
}
namespace ue_wrap::prop {
bool IsDescendantOfProp(void*){return true;}
std::wstring GetInteractableKeyString(void*){return ++test::keyReads>=test::readableAfter || test::readable?L"item":L"";}
std::wstring GetPropNameString(void*){return L"test";}
}
namespace ue_wrap::floor_bed {
bool IsFloorBed(void*){return test::floorActor;}
bool IsFloorBedClass(void*){return test::floorActor;}
bool EnsureKey(void*){++test::mints;test::Callback(test::duringKey);if(test::mintOk)test::readable=true;return test::mintOk;}
}
namespace ue_wrap::floppy_disc {bool EnsureResolved(){return false;}bool IsDiscClass(void*){return false;}}
namespace ue_wrap::tape_caddy {bool EnsureResolved(){return false;}bool IsReelClass(void*){return false;}}
namespace ue_wrap::phys_mods {bool EnsureResolved(){return false;}bool IsModuleClass(void*){return false;}}
namespace ue_wrap::drive_chain {bool EnsureResolved(){return false;}bool IsDriveClass(void*){return false;}}
namespace ue_wrap::serverbox {bool IsUpgradeClass(void*){return false;}}
namespace coop::prop_wire_parity {uint8_t PhysFlagsOf(void*){return 0;}}
namespace coop::element {
Registry::Registry()=default;Registry::~Registry()=default;
Registry& Registry::Get(){static Registry r;return r;}
ElementId Registry::EidForActor(void*)const{return test::tracked?1:kInvalidId;}
}
namespace coop::prop_element_tracker {coop::element::ElementId GetPropElementIdForActor(void*){return test::tracked?1:coop::element::kInvalidId;}}
namespace coop::prop_spawn_authoring {bool BirthIsPlayerAuthored(void*){return test::authored;}}
namespace coop::props::container_contents_sync {bool TakeObjInCall(){return test::extract;}bool TakeObjWindowOpen(){return false;}}
namespace coop::world_load_episode {bool InEpisode(){return false;}bool InReconcileWindow(){return false;}bool ReconcileWindowIsLoadKind(){return false;}}
namespace coop::hand_item {bool IsHandAxisActor(void*){return test::hand;}}
namespace coop::prop_echo_suppress {bool IsMirrorSpawn(void*){return test::mirror;}}
namespace coop::join_membership_sweep {void RecordSelfAuthored(void*){}void RecordClaimIfTracking(void*){++test::claims;}}
namespace coop::drive_payload_sync {void NoteOwnDrive(void*){}}
namespace coop::prop_save_data {bool Covers(void*){return true;}bool Publish(net::Session*,void*,const std::wstring&){++test::publishes;test::Callback(test::duringPublish);return true;}}
namespace coop::dev::prop_birth_key_probe {
void NoteBirth(void*){}
bool IsEnabled(){return false;}
void NoteEnqueue(void*,const std::wstring&,const std::wstring&,bool){}
void NotePendingCapHit(void*){}
void NoteParkEvict(const std::wstring&){}
void NoteKeyReadable(void*,int){}
void NoteDrainExit(void*,const char*,int,const std::wstring&){}
}
namespace ue_wrap::food {
bool IsFood(void*) {return test::foodActor;}
bool Read(void*,Condition& out) {out=test::condition;return test::foodActor&&test::foodReadable;}
bool Write(void*,const Condition& in) {CHECK(test::finished);if(!test::foodWritable)return false;test::condition=in;++test::foodWrites;return true;}
}
namespace coop::props::prop_food_sync {void ForgetPublished(const std::wstring& key){CHECK(key==L"item");++test::invalidates;}}
// Unused host/installation boundaries must never accidentally run in this client-side test.
namespace coop::prop_lifecycle {bool IsWireSuppressedPropClass(const std::wstring&){return false;}bool IsPerPlayerPropClass(const std::wstring&){return false;}}
namespace coop::prop_save_data {void ExpectRecordFor(void*,const std::wstring&,uint8_t){}bool ApplyParked(void*,const std::wstring&){return false;}}
namespace coop::prop_element_tracker {void* ResolveLiveActorByKey(const std::wstring&,bool*){std::abort();}}
namespace coop::physmods_sync {bool HostShouldReapModuleBirth(uint8_t,void*){std::abort();}}
namespace coop::server_upgrade_sync {bool HostShouldReapUpgradeBirth(uint8_t){std::abort();}}
namespace coop::drive_payload_sync {void NoteClientBrought(void*,uint8_t){++test::brought;}}
namespace ue_wrap::reflection {void* FindClass(const wchar_t*){return &test::actor;}void* FindFunction(void*,const wchar_t*){return nullptr;}}
namespace ue_wrap::engine {
void* BeginDeferredSpawn(void*,const FVector&,const FRotator&){++test::spawns;return test::spawnOK?&test::actor:nullptr;}
bool FinishDeferredSpawn(void*,const FVector&,const FRotator&){test::condition={290.f,1.f,5};test::finished=true;test::Callback(test::duringFinish);return true;}
bool DestroyActor(void*){++test::destroys;return true;}
bool SetActorScale3D(void*,const FVector&){return true;}
}
namespace ue_wrap {ParamFrame::ParamFrame(void*){std::abort();}bool ParamFrame::SetRaw(const wchar_t*,const void*,int32_t){std::abort();}bool Call(void*,ParamFrame&){std::abort();}}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring&){return {1,0};}}
namespace ue_wrap::prop {bool IsClassDescendantOfProp(void*){return true;}bool WriteSpParityIdentity(void*,reflection::FName,bool,bool,bool,bool){return true;}}
namespace ue_wrap::ufunction_hook {bool InstallPostHook(void*,void(*)(void*,void*,void*),bool){std::abort();}}
int main(){using namespace test;
    // Bench58's reported rifle uses the ordinary placement lifecycle too.
    // Refusal must retain its actor while reconciliation is running.
    Reset();className=L"prop_rifle_C";Enqueue();session.onDrop={};D::Tick(&session);
    CHECK(session.drops.size()==1&&D::IsAwaitingHost(&actor));
    CHECK(D::FindAwaitingHost(L"item",className)==&actor);
    D::OnPlacementResult(session,{session.drops[0].requestId,
        static_cast<uint8_t>(coop::net::PropPlacementCode::Forbidden),{}},0);
    CHECK(D::IsLocalPlacement(&actor));
    coop::join_membership_sweep::LocalBirths rifleBirths;
    CHECK(coop::join_membership_sweep::ProtectedLocalBirth(&actor,rifleBirths));
    // Older accepted work retains its place when native capture enqueues a
    // full new batch. The combined retry queue must still respect its bound.
    Reset();Enqueue();const auto queued=D::g_pending.front();
    D::g_pending.assign(D::kMaxPending,queued);locationOk=false;
    duringRead=[&]{D::g_pending.assign(D::kMaxPending,queued);};D::Tick(&session);
    CHECK(D::g_pending.size()==D::kMaxPending&&D::g_pending.front().tries==1);
    Reset();Enqueue();CHECK(D::g_pending.size()==1);D::Tick(&session);
    CHECK(session.drops.size()==1);CHECK(publishes==1);CHECK(D::g_pending.empty());
    Reset();Enqueue();++serial;D::Tick(&session);CHECK(session.drops.empty());CHECK(publishes==0);
    Reset();Enqueue();currentWorld=&nextWorld;D::Tick(&session);CHECK(session.drops.empty());
    Reset();Enqueue();live=false;D::Tick(&session);CHECK(session.drops.empty());
    Reset();readable=false;Enqueue();D::Tick(&session);CHECK(D::g_pending.size()==1);
    readable=true;D::Tick(&session);CHECK(session.drops.size()==1);CHECK(D::g_pending.empty());
    Reset();readable=false;Enqueue();for(int i=0;i<10;++i)D::Tick(&session);
    CHECK(session.drops.empty());CHECK(D::g_pending.empty());
    for(int gate=0;gate<3;++gate){
        Reset();Enqueue();if(gate==0)hand=true;if(gate==1)tracked=true;if(gate==2)mirror=true;
        D::Tick(&session);CHECK(session.drops.empty());CHECK(D::g_pending.empty());
    }
    Reset();authored=false;Enqueue();D::Tick(&session);CHECK(session.drops.empty());
    Reset();authored=false;extract=true;Enqueue();extract=false;D::Tick(&session);CHECK(session.drops.size()==1);
    Reset();Enqueue();duringRead=[]{++serial;};D::Tick(&session);CHECK(session.drops.empty());
    for(bool atPublish:{false,true}) {
        Reset();Enqueue();
        auto& callback=atPublish?duringPublish:duringRead;
        callback=[]{D::Reset();Enqueue();};
        D::Tick(&session);CHECK(D::g_pending.size()==1);CHECK(session.drops.size()==(atPublish?1:0));
        D::Tick(&session);CHECK(D::g_pending.empty());CHECK(session.drops.size()==(atPublish?2:1));

        Reset();Enqueue();
        auto& nested=atPublish?duringPublish:duringRead;
        nested=[]{Enqueue();D::Tick(&session);};
        D::Tick(&session);CHECK(D::g_pending.size()==1);CHECK(session.drops.size()==1);
        D::Tick(&session);CHECK(D::g_pending.empty());CHECK(session.drops.size()==1);
    }
    Reset();Enqueue();duringPublish=[]{for(int i=0;i<40;++i)Enqueue();};D::Tick(&session);
    CHECK(D::g_pending.size()==32);CHECK(session.drops.size()==1);
    for(int i=0;i<16;++i)D::Tick(&session);
    CHECK(D::g_pending.empty());CHECK(session.drops.size()==1);CHECK(D::IsAwaitingHost(&actor));
    CHECK(D::FindAwaitingHost(L"item",L"prop_test_C")==&actor);
    CHECK(!D::FindAwaitingHost(L"other",L"prop_test_C"));
    CHECK(!D::FindAwaitingHost(L"item",L"prop_other_C"));
    hand=true;CHECK(!D::FindAwaitingHost(L"item",L"prop_test_C"));hand=false;
    tracked=true;D::Tick(&session);CHECK(!D::IsAwaitingHost(&actor));
    Reset();readable=false;readableAfter=32;for(int i=0;i<32;++i)Enqueue();
    duringRead=[]{for(int i=0;i<40;++i)Enqueue();};D::Tick(&session);
    CHECK(session.drops.empty());CHECK(D::g_pending.size()==32);
    for(int i=0;i<160;++i)D::Tick(&session);
    CHECK(session.drops.size()==1);CHECK(D::g_pending.empty());
    // Reusing the same address for a different actor must not inherit the pending-host latch.
    CHECK(D::IsAwaitingHost(&actor));++serial;CHECK(!D::IsAwaitingHost(&actor));
    CHECK(!D::FindAwaitingHost(L"item",L"prop_test_C"));
    Enqueue();D::Tick(&session);CHECK(session.drops.size()==2);
    Reset();readable=false;for(int i=0;i<40;++i)Enqueue();CHECK(D::g_pending.size()==32);
    session.linked=false;D::Tick(&session);CHECK(D::g_pending.empty());
    Reset();D::NoteClientKeyedDestroy(L"item");Enqueue();duringPublish=[]{++serial;};D::Tick(&session);
    CHECK(session.drops.size()==1);CHECK(D::g_parkedKeys.empty());CHECK(D::g_parkFifo.empty());
    // b2142: a bitten meal is captured AFTER local restoration and survives host construction.
    Reset();foodActor=true;authored=false;D::NoteClientKeyedDestroy(L"item");Enqueue();D::Tick(&session);
    CHECK(session.drops.size()==1);auto meal=session.drops[0];
    CHECK(meal.hasFoodCondition==1&&meal.foodUses==2&&meal.foodTemperature==300.f&&meal.foodRipeness==0.75f);
    session.mode=coop::net::Role::Host;
    CHECK(D::HostSpawnPlacedProp(meal,L"prop_test_C",L"item",1)==&actor);
    CHECK(foodWrites==1&&invalidates==1&&condition.uses==2&&condition.temperature==300.f&&condition.ripeness==0.75f);
    for(int failure=0;failure<6;++failure){
        Reset();foodActor=true;auto bad=meal;
        if(failure==0)bad.hasFoodCondition=0;
        if(failure==1)bad.hasFoodCondition=2;
        if(failure==2)bad.foodUses=100001;
        if(failure==3)bad.foodTemperature=std::nanf("");
        if(failure==4)foodActor=false;
        if(failure==5)foodWritable=false;
        CHECK(!D::HostSpawnPlacedProp(bad,L"prop_test_C",L"item",1));
        CHECK(destroys==1&&foodWrites==0&&invalidates==0);
    }
    Reset();foodActor=true;duringFinish=[]{++serial;};
    CHECK(!D::HostSpawnPlacedProp(meal,L"prop_test_C",L"item",1));CHECK(foodWrites==0&&invalidates==0);
    Reset();foodActor=true;duringFinish=[]{D::Reset();};
    CHECK(!D::HostSpawnPlacedProp(meal,L"prop_test_C",L"item",1));CHECK(foodWrites==0);
    Reset();foodActor=true;foodReadable=false;Enqueue();D::Tick(&session);
    CHECK(session.drops.empty()&&D::g_pending.size()==1);foodReadable=true;D::Tick(&session);CHECK(session.drops.size()==1);
    Reset();foodActor=true;D::NoteClientKeyedDestroy(L"item");Enqueue();session.sendOK=false;D::Tick(&session);
    CHECK(session.drops.empty()&&D::g_pending.size()==1&&D::g_parkedKeys.count(L"item")&&publishes==0);
    session.sendOK=true;D::Tick(&session);CHECK(session.drops.size()==1&&D::g_parkedKeys.empty());
    for(bool locationFailure:{true,false}) {
        Reset();D::NoteClientKeyedDestroy(L"item");Enqueue();
        if(locationFailure)locationOk=false;else rotationOk=false;
        D::Tick(&session);CHECK(D::g_pending.size()==1&&session.drops.empty()&&publishes==0);
        CHECK(D::g_parkedKeys.count(L"item"));
        locationOk=rotationOk=true;D::Tick(&session);
        CHECK(D::g_pending.empty()&&session.drops.size()==1&&D::g_parkedKeys.empty());
        D::Tick(&session);CHECK(session.drops.size()==1);
    }
    Reset();locationOk=false;Enqueue();
    for(int i=0;i<D::kMaxKeyTries+1;++i)D::Tick(&session);
    CHECK(D::g_pending.empty()&&session.drops.empty());
    Reset();locationOk=false;Enqueue();duringRead=[]{++serial;};D::Tick(&session);
    CHECK(D::g_pending.empty()&&session.drops.empty());
    Reset();locationOk=false;Enqueue();duringRead=[]{D::Reset();};D::Tick(&session);
    CHECK(D::g_pending.empty()&&session.drops.empty());
    // A parked placement must survive reconciliation while its first send is refused.
    Reset();authored=false;D::NoteClientKeyedDestroy(L"item");Enqueue();session.sendOK=false;
    CHECK(!D::IsPendingHostPlacement(&actor));D::Tick(&session);
    CHECK(D::IsPendingHostPlacement(&actor)&&!D::IsAwaitingHost(&actor));
    coop::join_membership_sweep::LocalBirths noAuthorship;
    CHECK(coop::join_membership_sweep::ProtectedLocalBirth(&actor,noAuthorship));
    session.linked=false;CHECK(!D::IsPendingHostPlacement(&actor));session.linked=true;
    ++serial;CHECK(!D::IsPendingHostPlacement(&actor));--serial;
    session.sendOK=true;D::Tick(&session);
    CHECK(!D::IsPendingHostPlacement(&actor)&&D::IsAwaitingHost(&actor));
    D::Reset();CHECK(!coop::join_membership_sweep::ProtectedLocalBirth(&actor,noAuthorship));
    // The enqueue queue alone is not evidence of a legitimate placement.
    Reset();authored=false;Enqueue();D::Tick(&session);
    CHECK(!D::IsPendingHostPlacement(&actor)&&session.drops.empty());
    Reset();authored=false;D::NoteClientKeyedDestroy(L"item");Enqueue();locationOk=false;
    D::Tick(&session);CHECK(D::IsPendingHostPlacement(&actor));
    for(int i=0;i<D::kMaxKeyTries;++i)D::Tick(&session);
    CHECK(!D::IsPendingHostPlacement(&actor));
    // The drain owns a separate batch. Native callbacks must still see its admitted
    // placement, while a reset invalidates that batch immediately.
    Reset();authored=false;D::NoteClientKeyedDestroy(L"item");Enqueue();
    duringRead=[&]{
        CHECK(D::g_pending.empty());
        CHECK(D::IsPendingHostPlacement(&actor));
        CHECK(coop::join_membership_sweep::ProtectedLocalBirth(&actor,noAuthorship));
        D::Tick(&session);CHECK(D::IsPendingHostPlacement(&actor)); // reentry does not clear outer scope
        ++serial;CHECK(!D::IsPendingHostPlacement(&actor));--serial;
        D::Reset();CHECK(!D::IsPendingHostPlacement(&actor));
    };
    D::Tick(&session);CHECK(session.drops.empty()&&!D::g_activePending);
    Reset();authored=false;D::NoteClientKeyedDestroy(L"item");Enqueue();locationOk=false;
    duringRead=[]{CHECK(D::IsPendingHostPlacement(&actor));};D::Tick(&session);
    CHECK(!D::g_activePending&&D::IsPendingHostPlacement(&actor));
    locationOk=true;D::Tick(&session);CHECK(session.drops.size()==1&&!D::g_activePending);
    // Explicit refusal is terminal but keeps the original throw protected from
    // reconciliation and the held-item fallback. An eventual host spawn adopts it.
    Reset();session.onDrop={};D::NoteClientKeyedDestroy(L"item");Enqueue();D::Tick(&session);
    CHECK(publishes==0&&claims==0&&D::g_parkedKeys.count(L"item"));
    auto request=session.drops.back();CHECK(request.requestId!=0);
    coop::net::PropPlacementResultPayload reply{request.requestId,static_cast<uint8_t>(coop::net::PropPlacementCode::Forbidden),{}};
    D::OnPlacementResult(session,reply,1);CHECK(D::IsAwaitingHost(&actor)); // only host may answer
    D::OnPlacementResult(session,reply,0);
    CHECK(D::StateFor(&actor)==D::PlacementState::Refused&&!D::IsAwaitingHost(&actor));
    CHECK(publishes==0); // refusal cannot overwrite the host's existing same-key record
    CHECK(D::IsLocalPlacement(&actor)&&D::g_parkedKeys.count(L"item"));
    CHECK(coop::join_membership_sweep::ProtectedLocalBirth(&actor,noAuthorship));
    CHECK(D::FindAwaitingHost(L"item",L"prop_test_C")==&actor);
    Enqueue();D::Tick(&session);CHECK(session.drops.size()==1); // cannot bypass refusal
    Reset();session.onDrop={};Enqueue();D::Tick(&session);request=session.drops.back();reply={request.requestId,0,{}};
    CHECK(publishes==0);
    D::OnPlacementResult(session,reply,0);CHECK(D::StateFor(&actor)==D::PlacementState::Accepted);
    CHECK(publishes==1);
    auto deadline=D::g_awaitingHost.front().until;
    D::OnPlacementResult(session,reply,0);CHECK(D::g_awaitingHost.front().until==deadline);
    D::g_awaitingHost.front().until=0;D::Tick(&session);
    CHECK(D::StateFor(&actor)==D::PlacementState::TimedOut&&!D::IsAwaitingHost(&actor));
    CHECK(D::FindAwaitingHost(L"item",L"prop_test_C")==&actor);
    tracked=true;D::Tick(&session);CHECK(!D::IsLocalPlacement(&actor));
    Reset();session.onDrop={};Enqueue();D::Tick(&session);request=session.drops.back();reply={request.requestId,0,{}};
    tracked=true;D::Tick(&session);CHECK(D::IsLocalPlacement(&actor)&&publishes==0);
    D::OnPlacementResult(session,reply,0);CHECK(publishes==1);D::Tick(&session);CHECK(!D::IsLocalPlacement(&actor));
    Reset();session.onDrop={};Enqueue();D::Tick(&session);request=session.drops.back();reply={request.requestId,0,{}};
    D::g_awaitingHost.front().until=0;D::Tick(&session);CHECK(D::StateFor(&actor)==D::PlacementState::TimedOut);
    D::OnPlacementResult(session,reply,0);CHECK(D::StateFor(&actor)==D::PlacementState::Accepted&&publishes==1);
    // Host outcomes, retries and reentrant duplicates execute native construction once.
    const auto hostRequest=request;
    Reset();session.mode=coop::net::Role::Host;
    D::OnPropDropIntent(session,hostRequest,1);CHECK(spawns==1&&brought==1);
    D::OnPropDropIntent(session,hostRequest,1);CHECK(spawns==1);
    session.sendOK=false;D::Tick(&session);CHECK(session.results.empty());
    session.sendOK=true;D::Tick(&session);CHECK(session.results.size()==1&&session.results[0].second.code==0);
    D::OnPropDropIntent(session,hostRequest,1);D::Tick(&session);CHECK(session.results.size()==2&&spawns==1);
    for(int failure=0;failure<5;++failure){
        Reset();session.mode=coop::net::Role::Host;auto p=hostRequest;
        if(failure==0)p.className.len=255;
        if(failure==1)p.locX=std::nanf("");
        if(failure==2)existing=&actor;
        if(failure==3)spawnOK=false;
        if(failure==4){D::OnReelEjectIntent(session,p,1);}else D::OnPropDropIntent(session,p,1);
        D::Tick(&session);CHECK(session.results.size()==1&&session.results[0].second.code!=0&&brought==0);
        CHECK(spawns==(failure==3?1:0));
    }
    Reset();session.mode=coop::net::Role::Host;
    duringFinish=[&]{D::OnPropDropIntent(session,hostRequest,1);};
    D::OnPropDropIntent(session,hostRequest,1);D::Tick(&session);
    CHECK(spawns==1&&session.results.size()==1);
    Reset();session.mode=coop::net::Role::Host;
    duringFinish=[&]{auto next=hostRequest;++next.requestId;D::OnPropDropIntent(session,next,2);};
    D::OnPropDropIntent(session,hostRequest,1);D::Tick(&session);CHECK(spawns==1&&session.results.size()==2);
    Reset();session.mode=coop::net::Role::Host;
    duringFinish=[]{D::OnPeerLeft(1);};D::OnPropDropIntent(session,hostRequest,1);D::Tick(&session);
    CHECK(session.results.empty()&&brought==0);
    D::OnPropDropIntent(session,hostRequest,1); // a new slot occupant starts its own request stream
    session.duringSend=[]{D::Reset();};D::Tick(&session);CHECK(D::g_replies[1].empty());
    Reset();session.mode=coop::net::Role::Host;
    // Evicted successes cannot be replayed as new births, even after the old actor disappeared.
    for(uint32_t id=1;id<=260;++id){auto p=hostRequest;p.requestId=id;D::OnPropDropIntent(session,p,1);D::Tick(&session);}
    CHECK(spawns==260&&D::g_replies[1].size()==256);
    auto replay=hostRequest;replay.requestId=1;D::OnPropDropIntent(session,replay,1);D::Tick(&session);
    CHECK(spawns==260&&session.results.back().second.code!=0);
    puts("PASS: placement refusal/timeout/late binding, host outcomes, duplicate/reentrant/replayed requests, send retry and disconnect");
    puts("PASS: admitted protection across active drain, callback reset/reentry and queue handoff");
    puts("PASS: admitted placement sweep protection during capture/send retry and expiry");
    puts("PASS: transform-read recovery, bounded refusal and stale/reset birth exclusion");
    puts("PASS: b2142 food capture, host restore, malformed values, callback lifetime and refused-send retry");
    puts("PASS: pending birth identity, callback enqueue/reset/reentry, admission, key wait and bounded queue");
    return 0;
}
