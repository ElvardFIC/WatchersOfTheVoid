#pragma once
#include "coop/net/protocol.h"
namespace coop::net { class Session; }
namespace coop::emote_sync {
void Install(net::Session* session);
void Tick();
void Receive(const net::EmoteStatePayload& state, uint8_t sender);
void OnPeerLeft(int slot);
void OnDisconnect();
enum class HandDrive { Fallback, Applied, Retired };
HandDrive DriveHand(void* pawn, void* mirror);
}
