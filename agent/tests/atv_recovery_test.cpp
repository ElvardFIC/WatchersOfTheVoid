// Actual hit guard plus unchanged controller functions extracted by the runner.
#include "../../src/votv-coop/src/coop/interactables/atv_hit_guard.cpp"
#include "facts_test_support.h"
#include "coop/net/session.h"
#include "ue_wrap/core/script_gate.h"
#include <array>
#include <cassert>
#include <cstring>
#include <thread>
#include <vector>
namespace test {
int functions[7]{};
int missing=-1, invalid=-1, capacity=7, resolves=0, logs=0;
int badOffset=121, badFrame=128;
std::vector<std::pair<void*,ue_wrap::game_thread::UFunctionInterceptor>> hooks;
int honkTries=0,mountTries=0,damageTries=0,subscriptions=0,scans=0;
bool verbsReady=false;
}
namespace ue_wrap::log {void Write(Level,const char*,...){++test::logs;}}
namespace ue_wrap::facts {
Resolved Resolve(const Function& row) {
    using namespace coop::atv_hit_guard;
    const Function* rows[]={&kRowHitMesh,&kRowHitCapsule,&kRowHitBackWheelR,&kRowHitBackWheelL,
        &kRowHitFrontWheelR,&kRowHitFrontWheelRoot,&kRowHitBackWheelRoot};
    ++test::resolves;
    for(int i=0;i<7;++i) if(rows[i]==&row)
        return i==test::missing?Resolved{}:Resolved(&test::functions[i],&row);
    std::abort();
}
}
namespace ue_wrap::reflection {
int32_t FindParamOffset(void* fn,const wchar_t*) {
    return fn==&test::functions[test::invalid<0?0:test::invalid] && test::invalid>=0?test::badOffset:16;
}
int32_t FunctionFrameSize(void* fn) {
    return test::invalid>=0 && fn==&test::functions[test::invalid]?test::badFrame:128;
}
}
namespace ue_wrap::game_thread {
bool RegisterInterceptor(const facts::Resolved& fn,UFunctionInterceptor cb) {
    const auto pair=std::make_pair(fn.Fn(),cb);
    for(const auto& p:test::hooks) if(p==pair)return true;
    if(test::hooks.size()>=test::capacity)return false;
    test::hooks.push_back(pair);return true;
}
}
namespace ue_wrap::atv {bool EnsureResolved(){return true;}}
namespace ue_wrap::atv_condition {bool Resolve(){return true;}}
namespace coop::atv_steering_sync {void Install(coop::net::Session*){}}
namespace coop::atv_damage_gate {void Install(){++test::damageTries;}}
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t*,const wchar_t* name,int,PreFn,PostFn) {
    if(std::wcscmp(name,L"actionName")==0){++test::mountTries;return true;}
    ++test::honkTries;return test::verbsReady;
}
void ResolvePendingNames(){}
}
namespace coop::atv_sync {
namespace A=ue_wrap::atv;
bool g_installed=false;
std::atomic<coop::net::Session*> g_session{nullptr};
void RegisterWithScanHub(){++test::scans;}
void SubscribeDepartures(){++test::subscriptions;}
ue_wrap::script_gate::Verdict OnHonkPre(const ue_wrap::script_gate::Call&){return ue_wrap::script_gate::Verdict::Run;}
ue_wrap::script_gate::Verdict OnMountPre(const ue_wrap::script_gate::Call&){return ue_wrap::script_gate::Verdict::Run;}
#include "atv_install.inc"
}
int main(int argc,char** argv) {
    using namespace coop::atv_hit_guard;
    const std::string mode=argc>1?argv[1]:"metadata";
    if(mode=="metadata")test::missing=3;
    else if(mode=="capacity")test::capacity=4;
    else if(mode=="frame")test::invalid=2;
    else return 2;
    coop::net::Session session;
    coop::atv_sync::Install(&session);
    assert(coop::atv_sync::g_installed && !Armed());
    const int resolved=test::resolves,logged=test::logs;
    coop::atv_sync::Install(&session);
    assert(test::resolves==resolved && test::logs==logged);
    if(mode=="frame") {
        // Truncated vector, negative frame, offset beyond frame, and exact fit.
        for(auto pair:{std::pair{0,-1},std::pair{129,128},std::pair{121,128}}) {
            test::badOffset=pair.first;test::badFrame=pair.second;
            g_nextHitTry={};coop::atv_sync::Install(&session);assert(!Armed());
        }
        test::badOffset=116;
    }
    test::missing=-1;test::capacity=7;g_nextHitTry={};
    coop::atv_sync::Install(&session);
    assert(Armed() && test::hooks.size()==7);
    assert(test::subscriptions==1 && test::scans==1); // latch still protects one-time work
    const int done=test::resolves;coop::atv_sync::Install(&session);assert(test::resolves==done);
    assert(test::honkTries==1 && test::mountTries==1);
    // The rejected honk retries independently without duplicating the mount watch.
    test::verbsReady=true;std::this_thread::sleep_for(std::chrono::milliseconds(1010));
    coop::atv_sync::Install(&session);
    assert(test::honkTries==2 && test::mountTries==1);
    int owner=0,mirror=0;void* owned[]={&owner};PublishOwned(owned,1);SetActive(true);
    for(const auto& hook:test::hooks) {
        std::array<unsigned char,144> frame;frame.fill(0x5A);const auto original=frame;
        assert(!hook.second(&owner,frame.data()) && frame==original);
        assert(!hook.second(&mirror,frame.data()));
        const int off=ue_wrap::reflection::FindParamOffset(hook.first,L"NormalImpulse");
        for(int i=0;i<int(frame.size());++i)assert(frame[i]==(i>=off&&i<off+12?0:0x5A));
        frame=original;SetActive(false);assert(!hook.second(&mirror,frame.data()) && frame==original);SetActive(true);
    }
    assert(ReadCounters().unresolved==0);
    std::printf("PASS ATV %s recovery, install latch, bounded retries, independent verbs and impulse writes\n",mode.c_str());
}
