// Actual publisher; engine reads and reliable transport are the only boundaries used here.
#include "../../src/votv-coop/src/coop/props/prop_food_sync.cpp"
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace F=ue_wrap::food;
namespace S=coop::props::prop_food_sync;
F::Condition meal{300.f,0.75f,2};int actor;
bool foodAlive=true;int foodSerial=1;
bool foodHand=false;
namespace ue_wrap::food {bool Read(void*,Condition& out){out=meal;return true;}}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){return L"meal";}}
namespace coop::prop_element_tracker {element::ElementId GetPropElementIdForActor(void*){return 1;}}
// Unused engine surfaces fail loudly; no fake implementation of food synchronization.
namespace ue_wrap::reflection {
int32_t FindParamOffset(void*,const wchar_t*){return 0;}
int32_t FunctionFrameSize(void*){return 1;}
int32_t InternalIndexOf(void*){return 1;}
int32_t SlotSerial(int32_t){return foodSerial;}
int32_t AllocateSlotSerial(int32_t){return foodSerial;}
bool IsLiveByIndex(void*,int32_t){return foodAlive;}
bool IsLive(void*){return foodAlive;}
const FName& NameOf(void*){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
void* FindClass(const wchar_t*){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
bool NameStartsWith(const FName&,const wchar_t*){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
bool IsDescendantOfAny(void*,void* const*,size_t,int){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
void* ClassOf(void*){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
void* OuterOf(void*){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
void* FindDispatchFunctionCached(void*,const wchar_t*){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
bool NameEquals(const FName&,const wchar_t*){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void*){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
bool ParamFrame::SetRaw(const wchar_t*,const void*,int32_t){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
bool Call(void*,ParamFrame&){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
}
namespace ue_wrap::world_identity {void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}
namespace ue_wrap::engine {bool DestroyActor(void*){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}}
namespace coop::hand_item {bool IsHandAxisActor(void*){return foodHand;}}
namespace coop::hand_item {
void* LocalHandActor(){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}void* MirrorActorForSlot(uint8_t){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
uint32_t Generation(uint8_t){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}uint32_t MirrorGeneration(uint8_t){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return 1;}}
namespace coop::players {
Registry& Registry::Get(){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
uint8_t Registry::LocalPeerId()const{return 0;}
}
namespace coop::prop_element_tracker {void* FindLiveActorByKey(const std::wstring&){return &actor;}}
namespace ue_wrap::food {bool IsFood(void*){return true;}bool Write(void*,const Condition& in){meal=in;return true;}}
namespace ue_wrap::object_index {
bool IsSeeded(){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}uint64_t ClassSetVersion(){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
size_t ForEachInstance(void*,void(*)(void*,void*,int32_t),void*){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
size_t ForEachClass(void(*)(void*,void*,void*),void*){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
}
namespace ue_wrap::script_gate {bool WatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}}
namespace coop::element {
IntentTarget IntentTarget::ForClientIntent(coop::net::Session&,uint8_t,float){std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
IntentSubject IntentTarget::Authorize(void*)const{std::fprintf(stderr,"unexpected %s\n",__FUNCTION__);std::exit(2);}
}
int main(){
 coop::net::Session session;
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
 uint8_t action=4;ue_wrap::script_gate::Call call{};call.object=&actor;call.function=&actor;call.locals=&action;
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
 CHECK(S::OnBoxOutput(call)==ue_wrap::script_gate::Verdict::Cancel);
 foodHand=true;CHECK(S::OnBoxOutput(call)==ue_wrap::script_gate::Verdict::Run);foodHand=false;
 session.mode=coop::net::Role::Host;CHECK(S::OnBoxOutput(call)==ue_wrap::script_gate::Verdict::Run);
 puts("PASS: actual food publisher rebirth, late join, backpressure and lifecycle FIFO");
}
