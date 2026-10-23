#pragma once
namespace coop::net { class Session; struct AriralShipStatePayload; }
namespace coop::ariral_ship_sync {
// The blank-email arirShipAppear event: host phase, local native photo listener.
// Reuses map actors; never runs the host's gameplay trigger on a client.
void Install(net::Session*);
void Tick();
void Receive(const net::AriralShipStatePayload&, int sender);
void QueueConnectBroadcastForSlot(int);
void OnDisconnect();
}
