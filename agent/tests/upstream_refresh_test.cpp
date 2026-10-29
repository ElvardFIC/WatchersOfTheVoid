// Real packet builders, pause set and bind-wait controller; registry/config/native boundaries doubled.
#include "coop/net/protocol.h"
#include "coop/net/apply_pause.h"
#include "../../src/votv-coop/src/coop/net/apply_pause_selftest.cpp"
#include "../../src/votv-coop/src/coop/element/bind_wait.cpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <array>
#include "upstream_refresh_wire.inc"

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace ue_wrap::game_thread { bool IsDefinitelyOffGameThread() { return false; } }
namespace ue_wrap::hot_path {
unsigned long long CallerRva(const void*) { return 0; }
unsigned long CurrentThreadId() { return 1; }
}
namespace ue_wrap::reflection { bool IsLiveByIndex(void*,int32_t) { return false; } }
namespace coop::element {
Registry::Registry() = default;
Registry::~Registry() = default;
Registry& Registry::Get() { static Registry registry; return registry; }
Element* Registry::Get(ElementId) const { return nullptr; }
}
namespace test {
std::vector<std::pair<int,int>> calls;
std::function<void()> callback;
void Replay(const uint8_t* bytes,size_t n,int slot) {
    CHECK(n == 1); calls.emplace_back(bytes[0],slot);
    auto next=std::move(callback); callback={}; if(next) next();
}
}
int main() {
    namespace N=coop::net;
    namespace B=coop::element::bind_wait;
    using namespace test;
    CHECK(N::apply_pause::RunSelftest());
    CHECK(sizeof(N::ReliableHeader)==8 && offsetof(N::ReliableHeader,payloadLen)==4);
    std::array<uint8_t,256> wire{};
    const uint8_t payload[]{8,9,10};
    for (unsigned id : {1u,253u,254u,255u,257u,258u,259u,260u,261u,262u,263u}) {
        CHECK(N::BuildReliableWire_(wire.data(),static_cast<N::ReliableKind>(id),payload,3,7,9,31)==31);
        N::ReliableHeader header{}; std::memcpy(&header,wire.data()+sizeof(N::PacketHeader),sizeof(header));
        CHECK(header.kind==id && header.payloadLen==3 && header._pad[0]==0 && header._pad[1]==0);
        CHECK(!std::memcmp(wire.data()+28,payload,3));
    }
    std::vector<uint8_t> batch;
    std::atomic<uint32_t> sequence{20};
    const uint8_t values[]{1,2,3,4};
    CHECK(N::BuildReliableBatch_(batch,N::ReliableKind::CoffeeActionIntent,values,1,4,sequence,9)==29);
    CHECK(sequence==24 && batch.size()==116);
    for (int i=0;i<4;++i) {
        N::PacketHeader packet{}; N::ReliableHeader reliable{};
        std::memcpy(&packet,batch.data()+29*i,sizeof(packet));
        std::memcpy(&reliable,batch.data()+29*i+20,sizeof(reliable));
        CHECK(packet.seq==20u+i && reliable.kind==263 && batch[29*i+28]==values[i]);
    }
    B::Clear("fixture"); uint8_t value=1;
    CHECK(!B::Hold(100,257,31,Replay,&value,1,true));
    B::MarkDeferred(100,"fixture");
    CHECK(B::Hold(100,257,31,Replay,&value,1,true));
    value=2; CHECK(B::Hold(100,1,1,Replay,&value,1,true));
    value=3; CHECK(B::Hold(100,257,31,Replay,&value,1,true));
    B::OnBound(100); B::Pump();
    CHECK((calls==std::vector<std::pair<int,int>>{{3,31},{2,1}}));
    CHECK(B::CoalescedOf(257)==1 && !B::HasHolds());
    calls.clear(); B::MarkDeferred(101,"fixture"); value=4;
    CHECK(B::Hold(101,257,1,Replay,&value,1,true));
    CHECK(B::Hold(101,258,1,Replay,&value,1,true));
    callback=[] { B::Drop(101,258,"newer native state"); };
    B::OnBound(101); B::Pump(); CHECK(calls.size()==1 && B::DroppedOf(258)==1);
    calls.clear(); B::MarkDeferred(102,"fixture");
    CHECK(B::Hold(102,257,1,Replay,&value,1,true));
    CHECK(B::Hold(102,258,31,Replay,&value,1,true));
    B::OnSlotLeft(31); B::OnBound(102); B::Pump();
    CHECK(calls.size()==1 && calls.front().second==1);
    calls.clear(); B::MarkDeferred(103,"fixture");
    CHECK(B::HoldReplayOnGiveUp(103,257,0,Replay,&value,1));
    CHECK(B::Hold(103,258,0,Replay,&value,1,true));
    B::GiveUpReplayHeld(103,"fixture"); B::Pump(); CHECK(calls.size()==1);
    calls.clear(); B::MarkDeferred(104,"fixture");
    CHECK(B::Hold(104,257,0,Replay,&value,1,true));
    callback=[&] { B::MarkDeferred(105,"nested"); CHECK(B::Hold(105,257,0,Replay,&value,1,true)); B::OnBound(105); B::Pump(); };
    B::OnBound(104); B::Pump(); CHECK(calls.size()==1 && B::HasHolds());
    B::Pump(); CHECK(calls.size()==2 && !B::HasHolds());
    B::Clear("fixture done");
    std::puts("PASS actual reliable packet/batch builders: retained and extended IDs, header size, bytes and sequence");
    std::puts("PASS actual pause and bind-wait: high IDs, coalescing/order, slot 31, native supersession, give-up and nested pump");
}
