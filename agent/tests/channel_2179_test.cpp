#include "coop/config/config.h"
namespace coop::config_registry::rows {
inline constexpr FlagRow interactable_log{}, channel_shadow_probe{};
}
#include "coop/interactables/interactable_channel.h"
#include <cstdio>
#include <cstdlib>
#include <thread>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
namespace test {
struct Actor { bool ready=false, value=false; int calls=0; } actor;
uint32_t generation=1;
bool resolve=true;
bool Ensure(){return resolve;}
bool IsInstance(void*){return true;}
std::wstring Key(void*){return L"light";}
bool Read(void*,bool& on){on=actor.value;return actor.ready;}
bool Apply(void*,bool on){++actor.calls;if(!actor.ready)return false;actor.value=on;return true;}
}
namespace ue_wrap::reflection {
bool IsLive(void* p){return p==&test::actor;}
bool IsLiveByIndex(void* p,int32_t i){return IsLive(p)&&i==1;}
int32_t InternalIndexOf(void*){return 1;}
const FName& NameOf(void*){static FName n{};return n;}
std::wstring ToString(const FName&){return L"light";}
void* OuterOf(void*){return nullptr;}
std::wstring ClassNameOf(void*){return L"ambientLight_C";}
}
namespace ue_wrap::engine {
bool TryGetActorLocation(void*,FVector&){std::abort();}
void* ParentActorOf(void*,std::wstring*){std::abort();}
}
namespace ue_wrap::prop {
std::wstring GetInteractableKeyString(void*){std::abort();}
std::wstring GetActorSaveKeyString(void*){std::abort();}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::generation;}}
namespace coop::element {
std::wstring PortableWireKey(void*){return L"light";}
std::wstring PortableIdentity(void*){return L"light";}
}
namespace coop::element::scan_hub {void Register(const Consumer&) {}}
namespace coop::net_pump {bool IsInAnnouncedWorld(void*){return true;}}
using namespace coop::interactable_sync;
using coop::net::ReliableKind;
void Index(Channel& c) {c.HubPassBegin(true);c.HubMatch(&test::actor);c.HubPassComplete(true,test::generation);}
void Send(Channel& c,bool on) {
    coop::net::KeyedTogglePayload p{};coop::net::WireKeyFromString(L"light",p.key);p.action=on;
    c.OnReliable(p,0);
}
void Later(Channel& c) {std::this_thread::sleep_for(std::chrono::milliseconds(2050));c.Tick();}
int main() {
    using namespace test;
    Adapter retry{"lamp",ReliableKind::AmbientLightState,Ensure,IsInstance,Key,Read,Apply,
                  nullptr,nullptr,nullptr,true};
    Channel c(retry,Channel::Mode::HostAuth);Index(c);
    Send(c,true);CHECK(actor.calls==1&&!actor.value);
    // Stable actor counts do not mean its components are ready.
    for(int i=0;i<7;++i)Index(c);
    c.Tick();CHECK(actor.calls==2&&!actor.value);
    actor.ready=true;Later(c);CHECK(actor.calls==3&&actor.value);
    Later(c);CHECK(actor.calls==3); // successful retry was removed

    // Newer OFF cancels an older ON queued before the actor was indexed.
    Channel newer(retry,Channel::Mode::HostAuth);Send(newer,true);Index(newer);
    Send(newer,false);CHECK(!actor.value);int calls=actor.calls;
    newer.Tick();CHECK(actor.calls==calls&&!actor.value);

    // Multiple failed writes coalesce to the latest target, including OFF.
    Channel latest(retry,Channel::Mode::HostAuth);Index(latest);actor.ready=false;
    Send(latest,true);Send(latest,false);actor.value=true;actor.ready=true;
    latest.Tick();CHECK(!actor.value);

    // Default adapters keep their deliberate-refusal semantics.
    Adapter refusal=retry;refusal.retryFailedApply=false;
    Channel normal(refusal);Index(normal);actor.ready=false;Send(normal,true);
    calls=actor.calls;actor.ready=true;normal.Tick();CHECK(actor.calls==calls);

    Channel gone(retry,Channel::Mode::HostAuth);Index(gone);actor.ready=false;
    Send(gone,true);gone.OnDisconnect();calls=actor.calls;actor.ready=true;
    gone.Tick();CHECK(actor.calls==calls);

    Channel world(retry,Channel::Mode::HostAuth);Index(world);++generation;
    Send(world,true);calls=actor.calls;world.Tick();CHECK(actor.calls==calls);
    Index(world);world.Tick();CHECK(actor.calls==calls+1&&actor.value);
    // Failed join sends retain a key and read its latest value on retry.
    coop::net::Session host;
    Channel join(retry,Channel::Mode::HostAuth);join.SetSession(&host);Index(join);
    host.sendOk=false;actor.value=true;join.QueueConnectBroadcastForSlot(1);
    CHECK(host.sent.empty());actor.value=false;host.sendOk=true;join.Tick();
    CHECK(host.sent.size()==1&&host.sent[0].slot==1);
    coop::net::KeyedTogglePayload received{};
    std::memcpy(&received,host.sent.back().data.data(),sizeof(received));CHECK(received.action==0);
    join.Tick();CHECK(host.sent.size()==1);
    // Unreadable state is retained too; non-world-ready peers wait.
    host.sent.clear();actor.ready=false;join.QueueConnectBroadcastForSlot(1);
    join.Tick();CHECK(host.sent.empty());actor.ready=true;host.ready=false;
    join.Tick();CHECK(host.sent.empty());host.ready=true;join.Tick();CHECK(host.sent.size()==1);
    // One joiner's snapshot must not advance the all-peer broadcast baseline.
    host.sent.clear();actor.value=false;join.OnLocalEdge(&actor);CHECK(host.sent.size()==1);
    actor.value=true;join.QueueConnectBroadcastForSlot(1);CHECK(host.sent.size()==2);
    join.OnLocalEdge(&actor);CHECK(host.sent.size()==3&&host.sent.back().slot==-1);
    // Slot reuse, session teardown and world transitions cannot inherit old work.
    host.sendOk=false;join.QueueConnectBroadcastForSlot(1);join.OnPeerLeft(1);
    host.sent.clear();host.sendOk=true;join.Tick();CHECK(host.sent.empty());
    host.sendOk=false;join.QueueConnectBroadcastForSlot(1);join.OnDisconnect();
    host.sendOk=true;join.Tick();CHECK(host.sent.empty());
    host.sendOk=false;join.QueueConnectBroadcastForSlot(1);++generation;Index(join);
    host.sendOk=true;join.Tick();CHECK(host.sent.empty());
    // A request made against a stale index waits for a current-world scan.
    Channel loading(retry,Channel::Mode::HostAuth);loading.SetSession(&host);
    loading.QueueConnectBroadcastForSlot(1);CHECK(host.sent.empty());
    Index(loading);loading.Tick();CHECK(host.sent.size()==1);
    std::puts("PASS join retry/current values, unreadable state, readiness, unicast baseline and lifecycle cleanup");
    std::puts("PASS real channel readiness retry, settled index, latest-state ordering, refusal, disconnect and world generation");
}
