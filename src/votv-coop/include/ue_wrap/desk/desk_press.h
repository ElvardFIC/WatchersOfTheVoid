// ue_wrap/desk/desk_press.h -- a player's press on the main desk (analogDScreenTest_C), as the game makes it: the
// player's useSelectedAction runs the desk's lookAt(player, hit), which sets the panel under the player's eye, and
// then actionOptionIndex(player, hit, action, lookAtComponent), whose body takes the pressed button from the hit
// (CXXHeaderDump/analogDScreenTest.cpp:2355). A press made here replays that pair with a hit on the chosen component; a press the
// game makes is read back from its actionOptionIndex frame. Engine-wrapper layer (principle 7), no network or coop
// state. Game thread, every function. Implementation: src/ue_wrap/desk/desk_press.cpp.

#pragma once

#include <cstdint>

namespace ue_wrap::desk_press {

// The desk's component variable `member` (a button, as `button_downl_saveSig1`), or null when it does not read.
// `member` keys a cached offset, so it must have static lifetime (a literal).
void* Member(void* desk, const wchar_t* member);

// `player` presses `component` on `desk`: lookAt, then actionOptionIndex, each with a hit on the component. A look
// comes first because a press with a panel under the player's eye goes to that panel. True when both dispatched.
bool Press(void* desk, void* player, void* component);

// Press, made for a player other than this machine's own, which leaves that one and the desk's use as they were.
// A press writes the desk's player_using and lookingAtPanel (:2347, lookAt's first line), and every press that
// reaches the button compare chain clears the scroll-wheel bindings of the gamemode's mainPlayer (:2522-2525),
// which here is not the presser. All four are held aside across the press and put back after it, and a binding
// the press made on this machine's player meanwhile is dropped (`dropped` counts them). The `used` binding the
// press adds lands on `player`. That covers a button whose branch reaches this machine's player only through
// those lists, as the save family's five do; a scroll branch's resetScrollOnUseRelease (:2540) or a branch that
// enters that player into an interface is not undone here. True when the press dispatched.
bool PressForAnother(void* desk, void* player, void* component, int* dropped = nullptr);

// The deck's selected row, play_selectIndex: the savedSignals_0 index its export and send act on (:2652,
// :2698-2709). -1 when it does not read.
int32_t SelectedRow(void* desk);
bool SelectRow(void* desk, int32_t index);

// The component an actionOptionIndex call's frame took its press from: its `hit` parameter's component, read from
// `locals` (the call's parameters) as the body's BreakHitResult reads it. Null when it does not resolve.
void* Pressed(void* pressFunction, const uint8_t* locals);

}  // namespace ue_wrap::desk_press
