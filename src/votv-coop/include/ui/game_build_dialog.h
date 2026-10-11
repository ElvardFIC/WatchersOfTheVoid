// ui/game_build_dialog.h -- the popup that names the game build when it is not the one this Multivoid was
// made for (ImGui overlay surface, the boot-warning modal's shape with two answers).
//
// Warn, never forbid: the mod keeps running whatever the answer. "Continue anyway" remembers this build
// (coop::game_build::Acknowledge), so the popup is asked once per game build, not per launch; "Check for an
// update" opens the Multivoid releases page and leaves the popup up. The text is composed when it is drawn,
// in the player's language: the boot thread arms the popup before the catalogue is loaded.
//
// Threading: Arm() from the boot thread (before the overlay ever presents); IsOpen(), Render() and Clear()
// on the render thread, as boot_warning_dialog's.

#pragma once

namespace ui::game_build_dialog {

// Show the popup when coop::game_build says an answer is owed for the running build. Boot thread, once.
void Arm();

// True while the popup waits for its answer. Cheap peek; render thread.
bool IsOpen();

// Draw the popup this frame. No-op when closed. Render thread only.
void Render();

// Drop it without an answer -- the overlay's SEH re-fault guard, as for every surface whose open flag would
// otherwise re-enter a faulted Render() every frame.
void Clear();

// Every variant of the text formats in the l10n grammar, so a variant no run draws (the older build's, while no
// older build is known) is still checked. Un-gated at boot; logs; true when every check passed.
bool RunSelftest();

}  // namespace ui::game_build_dialog
