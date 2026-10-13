// Exercise the REAL wrapper and parameter marshalling, not radio_sync's player double.
#include "../../src/votv-coop/src/ue_wrap/world/radio_player.cpp"
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1); } } while(0)
namespace test {
using ue_wrap::reflection::FString;
struct Obj { void* cls=nullptr; int id=1; } radioClass,atvClass,playerClass,gmClass,dataClass;
struct Track { FString link{}; uint8_t extra[40]{}; bool url=false; }; // 57 property bytes, 64-byte value
static_assert(sizeof(Track)==64);
struct Player:Obj { std::wstring url;bool preparing=false,playing=false;int64_t ticks=0; } player;
struct Gm:Obj { ue_wrap::field_io::TArrayView catalog{}; } gm;
struct Radio:Obj { Track data{};void* media=&player;void* game=&gm;uint8_t mode=0,pause=0; } radio,atv;
struct Fn:Obj { const wchar_t* name; Fn(const wchar_t* n,int i):name(n){id=i;} };
Fn fns[]={{L"openLink",20},{L"IsPreparing",21},{L"IsPlaying",22},{L"GetTime",23},{L"GetUrl",24},{L"Seek",25},{L"Play",26},{L"Pause",27}};
int opens=0,seeks=0,freed=0;bool seekOk=true;
Track Make(std::wstring& s,bool url=false) { Track t;t.link={s.data(),int(s.size()+1),int(s.size()+1)};t.url=url;return t; }
}
namespace ue_wrap::reflection {
void* ClassOf(void* a) { return a ? static_cast<test::Obj*>(a)->cls : nullptr; }
bool IsLive(void* a) { return a!=nullptr; }
int32_t InternalIndexOf(void* a) { return a ? static_cast<test::Obj*>(a)->id : -1; }
bool IsLiveByIndex(void* a,int32_t i) { return a && InternalIndexOf(a)==i; }
int32_t SlotSerial(int32_t) { return 1; }
int32_t AllocateSlotSerial(int32_t) { return 1; }
void* ObjectAt(int32_t i) { for (auto& f:test::fns) if (f.id==i) return &f;return nullptr; }
const FName& NameOf(void*) { static FName n{};return n; }
std::wstring ToString(const FName&) { return L"test"; }
bool IsDescendantOfAny(void* c,void* const* bases,size_t n,int) {
    for (size_t i=0;i<n;++i) if (c==bases[i] || (c==&test::atvClass && bases[i]==&test::radioClass)) return true;
    return false;
}
void* PropertyInnerStruct(void*,const wchar_t*) { return &test::dataClass; }
int32_t StructSize(void*) { return 57; }
int32_t FindPropertyOffsetByPrefix(void*,const wchar_t* p) { return std::wcscmp(p,L"link_")==0 ? 0 : 56; }
int32_t InstanceOffset::Of(void*) {
    if (!std::wcscmp(prop_,L"data")) return offsetof(test::Radio,data);
    if (!std::wcscmp(prop_,L"mediaPlayer")) return offsetof(test::Radio,media);
    if (!std::wcscmp(prop_,L"gamemode")) return offsetof(test::Radio,game);
    if (!std::wcscmp(prop_,L"playmode")) return offsetof(test::Radio,mode);
    if (!std::wcscmp(prop_,L"pausePlayer")) return offsetof(test::Radio,pause);
    if (!std::wcscmp(prop_,L"radioLinks")) return offsetof(test::Gm,catalog);
    return -1;
}
void* FindDispatchFunctionCached(void*,const wchar_t* name) {
    for (auto& f:test::fns) if (!std::wcscmp(f.name,name)) return &f;
    return nullptr;
}
int32_t FindParamSize(void*,const wchar_t*) { return sizeof(test::Track); }
int32_t FunctionFrameSize(void*) { return 80; }
std::vector<ParamInfo> FunctionParams(void* fn) {
    auto n=static_cast<test::Fn*>(fn)->name;
    if (!std::wcscmp(n,L"openLink")) return {{L"data",0,64,0},{L"pausePlayer",64,1,0}};
    if (!std::wcscmp(n,L"Seek")) return {{L"Time",0,8,0},{L"ReturnValue",8,1,0}};
    return {{L"ReturnValue",0,16,0}};
}
void EngineFree(void* p) { ++test::freed;std::free(p); }
bool CallFunction(void* obj,void* fn,void* params) {
    auto n=static_cast<test::Fn*>(fn)->name;auto* b=static_cast<uint8_t*>(params);
    if (!std::wcscmp(n,L"openLink")) {
        auto& r=*static_cast<test::Radio*>(obj);std::memcpy(&r.data,b,64);r.pause=b[64];
        auto& p=*static_cast<test::Player*>(r.media);
        p.url=L"file://"+std::wstring(r.data.link.Data,r.data.link.Num-1);
        p.preparing=true;++test::opens;return true;
    }
    auto& p=*static_cast<test::Player*>(obj);
    if (!std::wcscmp(n,L"IsPreparing")) *b=p.preparing;
    else if (!std::wcscmp(n,L"IsPlaying")) *b=p.playing;
    else if (!std::wcscmp(n,L"GetTime")) std::memcpy(b,&p.ticks,8);
    else if (!std::wcscmp(n,L"GetUrl")) {
        FString s{static_cast<wchar_t*>(std::malloc((p.url.size()+1)*sizeof(wchar_t))),int(p.url.size()+1),int(p.url.size()+1)};
        std::memcpy(s.Data,p.url.c_str(),s.Num*sizeof(wchar_t));std::memcpy(b,&s,sizeof(s));
    } else if (!std::wcscmp(n,L"Seek")) { if (test::seekOk) std::memcpy(&p.ticks,b,8);b[8]=test::seekOk;++test::seeks; }
    else if (!std::wcscmp(n,L"Play")) { p.playing=true;*b=true; }
    else if (!std::wcscmp(n,L"Pause")) { p.playing=false;*b=true; }
    else return false;
    return true;
}
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* n) { return !std::wcscmp(n,L"prop_radio_internet_C") ? &test::radioClass : &test::atvClass; }
size_t ForEachInstance(void*,InstanceFn fn,void* ctx) { fn(ctx,&test::radio,1);fn(ctx,&test::atv,1);return 2; }
}
namespace ue_wrap::world_identity { void* CurrentWorld(){return &test::gm;} void* WorldOf(void*){return CurrentWorld();} }
namespace ue_wrap::field_io { std::wstring ReadFStringAt(const void* p,int32_t off) { auto* s=reinterpret_cast<const FStringView*>(static_cast<const uint8_t*>(p)+off);return std::wstring(s->data,s->num-1); } }
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
int main() {
    using namespace test;using namespace ue_wrap::radio_player;
    radio.cls=&radioClass;atv.cls=&atvClass;player.cls=&playerClass;gm.cls=&gmClass;
    std::wstring a=L"C:/Alice/radio/first.ogg",b=L"C:/Alice/radio/album/second.ogg",url=L"https://invalid.example/stream",other=L"D:/other/radio/album/second.ogg";
    Track tracks[]={Make(a),Make(url,true),Make(b)};
    gm.catalog={reinterpret_cast<uint8_t*>(tracks),3,3};
    CHECK(IsRadio(&radio) && !IsRadio(&atv));CHECK(OwnsPlayer(&player));
    State wanted{TrackId(L"D:/Bob/radio/album/second.ogg"),35000000,1,2},out{};
    CHECK(!Apply(&radio,wanted) && opens==1); // finds third row using aligned stride
    CHECK(radio.data.link.Data==b.data() && radio.pause==0 && radio.mode==2);
    CHECK(!Read(&radio,out));CHECK(!Apply(&radio,wanted) && opens==1); // no reopen loop during loading
    player.preparing=false;CHECK(Apply(&radio,wanted) && seeks==1 && player.playing);
    CHECK(Read(&radio,out) && out.track==wanted.track && out.ticks==wanted.ticks);
    wanted.playing=0;CHECK(Apply(&radio,wanted) && !player.playing && radio.pause==1);
    wanted.track=TrackId(a);CHECK(!Apply(&radio,wanted) && opens==2);
    player.preparing=false;CHECK(Apply(&radio,wanted));
    auto missing=wanted;missing.track=999;CHECK(!Apply(&radio,missing) && opens==2);
    tracks[0]=Make(other);CHECK(!Apply(&radio,State{TrackId(b),0,1,0}) && opens==2); // ambiguous catalog
    Track malformed{};malformed.link={reinterpret_cast<wchar_t*>(~uintptr_t(0)),INT32_MAX,INT32_MAX};
    CHECK(Id(reinterpret_cast<uint8_t*>(&malformed))==0); // refuse corrupt/unaligned FString before copying
    CHECK(freed>0);
    std::puts("PASS: real radio wrapper + ParamFrame, padded catalog, third track, loading, play/pause/seek, ATV isolation, malformed/ambiguous rows");
}
