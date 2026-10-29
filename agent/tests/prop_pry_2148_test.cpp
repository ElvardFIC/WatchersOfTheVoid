// Actual pry observer/drain/replay; only engine and transport boundaries are doubled.
#include "support_2135.h"
#include "../../src/votv-coop/src/coop/props/prop_stick_sync.cpp"
#include "facts_test_support.h"
namespace ue_wrap::facts {Resolved ResolveOn(const Function&,void*){return {};}}
namespace S=coop::prop_stick_sync;
namespace test {
bool statiq=true,frozen=false,sleeping=false,owned=false,pose=true;
int setters=0,moves=0,parks=0; bool setterOk=true,destroyOnSet=false;
int localHold=0;bool handAxis=false;
int syncerOverrides=0;
}
namespace ue_wrap::reflection {
void* FindFunction(void*,const wchar_t*){return nullptr;}
int32_t FindParamOffset(void*,const wchar_t*){return -1;}
int32_t FunctionFrameSize(void*){return 0;}
bool CallFunction(void*,void*,void*){return false;}
bool IsDescendantOfAny(void*,void* const*,size_t,int){return false;}
}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t*){return nullptr;}}
namespace ue_wrap::game_thread {
bool IsDefinitelyOffGameThread(){return false;}
bool RegisterPostObserver(const facts::Resolved&,ProcessEventObserverFn){return false;}
}
namespace ue_wrap::hot_path {
unsigned long long CallerRva(const void*){return 0;}
unsigned long CurrentThreadId(){return 0;}
}
namespace ue_wrap::script_gate {
bool WatchName(const wchar_t*,int,PreFn,PostFn){return true;}
bool Watch(const facts::Resolved&,int,PreFn,PostFn){return false;}
}
namespace ue_wrap::prop {
bool IsDescendantOfProp(void* a){return a==&test::actor;}
bool IsStatic(void*){return test::statiq;}bool IsFrozen(void*){return test::frozen;}
bool IsSleeping(void*){return test::sleeping;}
bool CallBaseSetPropProps(void*,bool st,bool fr,bool sl){
 ++test::setters;if(!test::setterOk)return false;
 test::statiq=st;test::frozen=fr;test::sleeping=sl;
 if(test::destroyOnSet)test::actor.live=false;return true;
}
void WriteStatic(void*,bool){}void WriteFrozen(void*,bool){}
std::wstring GetInteractableKeyString(void*){return L"pried";}
void* GetStaticMesh(void*){return &test::actor;}
VelocityState GetPhysicsVelocity(void*,bool){return {};}
}
namespace ue_wrap::engine {
bool TryGetActorLocation(void*,FVector& p){p={12,34,56};return test::pose;}
bool TryGetActorRotation(void*,FRotator& p){p={};return test::pose;}
bool SetActorLocation(void*,const FVector&){++test::moves;return true;}
bool SetActorRotation(void*,const FRotator&){return true;}
bool SetActorSimulatePhysics(void*,bool simulate){if(!simulate)++test::parks;return true;}
bool SetComponentLinearVelocity(void*,float,float,float){return true;}
bool SetComponentAngularVelocity(void*,float,float,float){return true;}
bool IsMainPlayerGrabbing(void*,void*){return test::owned;}
bool ReadMainPlayerGrabState(void*,MainPlayerGrabState& s){
 s={};if(test::localHold==1)s.grabbingActor=&test::actor;
 if(test::localHold==3)s.holdingActor=&test::actor;return true;
}
}
namespace coop::prop_element_tracker {
element::ElementId GetPropElementIdForActor(void*){return element::kInvalidId;}
void* ResolveLiveActorByKey(const std::wstring&,bool*){return &test::actor;}
}
namespace coop::prop_key_index {void* Find(const std::wstring&){return &test::actor;}}
namespace coop::local_streams {void* LastHeldActor(){return nullptr;}uint16_t CurrentHoldGen(){return 0;}}
namespace coop::remote_prop {
bool IsActorUnderAnyDrive(void*){return test::owned;}
void CloseHold(int,uint16_t){}void ClearAnyDriveFor(void*){}
std::wstring KeyToWString(const coop::net::WireKey&){return L"pried";}
}
namespace coop::prop_drive_stream {bool IsParked(void*){return false;}}
namespace coop::prop_drive_host {
void OverrideClientSyncer(void* actor,const char*) { CHECK(actor==&test::actor);++test::syncerOverrides; }
}
namespace coop::gravity_motion {void* Target(){return test::localHold==2?&test::actor:nullptr;}}
namespace coop::hand_item {bool IsHandAxisActor(void*){return test::handAxis;}}
int main(){using namespace test;
 coop::net::Session session;S::g_session=&session;
 auto& cls=S::g_compOffsets[&actor];cls.cls.Set(&actor);cls.off=-1;
 ue_wrap::script_gate::Call c{};c.object=&actor;
 CHECK(S::OnPryPre(c)==ue_wrap::script_gate::Verdict::Run);
 S::Tick();CHECK(session.sent.empty()); // native refusal must not free the other copy
 S::OnPryPre(c);S::OnPryPre(c);CHECK(S::g_pendingCount==1);
 statiq=false;S::Tick();CHECK(session.sent.size()==1);
 coop::net::PropStickStatePayload p{};
 std::memcpy(&p,session.sent.back().data.data(),sizeof(p));
 CHECK(p.flags==0&&p.key.len==5&&p.locX==12);
 statiq=true;S::OnStickState(p,1);
 CHECK(!statiq&&setters==1&&moves==1&&syncerOverrides==1);
 owned=true;statiq=true;S::ApplyUnstick(&actor,p,L"pried",1);
 CHECK(!statiq&&moves==1&&parks==1); // keep a network-owned body parked, without teleporting
 for(int mode:{1,2,3}){
  localHold=mode;statiq=true;S::ApplyUnstick(&actor,p,L"pried",1);
  CHECK(!statiq&&moves==1&&parks==1); // a fresh local hold wins over a stale drive row
 }localHold=0;
 handAxis=true;statiq=true;S::OnStickState(p,1);CHECK(statiq);handAxis=false;
 setterOk=false;statiq=true;S::ApplyUnstick(&actor,p,L"pried",1);
 CHECK(statiq&&moves==1);setterOk=true;owned=false;
 S::ConvergeStuck(&actor,coop::net::propspawn_flags::kLiveState);
 CHECK(!statiq); // join's existing non-component copy no longer remains static
 S::ConvergeStuck(&actor,coop::net::propspawn_flags::kLiveState|coop::net::propspawn_flags::kStatic);
 CHECK(statiq);
 cls.off=offsetof(Obj,clipboard);const int untouched=setters;
 CHECK(!S::ReplayUnstick(&actor)&&setters==untouched);cls.off=-1;
 auto bad=p;bad.holdGen=1;S::OnStickState(bad,1);CHECK(statiq);
 bad=p;bad.flags=8;S::OnStickState(bad,1);CHECK(statiq);
 destroyOnSet=true;const int before=moves;S::ApplyUnstick(&actor,p,L"pried",1);
 CHECK(moves==before);actor.live=true;destroyOnSet=false;
 statiq=true;S::OnPryPre(c);statiq=false;session.sendOk=false;
 S::Tick();CHECK(S::g_pendingCount==1);session.sendOk=true;S::Tick();
 CHECK(S::g_pendingCount==0);
 statiq=true;S::OnPryPre(c);statiq=false;session.sendOk=false;S::Tick();
 S::g_pending[0].queuedAt=S::NowMs()-3000;S::Tick();CHECK(S::g_pendingCount==0);
 session.sendOk=true;
 const auto count=session.sent.size();
 statiq=true;S::OnPryPre(c);++actor.serial;statiq=false;S::Tick();CHECK(session.sent.size()==count);
 statiq=true;S::OnPryPre(c);++world;statiq=false;S::Tick();CHECK(session.sent.size()==count);
 statiq=true;++S::g_replayDepth;S::OnPryPre(c);CHECK(S::g_pendingCount==0);--S::g_replayDepth;
 puts("PASS actual crowbar outcome drain, refusal, dedupe, remote ownership, static join and recycled/world lifetime");
}
