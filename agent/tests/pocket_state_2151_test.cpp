// Exact production pickup gate + pending-state fence; native calls and queue execution doubled.
#include "support_2135.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/props/prop_save_data.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include <atomic>
#include <cstring>
#include <map>
#include <set>
namespace test {
bool applyOk=true,retainNewer=false,dieOnKey=false,dieOnApply=false,dieOnPublish=false;
bool awaiting=false,awaitOnApply=false,carriesRecord=true;
int applies=0,publishes=0;float liveCharge=1,queuedCharge=.25f,publishedCharge=-1;
struct Args {void* target;uint8_t result=1;};uint8_t callerResult=1;
}
namespace ue_wrap::reflection {
int32_t FindParamOffset(void*,const wchar_t* n){return !wcscmp(n,L"return")?offsetof(test::Args,result):0;}
int32_t FunctionFrameSize(void*){return sizeof(test::Args);}
std::wstring ClassNameOf(void* p){CHECK(static_cast<test::Obj*>(p)->live);return L"prop_batts_C";}
}
namespace ue_wrap::script_gate {uint8_t* OutParamPtr(const Call&,int32_t){return &test::callerResult;}}
namespace ue_wrap::prop {
bool IsKeyedInteractable(void*){return true;}
std::wstring GetInteractableKeyString(void*){if(test::dieOnKey)test::actor.live=false;return L"battery";}
}
namespace coop::prop_save_data {
std::set<std::wstring> g_applying,g_publishing;
std::map<std::wstring,int> g_parked;
uint64_t g_worldEpoch=1;
bool Covers(void*){return test::carriesRecord;}
bool StillAwaited(const std::wstring&){return test::awaiting;}
bool ApplyQueued(void*,const std::wstring& key,bool budgeted){CHECK(!budgeted);++test::applies;
 if(test::dieOnApply)test::actor.live=false;
 if(!test::applyOk)return false;test::liveCharge=test::queuedCharge;
 if(!test::retainNewer)g_parked.erase(key);test::awaiting=test::awaitOnApply;return true;
}
#include "../../_research/tests_2151/prepare.h"
bool Publish(coop::net::Session*,void*,const std::wstring&){++test::publishes;test::publishedCharge=test::liveCharge;
 if(test::dieOnPublish)test::actor.live=false;return true;}
}
namespace coop::inventory_pickup_sync {
namespace R=ue_wrap::reflection;
std::atomic<coop::net::Session*> g_session=nullptr;
constexpr const wchar_t* kPocketVerb=L"putObjectInventory2";
namespace detail {
enum class ExtractRestore {Unrelated,Restored,Failed,Invalidated};
ExtractRestore extraction=ExtractRestore::Unrelated;
ExtractRestore RestoreContainerCarrier(const ue_wrap::script_gate::Call&,void*){return extraction;}
}
#include "../../_research/tests_2151/pocket.h"
}
int main(){using namespace test;namespace P=coop::prop_save_data;namespace I=coop::inventory_pickup_sync;
 using V=ue_wrap::script_gate::Verdict;coop::net::Session s;I::g_session=&s;
 coop::players::Registry::Get().local=&ui;Args args{&actor};
 ue_wrap::script_gate::Call c{};c.object=&ui;c.function=&ui;c.locals=reinterpret_cast<uint8_t*>(&args);
 auto run=[&]{return I::OnPocketPre(c);};
 I::detail::extraction=I::detail::ExtractRestore::Restored;
 CHECK(run()==V::Run&&!publishes&&!applies);
 I::detail::extraction=I::detail::ExtractRestore::Failed;args.result=callerResult=1;
 CHECK(run()==V::Cancel&&!publishes&&!applies&&!args.result&&!callerResult);
 I::detail::extraction=I::detail::ExtractRestore::Invalidated;args.result=callerResult=1;
 CHECK(run()==V::Cancel&&!publishes&&!applies&&!args.result&&!callerResult);
 I::detail::extraction=I::detail::ExtractRestore::Unrelated;
 awaiting=true;int before=publishes;
 CHECK(run()==V::Cancel&&publishes==before&&!args.result&&!callerResult);
 // Class coverage can fail independently; the pending author still owns this state.
 carriesRecord=false;args.result=callerResult=1;
 CHECK(run()==V::Cancel&&publishes==before&&!args.result&&!callerResult);
 carriesRecord=true;awaiting=false;
 CHECK(run()==V::Run&&applies==0&&publishedCharge==1);
 P::g_parked[L"battery"]=1;CHECK(run()==V::Run&&applies==1&&publishedCharge==.25f);
 liveCharge=1;queuedCharge=0;P::g_parked[L"battery"]=1;
 CHECK(run()==V::Run&&publishedCharge==0); // empty must not become full
 awaiting=true;P::g_parked[L"battery"]=1;
 CHECK(run()==V::Run&&!awaiting&&publishedCharge==0); // matching apply releases the wait
 awaitOnApply=true;P::g_parked[L"battery"]=1;before=publishes;
 CHECK(run()==V::Cancel&&publishes==before&&P::g_parked.empty()&&awaiting);
 awaitOnApply=awaiting=false;
 applyOk=false;P::g_parked[L"battery"]=1;int n=publishes;
 CHECK(run()==V::Cancel&&publishes==n&&P::g_parked.size()==1);
 CHECK(!args.result&&!callerResult); // cancellation must not reuse previous success
 applyOk=true;retainNewer=true;CHECK(run()==V::Cancel&&publishes==n);
 retainNewer=false;P::g_applying.insert(L"battery");int a=applies;
 CHECK(run()==V::Cancel&&applies==a);P::g_applying.clear();
 dieOnApply=true;CHECK(run()==V::Cancel&&publishes==n);actor.live=true;dieOnApply=false;
 dieOnKey=true;CHECK(run()==V::Cancel&&publishes==n);actor.live=true;dieOnKey=false;
 dieOnPublish=true;CHECK(run()==V::Cancel);actor.live=true;dieOnPublish=false;
 s.linked=false;n=publishes;CHECK(run()==V::Run&&publishes==n);
 puts("PASS pickup applies queued partial/zero state before capture; apply failure/reentry/dead actor defer safely");
}
