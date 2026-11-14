#include <algorithm>
#ifndef LIGHTSWITCH_SOURCE
#define LIGHTSWITCH_SOURCE "../../src/votv-coop/src/ue_wrap/devices/lightswitch.cpp"
#endif
#include LIGHTSWITCH_SOURCE
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include "facts_test_support.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <cstddef>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
namespace L=ue_wrap::lightswitch;
struct Obj {
 int id=0,serial=1;bool live=true;void* cls=nullptr;void* world=nullptr;
 ue_wrap::reflection::FName key{17,0};uint8_t flags=0x80;
 ue_wrap::field_io::TArrayView links{};void* bottom=nullptr;
} objects[32];
void *rootClass,*switchClass,*runFunction,*setFunction,*useFunction;
void *lampClass,*ambientClass,*lampFunction,*ambientFunction,*soundFunction,*attenuationValue;
void* links[2]{};
uint32_t world=1;
bool rootLoaded=true,switchLoaded=true,runLoaded=true,useLoaded=true;
std::wstring missingProperty,missingBool,missingParam;
int calls=0,lastIndex=-1;
bool lastBreaker=false,callOk=true;
bool soundLoaded=true,hookOk=true,attenuationOk=true,locationOk=true,emitReplay=false,nestedUse=false;
int spawns=0,plays=0,installs=0,soundFrame=16,soundOffset=0,volumeOffset=8,pitchOffset=12;
ue_wrap::FVector position{10,20,30};float playedVolume=0,playedPitch=0;
void* playedCue=nullptr;void* playedAttenuation=nullptr;
ue_wrap::ufunction_hook::PreNativeCallback soundHook=nullptr;
void* callbackFunction=nullptr;
struct SoundArgs{void* cue=nullptr;float volume=.75f,pitch=1.2f;} sound;
std::function<void()> onFrame,onSound,onCall,onClass,onLocation,onSpawn,onBool;
void* classCallbackTarget=nullptr;
void Callback(std::function<void()>& f){auto cb=std::move(f);f={};if(cb)cb();}
void Reset(){
 onFrame=onSound=onCall=onClass=onLocation=onSpawn=onBool={};classCallbackTarget=nullptr;++world;
 for(int i=0;i<32;++i){const int serial=objects[i].serial+1;objects[i]={};objects[i].id=i;objects[i].serial=serial;objects[i].cls=&objects[2];}
 rootClass=&objects[0];switchClass=&objects[1];runFunction=&objects[3];setFunction=&objects[4];useFunction=&objects[5];
 objects[8].cls=rootClass;objects[9].cls=switchClass;objects[16].cls=rootClass;objects[15].cls=switchClass;
 for(int i:{8,9,15,16})objects[i].world=reinterpret_cast<void*>(uintptr_t(world));
 lampClass=&objects[17];ambientClass=&objects[20];lampFunction=&objects[19];ambientFunction=&objects[24];soundFunction=&objects[6];attenuationValue=&objects[26];
 objects[18].cls=lampClass;objects[21].cls=ambientClass;objects[23].cls=objects[31].cls=&objects[22];
 objects[21].bottom=&objects[23];objects[25].cls=&objects[29];
 for(int i:{18,21,23,31})objects[i].world=reinterpret_cast<void*>(uintptr_t(world));
 links[0]=&objects[8];links[1]=&objects[16];objects[9].links={reinterpret_cast<uint8_t*>(links),2,2};
 rootLoaded=switchLoaded=runLoaded=useLoaded=true;missingProperty=missingBool=missingParam=L"";
 calls=0;lastIndex=-1;lastBreaker=false;callOk=true;
 soundLoaded=hookOk=attenuationOk=locationOk=true;emitReplay=nestedUse=false;
 spawns=plays=installs=0;soundFrame=16;soundOffset=0;volumeOffset=8;pitchOffset=12;
 position={10,20,30};playedVolume=playedPitch=0;playedCue=playedAttenuation=nullptr;soundHook=nullptr;callbackFunction=nullptr;sound={&objects[25],.75f,1.2f};
 L::g_resolved=false;L::g_swResolved=false;L::g_clickHookReady=false;L::g_clickAttenuation={};L::g_soundFunction={};
 L::g_replayedSwitch=nullptr;L::g_replayIdentity=nullptr;L::g_buildingAttenuation=false;
}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p && static_cast<test::Obj*>(p)->live;}
int32_t InternalIndexOf(void* p){return p?static_cast<test::Obj*>(p)->id:-1;}
int32_t SlotSerial(int32_t i){return test::objects[i].serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
bool IsLiveByIndex(void* p,int32_t i){return i>=0 && i<32 && p==&test::objects[i] && test::objects[i].live;}
void* ObjectAt(int32_t i){return i>=0 && i<32?&test::objects[i]:nullptr;}
void* ClassOf(void* p){if(p==test::classCallbackTarget)test::Callback(test::onClass);return p?static_cast<test::Obj*>(p)->cls:nullptr;}
bool IsDescendantOfAny(void* cls,void* const* bases,size_t count,int){for(size_t i=0;i<count;++i)if(cls==bases[i])return true;return false;}
void* FindClass(const wchar_t* n){
 if(!_wcsicmp(n,L"trigger_lightRoot_C"))return test::rootLoaded?test::rootClass:nullptr;
 if(!_wcsicmp(n,L"lightswitch_C"))return test::switchLoaded?test::switchClass:nullptr;
 if(!_wcsicmp(n,L"ceilingLamp_C"))return test::lampClass;
 if(!_wcsicmp(n,L"ambientLight_C"))return test::ambientClass;
 return nullptr;
}
void* FindFunction(void* cls,const wchar_t* n){
 if(cls==test::rootClass){if(!_wcsicmp(n,L"runTrigger"))return test::runLoaded?test::runFunction:nullptr;if(!_wcsicmp(n,L"SetActive"))return test::setFunction;}
 if(cls==test::switchClass && !_wcsicmp(n,L"use"))return test::useLoaded?test::useFunction:nullptr;
 return nullptr;
}
void* FindDispatchFunctionCached(void* cls,const wchar_t* n){
 if(!_wcsicmp(n,L"setActive")){if(cls==test::lampClass)return test::lampFunction;if(cls==test::ambientClass)return test::ambientFunction;}
 return FindFunction(cls,n);
}
int32_t FindPropertyOffset(void*,const wchar_t* n){
 if(!_wcsicmp(n,test::missingProperty.c_str()))return -1;
 if(!_wcsicmp(n,L"Key"))return offsetof(test::Obj,key);
 if(!_wcsicmp(n,L"objects"))return offsetof(test::Obj,links);
 if(!_wcsicmp(n,L"bottomLight"))return offsetof(test::Obj,bottom);
 return -1;
}
bool FindBoolProperty(void* cls,const wchar_t* n,int32_t& off,uint8_t& mask){
 off=-1;mask=0;if(!_wcsicmp(n,test::missingBool.c_str()))return false;
 if(cls==test::lampClass && !_wcsicmp(n,L"isActive"))mask=8;
 else if(cls==&test::objects[22] && !_wcsicmp(n,L"bVisible"))mask=32;
 else if(!_wcsicmp(n,L"IsActive"))mask=2;else if(!_wcsicmp(n,L"Active"))mask=4;else if(!_wcsicmp(n,L"A"))mask=16;else return false;
 off=offsetof(test::Obj,flags);test::Callback(test::onBool);return true;
}
const FName& NameOf(void* p){return static_cast<test::Obj*>(p)->key;}
std::wstring ToString(const FName& n){return std::to_wstring(n.ComparisonIndex);}
int32_t FunctionFrameSize(void* fn){test::Callback(test::onFrame);if(fn==test::lampFunction || fn==test::ambientFunction)return 1;if(fn==test::soundFunction)return test::soundFrame;return fn==&test::objects[3] || fn==&test::objects[12]?16:fn==&test::objects[4]?1:0;}
std::vector<ParamInfo> FunctionParams(void* fn){
 std::vector<ParamInfo> params;
 if(fn==&test::objects[3] || fn==&test::objects[12])params={{L"owner",0,8,0},{L"index",8,4,0}};
 if(fn==&test::objects[4])params={{L"active",0,1,0}};
 if(fn==test::lampFunction)params={{L"isActive",0,1,0}};
 if(fn==test::ambientFunction)params={{L"active",0,1,0}};
 for(auto it=params.begin();it!=params.end();)if(it->name==test::missingParam)it=params.erase(it);else ++it;
 return params;
}
int32_t FindParamOffset(void*,const wchar_t* n){if(!_wcsicmp(n,L"Sound"))return test::soundOffset;if(!_wcsicmp(n,L"VolumeMultiplier"))return test::volumeOffset;if(!_wcsicmp(n,L"PitchMultiplier"))return test::pitchOffset;return -1;}
bool CallFunction(void* actor,void* fn,void* frame){
 ++test::calls;
 if(fn==&test::objects[3] || fn==&test::objects[12]){void* owner=nullptr;std::memcpy(&owner,frame,8);CHECK(owner==&test::objects[8]);std::memcpy(&test::lastIndex,static_cast<uint8_t*>(frame)+8,4);}
 if(fn==&test::objects[4])std::memcpy(&test::lastBreaker,frame,1);
 if(fn==test::lampFunction){auto& flags=static_cast<test::Obj*>(actor)->flags;flags=(flags&~8)|(*static_cast<bool*>(frame)?8:0);}
 if(fn==test::ambientFunction){auto& flags=static_cast<test::Obj*>(static_cast<test::Obj*>(actor)->bottom)->flags;flags=(flags&~32)|(*static_cast<bool*>(frame)?32:0);}
 if((fn==test::useFunction) && test::emitReplay){
  CHECK(test::soundHook);const auto previous=test::callbackFunction;test::callbackFunction=test::soundFunction;
  CHECK(test::soundHook(nullptr,actor,reinterpret_cast<const uint8_t*>(&test::sound))==script_gate::Verdict::Cancel);
  test::callbackFunction=previous;
  if(test::nestedUse){test::nestedUse=false;const auto* outer=lightswitch::g_replayIdentity;CHECK(lightswitch::CallUse(&test::objects[15]));CHECK(lightswitch::g_replayedSwitch==actor && lightswitch::g_replayIdentity==outer);}
 }
 test::Callback(test::onCall);return test::callOk;
}
}
namespace ue_wrap::facts {Resolved Resolve(const Function& row){test::Callback(test::onSound);return test::soundLoaded?Resolved(test::soundFunction,&row):Resolved{};}}
namespace ue_wrap::object_index {void* ClassByName(const wchar_t* n){return reflection::FindClass(n);}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* p){return p?static_cast<test::Obj*>(p)->world:nullptr;}
}
namespace ue_wrap::engine {
void* SpawnSoundAttenuation(const SoundAttenuationConfig& cfg){CHECK(cfg.spatialize && cfg.attenuate && cfg.extents[0]==100 && cfg.falloffDistance==1400);++test::spawns;test::Callback(test::onSpawn);return test::attenuationOk?test::attenuationValue:nullptr;}
bool TryGetActorLocation(void*,FVector& at){at=test::position;test::Callback(test::onLocation);return test::locationOk;}
void PlaySoundAtLocation(void*,void* cue,const FVector&,void* attenuation,float volume,float pitch){CHECK(attenuation);++test::plays;test::playedCue=cue;test::playedAttenuation=attenuation;test::playedVolume=volume;test::playedPitch=pitch;}
}
namespace ue_wrap::ufunction_hook {
bool InstallPreHook(const facts::Resolved&,PreNativeCallback cb,bool){++test::installs;if(test::hookOk)test::soundHook=cb;return test::hookOk;}
void* CurrentNativeFunction(){return test::callbackFunction;}
}
namespace test {
void RootReload(){
 Reset();runLoaded=false;CHECK(!L::EnsureResolved());runLoaded=true;CHECK(L::EnsureResolved());
 CHECK(L::g_runTrigger.Get()==runFunction);++objects[3].serial;CHECK(!L::RootLayoutCurrent());CHECK(L::EnsureResolved());
 runFunction=&objects[12];CHECK(!L::RootLayoutCurrent());CHECK(L::EnsureResolved() && L::g_runTrigger.Get()==runFunction);
 rootClass=&objects[11];CHECK(!L::RootLayoutCurrent());CHECK(L::EnsureResolved() && L::g_rootClass.Get()==rootClass);
 bool value=true;CHECK(!L::TryReadActive(&objects[8],value) && value);objects[8].cls=rootClass;CHECK(L::TryReadActive(&objects[8],value) && !value);
}
void SwitchReload(){
 Reset();useLoaded=false;CHECK(!L::EnsureSwitchResolved());useLoaded=true;CHECK(L::EnsureSwitchResolved());
 ++objects[5].serial;CHECK(!L::SwitchLayoutCurrent());CHECK(L::EnsureSwitchResolved());
 useFunction=&objects[14];CHECK(!L::SwitchLayoutCurrent());CHECK(L::EnsureSwitchResolved() && L::g_use.Get()==useFunction);
 switchClass=&objects[13];CHECK(!L::SwitchLayoutCurrent());CHECK(L::EnsureSwitchResolved() && L::g_switchClass.Get()==switchClass);
 bool value=true;CHECK(!L::TryReadSwitchA(&objects[9],value) && value);objects[9].cls=switchClass;CHECK(L::TryReadSwitchA(&objects[9],value) && !value);
}
void RootFields(){
 for(const wchar_t* field:{L"Key",L"IsActive",L"Active"}){
  Reset();if(!_wcsicmp(field,L"Key"))missingProperty=field;else missingBool=field;
  CHECK(!L::EnsureResolved());bool value=true;CHECK(!L::TryReadActive(&objects[8],value) && value);
  CHECK(L::IsLightRoot(&objects[8])); // class predicates remain usable while fields wait
  missingProperty=missingBool=L"";CHECK(L::EnsureResolved());
 }
 Reset();rootLoaded=false;CHECK(!L::EnsureResolved());rootLoaded=true;CHECK(L::EnsureResolved());
}
void SwitchFields(){
 for(const wchar_t* field:{L"Key",L"objects",L"A"}){
  Reset();if(!_wcsicmp(field,L"A"))missingBool=field;else missingProperty=field;
  CHECK(!L::EnsureSwitchResolved());bool value=true;CHECK(!L::TryReadSwitchA(&objects[9],value) && value);
  CHECK(L::IsLightSwitch(&objects[9]));missingProperty=missingBool=L"";CHECK(L::EnsureSwitchResolved());
 }
}
void RootMask(){Reset();bool value=true;CHECK(L::TryReadActive(&objects[8],value) && !value);objects[8].flags|=2;CHECK(L::TryReadActive(&objects[8],value) && value);CHECK(objects[8].flags==0x82);objects[8].live=false;CHECK(!L::TryReadActive(&objects[8],value) && value);}
void SwitchMask(){Reset();bool value=true;CHECK(L::TryReadSwitchA(&objects[9],value) && !value);objects[9].flags|=16;CHECK(L::TryReadSwitchA(&objects[9],value) && value);CHECK(objects[9].flags==0x90);objects[9].cls=&objects[2];CHECK(!L::TryReadSwitchA(&objects[9],value) && value);}
void BreakerMask(){Reset();objects[8].flags|=2;bool value=true;CHECK(L::TryReadBreaker(&objects[8],value) && !value);objects[8].flags|=4;CHECK(L::TryReadBreaker(&objects[8],value) && value);CHECK(objects[8].flags==0x86);}
void Links(){
 Reset();CHECK(L::SwitchRoot(&objects[9])==&objects[8]);
 for(auto pair:{std::pair{-1,2},std::pair{2,1},std::pair{4097,4097},std::pair{1,5000},std::pair{1,-1}}){
  objects[9].links.num=pair.first;objects[9].links.max=pair.second;CHECK(!L::SwitchRoot(&objects[9]));
 }
 objects[9].links={nullptr,1,1};CHECK(!L::SwitchRoot(&objects[9]));
 Reset();classCallbackTarget=&objects[8];onClass=[]{objects[9].links.max=1;};CHECK(!L::SwitchRoot(&objects[9]));
 Reset();classCallbackTarget=&objects[8];onClass=[]{++objects[8].serial;};CHECK(!L::SwitchRoot(&objects[9]));
 Reset();links[0]=&objects[10];CHECK(!L::SwitchRoot(&objects[9]));
}
void GroupCalls(){
 for(const wchar_t* field:{L"owner",L"index"}){Reset();missingParam=field;CHECK(!L::ApplyGroupState(&objects[8],true) && !calls);}
 Reset();missingParam=L"active";CHECK(!L::CallSetBreaker(&objects[8],true) && !calls);
 for(int change=0;change<5;++change){Reset();onFrame=[change]{
  if(change==0)++objects[8].serial;if(change==1)++world;if(change==2)objects[8].cls=&objects[2];
  if(change==3)++objects[3].serial;if(change==4){runFunction=&objects[12];CHECK(L::EnsureResolved());}
 };CHECK(!L::ApplyGroupState(&objects[8],true) && !calls);}
 Reset();CHECK(L::ApplyGroupState(&objects[8],true) && calls==1 && lastIndex==1);CHECK(L::ApplyGroupState(&objects[8],false) && lastIndex==2);
 CHECK(L::CallSetBreaker(&objects[8],true) && lastBreaker);CHECK(L::CallSetBreaker(&objects[8],false) && !lastBreaker);
 Reset();onCall=[]{++world;};CHECK(!L::ApplyGroupState(&objects[8],true) && calls==1);
}
void UseLifetime(){
 for(int change=0;change<5;++change){Reset();onFrame=[change]{
  if(change==0)++objects[9].serial;if(change==1)++world;if(change==2)objects[9].cls=&objects[2];
  if(change==3)++objects[5].serial;if(change==4){useFunction=&objects[14];CHECK(L::EnsureSwitchResolved());}
 };CHECK(!L::CallUse(&objects[9]) && !calls && !L::g_replayedSwitch);}
 Reset();onSound=[]{++objects[9].serial;};CHECK(!L::CallUse(&objects[9]) && !calls);
 Reset();onCall=[]{++world;};CHECK(!L::CallUse(&objects[9]) && calls==1 && !L::g_replayedSwitch);
 Reset();CHECK(L::CallUse(&objects[9]) && calls==1 && !L::g_replayedSwitch);
}
}
int main(int argc,char** argv){
 using namespace test;struct Case{const char* name;void(*run)();};
 const Case cases[]={{"root-reload",RootReload},{"switch-reload",SwitchReload},{"root-fields",RootFields},{"switch-fields",SwitchFields},
  {"root-mask",RootMask},{"switch-mask",SwitchMask},{"breaker-mask",BreakerMask},{"links",Links},{"group-calls",GroupCalls},{"use-lifetime",UseLifetime}};
 bool ran=false;for(const auto& item:cases)if(argc<2 || std::string(argv[1])==item.name){item.run();std::printf("PASS lightswitch native %s\n",item.name);ran=true;}CHECK(ran);return 0;
}
