#pragma once
namespace coop::net { class Session; }
namespace coop::wall_build_sync {
void Install(net::Session* session);
void Tick();
void OnDisconnect();
}
