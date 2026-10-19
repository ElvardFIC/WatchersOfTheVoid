#pragma once

namespace ue_wrap::laptop {

struct PowerState {
    bool powered = false;
    bool isOpened = false;
    bool anim = false;
};

bool EnsureResolved();
void* Instance();
bool ReadPower(PowerState& out);
bool CallPowerToggle();

}  // namespace ue_wrap::laptop
