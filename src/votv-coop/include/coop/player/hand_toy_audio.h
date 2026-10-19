#pragma once
#include "coop/net/protocol.h"
namespace coop::net {class Session;}
namespace coop::hand_toy_audio {
void Install(net::Session* session);
void Tick();
void Receive(const net::HandToyAudioPayload& p,uint8_t sender);
void QueueConnect(int slot);
void OnPeerLeft(int slot);
void OnDisconnect();
}
