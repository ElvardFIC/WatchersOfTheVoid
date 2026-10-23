// Actual finish-spawn enqueue and Tick. Engine/object-array/network boundaries are doubled.
#define NDEBUG
#include <intrin.h>
#include "../../src/votv-coop/src/coop/props/prop_drop_intent.cpp"
#include "ue_wrap/core/cached_obj_ref.h"
#include "coop/props/local_birth_protection.h"
#include "coop/element/player.h"
#include "nonentity_facts_support.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace D=coop::prop_drop_intent;
namespace coop::water_pour_sync {bool ObserveBirth(void*,void*){return false;}}
namespace test {
int actor=0,world=0,nextWorld=0,serial=1,publishes=0,claims=0;
int keyReads=0,readableAfter=100000;
void* currentWorld=&world;
bool live=true,authored=true,extract=false,hand=false,mirror=false,tracked=false,readable=true;
bool floorActor=false,mintOk=true;
bool locationOk=true,rotationOk=true;
std::wstring className=L"prop_test_C";
int mints=0;
int begins=0,expresses=0,notices=0;
int expectedRecords=0;
int authoredSlices=0,awaitedSlices=0;
std::function<void()> duringSlice;
bool keySetOK=true,finishOK=true;
bool parityOK=true,scaleOK=true;
void* indexed=nullptr;
coop::prop_key_index::Read indexRead=coop::prop_key_index::Read::Miss;
coop::roster_ledger::Row roster[4];
bool foodActor=false,foodReadable=true,foodWritable=true,finished=false;
int destroys=0,foodWrites=0,invalidates=0;
ue_wrap::food::Condition condition{300.f,0.75f,2};
std::function<void()> duringFinish;
std::function<void()> duringBegin;
std::function<void()> duringFoodWrite,duringApplyParked;
std::function<void()> duringKey;
std::function<void()> duringRead,duringPublish;
coop::net::Session session;
void Enqueue(){D::OnClientFinishSpawn(nullptr,nullptr,&actor);}
void Reset(){currentWorld=&world;D::Reset();session.drops.clear();session.linked=true;D::g_session=&session;
    locationOk=rotationOk=true;
    className=L"prop_test_C";
    live=authored=readable=true;extract=hand=mirror=tracked=false;serial=1;currentWorld=&world;
    foodActor=false;foodReadable=foodWritable=true;finished=false;destroys=foodWrites=invalidates=0;condition={300.f,0.75f,2};duringFinish={};session.sendOK=true;session.mode=coop::net::Role::Client;
    publishes=claims=keyReads=mints=begins=expresses=notices=0;floorActor=false;mintOk=keySetOK=finishOK=parityOK=scaleOK=true;duringKey={};
    indexed=nullptr;indexRead=coop::prop_key_index::Read::Miss;
    session.replies.clear();session.ready=true;session.blockedSlot=-1;session.duringSend={};
    session.onDrop=[](const auto& p){D::OnPlacementResult(D::PlacementReply(p,coop::net::PropPlacementCode::Accepted),0);};
    duringBegin={};duringFoodWrite={};duringApplyParked={};expectedRecords=0;
    authoredSlices=awaitedSlices=0;duringSlice={};
    roster[1].playerNo=2;roster[1].bornGeneration=1;
    readableAfter=100000;duringRead={};duringPublish={};}
void Callback(std::function<void()>& slot){auto fn=std::move(slot);slot={};if(fn)fn();}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace coop::props::container_birth {
void NoteAuthoredBirth(void*){++test::authoredSlices;test::Callback(test::duringSlice);}
void ExpectBirthSlice(void*,uint8_t){++test::awaitedSlices;test::Callback(test::duringSlice);}
}
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
namespace ue_wrap::world_identity {uint32_t Generation(){return test::currentWorld==&test::world?1:2;}void* WorldOf(void*){return &test::world;}void* CurrentWorld(){return test::currentWorld;}}
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
bool Write(void*,const Condition& in,const std::function<bool()>& current) {CHECK(test::finished);test::Callback(test::duringFoodWrite);if(!test::foodWritable || (current && !current()))return false;test::condition=in;++test::foodWrites;return true;}
}
namespace coop::props::prop_food_sync {void ForgetPublished(const std::wstring& key){CHECK(key==L"item");++test::invalidates;}}
// Unused host/installation boundaries must never accidentally run in this client-side test.
namespace coop::prop_lifecycle {bool IsWireSuppressedPropClass(const std::wstring&){return false;}bool IsPerPlayerPropClass(const std::wstring&){return false;}}
namespace coop::prop_save_data {
void ExpectRecordFor(void*,const std::wstring&,uint8_t){++test::expectedRecords;}
bool ApplyParked(void*,const std::wstring&){std::abort();}
bool ApplyInitialParked(void*,const std::wstring&,uint8_t author){CHECK(author==1);test::Callback(test::duringApplyParked);return false;}
}
namespace coop::prop_key_index {Read Resolve(const std::wstring&,void** out){*out=test::indexed;return test::indexRead;}}
namespace coop::prop_snapshot {void ExpressIncrementalSpawn(void*){++test::expresses;}}
namespace coop::peer_action_feed {void AnnounceDirect(uint8_t,Action,const std::wstring&){++test::notices;}}
namespace coop::players {
Registry& Registry::Get(){static auto* r=new Registry;return *r;}
uint8_t Registry::LocalPeerId()const{return 1;}
}
namespace coop::roster_ledger {const Row& Get(int slot){return test::roster[slot>=0&&slot<4?slot:0];}}
namespace coop::prop_element_tracker {void* ResolveLiveActorByKey(const std::wstring&,bool*){std::abort();}}
namespace coop::physmods_sync {bool HostShouldReapModuleBirth(uint8_t,void*){std::abort();}}
namespace coop::server_upgrade_sync {bool HostShouldReapUpgradeBirth(uint8_t){std::abort();}}
namespace coop::drive_payload_sync {void NoteClientBrought(void*,uint8_t){}}
namespace ue_wrap::reflection {void* FindClass(const wchar_t*){return &test::actor;}void* FindFunction(void*,const wchar_t*){return &test::actor;}}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t*){return &test::actor;}}
namespace ue_wrap::engine {
void* BeginDeferredSpawn(void*,const FVector&,const FRotator&){++test::begins;test::Callback(test::duringBegin);return &test::actor;}
bool FinishDeferredSpawn(void*,const FVector&,const FRotator&){test::condition={290.f,1.f,5};test::finished=true;test::Callback(test::duringFinish);return test::finishOK;}
bool DestroyActor(void*){++test::destroys;return true;}
bool SetActorScale3D(void*,const FVector&){return test::scaleOK;}
}
namespace ue_wrap {ParamFrame::ParamFrame(void* fn):fn_(fn){}bool ParamFrame::SetRaw(const wchar_t*,const void*,int32_t){return test::keySetOK;}bool Call(void*,ParamFrame&){return test::keySetOK;}}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring&){return {1,0};}}
namespace ue_wrap::prop {bool IsClassDescendantOfProp(void*){return true;}bool WriteSpParityIdentity(void*,reflection::FName,bool,bool,bool,bool){return test::parityOK;}}
namespace ue_wrap::ufunction_hook {bool InstallPostHook(const facts::Resolved&,void(*)(void*,void*,void*),bool){std::abort();}}
int main(){using namespace test;
    Reset();Enqueue();D::Tick(&session);CHECK(authoredSlices==1&&publishes==1);
    D::Tick(&session);CHECK(authoredSlices==1);
    const auto containerRequest=session.drops.front();
    Reset();session.onDrop=[](const auto& p){D::OnPlacementResult(D::PlacementReply(p,coop::net::PropPlacementCode::Refused),0);};
    Enqueue();D::Tick(&session);CHECK(authoredSlices==0&&publishes==0);
    Reset();session.sendOK=false;Enqueue();D::Tick(&session);CHECK(authoredSlices==0&&publishes==0);
    Reset();duringSlice=[]{D::Reset();};Enqueue();D::Tick(&session);CHECK(authoredSlices==1&&publishes==0);
    Reset();session.mode=coop::net::Role::Host;
    duringApplyParked=[]{CHECK(awaitedSlices==1);};
    CHECK(D::HostSpawnPlacedProp(containerRequest,L"prop_test_C",L"item",1)==&actor);
    CHECK(awaitedSlices==1&&expectedRecords==1);
    Reset();session.mode=coop::net::Role::Host;duringSlice=[]{D::Reset();};
    CHECK(!D::HostSpawnPlacedProp(containerRequest,L"prop_test_C",L"item",1));CHECK(expectedRecords==0);
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
    Reset();foodActor=true;condition.cookProgress=12.5f;authored=false;D::NoteClientKeyedDestroy(L"item");Enqueue();D::Tick(&session);
    CHECK(session.drops.size()==1);auto meal=session.drops[0];
    CHECK(meal.hasFoodCondition==1&&meal.foodUses==2&&meal.foodTemperature==300.f&&meal.foodRipeness==0.75f&&meal.foodCookProgress==12.5f);
    session.mode=coop::net::Role::Host;
    CHECK(D::HostSpawnPlacedProp(meal,L"prop_test_C",L"item",1)==&actor);
    CHECK(foodWrites==1&&invalidates==1&&condition.uses==2&&condition.temperature==300.f&&condition.ripeness==0.75f&&condition.cookProgress==12.5f);
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
    // Failed captures retain old work while callbacks fill the arrival queue.
    Reset();locationOk=false;for(int n=0;n<32;++n)Enqueue();
    duringRead=[]{for(int n=0;n<40;++n)Enqueue();};D::Tick(&session);
    CHECK(D::g_pending.size()==D::kMaxPending);
    puts("PASS: container slice hooks run only after accepted placement, before host record restore, and stop after teardown");
    puts("PASS: admitted protection across active drain, callback reset/reentry and queue handoff");
    puts("PASS: admitted placement sweep protection during capture/send retry and expiry");
    puts("PASS: transform-read recovery, bounded refusal and stale/reset birth exclusion");
    puts("PASS: b2142 food capture, host restore, malformed values, callback lifetime and refused-send retry");
    puts("PASS: pending birth identity, callback enqueue/reset/reentry, admission, key wait and bounded queue");
    return 0;
}
