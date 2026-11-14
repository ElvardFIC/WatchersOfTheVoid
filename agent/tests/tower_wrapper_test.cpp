// Compile each real tower wrapper and the real ParamFrame against native doubles.
#ifdef TEST_RADIO
#ifndef TOWER_WRAPPER_SOURCE
#define TOWER_WRAPPER_SOURCE "../../src/votv-coop/src/ue_wrap/devices/radio_tower.cpp"
#endif
#include TOWER_WRAPPER_SOURCE
namespace W = ue_wrap::radio_tower;
#else
#ifndef TOWER_WRAPPER_SOURCE
#define TOWER_WRAPPER_SOURCE "../../src/votv-coop/src/ue_wrap/devices/coord_tower.cpp"
#endif
#include TOWER_WRAPPER_SOURCE
namespace W = ue_wrap::coord_tower;
#endif
#include "../../src/votv-coop/src/ue_wrap/core/call.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <stdexcept>
#include <map>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)

namespace test {
using namespace ue_wrap;
struct Obj {
    void* cls = nullptr;
    int id = 0, serial = 1;
    bool live = true, scoped = false;
    reflection::FName name{};
};
struct Tower : Obj {
    int32_t identity = 7, buttonIndex = 0, fuseIndex = 0, size[2] = {2,2};
    uint8_t broken = 1, alternateBroken = 0, opened = 1, anim = 0, moving = 0, skip = 0;
    uint8_t lookButton = 0, lookLever = 0, lookFuse = 0, lookOther = 0;
    field_io::TArrayView fuses{}, puzzle{}, buttons{}, slots{};
    void* lever = nullptr;
    void* other = nullptr;
    void* timeline = nullptr;
    void* reset = nullptr;
    void* audio = nullptr;
};
struct Gm : Obj { void* tower = nullptr; } gm;
Obj cls, otherClass, actorClass, componentClass, gmClass, functionClass;
Obj player, fuse, part, part2, timeline, timeline2, audio;
Tower tower, tower2;
Obj fn, locationFn, audioFn, leverFn;
std::vector<Obj*> objects;
std::map<int,std::wstring> names;
std::function<void()> onFind, onFrame, onParams, onNative, onLocation, onTimeline, onHit, onAudio, onLever, onField;
std::wstring badField;
int badOffset = -1, boolMask = 1, frameSize = 64, calls = 0, frames = 0, locations = 0, audioCalls = 0;
int expectedButton = 0;
bool alternate = false, locationOk = true;
uint32_t world = 1;
uint8_t fuseBytes[8]{}, puzzleBytes[25]{};
void* parts[25]{};
FVector location{1,2,3};
void Fire(std::function<void()>& cb) { auto owned = std::move(cb); cb = {}; if (owned) owned(); }
void InitObj(Obj& o, void* type, const wchar_t* name, bool scoped = false) {
    o.cls = type; o.live = true; o.scoped = scoped; ++o.serial;
    if (!o.id) { o.id = int(objects.size()+1); objects.push_back(&o); }
    o.name = {o.id,0}; names[o.id] = name;
}
void Reset() {
    onFind={};onFrame={};onParams={};onNative={};onLocation={};onTimeline={};onHit={};onAudio={};onLever={};onField={};
    badField.clear(); badOffset=-1; boolMask=1; frameSize=64; calls=frames=locations=audioCalls=0;
    expectedButton=0; alternate=false; locationOk=true; location={1,2,3}; ++world;
#ifdef TEST_RADIO
    InitObj(cls,&otherClass,L"radiotower_C");
#else
    InitObj(cls,&otherClass,L"coordRadarDish_C");
#endif
    InitObj(otherClass,&otherClass,L"other"); InitObj(actorClass,&otherClass,L"actor");
    InitObj(componentClass,&otherClass,L"component"); InitObj(gmClass,&otherClass,L"mainGamemode_C");
    InitObj(functionClass,&otherClass,L"Function"); InitObj(fn,&functionClass,L"verb");
    InitObj(locationFn,&functionClass,L"K2_GetComponentLocation");
    InitObj(audioFn,&functionClass,L"Play"); InitObj(leverFn,&functionClass,L"PlayFromStart");
    InitObj(player,&actorClass,L"player",true);InitObj(fuse,&actorClass,L"fuse",true);
    InitObj(part,&componentClass,L"part",true);InitObj(part2,&componentClass,L"part2",true);
    InitObj(timeline,&componentClass,L"timeline",true);InitObj(timeline2,&componentClass,L"timeline2",true);
    InitObj(audio,&componentClass,L"audio",true); InitObj(tower,&cls,L"tower",true);
    InitObj(tower2,&cls,L"tower2",true);InitObj(gm,&gmClass,L"gamemode",true);gm.tower=&tower;
    tower.identity=7;tower.buttonIndex=tower.fuseIndex=0;tower.size[0]=tower.size[1]=2;
    tower.broken=tower.opened=1;tower.alternateBroken=tower.anim=tower.moving=tower.skip=0;
    tower.lookButton=tower.lookLever=tower.lookFuse=tower.lookOther=0;
    std::fill(std::begin(fuseBytes),std::end(fuseBytes),1);
    std::fill(std::begin(puzzleBytes),std::end(puzzleBytes),0);
    std::fill(std::begin(parts),std::end(parts),&part);
    tower.fuses={fuseBytes,8,8};tower.puzzle={puzzleBytes,4,25};
    tower.buttons={reinterpret_cast<uint8_t*>(parts),4,25};tower.slots={reinterpret_cast<uint8_t*>(parts),8,25};
    tower.lever=tower.other=&part;tower.timeline=&timeline;tower.reset=&timeline2;tower.audio=&audio;
}
bool Read(W::State& out) { return W::Read(&tower,out); }
bool Use() { return W::Use(&tower,&player,{W::Part::Button,0}); }
void Pass(int id) {
#ifdef TEST_RADIO
    std::printf("PASS RW%02d\n",id);
#else
    std::printf("PASS CW%02d\n",id);
#endif
}
}

namespace ue_wrap::reflection {
void* ClassOf(void* p) { return p ? static_cast<test::Obj*>(p)->cls : nullptr; }
bool IsLive(void* p) { return p && static_cast<test::Obj*>(p)->live; }
int32_t InternalIndexOf(void* p) { return p ? static_cast<test::Obj*>(p)->id : -1; }
void* ObjectAt(int32_t i) { return i>0 && size_t(i)<=test::objects.size() ? test::objects[i-1] : nullptr; }
bool IsLiveByIndex(void* p,int32_t i) { return p && ObjectAt(i)==p && IsLive(p); }
int32_t SlotSerial(int32_t i) { auto* o=static_cast<test::Obj*>(ObjectAt(i)); return o?o->serial:0; }
int32_t AllocateSlotSerial(int32_t i) { return SlotSerial(i); }
int32_t SlotFlags(int32_t) { return 0; }
const FName& NameOf(void* p) { return static_cast<test::Obj*>(p)->name; }
std::wstring ToString(const FName& n) { return test::names[n.ComparisonIndex]; }
bool NameEquals(const FName& n,const wchar_t* s) { return test::names[n.ComparisonIndex]==s; }
bool NameStartsWith(const FName& n,const wchar_t* s) { return test::names[n.ComparisonIndex].starts_with(s); }
int32_t StructSize(void* c) { return c==&test::gmClass?sizeof(test::Gm):sizeof(test::Tower); }
int32_t FindPropertyOffset(void*,const wchar_t* n) {
    using namespace test;Fire(onField);
    if (badField==n) return badOffset;
#define FIELD(name,field) if (!std::wcscmp(n,L##name)) return offsetof(Tower,field)
    FIELD("id",identity);FIELD("lookAtButtonIndex",buttonIndex);FIELD("lookAtIndex",buttonIndex);
    FIELD("lookAtFuseIndex",fuseIndex);FIELD("puzzleSize",size);FIELD("fuses",fuses);
    FIELD("puzzleLights",puzzle);FIELD("puzzleData",puzzle);FIELD("puzzle_buttons",buttons);FIELD("puzzleButtons",buttons);
    FIELD("fusesButtons",slots);FIELD("buttonLever",lever);FIELD("button_confirm",lever);
    FIELD("button_retract1",other);FIELD("button_reset",other);FIELD("leverTL",timeline);FIELD("TL_lever",timeline);
    FIELD("TL_reset",reset);
#undef FIELD
    if (std::wstring(n).starts_with(L"audio_")) return offsetof(Tower,audio);
    if (!std::wcscmp(n,L"radiotower")) return offsetof(Gm,tower);
    return -1;
}
bool FindBoolProperty(void*,const wchar_t* n,int32_t& off,uint8_t& mask) {
    using namespace test; mask=static_cast<uint8_t>(boolMask);off=-1;
    if (badField==n) { off=badOffset;return true; }
#define FIELD(name,field) if (!std::wcscmp(n,L##name)) off=offsetof(Tower,field)
    FIELD("isBroken",broken);FIELD("opened",opened);FIELD("isAnim",anim);FIELD("animation",anim);
    FIELD("leverMoving",moving);FIELD("skipPuzzle",skip);FIELD("isLookAtButton",lookButton);
    FIELD("isLookAtLever",lookLever);FIELD("lookAtConfirm",lookLever);FIELD("isLookAtFuse",lookFuse);
    FIELD("lookAtFuse",lookFuse);FIELD("isLookAtRetract",lookOther);FIELD("lookAtReset",lookOther);
#undef FIELD
    if (alternate && !std::wcscmp(n,L"isBroken")) off=offsetof(Tower,alternateBroken);
    return off>=0;
}
void* FindDispatchFunctionCached(void*,const wchar_t* name) {
    if (!std::wcscmp(name,L"K2_GetComponentLocation")) return &test::locationFn;
    if (!std::wcscmp(name,L"Play")) return &test::audioFn;
    if (!std::wcscmp(name,L"PlayFromStart") || !std::wcscmp(name,L"ReverseFromEnd")) return &test::leverFn;
    test::Fire(test::onFind);return &test::fn;
}
int32_t FunctionFrameSize(void* f) {if(f==&test::fn)test::Fire(test::onFrame);return test::frameSize;}
std::vector<ParamInfo> FunctionParams(void* f) {
    if(f==&test::fn){++test::frames;test::Fire(test::onParams);}
    return {{L"player",0,8,0},{L"lookAtComponent",8,8,0},{L"holdObject",16,8,0},{L"action",24,1,0},
            {L"Condition",25,1,0},{L"isBroken",26,1,0},{L"pullLever",27,1,0},
            {L"radiotower",32,8,0},{L"damagedHealth",40,1,0},{L"ReturnValue",48,12,0},{L"StartTime",0,4,0}};
}
bool CallFunction(void*,void* fn,void* params) {
    using namespace test;
    if(fn==&locationFn){++locations;Fire(onLocation);std::memcpy(static_cast<uint8_t*>(params)+48,&location,sizeof(location));return locationOk;}
    if(fn==&audioFn){++audioCalls;Fire(onAudio);return true;}
    if(fn==&leverFn){Fire(onLever);return true;}
    ++calls;Fire(onNative);return true;
}
}
namespace ue_wrap::world_identity {
uint32_t Generation(){return test::world;}
void* CurrentWorld(){return reinterpret_cast<void*>(uintptr_t(test::world));}
void* WorldOf(void* p){return static_cast<test::Obj*>(p)->scoped?CurrentWorld():nullptr;}
}
namespace ue_wrap::engine_timeline {
bool Read(void*,State& out){test::Fire(test::onTimeline);out={true,false,0.5f};return true;}
}
namespace ue_wrap::hit_result {
bool WriteWhile(ParamFrame&,const wchar_t*,void*,void*,const FVector&,const std::function<bool()>& current){
    if(!current())return false;test::Fire(test::onHit);return current();
}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
#ifdef TEST_RADIO
namespace ue_wrap::world_singleton {void* Gamemode(){return &test::gm;}void* Find(const wchar_t*){return &test::player;}}
#else
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t*){return &test::cls;}
size_t ForEachInstance(void*,InstanceFn f,void* ctx){f(ctx,&test::tower,test::tower.id);return 1;}
}
namespace ue_wrap::engine {void* SpawnActor(void*,const FVector&,bool,void(*)(void*,void*),void*){return nullptr;}}
namespace ue_wrap::engine {void* SpawnActorTracked(void*,const FVector&,bool& attempted,const std::function<bool()>&){attempted=false;return nullptr;}}
namespace ue_wrap::prop {std::wstring GetPropNameString(void*){return L"fuse_0";}}
#endif

int main() {
    using namespace test;
    W::State out{};
    Reset();CHECK(Read(out));tower.cls=&otherClass;CHECK(!W::WriteBroken(&tower,false));CHECK(tower.broken==1);Pass(1);
    Reset();CHECK(Read(out));++cls.serial;alternate=true;CHECK(Read(out)&&!out.isBroken);Pass(2);
    Reset();badField=L"fuses";CHECK(!Read(out));badField.clear();CHECK(!Read(out));++cls.serial;CHECK(Read(out));Pass(3);
    Reset();badField=L"lookAtFuseIndex";badOffset=sizeof(Tower)-1;CHECK(!Read(out));Pass(4);
    for(int mask:{0,3}){Reset();boolMask=mask;CHECK(!Read(out));}
    Reset();badField=L"isBroken";badOffset=sizeof(Tower);CHECK(!Read(out));Pass(5);
    Reset();badField=L"fuses";badOffset=sizeof(Tower)-8;CHECK(!Read(out));Pass(6);
    Reset();badField=L"audio_click";badOffset=sizeof(Tower)-4;CHECK(!Read(out));Pass(7);
    Reset();badField=L"isLookAtButton";badOffset=sizeof(Tower);CHECK(!Use());CHECK(calls==0);Pass(8);
    Reset();onTimeline=[]{++tower.serial;};CHECK(!Read(out));Pass(9);
    Reset();onTimeline=[]{++world;};CHECK(!Read(out));Pass(10);
    Reset();onTimeline=[]{tower.cls=&otherClass;};CHECK(!Read(out));Pass(11);
    Reset();onTimeline=[]{tower.timeline=&timeline2;};CHECK(!Read(out));Pass(12);
#ifdef TEST_RADIO
    Reset();onTimeline=[]{onTimeline=[]{tower.timeline=&part2;};};CHECK(!Read(out));
    Reset();onTimeline=[]{onTimeline=[]{++timeline.serial;};};CHECK(!Read(out));
    Reset();onTimeline=[]{tower.reset=&part2;};CHECK(!Read(out));
#endif
    Reset();out.isBroken=false;out.fuseCount=77;onTimeline=[]{++world;};CHECK(!Read(out));CHECK(!out.isBroken&&out.fuseCount==77);Pass(13);
    for(int fault=0;fault<4;++fault){Reset();if(fault==0)tower.fuses.max=7;if(fault==1)tower.fuses.num=-1;if(fault==2)tower.fuses.num=9;if(fault==3)tower.fuses.data=nullptr;CHECK(!Read(out));}Pass(14);
    Reset();fuseBytes[7]=3;CHECK(!Read(out));Pass(15);
    for(int fault=0;fault<4;++fault){Reset();if(fault==0)tower.puzzle.max=3;if(fault==1)tower.puzzle.num=-1;if(fault==2)tower.puzzle.num=26;if(fault==3)tower.puzzle.data=nullptr;CHECK(!Read(out));}Pass(16);
    Reset();CHECK(!W::WriteFuse(&tower,8,0));CHECK(!W::WriteFuse(&tower,-1,0));tower.fuses.max=7;CHECK(!W::WriteFuse(&tower,0,0));CHECK(fuseBytes[0]==1);Pass(17);
    Reset();CHECK(!W::WriteFuse(&tower,0,3));CHECK(fuseBytes[0]==1);CHECK(W::WriteFuse(&tower,0,2));Pass(18);
    Reset();bool values[30]{};
#ifdef TEST_RADIO
    auto write=[&](int n){return W::WriteCells(&tower,values,n);};
#else
    auto write=[&](int n){return W::WriteLights(&tower,values,n);};
#endif
    CHECK(!write(-1)&&!write(26)&&!write(3)&&write(4));tower.puzzle.max=3;CHECK(!write(4));Pass(19);
    Reset();tower.buttons.max=3;CHECK(!Use());CHECK(locations==0);tower.buttons.max=25;CHECK(!W::Use(&tower,&player,{W::Part::Button,25}));Pass(20);
    Reset();FVector pos{9,9,9};location.X=std::numeric_limits<float>::quiet_NaN();CHECK(!W::PartLocation(&tower,{W::Part::Button,0},pos));CHECK(pos.X==9);CHECK(!Use());CHECK(calls==0);Pass(21);
    Reset();onLocation=[]{parts[0]=&part2;};CHECK(!W::PartLocation(&tower,{W::Part::Button,0},pos));CHECK(pos.X==9);Pass(22);
    for(int seam=0;seam<4;++seam){Reset();auto retire=[]{++tower.serial;};if(seam==0)onLocation=retire;if(seam==1)onFind=retire;if(seam==2)onFrame=retire;if(seam==3)onHit=retire;CHECK(!Use());CHECK(calls==0);}Pass(23);
    Reset();onHit=[]{++player.serial;};CHECK(!Use());CHECK(calls==0);Pass(24);
    Reset();onHit=[]{parts[0]=&part2;};CHECK(!Use());CHECK(calls==0);Reset();onHit=[]{++part.serial;};CHECK(!Use());CHECK(calls==0);Pass(25);
    Reset();onFrame=[]{++fn.serial;};CHECK(!Use());CHECK(calls==0);Pass(26);
    Reset();frameSize=1025;CHECK(!Use());CHECK(calls==0&&frames==0);Pass(27);
    Reset();W::LookAt input{W::Part::Button,0};onLocation=[&]{input={W::Part::Fuse,7};};onNative=[]{CHECK(tower.lookButton==1&&tower.buttonIndex==0);};CHECK(W::Use(&tower,&player,input));CHECK(calls==1);Pass(28);
    Reset();onHit=[]{CHECK(!Use());CHECK(!W::InsertFuse(&tower,&player,0,&fuse));};CHECK(Use());CHECK(calls==1);Pass(29);
    Reset();onNative=[]{tower.lookButton=0;tower.lookFuse=1;tower.fuseIndex=6;};CHECK(!Use());CHECK(tower.lookFuse==1&&tower.fuseIndex==6);Pass(30);
    Reset();tower.lookOther=1;onHit=[]{throw std::runtime_error("native double");};try{Use();CHECK(false);}catch(const std::runtime_error&){}CHECK(tower.lookOther==1&&tower.lookButton==0);CHECK(Use());Pass(31);
    Reset();onParams=[]{++fuse.serial;};CHECK(!W::InsertFuse(&tower,&player,0,&fuse));CHECK(calls==0);Pass(32);
    Reset();onFind=[]{++tower.serial;};CHECK(!W::UpdPuzzle(&tower));CHECK(calls==0);Reset();onFrame=[]{++world;};CHECK(!W::UpdFuses(&tower));CHECK(calls==0);Pass(33);
    Reset();onAudio=[]{tower.audio=&part2;};CHECK(!W::Play(&tower,W::Sound::Click));CHECK(audioCalls==1);CHECK(!W::Play(&tower,static_cast<W::Sound>(255)));Pass(34);
    Reset();
#ifdef TEST_RADIO
    onLever=[]{tower.timeline=&timeline2;};CHECK(!W::PlayLever(&tower,W::Lever::Confirm,true));
#else
    onFrame=[]{++tower.serial;};CHECK(!W::MoveLever(&tower,true));CHECK(calls==0);
#endif
    Pass(35);
    Reset();tower.lookButton=1;tower.buttonIndex=99;W::LookAt look{W::Part::Fuse,7};CHECK(!W::ReadLookAt(&tower,look));CHECK(look.part==W::Part::Fuse&&look.index==7);Pass(36);
    Reset();tower.fuseIndex=8;int32_t slot=99;CHECK(!W::ReadFuseLook(&tower,slot));CHECK(slot==99);Pass(37);
#ifdef TEST_RADIO
    Reset();CHECK(W::Instance()==&tower);gm.tower=&tower2;CHECK(W::Instance()==&tower2);
    badField=L"radiotower";badOffset=sizeof(Gm)-1;CHECK(!W::Instance());badField.clear();
    onField=[]{++gm.serial;};CHECK(!W::Instance());Pass(38);
#endif
    Reset();CHECK(Use());CHECK(W::InsertFuse(&tower,&player,0,&fuse));CHECK(calls==2);
    CHECK(W::UpdPuzzle(&tower)&&W::UpdFuses(&tower));CHECK(Read(out));
    std::puts("PASS native tower wrapper and real ParamFrame controls");
}
