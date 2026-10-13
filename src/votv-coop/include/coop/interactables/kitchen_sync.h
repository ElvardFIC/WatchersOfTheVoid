#pragma once
#include <cstdint>
namespace coop::net { class Session; struct KitchenStatePayload; struct KitchenIntentPayload; }
namespace coop::kitchen_sync {
void Install(net::Session* session);
void Tick();
void Apply(const net::KitchenStatePayload& p);
void OnIntent(const net::KitchenIntentPayload& p, uint8_t senderSlot);
void SendAllToSlot(int slot);
void OnDisconnect();
}
