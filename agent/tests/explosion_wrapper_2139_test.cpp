#include "../../src/votv-coop/src/ue_wrap/actors/prop_explosion.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace X=ue_wrap::prop_explosion;
namespace test {
struct Obj {int id=0,serial=1;bool live=true;void* cls=nullptr;ue_wrap::reflection::FName name{};
 float radius=1000,damage=50,fireStrength=1,fireFuel=40,innerShake=1000,outerShake=2000,force=5000;
 int32_t debris=6;void* shake=nullptr;uint8_t bits=0x80;} objects[12];
auto& can=objects[1];auto& blast=objects[2];auto& cls=objects[3];auto& shake=objects[4];auto& gs=objects[5];
uint32_t world=1;bool current=true,classes=true,bind=true,dispatch=true,spawn=true;
bool locOk=true,uniform=true,foreignWorld=false,cdo=false,marked=false;std::wstring missing;
int begins=0,finishes=0,destroys=0,breaks=0,lastBreak=0;
std::function<void()> callback,locCallback,finishCallback;
X::State expected{{100,200,300},1.5f,700,35,.8f,25,800,1900,true,false};
void Invoke(std::function<void()>& fn){auto c=std::move(fn);fn={};if(c)c();}
void Reset(){++world;for(int i=0;i<12;++i){objects[i]={};objects[i].id=i;objects[i].serial=int(world);objects[i].cls=&cls;objects[i].name={i,0};}
 current=classes=bind=dispatch=spawn=locOk=uniform=true;foreignWorld=cdo=marked=false;missing.clear();
 begins=finishes=destroys=breaks=lastBreak=0;callback=locCallback=finishCallback={};}
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
std::wstring ClassNameOf(void* a){return a==&test::blast?L"explosion_C":a==&test::can?L"prop_gascan_C":L"other_C";}
const FName& NameOf(void* a){return static_cast<test::Obj*>(a)->name;}
bool NameStartsWith(const FName&,const wchar_t*){return test::cdo;}
void* FindClassDefaultObject(const wchar_t*){return test::classes?&test::gs:nullptr;}
int32_t FindPropertyOffset(void*,const wchar_t* n){using O=test::Obj;std::wstring name=n;if(name==test::missing)return -1;
#define FIELD(f) if(name==L###f)return offsetof(O,f)
 FIELD(radius);FIELD(damage);FIELD(fireStrength);FIELD(fireFuel);FIELD(innerShake);FIELD(outerShake);FIELD(force);FIELD(debris);FIELD(shake);
#undef FIELD
 return -1;}
bool FindBoolProperty(void*,const wchar_t* n,int32_t& off,uint8_t& mask){std::wstring name=n;
 if(name==test::missing)return false;off=offsetof(test::Obj,bits);mask=name==L"ignoreWalls"?4:name==L"Skip Setting"?8:0;return mask!=0;}
void* FindDispatchFunctionCached(void*,const wchar_t* n){std::wstring name=n;
 if(name==L"BeginDeferredActorSpawnFromClass")return &test::objects[6];if(name==L"FinishSpawningActor")return &test::objects[7];
 if(name==L"broken")return &test::objects[8];if(name==L"broken_fire")return &test::objects[9];return nullptr;}
int32_t FunctionFrameSize(void* f){int id=InternalIndexOf(f);return id==6?sizeof(test::SpawnParams):id==7?sizeof(test::FinishParams):0;}
std::vector<ParamInfo> FunctionParams(void* f){using B=test::SpawnParams;using F=test::FinishParams;if(!test::bind)return {};
 if(InternalIndexOf(f)==6)return {{L"WorldContextObject",offsetof(B,world),8,0},{L"ActorClass",offsetof(B,cls),8,0},
 {L"SpawnTransform",offsetof(B,transform),sizeof(FTransform),0},{L"CollisionHandlingOverride",offsetof(B,collision),1,0},
 {L"Owner",offsetof(B,owner),8,0},{L"ReturnValue",offsetof(B,result),8,0}};
 return {{L"Actor",offsetof(F,actor),8,0},{L"SpawnTransform",offsetof(F,transform),sizeof(FTransform),0},{L"ReturnValue",offsetof(F,result),8,0}};}
bool CallFunction(void* a,void* f,void* raw){using namespace test;if(!dispatch)return false;int id=InternalIndexOf(f);
 if(id==6){++begins;auto& p=*static_cast<SpawnParams*>(raw);CHECK(a==&gs&&p.world&&p.cls==&cls&&p.collision==1&&!p.owner);
  CHECK(p.transform.TX==100&&p.transform.TY==200&&p.transform.TZ==300&&p.transform.RotW==1&&p.transform.SX==1.5f);
  p.result=spawn?&blast:nullptr;Invoke(callback);}
 else if(id==7){++finishes;CHECK(marked);auto& p=*static_cast<FinishParams*>(raw);CHECK(p.actor==&blast&&p.transform.SZ==1.5f);
  CHECK(blast.radius==expected.radius&&blast.damage==expected.damage&&blast.fireStrength==expected.fireStrength&&blast.fireFuel==expected.fireFuel);
  CHECK(blast.innerShake==expected.innerShake&&blast.outerShake==expected.outerShake&&blast.force==0&&blast.debris==0&&blast.shake==&shake&&blast.bits==0x84);
  Invoke(finishCallback);}
 else {CHECK(a==&can&&(id==8||id==9));++breaks;lastBreak=id;}return true;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return (void*)(uintptr_t)test::world;}void* WorldOf(void*){return test::foreignWorld?(void*)999:CurrentWorld();}}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t* n){return !test::classes?nullptr:std::wstring(n)==L"explosion_C"?&test::cls:&test::shake;}}
namespace ue_wrap::engine {
void* GetWorldContext(){return world_identity::CurrentWorld();}
bool TryGetActorLocation(void*,FVector& loc){loc={100,200,300};test::Invoke(test::locCallback);return test::locOk;}
FVector GetActorScale3D(void*){return {1.5f,1.5f,test::uniform?1.5f:2.f};}
bool DestroyActor(void* a){CHECK(a==&test::blast);++test::destroys;test::blast.live=false;return true;}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
using namespace test;
bool Replay(){const auto epoch=world;return X::Replay(expected,[](void* a){CHECK(a==&blast);marked=true;return true;},[epoch]{return current&&epoch==world;});}
int main(){
 Reset();CHECK(Replay()&&begins==1&&finishes==1&&!destroys);
 Reset();X::State s{};CHECK(X::Read(&blast,s)&&s.position.X==100&&s.radius==1000&&s.damage==50&&s.scale==1.5f);
 Reset();CHECK(X::IsGascan(&can)&&!X::IsGascan(&blast)&&!X::IsExplosion(&can));cdo=true;CHECK(!X::IsGascan(&can));cdo=false;foreignWorld=true;CHECK(!X::IsGascan(&can));
 Reset();CHECK(X::BreakGascan(&can,false)&&lastBreak==8);CHECK(X::BreakGascan(&can,true)&&lastBreak==9);CHECK(!X::BreakGascan(&blast,false)&&breaks==2);
 for(int i=0;i<7;++i){Reset();if(i==0)classes=false;if(i==1)current=false;if(i==2)bind=false;if(i==3)dispatch=false;if(i==4)spawn=false;
  if(i==5)missing=L"Skip Setting";if(i==6)missing=L"force";CHECK(!Replay()&&!finishes);CHECK(destroys==(i>=5));}
 for(int i=0;i<4;++i){Reset();callback=[i]{if(i==0)current=false;if(i==1)++world;if(i==2)++blast.serial;if(i==3)blast.live=false;};
  // A successor created DURING Begin is captured fresh, so epoch/current is the cancellation contract.
  if(i<2||i==3)CHECK(!Replay()&&!finishes);else CHECK(Replay());}
 Reset();CHECK(!X::Replay(expected,[](void*){return false;},[]{return current;})&&destroys==1&&!finishes);
 Reset();CHECK(!X::Replay(expected,[](void*){++blast.serial;return true;},[]{return current;})&&!destroys&&!finishes);
 Reset();finishCallback=[]{blast.live=false;current=false;};CHECK(Replay()&&finishes==1&&!destroys); // a completed one-shot is never retried
 for(int i=0;i<4;++i){Reset();if(i==0)locOk=false;if(i==1)uniform=false;if(i==2)locCallback=[]{++world;};if(i==3)locCallback=[]{++blast.serial;};CHECK(!X::Read(&blast,s));}
 Reset();missing=L"fireFuel";CHECK(!X::Read(&blast,s));
 for(int i=0;i<10;++i){auto v=expected;if(i==0)v.position.X=INFINITY;if(i==1)v.scale=0;if(i==2)v.radius=0;if(i==3)v.radius=10001;
  if(i==4)v.damage=-1;if(i==5)v.fireStrength=1.01f;if(i==6)v.fireFuel=61;if(i==7)v.innerShake=-1;if(i==8)v.outerShake=NAN;if(i==9)v.position.Z=1000001;CHECK(!X::Valid(v));}
 puts("PASS actual explosion wrapper/ParamFrame: deferred field binding, preserved bool bits, no replica force/debris, native break variants, cancellation/cleanup, one-shot finish and bounds");
}

// Reflection names used by ParamFrame missing-field diagnostics.
namespace ue_wrap::reflection {
std::wstring ToString(const FName&){return L"fixture_function";}
}
