#include "../../src/votv-coop/src/coop/interactables/laptop_sync.cpp"
namespace coop::net {void RefusalTally::Clear(){}}
namespace coop::roster_ledger {void RegisterPerSlotClear(PerSlotClearFn,void*){}}
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

namespace {
ue_wrap::laptop::PowerState power;
bool available = true;
int toggles = 0, lids = 0;
void Poll() { coop::laptop_sync::g_nextPoll = 0; coop::laptop_sync::Tick(); }
void Reset(coop::net::Session& session, bool host = false) {
    coop::laptop_sync::OnDisconnect();
    session = {};
    session.currentRole = host ? coop::net::Role::Host : coop::net::Role::Client;
    session.ready.fill(true);
    power = {true, false, false};
    available = true;
    toggles = lids = 0;
    coop::laptop_sync::Install(&session);
}
coop::net::LaptopStatePayload State(bool opened, uint8_t op = 0) {
    coop::net::LaptopStatePayload p{}; p.op = op; p.isOpened = opened; return p;
}
}
namespace ue_wrap::laptop {
bool EnsureResolved() { return available; }
void* Instance() { return available ? &power : nullptr; }
bool ReadPower(PowerState& out) { if (!available) return false; out = power; return true; }
bool CallPowerToggle() { ++toggles; power.isOpened = !power.isOpened; return true; }
}
namespace coop::portable_pc_lid {
void OnLid(const coop::net::LaptopStatePayload&, uint8_t) { ++lids; }
}

int main() {
    using namespace coop::laptop_sync;
    coop::net::Session session;
    Reset(session);
    available = false;
    OnLaptopState(State(true, 3), 0);
    Poll();
    CHECK(toggles == 0);
    available = true;
    Poll(); Poll();
    CHECK(power.isOpened && toggles == 1 && session.sent.empty());

    // Only the latest unresolved target survives, including a request for OFF.
    Reset(session);
    available = false;
    OnLaptopState(State(true), 0);
    OnLaptopState(State(false), 0);
    available = true;
    Poll(); Poll();
    CHECK(!power.isOpened && toggles == 0 && session.sent.empty());

    // The retained target also waits for wall power rather than forcing a native toggle.
    Reset(session); available = false;
    OnLaptopState(State(true, 3), 0);
    available = true; power.powered = false;
    Poll(); Poll(); CHECK(!power.isOpened && toggles == 0);
    power.powered = true; Poll(); Poll();
    CHECK(power.isOpened && toggles == 1 && session.sent.empty());

    // Reliable enqueue refusal must not lose a local edge. A newer edge replaces it.
    Reset(session); Poll();
    session.sendOk[0] = false;
    power.isOpened = true; Poll();
    CHECK(session.sent.empty());
    session.sendOk[0] = true; Poll();
    CHECK(session.sent.size() == 1 && session.sent.back().payload.isOpened == 1);
    session.sent.clear(); session.sendOk[0] = false;
    power.isOpened = false; Poll();
    power.isOpened = true; Poll();
    session.sendOk[0] = true; Poll(); Poll();
    CHECK(session.sent.size() == 1 && session.sent.back().payload.isOpened == 1);

    // A newer wire target cancels an unsent local state, so it cannot undo the target.
    Reset(session); Poll(); session.sendOk[0] = false;
    power.isOpened = true; Poll();
    OnLaptopState(State(false), 0);
    session.sendOk[0] = true; Poll(); Poll();
    CHECK(!power.isOpened && session.sent.empty());

    // Join before actor creation, then enqueue failure, then delivery of settled state.
    Reset(session, true); available = false;
    QueueConnectBroadcastForSlot(2); Poll();
    available = true; power.isOpened = true; power.anim = true;
    Poll(); CHECK(session.sent.empty());
    power.anim = false; session.sendOk[2] = false; Poll();
    CHECK(session.sent.empty());
    session.sendOk[2] = true; Poll(); Poll();
    CHECK(session.sent.size() == 1 && session.sent[0].slot == 2);
    CHECK(session.sent[0].payload.op == 3 && session.sent[0].payload.isOpened == 1);

    // Relay is owed even while the host's actor is unavailable; only failed peers retry.
    Reset(session, true); available = false; session.sendOk[2] = false;
    OnLaptopState(State(true), 1);
    CHECK(session.sent.size() == 1 && session.sent[0].slot == 3);
    session.sendOk[2] = true; Poll();
    CHECK(session.sent.size() == 2 && session.sent[1].slot == 2);
    CHECK(session.sent[1].origin == 1);
    available = true; Poll(); Poll();
    CHECK(power.isOpened && session.sent.size() == 2);

    // A departing slot must not deliver old state to its next occupant.
    Reset(session, true); available = false;
    QueueConnectBroadcastForSlot(2);
    OnPeerLeft(2);
    available = true; Poll();
    CHECK(session.sent.empty());
    session.ready[1] = session.ready[3] = false;
    session.sendOk[2] = false;
    power.isOpened = true; Poll();
    OnPeerLeft(2);
    session.sendOk[2] = true; Poll();
    CHECK(session.sent.empty());
    QueueConnectBroadcastForSlot(2);
    CHECK(session.sent.size() == 1 && session.sent[0].payload.isOpened == 1);

    // A queued join waits for readiness; invalid slots cannot address the retry array.
    Reset(session, true); session.ready.fill(false);
    QueueConnectBroadcastForSlot(2); Poll();
    QueueConnectBroadcastForSlot(-1); QueueConnectBroadcastForSlot(4);
    OnPeerLeft(-1); OnPeerLeft(4);
    CHECK(session.sent.empty());
    power.isOpened = true; Poll(); // refresh the owed seed before the peer becomes ready
    session.ready[2] = true; Poll();
    CHECK(session.sent.size() == 1 && session.sent[0].slot == 2 && session.sent[0].payload.isOpened == 1);

    // Disconnect clears both deferred inbound application and outgoing work.
    Reset(session, true); available = false;
    QueueConnectBroadcastForSlot(2);
    OnLaptopState(State(true), 1);
    OnDisconnect(); session.sent.clear();
    available = true; Poll(); Poll();
    CHECK(!power.isOpened && session.sent.empty());

    // Portable-lid routing remains independent of stationary-PC availability.
    Reset(session); available = false;
    OnLaptopState(State(true, 6), 0);
    CHECK(lids == 1 && toggles == 0);
    std::puts("PASS laptop deferred target, join seed, delivery retries and latest state");
}
