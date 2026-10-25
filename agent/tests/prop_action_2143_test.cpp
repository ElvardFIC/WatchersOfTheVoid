// Real client gate, host receiver and dispatch; Unreal reflection/transport are boundary doubles.
#include "support_2135.h"
#ifndef PROP_ACTION_SOURCE
#define PROP_ACTION_SOURCE "../../src/votv-coop/src/coop/props/prop_action_sync.cpp"
#endif
#include PROP_ACTION_SOURCE
#include <limits>
#include <set>
namespace A=coop::prop_action_sync;
namespace coop::net::session_serial {uint32_t testSerial=1;uint32_t Current(){return testSerial;}}
namespace coop::roster_ledger {
std::array<Row,4> testRows=[] {std::array<Row,4> rows{};for(int i=0;i<4;++i){rows[i].playerNo=uint16_t(10+i);rows[i].bornGeneration=1;}return rows;}();
const Row& Get(int slot){static Row empty;return slot>=0 && slot<4?testRows[slot]:empty;}
}
namespace test {
Obj damageFn{3},sliceFn{4},mreFn{5},spawnFn{6},mreOwner{7},localPlayer{12},sawFn{13};
int damageCalls=0, sliceCalls=0, mreCalls=0, dispatches=0, cues=0;
bool haveFunction=true, hitOk=true, parametersOk=true;
bool mre=true,handAxis=false;uint8_t appliedAction=0;
void* localHand=nullptr;
void* meshActor=nullptr;void* functionOwner=&mreOwner;
std::set<uint64_t> invocations{1,2};
std::wstring propKey=L"box";
std::function<void()> keyCallback,frameCallback,bindCallback,locationCallback,hitCallback,callCallback,publishCallback;
std::function<void()> argCallback,heldCallback;
std::wstring held=L"hammer";bool sawTool=false,finiteLocation=true;
bool getterBlocked=false,getterCallOk=true,getterReadOk=true;int getterCalls=0;
std::function<void()> getterCallback;void* frameTarget=nullptr;
void CallbackOnce(std::function<void()>& slot){auto fn=std::move(slot);slot={};if(fn)fn();}
bool safe=false,locked=true,publishOK=true;int loot=0,records=0;
ue_wrap::placed_save::Kind wallKind=ue_wrap::placed_save::Kind::None;
std::wstring wallKey=L"native_wall_17";
int wallHits=0;
void (*wallResolveCallback)()=nullptr;
float appliedDamage=0;void* appliedPlayer=nullptr;
struct Params {void* actor;float damage;ue_wrap::FVector impact;uint8_t action=4;};
int frameSize=sizeof(Params),damageOffset=offsetof(Params,damage),impactOffset=offsetof(Params,impact);
}
namespace ue_wrap::reflection {
std::wstring ClassNameOf(void*){return L"prop_test_child_C";}
// Regression: this child does not declare addDamage/slice itself.
void* FindFunction(void*,const wchar_t*){return nullptr;}
void* FindDispatchFunctionCached(void*,const wchar_t* name){
 ++test::dispatches;if(!test::haveFunction)return nullptr;
 if(std::wstring(name)==L"actionOptionIndex")return &test::mreFn;
 if(std::wstring(name)==L"spawn")return &test::spawnFn;
 if(std::wstring(name)==L"isNotSawable")return &test::sawFn;
 return std::wstring(name)==L"addDamage"?&test::damageFn:&test::sliceFn;
}
void* OuterOf(void*){return test::functionOwner;}
void* FindObject(const wchar_t*,const wchar_t*){return &test::actor;}
const FName& NameOf(void*){static FName n{};return n;}
bool NameEquals(const FName&,const wchar_t*){return test::mre;}
int32_t FunctionFrameSize(void*){return test::frameSize;}
int32_t FindParamOffset(void*,const wchar_t* name){std::wstring n=name;
 test::CallbackOnce(test::argCallback);
 if(n==L"actor"||n==L"player")return offsetof(test::Params,actor);
 if(n==L"action")return offsetof(test::Params,action);
 if(n==L"clean")return 0;
 if(n==L"damage")return test::damageOffset;
 if(n==L"impact")return test::impactOffset;return -1;
}
}
namespace ue_wrap::script_gate {bool WatchName(const wchar_t*,int,PreFn,PostFn){return true;}}
namespace ue_wrap::script_gate {bool IsInvocationActive(uint64_t id){return test::invocations.contains(id);}}
namespace ue_wrap::prop {
bool IsDescendantOfProp(void* a){return a==&test::actor&&test::wallKind==ue_wrap::placed_save::Kind::None;}
std::wstring GetKeyString(void*){const auto key=test::propKey;test::CallbackOnce(test::keyCallback);return key;}
void* GetStaticMesh(void*){return test::meshActor?test::meshActor:&test::actor;}
}
namespace ue_wrap::engine {
bool TryGetActorLocation(void*,FVector& p){p={1,2,3};if(!test::finiteLocation)p.X=std::numeric_limits<float>::quiet_NaN();test::CallbackOnce(test::locationCallback);return true;}
void PlaySoundAtLocation(void*,void*,const FVector&,void* attenuation,float volume,float pitch){
 CHECK(attenuation&&volume==1.f&&pitch==1.f);++test::cues;
}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn){if(fn==&test::sawFn)buf_.assign(1,0);if(!test::frameTarget || test::frameTarget==fn)test::CallbackOnce(test::frameCallback);}
bool ParamFrame::GetRaw(const wchar_t* name,void* out,int32_t size)const{
 CHECK(fn_==&test::sawFn && !wcscmp(name,L"return") && size==1);if(!test::getterReadOk)return false;
 std::memcpy(out,buf_.data(),1);return true;
}
bool ParamFrame::SetRaw(const wchar_t* name,const void* data,int32_t){
 test::CallbackOnce(test::bindCallback);
 if(!test::parametersOk)return false;
 if(std::wstring(name)==L"damage")std::memcpy(&test::appliedDamage,data,sizeof(float));
 if(std::wstring(name)==L"action")std::memcpy(&test::appliedAction,data,sizeof(uint8_t));
 if(std::wstring(name)==L"actor"||std::wstring(name)==L"player")std::memcpy(&test::appliedPlayer,data,sizeof(void*));return true;
}
bool Call(void* target,ParamFrame& f){CHECK(target==&test::actor);
 if(f.function()==&test::sawFn){++test::getterCalls;if(!test::getterCallOk)return false;std::memcpy(f.data(),&test::getterBlocked,1);test::CallbackOnce(test::getterCallback);return true;}
 if(f.function()==&test::damageFn)++test::damageCalls;
 else if(f.function()==&test::mreFn)++test::mreCalls;else ++test::sliceCalls;test::CallbackOnce(test::callCallback);return true;
}
}
namespace ue_wrap::hit_result {bool Write(ParamFrame&,const wchar_t*,void*,void*,const FVector&){test::CallbackOnce(test::hitCallback);return test::hitOk;}}
namespace ue_wrap::hit_result {bool WriteWhile(ParamFrame& f,const wchar_t* n,void* a,void* c,const FVector& p,const std::function<bool()>& current){return current() && Write(f,n,a,c,p) && current();}}
namespace ue_wrap::weapon_catalog {bool Lookup(const std::wstring& name,Swing& out){out={name==L"hammer",25.f};return true;}}
namespace coop::hand_item {
std::wstring HeldItem(uint8_t){const auto held=test::held;test::CallbackOnce(test::heldCallback);return held;}
bool LastHeldClassIs(uint8_t,const wchar_t*){const auto saw=test::sawTool;test::CallbackOnce(test::heldCallback);return saw;}
bool IsHandAxisActor(void*){return test::handAxis;}
void* LocalHandActor(){return test::localHand?test::localHand:test::handAxis?&test::actor:nullptr;}
}
namespace coop::prop_element_tracker {void* ResolveLiveActorByKey(const std::wstring& key,void*){return key==L"box"?&test::actor:nullptr;}}
namespace ue_wrap::placed_save {Kind Type(void* a){return a==&test::actor?test::wallKind:Kind::None;}}
namespace ue_wrap::wall_build {
std::wstring Key(void*){return test::wallKey;}
bool Damage(void*,float amount){++test::wallHits;test::appliedDamage=amount;return true;}
bool DamageWhile(void* a,float amount,const std::function<bool()>& current){return current() && Damage(a,amount);}
}
namespace ue_wrap::keyed_objects {void* Resolve(const wchar_t* key){if(test::wallResolveCallback)test::wallResolveCallback();return key==test::wallKey?&test::actor:nullptr;}}
namespace ue_wrap::safe {
bool IsSafe(void* a){return test::safe&&a==&test::actor;}
bool Unlock(void* a){if(!IsSafe(a))return false;if(test::locked){test::locked=false;++test::loot;}return true;}
}
namespace coop::prop_save_data {
bool Publish(net::Session*,void* a,const std::wstring& key){CHECK(a==&test::actor&&key==L"box");++test::records;test::CallbackOnce(test::publishCallback);return test::publishOK;}
bool PublishToSlot(net::Session* s,int,void* a,const std::wstring& k){return Publish(s,a,k);}
}
coop::net::Session session;
void Reset(){A::OnDisconnect();session={};++test::world;
 test::frameSize=sizeof(test::Params);test::damageOffset=offsetof(test::Params,damage);test::impactOffset=offsetof(test::Params,impact);
 test::damageCalls=test::sliceCalls=test::mreCalls=test::dispatches=0;test::haveFunction=test::hitOk=test::parametersOk=true;
 test::mre=true;test::handAxis=false;
 test::safe=false;test::locked=test::publishOK=true;test::loot=test::records=0;test::announced=true;
 test::cues=0;
 test::localHand=test::meshActor=nullptr;test::functionOwner=&test::mreOwner;test::propKey=L"box";test::invocations={1,2};
 test::keyCallback=test::frameCallback=test::bindCallback=test::locationCallback=test::hitCallback=test::callCallback=test::publishCallback={};
 test::argCallback=test::heldCallback={};test::held=L"hammer";test::sawTool=false;test::finiteLocation=true;
 test::getterBlocked=false;test::getterCallOk=test::getterReadOk=true;test::getterCalls=0;test::getterCallback={};test::frameTarget=nullptr;
 test::wallKind=ue_wrap::placed_save::Kind::None;test::wallHits=0;test::wallKey=L"native_wall_17";test::wallResolveCallback=nullptr;
 coop::element::reach=true;coop::players::Registry::Get().peer=1;
 coop::players::Registry::Get().local=&test::localPlayer;
 coop::players::Registry::Get().puppet.actor=&test::ui;A::Install(&session);
}
int main(){using namespace test;using V=ue_wrap::script_gate::Verdict;
 {
  using namespace coop::net;PacketHeader h{};WriteHeader(h,static_cast<MsgType>(0),1,7,1);
  MsgType type{};uint32_t seq=0,epoch=0;uint8_t slot=0;
  CHECK(kProtocolVersion>2146&&ParseHeader(&h,sizeof(h),type,seq,epoch,slot));
  CHECK(seq==1&&epoch==7&&slot==1);h.version=2146;
  CHECK(!ParseHeader(&h,sizeof(h),type,seq,epoch,slot));
 }
 Reset();session.mode=coop::net::Role::Client;
 Params args{coop::players::Registry::Get().Local(),20.f,{100,0,0}};
 ue_wrap::script_gate::Call c{};c.object=&actor;c.function=&damageFn;c.locals=reinterpret_cast<uint8_t*>(&args);c.invocation=1;
 CHECK(A::OnDamage(c)==V::Cancel);CHECK(session.sent.size()==1&&damageCalls==0);
 auto sent=session.sent.back();CHECK(sent.slot==0&&sent.kind==coop::net::ReliableKind::PropActionIntent);
 coop::net::PropActionIntentPayload p{};std::memcpy(&p,sent.data.data(),sizeof(p));
 session.mode=coop::net::Role::Host;A::OnReliable(p,1);
 CHECK(damageCalls==1&&appliedDamage==20.f&&appliedPlayer==&ui&&dispatches>0);
 A::RunSlice(&actor,true,[]{return true;});CHECK(sliceCalls==1);
 for(int failure=0;failure<6;++failure){Reset();auto bad=p;
  if(failure==0)bad.damage=1000.f;
  if(failure==1)bad.impactX=std::numeric_limits<float>::quiet_NaN();
  if(failure==2)coop::element::reach=false;
  if(failure==3)haveFunction=false;
  if(failure==4)hitOk=false;
  if(failure==5)parametersOk=false;
  A::OnReliable(bad,1);CHECK(damageCalls==0);
 }
 Reset();A::OnReliable(p,0);A::OnReliable(p,4);CHECK(damageCalls==0);
 Reset();session.mode=coop::net::Role::Client;session.sendOk=false;
 CHECK(A::OnDamage(c)==V::Cancel);CHECK(A::g_pending.size()==1);
 session.sendOk=true;A::Tick();CHECK(A::g_pending.empty()&&session.sent.size()==1);
 session.sendOk=false;CHECK(A::OnDamage(c)==V::Cancel);CHECK(A::g_pending.size()==1);
 ++world;session.sendOk=true;A::Tick();CHECK(A::g_pending.empty()&&session.sent.size()==1);
 session.sendOk=false;CHECK(A::OnDamage(c)==V::Cancel);A::OnDisconnect();CHECK(A::g_pending.empty());
 // Backpressure must retain the original hit order when the transport recovers
 // before the next drain. A newer action cannot jump ahead of the queued one.
 Reset();session.mode=coop::net::Role::Client;session.sendOk=false;args.damage=11.f;
 CHECK(A::OnDamage(c)==V::Cancel);session.sendOk=true;args.damage=22.f;
 CHECK(A::OnDamage(c)==V::Cancel&&session.sent.empty()&&A::g_pending.size()==2);
 A::Tick();CHECK(session.sent.size()==2&&A::g_pending.empty());
 for(int i=0;i<2;++i){coop::net::PropActionIntentPayload q{};
  std::memcpy(&q,session.sent[i].data.data(),sizeof(q));CHECK(q.damage==(i?22.f:11.f));}
 args.damage=20.f;
 for(int cause=0;cause<4;++cause){Reset();session.mode=coop::net::Role::Client;
  if(cause==0)damageOffset=sizeof(Params)-1;if(cause==1)impactOffset=-1;
  if(cause==2)impactOffset=sizeof(Params)-4;if(cause==3)frameSize=sizeof(void*);
  CHECK(A::OnDamage(c)==V::Cancel&&session.sent.empty()&&A::g_pending.empty());}
 Reset();session.mode=coop::net::Role::Client;c.function=&mreFn;args.action=4;
 CHECK(A::OnMreOpen(c)==V::Cancel);CHECK(session.sent.size()==1&&mreCalls==0);
 CHECK(cues==1);CHECK(A::OnMreOpen(c)==V::Cancel&&session.sent.size()==1&&cues==1);
 coop::net::PropActionIntentPayload open{};std::memcpy(&open,session.sent.back().data.data(),sizeof(open));
 CHECK(open.action==3);session.mode=coop::net::Role::Host;A::OnReliable(open,1);
 CHECK(mreCalls==1&&appliedAction==4&&appliedPlayer==&ui);
 for(int failure=0;failure<5;++failure){Reset();auto bad=open;
  if(failure==0)mre=false;
  if(failure==1)coop::element::reach=false;
  if(failure==2)bad.damage=1;
  if(failure==3)bad.impactX=1;
  if(failure==4)handAxis=true;
  A::OnReliable(bad,1);CHECK(mreCalls==0);
 }
 Reset();session.mode=coop::net::Role::Client;handAxis=true;
 CHECK(A::OnMreOpen(c)==V::Run&&session.sent.empty());handAxis=false;
 args.action=1;CHECK(A::OnMreOpen(c)==V::Run&&session.sent.empty());args.action=4;
 c.fromOurCode=true;CHECK(A::OnMreOpen(c)==V::Cancel&&session.sent.empty());
 c.fromOurCode=false;Reset();session.mode=coop::net::Role::Client;session.sendOk=false;
 CHECK(A::OnMreOpen(c)==V::Cancel&&A::g_pending.size()==1);
 handAxis=true;session.sendOk=true;A::Tick();CHECK(A::g_pending.empty()&&session.sent.empty());
 handAxis=false;session.sendOk=false;A::g_lastMreRequest.Reset();
 CHECK(A::OnMreOpen(c)==V::Cancel&&A::g_pending.size()==1);
 ++actor.serial;session.sendOk=true;A::Tick();CHECK(A::g_pending.empty()&&session.sent.empty());
 // Both native safe-success paths reach opened. No client loot; one authoritative unlock.
 Reset();safe=true;session.mode=coop::net::Role::Client;session.sendOk=false;
 CHECK(A::OnSafeOpened(c)==V::Cancel&&A::g_pending.size()==1&&!loot);
 CHECK(A::OnSafeOpened(c)==V::Cancel&&A::g_pending.size()==1); // queued duplicate coalesces
 session.sendOk=true;A::Tick();CHECK(A::g_pending.empty()&&session.sent.size()==1);
 coop::net::PropActionIntentPayload unlock{};std::memcpy(&unlock,session.sent.back().data.data(),sizeof(unlock));CHECK(unlock.action==4);
 session.mode=coop::net::Role::Host;A::OnReliable(unlock,1);A::OnReliable(unlock,1);CHECK(loot==1&&!locked);
 publishOK=false;A::Tick();CHECK(records==1&&!A::g_safePublish.empty());
 for(auto& row:A::g_safePublish)row.second.retryAt=0;publishOK=true;A::Tick();CHECK(records==2&&A::g_safePublish.empty());
 for(int failure=0;failure<5;++failure){Reset();safe=true;auto bad=unlock;
  if(failure==0)safe=false;if(failure==1)coop::element::reach=false;
  if(failure==2)bad.damage=1;if(failure==3)bad.key.len=255;if(failure==4)handAxis=true;
  A::OnReliable(bad,1);CHECK(!loot&&A::g_safePublish.empty());}
 Reset();safe=true;session.mode=coop::net::Role::Client;announced=false;CHECK(A::OnSafeOpened(c)==V::Run);
 announced=true;c.fromOurCode=true;CHECK(A::OnSafeOpened(c)==V::Cancel&&session.sent.empty());c.fromOurCode=false;
 session.mode=coop::net::Role::Host;A::AfterSafeOpened(c);CHECK(!A::g_safePublish.empty());++world;A::Tick();CHECK(A::g_safePublish.empty()&&!records);
 puts("PASS client hit -> host inherited event, slice dispatch, malformed/rejected requests, retry and world/session teardown");
 puts("PASS world MRE intent -> verified host native opening, wrong class/reach/parameters, hand and other-action exclusions");
 puts("PASS safe success: client loot suppression, host idempotent unlock, record retry, role/reach/class/frame/lifecycle guards");
 return 0;
}
