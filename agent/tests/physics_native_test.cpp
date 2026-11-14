#ifndef PHYSICS_SOURCE
#define PHYSICS_SOURCE "../../src/votv-coop/src/ue_wrap/engine/engine_physics.cpp"
#endif
#include PHYSICS_SOURCE
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <map>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace E=ue_wrap::engine;
namespace test {
namespace R=ue_wrap::reflection;
struct Obj {void* cls=nullptr;void* outer=nullptr;void* super=nullptr;int id=0,serial=1;bool live=true;R::FName name{};};
struct Actor:Obj {void* parts[300]{};} actor;
struct Param {int off,size;uint8_t mask=0;};
struct Fn:Obj {const wchar_t* verb;int size=32;std::map<std::wstring,Param> params;};
Obj primitive,scene,componentClass,actorClass,nativeClass,otherClass,component,component2;
Fn funcs[5];
std::vector<Obj*> objects;
std::map<int,std::wstring> names;
std::vector<R::StructFieldInfo> fields;
std::wstring fieldType=L"ObjectProperty";
uint32_t world=1;
int calls=0,sets=0,lookups=0;bool classAvailable=true,functionAvailable=true,callOk=true,simulating=true,setOk=true;
uint8_t queryByte=0x04;
ue_wrap::FVector center{4,5,6};
std::function<void()> onClass,onFind,onFrame,onOffset,onNative,onFields,onType,onAncestry;
void Fire(std::function<void()>& fn){auto f=std::move(fn);fn={};if(f)f();}
void Init(Obj& o,void* cls,const wchar_t* n){o.cls=cls;o.live=true;++o.serial;o.super=nullptr;if(!o.id){o.id=int(objects.size()+1);objects.push_back(&o);}o.name={o.id,0};names[o.id]=n;}
void Reset(){
    onClass={};onFind={};onFrame={};onOffset={};onNative={};onFields={};onType={};onAncestry={};
    ++world;calls=sets=lookups=0;classAvailable=functionAvailable=callOk=simulating=setOk=true;queryByte=4;center={4,5,6};fieldType=L"ObjectProperty";
    Init(scene,&otherClass,L"SceneComponent");Init(primitive,&otherClass,L"PrimitiveComponent");primitive.super=&scene;
    Init(componentClass,&otherClass,L"MeshComponent");componentClass.super=&primitive;
    Init(actorClass,&otherClass,L"prop_C");Init(nativeClass,&otherClass,L"Actor");actorClass.super=&nativeClass;
    Init(otherClass,&otherClass,L"Other");Init(actor,&actorClass,L"prop");
    Init(component,&componentClass,L"mesh");Init(component2,&componentClass,L"mesh2");component.outer=component2.outer=&actor;
    actor.parts[0]=&component;actor.parts[1]=&component2;
    const wchar_t* verbs[]={L"SetSimulatePhysics",L"SetPhysicsLinearVelocity",L"SetPhysicsAngularVelocityInDegrees",L"IsSimulatingPhysics",L"GetCenterOfMass"};
    for(int i=0;i<5;++i){Init(funcs[i],&otherClass,verbs[i]);funcs[i].verb=verbs[i];funcs[i].outer=i==3?&scene:&primitive;funcs[i].size=32;funcs[i].params.clear();}
    funcs[0].params={{L"bSimulate",{0,1,0x04}}};
    funcs[1].params={{L"NewVel",{1,12}},{L"BoneName",{16,8}},{L"bAddToCurrent",{13,1,0x08}}};
    funcs[2].params={{L"NewAngVel",{1,12}},{L"BoneName",{16,8}},{L"bAddToCurrent",{13,1,0x08}}};
    funcs[3].params={{L"BoneName",{0,8}},{L"ReturnValue",{8,1,0x04}}};
    funcs[4].params={{L"BoneName",{0,8}},{L"ReturnValue",{9,12}}};
    fields={{L"mesh",int(offsetof(Actor,parts)),int(sizeof(void*)),ue_wrap::profile::cpf::InstancedReference}};
}
bool Invoke(int i){switch(i){case 0:return E::SetComponentSimulatePhysics(&component,true);case 1:return E::SetComponentLinearVelocity(&component,1,2,3);case 2:return E::SetComponentAngularVelocity(&component,1,2,3);case 3:return E::IsComponentSimulatingPhysics(&component);default:{ue_wrap::FVector v{9,9,9};return E::GetComponentCenterOfMass(&component,v);}}}
void Pass(int n){std::printf("PASS NP%02d\n",n);}
}
namespace ue_wrap::reflection {
void* ClassOf(void* p){return p?static_cast<test::Obj*>(p)->cls:nullptr;}
void* OuterOf(void* p){return p?static_cast<test::Obj*>(p)->outer:nullptr;}
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
void* ObjectAt(int32_t i){return i>0&&size_t(i)<=test::objects.size()?test::objects[i-1]:nullptr;}
bool IsLiveByIndex(void* p,int32_t i){return p&&p==ObjectAt(i)&&IsLive(p);}
int32_t SlotSerial(int32_t i){auto* p=static_cast<test::Obj*>(ObjectAt(i));return p?p->serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
const FName& NameOf(void* p){return static_cast<test::Obj*>(p)->name;}
std::wstring ToString(const FName& n){return test::names[n.ComparisonIndex];}
void* SuperStructOf(void* p){return p?static_cast<test::Obj*>(p)->super:nullptr;}
void* FindClass(const wchar_t*){++test::lookups;test::Fire(test::onClass);return test::classAvailable?&test::primitive:nullptr;}
bool IsDescendantOfAny(void* c,void* const* bases,size_t n,int depth){test::Fire(test::onAncestry);while(c&&depth--){for(size_t i=0;i<n;++i)if(c==bases[i])return true;c=SuperStructOf(c);}return false;}
void* FindDispatchFunction(void*,const wchar_t* name,void** declaring){test::Fire(test::onFind);if(!test::functionAvailable)return nullptr;for(auto& f:test::funcs)if(!std::wcscmp(name,f.verb)){if(declaring)*declaring=f.outer;return &f;}return nullptr;}
int32_t FunctionFrameSize(void* f){test::Fire(test::onFrame);return static_cast<test::Fn*>(f)->size;}
int32_t FindParamOffset(void* f,const wchar_t* n){test::Fire(test::onOffset);auto& p=static_cast<test::Fn*>(f)->params;return p.contains(n)?p[n].off:-1;}
int32_t FindParamSize(void* f,const wchar_t* n){auto& p=static_cast<test::Fn*>(f)->params;return p.contains(n)?p[n].size:-1;}
bool FindBoolProperty(void* f,const wchar_t* n,int32_t& off,uint8_t& mask){test::Fire(test::onOffset);auto& p=static_cast<test::Fn*>(f)->params;if(!p.contains(n))return false;off=p[n].off;mask=p[n].mask;return true;}
int32_t StructSize(void*){return sizeof(test::Actor);}
std::vector<StructFieldInfo> EnumerateStructFields(void*){test::Fire(test::onFields);return test::fields;}
bool PropertyOffsetAndClass(void*,const wchar_t* n,int32_t& off,std::wstring& type){test::Fire(test::onType);for(auto& f:test::fields)if(f.name==n){off=f.offset;type=test::fieldType;return true;}return false;}
bool CallFunction(void*,void* function,void* data){
    using namespace test;auto* f=static_cast<Fn*>(function);auto* bytes=static_cast<unsigned char*>(data);++calls;
    CHECK(reinterpret_cast<uintptr_t>(data)%16==0);
    for(const auto& [name,p]:f->params)CHECK(p.off>=0&&p.size>0&&p.size<=64&&p.off<=64-p.size);
    if(f==&funcs[0]){++sets;if(!setOk)return false;simulating=(bytes[f->params[L"bSimulate"].off]&f->params[L"bSimulate"].mask)!=0;}
    if(f==&funcs[1]||f==&funcs[2]){auto& add=f->params[L"bAddToCurrent"];CHECK(!(bytes[add.off]&add.mask));FVector v{};std::memcpy(&v,bytes+f->params[f==&funcs[1]?L"NewVel":L"NewAngVel"].off,sizeof(v));CHECK(v.X==1&&v.Y==2&&v.Z==3);}
    if(f==&funcs[3])bytes[f->params[L"ReturnValue"].off]=simulating?queryByte:0;
    if(f==&funcs[4])std::memcpy(bytes+f->params[L"ReturnValue"].off,&center,sizeof(center));
    Fire(onNative);return callOk;
}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return nullptr;}void* WorldOf(void*){return nullptr;}}

int main(){
    using namespace test;
    Reset();primitive.live=false;CHECK(!Invoke(0)&&calls==0);Pass(1);
    Reset();CHECK(Invoke(0));++funcs[0].serial;funcs[0].params[L"bSimulate"].off=64;CHECK(!Invoke(0)&&calls==1);Pass(2);
    Reset();CHECK(Invoke(3));++scene.serial;funcs[3].params[L"ReturnValue"].off=64;CHECK(!Invoke(3)&&calls==1);Pass(3);
    Reset();onFrame=[]{CHECK(!Invoke(0));};CHECK(Invoke(0)&&calls==1);Pass(4);
    Reset();onOffset=[]{++funcs[1].serial;};CHECK(!Invoke(1)&&calls==0);CHECK(Invoke(1));Pass(5);
    Reset();classAvailable=false;CHECK(!Invoke(0));classAvailable=true;functionAvailable=false;CHECK(!Invoke(0));functionAvailable=true;CHECK(Invoke(0));Pass(6);
    Reset();onFrame=[]{++world;};CHECK(!Invoke(0)&&calls==0);CHECK(Invoke(0));Pass(7);
    for(int n:{0,-1}){Reset();funcs[0].size=n;CHECK(!Invoke(0)&&calls==0);}Pass(8);
    Reset();funcs[0].size=65;CHECK(!Invoke(0)&&calls==0);Pass(9);
    Reset();funcs[0].params[L"bSimulate"].off=32;CHECK(!Invoke(0));Reset();funcs[0].params[L"bSimulate"].mask=3;CHECK(!Invoke(0));Pass(10);
    const int which[]={1,1,2,2,3,3,4,4};const wchar_t* params[]={L"NewVel",L"BoneName",L"NewAngVel",L"BoneName",L"BoneName",L"ReturnValue",L"BoneName",L"ReturnValue"};
    for(int k=0;k<8;++k){Reset();auto& p=funcs[which[k]].params[params[k]];p.off=32-p.size+1;CHECK(!Invoke(which[k])&&calls==0);Reset();funcs[which[k]].params[params[k]].size+=1;CHECK(!Invoke(which[k]));Pass(11+k);}
    Reset();funcs[1].params[L"BoneName"].off=4;CHECK(!Invoke(1)&&calls==0);Pass(19);
    Reset();funcs[1].params.erase(L"bAddToCurrent");CHECK(!Invoke(1));Reset();CHECK(Invoke(1)&&Invoke(2));Pass(20);
    Reset();CHECK(!E::SetComponentLinearVelocity(&component,NAN,2,3)&&calls==0);Pass(21);
    Reset();CHECK(!E::SetComponentAngularVelocity(&component,1,INFINITY,3)&&calls==0);Pass(22);
    Reset();center.X=NAN;ue_wrap::FVector out{9,9,9};CHECK(!E::GetComponentCenterOfMass(&component,out)&&out.X==9);Pass(23);
    Reset();onFrame=[]{++component.serial;};CHECK(!Invoke(0)&&calls==0);Pass(24);
    Reset();onFrame=[]{component.cls=&otherClass;};CHECK(!Invoke(0)&&calls==0);Pass(25);
    Reset();onClass=[]{++world;};CHECK(!Invoke(0)&&calls==0);Pass(26);
    Reset();onAncestry=[]{++primitive.serial;};CHECK(!Invoke(0)&&calls==0);Pass(27);
    Reset();onNative=[]{++component.serial;};CHECK(!Invoke(0));Pass(28);
    Reset();onNative=[]{++funcs[0].serial;};CHECK(!Invoke(0));Pass(29);
    Reset();onNative=[]{++scene.serial;};CHECK(!Invoke(3));Pass(30);
    Reset();callOk=false;CHECK(!Invoke(3)&&!Invoke(4));Pass(31);
    Reset();queryByte=1;CHECK(!Invoke(3));queryByte=4;CHECK(Invoke(3));Pass(32);
    Reset();onNative=[]{++world;};CHECK(!E::GetComponentCenterOfMass(&component,out)&&out.X==9);Pass(33);
    Reset();CHECK(Invoke(1)&&Invoke(2)&&E::GetComponentCenterOfMass(&component,out)&&out.X==4);Pass(34);
    Reset();onNative=[]{++funcs[4].serial;funcs[4].params[L"ReturnValue"].off=16;CHECK(Invoke(4));};CHECK(!Invoke(4));Pass(35);
    Reset();CHECK(E::StopActorSimulating(&actor)==1);++actorClass.serial;fields[0].offset+=sizeof(void*);component.outer=nullptr;simulating=true;CHECK(E::StopActorSimulating(&actor)==1);Pass(36);
    Reset();actorClass.super=&actorClass;CHECK(E::StopActorSimulating(&actor)==0&&calls==0);Pass(37);
    Reset();fields[0].offset=sizeof(Actor)-1;CHECK(E::StopActorSimulating(&actor)==0&&calls==0);Pass(38);
    Reset();fields.resize(1025,fields[0]);CHECK(E::StopActorSimulating(&actor)==0&&calls==0);Pass(39);
    Reset();fields.push_back(fields[0]);CHECK(E::StopActorSimulating(&actor)==1&&sets==1&&calls==2);Pass(40);
    Reset();fieldType=L"Int64Property";CHECK(E::StopActorSimulating(&actor)==0&&calls==0);Pass(41);
    Reset();onNative=[]{component.outer=&component2;};CHECK(E::StopActorSimulating(&actor)==0&&sets==0);Pass(42);
    Reset();onNative=[]{actor.parts[0]=&component2;};CHECK(E::StopActorSimulating(&actor)==0&&sets==0);Pass(43);
    Reset();setOk=false;CHECK(E::StopActorSimulating(&actor)==0&&sets==1);Pass(44);
    Reset();onNative=[]{CHECK(E::StopActorSimulating(&actor)==0);};CHECK(E::StopActorSimulating(&actor)==1&&sets==1);Pass(45);
    std::puts("PASS actual native physics wrapper: 45 boundaries");
}
