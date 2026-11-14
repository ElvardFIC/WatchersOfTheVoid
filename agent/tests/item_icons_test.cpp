#include "ue_wrap/hotbar/icon_cache.h"
#include "ue_wrap/hotbar/icons.h"
#include "ue_wrap/world/game_mode.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <vector>

#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1); } } while(0)
#define UE_LOGI(...) ((void)0)
#define UE_LOGW(...) ((void)0)
namespace fs=std::filesystem;
namespace test {
struct Array {void** data=nullptr;int32_t num=0,max=0;};
struct GI {Array textures;};
GI gi;int gm=0,mode=0;bool available=true,callOK=true,headerOK=true,offsetOK=true;
unsigned version=1,calls=0;std::wstring folder;std::function<void()> onCall;
}
namespace ue_wrap {
class CachedObjRef {
    void* p=nullptr;unsigned version=0;
public:
    void Set(void* obj){p=obj;version=test::version;}
    void* Get() const{return version==test::version?p:nullptr;}
    bool Alive() const{return Get()!=nullptr;}
    void Reset(){p=nullptr;}
};
namespace reflection {
struct FString {wchar_t* Data;int32_t Num,Max;};
void* FindClass(const wchar_t*){return test::available?&test::gm:nullptr;}
void* FindFunction(void*,const wchar_t*){return &test::gm;}
void* FindClassDefaultObject(const wchar_t*){return &test::gm;}
struct InstanceOffset {InstanceOffset(const wchar_t*){}int32_t Of(void*){return test::offsetOK?0:-1;}};
}
class ParamFrame {
public:
    ParamFrame(void*){}bool valid() const{return true;}
    template<class T> void Set(const wchar_t*,T){}
    template<class T> T Get(const wchar_t*){return {test::folder.data(),static_cast<int32_t>(test::folder.size()+1),static_cast<int32_t>(test::folder.size()+1)};}
};
bool Call(void*,ParamFrame&){++test::calls;auto fn=std::move(test::onCall);test::onCall={};if(fn)fn();return test::callOK;}
namespace game_mode {int ReadFrom(void*){return test::mode;}int ReadLocal(){return test::mode;}}
namespace world_singleton {void* GameInstance(){return &test::gi;}void* Gamemode(){return &test::gm;}}
namespace save_record {
struct Arr {const uint8_t* data=nullptr;int32_t num=0;};
bool TryReadArr(void*,int32_t,Arr& out){out={reinterpret_cast<const uint8_t*>(test::gi.textures.data),test::gi.textures.num};return test::headerOK;}
}
namespace hotbar {
namespace R=reflection;namespace SR=save_record;
#include "icons_prepare.inc"
State displayed;bool readOK=true,rebuildOK=true,repair=true;int rebuilt=0;
std::wstring texture=L"null";std::function<void()> onRebuild;
bool Read(State& s){s=displayed;return readOK;}
std::wstring SlotTexture(const State&,int){return texture;}
bool Rebuild(const State&){++rebuilt;auto fn=std::move(onRebuild);onRebuild={};if(fn)fn();if(rebuildOK&&repair)texture=L"ItemTexture";return rebuildOK;}
}
}
namespace coop::hotbar_icon_edge {
namespace HB=ue_wrap::hotbar;
constexpr int kPollTicks=15;ue_wrap::CachedObjRef g_settled;
#include "icons_edge.inc"
}

int main(int argc,char** argv) {
    CHECK(argc==2);const fs::path root=fs::absolute(argv[1]);
    const auto cache=root/L"icons"/L"items";
    fs::create_directories(cache);test::folder=root.wstring();
    const auto empty=cache/L"broken.vimg",valid=cache/L"good.vimg",other=cache/L"keep.txt";
    std::ofstream{empty};std::ofstream(valid)<<"valid icon bytes";std::ofstream{other};
    fs::create_directories(cache/L"nested.vimg");std::ofstream{cache/L"nested.vimg"/L"keep.vimg"};
    void* entries[]={&test::gm,&test::gm};test::gi.textures={entries,2,2};
    namespace H=ue_wrap::hotbar;
    test::mode=ue_wrap::game_mode::kTutorial;CHECK(H::PrepareForWorld(test::mode));
    CHECK(test::calls==0&&fs::exists(empty)&&test::gi.textures.num==2);
    test::mode=0;CHECK(H::PrepareForWorld(0));
    CHECK(!fs::exists(empty)&&fs::file_size(valid)==16&&fs::exists(other)&&fs::exists(cache/L"nested.vimg"/L"keep.vimg"));
    CHECK(test::gi.textures.data==entries&&test::gi.textures.num==0&&test::gi.textures.max==2);
    entries[1]=&test::gm;test::gi.textures.num=2;CHECK(H::PrepareForWorld(0));CHECK(test::gi.textures.num==2);
    entries[1]=nullptr;CHECK(H::PrepareForWorld(0));CHECK(test::gi.textures.num==0);
    test::available=false;CHECK(!H::PrepareForWorld(0));test::available=true;
    test::callOK=false;CHECK(!H::PrepareForWorld(0));test::callOK=true;
    test::headerOK=false;CHECK(!H::PrepareForWorld(0));test::headerOK=true;
    std::ofstream{empty};test::gi.textures.num=2;
    test::onCall=[]{++test::version;};CHECK(!H::PrepareForWorld(0));
    CHECK(fs::exists(empty)&&test::gi.textures.num==2);
    test::onCall=[]{test::mode=2;};CHECK(!H::PrepareForWorld(0));
    CHECK(fs::exists(empty));test::mode=0;
    CHECK(H::PrepareForWorld(0)&&!fs::exists(empty));
    // A concurrent generator must retain its exact open file and can finish it.
    const auto busy=cache/L"busy.vimg";
    HANDLE writer=::CreateFileW(busy.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,0,nullptr);
    CHECK(writer!=INVALID_HANDLE_VALUE);CHECK(!H::PrepareForWorld(0));CHECK(fs::exists(busy));
    DWORD written=0;CHECK(::WriteFile(writer,"new icon",8,&written,nullptr));::CloseHandle(writer);
    CHECK(H::PrepareForWorld(0)&&fs::file_size(busy)==8);
    puts("PASS actual cache preparation: empty files/null textures regenerate; valid files, nested files and concurrent writers survive; Tutorial and obsolete owners refuse mutation");

    H::State state;state.iconNames=2;state.iconTextures=2;
    CHECK(!state.IconsReady());state.validIconTextures=2;CHECK(state.IconsReady());
    state.iconNames=1;CHECK(!state.IconsReady());state.iconNames=2;
    state.renderer=&test::gi;state.ui=&test::gm;state.carried=1;state.slots=1;H::displayed=state;
    auto poll=[] {for(int i=0;i<15;++i)coop::hotbar_icon_edge::Tick();};
    test::mode=2;poll();CHECK(H::rebuilt==0);
    test::mode=0;H::rebuildOK=false;poll();CHECK(H::rebuilt==1);
    H::rebuildOK=true;H::repair=false;poll();CHECK(H::rebuilt==2);
    H::repair=true;poll();CHECK(H::rebuilt==3&&H::texture==L"ItemTexture");poll();CHECK(H::rebuilt==3);
    ++test::version;H::texture=L"null";H::onRebuild=[]{++test::version;};poll();CHECK(H::rebuilt==4);
    CHECK(!coop::hotbar_icon_edge::g_settled.Alive());
    H::texture=L"null";poll();CHECK(H::rebuilt==5);
    puts("PASS actual refresh policy: null tables do not become ready; Tutorial stays native; refused/incomplete rebuild retries, success settles and stale renderer cannot settle a new world");
}
