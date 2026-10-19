#pragma once
#include "coop/net/protocol.h"
#include "ue_wrap/core/script_gate.h"
namespace coop::net { class Session; }
namespace coop::grayboar_actions {
void Install(net::Session* session);
// Paired observations around bite-overlap (1), detected (2), damaged (3).
// Wire actions 4..7 reproduce the native damage1..4 section selected on a hit.
void Before(const ue_wrap::script_gate::Call& call, uint8_t action);
void After(const ue_wrap::script_gate::Call& call, uint8_t action);
void Receive(const net::GrayboarActionPayload& packet, int sender);
void Tick();
void QueueConnect(int slot);
void OnPeerLeft(int slot);
void OnDisconnect();
}
