#include "coop/interactables/cord_state_cache.h"
#include "coop/props/drive_generation.h"
#include "ue_wrap/devices/cord_blueprint.h"
#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
    using namespace coop;
    net::CordStatePayload a{}; a.cord.kind = 3; a.cord.elementId = 1;
    std::strcpy(a.cord.name, "Cord"); a.socket.kind = 2; std::strcpy(a.socket.name, "Socket_1");
    a.powered = 1;
    auto b = a; b.cord.elementId = 2;
    auto differentCase = a; std::strcpy(differentCase.cord.name, "cord");
    std::strcpy(differentCase.socket.name, "socket_1");
    assert(cord_wire::Same(a.cord, differentCase.cord));
    assert(cord_wire::Same(a.socket, differentCase.socket));
    cord_wire::StateCache cache;
    assert(cache.Observe(a, 0)); assert(cache.Observe(b, 1));
    assert(cache.Rows().size() == 2);
    assert(cache.Rows()[0].p.socket.kind == 0 && cache.Rows()[0].p.powered == 0);
    assert(cache.Rows()[1].p.socket.kind == 2); // old A cannot reconnect over accepted B
    auto unplugA = a; unplugA.socket = {}; unplugA.powered = 0;
    assert(cache.Observe(unplugA, 2)); assert(cache.Rows()[1].p.socket.kind == 2);
    assert(cache.Observe(differentCase, 3)); assert(cache.Rows().size() == 2);
    assert(cache.Rows()[1].p.socket.kind == 0); // canonical switch back
    auto bad = a; bad.powered = 2; assert(!cache.Observe(bad, 60000));
    assert(cache.Rows().size() == 2); // malformed input cannot evict good state
    cache.Prune(60001); assert(cache.Rows().size() == 1);
    cache.Prune(60003); assert(cache.Rows().empty());
    for (uint32_t i = 1; i <= cache.kCapacity; ++i) {
        auto p = unplugA; p.cord.elementId = i; assert(cache.Observe(p, 10));
    }
    auto extra = unplugA; extra.cord.elementId = 1000;
    assert(!cache.Observe(extra, 20));
    assert(cache.Observe(unplugA, 20)); // a full cache can still refresh an existing row
    assert(cache.Observe(extra, 60010)); assert(cache.Rows().size() == 2);
    cache.Clear(); assert(cache.Rows().empty());

    for (unsigned prev = 0; prev < 256; ++prev) {
        for (unsigned delta = 0; delta < 256; ++delta) {
            const auto next = static_cast<uint8_t>(prev + delta);
            assert(drive_generation::After(next, static_cast<uint8_t>(prev)) == (delta > 0 && delta < 128));
            assert(drive_generation::AtLeast(next, static_cast<uint8_t>(prev)) == (delta < 128));
        }
        const auto next = drive_generation::Next(static_cast<uint8_t>(prev));
        assert(next != 0 && drive_generation::After(next, static_cast<uint8_t>(prev)));
    }
    namespace B = ue_wrap::cord::blueprint;
    assert(B::UserDisconnect(B::kAction) && B::UserDisconnect(B::kUsed));
    assert(!B::UserDisconnect(611) && !B::UserDisconnect(B::kDestroyed) && !B::UserDisconnect(-1));
    for (int entry : {B::kAction, B::kUsed, B::kDestroyed, 611, 2057, B::kReattachResume}) {
        assert(!B::StaleReattach(entry, false));
        assert(B::StaleReattach(entry, true) == (entry == B::kReattachResume));
    }
    std::puts("PASS: cord ownership/cache expiry, FName case, 65536 generation pairs, native entry policy");
}
