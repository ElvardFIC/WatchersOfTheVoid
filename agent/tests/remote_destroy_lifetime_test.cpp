// Exercise the production receiver with native/reflection boundaries doubled.
#include "coop/element/player.h"
#include "../../src/votv-coop/src/coop/props/remote_prop_destroy.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test {
struct Obj { int id=0,serial=1; bool live=true,asset=false; } objects[5];
uint32_t world=1,session=1; int calls=0,releases=0,clears=0,marks=0,unpins=0,heard=0,lateHeard=0,armed=0;
void* resolved=nullptr; bool host=true,quiesced=true,echo=false,trash=false;
std::function<void()> listener,release,clear,unpin,findFunction;
void Run(std::function<void()>& cb) { auto f=std::move(cb);cb={};if(f)f(); }
void Hear(int,void*) { ++heard;Run(listener); }
void LateHear(int,void*) { ++lateHeard; }
coop::net::PropDestroyPayload Payload() {
 coop::net::PropDestroyPayload p{};p.elementId=7;p.key.len=5;std::memcpy(p.key.data,"rifle",5);return p;
}
void Reset() {
 for(int i=0;i<5;++i){objects[i]={};objects[i].id=i;}objects[0].asset=objects[1].asset=true;
 world=session=1;calls=releases=clears=marks=unpins=heard=lateHeard=armed=0;
 resolved=&objects[2];host=quiesced=true;echo=trash=false;
 listener=release=clear=unpin=findFunction={};
 coop::remote_prop::g_destroyHeard.clear();coop::remote_prop::g_actorCls.Reset();coop::remote_prop::g_destroyActorFn=nullptr;
 coop::remote_prop::AddDestroyListener(&Hear);
}
void Apply() { coop::remote_prop::OnDestroy(Payload(),&objects[3],1); }
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<5&&p==&test::objects[i]&&IsLive(p);}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
std::wstring ClassNameOf(void*){return L"prop_rifle_C";}
void* FindClass(const wchar_t*){return &test::objects[0];}
void* FindFunction(void*,const wchar_t*){test::Run(test::findFunction);return &test::objects[1];}
bool CallFunction(void* p,void*,void*){CHECK(p==&test::objects[2]);++test::calls;return true;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* p){return p&&static_cast<test::Obj*>(p)->asset?nullptr:CurrentWorld();}
}
namespace coop::net::session_serial {uint32_t Current(){return test::session;}}
namespace ue_wrap::game_thread {bool IsDefinitelyOffGameThread(){return false;}}
namespace ue_wrap::hot_path {unsigned long long CallerRva(const void*){return 0;}unsigned long CurrentThreadId(){return 1;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::prop {bool IsChipPile(void*){return test::trash;}bool IsGarbageClump(void*){return false;}}
namespace ue_wrap::engine {bool ReleaseMainPlayerGrabIfHolding(void*,void*){++test::releases;test::Run(test::release);return true;}}
namespace coop::element {Element::~Element()=default;}
namespace coop::kerfur_entity {void ForgetKerfurPropMirror(void*){std::abort();}}
namespace coop::remote_prop {
void* ResolveLiveActorByEid(uint32_t){return test::resolved;}
std::wstring KeyToWString(const net::WireKey& k){return {k.data,k.data+k.len};}
void ClearAnyDriveFor(void*){++test::clears;test::Run(test::clear);}
}
namespace coop::prop_key_index {
void* Find(const std::wstring&){return test::resolved;}
Read Resolve(const std::wstring&,void** out){*out=test::resolved;return *out?Read::Hit:Read::Miss;}
}
namespace coop::prop_echo_suppress {
void MarkIncomingDestroy(void*){++test::marks;}
bool ConsumeArbiterConsumedKey(const std::wstring&){return test::echo;}
}
namespace coop::prop_element_tracker {bool SessionIsHost(){return test::host;}}
namespace coop::join_membership_sweep {bool HasLoadTailQuiesced(){return test::quiesced;}}
namespace coop::element::quiescence_drain {void ArmPendingDestroy(const net::PropDestroyPayload&){++test::armed;}}
namespace coop::trash_channel {void ClearClientCarry(uint32_t){std::abort();}}
namespace coop::trash_mirror {
void Retire(element::ElementId,bool){std::abort();}
void Unpin(void*){++test::unpins;test::Run(test::unpin);}
}
namespace coop::players {
Registry& Registry::Get(){static Registry r;return r;}
void* Registry::Local(){return &test::objects[3];}
}
int main(){
 using namespace test;
 Reset();Apply();CHECK(calls==1&&heard==1&&releases==1&&marks==1&&unpins==1);
 Reset();listener=[] {++objects[2].serial;};Apply();CHECK(calls==0&&releases==0&&marks==0);
 Reset();listener=[] {objects[2].live=false;};Apply();CHECK(calls==0&&releases==0);
 Reset();listener=[] {++world;};Apply();CHECK(calls==0&&releases==0);
 Reset();listener=[] {++session;};Apply();CHECK(calls==0&&releases==0);
 Reset();release=[] {++objects[2].serial;};Apply();CHECK(calls==0&&marks==0&&unpins==0);
 Reset();clear=[] {++objects[2].serial;};Apply();CHECK(calls==0&&releases==0);
 Reset();findFunction=[] {++objects[2].serial;};Apply();CHECK(calls==0&&releases==0);
 Reset();unpin=[] {++objects[2].serial;};Apply();CHECK(calls==0&&marks==0);
 Reset();unpin=[] {++session;};Apply();CHECK(calls==0&&marks==0);
 Reset();listener=[] {Apply();};Apply();CHECK(calls==1&&heard==1);
 Reset();release=[] {Apply();};Apply();CHECK(calls==1&&heard==1&&releases==1);
 Reset();listener=[] {coop::remote_prop::AddDestroyListener(&LateHear);};Apply();CHECK(calls==1&&lateHeard==0);
 Apply();CHECK(calls==2&&lateHeard==1);
 Reset();resolved=nullptr;Apply();CHECK(!calls&&!armed);
 host=false;quiesced=false;Apply();CHECK(!calls&&armed==1);
 Reset();coop::remote_prop::TryApplyDestroy(Payload());CHECK(calls==1&&heard==0);
 Reset();unpin=[] {++objects[2].serial;};coop::remote_prop::ConsumeLocalActor(&objects[2]);CHECK(calls==0&&marks==0);
 Reset();findFunction=[] {++world;};coop::remote_prop::ConsumeLocalActor(&objects[2]);CHECK(calls==0&&marks==0);
 Reset();clear=[] {++objects[2].serial;};coop::remote_prop::DestroyEchoSuppressed(&objects[2]);CHECK(calls==0&&marks==0);
 Reset();coop::remote_prop::ConsumeLocalActor(&objects[2]);CHECK(calls==1&&marks==1);
 Reset();coop::remote_prop::DestroyEchoSuppressed(&objects[2]);CHECK(calls==1&&marks==1&&clears==1);
 std::puts("PASS remote destroy: native success, listener/resolve/drive/release/unpin identity changes, world/session replacement, recursion, listener registration and load-tail preservation");
}
