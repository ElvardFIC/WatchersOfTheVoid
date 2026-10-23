// coop/props/prop_food_sync.h -- host-authoritative temperature, ripeness and use count for
// world food and food held in a player's hand.
//
// The record lane cannot carry these: loadData reads the sender's clock stamp as elapsed time
// (prop_food_state.h, food_clock_probe). The rate is not constant, so an anchor does not fit.
// The host's tick is the one author. A client's food tick is refused, and the host's values are
// written onto the client's actor. Clients may run local eat feedback, but the host's use count is
// canonical and is broadcast immediately after either a host bite or an accepted client bite.
// Food inside a pocket or a container is a stored record and is not touched here. Held food uses
// the existing FoodState packet's elementId as a generation-scoped hand address.

#pragma once

#include <cstdint>
#include <cmath>
#include <string>

namespace coop::net {
class Session;
struct FoodStatePayload;
struct FoodBitePayload;
}

namespace coop::props::prop_food_sync {

void Install(coop::net::Session* session);
void Tick();
void Apply(const coop::net::FoodStatePayload& payload);
void OnHandState(const coop::net::FoodStatePayload& payload, uint8_t senderSlot);
void OnBite(const coop::net::FoodBitePayload& payload, uint8_t senderSlot);
// The final native bite is authored before its generic destruction can cross.
bool ConsumesClientDestroy(void* actor);
// Only an active native eaten invocation may author its verified empty wrapper.
bool IsLocalConsumedOutput(void* actor,void* callerFunction);
void SendAllToSlot(int slot);
void OnPeerLeft(int slot);
void OnDisconnect();

// No save timestamps: transfers between peers carry only the live condition.
inline bool ValidCondition(float temperature, float ripeness, int32_t uses,float cookProgress=-1.f) {
    return std::isfinite(temperature) && temperature >= 0.f && temperature <= 2000.f &&
           std::isfinite(ripeness) && ripeness >= -100.f && ripeness <= 10000.f &&
           uses >= -10 && uses <= 100000 && std::isfinite(cookProgress) &&
           (cookProgress==-1.f || (cookProgress>=0 && cookProgress<=1'000'000.f));
}
void ForgetPublished(const std::wstring& key);
// Called after a host birth/snapshot row, on the same ordered lane as that row.
bool PublishSpawnState(coop::net::Session& session, void* actor, int slot);

}  // namespace coop::props::prop_food_sync
