#pragma once

namespace coop::net { class Session; }

namespace coop::laptop_seat_trace {

void Install(coop::net::Session* session);
void Tick();
void OnDisconnect();

}  // namespace coop::laptop_seat_trace
