// coop/dev/laptop_drill.h -- [dev] the laptop's disc slot is the host's; a file edit lands on the disc it was made on.
//   CLIENT -- run: puts a disc holding "LAPTOP-DRILL<LF>A" in its laptop through the laptop's own insert, claims it and
//             appends "-B" in that frame, before the host can answer; ejects it; inserts a second disc
//             ("-E"). On the host's "-H" it appends "-C", which the host's eject races; once that eject is here, it
//             sends a batch appending "-D" on the ejected disc's generation. It passes when the host's canonical of the
//             ejected disc, on that generation too, is dropped and its laptop stays empty. join: its laptop holds the
//             host's disc ("-J") and the buffer row ("-K") only the quad's connect canonical carries.
//   HOST   -- run: reads A, B, the client's eject and E, then arms an eject ahead of the client's next batch and
//             appends H. It fails if C or D ever reaches its laptop, and once both are refused sends the ejected disc's
//             canonical on its old generation. join: puts J in its laptop and K in its buffer in the joiner's window.
// Every row carries a line break after LAPTOP-DRILL (B and K a CR LF), so a lane that strips one fails Has.
// Both log their final quad ("[LAPTOP-DRILL] quad"), which must agree. "[LAPTOP-DRILL] FAIL" is the lane failing a step
// (--fail-marker), "ABANDONED" the drill unable to do its part (--dead-marker). Run with laptop_drill=run|join and
// --done-marker "[LAPTOP-DRILL] client DONE".

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::laptop_drill {

// Game thread, once per pump tick; a latched read when off.
void Tick(coop::net::Session* session);

// The per-session half: a rejoin's peers run their legs again.
void OnDisconnect();

}  // namespace coop::dev::laptop_drill
