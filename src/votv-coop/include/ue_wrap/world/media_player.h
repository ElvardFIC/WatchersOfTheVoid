// ue_wrap/world/media_player.h -- the engine's MediaPlayer (a TV, a video, a music file).
// The ATV's own radio player is not one of these: world media must not publish it or open a
// url on it. No coop state. Game thread. A class that has not loaded is an empty read.

#pragma once

#include <cstdint>
#include <string>

namespace ue_wrap::media_player {

struct Playing {
    std::wstring url;
    int64_t ticks = 0;  // FTimespan, 100 ns
    bool playing = false;
};

// Up to `max` live players that are playing a url, the ATV's own radio left out. A base TV
// playing the same url is still written. Returns how many were written.
int CopyPlaying(Playing* out, int max);

// Open `url` on a player that already has it, or on a free one that is not an ATV radio, and seek
// when the clock is off. `playing` false pauses the one that has this url.
bool Apply(const wchar_t* url, int64_t ticks, bool playing);

// The player's own url and whether it is playing. Empty / false when `player` is null.
std::wstring Url(void* player);
bool IsPlaying(void* player);

// Open `url` on THIS player when it is not already the source, then play or pause. A path with no
// "://" is opened as a file. False when the player is null or the source would not open.
bool Drive(void* player, const wchar_t* url, bool playing);

}  // namespace ue_wrap::media_player
