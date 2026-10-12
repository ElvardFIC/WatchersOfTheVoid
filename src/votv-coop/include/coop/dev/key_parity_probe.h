// coop/dev/key_parity_probe.h -- [dev] whether a prop key index fed only at the game's key writes can be
// complete, and how far the index we keep today trails the world. A shadow index is fed by script-gate
// POST watches on every init, setKey, getKey and loadData body run on a keyed prop (the four bodies after
// which a prop's key can have changed); every 2 s a walk of the object array takes the live keyed props as
// they are and counts, against each index, the props it lacks and the props it holds under another key,
// with the first examples of each per world. Armed by key_parity_probe=1; it holds the gate for the process
// so a load outside a session is seen too; read-only otherwise (it assigns slot serials). Game thread.
#pragma once

namespace coop::dev::key_parity_probe {

// The frame tail's entry (harness::pump::TickFrameTail): one latched flag read when off.
void Tick();

}  // namespace coop::dev::key_parity_probe
