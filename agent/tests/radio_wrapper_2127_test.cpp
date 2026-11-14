// Exercise the REAL wrapper and parameter marshalling, not radio_sync's player double.
#ifndef RADIO_PLAYER_SOURCE
#define RADIO_PLAYER_SOURCE "../../src/votv-coop/src/ue_wrap/world/radio_player.cpp"
#endif
#include RADIO_PLAYER_SOURCE
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <functional>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1); } } while(0)
namespace test {
using ue_wrap::reflection::FString;
struct Obj { void* cls=nullptr; int id=1,serial=1;bool live=true; } radioClass,atvClass,playerClass,gmClass,dataClass;
struct Track { FString link{}; uint8_t extra[40]{}; bool url=false; }; // 57 property bytes, 64-byte value
static_assert(sizeof(Track)==64);
struct Player:Obj { std::wstring url;bool preparing=false,playing=false;int64_t ticks=0; } player;
struct Gm:Obj { ue_wrap::field_io::TArrayView catalog{}; } gm;
struct Radio:Obj { Track data{};void* media=&player;void* game=&gm;uint8_t mode=0,pause=0;std::wstring selected; } radio,atv;
struct Fn:Obj { const wchar_t* name; Fn(const wchar_t* n,int i):name(n){id=i;} };
Fn fns[]={{L"openLink",20},{L"IsPreparing",21},{L"IsPlaying",22},{L"GetTime",23},{L"GetUrl",24},{L"Seek",25},{L"Play",26},{L"Pause",27}};
int opens=0,seeks=0,freed=0;bool seekOk=true;
int plays=0,pauses=0,calls=0,urlFault=0,missing=0,prefixLink=0,prefixUrl=56,stride=64,fieldReads=0;
bool authority=true,atvReady=true;
bool oldActorWorld=false;
uint32_t world=1;
std::function<void(const wchar_t*)> onLookup,onField,onFind,onFrame,onParams,onNative,onPrefix;
std::function<void()> onType,onSize;
void Fire(std::function<void(const wchar_t*)>& f,const wchar_t* name){auto cb=f;if(cb)cb(name);}
void Fire(std::function<void()>& f){auto cb=std::move(f);f={};if(cb)cb();}
Track Make(std::wstring& s,bool url=false) { Track t;t.link={s.data(),int(s.size()+1),int(s.size()+1)};t.url=url;return t; }
}
namespace ue_wrap::reflection {
void* ClassOf(void* a) { return a ? static_cast<test::Obj*>(a)->cls : nullptr; }
bool IsLive(void* a) { return a && static_cast<test::Obj*>(a)->live; }
int32_t InternalIndexOf(void* a) { return a ? static_cast<test::Obj*>(a)->id : -1; }
bool IsLiveByIndex(void* a,int32_t i) { return IsLive(a) && InternalIndexOf(a)==i; }
void* ObjectAt(int32_t i) {
    for(auto* o:{&test::radioClass,&test::atvClass,&test::playerClass,&test::gmClass,&test::dataClass,
        static_cast<test::Obj*>(&test::radio),static_cast<test::Obj*>(&test::atv),static_cast<test::Obj*>(&test::player),static_cast<test::Obj*>(&test::gm)})if(o->id==i)return o;
    for (auto& f:test::fns) if (f.id==i) return &f;return nullptr;
}
int32_t SlotSerial(int32_t i) {auto* o=static_cast<test::Obj*>(ObjectAt(i));return o?o->serial:0;}
int32_t AllocateSlotSerial(int32_t i) {return SlotSerial(i);}
const FName& NameOf(void*) { static FName n{};return n; }
std::wstring ToString(const FName&) { return L"test"; }
bool IsDescendantOfAny(void* c,void* const* bases,size_t n,int) {
    for (size_t i=0;i<n;++i) if (c==bases[i] || (c==&test::atvClass && bases[i]==&test::radioClass)) return true;
    return false;
}
void* PropertyInnerStruct(void*,const wchar_t*) {test::Fire(test::onType);return &test::dataClass; }
int32_t StructSize(void*) { return 57; }
int32_t FindPropertyOffsetByPrefix(void*,const wchar_t* p) {test::Fire(test::onPrefix,p);return std::wcscmp(p,L"link_")==0 ? test::prefixLink : test::prefixUrl; }
int32_t InstanceOffset::Of(void*) {
    ++test::fieldReads;
    test::Fire(test::onField,prop_);
    if (!std::wcscmp(prop_,L"data")) return offsetof(test::Radio,data);
    if (!std::wcscmp(prop_,L"mediaPlayer")) return offsetof(test::Radio,media);
    if (!std::wcscmp(prop_,L"gamemode")) return offsetof(test::Radio,game);
    if (!std::wcscmp(prop_,L"playmode")) return offsetof(test::Radio,mode);
    if (!std::wcscmp(prop_,L"pausePlayer")) return offsetof(test::Radio,pause);
    if (!std::wcscmp(prop_,L"radioLinks")) return offsetof(test::Gm,catalog);
    return -1;
}
void* FindDispatchFunctionCached(void*,const wchar_t* name) {
    test::Fire(test::onFind,name);
    for (auto& f:test::fns) if (!std::wcscmp(f.name,name)) return &f;
    return nullptr;
}
int32_t FindParamSize(void*,const wchar_t*) {test::Fire(test::onSize);return test::stride; }
int32_t FunctionFrameSize(void* fn) {test::Fire(test::onFrame,static_cast<test::Fn*>(fn)->name);return 80; }
std::vector<ParamInfo> FunctionParams(void* fn) {
    auto n=static_cast<test::Fn*>(fn)->name;
    test::Fire(test::onParams,n);
    if (!std::wcscmp(n,L"openLink")) {
        if(test::missing==1)return {{L"pausePlayer",64,1,0}};
        if(test::missing==2)return {{L"data",0,64,0}};
        return {{L"data",0,64,0},{L"pausePlayer",64,1,0}};
    }
    if (!std::wcscmp(n,L"Seek")) return {{L"Time",0,8,0},{L"ReturnValue",8,1,0}};
    return {{L"ReturnValue",0,16,0}};
}
void EngineFree(void* p) { ++test::freed;std::free(p); }
bool CallFunction(void* obj,void* fn,void* params) {
    ++test::calls;
    auto n=static_cast<test::Fn*>(fn)->name;auto* b=static_cast<uint8_t*>(params);
    if (!std::wcscmp(n,L"openLink")) {
        auto& r=*static_cast<test::Radio*>(obj);std::memcpy(&r.data,b,64);r.pause=b[64];
        r.selected.assign(r.data.link.Data,r.data.link.Num-1);r.data.link.Data=r.selected.data();
        auto& p=*static_cast<test::Player*>(r.media);
        p.url=L"file://"+std::wstring(r.data.link.Data,r.data.link.Num-1);
        p.preparing=true;++test::opens;test::Fire(test::onNative,n);return true;
    }
    auto& p=*static_cast<test::Player*>(obj);
    if (!std::wcscmp(n,L"IsPreparing")) *b=p.preparing;
    else if (!std::wcscmp(n,L"IsPlaying")) *b=p.playing;
    else if (!std::wcscmp(n,L"GetTime")) std::memcpy(b,&p.ticks,8);
    else if (!std::wcscmp(n,L"GetUrl")) {
        FString s{static_cast<wchar_t*>(std::malloc((p.url.size()+1)*sizeof(wchar_t))),int(p.url.size()+1),int(p.url.size()+1)};
        std::memcpy(s.Data,p.url.c_str(),s.Num*sizeof(wchar_t));std::memcpy(b,&s,sizeof(s));
        if(test::urlFault==1)s.Max=s.Num-1;
        if(test::urlFault==2)s.Data[s.Num-1]=L'X';
        if(test::urlFault==3&&s.Num>2)s.Data[1]=0;
        std::memcpy(b,&s,sizeof(s));
    } else if (!std::wcscmp(n,L"Seek")) { if (test::seekOk) std::memcpy(&p.ticks,b,8);b[8]=test::seekOk;++test::seeks; }
    else if (!std::wcscmp(n,L"Play")) { p.playing=true;*b=true;++test::plays; }
    else if (!std::wcscmp(n,L"Pause")) { p.playing=false;*b=true;++test::pauses; }
    else return false;
    test::Fire(test::onNative,n);return true;
}
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t* n) {test::Fire(test::onLookup,n);return !std::wcscmp(n,L"prop_radio_internet_C") ? &test::radioClass : test::atvReady?&test::atvClass:nullptr; }
size_t ForEachInstance(void*,InstanceFn fn,void* ctx) { fn(ctx,&test::radio,1);fn(ctx,&test::atv,1);return 2; }
}
namespace ue_wrap::world_identity {
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* p){int id=static_cast<test::Obj*>(p)->id;return id>=6&&id<20?
    (test::oldActorWorld?reinterpret_cast<void*>(uintptr_t(test::world+100)):CurrentWorld()):nullptr;}
}
namespace ue_wrap::field_io { std::wstring ReadFStringAt(const void* p,int32_t off) { auto* s=reinterpret_cast<const FStringView*>(static_cast<const uint8_t*>(p)+off);return std::wstring(s->data,s->num-1); } }
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
void AdversarialWrapper();
void ObjectIds(){
    using namespace test;int i=1;
    for(auto* o:{&radioClass,&atvClass,&playerClass,&gmClass,&dataClass,static_cast<Obj*>(&radio),static_cast<Obj*>(&atv),static_cast<Obj*>(&player),static_cast<Obj*>(&gm)})o->id=i++;
}
int main() {
    using namespace test;using namespace ue_wrap::radio_player;
    ObjectIds();radio.cls=&radioClass;atv.cls=&atvClass;player.cls=&playerClass;gm.cls=&gmClass;
    std::wstring a=L"C:/Alice/radio/first.ogg",b=L"C:/Alice/radio/album/second.ogg",url=L"https://invalid.example/stream",other=L"D:/other/radio/album/second.ogg";
    Track tracks[]={Make(a),Make(url,true),Make(b)};
    gm.catalog={reinterpret_cast<uint8_t*>(tracks),3,3};
    CHECK(IsRadio(&radio) && !IsRadio(&atv));CHECK(OwnsPlayer(&player));
    State wanted{TrackId(L"D:/Bob/radio/album/second.ogg"),35000000,1,2},out{};
    CHECK(!Apply(&radio,wanted) && opens==1); // finds third row using aligned stride
    CHECK(radio.selected==b && radio.pause==0 && radio.mode==2);
    CHECK(!Read(&radio,out));CHECK(!Apply(&radio,wanted) && opens==1); // no reopen loop during loading
    player.preparing=false;CHECK(Apply(&radio,wanted) && seeks==1 && player.playing);
    CHECK(Read(&radio,out) && out.track==wanted.track && out.ticks==wanted.ticks);
    wanted.playing=0;CHECK(Apply(&radio,wanted) && !player.playing && radio.pause==1);
    wanted.track=TrackId(a);CHECK(!Apply(&radio,wanted) && opens==2);
    player.preparing=false;CHECK(Apply(&radio,wanted));
    auto missing=wanted;missing.track=999;CHECK(!Apply(&radio,missing) && opens==2);
    tracks[0]=Make(other);CHECK(!Apply(&radio,State{TrackId(b),0,1,0}) && opens==2); // ambiguous catalog
    Track malformed{};malformed.link={reinterpret_cast<wchar_t*>(~uintptr_t(0)),INT32_MAX,INT32_MAX};
    LayoutData layout;CHECK(Layout(&radio,layout));
    CHECK(Id(reinterpret_cast<uint8_t*>(&malformed),layout)==0); // refuse corrupt/unaligned FString before copying
    CHECK(freed>0);
    std::puts("PASS: real radio wrapper + ParamFrame, padded catalog, third track, loading, play/pause/seek, ATV isolation, malformed/ambiguous rows");
    AdversarialWrapper();return 0;
}
namespace test {
std::wstring catalogPath=L"C:/radio/owned/track.ogg";
Track catalogTrack;
void ResetWrapper(){
    ++world;ObjectIds();
    for(auto* o:{&radioClass,&atvClass,&playerClass,&gmClass,&dataClass,static_cast<Obj*>(&radio),static_cast<Obj*>(&atv),static_cast<Obj*>(&player),static_cast<Obj*>(&gm)}){++o->serial;o->live=true;}
    for(auto& f:fns){++f.serial;f.live=true;}
    radio.cls=&radioClass;atv.cls=&atvClass;player.cls=&playerClass;gm.cls=&gmClass;
    radio.media=&player;radio.game=&gm;radio.mode=radio.pause=0;radio.data={};radio.selected.clear();
    player.preparing=player.playing=false;player.ticks=0;player.url.clear();
    catalogPath=L"C:/radio/owned/track.ogg";catalogTrack=Make(catalogPath);gm.catalog={reinterpret_cast<uint8_t*>(&catalogTrack),1,1};
    opens=seeks=freed=plays=pauses=calls=urlFault=missing=prefixLink=fieldReads=0;prefixUrl=56;stride=64;
    authority=atvReady=seekOk=true;oldActorWorld=false;
    onLookup=onField=onFind=onFrame=onParams=onNative=onPrefix={};onType=onSize={};
    ue_wrap::radio_player::g_class.Reset();ue_wrap::radio_player::g_atvClass.Reset();ue_wrap::radio_player::g_layout={};
}
ue_wrap::radio_player::State Wanted(){return {ue_wrap::radio_player::TrackId(catalogPath),35000000,1,2};}
void Loaded(){radio.data=Make(catalogPath);player.url=L"file://"+catalogPath;player.ticks=35000000;}
}
void AdversarialWrapper(){
    using namespace test;using namespace ue_wrap::radio_player;
    auto allowed=[] {return authority;};
    ResetWrapper();onLookup=[](const wchar_t*){onLookup={};++radio.serial;};CHECK(!IsRadio(&radio));
    ResetWrapper();onLookup=[](const wchar_t* n){if(!wcscmp(n,L"prop_radio_atv_C")){onLookup={};++radioClass.serial;}};CHECK(!IsRadio(&radio));
    ResetWrapper();atvReady=false;CHECK(!IsRadio(&radio)&&!IsRadio(&atv));atvReady=true;CHECK(IsRadio(&radio));
    ResetWrapper();oldActorWorld=true;CHECK(!IsRadio(&radio)&&IsRadioClass(&radioClass));
    ResetWrapper();onField=[](const wchar_t*){onField={};++radio.serial;};CHECK(Object(&radio,g_player)==nullptr);
    ResetWrapper();player.live=false;CHECK(Object(&radio,g_player)==nullptr);
    ResetWrapper();CHECK(Object(&radio,g_player,[] {++radio.serial;return true;})==nullptr&&fieldReads==0);
    for(int kind=0;kind<3;++kind){ResetWrapper();LayoutData layout;
        if(kind==0)onType=[] {++radioClass.serial;};
        if(kind==1)onFind=[](const wchar_t*){onFind={};++dataClass.serial;};
        if(kind==2)onSize=[] {++fns[0].serial;};
        CHECK(!Layout(&radio,layout)&&!g_layout.Valid());
    }
    ResetWrapper();LayoutData layout;CHECK(Layout(&radio,layout));auto original=g_layout;
    ++radioClass.serial;onPrefix=[](const wchar_t*){onPrefix={};++dataClass.serial;};
    LayoutData untouched=original;CHECK(!Layout(&radio,untouched)&&untouched.stride==original.stride&&g_layout.cls.Raw()==original.cls.Raw());
    ResetWrapper();CHECK(Layout(&radio,layout));++fns[0].serial;stride=80;CHECK(Layout(&radio,layout)&&layout.stride==80);
    ResetWrapper();CHECK(Layout(&radio,layout));++dataClass.serial;prefixLink=8;CHECK(Layout(&radio,layout)&&layout.link==8);
    for(int kind=0;kind<4;++kind){ResetWrapper();if(kind==0)prefixLink=INT32_MAX;if(kind==1)prefixUrl=INT32_MAX;if(kind==2)stride=257;if(kind==3)stride=8;
        CHECK(!Layout(&radio,layout));
    }
    for(int kind=0;kind<5;++kind){ResetWrapper();CHECK(Layout(&radio,layout));auto malformed=catalogTrack;
        if(kind==0)malformed.link.Num=INT32_MAX;if(kind==1)malformed.link.Max=1;if(kind==2)malformed.link.Max=INT32_MAX;
        if(kind==3)catalogPath[1]=0;if(kind==4){catalogPath.back()=L'X';malformed.link.Num=int(catalogPath.size());}
        CHECK(Id(reinterpret_cast<uint8_t*>(&malformed),layout)==0);
    }
    ResetWrapper();CHECK(Layout(&radio,layout));Selected selected;
    onField=[](const wchar_t* n){if(!wcscmp(n,L"radioLinks")){onField={};++gm.serial;}};
    CHECK(!FindTrack(&radio,Wanted().track,layout,selected,allowed));
    ResetWrapper();CHECK(Layout(&radio,layout));CHECK(FindTrack(&radio,Wanted().track,layout,selected,allowed));
    CHECK(selected.bytes.data()!=reinterpret_cast<uint8_t*>(&catalogTrack));selected.BindPath();
    ue_wrap::reflection::FString owned{};std::memcpy(&owned,selected.bytes.data()+layout.link,sizeof(owned));
    CHECK(owned.Data==selected.path.data()&&owned.Data!=catalogPath.data());
    // Native frame preparation can replace the catalog; borrowed metadata must not be dispatched.
    ResetWrapper();onParams=[](const wchar_t* n){if(!wcscmp(n,L"openLink")){onParams={};gm.catalog={};}};
    CHECK(!Apply(&radio,Wanted(),allowed)&&opens==0&&radio.mode==0&&radio.pause==0);
    // The selected filename survives an in-place write to the engine's string storage.
    ResetWrapper();const auto wanted=Wanted();const auto oldPath=catalogPath;
    onParams=[](const wchar_t* n){if(!wcscmp(n,L"openLink")){onParams={};catalogPath[0]=L'Z';}};
    CHECK(!Apply(&radio,wanted,allowed)&&opens==1&&radio.selected==oldPath);
    ResetWrapper();Loaded();State out{};
    onNative=[](const wchar_t* n){if(!wcscmp(n,L"IsPreparing")){onNative={};g_layout={};}};
    CHECK(Read(&radio,out,allowed)&&out.track==Wanted().track);
    ResetWrapper();bool value=true;onFind=[](const wchar_t*){onFind={};++player.serial;};CHECK(!Query(&player,L"IsPlaying",value,allowed)&&value&&calls==0);
    for(int kind=0;kind<3;++kind){ResetWrapper();value=true;onFrame=[kind](const wchar_t*){
        onFrame={};if(kind==0)++playerClass.serial;if(kind==1)++fns[2].serial;if(kind==2)authority=false;};
        CHECK(!Query(&player,L"IsPlaying",value,allowed)&&value&&calls==0);
    }
    ResetWrapper();value=true;onNative=[](const wchar_t*){onNative={};++player.serial;};
    CHECK(!Query(&player,L"IsPlaying",value,allowed)&&value&&calls==1);
    ResetWrapper();Loaded();onNative=[](const wchar_t* n){if(!wcscmp(n,L"GetUrl")){onNative={};authority=false;}};
    CHECK(CurrentTrack(&player,allowed)==0&&freed==1);
    for(int kind=1;kind<=3;++kind){ResetWrapper();Loaded();urlFault=kind;CHECK(CurrentTrack(&player,allowed)==0&&freed==1);}
    for(auto* name:{L"IsPreparing",L"GetUrl",L"IsPlaying",L"GetTime"}){
        ResetWrapper();Loaded();out={999,777,1,3};
        onNative=[name](const wchar_t* n){if(!wcscmp(n,name)){onNative={};radio.media=nullptr;}};
        CHECK(!Read(&radio,out,allowed)&&out.track==999&&out.ticks==777);
    }
    for(int kind=1;kind<=2;++kind){ResetWrapper();missing=kind;CHECK(!Apply(&radio,Wanted(),allowed)&&opens==0&&radio.mode==0&&radio.pause==0);}
    for(int kind=0;kind<3;++kind){ResetWrapper();onFrame=[kind](const wchar_t* n){if(!wcscmp(n,L"Seek")){
        onFrame={};if(kind==0)++player.serial;if(kind==1)++fns[5].serial;if(kind==2)authority=false;}};
        CHECK(!Seek(&player,1,allowed)&&seeks==0);
    }
    ResetWrapper();Loaded();player.ticks=0;
    onNative=[](const wchar_t* n){if(!wcscmp(n,L"Seek")){onNative={};authority=false;}};
    CHECK(!Apply(&radio,Wanted(),allowed)&&seeks==1&&plays==0);
    ResetWrapper();Loaded();onNative=[](const wchar_t* n){if(!wcscmp(n,L"Play")){onNative={};++player.serial;}};
    CHECK(!Apply(&radio,Wanted(),allowed)&&plays==1);authority=false;CHECK(!Apply(&radio,Wanted(),allowed)&&plays==1);
    ResetWrapper();auto invalid=Wanted();invalid.ticks=INT64_MAX;CHECK(!Apply(&radio,invalid,allowed)&&calls==0);
    Loaded();player.ticks=INT64_MIN;CHECK(!Apply(&radio,Wanted(),allowed)&&seeks==0);
    player.ticks=INT64_MAX;CHECK(!Apply(&radio,Wanted(),allowed)&&seeks==0);
    std::puts("PASS radio M01-M30: actual wrapper/ParamFrame authority, layout/cache incarnations, owned catalog/path, media binding, FString cleanup and bounded times");
}
