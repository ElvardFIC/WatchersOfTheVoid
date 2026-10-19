#include "../../src/votv-coop/src/coop/player/skin_preview.cpp"
#include <cassert>
#include <cstdio>
#include <thread>
namespace test {int pawn=0,mannequin=1;bool success=true,hidden=false;int applies=0;}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p!=nullptr;}
int32_t InternalIndexOf(void* p){return p==&test::mannequin?1:0;}
bool IsLiveByIndex(void* p,int32_t i){return p&&InternalIndexOf(p)==i;}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::puppet {
void* GetMeshPlayerVisibleAsset(void*){return &test::pawn;}
void* GetMeshPlayerVisibleAnimClass(void*){return &test::pawn;}
void* SpawnPuppet(const FVector&,void*,void*,engine::BeforeFinishFn beforeFinish,void* ctx){
 if(beforeFinish)beforeFinish(&test::mannequin,ctx);return &test::mannequin;
}
}
namespace ue_wrap::engine {
bool SetActorEnableCollision(void*,bool){return true;}
bool SetActorHiddenInGame(void*,bool hidden){test::hidden=hidden;return true;}
bool DestroyActor(void*){return true;}
FVector GetCameraLocation(){return {};}
FRotator GetCameraRotation(){return {};}
bool SetActorLocation(void*,const FVector&){return true;}
bool SetActorRotation(void*,const FRotator&){return true;}
}
namespace coop::local_body {void* NativeBodyMesh(){return &test::pawn;}}
namespace coop::client_model {
bool ApplySkinToBody(void*,const std::string&,void*){++test::applies;return test::success;}
}
int main(){namespace P=coop::skin_preview;using namespace test;
 coop::players::Registry::Get().local=&pawn;
 P::Activate("first");P::Tick();assert(!hidden&&applies==1&&P::g_appliedSkin=="first");
 success=false;P::Activate("missing");P::Tick();assert(hidden&&applies==2&&P::g_appliedSkin=="first");
 P::Activate("missing");P::Tick();assert(hidden&&applies==2); // Retry is throttled.
 success=true;std::this_thread::sleep_for(std::chrono::milliseconds(1050));
 P::Activate("missing");P::Tick();assert(!hidden&&applies==3&&P::g_appliedSkin=="missing");
 success=false;P::Activate("another");P::Tick();assert(hidden&&applies==4);
 P::Activate("missing");P::Tick();assert(!hidden&&applies==4);
 P::OnDisconnect();assert(!P::g_mannequin&&P::g_appliedSkin.empty());
 puts("PASS failed preview never displays the previous model as requested; throttled retry and teardown");
}
