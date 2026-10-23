// coop/dev/drill_chat.h -- the lobby-chat lines two drill peers say to each other. A drill that sequences its peers on
// an edge the other one printed says a line no save can hold and reads the other's: the host says it is ready, the
// client says its start set is recorded. Nothing here ships enabled.

#pragma once

#include <string>

namespace coop::dev::drill_chat {

void Say(const std::string& line);

// True when a LIVE chat row holds `needle`. A row seeded at the join sits in the retained tier and never counts, so a
// peer acts on what the other said after it joined, never on the record of an earlier life.
bool ChatHasLive(const std::string& needle);

// A feed walk is a few times a second: true on every fifteenth call.
bool PollDue();

}  // namespace coop::dev::drill_chat
