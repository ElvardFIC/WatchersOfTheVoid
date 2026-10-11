// coop/dev/join_abort_drill.h -- [dev] the proof of A92's session-end seam.
// A client presses the loading screen's Cancel mid-join (join_progress::RequestCancel), the
// abort routes through harness::session_runtime::StopClientSession, and the drill judges that the
// session's end tore the coop state down:
//   - arm 'download' (window A, before the pawn): no per-player apply blob survives the abort, so an
//     aborted join's inventory cannot bleed into the next join. Run with the host's
//     stall_world_stream_at_pct so the transfer cannot finish under the press. No rejoin leg.
//   - arm 'world' (window B, after this client announced world-ready): the world-ready announce state
//     is cleared after the stop, then the drill rejoins and passes at the second world-ready. Run with
//     the host's hold_snapshot_sec so the announced-world stretch stays open.
//
// The press rides TickFrameTail (which runs in BOTH the menu-mode transfer wait, via PostMenuTick,
// and the play loop, so it covers either window whichever loop is active) -- OnFrameTail. The judge
// and the rejoin dial ride the play loop's no-session branch -- OnNoSession -- which runs only after
// the stop site returned, so after StopClientSession's synchronous teardown, on the seam's bytes. Both
// run on the game thread; the drill's state is its own, outside dev_lanes::EndSession, so a rejoin does
// not reset it. Client only, off unless config join_abort is download|world.
//
// The drill uses no symbol the seam adds, so it runs RED on the commit before the seam (the apply blob
// survives; the announce stays stale) and GREEN after.
#pragma once

namespace coop::dev::join_abort_drill {

// Press Cancel at the arm's moment, and (world arm) note the second join's world-ready as the rejoin
// pass. From harness::pump::TickFrameTail -- the transfer wait and the play loop alike. Game thread.
void OnFrameTail();

// Judge the abort, and (world arm) dial the rejoin. From the play loop's no-session branch, after the
// stop. Game thread.
void OnNoSession();

}  // namespace coop::dev::join_abort_drill
