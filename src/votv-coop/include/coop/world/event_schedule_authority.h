#pragma once

namespace coop::net { class Session; }
namespace coop::event_schedule_authority {
// Hold the native schedule before EVERY saveSlot.settime body, including Blueprint
// calls during world loading. Time/day updates still run; only host rows may fire.
void Install(coop::net::Session* session);
void OnDisconnect();
}
