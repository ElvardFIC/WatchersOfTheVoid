#pragma once
namespace coop::net {class Session;}
namespace coop::event_location_sync {
void Tick(net::Session& session);
void OnDisconnect();
// These rows arm a location; their scheduler/menu call must not gather yet.
bool IsLocationRow(const char* name);
}
