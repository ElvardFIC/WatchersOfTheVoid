// ue_wrap/engine/screen_state.h -- what covers this machine's view: the black screens and the blood-loss blurs on
// the viewport, and the view target. A census read now and then (after a revive), never per frame: it walks the
// object array twice. The camera fade is not read: no Blueprint of the game starts or sets one (StartCameraFade,
// SetManualCameraFade appear in none), and its FadeAmount / bEnableFading are not reflected in this engine build.

#pragma once

#include <string>

namespace ue_wrap::engine {

struct ScreenState {
    int blackLive = 0;      // live blackScreen_C widgets, on the viewport or detached
    int blackOnScreen = 0;  // of those, the ones on the viewport
    int blurOnScreen = 0;   // ui_bloodLossBlur_C widgets on the viewport
    std::wstring viewTarget;  // the local view target's class; empty when it could not be read
};

// Game thread.
ScreenState ReadScreenState();

}  // namespace ue_wrap::engine
