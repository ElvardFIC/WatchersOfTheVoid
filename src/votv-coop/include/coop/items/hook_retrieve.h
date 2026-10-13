#pragma once
#include <cstdint>
namespace coop::net { class Session; struct HookRetrievePayload; }
namespace coop::hook_retrieve {
void Install(coop::net::Session* session);
void Note(void* actor, bool knownAnchor = false);
void Tick();
void Receive(const coop::net::HookRetrievePayload& payload, uint8_t sender);
void SendAllToSlot(int slot);
void OnPeerLeftSlot(int slot);
void OnDisconnect();
}
