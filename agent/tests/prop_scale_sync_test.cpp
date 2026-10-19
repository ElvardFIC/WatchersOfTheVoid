#include "support_2135.h"
#include "../../src/votv-coop/src/coop/props/prop_scale_sync.cpp"
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include <limits>
namespace test {
ue_wrap::FVector scale{1,1,1};int writes=0;
bool present=true,held=false,accepted=true;uint16_t generation=7;
}
namespace ue_wrap::engine {
FVector GetActorScale3D(void*){return test::scale;}
bool SetActorScale3D(void*,const FVector& value){test::scale=value;++test::writes;test::Callback();return true;}
bool ReadMainPlayerGrabState(void*,MainPlayerGrabState& state){state={};if(test::held)state.grabbingActor=&test::actor;return true;}
}
namespace ue_wrap::prop {
bool IsDescendantOfProp(void* a){return a==&test::actor;}
std::wstring GetInteractableKeyString(void*){return L"scale-item";}
}
namespace coop::prop_element_tracker {
void* FindLiveActorByKey(const std::wstring& key){return test::present&&key==L"scale-item"?&test::actor:nullptr;}
element::ElementId GetPropElementIdForActor(void*){return 42;}
}
namespace coop::remote_prop {
void* DrivenActor(int slot){return slot==1&&test::accepted?&test::actor:nullptr;}
uint16_t DrivenHoldGeneration(int){return test::generation;}
}
namespace coop::gravity_motion {void* Target(){return nullptr;}}
int main(){
 using namespace test;namespace S=coop::prop_scale_sync;
 CHECK(!coop::net::IsClientRelayableReliableKind(coop::net::ReliableKind::PropScaleState));
 CHECK(coop::net::LaneForKind(coop::net::ReliableKind::PropScaleState)==coop::net::LaneForKind(coop::net::ReliableKind::PropRelease));
 coop::net::Session s;
 coop::net::PropScaleStatePayload p{};
 coop::net::WireKeyFromString(L"scale-item",p.key);p.elementId=42;p.holdGen=7;p.x=2;p.y=3;p.z=4;
 S::Receive(s,p,0);S::Receive(s,p,4);S::Tick(s,nullptr,0);CHECK(writes==0&&s.sent.empty());
 accepted=false;S::Receive(s,p,1);S::Tick(s,nullptr,0);CHECK(writes==0);
 accepted=true;generation=8;S::Tick(s,nullptr,0);CHECK(writes==0);
 generation=7;S::Tick(s,nullptr,0);CHECK(writes==1&&scale.Z==4&&s.sent.size()==1);
 S::Receive(s,p,1);++s.peerEpoch;S::Tick(s,nullptr,0);CHECK(!S::requests[1].until);
 p.x=5;S::Receive(s,p,1);callback=[]{S::Reset();};auto sent=s.sent.size();S::Tick(s,nullptr,0);
 CHECK(s.sent.size()==sent); // no use of a pending row after native reset
 S::Receive(s,p,1);callback=[&]{auto newer=p;newer.x=6;S::Receive(s,newer,1);};scale.X=1;
 S::Tick(s,nullptr,0);CHECK(S::requests[1].packet.x==6);
 S::Tick(s,nullptr,0);CHECK(scale.X==6&&!S::requests[1].until);
 s.mode=coop::net::Role::Client;S::Reset();writes=0;s.sent.clear();present=false;
 S::Receive(s,p,2);CHECK(S::canonicals.empty());
 S::Receive(s,p,0);S::Tick(s,nullptr,0);CHECK(writes==0&&!S::canonicals.empty());
 present=true;S::Tick(s,nullptr,0);CHECK(writes==1&&S::canonicals.empty());
 held=true;S::Receive(s,p,0);S::Tick(s,nullptr,0);CHECK(writes==1&&S::canonicals.empty());held=false;
 auto bad=p;bad.x=0;S::Receive(s,bad,0);bad=p;bad.y=std::numeric_limits<float>::quiet_NaN();S::Receive(s,bad,0);
 bad=p;bad.key.len=255;S::Receive(s,bad,0);CHECK(S::canonicals.empty());
 S::Tick(s,&actor,7);CHECK(s.sent.size()==1&&s.sent.back().slot==0);
 scale.X=8;S::Tick(s,nullptr,0);CHECK(s.sent.size()==2);
 coop::net::PropScaleStatePayload final{};std::memcpy(&final,s.sent.back().data.data(),sizeof(final));CHECK(final.x==8);
 S::Receive(s,p,0);scale.X=1;callback=[]{S::Reset();};S::Tick(s,nullptr,0);CHECK(S::canonicals.empty());
 puts("PASS scale host holder/generation/occupancy validation, no blind relay, late actor, local hold, malformed data, reentry and final scale flush");
}
