#pragma once
#include "coop/net/protocol.h"
namespace coop::net { class Session; }
namespace coop::event_fire_sync::delivery {
void Install(net::Session* session);
void Publish(const net::EventFirePayload& payload);
void Clear();
}
