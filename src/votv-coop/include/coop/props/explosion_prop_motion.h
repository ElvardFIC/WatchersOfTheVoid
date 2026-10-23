#pragma once
namespace coop::net { class Session; }
namespace coop::explosion_prop_motion {
void Install(net::Session* session);
// Register only an accepted host-authoritative blast, including zero-damage ones.
void ObserveBlast(void* blast);
// The managed host blast's native overlap selected this prop. Remember it until
// that blast actually fires its delayed radial impulse, then enroll it in Tick.
void Observe(void* actor, void* blast);
void Tick();
void OnDisconnect();
}
