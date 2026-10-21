#include "held_identity.inc"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Actor {int serial=1;bool live=true;} actor;
std::wstring key=L"item";
uint32_t world=1;
coop::element::Element* element=nullptr;
coop::element::ElementId eid=coop::element::kInvalidId;
int publishes=0,enrollments=0;
std::function<void()> onKey;
}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a==&test::actor&&test::actor.live;}
bool IsLiveByIndex(void* a,int32_t i){return i==1&&IsLive(a);}
int32_t InternalIndexOf(void*){return 1;}
int32_t SlotSerial(int32_t){return test::actor.serial;}
int32_t AllocateSlotSerial(int32_t){return test::actor.serial;}
std::wstring ClassNameOf(void*){return L"prop_food_C";}
void* ClassOf(void* a){return a;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void*){return CurrentWorld();}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::prop {
bool IsGarbageClump(void*){return false;}
bool IsDescendantOfProp(void*){return true;}
bool IsKeyedInteractable(void*){return true;}
std::wstring GetInteractableKeyString(void*){auto f=std::move(test::onKey);test::onKey={};if(f)f();return test::key;}
std::wstring GetPropNameString(void*){return L"food";}
uint8_t GetChipType(void*){return 0;}
}
namespace ue_wrap::engine {
bool TryGetActorLocation(void*,FVector&){return true;}
bool TryGetActorRotation(void*,FRotator&){return true;}
FVector GetActorScale3D(void*){return {1,1,1};}
}
namespace ue_wrap::drive_chain {bool IsDriveClass(void*){return false;}}
namespace coop::element {
Registry::Registry(){}
Registry::~Registry(){}
Registry& Registry::Get(){static Registry registry;return registry;}
Element* Registry::Get(ElementId id)const{return id==test::eid?test::element:nullptr;}
ElementId Registry::EidForActor(void* a)const{return a==&test::actor?test::eid:kInvalidId;}
bool Registry::RegisterMirror(ElementId id,Element* e){e->SetMirror_(true);e->SetId_(id);return true;}
Element::Element(ElementType type):m_type(type){}
Element::~Element(){}
void Element::SetActor(void* a,int32_t i){m_actor=a;m_internalIdx=i;}
void* Element::LiveActor()const{return ue_wrap::reflection::IsLiveByIndex(m_actor,m_internalIdx)?m_actor:nullptr;}
}
namespace coop::prop_drop_intent {bool IsAwaitingHost(void*){return false;}bool IsRefusedPlacement(void*){return false;}}
namespace coop::kerfur_entity {bool IsKerfurActor(void*){return false;}}
namespace coop::join_membership_sweep {
bool IsPendingSweepCandidate(void*){return false;}
bool HasLoadTailQuiesced(){return true;}
bool IsInDivergenceUniverseUnclaimed(void*){return false;}
void RecordClaimIfTracking(void*){}
}
namespace coop::prop_element_tracker {
element::ElementId GetPropElementIdForActor(void*){return test::element&&!test::element->IsMirror()?test::eid:element::kInvalidId;}
void MarkProcessedInit(void*){}
std::wstring MarkPropElement(void*,const std::wstring& key,const std::wstring&,EnrollSource){++test::enrollments;test::eid=43;return key;}
}
namespace coop::prop_save_data {
bool Publish(net::Session*,void*,const std::wstring&){++test::publishes;return true;}
bool PublishWithSpawn(net::Session*,void*,const net::WireKey&,int){++test::publishes;return true;}
}
namespace coop::prop_synth_key {std::wstring EnsureKeyForBroadcast(void*,const std::wstring& key,bool){return key;}}
namespace coop::prop_wire_parity {uint8_t PhysFlagsOf(void*){return 0;}}
namespace coop::drive_payload_sync {void NoteOwnDrive(void*){}}
int main(){
    using namespace test;
    namespace E=coop::element;
    coop::net::Session session;
    E::Element mirror(E::ElementType::Prop);mirror.SetActor(&actor,1);
    E::Registry::Get().RegisterMirror(42,&mirror);element=&mirror;eid=42;
    // Mirrors have no locals-only tracker id. Neither a keyed nor a keyless
    // mirror may be authored again or send a stale record back to its owner.
    for(const auto& value:{std::wstring(L"item"),std::wstring(),std::wstring(L"None")}){
        key=value;CHECK(!coop::trash_collect_sync::EnsureHeldItemBroadcast(&actor,&session));
        CHECK(!session.births&&!publishes&&!enrollments);
    }
    E::Element local(E::ElementType::Prop);local.SetActor(&actor,1);element=&local;
    key=L"item";session.mode=coop::net::Role::Host;
    CHECK(coop::trash_collect_sync::EnsureHeldItemBroadcast(&actor,&session));
    CHECK(publishes==1&&!session.births&&!enrollments);
    // The key getter can travel, recycle the actor or disconnect the session.
    for(int cause=0;cause<3;++cause){
        onKey=[&,cause]{if(cause==0)++world;if(cause==1)++actor.serial;if(cause==2)session.linked=false;};
        CHECK(!coop::trash_collect_sync::EnsureHeldItemBroadcast(&actor,&session));
        CHECK(publishes==1&&!session.births&&!enrollments);session.linked=true;
    }
    element=nullptr;eid=E::kInvalidId;
    CHECK(coop::trash_collect_sync::EnsureHeldItemBroadcast(&actor,&session));
    CHECK(session.births==1&&publishes==2&&enrollments==1);
    puts("PASS held identity: keyed/keyless mirrors refuse duplicate birth and stale save; owned state publishes; new actors enroll; key callback lifetime guarded");
}
