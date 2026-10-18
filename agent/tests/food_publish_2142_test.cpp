// Actual publisher; engine reads and reliable transport are the only boundaries used here.
#include "../../src/votv-coop/src/coop/props/prop_food_sync.cpp"
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace F=ue_wrap::food;
namespace S=coop::props::prop_food_sync;
F::Condition meal{300.f,0.75f,2};int actor;
namespace ue_wrap::food {bool Read(void*,Condition& out){out=meal;return true;}}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){return L"meal";}}
namespace coop::prop_element_tracker {element::ElementId GetPropElementIdForActor(void*){return 1;}}
// Unused engine surfaces fail loudly; no fake implementation of food synchronization.
namespace ue_wrap::reflection {
int32_t FindParamOffset(void*,const wchar_t*){std::abort();}
bool IsLive(void*){return true;}
const FName& NameOf(void*){std::abort();}
void* FindClass(const wchar_t*){std::abort();}
bool NameStartsWith(const FName&,const wchar_t*){std::abort();}
bool IsDescendantOfAny(void*,void* const*,size_t,int){std::abort();}
}
namespace ue_wrap::world_identity {void* CurrentWorld(){std::abort();}void* WorldOf(void*){std::abort();}}
namespace ue_wrap::engine {bool DestroyActor(void*){std::abort();}}
namespace coop::hand_item {bool IsHandAxisActor(void*){std::abort();}}
namespace coop::hand_item {
void* LocalHandActor(){std::abort();}void* MirrorActorForSlot(uint8_t){std::abort();}
uint32_t Generation(uint8_t){std::abort();}uint32_t MirrorGeneration(uint8_t){std::abort();}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return 1;}}
namespace coop::players {
Registry& Registry::Get(){std::abort();}
uint8_t Registry::LocalPeerId()const{return 0;}
}
namespace coop::prop_element_tracker {void* FindLiveActorByKey(const std::wstring&){return &actor;}}
namespace ue_wrap::food {bool IsFood(void*){return true;}bool Write(void*,const Condition& in){meal=in;return true;}}
namespace ue_wrap::object_index {
bool IsSeeded(){std::abort();}uint64_t ClassSetVersion(){std::abort();}
size_t ForEachInstance(void*,void(*)(void*,void*,int32_t),void*){std::abort();}
size_t ForEachClass(void(*)(void*,void*,void*),void*){std::abort();}
}
namespace ue_wrap::script_gate {bool WatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn){std::abort();}}
namespace coop::element {
IntentTarget IntentTarget::ForClientIntent(coop::net::Session&,uint8_t,float){std::abort();}
IntentSubject IntentTarget::Authorize(void*)const{std::abort();}
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
 puts("PASS: actual food publisher rebirth, late join, backpressure and lifecycle FIFO");
}
