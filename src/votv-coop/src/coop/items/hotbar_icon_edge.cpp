// coop/items/hotbar_icon_edge.cpp -- see coop/items/hotbar_icon_edge.h.

#include "coop/items/hotbar_icon_edge.h"

#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/hotbar/icons.h"
#include "ue_wrap/world/game_mode.h"

#include <string>

namespace coop::hotbar_icon_edge {
namespace {

namespace HB = ue_wrap::hotbar;

// Asked a few times a second until it latches, then never again for that world. Each ask is a
// handful of pointer chases and TArray headers; the material walk below runs only once both icon
// tables are up, which is the last thing to happen in the window this watches.
constexpr int kPollTicks = 15;   // ~8 Hz at ~120 Hz

// Does the bar contradict the store -- items carried, icons published, and the first slot still
// showing none? updateSlotInv fills from the end of the item list down to slot 0, so slot 0 is
// the one slot a rebuild always reaches when the bar lists anything; its emptiness therefore
// means the rebuild found no icons rather than that the bar is short.
//
// This asks about the OUTCOME rather than watching the tables go from empty to full, because the
// transition is not reliably observable: a world load runs as one long blocking call, and a run
// was measured in which the tables filled between the game's own refresh and this reader's first
// sample. A trigger armed on that edge would have missed it; a question about the bar is still
// true afterwards.
bool BuiltWithoutIcons(const HB::State& s) {
    if (!s.IconsReady() || s.carried <= 0 || s.slots <= 0) return false;
    const std::wstring tex = HB::SlotTexture(s, 0);
    return tex.empty() || tex == L"Black" || tex == L"null" || tex == L"no-parameter" || tex == L"no-material";
}

// Shape: MTA re-expresses what depends on an asset when the asset situation changes rather than
// assuming the dependants notice -- CClientGame::RestreamModel streams every entity of a model
// out so the streamer brings it back with the new art
// (reference/mtasa-blue/Client/mods/deathmatch/logic/CClientGame.cpp:6859). Deliberate divergence:
// MTA owns its streamer and can force the re-express directly, while the bar is the game's own
// widget, so the re-express here is the game's own rebuild verb called once.
//
// The world this edge has finished with. Held as a CachedObjRef rather than a bare pointer: the
// engine reuses object slots, so a new world's renderer can land on the freed address of the old
// one and a pointer comparison would silently disarm the edge for that world.
ue_wrap::CachedObjRef g_settled;

}  // namespace

void Tick() {
    static int s_ticks = 0;
    if (++s_ticks < kPollTicks) return;
    s_ticks = 0;
    if (ue_wrap::game_mode::ReadLocal() == ue_wrap::game_mode::kTutorial) return;

    // The latch is tested first, on the renderer this world is running, and only then is the full
    // state read: an edge that fires once per world must cost nothing for the rest of it.
    HB::State s;
    if (!HB::Read(s) || !s.renderer || !s.ui || s.slots <= 0 || s.carried < 0) return;
    if (g_settled.Alive() && g_settled.Get() == s.renderer) return;

    if (!BuiltWithoutIcons(s)) {
        // Nothing to do, and nothing that can go wrong later: once the icons are published, every
        // rebuild the game runs from here on finds them. Settle, so a world whose player is
        // carrying nothing does not keep asking for the rest of its life.
        if (s.IconsReady()) {
            g_settled.Set(s.renderer);
            // Said on the settle as well as on the rebuild: a gate that only speaks when it acts
            // cannot report that it RAN, and this one spent a release unreachable on the launch
            // every player makes while its own readout was inside the branch that never ran.
            UE_LOGI("hotbar: the quick-slot bar was built with its icons (names=%d texs=%d "
                    "carried=%d) -- nothing to rebuild", s.iconNames, s.iconTextures, s.carried);
        }
        return;
    }

    ue_wrap::CachedObjRef renderer;
    renderer.Set(s.renderer);
    const std::wstring before = HB::SlotTexture(s, 0);
    const bool ok = HB::Rebuild(s);
    HB::State after;
    const bool read = HB::Read(after);
    if (ok && read && renderer.Get() == s.renderer && after.renderer == s.renderer &&
        after.ui == s.ui && after.IconsReady() && !BuiltWithoutIcons(after))
        g_settled.Set(s.renderer);
    UE_LOGI("hotbar: the icon tables arrived after the bar was built (names=%d texs=%d phases=%d "
            "carried=%d) -- rebuilt through updateSlotInv: called=%d, slot0 %ls -> %ls",
            s.iconNames, s.iconTextures, s.rendererPhases, s.carried, ok ? 1 : 0, before.c_str(),
            HB::SlotTexture(after, 0).c_str());
}

}  // namespace coop::hotbar_icon_edge
