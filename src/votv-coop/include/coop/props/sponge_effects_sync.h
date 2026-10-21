#pragma once
#include <cstdint>
namespace coop::net {class Session;struct SpongeEffectPayload;}
namespace coop::sponge_effects_sync {
void Install(net::Session* session);
void Tick();
void Receive(const net::SpongeEffectPayload& effect,uint8_t sender);
void OnPeerLeft(int slot);
void OnDisconnect();
}
