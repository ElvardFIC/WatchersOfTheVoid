#pragma once
#include <cstdint>
namespace coop::net { class Session; struct RadioStatePayload; }
namespace coop::radio_sync {
void Install(net::Session* session);
void Tick();
void Receive(const net::RadioStatePayload& p,uint8_t sender);
void OnDisconnect();
}
