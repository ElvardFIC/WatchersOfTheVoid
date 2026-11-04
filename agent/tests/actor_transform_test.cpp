#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <cwchar>
#include <string>
#include <cstddef>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct FVector{float X=0,Y=0,Z=0;};
struct FRotator{float Pitch=0,Yaw=0,Roll=0;};
struct Object{void* root=nullptr;int serial=1;bool live=true;} objects[12];
int bindings[3]={2,3,4};uint32_t world=1;int worldObject=0;
bool allowed=true,haveWorld=true;
bool frameValid[3],readable[3],callOkay[3];
float values[9];int calls=0,currentChecks=0;
std::function<void(int)> onCall,onCurrent;
int rootOffset=0,objectSize=sizeof(Object);std::wstring rootType=L"ObjectProperty";
namespace P::name {const wchar_t* GetActorLocationFn=L"location";const wchar_t* GetActorRotationFn=L"rotation";const wchar_t* GetActorScale3DFn=L"scale";}
namespace world_identity {uint32_t Generation(){return world;}void* CurrentWorld(){return haveWorld?&worldObject:nullptr;}}
namespace R {
bool IsLive(void* p){return p&&static_cast<Object*>(p)->live;}
void* ClassOf(void* p){return p==&objects[0]?&objects[worldObject?6:1]:p==&objects[8]?&objects[9]:nullptr;}
int32_t StructSize(void*){return objectSize;}
bool PropertyOffsetAndClass(void*,const wchar_t*,int32_t& offset,std::wstring& type){offset=rootOffset;type=rootType;return true;}
void* FindDispatchFunctionCached(void*,const wchar_t* name){int i=!std::wcscmp(name,L"location")?0:!std::wcscmp(name,L"rotation")?1:2;return bindings[i]>=0?&objects[bindings[i]]:nullptr;}
}
struct CachedObjRef{
 Object* value=nullptr;int serial=0;
 void Set(void* p){value=static_cast<Object*>(p);serial=value?value->serial:0;}
 void* Raw()const{return value;}void* Get()const{return value&&value->live&&value->serial==serial?value:nullptr;}
};
struct ParamFrame{
 Object* function;int which;
 explicit ParamFrame(void* fn):function(static_cast<Object*>(fn)),which(int(function-objects)-2){}
 bool valid()const{return which>=0&&which<3&&frameValid[which];}
 bool GetRaw(const wchar_t*,void* output,int size){CHECK(size==12);if(!readable[which])return false;std::memcpy(output,values+which*3,12);return true;}
};
bool Call(void*,ParamFrame& frame){++calls;if(onCall)onCall(frame.which);return callOkay[frame.which];}
#include "actor_transform.inc"
void Reset(){for(auto& object:objects)object={};for(int i=0;i<3;++i){bindings[i]=i+2;frameValid[i]=readable[i]=callOkay[i]=true;}
 objects[0].root=&objects[8];rootOffset=offsetof(Object,root);objectSize=sizeof(Object);rootType=L"ObjectProperty";
 for(int i=0;i<9;++i)values[i]=float(i+1);allowed=haveWorld=true;world=1;worldObject=0;calls=currentChecks=0;onCall=onCurrent={};}
bool Current(){if(onCurrent)onCurrent(++currentChecks);return allowed;}
bool Read(){FVector location{91,92,93},scale{97,98,99};FRotator rotation{94,95,96};
 bool result=TryGetActorTransform(&objects[0],location,rotation,scale,Current);
 if(!result){CHECK(location.X==91&&location.Y==92&&location.Z==93);CHECK(rotation.Pitch==94&&rotation.Yaw==95&&rotation.Roll==96);CHECK(scale.X==97&&scale.Y==98&&scale.Z==99);}
 else{CHECK(location.X==1&&location.Y==2&&location.Z==3);CHECK(rotation.Pitch==4&&rotation.Yaw==5&&rotation.Roll==6);CHECK(scale.X==7&&scale.Y==8&&scale.Z==9);}
 return result;
}
int main(){
 Reset();CHECK(Read()&&calls==3);
 for(int getter=0;getter<3;++getter)for(int cause=0;cause<10;++cause){
  Reset();onCall=[=](int stage){if(stage!=getter)return;
   if(cause==0)++world;if(cause==1)++objects[0].serial;if(cause==2)objects[0].live=false;
   if(cause==3)++objects[1].serial;if(cause==4)worldObject=1;if(cause==5)++objects[getter+2].serial;
   if(cause==6)objects[getter+2].live=false;if(cause==7)bindings[getter]=7;
   if(cause==8)allowed=false;if(cause==9)haveWorld=false;};
  CHECK(!Read()&&calls==getter+1);
 }
 for(int getter=0;getter<3;++getter)for(int cause=0;cause<4;++cause){
  Reset();if(cause==0)bindings[getter]=-1;if(cause==1)frameValid[getter]=false;
  if(cause==2)callOkay[getter]=false;if(cause==3)readable[getter]=false;
  CHECK(!Read()&&calls==getter+(cause>=2?1:0));
 }
 for(int axis=0;axis<9;++axis){Reset();values[axis]=std::numeric_limits<float>::quiet_NaN();CHECK(!Read());}
 Reset();allowed=false;CHECK(!Read()&&calls==0);
 Reset();haveWorld=false;CHECK(!Read()&&calls==0);
 Reset();objects[0].live=false;CHECK(!Read()&&calls==0);
 Reset();onCurrent=[](int n){if(n==3)bindings[0]=7;};CHECK(!Read()&&calls==0);
 for(int getter=0;getter<3;++getter)for(int cause=0;cause<4;++cause){Reset();onCall=[=](int stage){if(stage!=getter)return;
  if(cause==0)++objects[8].serial;if(cause==1)objects[8].live=false;
  if(cause==2)objects[0].root=&objects[10];if(cause==3)++objects[9].serial;
 };CHECK(!Read()&&calls==getter+1);}
 for(int cause=0;cause<4;++cause){Reset();if(cause==0)objects[0].root=nullptr;if(cause==1)rootOffset=-1;
  if(cause==2)rootType=L"IntProperty";if(cause==3)objectSize=sizeof(void*)-1;
  CHECK(!Read()&&calls==0);
 }
 puts("PASS UI06 actual actor transform: 71 refusal/replacement/root/schema/finite boundaries preserve all outputs; complete transform commits once");
}
