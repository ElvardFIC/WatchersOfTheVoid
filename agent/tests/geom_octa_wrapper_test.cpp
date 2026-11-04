#include "../../src/votv-coop/src/ue_wrap/actors/geom_octa.cpp"
#include <cassert>
#include <cstdio>
#include <cwchar>

namespace test {
struct Object {void* target=nullptr;void* sphere=nullptr;void* owner=nullptr;uint8_t anim=0;} objects[6];
int serials[6]={1,1,1,1,1,1};bool alive[6]={true,true,true,true,true,true};
int calls=0,boolReads=0,frameSize=8,offset=0;
bool boolReady=true,functionsReady=true,nameReady=false;
const wchar_t* failedParam=nullptr;
void* overlapComponent=nullptr;void* otherActor=nullptr;
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p){for(int i=0;i<6;++i)if(p==&test::objects[i])return i;return -1;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<6&&p==&test::objects[i]&&test::alive[i];}
bool IsLive(void* p){return IsLiveByIndex(p,InternalIndexOf(p));}
int32_t SlotSerial(int32_t i){return i>=0&&i<6?test::serials[i]:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ClassOf(void*){return &test::objects[4];}
int32_t InstanceOffset::Of(void* object){
    assert(IsLive(object));
    if(!wcscmp(prop_,L"targetActor"))return offsetof(test::Object,target);
    if(!wcscmp(prop_,L"minSphere"))return offsetof(test::Object,sphere);
    return offsetof(test::Object,owner);
}
int32_t ParamOffset::Of(void*){return test::offset;}
int32_t FunctionFrameSize(void*){return test::frameSize;}
void* FindFunction(void*,const wchar_t*){return test::functionsReady?&test::objects[5]:nullptr;}
bool FindBoolProperty(void*,const wchar_t*,int32_t& off,uint8_t& mask){
    ++test::boolReads;off=offsetof(test::Object,anim);mask=4;return test::boolReady;
}
bool NameEquals(const FName& name,const wchar_t*){return name.ComparisonIndex==42;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return 1;}
void* CurrentWorld(){return nullptr;}
void* WorldOf(void*){return nullptr;}
}
namespace ue_wrap::fname_utils {
reflection::FName StringToFName(const std::wstring&){return {test::nameReady?42:0,0};}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn),buf_(16){}
bool ParamFrame::SetRaw(const wchar_t* name,const void* value,int32_t size){
    if(test::failedParam&&!wcscmp(test::failedParam,name))return false;
    if(!wcscmp(name,L"OverlappedComponent")){assert(size==sizeof(void*));std::memcpy(&test::overlapComponent,value,size);}
    if(!wcscmp(name,L"OtherActor")){assert(size==sizeof(void*));std::memcpy(&test::otherActor,value,size);}
    return true;
}
bool Call(void*,ParamFrame&){++test::calls;return true;}
}
int main(){
    using namespace test;namespace G=ue_wrap::geom_octa;
    objects[0].sphere=&objects[1];
    assert(G::CallMinSphereBeginOverlap(&objects[0],&objects[2]));
    assert(calls==1&&overlapComponent==&objects[1]&&otherActor==&objects[2]);
    assert(G::CallMinSphereBeginOverlap(&objects[0],nullptr));assert(otherActor==nullptr);
    for(auto name:{L"OverlappedComponent",L"OtherActor"}){
        failedParam=name;assert(!G::CallMinSphereBeginOverlap(&objects[0],&objects[2]));assert(calls==2);
    }
    failedParam=nullptr;alive[2]=false;assert(!G::CallMinSphereBeginOverlap(&objects[0],&objects[2]));alive[2]=true;
    alive[1]=false;assert(!G::CallMinSphereBeginOverlap(&objects[0],&objects[2]));alive[1]=true;
    alive[0]=false;assert(!G::CallFireFinishNotify(&objects[0]));assert(!G::GraphFunction(&objects[0]));
    assert(!G::TargetActor(&objects[0])&&!G::MinSphere(&objects[0])&&!G::LaserOwner(&objects[0]));alive[0]=true;
    assert(!G::CallFireFinishNotify(&objects[0]));assert(calls==2);
    nameReady=true;failedParam=L"NotifyName";assert(!G::CallFireFinishNotify(&objects[0]));
    failedParam=nullptr;assert(G::CallFireFinishNotify(&objects[0]));assert(calls==3);
    ue_wrap::reflection::FName name{42,0};ue_wrap::script_gate::Call call{};
    call.function=&objects[5];call.locals=reinterpret_cast<uint8_t*>(&name);
    assert(G::IsFinishNotify(call));
    for(int size:{-1,0,7}){frameSize=size;assert(!G::IsFinishNotify(call));}
    frameSize=8;offset=1;assert(!G::IsFinishNotify(call));offset=-1;assert(!G::IsFinishNotify(call));
    boolReady=false;objects[0].anim=4;assert(!G::IsAnim(&objects[0]));
    boolReady=true;assert(G::IsAnim(&objects[0]));int before=boolReads;
    assert(G::IsAnim(&objects[0])&&boolReads==before);
    ++serials[4];assert(G::IsAnim(&objects[0])&&boolReads==before+1);
    puts("PASS Ramiel wrappers: checked overlap/notify parameters, missing names/functions, dead actors/components, bool metadata retry and class replacement");
}
