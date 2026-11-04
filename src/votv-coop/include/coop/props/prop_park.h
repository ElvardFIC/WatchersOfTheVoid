// coop/props/prop_park.h -- a prop a receiver parks: while another peer's hand or the host's drive moves it,
// its body stops simulating, and the receiver gives it back simulating. A Character welded onto a prop -- the
// Kerfus's navigation pawn is the one measured -- moves only while the prop's body does not simulate: its
// movement skips a tick whose capsule simulates, and a welded capsule answers with its weld parent's state. So a
// park wakes it. The Kerfus's pawn then falls, its capsule blocking nothing, and each step re-welds it lower: the
// prop's centre of mass falls with it, at 44 m/s on a copy, and the spin a throw gives at release swings the prop
// about that far point -- a thrown copy flew 1.26 km. A park therefore holds still the movement of each Character
// it wakes (rider_hold.h), and the give-back lets it go once the body simulates. MTA gives an attached ped no
// movement of its own; what carries it places it
// (reference/mtasa-blue/Client/mods/deathmatch/logic/CClientPed.cpp:3441-3442,
// reference/mtasa-blue/Client/mods/deathmatch/logic/CClientEntity.cpp:1183-1195). Game thread, every function.

#pragma once
#include <functional>

namespace coop::prop_park {

// A receiver takes `actor`'s physics: its body stops simulating (`mesh`, an Aprop_C's StaticMesh, or null for the
// actor's root, a clump's), and each Character that wakes is held still. The first park walks the prop for the
// Characters attached to it; a park of a parked prop, as a re-latch each tick, tests only those.
void Park(void* actor, void* mesh);
bool Park(void* actor, void* mesh, const std::function<bool()>& current);

// The receiver gives it back: its body simulates again when `rootSimulates` says so (false leaves it as it is, a
// stuck or frozen prop), then each hold its park took is given back.
void Unpark(void* actor, void* mesh, bool rootSimulates);
bool Unpark(void* actor, void* mesh, bool rootSimulates, const std::function<bool()>& current);

// The session ended: every hold a park still has is given back, and the parks are forgotten.
void OnDisconnect();

}  // namespace coop::prop_park
