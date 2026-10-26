#include "../../src/votv-coop/src/ue_wrap/actors/llama_events.cpp"
#include "../../src/votv-coop/src/coop/props/llama_events.cpp"
#include "../../src/votv-coop/src/ue_wrap/actors/llama.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL llama events %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
namespace T=coop::llama_events;namespace M=ue_wrap::llama_events;namespace R=ue_wrap::reflection;
enum Id {Actor=1,Second=2,Other=3,Gamemode=400,Light,Audio,Class=600,GmClass,ForeignClass,SetEvent=610,LightClass,AudioClass,Visibility,Activate,Count=1024};
struct Obj {int id=0,serial=1;bool live=true,foreignWorld=false;void* cls=nullptr;void* parent=nullptr;R::FName name{};
    uint8_t flags=0xA5;int32_t count=0;void** data=nullptr;int32_t num=0,max=0;void* light=nullptr;void* audio=nullptr;bool visible=false;} objects[Count];
Obj& O(void* p){CHECK(p);return *static_cast<Obj*>(p);}void* At(int i){return &objects[i];}
uint32_t generation=1,serial=1;uintptr_t world=100;uint64_t now=1000;
coop::net::Session session;bool hand=false,watch=true,gmFound=true,wrongCount=false,wrongArray=false,fieldMissing=false;
int holds=0,acquires=0,releases=0,readiness=0,cues=0;std::function<void()> onField;std::vector<void*> instances;void* senders[70000]{};
std::wstring badParam;int badSize=-1,badOffset=-1,frameSize=16;uint8_t mask=1;
void Fire(){auto action=std::move(onField);onField={};if(action)action();}
void Reset(){++generation;++serial;++world;T::OnDisconnect();
    for(int i=0;i<Count;++i){objects[i]={};objects[i].id=i;objects[i].serial=int(generation*10000)+i;objects[i].cls=At(i);objects[i].name.ComparisonIndex=i;}
    for(int i=1;i<=300;++i)objects[i].cls=At(Class);objects[Gamemode].cls=At(GmClass);
    objects[Light].cls=At(LightClass);objects[Audio].cls=At(AudioClass);objects[Actor].light=At(Light);objects[Actor].audio=At(Audio);
    session={};session.mode=coop::net::Role::Client;now=1000;hand=false;watch=gmFound=true;wrongCount=wrongArray=fieldMissing=false;
    onField={};instances={At(Actor)};badParam.clear();badSize=badOffset=-1;frameSize=16;mask=1;acquires=releases=readiness=cues=0;
    std::fill(std::begin(senders),std::end(senders),nullptr);objects[Gamemode].data=senders;objects[Gamemode].max=700;
}
void Ledger(std::initializer_list<void*> values,int32_t count){auto& gm=objects[Gamemode];gm.count=count;gm.num=int(values.size());std::copy(values.begin(),values.end(),senders);}
void Begin(){T::Install(&session);T::Tick();}
ue_wrap::script_gate::Verdict Event(bool active=false,bool ambient=false){uint8_t params[16]{};params[0]=uint8_t(active);params[1]=uint8_t(ambient);void* actor=At(Actor);std::memcpy(params+8,&actor,8);
    ue_wrap::script_gate::Call call{};call.function=At(SetEvent);call.object=At(Gamemode);call.locals=params;return T::BeforeSetEvent(call);}
M::Owner Owner(){M::Owner owner;CHECK(M::Identify(At(Actor),owner));return owner;}
bool Prepare(){return T::Prepare(At(Actor),[]{return true;});}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p&&test::O(p).live;}void* ClassOf(void* p){return p?test::O(p).cls:nullptr;}
int32_t InternalIndexOf(void* p){return p?test::O(p).id:-1;}int32_t SlotSerial(int32_t i){return i>=0&&i<test::Count?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}void* ObjectAt(int32_t i){return i>=0&&i<test::Count?test::At(i):nullptr;}
bool IsLiveByIndex(void* p,int32_t i){return p&&p==ObjectAt(i)&&IsLive(p);}void* SuperStructOf(void* p){return test::O(p).parent;}
const FName& NameOf(void* p){return test::O(p).name;}std::wstring ToString(const FName&){return L"llama_events_fixture";}
bool NameEquals(const FName& n,const wchar_t* name){return (n.ComparisonIndex==test::GmClass&&!wcscmp(name,L"mainGamemode_C"))||
    (n.ComparisonIndex==test::SetEvent&&!wcscmp(name,L"setEvent"))||(n.ComparisonIndex==test::Class&&!wcscmp(name,L"prop_llama_C"))||
    (n.ComparisonIndex==test::LightClass&&!wcscmp(name,L"PointLightComponent"))||(n.ComparisonIndex==test::AudioClass&&!wcscmp(name,L"AudioComponent"));}
bool PropertyOffsetAndClass(void*,const wchar_t* name,int32_t& offset,std::wstring& type){test::Fire();if(test::fieldMissing)return false;
    if(!wcscmp(name,L"activeEvents")){offset=offsetof(test::Obj,count);type=test::wrongCount?L"FloatProperty":L"IntProperty";return true;}
    if(!wcscmp(name,L"activeEvents_senders")){offset=offsetof(test::Obj,data);type=test::wrongArray?L"ObjectProperty":L"ArrayProperty";return true;}return false;}
int32_t StructSize(void*){return sizeof(test::Obj);}
bool FindBoolProperty(void*,const wchar_t* name,int32_t& offset,uint8_t& bits){CHECK(!wcscmp(name,L"h"));offset=offsetof(test::Obj,flags);bits=test::mask;return !test::fieldMissing;}
int32_t FindPropertyOffset(void*,const wchar_t* name){return !wcscmp(name,L"PointLight")?offsetof(test::Obj,light):!wcscmp(name,L"Audio")?offsetof(test::Obj,audio):-1;}
void* FindDispatchFunctionCached(void*,const wchar_t* name){return !wcscmp(name,L"SetVisibility")?test::At(test::Visibility):!wcscmp(name,L"Activate")?test::At(test::Activate):nullptr;}
int32_t FunctionFrameSize(void* function){return function==test::At(test::Visibility)?2:function==test::At(test::Activate)?1:test::frameSize;}
int32_t FindParamOffset(void*,const wchar_t*){return 0;}
std::vector<ParamInfo> FunctionParams(void* function){std::vector<ParamInfo> params=function==test::At(test::Visibility)?std::vector<ParamInfo>{{L"bNewVisibility",0,1,0},{L"bPropagateToChildren",1,1,0}}:
    function==test::At(test::Activate)?std::vector<ParamInfo>{{L"bReset",0,1,0}}:std::vector<ParamInfo>{{L"isEventActive",0,1,0},{L"deactivateAmbientTrack",1,1,0},{L"__WorldContext",8,8,0}};
    for(auto it=params.begin();it!=params.end();)if(it->name==test::badParam){if(test::badSize<0)it=params.erase(it);else{it->size=test::badSize;if(test::badOffset>=0)it->offset=test::badOffset;++it;}}else ++it;return params;}
bool CallFunction(void* owner,void* function,void* raw){auto* bytes=static_cast<uint8_t*>(raw);
    if(function==test::At(test::Visibility)){CHECK(owner==test::At(test::Light)&&!bytes[1]);test::objects[test::Light].visible=bytes[0]!=0;}
    else {CHECK(function==test::At(test::Activate)&&owner==test::At(test::Audio)&&!bytes[0]);++test::cues;}return true;}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::generation;}void* CurrentWorld(){return reinterpret_cast<void*>(test::world);}
void* WorldOf(void* p){if(!p||test::O(p).id>=test::Class)return nullptr;return reinterpret_cast<void*>(test::O(p).foreignWorld?test::world+1:test::world);}
}
namespace ue_wrap::world_singleton {void* Gamemode(){return test::gmFound?test::At(test::Gamemode):nullptr;}}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* name){return !wcscmp(name,L"prop_llama_C")?test::At(test::Class):nullptr;}
size_t ForEachInstance(void*,InstanceFn fn,void* context){for(void* actor:test::instances)fn(context,actor,test::O(actor).id);return test::instances.size();}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace coop::hand_item {bool IsHandAxisActor(void*){return test::hand;}}
namespace coop::net::session_serial {uint32_t Current(){return test::serial;}}
namespace ue_wrap::script_gate {
void Acquire(const char*){++test::holds;++test::acquires;}void Release(const char*){--test::holds;++test::releases;CHECK(test::holds>=0);}
bool WatchClassName(const wchar_t*,const wchar_t*,int,PreFn,PostFn){return test::watch;}void ResolvePendingNames(){++test::readiness;}
bool ClassNameWatchLive(const wchar_t*,const wchar_t*,int){return test::watch;}
}
namespace test {
void Group(int id){Reset();bool guarded=false;switch(id){
    case 26:{bool dispatched=false;const auto flags=objects[Actor].flags;CHECK(ue_wrap::llama::Apply(At(Actor),true,false,dispatched,[]{return true;})&&objects[Actor].flags==flags);
        CHECK(ue_wrap::llama::Apply(At(Actor),false,false,dispatched,[]{return true;})&&objects[Actor].flags==flags);}break;
    case 27:{bool dispatched=false;CHECK(ue_wrap::llama::Apply(At(Actor),true,true,dispatched,[]{return true;})&&dispatched&&objects[Light].visible&&cues==1);}
        break;
    case 28:Ledger({At(Other),At(Actor)},5);Begin();CHECK(objects[Gamemode].count==4&&objects[Gamemode].num==1&&senders[0]==At(Other)&&!(objects[Actor].flags&1));
        Reset();Ledger({At(Actor)},1);objects[Actor].flags=0xA4;Begin();CHECK(objects[Gamemode].count==0&&objects[Gamemode].num==0);break;
    case 29:T::Install(&session);fieldMissing=true;CHECK(!Prepare()&&objects[Actor].flags==0xA5);fieldMissing=false;CHECK(Prepare()&&!(objects[Actor].flags&1));
        Reset();Ledger({At(Actor)},3);T::Install(&session);watch=false;CHECK(!Prepare()&&objects[Gamemode].count==3);watch=true;
        CHECK(T::Prepare(At(Actor),[]{CHECK(!Prepare());return true;})&&objects[Gamemode].count==2);
        Reset();Ledger({At(Actor)},3);T::Install(&session);CHECK(!T::Prepare(At(Actor),[]{T::OnDisconnect();return true;})&&objects[Gamemode].count==3&&holds==0);break;
    case 30:Ledger({At(Other)},7);Begin();CHECK(objects[Gamemode].count==7&&senders[0]==At(Other));break;
    case 31:for(int cause=0;cause<4;++cause){Reset();Ledger({At(Actor)},1);if(cause==0)objects[Gamemode].cls=At(ForeignClass);if(cause==1)objects[Gamemode].foreignWorld=true;
        if(cause==2)gmFound=false;if(cause==3)objects[Actor].foreignWorld=true;Begin();CHECK(objects[Gamemode].count==1&&(objects[Actor].flags&1));}break;
    case 32:for(bool wrong:{false,true}){Reset();Ledger({At(Actor)},wrong?1:-1);wrongCount=wrong;Begin();CHECK(objects[Gamemode].num==1&&(objects[Actor].flags&1));}break;
    case 33:for(int cause=0;cause<5;++cause){Reset();Ledger({At(Actor)},1);if(cause==0)wrongArray=true;if(cause==1)objects[Gamemode].num=-1;
        if(cause==2)objects[Gamemode].num=65537;if(cause==3)objects[Gamemode].max=0;if(cause==4)objects[Gamemode].data=nullptr;Begin();CHECK(objects[Gamemode].count==1&&(objects[Actor].flags&1));}break;
    case 34:Ledger({At(Actor),At(Other),At(Actor)},9);Begin();CHECK(objects[Gamemode].count==7&&objects[Gamemode].num==1&&senders[0]==At(Other));break;
    case 35:Ledger({At(Other),At(Actor),At(Second),At(Other)},8);Begin();CHECK(objects[Gamemode].count==7&&objects[Gamemode].num==3&&senders[0]==At(Other)&&senders[1]==At(Second)&&senders[2]==At(Other));break;
    case 36:Ledger({At(Other),At(Actor)},120);Begin();CHECK(objects[Gamemode].count==119);Reset();Ledger({At(Actor),At(Actor)},1);Begin();CHECK(objects[Gamemode].count==1&&objects[Gamemode].num==2);break;
    case 37:Ledger({At(Actor)},1);Begin();CHECK(objects[Actor].flags==0xA4);break;
    case 38:for(int nth=1;nth<=4;++nth){Reset();Ledger({At(Actor)},3);auto owner=Owner();int calls=0,changed=0;const auto result=M::Cleanup(owner,guarded,[&]{if(++calls==nth){++objects[Gamemode].count;++changed;}return true;});
        CHECK(result!=M::Result::Done||(objects[Gamemode].count==3+changed-1&&objects[Gamemode].num==0));}
        Reset();Ledger({At(Actor)},3);{auto owner=Owner();int calls=0;CHECK(M::Cleanup(owner,guarded,[&]{if(++calls==3)++objects[Actor].serial;return true;})!=M::Result::Done);CHECK(objects[Gamemode].count==3);}break;
    case 39:{M::Owner owner;owner.world=reinterpret_cast<void*>(999);objects[Actor].cls=At(ForeignClass);CHECK(!M::Identify(At(Actor),owner)&&owner.world==reinterpret_cast<void*>(999));}
        {M::Event event;event.active=true;uint8_t params[16]{};void* actor=At(Actor);std::memcpy(params+8,&actor,8);badParam=L"__WorldContext";CHECK(!M::ReadEvent(At(SetEvent),params,event)&&event.active);}break;
    case 40:Ledger({At(Actor)},4);fieldMissing=true;Begin();CHECK(objects[Gamemode].count==4&&T::g_rows.size()==1);fieldMissing=false;now+=100;T::Tick();CHECK(objects[Gamemode].count==3&&!(objects[Actor].flags&1));break;
    case 41:Ledger({At(Actor)},1);session.mode=coop::net::Role::Host;Begin();CHECK(objects[Gamemode].count==1&&objects[Actor].flags==0xA5&&T::g_rows.empty());break;
    case 42:Ledger({At(Other)},8);Begin();CHECK(objects[Gamemode].count==8&&Event()==ue_wrap::script_gate::Verdict::Cancel);break;
    case 43:Ledger({At(Actor)},7);Begin();CHECK(objects[Gamemode].count==6);now+=100;T::Tick();CHECK(objects[Gamemode].count==6);CHECK(Prepare()&&objects[Gamemode].count==6);break;
    case 44:for(int cause=0;cause<3;++cause){Reset();Ledger({At(Actor)},7);Begin();if(cause==0)++objects[Actor].serial;if(cause==1)++objects[Class].serial;if(cause==2)++world;
        CHECK(Event()==ue_wrap::script_gate::Verdict::Run&&T::g_rows.empty());}break;
    case 45:instances.clear();for(int actor=1;actor<=300;++actor)instances.push_back(At(actor));Begin();CHECK(T::g_rows.size()==256);break;
    case 46:Ledger({At(Actor)},1);Begin();T::OnDisconnect();CHECK(Event()==ue_wrap::script_gate::Verdict::Cancel);
        for(const wchar_t* name:{L"isEventActive",L"deactivateAmbientTrack",L"__WorldContext"})for(int bytes:{-1,3}){
            Reset();Ledger({At(Actor)},1);Begin();T::OnDisconnect();badParam=name;badSize=bytes;CHECK(Event()==ue_wrap::script_gate::Verdict::Run);}
        Reset();Ledger({At(Actor)},1);Begin();T::OnDisconnect();badParam=L"__WorldContext";badSize=8;badOffset=0;CHECK(Event()==ue_wrap::script_gate::Verdict::Run);
        Reset();Ledger({At(Actor)},1);Begin();T::OnDisconnect();frameSize=70000;CHECK(Event()==ue_wrap::script_gate::Verdict::Run);break;
    case 47:Ledger({At(Actor)},1);Begin();T::OnDisconnect();CHECK(Event(true)==ue_wrap::script_gate::Verdict::Run);break;
    case 48:Ledger({At(Actor)},1);Begin();T::OnDisconnect();Ledger({At(Actor)},1);CHECK(Event()==ue_wrap::script_gate::Verdict::Run);
        CHECK(Event(false,true)==ue_wrap::script_gate::Verdict::Run);break;
    case 49:Ledger({At(Actor)},1);Begin();CHECK(holds==1);T::OnDisconnect();CHECK(holds==1&&T::g_hold);break;
    case 50:Ledger({At(Actor)},1);Begin();T::OnDisconnect();++world;T::Tick();CHECK(T::g_rows.empty()&&holds==0&&!T::g_hold);break;
    default:CHECK(false);}
}
}
int main(){for(int id=26;id<=50;++id){test::Group(id);std::printf("PASS llama lifecycle %d\n",id);}return 0;}
