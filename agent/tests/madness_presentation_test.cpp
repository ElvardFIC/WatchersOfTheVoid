#ifndef MADNESS_NATIVE_SOURCE
#define MADNESS_NATIVE_SOURCE "../../src/votv-coop/src/ue_wrap/world/madness.cpp"
#endif
#include MADNESS_NATIVE_SOURCE
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int id=0,serial=1,kind=-1;bool live=true,playing=true;void* cls=nullptr;void* world=nullptr;
 void* hud=nullptr;void* music=nullptr;void* text=nullptr;int count=4,calls=0;ue_wrap::reflection::FName name{};}objects[32];
uint32_t generation=1;uintptr_t world=100;int widgets=0,starts=0,shows=0,stops=0,removes=0,labels=0;
float seek=0;std::wstring text,missingParam;
int missingKind=-1;bool factoryOk=true,showOk=true,queryOk=true,stopOk=true,removeOk=true,produceOnFailure=false;
void* widgetResult=&objects[1];void* audioResult=&objects[2];void* context=&objects[4];void* widgetLibrary=&objects[5];void* audioLibrary=&objects[6];
void* widgetType=&objects[17];void* masterType=&objects[16];void* audioType=&objects[18];
std::array<void*,7> functions{};
std::function<void()> onFrame,onCall,onField,onText,onContext;std::wstring frameName,callName,fieldName;
Obj& O(void* p){return *static_cast<Obj*>(p);}void At(std::function<void()>& f){auto copy=std::move(f);f={};if(copy)copy();}
const wchar_t* names[]={L"Create",L"SpawnSound2D",L"AddToViewport",L"IsPlaying",L"GetGameTimeSinceCreation",L"Stop",L"RemoveFromParent"};
void Reset(){
 onFrame=onCall=onField=onText=onContext={};++generation;++world;
 for(int i=0;i<32;++i){int serial=objects[i].serial+1;objects[i]={};objects[i].id=i;objects[i].serial=serial;objects[i].cls=&objects[23];objects[i].name={i,0};}
 masterType=&objects[16];widgetType=&objects[17];audioType=&objects[18];objects[0].cls=masterType;objects[0].world=reinterpret_cast<void*>(world);
 objects[1].cls=widgetType;objects[2].cls=audioType;objects[2].world=reinterpret_cast<void*>(world);objects[3].cls=&objects[19];
 objects[4].cls=&objects[20];objects[5].cls=&objects[21];objects[6].cls=&objects[22];objects[7].cls=&objects[23];
 objects[0].hud=&objects[1];objects[0].music=&objects[2];objects[1].text=&objects[3];
 objects[27].cls=audioType;objects[27].world=reinterpret_cast<void*>(world);objects[28].cls=widgetType;objects[28].text=&objects[29];objects[29].cls=&objects[19];
 for(int i=0;i<7;++i){functions[i]=&objects[8+i];objects[8+i].kind=i;}
 widgetResult=&objects[1];audioResult=&objects[2];context=&objects[4];widgetLibrary=&objects[5];audioLibrary=&objects[6];
 widgets=starts=shows=stops=removes=labels=0;seek=0;text=missingParam=frameName=callName=fieldName=L"";missingKind=-1;
 factoryOk=showOk=queryOk=stopOk=removeOk=true;produceOnFailure=false;
}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&test::O(p).live;}void* ClassOf(void* p){return p?test::O(p).cls:nullptr;}
int32_t InternalIndexOf(void* p){return p?test::O(p).id:-1;}int32_t SlotSerial(int32_t i){return test::objects[i].serial;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<32&&p==&test::objects[i]&&test::objects[i].live;}
void* ObjectAt(int32_t i){return i>=0&&i<32?&test::objects[i]:nullptr;}
bool IsDescendantOfAny(void* cls,void* const* base,size_t count,int){for(size_t i=0;i<count;++i)if(cls==base[i]||(cls==&test::objects[25]&&base[i]==test::masterType))return true;return false;}
void* FindClass(const wchar_t* n){return !wcscmp(n,L"AudioComponent")?test::audioType:nullptr;}
void* FindClassDefaultObject(const wchar_t* n){return !wcscmp(n,L"WidgetBlueprintLibrary")?test::widgetLibrary:test::audioLibrary;}
void* FindDispatchFunctionCached(void*,const wchar_t* name){for(int i=0;i<7;++i)if(!wcscmp(name,test::names[i]))return test::functions[i];return nullptr;}
int32_t FindPropertyOffset(void*,const wchar_t* name){
 int result=!wcscmp(name,L"killcount")?offsetof(test::Obj,hud):!wcscmp(name,L"MC_music")?offsetof(test::Obj,music):!wcscmp(name,L"count")?offsetof(test::Obj,count):!wcscmp(name,L"text_count")?offsetof(test::Obj,text):-1;
 if(test::fieldName==name)test::At(test::onField);return result;
}
const FName& NameOf(void* p){return test::O(p).name;}std::wstring ToString(const FName&){return L"function";}
int32_t FunctionFrameSize(void* fn){const int k=test::O(fn).kind;if(test::frameName==test::names[k])test::At(test::onFrame);return k==0?32:k==1?56:k==2?4:k==3?1:k==4?4:0;}
std::vector<ParamInfo> FunctionParams(void* fn){
 const int k=test::O(fn).kind;std::vector<ParamInfo> p;
 if(k==0)p={{L"WorldContextObject",0,8,0},{L"WidgetType",8,8,0},{L"OwningPlayer",16,8,0},{L"ReturnValue",24,8,0}};
 if(k==1)p={{L"WorldContextObject",0,8,0},{L"Sound",8,8,0},{L"VolumeMultiplier",16,4,0},{L"PitchMultiplier",20,4,0},{L"StartTime",24,4,0},{L"ConcurrencySettings",32,8,0},{L"bPersistAcrossLevelTransition",40,1,0},{L"bAutoDestroy",41,1,0},{L"ReturnValue",48,8,0}};
 if(k==2)p={{L"ZOrder",0,4,0}};if(k==3)p={{L"ReturnValue",0,1,0}};if(k==4)p={{L"ReturnValue",0,4,0}};
 if(k==test::missingKind)for(auto it=p.begin();it!=p.end();)if(it->name==test::missingParam)it=p.erase(it);else ++it;
 return p;
}
bool CallFunction(void* actor,void* fn,void* args){using namespace test;const int k=O(fn).kind;++O(actor).calls;
 bool ok=true;
 if(k==0||k==1){ok=factoryOk;if(ok||produceOnFailure){void* result=k==0?widgetResult:audioResult;
  std::memcpy(static_cast<uint8_t*>(args)+(k==0?24:48),&result,8);
  if(k==0)++widgets;else{++starts;std::memcpy(&seek,static_cast<uint8_t*>(args)+24,4);if(result)O(result).playing=true;}}}
 else if(k==2){++shows;ok=showOk;}
 else if(k==3){ok=queryOk;if(ok)std::memcpy(args,&O(actor).playing,1);}
 else if(k==4){const float age=20;std::memcpy(args,&age,4);}
 else if(k==5){++stops;ok=stopOk;if(ok)O(actor).playing=false;}
 else if(k==6){++removes;ok=removeOk;}
 if(callName==names[k])At(onCall);return ok;
}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::generation;}void* CurrentWorld(){return reinterpret_cast<void*>(test::world);}void* WorldOf(void* p){return p?test::O(p).world:nullptr;}}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* n){return !wcscmp(n,L"ui_madnessCombat_C")?test::widgetType:!wcscmp(n,L"madnessCombatMaster_C")?test::masterType:nullptr;}
size_t ForEachInstance(void*,void(*fn)(void*,void*,int32_t),void* data){fn(data,&test::objects[0],0);return 1;}
}
namespace ue_wrap::engine {void* GetWorldContext(){auto p=test::context;test::At(test::onContext);return p;}void* FindSound(const std::wstring&){return &test::objects[7];}}
namespace ue_wrap::component_calls {bool SetText(void*,const wchar_t* text){test::text=text;++test::labels;test::At(test::onText);return true;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace test {
namespace M=ue_wrap::madness;
M::State State(){return {true,true,true,4,20};}
void FieldRead(){Reset();objects[0].cls=&objects[25];M::State out{};CHECK(M::Read(&objects[0],out)&&out.kills==4);out.kills=99;
 fieldName=L"count";onField=[]{++objects[17].serial;};CHECK(!M::Read(&objects[0],out)&&out.kills==99);}
void HudRead(){Reset();M::State out{};out.kills=99;callName=L"IsPlaying";onCall=[]{objects[0].hud=&objects[28];};CHECK(!M::Read(&objects[0],out)&&out.kills==99);}
void MissingAudio(){Reset();objects[0].music=nullptr;M::State out{};CHECK(M::Read(&objects[0],out)&&out.active&&out.hud&&!out.playing&&out.kills==4);}
void ReadFunctions(){Reset();M::State out{};out.kills=99;frameName=L"IsPlaying";onFrame=[]{++objects[11].serial;};CHECK(!M::Read(&objects[0],out)&&out.kills==99);
 Reset();missingKind=3;missingParam=L"ReturnValue";CHECK(!M::Read(&objects[0],out));}
void WidgetType(){Reset();M::View v;auto s=State();s.playing=false;widgetResult=&objects[26];CHECK(!M::Apply(v,s)&&!v.widget.Raw()&&!removes);
 Reset();frameName=L"Create";onFrame=[]{++objects[5].serial;};CHECK(!M::Apply(v,s)&&!widgets);}
void ViewportRetry(){Reset();M::View v;auto s=State();s.playing=false;showOk=false;CHECK(!M::Apply(v,s)&&widgets==1&&!v.shown);showOk=true;CHECK(M::Apply(v,s)&&widgets==1&&v.shown);CHECK(M::Apply(v,s)&&shows==2);M::Clear(v);}
void AudioType(){Reset();M::View v;auto s=State();s.hud=false;audioResult=&objects[26];CHECK(!M::Apply(v,s)&&!v.audio.Raw()&&!stops);
 Reset();CHECK(M::Apply(v,s)&&starts==1&&seek==20);CHECK(M::Apply(v,s)&&starts==1);
 objects[2].playing=false;s.elapsed=30;CHECK(M::Apply(v,s)&&starts==2&&seek==30);
 ++objects[2].serial;s.elapsed=40;CHECK(M::Apply(v,s)&&starts==3&&seek==40);
 queryOk=false;CHECK(!M::Apply(v,s)&&starts==3);queryOk=true;
 objects[2].playing=false;factoryOk=false;CHECK(!M::Apply(v,s)&&!v.started&&starts==3);factoryOk=true;
 CHECK(M::Apply(v,s)&&starts==4);M::Clear(v);}
void FactoryWorld(){for(bool audio:{false,true}){Reset();M::View v;auto s=State();s.playing=audio;s.hud=!audio;frameName=audio?L"SpawnSound2D":L"Create";onFrame=[]{++generation;};CHECK(!M::Apply(v,s)&&!widgets&&!starts);
 Reset();M::View after;callName=audio?L"SpawnSound2D":L"Create";onCall=[]{++generation;};CHECK(!M::Apply(after,s)&&!after.audio.Raw()&&!after.widget.Raw());
 Reset();M::View contextView;onContext=[]{context=&objects[26];};CHECK(!M::Apply(contextView,s)&&!widgets&&!starts);
}}
void ReturnRead(){for(bool audio:{false,true}){Reset();M::View v;auto s=State();s.playing=audio;s.hud=!audio;missingKind=audio?1:0;missingParam=L"ReturnValue";CHECK(!M::Apply(v,s)&&!v.widget.Raw()&&!v.audio.Raw()&&!widgets&&!starts);}
 Reset();M::View v;auto s=State();s.hud=false;factoryOk=false;produceOnFailure=true;CHECK(!M::Apply(v,s)&&v.started&&starts==1);factoryOk=true;CHECK(M::Apply(v,s)&&starts==1);M::Clear(v);}
void LabelBinding(){Reset();M::View v;auto s=State();CHECK(M::Apply(v,s)&&text==L"KILLS: 4"&&objects[1].count==4);s.kills=9;onText=[]{objects[1].text=&objects[29];};CHECK(!M::Apply(v,s)&&v.kills==4&&objects[1].count==4);
 objects[1].text=&objects[3];onText=[&]{s.kills=99;};CHECK(M::Apply(v,s)&&v.kills==9&&objects[1].count==9);M::Clear(v);}
void OwnedCleanup(){for(bool reuse:{false,true}){Reset();M::View v;CHECK(M::Apply(v,State()));if(reuse){++objects[2].serial;++objects[1].serial;}else{objects[2].cls=&objects[23];objects[1].cls=&objects[23];}M::Clear(v);CHECK(!stops&&!removes&&!v.audio.Raw()&&!v.widget.Raw());}}
void Reentry(){Reset();M::View v;auto s=State();callName=L"SpawnSound2D";onCall=[&]{audioResult=&objects[27];widgetResult=&objects[28];auto fresh=s;fresh.kills=9;CHECK(M::Apply(v,fresh));};
 CHECK(!M::Apply(v,s)&&v.audio.Is(&objects[27])&&v.widget.Is(&objects[28])&&v.kills==9);CHECK(objects[27].playing);M::Clear(v);
 Reset();M::View current;CHECK(M::Apply(current,s));callName=L"Stop";onCall=[&]{audioResult=&objects[27];widgetResult=&objects[28];auto fresh=s;fresh.kills=8;CHECK(M::Apply(current,fresh));};M::Clear(current);
 CHECK(current.audio.Is(&objects[27])&&current.widget.Is(&objects[28])&&current.kills==8&&objects[27].playing);M::Clear(current);}
void CleanupRetry(){Reset();M::View v;CHECK(M::Apply(v,State()));stopOk=removeOk=false;M::State end{};CHECK(!M::Apply(v,end)&&v.audio.Raw()&&v.widget.Raw());stopOk=removeOk=true;CHECK(M::Apply(v,end)&&!v.audio.Raw()&&!v.widget.Raw()&&stops==2&&removes==2);}
}
int main(){using namespace test;void(*groups[])()={FieldRead,HudRead,MissingAudio,ReadFunctions,WidgetType,ViewportRetry,AudioType,FactoryWorld,ReturnRead,LabelBinding,OwnedCleanup,Reentry,CleanupRetry};for(int i=0;i<13;++i){groups[i]();std::printf("PASS Madness recovery %d\n",i+13);}return 0;}
