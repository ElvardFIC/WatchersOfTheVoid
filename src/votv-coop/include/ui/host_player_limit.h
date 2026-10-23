#pragma once

namespace ui::host_player_limit {

bool Build(void* row, void* buttonDonor);
bool HandleClick();
int Value();
// Shared by native and overlay hosting; persisted only on an actual choice.
// Value/Select are safe on either UI thread; widget functions are game-thread only.
void Select(int players,bool remember=true);
void Remember();
void Refresh();
void ClearWidgets();

}  // namespace ui::host_player_limit
