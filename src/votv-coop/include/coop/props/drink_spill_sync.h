#pragma once
#include <string>
#include <cstdint>
namespace coop::net {class Session;struct DrinkSpillPayload;struct DrinkLaunchPayload;struct DrinkPourPayload;}
namespace coop::drink_spill_sync {
void Install(net::Session* session);
void Tick();
void Receive(const net::DrinkSpillPayload& packet,uint8_t sender);
void ReceiveLaunch(const net::DrinkLaunchPayload& packet,uint8_t sender);
void ReceivePour(const net::DrinkPourPayload& packet,uint8_t sender);
void SendAllToSlot(int slot);
void OnPeerLeft(int slot);
void OnDisconnect();
// Dynamic identities are separate from the static grime position minimum.
std::wstring KeyFor(void* actor);
// Resolve only this session's live canonical dynamic decal, never a position twin.
void* FindByKey(const std::wstring& key);
bool ExcludeFromStaticIndex(void* actor);
void ObserveExisting(void* actor);
void IndexComplete(uint32_t world);
void OnRemoved(void* actor);
}
