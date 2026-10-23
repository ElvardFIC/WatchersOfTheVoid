// Actual publisher; engine reads and reliable transport are the only boundaries used here.
#ifndef FOOD_CALLBACK_SOURCE
#define FOOD_CALLBACK_SOURCE "../../src/votv-coop/src/coop/props/prop_food_sync.cpp"
#endif
#include FOOD_CALLBACK_SOURCE
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <set>
#include "coop/element/player.h"
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace F=ue_wrap::food;
namespace S=coop::props::prop_food_sync;
F::Condition meal{300.f,0.75f,2};int actor;
bool foodAlive=true;int foodSerial=1;
bool foodHand=false;
bool foodMirror=false;
bool foodCooking=true;
bool foodResolvable=true,foodWritable=true,foodReadable=true;
void* foodLocalActor=nullptr;void* foodRemoteActor=nullptr;
uint32_t foodGeneration=1,foodMirrorGeneration=1,foodWorld=1;
uint8_t foodLocalSlot=0;
bool foodReach=true;
int foodWrites=0;
std::wstring foodKey=L"meal";
std::function<void()> foodAuthorize;
std::array<coop::roster_ledger::Row,4> foodRoster{};
bool foodNative=false;int foodFn=0,foodOwner=0,foodOutputs=0,foodDestroys=0;
int foodGraph=0,foodOtherType=0,foodFnSerial=1,foodOwnerSerial=1;
void* foodActorType=&foodOwner;void* foodFunctionOwner=&foodOwner;
uint32_t foodElementId=1;
bool foodAllowCensus=false;
std::wstring foodOutputClass;
std::function<void()> foodOutputCallback;
std::function<void()> foodReadCallback,foodWriteCallback,foodResolveCallback;
std::function<void()> foodKeyCallback,foodFrameCallback,foodBindCallback;
std::set<uint64_t> foodInvocations{1};
namespace ue_wrap::food {bool Read(void*,Condition& out){out=meal;auto fn=std::move(foodReadCallback);foodReadCallback={};if(fn)fn();return foodReadable;}}
namespace ue_wrap::cooking_food {bool IsCookingFood(void*){return foodCooking;}}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){const auto key=foodKey;auto fn=std::move(foodKeyCallback);foodKeyCallback={};if(fn)fn();return key;}}
namespace coop::prop_element_tracker {element::ElementId GetPropElementIdForActor(void*){return foodElementId;}}
// Unused engine surfaces fail loudly; no fake implementation of food synchronization.
namespace ue_wrap::reflection {
int32_t FindParamOffset(void*,const wchar_t*){return 0;}
int32_t FunctionFrameSize(void*){return 1;}
int32_t InternalIndexOf(void* p){return p==&foodFn?2:p==&foodOwner?3:p==&foodGraph?4:p==&foodOtherType?5:1;}
int32_t SlotSerial(int32_t i){return i==2?foodFnSerial:i==3?foodOwnerSerial:i==4 || i==5?1:foodSerial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t){return p && (p!=&actor || foodAlive);}
bool IsLive(void* p){return p && (p!=&actor || foodAlive);}
const FName& NameOf(void* p){CHECK(foodNative && p==&foodOwner);static FName name{1,0};return name;}
void* FindClass(const wchar_t* name){if(foodAllowCensus && !wcscmp(name,L"prop_food_C"))return nullptr;std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
bool NameStartsWith(const FName&,const wchar_t*){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
bool IsDescendantOfAny(void*,void* const*,size_t,int){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
void* ClassOf(void* p){CHECK(p==&actor || p==&foodOtherType);return p==&actor?foodActorType:&foodOwner;}
void* OuterOf(void* p){CHECK(foodNative && (p==&foodFn || p==&foodGraph));return foodFunctionOwner;}
void* FindDispatchFunctionCached(void* cls,const wchar_t* name){CHECK(foodNative && cls==&foodOwner && !wcscmp(name,L"eaten"));return &foodFn;}
bool NameEquals(const FName& name,const wchar_t* text){CHECK(foodNative && name.ComparisonIndex==1);return foodOutputClass==text;}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn){CHECK(foodNative && fn==&foodFn);auto cb=std::move(foodFrameCallback);foodFrameCallback={};if(cb)cb();}
bool ParamFrame::SetRaw(const wchar_t* name,const void* p,int32_t n){CHECK(foodNative && !wcscmp(name,L"player") && n==8 && !*static_cast<void* const*>(p));auto cb=std::move(foodBindCallback);foodBindCallback={};if(cb)cb();return true;}
bool Call(void* object,ParamFrame& f){CHECK(foodNative && object==&actor && f.function()==&foodFn);++foodOutputs;
    auto callback=std::move(foodOutputCallback);foodOutputCallback={};if(callback)callback();return true;}
}
namespace ue_wrap::world_identity {void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::engine {bool DestroyActor(void* object){CHECK(foodNative && object==&actor);++foodDestroys;foodAlive=false;return true;}}
namespace coop::hand_item {bool IsHandAxisActor(void*){return foodHand;}}
namespace coop::hand_item {bool IsMirrorActor(void*){return foodMirror;}}
namespace coop::hand_item {
void* LocalHandActor(){return foodLocalActor;}void* MirrorActorForSlot(uint8_t){return foodRemoteActor;}
uint32_t Generation(uint8_t){return foodGeneration;}uint32_t MirrorGeneration(uint8_t){return foodMirrorGeneration;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return foodWorld;}}
namespace coop::players {
Registry& Registry::Get(){static Registry r;return r;}
uint8_t Registry::LocalPeerId()const{return foodLocalSlot;}
}
namespace coop::roster_ledger {const Row& Get(int slot){static Row empty;return slot>=0 && slot<4?foodRoster[slot]:empty;}}
namespace coop::prop_element_tracker {void* FindLiveActorByKey(const std::wstring&){auto fn=std::move(foodResolveCallback);foodResolveCallback={};if(fn)fn();return foodResolvable?&actor:nullptr;}}
namespace ue_wrap::food {bool IsFood(void* a){return a!=nullptr;}bool Write(void*,const Condition& in,const std::function<bool()>& current){auto fn=std::move(foodWriteCallback);foodWriteCallback={};if(fn)fn();if(!foodWritable || (current && !current()))return false;++foodWrites;meal=in;return true;}}
namespace ue_wrap::object_index {
bool IsSeeded(){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}uint64_t ClassSetVersion(){CHECK(foodAllowCensus);return 1;}
size_t ForEachInstance(void*,void(*)(void*,void*,int32_t),void*){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
size_t ForEachClass(void(*)(void*,void*,void*),void*){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
}
namespace ue_wrap::script_gate {bool WatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}}
namespace ue_wrap::script_gate {bool WatchName(const wchar_t*,int,PreFn,PostFn){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}}
namespace ue_wrap::script_gate {bool IsInvocationActive(uint64_t id){return foodInvocations.contains(id);}}
namespace coop::element {
IntentTarget IntentTarget::ForClientIntent(coop::net::Session&,uint8_t,float){return {};}
IntentSubject IntentTarget::Authorize(void*)const{auto callback=std::move(foodAuthorize);foodAuthorize={};if(callback)callback();return {foodReach?IntentOutcome::Ok:IntentOutcome::OutOfReach};}
}
int main(){
 coop::net::Session session;
 S::g_session=&session;
 CHECK(S::PublishSpawnState(session,&actor,-1));CHECK(session.sent.size()==1&&session.sent.back().state.uses==2);
 CHECK(S::Publish(session,&actor,-1));CHECK(session.sent.size()==1); // no idle resend
 CHECK(S::PublishSpawnState(session,&actor,-1));CHECK(session.sent.size()==2); // same-key newborn must receive state
 meal.uses=1;CHECK(S::PublishSpawnState(session,&actor,2));CHECK(session.sent.size()==3);
 CHECK(S::Publish(session,&actor,-1));CHECK(session.sent.size()==4); // one joiner cannot consume everyone's update
 session.sendOk=false;CHECK(!S::PublishSpawnState(session,&actor,-1));session.sendOk=true;
 CHECK(S::Publish(session,&actor,-1));CHECK(session.sent.size()==5); // refused birth state stays dirty
 S::ForgetPublished(L"meal");CHECK(S::Publish(session,&actor,-1));CHECK(session.sent.size()==6);
 session.mode=coop::net::Role::Client;CHECK(!S::PublishSpawnState(session,&actor,-1));
 using namespace coop::net;
 CHECK(LaneForKind(ReliableKind::FoodState)==LaneForKind(ReliableKind::PropSpawn));
 CHECK(LaneForKind(ReliableKind::FoodBite)==LaneForKind(ReliableKind::PropDropIntent));
 CHECK(LaneForKind(ReliableKind::FoodState)==LaneForKind(ReliableKind::PropDestroy));
 S::g_session=&session;meal.uses=1;
 uint8_t action=4;ue_wrap::script_gate::Call call{};call.object=&actor;call.function=&actor;call.locals=&action;call.invocation=1;
 CHECK(S::OnAction(call)==ue_wrap::script_gate::Verdict::Run);
 foodAlive=false;S::OnActionPost(call);
 CHECK(session.bites.size()==1&&session.bites.back().key.len==4&&S::g_bite.empty());
 // The dead actor is never queried for its identity after the native final bite.
 foodAlive=true;S::g_clientBitePending.clear();meal.uses=2;
 CHECK(S::OnAction(call)==ue_wrap::script_gate::Verdict::Run);
 foodAlive=false;S::OnActionPost(call);CHECK(session.bites.size()==1);
 foodAlive=true;S::g_clientBitePending.clear();meal.uses=1;action=0;
 CHECK(S::OnAction(call)==ue_wrap::script_gate::Verdict::Run);S::OnActionPost(call);
 CHECK(session.bites.size()==1&&S::g_bite.empty());
 action=4;meal.uses=1;S::g_clientBitePending.clear();
 CHECK(S::OnAction(call)==ue_wrap::script_gate::Verdict::Run);
 session.sendOk=false;CHECK(S::ConsumesClientDestroy(&actor));
 foodAlive=false;S::OnActionPost(call);CHECK(session.bites.size()==1&&S::g_clientBiteRetry.size()==1);
 session.sendOk=true;S::g_nextPoll=S::NowMs()+100000;S::Tick();
 CHECK(session.bites.size()==2&&S::g_clientBiteRetry.empty()&&S::g_bite.empty());
 CHECK(!S::ConsumesClientDestroy(&actor)); // no unrelated destroy is swallowed
 foodAlive=true;S::g_clientBitePending.clear();
 CHECK(S::OnAction(call)==ue_wrap::script_gate::Verdict::Run);++foodSerial;S::OnActionPost(call);
 CHECK(session.bites.size()==2); // recycled pointer is not the consumed item
 CHECK(S::OnConsumedOutput(call)==ue_wrap::script_gate::Verdict::Cancel);
 foodNative=true;foodOutputClass=L"prop_foodBox_C";call.function=&foodFn;
 foodLocalActor=&actor;CHECK(S::OnConsumedOutput(call)==ue_wrap::script_gate::Verdict::Run);S::OnConsumedOutputPost(call);foodLocalActor=nullptr;
 foodMirror=true;CHECK(S::OnConsumedOutput(call)==ue_wrap::script_gate::Verdict::Cancel);
 session.mode=coop::net::Role::Host;CHECK(S::OnConsumedOutput(call)==ue_wrap::script_gate::Verdict::Cancel);foodMirror=false;
 CHECK(S::OnConsumedOutput(call)==ue_wrap::script_gate::Verdict::Run);
 foodNative=false;call.function=&actor;
 // Progress-only changes publish and survive live receive without cooking again.
 meal={350,.5f,2,12.f};S::ForgetPublished(L"meal");CHECK(S::Publish(session,&actor,-1));
 const auto progressState=session.sent.back().state;CHECK(progressState.cookProgress==12.f);
 const auto sentCount=session.sent.size();meal.cookProgress=12.1f;CHECK(S::Publish(session,&actor,-1));CHECK(session.sent.size()==sentCount+1);
 meal.cookProgress=13;CHECK(S::Publish(session,&actor,2));CHECK(S::Publish(session,&actor,-1));
 session.mode=coop::net::Role::Client;S::g_session=&session;S::g_world=1;
 meal.cookProgress=0;S::Apply(progressState);CHECK(meal.cookProgress==12);
 auto invalid=progressState;invalid.cookProgress=-.5f;S::Apply(invalid);CHECK(meal.cookProgress==12);
 invalid.cookProgress=NAN;S::Apply(invalid);CHECK(meal.cookProgress==12);
 invalid.cookProgress=1'000'001;S::Apply(invalid);CHECK(meal.cookProgress==12);
 // A state arriving before actor construction is retained, with the newest
 // progress replacing an older retry. A failed field apply remains retryable.
 foodResolvable=false;auto delayed=progressState;delayed.elementId=0;delayed.cookProgress=20;S::Apply(delayed);
 CHECK(S::g_pendingWorld.size()==1);delayed.cookProgress=25;S::Apply(delayed);CHECK(S::g_pendingWorld.size()==1);
 S::RetryWorldConditions();CHECK(meal.cookProgress==12&&S::g_pendingWorld.size()==1);
 foodResolvable=true;foodWritable=false;S::RetryWorldConditions();CHECK(S::g_pendingWorld.size()==1);
 foodWritable=true;S::RetryWorldConditions();CHECK(meal.cookProgress==25&&S::g_pendingWorld.empty());
 foodResolvable=false;S::Apply(delayed);S::OnDisconnect();CHECK(S::g_pendingWorld.empty());foodResolvable=true;S::g_session=&session;
 CHECK(S::OnTick(call)==ue_wrap::script_gate::Verdict::Cancel);
 CHECK(S::OnCook(call)==ue_wrap::script_gate::Verdict::Cancel);
 foodCooking=false;CHECK(S::OnCook(call)==ue_wrap::script_gate::Verdict::Run);foodCooking=true;
 foodHand=true;CHECK(S::OnTick(call)==ue_wrap::script_gate::Verdict::Run);
 foodMirror=true;CHECK(S::OnTick(call)==ue_wrap::script_gate::Verdict::Cancel);
 session.mode=coop::net::Role::Host;CHECK(S::OnTick(call)==ue_wrap::script_gate::Verdict::Cancel);
 foodMirror=false;foodHand=false;CHECK(S::OnTick(call)==ue_wrap::script_gate::Verdict::Run);
 // Hand samples can overtake HandItem/mirror construction. Keep only the latest
 // value, and fence it by the actual occupant and hand generation.
 S::OnDisconnect();session.mode=coop::net::Role::Client;S::g_session=&session;foodLocalSlot=1;
 for(int slot=0;slot<4;++slot)foodRoster[slot].playerNo=uint16_t(10+slot);
 foodGeneration=3;foodMirrorGeneration=0;foodLocalActor=foodRemoteActor=nullptr;
 auto hand=progressState;hand.elementId=S::HandAddress(2,4);hand.playerNo=12;hand.cookProgress=20;
 auto writes=foodWrites;S::Apply(hand);CHECK(S::g_pendingHand[2].have && foodWrites==writes);
 foodRemoteActor=&actor;foodMirrorGeneration=3;S::RetryHandConditions();CHECK(foodWrites==writes);
 foodGeneration=foodMirrorGeneration=4;foodWritable=false;S::RetryHandConditions();CHECK(S::g_pendingHand[2].have);
 hand.cookProgress=25;S::Apply(hand);CHECK(S::g_pendingHand[2].have && foodWrites==writes);
 foodWritable=true;S::RetryHandConditions();CHECK(!S::g_pendingHand[2].have && meal.cookProgress==25 && foodWrites==writes+1);
 foodGeneration=foodMirrorGeneration=5;hand.cookProgress=1;S::Apply(hand);CHECK(meal.cookProgress==25 && !S::g_pendingHand[2].have);
 foodRoster[2].playerNo++;S::Apply(hand);CHECK(!S::g_pendingHand[2].have && S::g_handKeys[2].empty());
 hand.playerNo=foodRoster[2].playerNo;foodGeneration=1;foodMirrorGeneration=0;foodRemoteActor=nullptr;
 hand.elementId=S::HandAddress(2,1);S::Apply(hand);CHECK(S::g_pendingHand[2].have);S::OnPeerLeft(2);CHECK(!S::g_pendingHand[2].have);
 // An unresolved local correction must not be overwritten by publication of
 // a freshly reconstructed hand actor's default condition.
 foodGeneration=6;hand.elementId=S::HandAddress(1,6);hand.playerNo=11;hand.cookProgress=30;
 S::Apply(hand);CHECK(S::g_pendingHand[1].have);meal.cookProgress=0;auto sends=session.sent.size();S::g_nextPoll=0;
 S::Tick();CHECK(S::g_pendingHand[1].have && meal.cookProgress==0 && session.sent.size()==sends);
 foodLocalActor=&actor;foodWritable=false;S::Tick();CHECK(S::g_pendingHand[1].have && session.sent.size()==sends);
 foodWritable=true;S::g_nextPoll=0;S::Tick();CHECK(!S::g_pendingHand[1].have && meal.cookProgress==30);
 CHECK(S::g_clientHandGeneration[L"meal"]==6 && session.sent.back().state.playerNo==11);
 foodKey=L"other";hand.cookProgress=35;writes=foodWrites;S::Apply(hand);CHECK(S::g_pendingHand[1].have && foodWrites==writes);
 foodKey=L"meal";S::RetryHandConditions();CHECK(!S::g_pendingHand[1].have && meal.cookProgress==35);
 auto malformed=hand;malformed.key.len=32;S::Apply(malformed);CHECK(!S::g_pendingHand[1].have);
 malformed=hand;malformed.elementId=0x80000000u;S::Apply(malformed);CHECK(meal.cookProgress==35);
 // Host acceptance also waits for the actual mirror, then publication retries
 // a failed send and distinguishes an equal-valued replacement generation.
 S::OnDisconnect();session.mode=coop::net::Role::Host;S::g_session=&session;foodLocalSlot=0;
 foodLocalActor=foodRemoteActor=nullptr;foodGeneration=7;foodMirrorGeneration=0;
 hand.elementId=S::HandAddress(1,8);hand.playerNo=11;hand.cookProgress=40;
 S::OnHandState(hand,2);CHECK(!S::g_pendingHand[1].have);S::OnHandState(hand,1);CHECK(S::g_pendingHand[1].have);
 foodRemoteActor=&actor;foodGeneration=foodMirrorGeneration=8;session.sendOk=false;S::RetryHandConditions();
 CHECK(!S::g_pendingHand[1].have && meal.cookProgress==40 && !S::g_handSent[1].sent && S::g_handPublished[1].generation==0);
 session.sendOk=true;foodAllowCensus=true;S::g_nextPoll=0;S::Tick();foodAllowCensus=false;
 CHECK(S::g_handPublished[1].generation==8); // the production poll retries a failed remote-hand broadcast
 sends=session.sent.size();S::BroadcastHandState(session,&actor,1,8);CHECK(session.sent.size()==sends);
 foodGeneration=foodMirrorGeneration=9;CHECK(!S::BroadcastHandState(session,&actor,1,9) && session.sent.size()==sends);
 hand.elementId=S::HandAddress(1,9);hand.cookProgress=40;S::OnHandState(hand,1);CHECK(session.sent.size()==sends+1);
 meal.cookProgress=45;S::BroadcastHandState(session,&actor,1,9,2);S::BroadcastHandState(session,&actor,1,9);
 CHECK(session.sent.size()==sends+3 && S::g_handSent[1].cookProgress==45);
 hand.elementId=S::HandAddress(1,9);hand.cookProgress=100;foodReach=false;writes=foodWrites;
 S::OnHandState(hand,1);CHECK(foodWrites==writes && !S::g_pendingHand[1].have);foodReach=true;
 session.ready=false;S::OnHandState(hand,1);CHECK(foodWrites==writes);session.ready=true;
 // A newer sample accepted reentrantly during reach validation wins over the
 // suspended older sample; a teardown cannot resume a captured actor write.
 foodGeneration=foodMirrorGeneration=10;hand.elementId=S::HandAddress(1,10);hand.cookProgress=50;
 auto latest=hand;latest.cookProgress=60;foodAuthorize=[&]{S::OnHandState(latest,1);};
 S::OnHandState(hand,1);CHECK(S::g_pendingHand[1].have && foodWrites==writes);S::RetryHandConditions();
 CHECK(meal.cookProgress==60 && foodWrites==writes+1 && !S::g_pendingHand[1].have);
 writes=foodWrites;foodAuthorize=[]{S::OnDisconnect();};S::OnHandState(hand,1);CHECK(foodWrites==writes && !S::g_session);
 S::g_session=&session;foodRemoteActor=nullptr;S::OnHandState(hand,1);CHECK(S::g_pendingHand[1].have);
 ++foodWorld;S::EnsureWorld();CHECK(!S::g_pendingHand[1].have);S::OnDisconnect();
 // A failed join unicast must survive even when the ordinary broadcast
 // baseline is already current. Retry reads the latest value, never a snapshot.
 S::g_session=&session;S::EnsureWorld();meal={300,.5f,4,22};foodAlive=true;foodHand=false;foodRemoteActor=nullptr;
 session.sendOk=false;S::QueueFoodJoin(&actor,2);S::RetryFoodJoins();CHECK(S::g_joinFoods.size()==1);
 meal.cookProgress=23;session.sendOk=true;sends=session.sent.size();S::RetryFoodJoins();
 CHECK(S::g_joinFoods.empty() && session.sent.size()==sends+1 && session.sent.back().slot==2 && session.sent.back().state.cookProgress==23);
 CHECK(!S::g_sent[L"meal"].sent);CHECK(S::Publish(session,&actor,-1));
 S::QueueFoodJoin(&actor,2);foodHand=true;S::RetryFoodJoins();CHECK(S::g_joinFoods.empty());foodHand=false;
 S::QueueFoodJoin(&actor,2);S::QueueFoodJoin(&actor,3);S::OnPeerLeft(2);CHECK(S::g_joinFoods.at(L"meal").pending==(1u<<3));
 S::OnPeerLeft(3);CHECK(S::g_joinFoods.empty());
 S::QueueHandFoodJoin(1,2);S::RetryFoodJoins();CHECK(S::g_handJoin[1] && !foodRemoteActor);
 foodRemoteActor=&actor;foodGeneration=foodMirrorGeneration=12;S::g_handKeys[1]=L"meal";S::g_handAccepted[1]={12,11,L"meal"};S::g_handSent[1]={};
 session.sendOk=false;S::RetryFoodJoins();CHECK(S::g_handJoin[1]);session.sendOk=true;S::RetryFoodJoins();
 CHECK(!S::g_handJoin[1] && session.sent.back().slot==2 && session.sent.back().state.playerNo==11 && !S::g_handSent[1].sent);
 S::OnDisconnect();
 // Only output-only native eaten overrides replay on the host. Player poison,
 // hunger, coffee effects and unrelated achievements never execute here.
 session.mode=coop::net::Role::Host;S::g_session=&session;foodNative=true;foodRemoteActor=foodLocalActor=nullptr;
 coop::net::FoodBitePayload bite{};coop::net::WireKeyFromString(L"meal",bite.key);bite.elementId=1;
 for(const auto* cls:{L"prop_foodBox_C",L"prop_food_banana_C",L"prop_food_coffeeCup_C",L"prop_food_poison_C"}) {
     foodOutputClass=cls;meal={300,.5f,1};foodAlive=true;S::g_biteAt.clear();auto outputs=foodOutputs,retired=foodDestroys;
     S::OnBite(bite,1);CHECK(foodDestroys==retired+1 && meal.uses==0);
     CHECK(foodOutputs==outputs+(foodOutputClass!=L"prop_food_poison_C"));
 }
 foodAlive=true;meal.uses=1;S::g_biteAt.clear();foodOutputClass=L"prop_food_banana_C";
 auto retired=foodDestroys;foodOutputCallback=[]{S::OnDisconnect();};S::OnBite(bite,1);CHECK(foodDestroys==retired && !S::g_session);
 S::g_session=&session;foodAlive=true;meal.uses=1;S::g_biteAt.clear();writes=foodWrites;
 foodAuthorize=[]{S::OnDisconnect();};S::OnBite(bite,1);CHECK(foodWrites==writes && !S::g_session);
 puts("PASS: actual food publisher rebirth, late join, backpressure and lifecycle FIFO");
 puts("PASS: cooking progress, progress-only/join publication, receive bounds and world/hand/mirror cooking authority");
 puts("PASS: hand condition readiness, generation/occupancy/key fencing, replacement identity, host retry, latest/reentrant cancellation");
 puts("PASS: canonical box/banana/cup final output, personal-effect exclusion and authority/output teardown guards");
 puts("PASS: latest world/hand join retry, unicast/broadcast independence, handoff and recipient cleanup");
 return 0;
}
