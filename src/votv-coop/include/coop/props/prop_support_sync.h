#pragma once
#include <cstdint>
namespace coop::net { class Session; struct PropSupportPayload; }
namespace coop::prop_support_sync {
void Tick(net::Session& session);
void OnIntent(net::Session& session, const net::PropSupportPayload& p, uint8_t slot);
void OnDisconnect();
}
