#include "../../src/votv-coop/src/ue_wrap/actors/explosion_blast.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace X=ue_wrap::prop_explosion;
namespace test {
struct Obj {int id=0,serial=1;bool live=true;void* cls=nullptr;ue_wrap::reflection::FName name{};
 float radius=1000,damage=50,fireStrength=1,fireFuel=40,innerShake=1000,outerShake=2000,force=5000;
 float debrisForce=3000;ue_wrap::FVector impact{1,2,3};int32_t debris=6;void* shake=nullptr;uint8_t bits=0x80;} objects[12];
auto& can=objects[1];auto& blast=objects[2];auto& cls=objects[3];auto& shake=objects[4];auto& gs=objects[5];
uint32_t world=1;bool current=true,classes=true,bind=true,dispatch=true,spawn=true;
bool privatePc=false;
bool locOk=true,uniform=true,foreignWorld=false,cdo=false,marked=false,authority=false,salt=false,hasShake=true;std::wstring missing;
int begins=0,finishes=0,destroys=0,breaks=0,lastBreak=0;
std::function<void()> callback,locCallback,finishCallback;
X::Blast expected=[] {X::Blast b;b.transform={0,0,.6f,.8f,100,200,300,0,1.5f,2,3,0};b.force=5000;b.debrisForce=3000;b.debris=6;b.impact={1,2,3};b.ignoreWalls=true;b.fireStrength=30;return b;}();
void Invoke(std::function<void()>& fn){auto c=std::move(fn);fn={};if(c)c();}
void Reset(){++world;for(int i=0;i<12;++i){objects[i]={};objects[i].id=i;objects[i].serial=int(world);objects[i].cls=&cls;objects[i].name={i,0};}
 current=classes=bind=dispatch=spawn=locOk=uniform=true;foreignWorld=cdo=marked=false;missing.clear();
 authority=salt=privatePc=false;hasShake=true;expected.variant=0;expected.shake=true;begins=finishes=destroys=breaks=lastBreak=0;callback=locCallback=finishCallback={};}
struct SpawnParams {void* world;void* cls;ue_wrap::FTransform transform;uint8_t collision;uint8_t pad[7];void* owner;void* result;};
struct FinishParams {void* actor;ue_wrap::FTransform transform;void* result;};
}
namespace ue_wrap::reflection {
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<12?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return i>=0&&i<12?&test::objects[i]:nullptr;}
bool IsLiveByIndex(void* a,int32_t i){return IsLive(a)&&ObjectAt(i)==a;}
void* ClassOf(void* a){return a?static_cast<test::Obj*>(a)->cls:nullptr;}
std::wstring ClassNameOf(void* a){return a==&test::blast?(test::privatePc?L"explosion_pc_C":test::salt?L"explosion_saltrocket_C":L"explosion_C"):a==&test::can?L"prop_gascan_C":L"other_C";}
const FName& NameOf(void* a){return static_cast<test::Obj*>(a)->name;}
bool NameEquals(const FName&,const wchar_t*){return test::hasShake;}
bool NameStartsWith(const FName&,const wchar_t*){return test::cdo;}
void* FindClassDefaultObject(const wchar_t*){return test::classes?&test::gs:nullptr;}
int32_t FindPropertyOffset(void*,const wchar_t* n){using O=test::Obj;std::wstring name=n;if(name==test::missing)return -1;
#define FIELD(f) if(name==L###f)return offsetof(O,f)
 FIELD(radius);FIELD(damage);FIELD(fireStrength);FIELD(fireFuel);FIELD(innerShake);FIELD(outerShake);FIELD(force);FIELD(debrisForce);FIELD(impact);FIELD(debris);FIELD(shake);
#undef FIELD
 return -1;}
bool FindBoolProperty(void*,const wchar_t* n,int32_t& off,uint8_t& mask){std::wstring name=n;
 if(name==test::missing)return false;off=offsetof(test::Obj,bits);mask=name==L"ignoreWalls"?4:name==L"Skip Setting"?8:0;return mask!=0;}
void* FindDispatchFunctionCached(void*,const wchar_t* n){std::wstring name=n;
 if(name==L"BeginDeferredActorSpawnFromClass")return &test::objects[6];if(name==L"FinishSpawningActor")return &test::objects[7];
 if(name==L"GetTransform")return &test::objects[10];if(name==L"broken")return &test::objects[8];if(name==L"broken_fire")return &test::objects[9];return nullptr;}
int32_t FunctionFrameSize(void* f){int id=InternalIndexOf(f);return id==6?sizeof(test::SpawnParams):id==7?sizeof(test::FinishParams):id==10?sizeof(FTransform):0;}
std::vector<ParamInfo> FunctionParams(void* f){using B=test::SpawnParams;using F=test::FinishParams;if(!test::bind)return {};
 if(InternalIndexOf(f)==10)return {{L"ReturnValue",0,sizeof(FTransform),0}};
 if(InternalIndexOf(f)==6)return {{L"WorldContextObject",offsetof(B,world),8,0},{L"ActorClass",offsetof(B,cls),8,0},
 {L"SpawnTransform",offsetof(B,transform),sizeof(FTransform),0},{L"CollisionHandlingOverride",offsetof(B,collision),1,0},
 {L"Owner",offsetof(B,owner),8,0},{L"ReturnValue",offsetof(B,result),8,0}};
 return {{L"Actor",offsetof(F,actor),8,0},{L"SpawnTransform",offsetof(F,transform),sizeof(FTransform),0},{L"ReturnValue",offsetof(F,result),8,0}};}
bool CallFunction(void* a,void* f,void* raw){using namespace test;if(!dispatch)return false;int id=InternalIndexOf(f);
 if(id==10){*static_cast<FTransform*>(raw)=expected.transform;Invoke(locCallback);return locOk;}
 if(id==6){++begins;auto& p=*static_cast<SpawnParams*>(raw);CHECK(a==&gs&&p.world&&p.cls==&cls&&p.collision==1&&!p.owner);
  CHECK(p.transform.TX==100&&p.transform.TY==200&&p.transform.TZ==300&&p.transform.RotW==.8f&&p.transform.RotZ==.6f&&p.transform.SX==1.5f);
  p.result=spawn?&blast:nullptr;Invoke(callback);}
 else if(id==7){++finishes;CHECK(marked);auto& p=*static_cast<FinishParams*>(raw);CHECK(p.actor==&blast&&p.transform.SZ==3&&p.transform.SY==2);
  CHECK(blast.radius==expected.radius&&blast.damage==expected.damage&&blast.fireStrength==expected.fireStrength&&blast.fireFuel==expected.fireFuel);
  CHECK(blast.innerShake==expected.innerShake&&blast.outerShake==expected.outerShake&&blast.force==(authority?expected.force:0)&&blast.debris==(authority?expected.debris:0)&&blast.debrisForce==expected.debrisForce&&blast.impact.Z==3&&blast.shake==(expected.shake?&shake:nullptr)&&blast.bits==0x84);
  Invoke(finishCallback);}
 else {CHECK(a==&can&&(id==8||id==9));++breaks;lastBreak=id;}return true;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return (void*)(uintptr_t)test::world;}void* WorldOf(void*){return test::foreignWorld?(void*)999:CurrentWorld();}}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t* n){return !test::classes?nullptr:(std::wstring(n)==L"explosion_C"||std::wstring(n)==L"explosion_saltrocket_C")?&test::cls:&test::shake;}}
namespace ue_wrap::engine {
void* GetWorldContext(){return world_identity::CurrentWorld();}
bool TryGetActorLocation(void*,FVector& loc){loc={100,200,300};test::Invoke(test::locCallback);return test::locOk;}
FVector GetActorScale3D(void*){return {1.5f,1.5f,test::uniform?1.5f:2.f};}
bool DestroyActor(void* a){CHECK(a==&test::blast);++test::destroys;test::blast.live=false;return true;}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
using namespace test;
bool Replay(){const auto epoch=world;return X::ReplayBlast(expected,authority,[](void* a){CHECK(a==&blast);marked=true;return true;},[epoch]{return current&&epoch==world;});}
int main(){
 Reset();CHECK(!X::IsPrivatePcBlast(&blast));privatePc=true;CHECK(X::IsPrivatePcBlast(&blast) && !X::IsGameplayBlast(&blast));
 cdo=true;CHECK(!X::IsPrivatePcBlast(&blast));cdo=false;foreignWorld=true;CHECK(!X::IsPrivatePcBlast(&blast));
 foreignWorld=false;blast.live=false;CHECK(!X::IsPrivatePcBlast(&blast) && !X::IsPrivatePcBlast(nullptr));
 Reset();CHECK(Replay()&&begins==1&&finishes==1&&!destroys);
 Reset();authority=true;CHECK(Replay());
 Reset();salt=true;expected.variant=1;CHECK(Replay());
 Reset();expected.shake=false;CHECK(Replay()&&!blast.shake);
 Reset();X::Blast b;CHECK(X::ReadBlast(&blast,b)&&b.transform.RotZ==.6f&&b.transform.SY==2&&b.force==5000&&b.impact.Z==3&&b.debris==6);
 salt=true;CHECK(X::ReadBlast(&blast,b)&&b.variant==1);cdo=true;CHECK(!X::IsGameplayBlast(&blast));cdo=false;foreignWorld=true;CHECK(!X::IsGameplayBlast(&blast));
 for(int i=0;i<7;++i){Reset();if(i==0)classes=false;if(i==1)current=false;if(i==2)bind=false;if(i==3)dispatch=false;if(i==4)spawn=false;
  if(i==5)missing=L"Skip Setting";if(i==6)missing=L"debrisForce";CHECK(!Replay()&&!finishes);CHECK(destroys==(i>=5));}
 for(int i=0;i<4;++i){Reset();callback=[i]{if(i==0)current=false;if(i==1)++world;if(i==2)++blast.serial;if(i==3)blast.live=false;};
  if(i<2||i==3)CHECK(!Replay()&&!finishes);else CHECK(Replay());}
 Reset();CHECK(!X::ReplayBlast(expected,false,[](void*){return false;},[]{return current;})&&destroys==1&&!finishes);
 Reset();CHECK(!X::ReplayBlast(expected,false,[](void*){++blast.serial;return true;},[]{return current;})&&!destroys&&!finishes);
 Reset();finishCallback=[]{blast.live=false;current=false;};CHECK(Replay()&&finishes==1&&!destroys);
 for(int i=0;i<3;++i){Reset();if(i==0)locOk=false;if(i==1)locCallback=[]{++world;};if(i==2)locCallback=[]{++blast.serial;};CHECK(!X::ReadBlast(&blast,b));}
 Reset();missing=L"fireFuel";CHECK(!X::ReadBlast(&blast,b));
 for(int i=0;i<13;++i){auto v=expected;if(i==0)v.transform.TX=INFINITY;if(i==1)v.transform.RotW=0;if(i==2)v.radius=0;if(i==3)v.radius=1000001;
  if(i==4)v.damage=-1;if(i==5)v.fireStrength=1001;if(i==6)v.fireFuel=1001;if(i==7)v.innerShake=-1;if(i==8)v.outerShake=NAN;if(i==9)v.impact.Z=1000000010;
  if(i==10)v.debris=129;if(i==11)v.variant=2;if(i==12)v.transform.SZ=0;CHECK(!X::Valid(v));}
 puts("PASS actual general blast wrapper/ParamFrame: complete transform/parameters, salt rocket, optional shake, authority force/debris, identity/epoch cleanup, one-shot finish, bounds");
}

// Reflection names used by ParamFrame missing-field diagnostics.
namespace ue_wrap::reflection {
std::wstring ToString(const FName&){return L"fixture_function";}
}
