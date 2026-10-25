// coop/session/save_replay_watch.h -- the client's view of its world's save replay: the by-name POST
// watch on the gamemode's `loadObjects` (the function that respawns the saved objects and places
// the player), the installer that registers it and reads its liveness, and the record of which
// gamemode ran it and when. world_load_episode owns the request that waits on it and asks here.

#pragma once

#include <chrono>
#include <string>

namespace coop::net { class Session; }

namespace coop::save_replay_watch {

// Register the watch and keep its liveness read, on a client; a no-op on the host. Called every
// session tick, acts at most once a second, and only while a request pends or the watch is not
// yet settled. Game thread.
void Drive(coop::net::Session& session, bool requestPending);

// True when the replay ran in the world the player stands in now; the world tested is returned
// through `world`, read once. Game thread.
bool ReturnedInThisWorld(void** world);

// True when the script gate refused the watch. Game thread.
bool Refused();

// True when the watch is registered and settled, and its function does not exist in this game
// build. Game thread.
bool Dead();

// The red arm that skips the replay wait; read once per process at the first Drive. Game thread.
bool GateBreak();

// True when a replay has been recorded since the last Reset. Game thread.
bool HasRecord();

// Milliseconds from the recorded replay's return to `t`; meaningful only with HasRecord. Game thread.
long long MsFromPostTo(std::chrono::steady_clock::time_point t);

// "<class>::<function>" of the registered row, for the failure text. Valid once Drive has run.
// Game thread.
std::string RowText();

// Session teardown: forget the replay record and the watch's liveness; the registration and a
// refusal last for the process. Game thread.
void Reset();

}  // namespace coop::save_replay_watch
