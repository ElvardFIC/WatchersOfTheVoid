#pragma once
namespace coop::net { class Session; }
namespace coop::bucket_sync {
void Install(net::Session* session);
void Tick();
void OnDisconnect();
// Narrow native cement-use/pouring provenance for the existing client birth lane.
bool IsMaterialBirth();
}
