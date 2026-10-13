// Actual mounting query with engine/registry boundaries doubled.
#include "support_2135.h"
#include "coop/interactables/atv_sync_internal.h"
#include "coop/player/players_registry.h"
#include <unordered_map>
void* held=nullptr;
namespace coop::atv_sync {
namespace R=ue_wrap::reflection;
std::unordered_map<std::wstring,AtvEntry> g_atvs;
uint32_t g_indexGen=0;
bool IndexCurrent(){return g_indexGen==test::world;}
bool IsLocalGrabber(void* actor,void*){return actor&&actor==held;}
// Generated directly from production source by run_2148.ps1; not a second implementation.
#include "../../_research/tests_2148/atv_mount_body.h"
}
namespace S=coop::atv_sync;
int main(){using namespace test;
 auto& registry=coop::players::Registry::Get();registry.peer=1;
 S::g_indexGen=world;S::AtvEntry e{};e.actor=&actor;e.idx=1;e.authorSlot=2;
 S::g_atvs[L"atv"]=e;uint8_t holder=0xFF;
 CHECK(S::IsHeldForMount(&actor,&holder)&&holder==2);
 S::g_atvs[L"atv"].occupantSlot=2;CHECK(!S::IsHeldForMount(&actor,nullptr)); // seated != carried
 S::g_atvs[L"atv"].occupantSlot=0xFF;S::g_atvs[L"atv"].authorSlot=1;
 CHECK(!S::IsHeldForMount(&actor,nullptr)); // a stale local carry claim cannot deny after release
 held=&actor;CHECK(S::IsHeldForMount(&actor,&holder)&&holder==1);
 ++world;CHECK(S::IsHeldForMount(&actor,nullptr)); // fresh local hold before indexing
 held=nullptr;CHECK(!S::IsHeldForMount(&actor,nullptr));
 S::g_indexGen=world;S::g_atvs[L"atv"].authorSlot=2;actor.live=false;
 CHECK(!S::IsHeldForMount(&actor,nullptr));actor.live=true;
 S::g_atvs[L"atv"].authorSlot=0xFF;CHECK(!S::IsHeldForMount(&actor,nullptr));
 CHECK(!S::IsHeldForMount(nullptr,nullptr));
 puts("PASS actual ATV mount query: remote/local carry, seated, release, dead actor and world change");
}
