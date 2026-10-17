// coop/dev/badsun_drill_shared.h -- what the Bad Sun drill's files share: the arm the config names, the one verdict
// line, the join's end, and each arm file's entry points. Dev only; game thread. A private header of
// coop/dev/badsun_drill*.cpp (src tree, not include/).

#pragma once

#include <chrono>
#include <cstdarg>

namespace coop::net { class Session; }

namespace coop::dev::badsun_drill::detail {

using Steady = std::chrono::steady_clock;

enum class Arm { Off, ClockArm };

// The configured arm, read once.
Arm ArmOf();

bool IsHost(coop::net::Session* s);
const char* RoleName(bool host);

// The one verdict line: the drill tag, the arm, the role, the word, then the reason; PASS at info, the rest at warning.
void VerdictV(const char* arm, bool host, const char* word, const char* fmt, va_list ap);

// Whether the join is over on this peer. The host: a client's slot is world-ready and its snapshot bracket closed
// (that slot is kept in `slot`). A client: its world-ready announced and its join phases over.
bool JoinOver(coop::net::Session* s, bool host, int& slot);

// The `clock` arm (coop/dev/badsun_drill_clock.cpp): one tick of it, and its state back to the first phase.
void ClockTick(coop::net::Session* s, bool host);
void ClockReset();

}  // namespace coop::dev::badsun_drill::detail
