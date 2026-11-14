#include "../../src/votv-coop/src/ue_wrap/devices/cord.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {
 int id=0,serial=1;bool live=true;void* cls=nullptr;
 void* root=nullptr;void* dir=nullptr;void* socket=nullptr;void* cord=nullptr;void* head=nullptr;
 float length=1000;ue_wrap::FVector pos{};
} objects[16];
uint32_t world=1;int reads=0,connects=0,disconnects=0;bool fields=true,callOk=true;
int callbackRead=1;std::function<void()> locationCallback,disconnectCallback,connectCallback;
int rotationReads=0,rotationWrites=0;std::function<void()> positionWriteCallback,rotationCallback;
void Invoke(std::function<void()>& cb){auto f=std::move(cb);cb={};if(f)f();}
void Reset(){++world;for(int i=0;i<16;++i){objects[i]={};objects[i].id=i;objects[i].serial=int(world);}
 objects[0].cls=&objects[10];objects[1].cls=&objects[11];objects[2].cls=objects[3].cls=&objects[12];
 objects[0].root=&objects[2];objects[1].dir=&objects[3];objects[3].pos={1200,0,0};
 objects[0].head=&objects[2];rotationReads=rotationWrites=0;positionWriteCallback=rotationCallback={};
 reads=connects=disconnects=0;fields=callOk=true;callbackRead=1;
 locationCallback=disconnectCallback=connectCallback={};
}
}
namespace ue_wrap::reflection {
bool FindBoolProperty(void*,const wchar_t*,int32_t&,uint8_t&){std::abort();}
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
void* ClassOf(void* a){return static_cast<test::Obj*>(a)->cls;}
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
bool IsLiveByIndex(void* a,int32_t i){return i>=0&&i<16&&a==&test::objects[i]&&IsLive(a);}
int32_t SlotSerial(int32_t i){return i>=0&&i<16?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return i>=0&&i<16?&test::objects[i]:nullptr;}
bool IsDescendantOfAny(void* cls,void* const* roots,size_t n,int){for(size_t i=0;i<n;++i)if(cls==roots[i])return true;return false;}
int32_t InstanceOffset::Of(void*){
 if(!test::fields)return -1;
 if(!wcscmp(prop_,L"root"))return offsetof(test::Obj,root);
 if(!wcscmp(prop_,L"head"))return offsetof(test::Obj,head);
 if(!wcscmp(prop_,L"dir"))return offsetof(test::Obj,dir);
 if(!wcscmp(prop_,L"socket"))return offsetof(test::Obj,socket);
 if(!wcscmp(prop_,L"cord"))return offsetof(test::Obj,cord);
 if(!wcscmp(prop_,L"cordMaxLength"))return offsetof(test::Obj,length);
 return -1;
}
void* FindDispatchFunctionCached(void*,const wchar_t* n){
 if(!wcscmp(n,L"K2_GetComponentLocation"))return &test::objects[13];
 if(!wcscmp(n,L"K2_SetWorldLocation"))return &test::objects[15];
 if(!wcscmp(n,L"connect"))return &test::objects[14];return nullptr;
}
int32_t FunctionFrameSize(void*){return 32;}
std::vector<ParamInfo> FunctionParams(void* f){
 ParamInfo a{},b{};a.offset=0;b.offset=8;
 if(f==&test::objects[13]){a.name=L"ReturnValue";return {a};}
 if(f==&test::objects[15]){ParamInfo c{};a.name=L"NewLocation";b.name=L"bSweep";b.offset=12;c.name=L"bTeleport";c.offset=13;return {a,b,c};}
 a.name=L"socket";b.name=L"load";return {a,b};
}
bool CallFunction(void* a,void* f,void* raw){
 if(!test::callOk)return false;
 auto* object=static_cast<test::Obj*>(a);
 if(f==&test::objects[13]){
  ++test::reads;std::memcpy(raw,&object->pos,sizeof(FVector));
  if(test::reads==test::callbackRead)test::Invoke(test::locationCallback);
 }else if(f==&test::objects[15]){
  std::memcpy(&object->pos,raw,sizeof(FVector));test::Invoke(test::positionWriteCallback);
 }else{
  CHECK(f==&test::objects[14]&&a==&test::objects[0]);
  auto* socket=*static_cast<test::Obj**>(raw);CHECK(socket==&test::objects[1]);
  CHECK(static_cast<uint8_t*>(raw)[8]==0); // network never requests load=true
  ++test::connects;object->socket=socket;socket->cord=object;test::Invoke(test::connectCallback);
 }
 return true;
}
}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t* n){return &test::objects[!wcscmp(n,L"cord_C")?10:11];}}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* a){return static_cast<test::Obj*>(a)->id<10?CurrentWorld():nullptr;}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::engine {
FRotator GetComponentWorldRotation(void*){++test::rotationReads;test::Invoke(test::rotationCallback);return {};}
bool SetComponentWorldRotation(void*,const FRotator&){++test::rotationWrites;test::Invoke(test::rotationCallback);return true;}
}
namespace ue_wrap::component_calls {
bool CallParamlessNamed(void* a,const wchar_t* n){
 CHECK(!wcscmp(n,L"disconnect"));if(!test::callOk)return false;
 ++test::disconnects;auto* cord=static_cast<test::Obj*>(a);
 if(cord->socket)static_cast<test::Obj*>(cord->socket)->cord=nullptr;cord->socket=nullptr;
 test::Invoke(test::disconnectCallback);return true;
}
}
int main(){using namespace test;namespace C=ue_wrap::cord;
 Reset();CHECK(C::InCableReach(&objects[0],&objects[1])&&reads==2);
 objects[3].pos.X=1300;CHECK(!C::InCableReach(&objects[0],&objects[1])); // native disconnect boundary
 objects[3].pos={0,0,1301};CHECK(!C::InCableReach(&objects[0],&objects[1]));
 objects[3].pos={NAN,0,0};CHECK(!C::InCableReach(&objects[0],&objects[1]));
 for(float length:{0.f,-1.f,NAN,INFINITY}){Reset();objects[0].length=length;CHECK(!C::InCableReach(&objects[0],&objects[1]));}
 Reset();fields=false;CHECK(!C::InCableReach(&objects[0],&objects[1])&&!reads);
 Reset();callOk=false;CHECK(!C::InCableReach(&objects[0],&objects[1]));
 for(int phase:{1,2})for(int change=0;change<7;++change){
  Reset();callbackRead=phase;locationCallback=[change]{
   if(change<4)++objects[change].serial;
   if(change==4)++world;
   if(change==5)objects[0].root=&objects[4];
   if(change==6)objects[1].dir=&objects[4];
  };
  CHECK(!C::InCableReach(&objects[0],&objects[1])&&reads==phase);
 }
 Reset();CHECK(C::Connect(&objects[0],&objects[1])&&connects==1);
 CHECK(C::Connect(&objects[0],&objects[1])&&connects==1&&!disconnects);
 Reset();objects[1].cord=&objects[4];CHECK(!C::Connect(&objects[0],&objects[1])&&!connects&&!disconnects);
 Reset();objects[0].socket=&objects[4];objects[4].cord=&objects[0];
 CHECK(C::Connect(&objects[0],&objects[1])&&connects==1&&disconnects==1);
 for(int change=0;change<5;++change){
  Reset();objects[0].socket=&objects[4];objects[4].cord=&objects[0];
  disconnectCallback=[change]{
   if(change<2)++objects[change].serial;
   if(change==2)++world;
   if(change==3)objects[1].cord=&objects[5];
   if(change==4)objects[0].socket=&objects[5];
  };
  CHECK(!C::Connect(&objects[0],&objects[1])&&!connects&&disconnects==1);
 }
 for(int change=0;change<3;++change){
  Reset();connectCallback=[change]{if(change<2)++objects[change].serial;else ++world;};
  CHECK(!C::Connect(&objects[0],&objects[1])&&connects==1);
 }
 for(int phase=0;phase<4;++phase)for(int cause=0;cause<7;++cause){
  Reset();bool allowed=true;auto current=[&]{return allowed;};
  auto invalidate=[&]{if(cause==0)++world;if(cause==1)++objects[0].serial;if(cause==2)++objects[10].serial;
   if(cause==3)++objects[2].serial;if(cause==4)++objects[12].serial;if(cause==5)objects[0].head=&objects[3];if(cause==6)allowed=false;};
  ue_wrap::FVector at{99,99,99};ue_wrap::FRotator angle{99,99,99};
  if(phase==0)locationCallback=invalidate;
  if(phase==1||phase==3)rotationCallback=invalidate;
  if(phase==2)positionWriteCallback=invalidate;
  if(phase<2){CHECK(!C::ReadHeadPose(&objects[0],at,angle,current));CHECK(at.X==99&&angle.Pitch==99);if(phase==0)CHECK(!rotationReads);}
  else {CHECK(!C::WriteHeadPose(&objects[0],{}, {},current));if(phase==2)CHECK(!rotationWrites);}
 }
 Reset();ue_wrap::FVector at{};ue_wrap::FRotator angle{};CHECK(C::ReadHeadPose(&objects[0],at,angle)&&rotationReads==1);
 CHECK(C::WriteHeadPose(&objects[0],{1,2,3},{})&&rotationWrites==1&&objects[2].pos.X==1);
 CHECK(!C::WriteHeadPose(&objects[0],{NAN,0,0},{})&&rotationWrites==1);
 Reset();bool allowed=true;locationCallback=[&]{allowed=false;};
 CHECK(!C::InCableReach(&objects[0],&objects[1],[&]{return allowed;})&&reads==1);
 Reset();allowed=true;objects[0].socket=&objects[4];objects[4].cord=&objects[0];disconnectCallback=[&]{allowed=false;};
 CHECK(!C::Connect(&objects[0],&objects[1],[&]{return allowed;})&&disconnects==1&&!connects);
 puts("PASS UI05 real head read/write: 28 callback replacements, caller proof, owned outputs and finite input");
 puts("PASS real cord wrapper/ParamFrame: native length boundary, argument values, occupancy and callback lifetime");
}

// Reflection names used by ParamFrame missing-field diagnostics.
namespace ue_wrap::reflection {
std::wstring ToString(const FName&){return L"fixture_function";}
const FName& NameOf(void*){static const FName name{};return name;}
}
