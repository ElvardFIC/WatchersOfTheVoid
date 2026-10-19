#include "../../src/votv-coop/src/coop/interactables/laptop_ui_sync.cpp"
#include "../../src/votv-coop/src/coop/net/blob_chunks.cpp"
#include "../../src/votv-coop/src/coop/items/save_record_wire.cpp"
#include "../../src/votv-coop/src/coop/interactables/signal_wire.cpp"
#include "ue_wrap/core/log.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
namespace test {
uint32_t world = 1;
uint64_t claim = 1;
uint8_t holder = 0xff;
bool held = false, ready = true, applyOK = true;
int applies = 0;
ue_wrap::laptop_ui::State visible;
std::function<void()> onApply, onValid;
}
namespace ue_wrap::world_identity { uint32_t Generation() { return test::world; } }
namespace ue_wrap::game_thread { bool IsGameThread() { return true; } }
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace ue_wrap::laptop_ui {
bool Valid(const State& s) {
    auto cb=std::move(test::onValid); test::onValid={}; if(cb) cb();
    if (s.tabs[0]<0||s.tabs[0]>11||s.tabs[1]<0||s.tabs[1]>1||s.tabs[2]<0||s.tabs[2]>1||s.cart.size()>50) return false;
    for(const auto& row:s.cart) if(row!=L"apple"&&row!=L"cheese"&&row.size()!=128) return false;
    return true;
}
bool Read(State& out) { if(!test::ready) return false;out=test::visible;return true; }
bool Apply(const State& s) {
    ++test::applies; auto cb=std::move(test::onApply);test::onApply={};if(cb)cb();
    if(!test::ready||!test::applyOK||!Valid(s))return false;test::visible=s;return true;
}
}
namespace coop::device_occupancy {
bool LocalHolds(const wchar_t*) { return test::held; }
uint8_t HolderOf(const wchar_t*) { return test::holder; }
uint64_t LaptopClaimRevision() { return test::claim; }
}
namespace test {
namespace S=coop::laptop_ui_sync;
void Tick() { S::g_poll=0; S::Tick(); }
void Deliver(coop::net::Session& pipe,const std::vector<uint8_t>& bytes,int from) {
    pipe.sent.clear(); pipe.blocked=-1;pipe.budget=-1;
    CHECK(coop::blob_chunks::SendBlobToSlot(&pipe,0,coop::net::ReliableKind::LaptopUI,++S::g_blob,bytes));
    const auto chunks=pipe.sent;pipe.sent.clear();
    for(const auto& p:chunks)S::OnChunk(p.chunk,from);
}
}
int main() {
    using namespace test; using coop::net::Role;
    coop::net::Session s, pipe; S::Install(&s);
    visible.tabs={3,0,0};visible.cart={L"apple",L"apple"};Tick();
    CHECK(S::g_have && S::g_state==visible && s.sent.size()==3);
    const auto original=S::g_state; holder=1;++claim;Tick();CHECK(S::g_owner==1&&S::g_lease==2);
    auto state=original;state.tabs={2,1,0};state.cart={L"cheese"};
    const auto intent=S::Encode(false,1,state);Deliver(pipe,intent,2);CHECK(S::g_state==original);
    Deliver(pipe,intent,1);CHECK(S::g_state==state&&S::g_apply);Tick();CHECK(visible==state);
    auto newer=state;newer.tabs[0]=4;
    Deliver(pipe,S::Encode(false,2,newer),1);Deliver(pipe,intent,1);CHECK(S::g_state==newer);
    auto invalid=newer;invalid.cart={L"forged"};Deliver(pipe,S::Encode(false,3,invalid),1);CHECK(S::g_state==newer);
    auto malformed=S::Encode(false,3,state);malformed.push_back(0);Deliver(pipe,malformed,1);CHECK(S::g_state==newer);
    ++claim;Tick();CHECK(S::g_owner==1&&S::g_lease==3);Deliver(pipe,intent,1);CHECK(S::g_state==newer); // same slot, new claim
    auto fresh=S::Encode(false,1,state);Deliver(pipe,fresh,1);CHECK(S::g_state==state);
    applyOK=false;Tick();CHECK(S::g_apply&&!S::g_dirty[2]);applyOK=true;Tick();
    onValid=[] { holder=2;++claim; };Deliver(pipe,S::Encode(false,2,newer),1);CHECK(S::g_state==state);
    s.blocked=1;Tick();CHECK(S::g_dirty[1]&&!S::g_dirty[2]&&!S::g_dirty[3]);
    s.blocked=-1;Tick();CHECK(!S::g_dirty[1]);
    s.sent.clear();S::QueueConnectBroadcastForSlot(2);Tick();CHECK(s.sent.size()==1&&s.sent[0].slot==2);
    S::OnPeerLeft(2);CHECK(!S::g_dirty[2]);
    // Full-size cart spans chunks. A failed prefix must never count as a sent snapshot.
    S::OnDisconnect();s.mode=Role::Client;coop::players::Registry::Get().local=1;held=true;holder=1;
    S::Install(&s);S::g_lease=10;S::g_owner=1;S::g_revision=20;S::g_have=true;
    visible.tabs={3,1,1};visible.cart.assign(50,std::wstring(128,L'x'));
    s.sent.clear();s.budget=1;Tick();CHECK(!S::g_haveSent&&s.sent.size()==1);
    const auto failed=s.sent.front().chunk.blobSeq;s.budget=-1;Tick();CHECK(S::g_haveSent&&s.sent.back().chunk.blobSeq!=failed);
    auto local=visible;local.tabs[0]=6;visible=local;
    // An echo for this lease acknowledges the past, never erases newer local clicks.
    Deliver(pipe,S::Encode(true,21,state),0);CHECK(!S::g_apply);Tick();CHECK(visible==local);
    s.blocked=0;visible.cart.clear();CHECK(!S::BeforeLocalRelease());
    s.blocked=-1;CHECK(S::BeforeLocalRelease()&&S::g_sent.cart.empty());
    // A fresh lease seeds the next operator even if that operator is this client again.
    S::g_lease=11;auto seed=S::Encode(true,22,state);S::g_lease=10;
    ready=false;Deliver(pipe,seed,0);Tick();CHECK(S::g_apply);
    ready=true;Tick();CHECK(!S::g_apply&&visible==state);
    // A new canonical arriving from inside an engine call survives the old apply.
    held=false;S::g_apply=true;onApply=[] { ++S::g_revision;S::g_state.tabs[0]=7;S::g_apply=true; };
    Tick();CHECK(S::g_apply);Tick();CHECK(!S::g_apply&&visible.tabs[0]==7);
    const auto revision=S::g_revision;Deliver(pipe,S::Encode(true,revision+1,state),2);CHECK(S::g_revision==revision);
    auto truncated=seed;truncated.resize(12);Deliver(pipe,truncated,0);CHECK(S::g_revision==revision);
    S::g_apply=true;++world;ready=false;Tick();CHECK(!S::g_have&&!S::g_apply&&S::g_revision==0);
    S::OnDisconnect();CHECK(!S::g_session&&S::g_assembler.Idle());
    puts("PASS laptop UI: production chunks, holder/lease, stale/replayed/malformed edits, new operator seed, local echo, final-exit retry, per-peer backpressure, join, reentrancy and travel");
}
