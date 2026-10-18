#include <cassert>
#include <cstdio>
#include "../../src/votv-coop/src/coop/player/sleep_sync.cpp"

namespace SS = coop::sleep_sync;
namespace SLP = ue_wrap::sleep;
using namespace coop::net;

void Reset(Session& session, Role role) {
    SS::OnDisconnect();
    session = {};
    session.mode = role;
    SLP::sleeping = false;
    SLP::policyAvailable = true;
    SLP::need = 10.f;
    SLP::probability = -1.f;
    SLP::dilation = 1.f;
    SLP::wakeCalls = 0;
    SS::Install(&session);
    SS::Tick();
}

int main() {
    Session s;
    // Both roles keep waiting sleepers at normal speed, with no nightmare roll.
    for (Role role : {Role::Client, Role::Host}) {
        Reset(s, role);
        s.ready[1] = true;
        SLP::sleeping = true;
        SS::Tick();
        assert(!SS::InAcceleratePhase());
        assert(SLP::probability == 0.f && SLP::dilation == 1.f);
    }
    // Client may use its own bed's chance during the shared phase.
    Reset(s, Role::Client);
    SLP::sleeping = true;
    SS::Tick();
    SS::OnReliable({2, 0, 2, 2}, 1); // A peer cannot start the phase.
    assert(!SS::InAcceleratePhase());
    SLP::policyAvailable = false;
    SS::OnReliable({2, 0, 2, 2}, 0);
    assert(SS::InAcceleratePhase());
    assert(SLP::probability == 0.f); // Failed policy update must retry.
    SLP::policyAvailable = true;
    SS::Tick();
    assert(SLP::probability == -1.f && SLP::dilation == 20.f);
    SLP::need = 100.f;
    SS::Tick();
    assert(SLP::need == 98.f); // Preserve sole natural-end authority.
    // Native personal nightmare wakes this player; report only the falling edge.
    s.sent.clear();
    SLP::sleeping = false;
    SS::Tick();
    assert(s.sent.size() == 1 && s.sent[0].op == 0 && s.sent[0].flag == 0);
    SS::OnReliable({3, 0, 0, 2}, 0);
    assert(!SS::InAcceleratePhase() && SLP::probability == 0.f);
    // Legacy broadcasts never enter a dream or wake a rested/awake recipient.
    const int wakes = SLP::wakeCalls;
    SS::OnDreamScene({}, 0);
    SS::Tick();
    assert(SLP::wakeCalls == wakes && !SLP::sleeping);
    // A client nightmare ends the host's timelapse without granting full rest.
    Reset(s, Role::Host);
    s.ready[1] = true;
    SLP::sleeping = true;
    SS::Tick();
    SS::OnReliable({0, 1, 0, 0}, 1);
    assert(SS::InAcceleratePhase() && SLP::probability == -1.f);
    s.sent.clear();
    SS::OnReliable({0, 0, 0, 0}, 1);
    assert(!SS::InAcceleratePhase() && !SLP::sleeping && SLP::need == 10.f);
    assert(s.sent.size() == 1 && s.sent[0].op == 3 && s.sent[0].flag == 0);
    // Natural end retains wake-before-refill ordering (native gift threshold).
    Reset(s, Role::Client);
    SLP::sleeping = true;
    SS::Tick();
    SS::OnReliable({2, 0, 2, 2}, 0);
    SLP::need = 98.f;
    SS::OnReliable({3, 1, 0, 2}, 0);
    assert(SLP::needAtWake == 98.f && SLP::need == 100.f);
    SS::OnDisconnect();
    assert(SLP::probability == -1.f && !SS::InAcceleratePhase());
    puts("PASS personal nightmare policy, client interruption, legacy rejection and natural rest");
}
