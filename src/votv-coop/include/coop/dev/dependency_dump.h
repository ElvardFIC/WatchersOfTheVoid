// coop/dev/dependency_dump.h -- [dev] dependency_dump: what the mod has registered on the game, as text.
//
// At a readiness milestone (coop/session/rig_ready.h: `joined` on a client, `peer-world-ready` on the
// host), every watch of ue_wrap/core/script_gate, every patch of ue_wrap/core/ufunction_hook and every
// observer and interceptor of ue_wrap/core/game_thread is written, sorted and counted, to
// multivoid-dependency-dump-<milestone>.txt beside the exe: the record two builds are diffed by when a
// change must leave the registered set exactly as it was. Off by default; a dev instrument.

#pragma once

namespace coop::dev::dependency_dump {

// Called with every milestone the rig is told; writes on the two above when the flag is on. Game thread.
void OnMilestone(const char* milestone);

}  // namespace coop::dev::dependency_dump
