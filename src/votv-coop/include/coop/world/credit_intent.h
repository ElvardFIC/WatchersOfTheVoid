// coop/world/credit_intent.h -- a client's point sack, treasure chest or achievement completion
// reaches the shared balance.
//
// The client names the artifact. The host runs the game's own credit for that artifact (the
// sack's action, the chest's gold branch, the achievement row's award) and will not credit the
// same one again. A raw point delta never travels. The host's balance poll then broadcasts the
// absolute total, which is what a late joiner already receives.

#pragma once

#include <cstdint>

namespace coop::net {
class Session;
struct CreditIntentPayload;
struct ChestFlagsPayload;
}

namespace coop::credit_intent {

void Install(coop::net::Session* session);
void Tick();
void OnIntent(const coop::net::CreditIntentPayload& payload, uint8_t senderSlot);
void ApplyFlags(const coop::net::ChestFlagsPayload& payload);
void SendChestsToSlot(int slot);
void OnDisconnect();

}  // namespace coop::credit_intent
