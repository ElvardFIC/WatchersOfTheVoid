#pragma once
#include "coop/net/protocol.h"
namespace coop::net { class Session; }
namespace coop::grayboar_turn {
// Host AnimBP turn -> cosmetic client bone steering and motor pitch only.
void Install(net::Session* session);
void Tick();
void Receive(const net::GrayboarTurnPayload& packet, int sender);
void OnDisconnect();
}
