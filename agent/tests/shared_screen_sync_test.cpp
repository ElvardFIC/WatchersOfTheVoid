#include "../../src/votv-coop/src/coop/interactables/shared_screen_sync.cpp"
#include "../../src/votv-coop/src/coop/net/blob_chunks.cpp"
#include "../../src/votv-coop/src/coop/net/session_lanes.h"
#include <cassert>
#include <cstdio>
#include <functional>
namespace test {
namespace S = coop::shared_screen_sync;
namespace W = coop::shared_screen_wire;
uint32_t world = 1;
uint32_t serial = 1;
uint64_t claim = 1;
uint8_t holder = 1;
bool local = false, widgetReady = true, applyReady = true;
int applies = 0;
std::vector<ue_wrap::shared_screen::Row> visible;
std::function<void()> onRead, onApply;
std::vector<ue_wrap::shared_screen::Row> State(int tab, const std::wstring& text = L"mail", float scroll = 4) {
    ue_wrap::shared_screen::Row row;
    row.path = "pc/r/0"; row.flags = 7; row.tab = tab; row.text = text; row.scroll = scroll;
    return {row};
}
void Tick() { S::g_next = {}; S::Tick(); }
std::vector<uint8_t> Message(bool canonical, uint32_t serial, const std::vector<ue_wrap::shared_screen::Row>& rows,
                             uint32_t lease = 0, uint8_t owner = 0xfe) {
    return W::EncodeSnapshot(canonical, lease ? lease : S::g_lease, serial,
        owner == 0xfe ? S::g_holder : owner, W::Encode(rows));
}
void Deliver(coop::net::Session& pipe, const std::vector<uint8_t>& bytes, uint8_t sender) {
    pipe.sent.clear(); pipe.refuse = false; pipe.budget = -1; pipe.blocked = -1;
    assert(coop::blob_chunks::SendBlobToSlot(&pipe, 0, coop::net::ReliableKind::SharedScreen, ++S::g_sequence, bytes));
    const auto chunks = pipe.sent; pipe.sent.clear();
    for (const auto& entry : chunks) S::OnChunk(entry.second, sender);
}
W::Snapshot Sent(const std::vector<std::pair<int, coop::net::BlobChunkPayload>>& packets, int slot) {
    coop::blob_chunks::Assembler assembler;
    std::vector<uint8_t> bytes;
    W::Snapshot snapshot;
    for (const auto& entry : packets) if (entry.first == slot && assembler.OnChunk(entry.second, 0, bytes))
        assert(W::DecodeSnapshot(bytes, snapshot));
    assert(!snapshot.rows.empty()); return snapshot;
}
}
namespace ue_wrap::world_identity { uint32_t Generation() { return test::world; } }
namespace coop::net::session_serial { uint32_t Current() { return test::serial; } }
namespace ue_wrap::game_thread { bool IsGameThread() { return true; } }
namespace coop::device_occupancy {
uint8_t HolderOf(const wchar_t*) { return test::holder; }
bool LocalHolds(const wchar_t*) { return test::local; }
uint64_t LaptopClaimRevision() { return test::claim; }
}
namespace ue_wrap::shared_screen {
bool Read(std::vector<Row>& rows) {
    rows = test::visible;
    auto cb = std::move(test::onRead); test::onRead = {}; if (cb) cb();
    return test::widgetReady;
}
bool Apply(const std::vector<Row>& rows) {
    ++test::applies;
    auto cb = std::move(test::onApply); test::onApply = {}; if (cb) cb();
    if (!test::widgetReady || !test::applyReady) return false;
    test::visible = rows; return true;
}
}
namespace ue_wrap::log { void Write(Level, const char*, ...) {} }
int main() {
    using namespace coop; using namespace test;
    assert(net::LaneForKind(net::ReliableKind::SharedScreen) == net::LaneForKind(net::ReliableKind::DeviceClaim));
    net::Session session, pipe; session.ready[1] = session.ready[2] = true;
    visible = State(1); S::Install(&session); Tick();
    assert(!S::g_canonical.empty() && session.sent.size() == 2);
    const auto initialLease = S::g_lease;
    const auto initial = S::g_canonical;
    const auto first = Message(false, 1, State(3));
    Deliver(pipe, first, 2); assert(S::g_canonical == initial); // wrong holder
    session.refuse = true; Deliver(pipe, first, 1); Tick();
    assert(visible[0].tab == 3 && S::g_owed[2]);
    Deliver(pipe, Message(false, 2, State(4, L"new mail", 80)), 1);
    Deliver(pipe, first, 1); // completed stale or duplicated snapshots cannot roll back the UI
    session.refuse = false; session.sent.clear(); Tick();
    assert(visible[0].tab == 4 && Sent(session.sent, 2).rows[0].scroll == 80);
    const auto current = S::g_canonical;
    Deliver(pipe, Message(true, 3, State(8)), 1); assert(S::g_canonical == current);
    session.ready[1] = false; Deliver(pipe, Message(false, 3, State(8)), 1);
    assert(S::g_canonical == current); session.ready[1] = true;
    // Same slot releases/reclaims between polls: old commands have the wrong lease.
    ++claim; Tick(); assert(S::g_lease != initialLease);
    Deliver(pipe, first, 1); assert(S::g_canonical == current);
    Deliver(pipe, Message(false, 1, State(5)), 1); Tick(); assert(visible[0].tab == 5);
    session.sent.clear(); S::QueueConnectBroadcastForSlot(3); Tick(); assert(session.sent.empty());
    session.ready[3] = true; Tick(); assert(Sent(session.sent, 3).rows[0].tab == 5);
    // Reentrant teardown during a host multi-chunk broadcast must stop the old stream.
    visible = State(5, std::wstring(500, L'x')); holder = 0; local = true; ++claim; Tick();
    visible = State(6, std::wstring(500, L'y')); session.sent.clear();
    session.callback = [] { S::OnDisconnect(); }; Tick();
    assert(!S::g_session && S::g_canonical.empty() && session.sent.size() == 1);

    session.mode = net::Role::Client; session.sent.clear(); local = false; holder = 1;
    players::Registry::Get().local = 1; S::Install(&session);
    Deliver(pipe, Message(true, 10, State(7), 10, 1), 2); assert(S::g_target.empty());
    widgetReady = false; Deliver(pipe, Message(true, 10, State(7, L"host text", 125), 10, 1), 0);
    Tick(); assert(S::g_apply);
    // A newly local operator must inherit the host's complete screen before publishing.
    local = true; visible = State(0); session.sent.clear(); Tick(); assert(session.sent.empty());
    widgetReady = true; Tick();
    assert(!S::g_apply && visible[0].tab == 7 && visible[0].text == L"host text" && visible[0].scroll == 125);
    assert(!Sent(session.sent, 0).canonical);
    visible = State(8, L"new local text", 140);
    Deliver(pipe, Message(true, 11, State(7), 10, 1), 0); Tick();
    assert(visible[0].tab == 8 && visible[0].scroll == 140); // own echo never rewinds input
    // Final changes inside the 200 ms poll window enter the queue before release.
    visible = State(9, L"last edit", 150); session.refuse = true;
    assert(!S::BeforeLocalRelease()); session.refuse = false;
    assert(S::BeforeLocalRelease());
    assert(Sent(session.sent, 0).rows[0].text == L"last edit");
    local = false; Tick(); assert(visible[0].tab == 9 && visible[0].scroll == 150);
    // Fresh lease seeds the same client again. Older host revisions remain rejected.
    local = true; Deliver(pipe, Message(true, 12, State(3), 11, 1), 0); Tick();
    Deliver(pipe, Message(true, 11, State(1), 10, 1), 0); Tick(); assert(visible[0].tab == 3);
    visible = State(4, std::wstring(500, L'z')); session.sent.clear(); session.budget = 1;
    assert(!S::BeforeLocalRelease()); assert(session.sent.size() == 1);
    const auto partialSequence = session.sent.front().second.blobSeq;
    session.budget = -1; assert(S::BeforeLocalRelease());
    assert(session.sent.back().second.blobSeq != partialSequence && Sent(session.sent, 0).rows[0].tab == 4);
    // Native read reentry into another world cannot publish a captured old screen.
    session.sent.clear(); visible = State(5); onRead = [] { ++world; };
    Tick(); assert(session.sent.empty()); widgetReady = false; Tick();
    assert(S::g_target.empty() && S::g_sent.empty() && !S::g_apply);
    widgetReady = true; local = false;
    Deliver(pipe, Message(true, 20, State(6), 12, 2), 0);
    onApply = [&] { Deliver(pipe, Message(true, 21, State(7), 12, 2), 0); };
    Tick(); assert(S::g_apply); Tick(); assert(visible[0].tab == 7);
    ++serial; widgetReady = false; Tick();
    assert(S::g_target.empty() && S::g_revision == 0 && S::g_lease == 0);
    widgetReady = true; Deliver(pipe, Message(true, 1, State(8), 1, 2), 0); Tick();
    assert(visible[0].tab == 8); // reused Session address accepts the new host's first revision
    S::OnDisconnect(); const auto before = applies; Tick(); assert(applies == before);
    puts("PASS shared screen lease/holder trust, stale snapshots, final-exit retry, new-operator seed, partial sends, join, teardown, native reentry and world travel");
}
