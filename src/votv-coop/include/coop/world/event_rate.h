#pragma once

namespace coop::net { class Session; }

namespace coop::event_rate {
// Game thread. Scale the supported repeatable world spawners' wait intervals;
// native eligibility, spawn rolls and creature simulation remain native.
void Install(net::Session* session);
void OnDisconnect();
}
