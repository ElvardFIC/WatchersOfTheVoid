#pragma once
namespace coop::net { class Session; }
namespace coop::props::notebook_refresh {
void Install(net::Session* session);
void Tick();
void OnDisconnect();
}
