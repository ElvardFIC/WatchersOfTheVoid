#include "coop/net/peer_limits.h"
#include "coop/net/protocol.h"
#include "coop/net/session_config.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

int main() {
    using namespace coop::net;
    std::array<bool, kPeerCapacity> occupied{}, tearingDown{};
    auto free = [&](int slot) { return !occupied[slot] && !tearingDown[slot]; };
    for (int players = 2; players <= 32; ++players) {
        occupied = {}; tearingDown = {};
        for (int next = 1; next < players; ++next) {
            CHECK(FindAvailableClientSeat(players, free) == next);
            occupied[next] = true;
        }
        CHECK(FindAvailableClientSeat(players, free) == -1);
        occupied[1] = false; tearingDown[1] = true;
        CHECK(FindAvailableClientSeat(players, free) == -1);
        tearingDown[1] = false;
        CHECK(FindAvailableClientSeat(players, free) == 1);
    }
    CHECK(ClampPlayerLimit(-1) == 2 && ClampPlayerLimit(100) == 32);
    CHECK(Config{}.playerLimit == 4);
    CHECK(kAllClientPeers & (PeerMask{1} << 31));
    CHECK(!(kAllClientPeers & 1));
    // The largest canonical still fits the existing 256-byte transport and preserves
    // every generator row while carrying an acknowledgement for the final seat.
    static_assert(sizeof(PowerGridPayload) == kMaxReliablePayload);
    PowerGridPayload grid{};
    grid.count = kPowerGridGenerators;
    grid.ack[31] = 65535;
    grid.rows[3].cycle = 87;
    std::array<uint8_t, kMaxReliablePayload> wire{};
    std::memcpy(wire.data(), &grid, sizeof(grid));
    PowerGridPayload received{};
    std::memcpy(&received, wire.data(), sizeof(received));
    CHECK(received.ack[31] == 65535 && received.rows[3].cycle == 87);
    ServicePanelPayload panel{}; panel.ack[31] = 1234;
    RadioStatePayload radio{}; radio.ack[31] = 1235;
    PowerPanelPayload power{}; power.ack[31] = 1236;
    CoordTowerPayload tower{}; tower.ack[31] = 1237; tower.refused[31] = 1238;
    CHECK(panel.ack[31] == 1234 && radio.ack[31] == 1235 && power.ack[31] == 1236);
    CHECK(tower.ack[31] == 1237 && tower.refused[31] == 1238);
    std::puts("PASS all 2-32 seat limits, full/reuse/teardown admission, slot 31 acknowledgements and datagram bounds");
}
