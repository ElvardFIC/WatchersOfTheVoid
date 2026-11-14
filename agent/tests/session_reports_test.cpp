// Actual production console classifiers and sleep controller; ATV lifecycle runs separately.
#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <cstdio>
#include <atomic>
struct FVector { float X=0,Y=0,Z=0; };
struct FRotator { float Pitch=0,Yaw=0,Roll=0; };
namespace ue_wrap { using ::FVector; using ::FRotator; }
#define UE_LOGW(...) ((void)0)
#define UE_LOGI(...) ((void)0)
#define UE_LOGE(...) ((void)0)
struct Obj { void* cls=nullptr; bool live=true, located=true; FVector pos{}; void* used=nullptr; };
namespace R {
void* ClassOf(void* a){return static_cast<Obj*>(a)->cls;}
bool IsLive(void* a){return a && static_cast<Obj*>(a)->live;}
bool IsLiveByIndex(void* a,int){return IsLive(a);}
int32_t FindPropertyOffset(void*,const wchar_t*){return offsetof(Obj,used);}
}
namespace ue_wrap::engine {
bool TryGetActorLocation(void* a,FVector& p){auto* o=static_cast<Obj*>(a);p=o->pos;return o->located;}
}
namespace ue_wrap::notebook {
bool IsEditorWidget(void*){return false;}bool IsPaperDrawWidget(void*){return false;}
bool IsNotebook(void*){return false;}bool IsDrawablePaper(void*){return false;}
void* NotebookOf(void*){return nullptr;}void* PaperOf(void*){return nullptr;}
}
namespace screen {
std::wstring PropKey(const wchar_t*,void*){return {};}

struct Slot { void* cls=nullptr; };
Obj widgetClasses[7],deviceClasses[8];
Slot g_widgets[7],g_devices[8];
const wchar_t* g_widgetKeys[]{L"desk",L"sat",L"radar",L"reactor",L"laptop"};
const wchar_t* g_deviceKeys[]{L"desk",L"sat",L"radar",L"reactor",L"laptop",L"laptop"};
int g_offTfmrWidgetInst=0,g_offArcadeScrWidge=0;
constexpr double kPosGrid=10;
void FillWidgetSlotFromClass(void*){}void FillDeviceSlotFromClass(void*){}
void EnsurePerInstanceDevice(size_t){}
void* FindOwnerByWidgetField(const wchar_t*,int,void*){return nullptr;}
#include "console_identity.inc"
}
namespace coop::players {
struct Registry { static Registry& Get(){static Registry r;return r;}void* Local(){return nullptr;} };
}
namespace coop::net {
enum class Role {Host,Client};
struct Session {Role mode=Role::Host;bool linked=true;bool running(){return true;}bool connected(){return linked;}Role role(){return mode;}};
}
namespace coop::sleep_sync {bool unanimous=false;bool AllPlayersResting(){return unanimous;}}
namespace rest {
int gm=0,body=0;
struct Ref{void* value;void* Raw(){return value;}bool Alive(){return true;}};
Ref g_gm{&gm},g_body{&body};
std::atomic<coop::net::Session*> g_session{nullptr};
bool g_resting=false,g_saidFactor=false;
float sleeping=1,dilation=20,custom=1;
constexpr float kNativeSleepDilation=20;
namespace sg {struct Call{};}
namespace V {enum class Field{Sleeping};bool Read(Field,float* out){*out=sleeping;return true;}}
namespace SLP {bool EnsureResolved(){return true;}float GetGlobalTimeDilation(){return dilation;}bool SetGlobalTimeDilation(float value){dilation=value;return true;}}
namespace FX {void ForEachWorldEffect(void(*)(void*,void*),void*){}}
void DilateEffect(void*,void*){}void SettleNeeds(){}void EnsureWatch(){}
void* CurrentBody(){return &body;}
void BeginRest(){g_resting=true;custom=20;}
void EndRest(const char*){g_resting=false;custom=1;}
}
namespace ue_wrap::world_singleton {void* Gamemode(){return &rest::gm;}}
namespace rest {
#include "sleep_rest_controller.inc"
}
int main(){
    using namespace screen;
    for(int i=0;i<7;++i)g_widgets[i].cls=&widgetClasses[i];
    for(int i=0;i<8;++i)g_devices[i].cls=&deviceClasses[i];
    Obj first{&deviceClasses[1]},second{&deviceClasses[1]};second.pos={10000,0,0};
    Obj widget{&widgetClasses[1]};widget.used=&first;
    const auto a=ClassifyDeviceActorClaimKey(&first),b=ClassifyDeviceActorClaimKey(&second);
    assert(!a.empty() && !b.empty() && a!=b && ClassifyWidgetClaimKey(&widget)==a);
    widget.used=&second;assert(ClassifyWidgetClaimKey(&widget)==b);
    widget.used=nullptr;assert(ClassifyWidgetClaimKey(&widget).empty());
    second.located=false;assert(ClassifyDeviceActorClaimKey(&second).empty());
    Obj desk{&deviceClasses[0]};assert(ClassifyDeviceActorClaimKey(&desk)==L"desk");
    coop::net::Session session;rest::g_session=&session;
    rest::OnSleepPost({});assert(rest::g_resting && rest::dilation==1 && rest::custom==20);
    coop::sleep_sync::unanimous=true;rest::Tick();assert(!rest::g_resting && rest::dilation==20 && rest::custom==1);
    coop::sleep_sync::unanimous=false;rest::Tick();assert(rest::g_resting && rest::dilation==1 && rest::custom==20);
    rest::sleeping=0;rest::dilation=1;rest::Tick();assert(!rest::g_resting && rest::custom==1);
    rest::sleeping=1;session.linked=false;coop::sleep_sync::unanimous=true;
    rest::Tick();assert(!rest::g_resting && rest::dilation==20);
    session.linked=true;session.mode=coop::net::Role::Client;rest::OnSleepPost({});rest::Tick();
    assert(rest::g_resting && rest::dilation==1 && rest::custom==20);
    puts("PASS independent physical console claims");
    puts("PASS actual sleep controller: host personal rest, unanimous acceleration, wake/join changes, solo host and unchanged client rate");
}
