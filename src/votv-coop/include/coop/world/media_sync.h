// coop/world/media_sync.h -- TVs and file players. The host reads each MediaPlayer and the
// others open the same url and catch up when the clock drifts. Game thread.

#pragma once

#include <cstdint>

namespace coop::net {
class Session;
struct MediaStatePayload;
}

namespace coop::media_sync {

void Install(coop::net::Session* session);
void Tick();
void OnReliable(const coop::net::MediaStatePayload& payload);
void OnDisconnect();

}  // namespace coop::media_sync
