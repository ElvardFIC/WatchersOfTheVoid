#pragma once
namespace coop::net { class Session; }
namespace coop::wall_build_sync {
// Deferred host material overlaps retain source/actor ownership. Retry only
// before native entry; an interrupted admitted body is never spent again by
// that queued attempt. Client paid repair remains a no-spend refusal.
void Install(net::Session* session);
void Tick();
void OnDisconnect();
}
