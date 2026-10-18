#include "../../src/votv-coop/src/coop/interactables/laptop_sync.cpp"

#include "coop/interactables/portable_pc_lid.h"
#include "coop/net/session.h"
#include "ue_wrap/devices/laptop.h"

#include <cstdio>
#include <cstdlib>
#include <thread>
#include <chrono>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

namespace {
ue_wrap::laptop::PowerState power;
int toggles = 0;
}

namespace ue_wrap::laptop {
bool EnsureResolved() { return true; }
void* Instance() { return &power; }
bool ReadPower(PowerState& out) { out = power; return true; }
bool CallPowerToggle() {
    ++toggles;
    power.isOpened = !power.isOpened;
    power.anim = false;
    return true;
}
}

namespace coop::portable_pc_lid {
void OnLid(const coop::net::LaptopStatePayload&, uint8_t) {}
}

int main() {
    coop::net::Session session;
    coop::laptop_sync::Install(&session);

    power = {true, true, true};
    coop::net::LaptopStatePayload targetOn{};
    targetOn.op = 0;
    targetOn.isOpened = 1;
    coop::laptop_sync::OnLaptopState(targetOn, 0);

    // A pending wire target survives a transient match during animation.
    coop::laptop_sync::Tick();
    CHECK(session.laptopStateSends == 0);

    // An intermediate state remains suppressed until the native animation settles.
    power.isOpened = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(260));
    coop::laptop_sync::Tick();
    CHECK(session.laptopStateSends == 0);

    // The eventual target application must also remain silent instead of echoing to the host.
    power.anim = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(260));
    coop::laptop_sync::Tick();
    CHECK(toggles == 1);
    CHECK(power.isOpened);
    CHECK(session.laptopStateSends == 0);

    coop::laptop_sync::OnDisconnect();
    coop::laptop_sync::Install(&session);
    power = {true, false, true};
    coop::net::LaptopStatePayload targetOff{};
    targetOff.op = 0;
    targetOff.isOpened = 0;
    coop::laptop_sync::OnLaptopState(targetOff, 0);
    coop::laptop_sync::Tick();
    CHECK(session.laptopStateSends == 0);

    power.isOpened = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(260));
    coop::laptop_sync::Tick();
    CHECK(session.laptopStateSends == 0);

    power.anim = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(260));
    coop::laptop_sync::Tick();
    CHECK(toggles == 2);
    CHECK(!power.isOpened);
    CHECK(session.laptopStateSends == 0);

    coop::laptop_sync::OnDisconnect();
    coop::laptop_sync::Install(&session);
    coop::laptop_sync::Tick();
    power.isOpened = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(260));
    coop::laptop_sync::Tick();
    CHECK(session.laptopStateSends == 1);

    coop::laptop_sync::OnDisconnect();
    std::puts("PASS laptop power target remains pending through native animation");
    return 0;
}
