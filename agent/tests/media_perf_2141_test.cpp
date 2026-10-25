#include "../../src/votv-coop/src/ue_wrap/world/media_player.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <set>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace M=ue_wrap::media_player;
namespace test {
struct Obj {int id=0,serial=1;bool live=true,playing=false;std::wstring url;int64_t time=0;} objects[24];
uint32_t world=1;bool loaded=true,inEnumeration=false,acceptOpen=true,dispatch=true,malformed=false,radio=false;
int walks=0,visits=0,opens=0,seeks=0,plays=0,pauses=0,allocs=0,frees=0;
std::set<void*> allocations;std::function<void()> callback;
ue_wrap::reflection::FString Allocate(const std::wstring& text){auto* mem=new wchar_t[text.size()+1];std::copy(text.begin(),text.end(),mem);mem[text.size()]=0;
 allocations.insert(mem);++allocs;return {mem,int32_t(text.size()+1),int32_t(text.size()+1)};}
void Reset(){CHECK(allocations.empty());++world;for(int i=0;i<24;++i){objects[i]={};objects[i].id=i;objects[i].serial=int(world);}
 objects[1].url=L"http://video";objects[1].playing=true;objects[1].time=30000000;
 loaded=acceptOpen=dispatch=true;inEnumeration=malformed=radio=false;walks=visits=opens=seeks=plays=pauses=allocs=frees=0;callback={};}
int Fn(const wchar_t* n){const wchar_t* names[]={L"GetUrl",L"IsPlaying",L"GetTime",L"OpenFile",L"OpenUrl",L"Seek",L"Pause",L"Play"};
 for(int i=0;i<8;++i)if(std::wstring(n)==names[i])return 10+i;return -1;}
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* a){return a?static_cast<test::Obj*>(a)->id:-1;}
int32_t SlotSerial(int32_t i){return i>=0&&i<24?test::objects[i].serial:0;}
int32_t AllocateSlotSerial(int32_t i){return SlotSerial(i);}
void* ObjectAt(int32_t i){return i>=0&&i<24?&test::objects[i]:nullptr;}
bool IsLiveByIndex(void* a,int32_t i){return i>=0&&i<24&&a==&test::objects[i]&&test::objects[i].live;}
bool IsLive(void* a){return a&&static_cast<test::Obj*>(a)->live;}
void* ClassOf(void*){return &test::objects[0];}void* OuterOf(void*){return nullptr;}
const FName& NameOf(void* a){static FName n{};n.ComparisonIndex=InternalIndexOf(a);return n;}
bool NameStartsWith(const FName& n,const wchar_t*){return n.ComparisonIndex==3;}
void* FindClass(const wchar_t*){return nullptr;}
int32_t FindPropertyOffset(void*,const wchar_t*){return -1;}
void* FindDispatchFunctionCached(void*,const wchar_t* n){int i=test::Fn(n);return i<0?nullptr:&test::objects[i];}
int32_t FunctionFrameSize(void*){return 64;}
std::vector<ParamInfo> FunctionParams(void* f){int i=InternalIndexOf(f);std::vector<ParamInfo> out;
 if(i==13||i==14)out.push_back({i==13?L"FilePath":L"Url",0,16,0});
 if(i==15)out.push_back({L"Time",0,8,0});
 out.push_back({L"ReturnValue",32,i==10?16:i==12?8:1,0});return out;}
void EngineFree(void* p){CHECK(test::allocations.erase(p)==1);delete[] static_cast<wchar_t*>(p);++test::frees;}
std::vector<void*> FindObjectsByClass(const wchar_t*){++test::walks;return {};}
bool CallFunction(void* a,void* f,void* raw){using namespace test;CHECK(!inEnumeration&&IsLive(a));auto& actor=*static_cast<Obj*>(a);int i=InternalIndexOf(f);auto* p=static_cast<uint8_t*>(raw);
 if(i==10){auto s=Allocate(actor.url);if(malformed)s.Max=0;std::memcpy(p+32,&s,sizeof(s));}
 else if(i==11)std::memcpy(p+32,&actor.playing,1);
 else if(i==12)std::memcpy(p+32,&actor.time,8);
 else if(i==13||i==14){++opens;FString s{};std::memcpy(&s,p,sizeof(s));CHECK(allocations.count(s.Data));if(acceptOpen)actor.url=std::wstring(s.Data,s.Num-1);std::memcpy(p+32,&acceptOpen,1);}
 else if(i==15){++seeks;std::memcpy(&actor.time,p,8);p[32]=1;}
 else if(i==16){++pauses;actor.playing=false;p[32]=1;}
 else if(i==17){++plays;actor.playing=true;p[32]=1;}
 auto cb=std::move(callback);callback={};if(cb)cb();return dispatch;}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* CurrentWorld(){return (void*)(uintptr_t)test::world;}void* WorldOf(void* a){return ue_wrap::reflection::InternalIndexOf(a)<4?CurrentWorld():nullptr;}}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* n){return test::loaded&&std::wstring(n)==L"MediaPlayer"?&test::objects[0]:nullptr;}
size_t ForEachInstance(void* cls,InstanceFn fn,void* ctx){CHECK(cls==&test::objects[0]);test::inEnumeration=true;
 for(int i=1;i<=3;++i)if(test::objects[i].live){++test::visits;fn(ctx,&test::objects[i],i);}test::inEnumeration=false;return 3;}
}
namespace ue_wrap::atv {bool IsAtv(void*){return false;}}
namespace ue_wrap::world_instances {bool IsActorClass(void*){return false;}}
namespace ue_wrap::engine {void* ParentActorOf(void*,std::wstring*){return nullptr;}}
namespace ue_wrap::radio_player {bool OwnsPlayer(void* a){return test::radio&&a==&test::objects[1];}}
namespace ue_wrap::fstring_utils {bool MintFString(const std::wstring& s,void* out){auto v=test::Allocate(s);std::memcpy(out,&v,sizeof(v));return true;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
using namespace test;
int main(){
 Reset();M::Playing out[4];for(int i=0;i<1000;++i){CHECK(M::CopyPlaying(out,4)==1&&out[0].url==objects[1].url&&out[0].ticks==30000000);}
 CHECK(!walks&&visits==3000&&allocations.empty()&&allocs==frees);
 std::printf("1000 media polls: full-object walks=%d; GetUrl allocations=%d freed=%d\n",walks,allocs,frees);
 CHECK(M::Apply(L"http://video",60000000,true)&&seeks==1&&!opens);CHECK(allocations.empty());
 CHECK(M::Apply(L"http://video",0,false)&&pauses==1);
 CHECK(M::Apply(L"http://new",0,true)&&opens==1&&objects[2].url==L"http://new"&&objects[1].url==L"http://video");CHECK(allocations.empty());
 Reset();radio=true;CHECK(M::CopyPlaying(out,4)==0);CHECK(M::Apply(L"http://other",0,true)&&objects[1].url==L"http://video"&&objects[2].url==L"http://other");
 Reset();acceptOpen=false;CHECK(!M::Drive(&objects[2],L"http://fail",true)&&!plays&&allocations.empty());
 CHECK(!M::Drive(&objects[2],L"C:/music.mp3",true)&&allocations.empty()); // OpenFile return + input lifetime
 Reset();CHECK(M::Drive(&objects[2],L"C:/music.mp3",true)&&plays==1&&allocations.empty());
 Reset();malformed=true;CHECK(M::Url(&objects[1]).empty()&&allocations.empty());
 Reset();dispatch=false;CHECK(M::Url(&objects[1]).empty()&&allocations.empty());
 Reset();callback=[]{objects[1].live=false;};CHECK(M::CopyPlaying(out,4)==0&&allocations.empty());
 Reset();callback=[]{++objects[1].serial;};CHECK(M::CopyPlaying(out,4)==0&&allocations.empty());
 Reset();callback=[]{++world;};CHECK(M::CopyPlaying(out,4)==0&&allocations.empty());
 Reset();callback=[]{++objects[1].serial;};CHECK(!M::Drive(&objects[1],L"http://new",true)&&!opens&&allocations.empty());
 Reset();objects[1].live=false;callback=[]{++objects[2].serial;};CHECK(!M::Apply(L"http://new",0,true)&&!opens&&allocations.empty());
 Reset();loaded=false;CHECK(M::CopyPlaying(out,4)==0);loaded=true;CHECK(M::CopyPlaying(out,4)==1);
 puts("PASS actual media wrapper/ParamFrame: indexed enumeration, no engine calls during iteration, balanced FString ownership, CDO/radio exclusion, seek/pause/open results, native lifetime changes and late class loading");
}

// Reflection names used by ParamFrame missing-field diagnostics.
namespace ue_wrap::reflection {
std::wstring ToString(const FName&){return L"fixture_function";}
}
