// ue_wrap/hotbar/icons.h -- the quick-slot bar and the two tables its icons are drawn from.
//
// `ui_UI_C::updateSlotInv` rebuilds the bar whole: it lists the player container's records,
// clears every slot image's material to the engine's black placeholder, then writes each icon
// from `lib_C::propToIcon`, which indexes a texture array on the game instance by a name table
// on `ApropProcessor_C`. The renderer publishes that pair seconds after the world is up, and
// the game's own post-load refresh runs first -- so the lookups return null and the black
// stands. docs/ui.md has the mechanism and what the mod adds.
//
// This file is the wrapper half: read the bar's inputs, read what a slot is showing, and
// re-issue the game's verb. Which world is owed a rebuild, and when, is a policy and lives in
// coop/items/hotbar_icon_edge.h.
//
// Engine-wrapper layer (principle 7): reflection and thunks only, no gameplay policy, no
// network and no coop state. Game thread.

#pragma once

#include <cstdint>
#include <string>

namespace ue_wrap::hotbar {

// The bar and its icon source as they stand right now. Every field is a count read straight off
// the live objects, so a caller can say WHICH input was missing rather than that one was.
struct State {
    void*   ui             = nullptr;  // ui_UI_C (mainGamemode.playerInterface)
    void*   renderer       = nullptr;  // ApropProcessor_C (mainGamemode.propRenderer)
    int32_t slots          = 0;        // ui_UI_C.slotInv    -- the ten slot images
    int32_t slotTexts      = 0;        // ui_UI_C.slotTexts  -- their amount labels
    int32_t iconNames      = 0;        // propProcessor.names          (propToIcon's index side)
    int32_t iconTextures   = 0;        // mainGameInstance.texs_GAMEINST (propToIcon's value side)
    int32_t validIconTextures = 0;     // populated entries, not null decoder results
    int32_t rendererPhases = -1;       // propProcessor.fins -- how far the render got
    int32_t carried        = -1;       // records in the live personal store, -1 if unreadable

    // Both halves agree and every texture decoded. A nonempty array of null
    // entries is the native empty-file cache failure, not published icons.
    bool IconsReady() const {
        return iconNames > 0 && iconNames == iconTextures && validIconTextures == iconTextures;
    }
};

// Read it. False (and `out` left alone) before the world is up or while the reflection has not
// resolved. Pure field reads, no UFunction dispatch -- safe from inside a Blueprint body.
bool Read(State& out);

// Before a non-Tutorial travel, reject empty disk icons and discard a cached
// array containing null textures so the next world's native renderer rebuilds.
// Tutorial keeps its native presentation. False asks the boot poll to retry.
bool PrepareForWorld(int mode);

// The texture bound to slot `i`'s `tex` material parameter, by name. Empty when the slot index is
// out of range or the widget is gone; `no-material` when the image was never touched by a
// rebuild; `no-parameter` when a material is bound but carries no `tex` override. A rebuild that
// found an icon leaves a texture name here; one that did not leaves the engine's `Black`, or
// `null` where the lookup wrote an empty result.
std::wstring SlotTexture(const State& s, int i);

// Rebuild the bar through the game's own `ui_UI_C::updateSlotInv`. Idempotent by construction
// (the verb rebuilds from the store every time). False if the verb or the widget is unavailable.
bool Rebuild(const State& s);

}  // namespace ue_wrap::hotbar
