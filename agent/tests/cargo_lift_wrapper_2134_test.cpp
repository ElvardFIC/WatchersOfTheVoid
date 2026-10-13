#include "../../src/votv-coop/src/ue_wrap/devices/cargo_lift.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace L=ue_wrap::cargo_lift;namespace R=ue_wrap::reflection;
namespace test {
struct Obj {int id=0,serial=1;bool live=true;R::FName name{};void* cls=nullptr;
 void* attach=nullptr;void* comp=nullptr;void* move=nullptr;void* buttons[3]{};float alpha=0;
 uint8_t flags=0;bool sim=true;ue_wrap::FVector pos{},relative{};ue_wrap::FRotator rot{};
} objects[32];
Obj& actor=objects[1];Obj& anchor=objects[2];Obj& body=objects[3];Obj& timeline=objects[4];Obj& cls=objects[5];Obj& other=objects[6];
uint32_t world=1;int calls=0,failAt=0,stops=0,activations=0;bool bind=true,current=true;
std::function<void()> callback;int callbackAt=0;
void Callback(){if(callback&&(!callbackAt||calls==callbackAt)){auto c=std::move(callback);callback={};c();}}
void Reset(){++world;for(int i=0;i<32;++i){objects[i]={};objects[i].id=i;objects[i].serial=int(world);objects[i].cls=&cls;objects[i].name={i,0};}
 actor.attach=&anchor;actor.comp=&body;actor.move=&timeline;actor.buttons[0]=&objects[7];actor.buttons[1]=&objects[8];actor.buttons[2]=&objects[9];
 calls=failAt=stops=activations=callbackAt=0;bind=current=true;callback={};actor.flags=0xA0;
}
}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<32?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return i>=0&&i<32?&test::objects[i]:nullptr;}
bool IsLiveByIndex(void* a,int32_t i){return IsLive(a)&&InternalIndexOf(a)==i;}
void* ClassOf(void* a){return a?static_cast<test::Obj*>(a)->cls:nullptr;}
std::wstring ClassNameOf(void* a){return a==&test::actor?L"cargoLift_C":L"SceneComponent";}
const FName& NameOf(void* a){return static_cast<test::Obj*>(a)->name;}
bool NameStartsWith(const FName&,const wchar_t*){return false;}
std::wstring ToString(const FName&){return L"cargoLift_1";}
int32_t InstanceOffset::Of(void*){using O=test::Obj;std::wstring n=prop_;
 if(n==L"attach")return offsetof(O,attach);if(n==L"comp")return offsetof(O,comp);if(n==L"move")return offsetof(O,move);if(n==L"alpha")return offsetof(O,alpha);
 if(n==L"RelativeLocation")return offsetof(O,relative);if(n==L"buttonsElev")return offsetof(O,buttons);if(n==L"buttonsElev1")return offsetof(O,buttons)+8;if(n==L"buttonsElev2")return offsetof(O,buttons)+16;return -1;
}
bool FindBoolProperty(void*,const wchar_t* n,int32_t& off,uint8_t& mask){off=offsetof(test::Obj,flags);mask=std::wstring(n)==L"moving"?1:2;return true;}
void* FindDispatchFunctionCached(void*,const wchar_t* name){
 const wchar_t* names[]={L"K2_GetComponentLocation",L"K2_GetComponentRotation",L"IsSimulatingPhysics",L"SetSimulatePhysics",L"K2_SetRelativeLocation",L"K2_SetWorldLocationAndRotation",L"actionOptionIndex"};
 for(int i=0;i<7;++i)if(std::wstring(name)==names[i])return &test::objects[10+i];return nullptr;
}
int32_t FunctionFrameSize(void*){return 80;}
std::vector<ParamInfo> FunctionParams(void* f){int id=InternalIndexOf(f);
 if(id<=12)return {{L"ReturnValue",0,12,0}};
 if(id==13)return {{L"bSimulate",0,1,0}};
 if(id==14||id==15)return test::bind?std::vector<ParamInfo>{{L"NewLocation",0,12,0},{L"NewRotation",12,12,0},{L"bSweep",24,1,0},{L"bTeleport",25,1,0}}:std::vector<ParamInfo>{{L"NewLocation",0,12,0},{L"bSweep",24,1,0},{L"bTeleport",25,1,0}};
 return {{L"player",0,8,0},{L"action",8,1,0},{L"lookAtComponent",16,8,0}};
}
bool CallFunction(void* a,void* f,void* data){using namespace test;++calls;auto& o=*static_cast<Obj*>(a);auto* p=static_cast<uint8_t*>(data);const int id=InternalIndexOf(f);
 if(calls==failAt){Callback();return false;}
 if(id==10)std::memcpy(p,&o.pos,12);if(id==11)std::memcpy(p,&o.rot,12);if(id==12)std::memcpy(p,&o.sim,1);if(id==13)o.sim=p[0]!=0;
 if(id==14){CHECK(!p[24]);std::memcpy(&o.relative,p,12);}if(id==15){CHECK(!p[24]);std::memcpy(&o.pos,p,12);std::memcpy(&o.rot,p+12,12);}
 if(id==16){CHECK(p[8]==4);++activations;o.flags|=1;}
 Callback();return true;
}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return (void*)(uintptr_t)test::world;}void* WorldOf(void*){return CurrentWorld();}}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t*){return &test::cls;}
size_t ForEachInstance(void*,InstanceFn fn,void* ctx){fn(ctx,&test::actor,1);return 1;}
}
namespace ue_wrap::engine {bool IsPrimitiveComponent(void* a){return a==&test::body||a==&test::other;}}
namespace ue_wrap::component_calls {bool CallParamlessNamed(void*,const wchar_t* n){CHECK(std::wstring(n)==L"Stop");++test::stops;++test::calls;test::Callback();return test::calls!=test::failAt;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
using namespace test;
L::State State(){L::State s;s.alpha=.75;s.anchor={0,0,420};s.position={20,30,419};s.rotation={0,179,0};s.moving=s.direction=true;return s;}
bool Apply(L::Mirror& m){return L::Apply(&actor,State(),m,false,[]{return current;});}
int main(){
 Reset();L::Mirror m;CHECK(Apply(m));CHECK(!body.sim&&body.pos.Z==419&&anchor.relative.Z==420&&actor.alpha==.75&&actor.flags==0xA3&&stops==1);
 L::State read;CHECK(L::Read(&actor,read)&&read.position.Z==419&&read.alpha==.75&&read.moving);L::Release(m);CHECK(body.sim&&actor.flags==0xA2&&!m.token);
 Reset();actor.flags|=1;CHECK(!L::Activate(&actor,2,&other));actor.flags&=~1;CHECK(L::Activate(&actor,2,&other)&&activations==1);CHECK(L::ButtonIndex(&actor,actor.buttons[2])==2);ue_wrap::FVector pos;CHECK(L::ButtonPosition(&actor,2,pos));
 Reset();body.sim=false;m={};CHECK(Apply(m));L::Release(m);CHECK(!body.sim);
 Reset();actor.comp=nullptr;m={};CHECK(!Apply(m)&&!m.token&&body.sim);
 Reset();m={};failAt=1;CHECK(!Apply(m)&&!m.token&&body.sim);failAt=0;CHECK(Apply(m));
 Reset();m={};bind=false;CHECK(!Apply(m)&&actor.alpha==0);++objects[15].serial;bind=true;CHECK(Apply(m));L::Release(m);
 for(int fail=2;fail<=5;++fail){Reset();m={};failAt=fail;CHECK(!Apply(m)&&actor.alpha==0);failAt=0;CHECK(Apply(m));L::Release(m);CHECK(body.sim);}
 for(int change=0;change<5;++change){Reset();m={};callbackAt=2;callback=[change,&m]{if(change==0)++world;if(change==1)++body.serial;if(change==2)actor.comp=&other;if(change==3){current=false;L::Release(m);}if(change==4)current=false;};CHECK(!Apply(m)&&actor.alpha==0);if(change==3)CHECK(body.sim);}
 Reset();m={};callbackAt=1;callback=[]{current=false;};CHECK(!Apply(m)&&body.sim&&!m.token); // query callbacks before loan acquisition
 Reset();m={};CHECK(Apply(m));actor.comp=&other;CHECK(Apply(m)&&body.sim&&!other.sim);L::Release(m);CHECK(other.sim);
 Reset();m={};CHECK(Apply(m));++body.serial;L::Release(m);CHECK(!body.sim); // never restore physics on a recycled object
 CHECK(!(actor.flags&1)); // actor's mirrored moving latch is still ours to release
 Reset();m={};CHECK(Apply(m));++world;L::Release(m);CHECK(!body.sim); // old-world body untouched
 Reset();m={};CHECK(Apply(m));callback=[]{actor.flags|=1;current=false;};L::Release(m,[]{return current;});CHECK(actor.flags&1); // teardown callback must not clear a new session's state
 Reset();auto bad=State();bad.alpha=-1;CHECK(!L::Valid(bad));bad=State();bad.position.Z=1e8;CHECK(!L::Valid(bad));bad=State();bad.rotation.Pitch=INFINITY;CHECK(!L::Valid(bad));
 puts("PASS lift real wrapper/ParamFrame: physical body+anchor, flags, restore, native failures and callback identity/world/cancellation");
}
