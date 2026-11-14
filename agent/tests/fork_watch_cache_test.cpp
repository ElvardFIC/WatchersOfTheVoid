#include "../../src/votv-coop/src/ue_wrap/core/script_gate_registry.cpp"
#include "ue_wrap/core/log.h"
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <functional>
#include <string>

#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace ue_wrap::script_gate::detail {
Entry g_nameTable[kSlots];
std::atomic<int> g_nameWatches{0}, g_unjudged{0}, g_namesPending{0};
int g_nameKeyed=0;
std::mutex g_regMutex;
bool RefusedUninstalled(const char*) {return false;}
}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace GT {int posts=0;void Post(std::function<void()>) {++posts;}}
namespace ue_wrap::script_gate {void ResolvePendingNames() {std::abort();}}
#include "fork_watch_cache.inc"

int main() {
    using namespace ue_wrap::script_gate;
    using namespace ue_wrap::script_gate::detail;
    auto cb=+[](const Call&){return Verdict::Run;};
    CHECK(WatchNameScoped(L"deer_C",L"K2_DestroyActor",7,cb,nullptr));
    int slot=-1;
    for(int i=0;i<kSlots;++i)if(g_nameTable[i].key.load()){slot=i;break;}
    CHECK(slot>=0 && g_nameKeyed==1 && GT::posts==1);
    // Mark the production table as the resolver does when the class has no declared body.
    auto& row=g_nameTable[slot];row.dead=true;row.enabled=false;g_nameWatches=0;
    for(int i=0;i<100000;++i)CHECK(!WatchNameScoped(L"deer_C",L"K2_DestroyActor",7,cb,nullptr));
    CHECK(g_nameKeyed==1 && g_nameWatches==0 && GT::posts==1);
    // Text-equal literals from another allocation still find the same dead entry.
    std::wstring cls=L"deer_C",name=L"K2_DestroyActor";
    CHECK(!WatchNameScoped(cls.c_str(),name.c_str(),7,cb,nullptr));
    CHECK(g_nameKeyed==1);
    CHECK(WatchNameScoped(L"deer_C",L"anotherVerb",8,cb,nullptr));
    CHECK(g_nameKeyed==2 && g_nameWatches==1 && GT::posts==2);
    const auto hash=WatchLookupHash(L"deer_C",L"anotherVerb",8,cb,nullptr);
    g_watchLookup[hash&(kWatchLookupSlots-1)]={hash,slot}; // stale/moved cache slot
    CHECK(WatchNameScoped(L"deer_C",L"anotherVerb",8,cb,nullptr));
    CHECK(g_nameKeyed==2 && g_nameWatches==1 && GT::posts==2);
    std::puts("PASS production name-watch cache: 100000 dead re-ensures, text identities and stale-slot fallback without table growth");
}
