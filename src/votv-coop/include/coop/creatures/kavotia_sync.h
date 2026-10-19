#pragma once

namespace coop::net { class Session; struct KavotiaStatePayload; }

namespace coop::kavotia_sync {
void Install(net::Session* session);
void Receive(const net::KavotiaStatePayload& packet, int sender);
void Tick();
void QueueConnect(int slot);
void OnPeerLeft(int slot);
void OnDisconnect();
}
