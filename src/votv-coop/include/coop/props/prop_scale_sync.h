#pragma once
#include "coop/net/protocol.h"
namespace coop::net { class Session; }
namespace coop::prop_scale_sync {
void Reset();
void Tick(net::Session& session, void* heldActor, uint16_t holdGen);
void Receive(net::Session& session, const net::PropScaleStatePayload& state, int sender);
}
