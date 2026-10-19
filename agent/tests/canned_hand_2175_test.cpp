#define CANNED_HAND_TEST
#define main ExistingHandFireRegression
#include "hand_fire_2138_test.cpp"
#undef main

namespace can_test {
ue_wrap::canned_food::State owner{false, 2, 5}, mirror;
bool available = true, apply = true;
int applyCount = 0;
std::function<void()> callback;
}
namespace ue_wrap::canned_food {
bool Read(void* actor, State& out) {
    if (actor != test::held || !can_test::available) return false;
    out = can_test::owner; return true;
}
bool Apply(void*, const State& state) {
    ++can_test::applyCount;
    auto cb = std::move(can_test::callback); can_test::callback = {};
    if (cb) cb();
    if (!can_test::apply) return false;
    can_test::mirror = state; return true;
}
}
void PollOwner() { for (int i = 0; i < 30; ++i) Owner(); }
void Receive(std::vector<uint8_t> bytes, int sender = 1) {
    coop::net::Session::ReliableMessage m{bytes.data(), int(bytes.size()), sender};
    H::HandleHandItem(session, m);
}
int main() {
    using namespace can_test;
    Reset(); Owner();
    CHECK(H::g_hands[0].hasCan && H::g_hands[0].can.foods == 5);
    const auto initial = H::Generation(0);
    owner.opened = true; PollOwner();
    CHECK(H::Generation(0) > initial && H::g_hands[0].can.opened);
    auto opened = H::Generation(0);
    owner.foods = 4; PollOwner(); // same actor/class/name: the old change detector missed this
    CHECK(H::Generation(0) > opened && H::g_hands[0].can.foods == 4);
    const auto beforeMiss = H::Generation(0); available = false; PollOwner();
    CHECK(H::Generation(0) == beforeMiss && H::g_hands[0].hasCan);
    available = true;
    session.sendOk = false; owner.foods = 3; PollOwner(); CHECK(H::g_dirty[0]);
    owner.foods = 0; PollOwner(); session.sendOk = true; Owner();
    CHECK(!H::g_dirty[0] && H::g_hands[0].can.foods == 0);
    auto snapshot = H::g_hands[0];
    auto packet = H::BuildPayload(1, snapshot);
    auto max = snapshot; max.cls = max.name = max.item = std::wstring(63,L'x');
    max.can = {true,3,4095};
    auto maxPacket = H::BuildPayload(1,max);
    CHECK(maxPacket.size() == 224 && maxPacket.size() <= coop::net::kMaxReliablePayload);
    Reset(); Receive(maxPacket); CHECK(H::g_hands[1].can == max.can);
    Reset(); puppet.posed = false; Receive(packet);
    CHECK(H::g_hands[1].can.foods == 0 && H::g_hands[1].can.opened);
    H::TickMirrors(); CHECK(!H::MirrorActorForSlot(1));
    puppet.posed = true; H::TickMirrors();
    CHECK(H::MirrorActorForSlot(1) && mirror == snapshot.can);
    // Host relay preserves the state, and a joiner receives it with the same generation.
    CHECK(!session.sent.empty() && session.sent.back().data == packet);
    H::g_session = &session; session.sent.clear(); H::ReplayPeerStatesToSlot(2);
    CHECK(session.sent.size() == 1 && session.sent[0].data == packet);
    Reset(coop::net::Role::Client); self = 2; Receive(packet, 0); H::TickMirrors();
    CHECK(H::MirrorActorForSlot(1) && mirror == snapshot.can);
    // Stow/new hand generation rejects an old can update.
    Reset(); Receive(packet); H::TickMirrors();
    Hand(1, snapshot.generation + 1, false); Receive(packet); H::TickMirrors();
    CHECK(!H::MirrorActorForSlot(1));
    // Wrong sender, truncated trailer, fields without presence, can state on empty hands.
    Reset(); Receive(packet, 2); CHECK(H::g_hands[1].generation == 0);
    for (size_t n = 0; n < packet.size(); ++n) {
        Reset(); Receive({packet.begin(), packet.begin() + n});
        CHECK(H::g_hands[1].generation == 0);
    }
    for (int field = 0; field < 2; ++field) {
        Reset(); auto bad = packet;
        if (field == 0) bad[bad.size() - 2] &= ~1u;
        else { H::SlotHand empty; empty.generation = 1; bad = H::BuildPayload(1, empty); bad[bad.size() - 2] = 1; }
        Receive(bad); CHECK(H::g_hands[1].generation == 0);
    }
    Reset(); apply = false; Receive(packet); H::TickMirrors();
    CHECK(!H::MirrorActorForSlot(1)); auto attempts = applyCount;
    H::TickMirrors(); CHECK(applyCount == attempts);
    apply = true; H::g_mirrors[1].retryAt = 0; H::TickMirrors();
    CHECK(H::MirrorActorForSlot(1));
    Reset(); can_test::callback = [] { H::Reset(); }; Receive(packet); H::TickMirrors();
    CHECK(!H::MirrorActorForSlot(1));
    std::puts("PASS held can owner changes, retries, relay/join, mirror application, stow and malformed input");
}
