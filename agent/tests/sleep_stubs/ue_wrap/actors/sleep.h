#pragma once
namespace ue_wrap::sleep {
inline bool sleeping = false;
inline bool policyAvailable = true;
inline float probability = -1.f, need = 10.f, dilation = 1.f;
inline int wakeCalls = 0;
inline float needAtWake = 0.f;
inline bool EnsureResolved() { return true; }
inline bool IsSleeping() { return sleeping; }
inline bool SetDreamProbability(float value) {
    if (!policyAvailable) return false;
    probability = value;
    return true;
}
inline bool SetGlobalTimeDilation(float value) { dilation = value; return true; }
inline bool ReadSleepNeed(float& out) { out = need; return true; }
inline bool WriteSleepNeed(float value) { need = value; return true; }
inline bool CallWakeup() { ++wakeCalls; needAtWake = need; sleeping = false; dilation = 1.f; return true; }
inline void* SleepCam() { return nullptr; }
inline void* SleepingPawn() { return nullptr; }
inline bool SetSleepViewTarget(void*) { return true; }
}
