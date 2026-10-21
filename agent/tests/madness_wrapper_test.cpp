#include "../../src/votv-coop/src/ue_wrap/world/madness.cpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Obj {int index=0,serial=1;bool live=true;void* text=nullptr;void* music=nullptr;void* hud=nullptr;int count=4;} objs[5];
std::map<std::wstring,std::wstring> functions;
bool playing=false,queryOk=true,spawnOk=true;
bool showOk=true,resultOk=true;uint32_t world=1;
std::function<void()> callback;std::wstring after;
void Callback(const wchar_t* name){if(after!=name)return;auto f=std::move(callback);callback={};if(f)f();}
int starts=0,stops=0,widgets=0,removes=0,labels=0;
float startAt=0;
std::wstring label;
void* Fn(const wchar_t* name){auto& s=functions[name];s=name;return &s;}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&static_cast<test::Obj*>(p)->live;}
void* ClassOf(void* p){return p;}
int32_t InternalIndexOf(void* p){return static_cast<test::Obj*>(p)->index;}
int32_t AllocateSlotSerial(int32_t i){return test::objs[i].serial;}
int32_t SlotSerial(int32_t i){return test::objs[i].serial;}
bool IsLiveByIndex(void* p,int32_t i){return i>=0&&i<5&&p==&test::objs[i]&&IsLive(p);}
void* FindClassDefaultObject(const wchar_t*){return &test::objs[0];}
void* FindDispatchFunctionCached(void*,const wchar_t* name){return test::Fn(name);}
int32_t FindPropertyOffset(void*,const wchar_t* name){
 if(!wcscmp(name,L"MC_music"))return offsetof(test::Obj,music);
 if(!wcscmp(name,L"killcount"))return offsetof(test::Obj,hud);
 if(!wcscmp(name,L"count"))return offsetof(test::Obj,count);
 CHECK(!wcscmp(name,L"text_count"));return offsetof(test::Obj,text);}
const FName& NameOf(void*){static FName name{};return name;}
bool NameEquals(const FName&,const wchar_t* text){return !wcscmp(text,L"madnessCombatMaster_C");}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}void* CurrentWorld(){return &test::objs[0];}void* WorldOf(void*){return CurrentWorld();}
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* name){CHECK(!wcscmp(name,L"ui_madnessCombat_C"));return &test::objs[0];}
size_t ForEachInstance(void*,void(*)(void*,void*,int32_t),void*){std::abort();}
}
namespace ue_wrap::engine {
void* GetWorldContext(){return &test::objs[0];}
void* FindSound(const std::wstring& name){CHECK(name==L"SoundWave /Game/audio/music/MC_music.MC_music");return &test::objs[0];}
}
namespace ue_wrap {
ParamFrame::ParamFrame(void* f):fn_(f){}
bool ParamFrame::SetRaw(const wchar_t* name,const void* p,int32_t n){
 if(!wcscmp(name,L"StartTime")){CHECK(n==4);std::memcpy(&test::startAt,p,4);}return true;
}
bool ParamFrame::GetRaw(const wchar_t*,void* p,int32_t n)const {
 const auto& name=*static_cast<std::wstring*>(fn_);
 if(name==L"IsPlaying"){CHECK(n==1);std::memcpy(p,&test::playing,1);return test::resultOk;}
 if(name==L"GetGameTimeSinceCreation"){CHECK(n==4);float age=20;std::memcpy(p,&age,4);return test::resultOk;}
 void* result=name==L"Create"?static_cast<void*>(&test::objs[2]):static_cast<void*>(&test::objs[1]);
 CHECK(n==8);std::memcpy(p,&result,8);return true;
}
bool Call(void*,ParamFrame& f) {
 const auto& name=*static_cast<std::wstring*>(f.function());
 if(name==L"IsPlaying"){test::Callback(name.c_str());return test::queryOk;}
 if(name==L"GetGameTimeSinceCreation"){test::Callback(name.c_str());return true;}
 if(name==L"SpawnSound2D"){
  if(!test::spawnOk)return false;++test::starts;test::objs[1].live=true;test::playing=true;test::Callback(name.c_str());return true;
 }
 if(name==L"Create"){++test::widgets;test::objs[2].live=true;test::Callback(name.c_str());return true;}
 CHECK(name==L"AddToViewport");test::Callback(name.c_str());return test::showOk;
}
}
namespace ue_wrap::component_calls {
bool CallParamlessNamed(void* p,const wchar_t* name){
 if(!wcscmp(name,L"Stop")){CHECK(p==&test::objs[1]);++test::stops;test::playing=false;test::Callback(name);return true;}
 CHECK(!wcscmp(name,L"RemoveFromParent")&&p==&test::objs[2]);++test::removes;test::Callback(name);return true;
}
bool SetText(void* p,const wchar_t* text){CHECK(p==&test::objs[3]);test::label=text;++test::labels;test::Callback(L"SetText");return true;}
}
int main(){using namespace test;namespace M=ue_wrap::madness;
 for(int i=0;i<5;++i)objs[i].index=i;objs[2].text=&objs[3];objs[4].music=&objs[1];objs[4].hud=&objs[2];
 M::View view;M::State state{true,true,true,4,20.25f};
 CHECK(M::Apply(view,state)&&view.started&&starts==1&&startAt==20.25f&&widgets==1&&label==L"KILLS: 4");
 CHECK(M::Apply(view,state)&&starts==1&&widgets==1&&labels==1); // no unchanged restart
 state.elapsed=25;playing=false;CHECK(M::Apply(view,state)&&starts==2&&startAt==25);
 objs[1].live=false;++objs[1].serial;state.elapsed=30;CHECK(M::Apply(view,state)&&starts==3&&startAt==30);
 queryOk=false;CHECK(!M::Apply(view,state)&&starts==3);queryOk=true; // failed query never duplicates live audio
 playing=false;spawnOk=false;CHECK(!M::Apply(view,state)&&!view.started&&starts==3);
 spawnOk=true;state.elapsed=31;CHECK(M::Apply(view,state)&&starts==4&&startAt==31);
 objs[2].live=false;++objs[2].serial;state.kills=5;CHECK(M::Apply(view,state)&&widgets==2&&label==L"KILLS: 5");
 state.playing=false;CHECK(M::Apply(view,state)&&!view.started&&stops==1);
 state.hud=false;CHECK(M::Apply(view,state)&&removes==1);
 state={};CHECK(M::Apply(view,state)&&starts==4&&removes==1);
 // Retain a created widget when its viewport is not ready, then retry without creating another.
 state={true,true,false,6,40};showOk=false;auto beforeWidgets=widgets;
 CHECK(!M::Apply(view,state) && widgets==beforeWidgets+1 && view.widget.Get() && !view.shown);
 showOk=true;CHECK(M::Apply(view,state) && view.shown && widgets==beforeWidgets+1);M::Clear(view);
 bool current=true;const auto valid=[&]{return current;};state.playing=true;
 for(const wchar_t* name:{L"SpawnSound2D",L"Create",L"AddToViewport",L"SetText"}){
  current=true;after=name;callback=[&]{current=false;};auto beforeLabels=labels;
  CHECK(!M::Apply(view,state,valid));if(after!=L"SetText")CHECK(labels==beforeLabels);
  M::Clear(view);CHECK(!view.audio.Get() && !view.widget.Get());
 }
 current=true;CHECK(M::Apply(view,state,valid));auto beforeStarts=starts;beforeWidgets=widgets;
 resultOk=false;CHECK(!M::Apply(view,state) && starts==beforeStarts && widgets==beforeWidgets);resultOk=true;
 after=L"IsPlaying";callback=[]{++objs[1].serial;};CHECK(!M::Apply(view,state));M::Clear(view);
 CHECK(M::Apply(view,state));auto beforeRemoves=removes;auto beforeStops=stops;
 ++world;M::Clear(view);CHECK(removes==beforeRemoves+1 && stops==beforeStops);
 CHECK(M::Apply(view,state));after=L"Stop";callback=[]{++world;};M::Clear(view);CHECK(removes==beforeRemoves+2);
 CHECK(M::Apply(view,state));beforeRemoves=removes;++world;CHECK(M::Apply(view,state) && removes==beforeRemoves+1);
 M::State sampled{};CHECK(M::Read(&objs[4],sampled) && sampled.kills==4 && sampled.elapsed==20);
 for(const wchar_t* name:{L"IsPlaying",L"GetGameTimeSinceCreation"}) {
  after=name;callback=[]{++world;};CHECK(!M::Read(&objs[4],sampled));
  callback=[]{++objs[4].serial;};CHECK(!M::Read(&objs[4],sampled));
 }
 after=L"IsPlaying";callback=[]{objs[4].music=nullptr;};CHECK(!M::Read(&objs[4],sampled));objs[4].music=&objs[1];
 resultOk=false;CHECK(!M::Read(&objs[4],sampled));resultOk=true;
 puts("PASS Madness presentation: seek, no unchanged restart, stopped/dead component recovery, query/spawn/viewport retry, HUD recovery, actual read/cleanup and native callback identity/world fences");
}
