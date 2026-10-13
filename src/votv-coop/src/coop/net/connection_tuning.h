// coop/net/connection_tuning.h -- the transport parameters a seated connection runs under.
//
// One concept: everything the transport is TOLD about a connection once it is ours, as opposed to
// what the connection then does (session_status.cpp owns the slot's life) or what we send over it.
// The parameters live here because they are written at one moment, to one connection, through one
// API: the priority lanes the reliable kinds ride, the send buffer those lanes share, the rate the
// whole connection is paced at, and a zero nagle wait so a small reliable is not held to coalesce.
//
// It was extracted from session_status.cpp when that file crossed the 800-line soft cap, and the
// function it came from was called ConfigureLanesForPeer while also setting the buffer and the
// rate -- a name that had stopped describing its own body.

#pragma once

#include <cstdint>

namespace coop::net {

// The per-connection send buffer used when the net.sendbuf_kb knob is 0. Public because the
// headroom rule measures occupancy against the real size and must be handed the same number
// (SendAdmission::SetSendBufBytes).
inline constexpr int kDefaultSendBufBytes = 4 * 1024 * 1024;

// Tune a newly seated connection: lanes, then the send buffer, then the opening send rate. Called
// once per connection from the Connected edge, before any traffic that matters. A lane failure is
// logged and survivable (reliable sends collapse to lane 0, functional but unprioritised).
//
// `rateControlled` is the session's own resolved answer, passed in rather than re-read here. The
// ini is resolved live on every read, so a flag flipped after Session::Start would otherwise give
// the one combination that is wrong in both directions: this function writing an opening rung that
// the controller -- disabled at Start -- will never move again.
// `pinnedRateKbs` is the drill's fixed-rate override, 0 for none. It is PASSED rather than
// resolved here because the session already resolved it once, at Start, to decide whether the
// controller runs at all -- and `ResolveInt` re-reads the ini on every call. Resolving it a second
// time per connection let an ini edited mid-session pin one connection while the controller was
// still steering it: two writers of one rate, out of one knob read twice.
void TuneConnection(uint32_t hConn, bool rateControlled, long pinnedRateKbs);

}  // namespace coop::net
