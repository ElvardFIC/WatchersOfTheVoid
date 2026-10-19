#pragma once
namespace coop::net {class Session;}
namespace coop::prop_knock {
// Observe host physics contacts, then enroll displaced props before the drive
// publisher iterates its vector. Never mutate that vector in a native callback.
void Tick(net::Session& session);
void OnDisconnect();
}
