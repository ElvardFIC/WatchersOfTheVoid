#include <atomic>
#include <cstdio>
#include <string>

namespace coop::net {
enum class Role { Host, Client };
struct Session {
    Role side = Role::Client;
    bool live = true;
    bool connected() const { return live; }
    Role role() const { return side; }
};
}
namespace sg {
enum class Verdict { Run, Cancel };
struct Call { void* object; bool fromOurCode; };
}
namespace fixture {
coop::net::Session session;
coop::net::Session* active = &session;
bool coin = true, mirrored = true, captured = false, sendOk = true;
int forwarded = 0;
}
namespace I {
bool IsCoinActor(void*) { return fixture::coin; }
bool IsCapturedCoin(void*) { return fixture::captured; }
coop::net::Session* Session() { return fixture::active; }
}
bool IsHostOwnedCoin(void*) { return fixture::mirrored; }
bool ForwardCollectToHost(void*, const wchar_t*) {
    ++fixture::forwarded;
    return fixture::sendOk;
}
std::atomic<unsigned long long> g_seenPressHost{0}, g_seenPressClient{0};
#define UE_LOGI(...) ((void)0)
#define UE_LOGW(...) ((void)0)

namespace ue_wrap::world_identity {uint32_t Generation(){return 1;}}
namespace coop::players {struct Registry{static Registry& Get(){static Registry r;return r;}void* Local(){return nullptr;}};}
struct NativeCollectEntry {struct Ref{void* p=nullptr;void* Get()const{return p;}} coin;bool entered=false;uint64_t epoch=1;uint32_t world=1;};
NativeCollectEntry* g_nativeCollect=nullptr;
uint64_t g_collectEpoch=1;
int performed=0;
bool PerformCollect(void*,void*){++performed;return true;}
#include COLLECT_CALLBACK

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* label) {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
        if (!ok) ++failures;
    };
    int body = 0;
    const sg::Call press{&body, false};
    auto localCollect = [&] {
        int balance = 100;
        bool coinLive = true;
        if (OnCollectVerb(press) == sg::Verdict::Run) {
            balance += 10;
            coinLive = false;
        }
        return balance == 100 && coinLive;
    };

    check(localCollect(), "refused host-owned pickup keeps local coin and canonical balance");
    check(fixture::forwarded == 1, "host-owned press forwards one intent");
    fixture::sendOk = false;
    check(localCollect(), "failed send leaves the coin available for retry");
    fixture::sendOk = true;

    fixture::mirrored = false;
    fixture::captured = true;
    const int before = fixture::forwarded;
    check(localCollect(), "pre-barrier sale coin cannot grant phantom credit");
    check(fixture::forwarded == before, "unpublished sale coin sends no intent");
    fixture::captured = false;
    check(OnCollectVerb(press) == sg::Verdict::Run, "map coin retains native collection");
    fixture::mirrored = true;
    fixture::session.side = coop::net::Role::Host;
    check(OnCollectVerb(press) == sg::Verdict::Cancel && performed==1, "host press enters the owned native collection boundary once");
    NativeCollectEntry entry;entry.coin.p=&body;g_nativeCollect=&entry;
    check(OnCollectVerb({&body,true})==sg::Verdict::Run && entry.entered, "matching host native dispatch runs once");
    check(OnCollectVerb({&body,true})==sg::Verdict::Cancel && performed==2, "recursive host dispatch cannot reuse native admission");
    g_nativeCollect=nullptr;
    fixture::session.side = coop::net::Role::Client;
    check(OnCollectVerb({&body, true}) == sg::Verdict::Cancel, "client mirror cannot author a native collection");
    fixture::coin = false;
    check(OnCollectVerb(press) == sg::Verdict::Run, "other interaction classes are untouched");
    fixture::coin = true;
    fixture::active = nullptr;
    check(OnCollectVerb(press) == sg::Verdict::Run, "solo collection remains native");
    return failures ? 1 : 0;
}
