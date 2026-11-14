#include "coop/net/protocol.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <functional>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace test { int calls=0, slot=-1, actor=0; bool puppetReady=true; std::vector<std::function<void()>> posted; }
namespace ue_wrap::game_thread { void Post(std::function<void()> task) { test::posted.push_back(std::move(task)); } }
#define UE_LOGW(...) ((void)0)
#define UE_LOGI(...) ((void)0)
namespace coop::net {
inline constexpr int kMaxPeers=32;
struct Message { ReliableKind kind=ReliableKind::ItemActivate; const void* payload=nullptr; int payloadLen=0,senderPeerSlot=0; };
}
namespace coop::element {
inline constexpr uint32_t kInvalidId=0xffffffffu;
enum class ElementType { Player, Prop };
struct Player { uint8_t slot=2; ElementType type=ElementType::Player; ElementType GetType() { return type; } uint8_t PeerSlot() { return slot; } };
struct Registry {
    Player player;
    static Registry& Get() { static Registry r; return r; }
    Player* Get(uint32_t eid) { return eid==100?&player:nullptr; }
};
}
namespace coop {
struct RemotePlayer { bool valid() { return test::puppetReady; } void* GetActor() { return &test::actor; } };
namespace players {
inline constexpr uint8_t kPeerIdUnknown=255;
struct Registry {
    uint8_t local=1;
    static Registry& Get() { static Registry r; return r; }
    uint32_t LocalPlayerElementId() { return 200; }
    uint8_t LocalPeerId() { return local; }
    RemotePlayer* Puppet(int) { static RemotePlayer p; return &p; }
};
}
namespace item_activate {
void ApplyToPuppetOrDefer(uint8_t slot,void* actor,const net::ItemActivatePayload&) {
    ++test::calls; test::slot=slot; CHECK(actor==&test::actor || (!actor && !test::puppetReady));
}
}
}
namespace coop::event_feed {
bool VerifySenderEidRange(int,uint32_t,const char*) { return true; }
void Receive(const net::Message& msg) {
    switch (msg.kind) {
#include "flashlight_dispatch_production.inc"
    default: break;
    }
}
}
int main() {
    using namespace coop;
    net::ItemActivatePayload p{}; p.state=1; p.intensity=5; p.outerConeAngle=45; p.innerConeAngle=20;
    net::Message m; m.payload=&p; m.payloadLen=sizeof(p); m.senderPeerSlot=2;
    auto run=[&] { test::calls=0; test::posted.clear(); event_feed::Receive(m); CHECK(test::posted.empty()); };
    run(); CHECK(test::calls==1 && test::slot==2);
    p.senderElementId=100; run(); CHECK(test::calls==1 && test::slot==2);
    element::Registry::Get().player.slot=3; run(); CHECK(test::calls==0);
    element::Registry::Get().player.slot=2; m.senderPeerSlot=0; run(); CHECK(test::calls==0);
    p.senderElementId=0; run(); CHECK(test::calls==1 && test::slot==0);
    m.senderPeerSlot=31; run(); CHECK(test::calls==1 && test::slot==31);
    m.senderPeerSlot=-1; run(); CHECK(!test::calls);
    m.senderPeerSlot=32; run(); CHECK(!test::calls);
    m.senderPeerSlot=2; m.payload=nullptr; run(); CHECK(!test::calls);
    m.payload=&p; --m.payloadLen; run(); CHECK(!test::calls);
    m.payloadLen=sizeof(p)+1; run(); CHECK(!test::calls);
    m.payloadLen=sizeof(p); p.senderElementId=200; run(); CHECK(!test::calls);
    p.senderElementId=0; m.senderPeerSlot=1; run(); CHECK(!test::calls);
    m.senderPeerSlot=2; test::puppetReady=false; run(); CHECK(test::calls==1);
    std::puts("PASS actual ItemActivate dispatcher: immediate admitted apply, no stale posted work, sender/id ownership, null/length/slot boundaries and pre-pose deferral");
}
