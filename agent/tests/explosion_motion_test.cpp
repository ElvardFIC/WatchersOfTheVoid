#include "../../src/votv-coop/src/coop/props/explosion_prop_motion.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace S=coop::explosion_prop_motion;
namespace test {
struct Obj { int id=0,serial=1; bool live=true; void* outer=nullptr; } objects[1040];
auto& actor=objects[1];auto& blast=objects[2];auto& component=objects[3];auto& second=objects[4];
auto& otherBlast=objects[5];auto& otherComponent=objects[6];auto& function=objects[7];
uint32_t world=1,serial=1;
void *held=nullptr,*hand=nullptr,*fixed=nullptr,*frozen=nullptr,*trash=nullptr,*notProp=nullptr;
bool hooks=true,worldReady=true,foreignWorld=false;
ue_wrap::ufunction_hook::PostNativeCallback impulse=nullptr;
std::vector<void*> coasted;std::function<void()> callback;
std::vector<void*> queried;std::function<void()> queryCallback;bool queryOk=true;int queries=0;
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<1040&&p==&test::objects[i]&&IsLive(p);}
int32_t SlotSerial(int32_t i){return i>=0&&i<1040?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* OuterOf(void* p){return p?static_cast<test::Obj*>(p)->outer:nullptr;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return test::worldReady?(void*)(uintptr_t)test::world:nullptr;}
void* WorldOf(void* p){return p==&test::function?nullptr:(test::foreignWorld?(void*)999:CurrentWorld());}
}
namespace ue_wrap::log { void Write(Level,const char*,...){} }
namespace ue_wrap::facts {
Function::Function(detail::MacroToken,const char* id,const char* feature,const wchar_t* cls,const wchar_t* fn,Mode mode)
 : ident_(id),feature_(feature),cls_(cls),fn_(fn),mode_(mode),next_(nullptr){}
Resolved Resolve(const Function& row){return test::hooks?Resolved(&test::function,&row):Resolved{};}
}
namespace ue_wrap::ufunction_hook {
bool InstallPostHook(const facts::Resolved& fn,PostNativeCallback cb,bool){CHECK(fn.Fn()==&test::function);test::impulse=cb;return test::hooks;}
}
namespace ue_wrap::radial_impulse {
bool CollectActors(void*,std::vector<CachedObjRef>& out,const std::function<bool()>& current){
 ++test::queries;for(void* actor:test::queried){CachedObjRef ref;ref.Set(actor);out.push_back(ref);}
 auto cb=std::move(test::queryCallback);test::queryCallback={};if(cb)cb();return test::queryOk&&current();
}
}
namespace ue_wrap::prop {
bool IsDescendantOfProp(void* p){return p&&p!=test::notProp;}
bool IsStatic(void* p){return p&&p==test::fixed;}
bool IsFrozen(void* p){return p&&p==test::frozen;}
bool IsGarbageClump(void* p){return p&&p==test::trash;}
bool IsTrashActor(void*){return false;}bool IsTrashBitsPile(void*){return false;}
}
namespace coop::gravity_motion { void* Target(){return nullptr;} }
namespace ue_wrap::engine {
bool ReadMainPlayerGrabState(void*,MainPlayerGrabState& state){state={};state.grabbingActor=test::held;return true;}
}
namespace coop::prop_drive_host {
bool InAHand(void* p){return p&&p==test::hand;}
void Coast(void* p,const char*,bool){test::coasted.push_back(p);auto cb=std::move(test::callback);test::callback={};if(cb)cb();}
}
namespace coop::net::session_serial { uint32_t Current(){return test::serial;} }
coop::net::Session session;
void Reset(coop::net::Role role=coop::net::Role::Host){using namespace test;
 S::OnDisconnect();++world;++serial;for(int i=0;i<1040;++i)objects[i]={i};
 component.outer=&blast;otherComponent.outer=&otherBlast;
 held=hand=fixed=frozen=trash=notProp=nullptr;hooks=worldReady=true;foreignWorld=false;
 coasted.clear();callback={};session={};session.mode=role;S::Install(&session);
 queried.clear();queryCallback={};queryOk=true;queries=0;
}
void Fire(void* c=&test::component){CHECK(test::impulse);test::impulse(c,nullptr,nullptr);}
int main(){using namespace test;using coop::net::Role;
 Reset();S::Observe(&actor,&blast);S::Observe(&actor,&blast);CHECK(S::g_size==1);S::Tick();CHECK(coasted.empty());
 // Any number of ticks may pass while the latent delay is paused. Another
 // source's impulse cannot start this source's victims early.
 for(int i=0;i<100;++i)S::Tick();Fire(&otherComponent);S::Tick();CHECK(coasted.empty());
 S::ObserveBlast(&otherBlast);Fire(&otherComponent);S::Tick();CHECK(coasted.empty());
 Fire();CHECK(coasted.empty());blast.live=false;S::Tick();CHECK(coasted.size()==1&&coasted[0]==&actor&&!S::g_size);
 // The source may destroy itself immediately after its real impulse.
 S::Tick();CHECK(coasted.size()==1);
 Reset();S::Observe(&actor,&blast);Fire();S::Tick();S::Observe(&actor,&blast);Fire();S::Tick();CHECK(coasted.size()==2);
 Reset();S::Observe(&actor,&blast);S::Observe(&second,&otherBlast);Fire();S::Tick();CHECK(coasted.size()==1&&S::g_size==2);
 Fire(&otherComponent);S::Tick();CHECK(coasted.size()==2&&coasted[1]==&second);
 for(void** flag:{&held,&hand,&fixed,&frozen,&trash,&notProp}){
  Reset();*flag=&actor;S::Observe(&actor,&blast);Fire();S::Tick();CHECK(coasted.empty());
  Reset();S::Observe(&actor,&blast);*flag=&actor;Fire();S::Tick();CHECK(coasted.empty());
  Reset();S::Observe(&actor,&blast);*flag=&actor;S::Tick();*flag=nullptr;Fire();S::Tick();CHECK(coasted.empty());
 }
 Reset();S::Observe(&actor,&blast);++actor.serial;Fire();S::Tick();CHECK(coasted.empty());
 Reset();S::Observe(&actor,&blast);++blast.serial;Fire();S::Tick();CHECK(coasted.empty());
 Reset();S::Observe(&actor,&blast);blast.live=false;Fire();S::Tick();CHECK(coasted.empty());
 Reset();S::Observe(&actor,&blast);++world;Fire();S::Tick();CHECK(coasted.empty()&&!S::g_size);
 Reset();S::Observe(&actor,&blast);++serial;Fire();S::Tick();CHECK(coasted.empty()&&!S::g_size);
 Reset();S::Observe(&actor,&blast);S::OnDisconnect();S::Install(&session);Fire();S::Tick();CHECK(coasted.empty());
 Reset(Role::Client);S::Observe(&actor,&blast);Fire();S::Tick();CHECK(coasted.empty()&&!S::g_size);
 Reset();session.linked=false;S::Observe(&actor,&blast);Fire();S::Tick();CHECK(coasted.empty());
 Reset();worldReady=false;S::Observe(&actor,&blast);Fire();S::Tick();CHECK(coasted.empty());
 Reset();foreignWorld=true;S::Observe(&actor,&blast);Fire();S::Tick();CHECK(coasted.empty());
 Reset();S::Observe(&actor,&blast);S::Observe(&second,&blast);Fire();callback=[]{S::OnDisconnect();};S::Tick();CHECK(coasted.size()==1);
 Reset();S::Observe(&actor,&blast);S::Observe(&second,&blast);Fire();callback=[]{++serial;};S::Tick();CHECK(coasted.size()==1);
 Reset();S::Observe(&actor,&blast);Fire();callback=[]{S::Observe(&second,&blast);Fire();S::Tick();};S::Tick();
 CHECK(coasted.size()==1);S::Tick();CHECK(coasted.size()==2&&coasted[1]==&second);
 Reset();for(int i=10;i<1039;++i)S::Observe(&objects[i],&blast);CHECK(S::g_size==1024);Fire();S::Tick();CHECK(coasted.size()==1024&&!S::g_size);
 Reset();S::g_hook.Reset();hooks=false;S::Observe(&actor,&blast);S::Tick();CHECK(!S::g_size);hooks=true;S::Tick();S::Observe(&actor,&blast);Fire();S::Tick();CHECK(coasted.size()==1);
 // Zero damage never visits Observe(actor,blast). The host's accepted birth
 // still lets the actual component query enroll its native overlap targets.
 Reset();queried={&actor};Fire();S::Tick();CHECK(!queries&&coasted.empty());
 S::ObserveBlast(&blast);Fire();CHECK(queries==1&&coasted.empty());S::Tick();CHECK(coasted.size()==1);
 Reset();queried={&actor};S::ObserveBlast(&blast);++blast.serial;Fire();S::Tick();CHECK(!queries&&coasted.empty());
 for(void** flag:{&held,&hand,&fixed,&frozen,&trash,&notProp}){
  Reset();*flag=&actor;queried={&actor};S::ObserveBlast(&blast);Fire();S::Tick();CHECK(coasted.empty());
 }
 Reset();queried={&actor};S::ObserveBlast(&blast);queryOk=false;Fire();S::Tick();CHECK(coasted.empty());
 Reset();S::Observe(&second,&blast);queried={&actor};queryOk=false;Fire();S::Tick();CHECK(coasted.size()==1&&coasted[0]==&second);
 Reset();queried={&actor};S::ObserveBlast(&blast);queryCallback=[]{S::OnDisconnect();};Fire();S::Tick();CHECK(coasted.empty());
 Reset();queried={&actor};S::ObserveBlast(&blast);queryCallback=[]{++serial;};Fire();S::Tick();CHECK(coasted.empty());
 Reset();queried={&actor};S::ObserveBlast(&blast);queryCallback=[]{++blast.serial;};Fire();S::Tick();CHECK(coasted.empty());
 puts("PASS production explosion motion: actual delayed impulse, source/target identity, repeated blasts, exclusions, bounded queue, lifecycle and reentrant drain");
}
