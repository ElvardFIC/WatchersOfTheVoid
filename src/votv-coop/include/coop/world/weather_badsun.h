// coop/world/weather_badsun.h -- the Bad Sun, its existence owned by the host.
//
// badSun_C is a world-event actor every peer expresses itself: a copy polls the shared clock and, at the first
// poll inside 07:00..18:00, shows its widget and damages its own player (ue_wrap/world/bad_sun), so the sky and the
// harm are peer-local by the game's own design. What the host owns is WHETHER a copy exists. A fresh one comes from
// the gamemode's `Spawn Bad Sun` (the day cycle's midnight roll, which no ProcessEvent hook sees, and the cheat
// menu) and the end from the copy's own remove(); the script gate sees both on every route. The HOST sends its state
// at the call that changes it, as MTA's server sends a weather change at SetWeather
// (reference/mtasa-blue/Server/mods/deathmatch/logic/CStaticFunctionDefinitions.cpp), and a joiner is seeded at its
// world-ready (coop/world/weather_sync), as MTA's join packet carries the weather
// (reference/mtasa-blue/Server/mods/deathmatch/logic/packets/CMapInfoPacket.cpp). A CLIENT refuses its own
// `Spawn Bad Sun` at the gate and applies the host's through the same verb, or destroys an inactive copy; an active
// copy ends by its own clock. The red sky's lane is the template (coop/world/weather_redsky).

#pragma once

namespace coop::net { class Session; struct BadSunPayload; }

namespace coop::weather_badsun {

// Cache the session and, once, watch `Spawn Bad Sun` and badSun_C's `remove` at the script gate. Called from
// weather_sync::Install on every re-entry.
void Install(coop::net::Session* session);

// Ask whether each watch settled and say, once each, whether it is live. Called from weather_sync::TickConnect;
// a single bool read once both are said.
void Tick();

// Whether the running world's gamemode holds a live Bad Sun: the seed a joiner's world-ready gets (principle 8).
// Game thread.
bool LocalBadSunPresent();

// The host's state on this client: spawn one through the gamemode's own verb when it has none, destroy its inactive
// copies when the host has none. Game thread.
void Apply(const coop::net::BadSunPayload& payload);

// The session ended: its counts are said and cleared.
void OnDisconnect();

}  // namespace coop::weather_badsun
