#define HANDTAKEN_NATIVE
#define INVENTORY_APPLY_NO_MAIN
#include "inventory_apply_test.cpp"
#include "ue_wrap/engine/engine_mainplayer.h"
#include "ue_wrap/core/sdk_profile.h"
namespace test {
template<class T>T Read(void* p,int offset){T value{};memcpy(&value,static_cast<uint8_t*>(p)+Field(p,offset),sizeof value);return value;}

std::function<void(const char*)> engineHook;bool frameOK=true,found=true;int updates=0,releases=0;
int releaseSize=sizeof(Obj),grabOffset=offsetof(Obj,fields),handleOffset=grabOffset+8,componentOffset=grabOffset+16;
std::wstring propertyType=L"ObjectProperty";int propertySize=8;
void Native(const char* name){auto fn=engineHook;if(fn)fn(name);}
void ResetNative(){engineHook={};Reset();frameOK=found=true;updates=releases=0;releaseSize=sizeof(Obj);grabOffset=offsetof(Obj,fields);handleOffset=grabOffset+8;componentOffset=grabOffset+16;propertyType=L"ObjectProperty";propertySize=8;Set(&objects[5],0,static_cast<void*>(&objects[6]));Set(&objects[5],8,static_cast<void*>(&objects[7]));Set(&objects[5],16,static_cast<void*>(&objects[8]));}
}
namespace ue_wrap::reflection {
void* FindFunction(void*,const wchar_t*){test::Native("resolve");return test::found?&test::objects[26]:nullptr;}
int32_t StructSize(void*){test::Native("layout");return test::releaseSize;}
bool PropertyOffsetAndClass(void*,const wchar_t* name,int32_t& offset,std::wstring& type){test::Native("property");type=test::propertyType;offset=!wcscmp(name,L"grabbing_actor")?test::grabOffset:!wcscmp(name,L"grabHandle")?test::handleOffset:test::componentOffset;return true;}
std::vector<StructFieldInfo> EnumerateStructFields(void*){return {{L"grabbing_actor",test::grabOffset,test::propertySize,0},{L"grabHandle",test::handleOffset,8,0},{L"grabbing_component",test::componentOffset,8,0}};}
bool CallFunction(void*,void*,void*){++test::releases;test::Native("release");return true;}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* fn):fn_(fn){test::Native("frame");if(!test::frameOK)fn_=nullptr;}
bool Call(void*,ParamFrame&){++test::updates;test::Native("update");return true;}
}
namespace ue_wrap::engine {namespace R=reflection;namespace P=profile;
#ifndef HANDTAKEN_NATIVE_SOURCE
#define HANDTAKEN_NATIVE_SOURCE "handtaken_native.inc"
#endif
#include HANDTAKEN_NATIVE_SOURCE
}
int main(){using namespace test;namespace E=ue_wrap::engine;
 ResetNative();CHECK(I::ApplyToSaveObject(Save(),Rich()));auto h=SR::ReadArr(Save(),0x450);std::array<uint8_t,0x120> before{};memcpy(before.data(),h.data,before.size());bool allowed=true;
 onNative=[&]{allowed=false;};CHECK(!I::WriteHeldItemWhile(L"None",nullptr,[&]{return allowed;})&&!memcmp(before.data(),h.data,before.size()));
 allowed=true;CHECK(I::WriteHeldItemWhile(L"None",nullptr,[&]{return allowed;}));I::EquipRecord empty;I::ReadEquipRecord(h.data,empty);CHECK(empty.propName.empty()&&empty.data.className.empty());
 for(const char* stage:{"resolve","frame","update"})for(int cause=0;cause<4;++cause){ResetNative();allowed=true;bool fired=false,dispatched=false;
 engineHook=[&](const char* name){if(!fired&&!strcmp(name,stage)){fired=true;if(cause==0)++objects[5].serial;if(cause==1)++objects[31].serial;if(cause==2)allowed=false;if(cause==3)++objects[26].serial;}};
 CHECK(!E::CallMainPlayerUpdateHoldWhile(&objects[5],[&]{return allowed;},&dispatched)||(!strcmp(stage,"resolve")&&cause==3));CHECK(fired);CHECK(updates==int(!strcmp(stage,"update")||(!strcmp(stage,"resolve")&&cause==3)));CHECK(dispatched==bool(updates));}
 ResetNative();frameOK=false;bool dispatched=true;CHECK(!E::CallMainPlayerUpdateHoldWhile(&objects[5],{},&dispatched)&&!dispatched&&!updates);
 for(int bad=0;bad<6;++bad){ResetNative();if(bad==0)propertyType=L"StructProperty";if(bad==1)releaseSize=4;if(bad==2)propertySize=16;if(bad==3)componentOffset=grabOffset+1;if(bad==4)componentOffset=-1;if(bad==5)found=false;CHECK(!E::ReleaseMainPlayerGrabIfHoldingWhile(&objects[5],&objects[6],{})&&!releases);CHECK(Read<void*>(&objects[5],0)==&objects[6]);}
 for(int cause=0;cause<8;++cause){ResetNative();allowed=true;engineHook=[&](const char* name){if(!strcmp(name,"release")){if(cause==0)++objects[5].serial;if(cause==1)++objects[6].serial;if(cause==2)++objects[7].serial;if(cause==3)allowed=false;if(cause==4)Set(&objects[5],0,static_cast<void*>(&objects[9]));if(cause==5)Set(&objects[5],16,static_cast<void*>(&objects[9]));if(cause==6)++objects[26].serial;if(cause==7)objects[7].cls=&objects[30];}};
 CHECK(!E::ReleaseMainPlayerGrabIfHoldingWhile(&objects[5],&objects[6],[&]{return allowed;})&&releases==1);CHECK(Read<void*>(&objects[5],0)!=nullptr);CHECK(Read<void*>(&objects[5],16)!=nullptr);}
 ResetNative();CHECK(E::ReleaseMainPlayerGrabIfHoldingWhile(&objects[5],&objects[6],{})&&releases==1&&!Read<void*>(&objects[5],0)&&!Read<void*>(&objects[5],16));
 puts("PASS HandTaken native inventory cancellation, updateHold dispatch receipts and typed grab-release callback guards");
}
