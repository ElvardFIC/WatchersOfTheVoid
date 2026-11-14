#include "coop/net/protocol.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/types.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
#define UE_LOGI(...) ((void)0)

namespace test {
int actor, other, world;
int serial=1;
bool alive=true, robot=false, trash=false, velocityOk=true;
float speed=200;
uint8_t slot=31;
int minted=0, notes=0, coasts=0, corrections=0;
uint16_t seq=73, nonce=0;
void* noted=nullptr;
}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void* p) { return p==&test::actor?1:2; }
int32_t AllocateSlotSerial(int32_t) { return test::serial; }
int32_t SlotSerial(int32_t) { return test::serial; }
bool IsLiveByIndex(void* p,int32_t) { return p && test::alive; }
}
namespace ue_wrap::world_identity {
void* WorldOf(void*) { return &test::world; }
void* CurrentWorld() { return &test::world; }
}
namespace ue_wrap::prop {
struct VelocityState { FVector linearCmS{}, angularDegS{}; bool ok=false; };
VelocityState GetPhysicsVelocity(void*) { return {{test::speed,0,0},{0,0,1},test::velocityOk}; }
}
namespace ue_wrap::engine {
bool GetActorRootPhysicsVelocity(void*,FVector&,FVector&) { return false; }
bool TryGetActorLocation(void*,FVector& p) { p={1,2,3};return true; }
bool TryGetActorRotation(void*,FRotator& p) { p={4,5,6};return true; }
}
namespace coop::element {
using ElementId=uint32_t;
inline constexpr ElementId kInvalidId=0xffffffff;
struct Registry { static Registry& Get(){static Registry r;return r;} ElementId EidForActor(void*){return 42;} };
namespace quiescence_drain { void SupersedePosCorrection(ElementId,const char*) { ++test::corrections; } }
}
namespace coop::net {
enum class Role { Host, Client };
struct Session {
    Role mode=Role::Host;
    std::vector<PropReleasePayload> sent;
    Role role() const { return mode; }
    void SetLocalPropPose(bool,const PropPoseSnapshot&) {}
    bool SendPropRelease(const PropReleasePayload& p) { sent.push_back(p);return true; }
};
}
namespace coop::players {
inline constexpr uint8_t kMaxPeers=32;
struct Registry { static Registry& Get(){static Registry r;return r;} uint8_t LocalPeerId(){return test::slot;} };
}
namespace coop::pvp {
uint16_t MintThrowNonce() { ++test::minted;return 0x1234; }
void NoteThrow(void* p,uint8_t slot,uint16_t nonce) { CHECK(slot==test::slot);++test::notes;test::noted=p;test::nonce=nonce; }
}
namespace coop::npc_sync { bool KerfurIsHolding(void*) { return test::robot; } }
namespace coop::trash_channel { int CtxForEid(coop::element::ElementId) { return test::trash?1:0; } }
namespace coop::prop_throw { uint16_t ThrowSeqFor(void*) { return test::seq; } }
namespace coop::prop_wire_parity { uint8_t PhysFlagsOf(void*) { return 5; } }
namespace coop::dev::swinger_drill { void NoteReleased(void*) {} }
namespace coop::prop_drive_host { void Coast(void* actor,const char*) { CHECK(actor==&test::actor);++test::coasts; } }

namespace coop::local_streams {
ue_wrap::CachedObjRef g_lastHeldProp, g_kerfurTakeActor;
coop::net::WireKey g_lastHeldKey{};
coop::element::ElementId g_lastHeldEid=42;
uint16_t g_holdGen=11;
bool g_heldSwinger=false,g_coasting=false;
#include "release_edge.cpp"
}

int main() {
    using namespace coop::local_streams;
    coop::net::Session session;
    auto reset=[&] {
        test::alive=true;test::robot=false;test::trash=false;test::velocityOk=true;
        test::speed=200;test::slot=31;test::minted=test::notes=test::coasts=test::corrections=0;
        test::noted=nullptr;test::nonce=0;test::seq=73;
        session.sent.clear();session.mode=coop::net::Role::Host;
        g_lastHeldProp.Set(&test::actor);g_kerfurTakeActor.Reset();g_lastHeldEid=42;g_holdGen=11;
    };
    reset();SendHeldRelease(session);
    CHECK(session.sent.size()==1);
    auto p=session.sent.back();
    CHECK(p.throwNonce[0]==0x34 && p.throwNonce[1]==0x12 && p.throwSeq==73);
    CHECK(test::notes==1 && test::noted==&test::actor && test::nonce==0x1234 && test::coasts==1);
    CHECK(p.holdGen==11 && p.elementId==42 && p.hasPose && p.locX==1 && p.physFlags==5);
    CHECK(!g_lastHeldProp.Raw());

    reset();ArmKerfurTakeRelease(&test::actor);SendHeldRelease(session);
    p=session.sent.back();CHECK(p._pad==1 && p.throwSeq==0 && !p.throwNonce[0] && !p.throwNonce[1]);
    CHECK(!test::notes && !test::minted && !test::coasts && !g_kerfurTakeActor.Raw());
    g_lastHeldProp.Set(&test::actor);SendHeldRelease(session);
    CHECK(session.sent.back()._pad==0 && test::notes==1);

    reset();test::robot=true;SendHeldRelease(session);
    CHECK(session.sent.back()._pad==1 && !test::notes && !test::coasts);
    reset();ArmKerfurTakeRelease(&test::other);SendHeldRelease(session);
    CHECK(session.sent.back()._pad==0 && test::notes==1 && !g_kerfurTakeActor.Raw());
    reset();ArmKerfurTakeRelease(&test::actor);++test::serial;g_lastHeldProp.Set(&test::actor);SendHeldRelease(session);
    CHECK(session.sent.back()._pad==0 && test::notes==1);
    reset();test::speed=150;SendHeldRelease(session);
    CHECK(!session.sent.back().throwNonce[0] && !test::notes);
    reset();test::speed=INFINITY;SendHeldRelease(session);CHECK(!test::notes);
    reset();test::velocityOk=false;SendHeldRelease(session);CHECK(!test::notes);
    reset();test::slot=32;SendHeldRelease(session);CHECK(!test::notes && !test::minted);
    reset();test::alive=false;SendHeldRelease(session);CHECK(!test::notes && !test::coasts);
    reset();test::trash=true;ArmKerfurTakeRelease(&test::actor);SendHeldRelease(session);
    CHECK(session.sent.empty() && !test::notes && !g_kerfurTakeActor.Raw());
    reset();session.mode=coop::net::Role::Client;SendHeldRelease(session);
    CHECK(test::notes==1 && !test::coasts && test::corrections==1);
    std::puts("PASS production release edge: nonce, native sequence, robot transfer, slot and actor lifetime");
}
