// Pin retained merge identities and exercise the real version/authority/lane boundaries.
#include "coop/net/protocol.h"
#include "../../src/votv-coop/src/coop/net/client_authorship.h"
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace coop::net;
static_assert(static_cast<uint8_t>(ReliableKind::PropScaleState)==202);
static_assert(static_cast<uint8_t>(ReliableKind::SharedScreen)==203);
static_assert(static_cast<uint8_t>(ReliableKind::LaptopUI)==204);
static_assert(static_cast<uint8_t>(ReliableKind::KavotiaState)==223);
static_assert(static_cast<uint8_t>(ReliableKind::PropPlacementResult)==224);
static_assert(static_cast<uint8_t>(ReliableKind::Watoy)==235);
static_assert(sizeof(KavotiaStatePayload)==25);
static_assert(AuthorshipOf(ReliableKind::KavotiaState)==ClientAuthorship::Host);
static_assert(AuthorshipOf(ReliableKind::Watoy)==ClientAuthorship::Client);
static_assert(sizeof(PropDropIntentPayload)==188 && offsetof(PropDropIntentPayload,requestId)==184);
static_assert(sizeof(PropPlacementResultPayload)==101 && offsetof(PropPlacementResultPayload,code)==100);
static_assert(AuthorshipOf(ReliableKind::PropPlacementResult)==ClientAuthorship::Host);
static_assert(AuthorshipOf(ReliableKind::PropDropIntent)==ClientAuthorship::Client);
static_assert(!IsClientRelayableReliableKind(ReliableKind::PropPlacementResult));
int main(){
    CHECK(LaneForKind(ReliableKind::PropPlacementResult)==LaneForKind(ReliableKind::PropSpawn));
    CHECK(LaneForKind(ReliableKind::PropDropIntent)==LaneForKind(ReliableKind::PropDestroy));
    CHECK(LaneForKind(ReliableKind::ReelEjectIntent)==LaneForKind(ReliableKind::PropDropIntent));
    CHECK(!IsPreWorldSendableKind(ReliableKind::PropPlacementResult));
    PacketHeader h{};WriteHeader(h,MsgType::Reliable,19,23,1);
    MsgType type{};uint32_t seq=0,epoch=0;uint8_t slot=0;
    CHECK(ParseHeader(&h,sizeof(h),type,seq,epoch,slot)&&seq==19&&epoch==23&&slot==1);
    --h.version;CHECK(!ParseHeader(&h,sizeof(h),type,seq,epoch,slot));
    CHECK(PeekProtocolVersion(&h,sizeof(h))==kProtocolVersion-1);
    CHECK(!ParseHeader(&h,sizeof(h)-1,type,seq,epoch,slot));
    std::puts("PASS merged ABI: retained 202/203/204, placement layout/host authority/ordered lanes and incompatible-version refusal");
}
