// coop/player/local_body.h -- the LOCAL player's body skin (first-person immersion).
//
// The local pawn's visible body -- torso and legs when looking down, and the mirror reflection
// -- must wear the SAME skin the player's puppet wears on other peers; otherwise a player sees
// the stock scientist's torso under their own camera while everyone else sees their chosen
// skin. This module is the single owner of the LOCAL skin choice:

#pragma once

#include <string>

namespace coop::net { class Session; }

namespace coop::local_body {

// Boot wiring (harness bring-up thread, before the net pump sends a Join):
// the persisted choice from multivoid.ini player_skin=.
void SetInitialSkin(const std::string& name);

// The local skin name (game thread -- the Join builder / puppet baseline path).
const std::string& LocalSkinName();

// Render-thread-safe snapshot for the F1 browser highlight.
std::string LocalSkinNameCopy();

// The pristine kel USkeletalMesh of the CURRENT pawn generation (captured
// before any swap). Null until the local pawn is dressed. Game thread.
void* NativeBodyMesh();

// F1 browser pick (render thread): validate, persist to ini, post the local
// apply to the game thread, announce SkinChange to the session (if connected).
void RequestSkin(const std::string& name);

// net_pump wiring (subsystems tick site). Install stores the session pointer
// (cheap per-tick re-store, the chat_sync shape); Tick applies/converges.
void Install(coop::net::Session* session);
void Tick();

// F1 > Cosmetics > Playermodel. Local only: this machine's own body stays visible
// when an event hides it. Other players' view of your puppet is unchanged.
bool AlwaysSeePlayermodel();
void SetAlwaysSeePlayermodel(bool on);

// Re-show the local body if the setting is on. The game hides it again during an
// event, so the pump calls this at the end of the tick. No-op when the setting is off.
void KeepPlayermodelVisible();

}  // namespace coop::local_body
