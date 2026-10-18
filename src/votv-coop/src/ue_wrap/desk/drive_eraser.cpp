// ue_wrap/desk/drive_eraser.cpp -- see ue_wrap/desk/drive_eraser.h.

#include "ue_wrap/desk/drive_eraser.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/component_calls.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine_component.h"
#include "ue_wrap/world/world_singleton.h"

namespace ue_wrap::drive_eraser {
namespace {

namespace E = ue_wrap::engine;
namespace R = ue_wrap::reflection;

// Offsets on the eraser's class, resolved once the class is seen (a level singleton: one class a process).
void* g_cls = nullptr;
int32_t g_offProcessing = -1, g_offButton = -1, g_offBegin = -1, g_offDone = -1, g_offDeny = -1, g_offWiper = -1;

bool Resolve(void* eraser) {
    void* cls = eraser ? R::ClassOf(eraser) : nullptr;
    if (!cls) return false;
    if (cls != g_cls) {
        g_cls = cls;
        g_offProcessing = R::FindPropertyOffset(cls, L"processing");
        g_offButton = R::FindPropertyOffset(cls, L"audio_button");
        g_offBegin = R::FindPropertyOffset(cls, L"audio_begin");
        g_offDone = R::FindPropertyOffset(cls, L"audio_done");
        g_offDeny = R::FindPropertyOffset(cls, L"audio_deny");
        g_offWiper = R::FindPropertyOffset(cls, L"driverwiperDynamic");
    }
    return true;
}

void* Component(void* eraser, int32_t off) {
    if (off < 0) return nullptr;
    void* comp = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(eraser) + off);
    return comp && R::IsLive(comp) ? comp : nullptr;
}

// USkeletalMeshComponent::SetPlayRate(Rate): the wiper's animation speed.
bool SetPlayRate(void* mesh, float rate) {
    void* fn = mesh ? R::FindDispatchFunctionCached(R::ClassOf(mesh), L"SetPlayRate") : nullptr;
    ue_wrap::ParamFrame f(fn);
    return fn && f.valid() && f.Set<float>(L"Rate", rate) && ue_wrap::Call(mesh, f);
}

}  // namespace

void* Instance() { return ue_wrap::world_singleton::Find(kClassName); }

bool PressDelete(void* eraser, void* player) {
    void* fn = eraser ? R::FindDispatchFunctionCached(R::ClassOf(eraser), L"actionOptionIndex") : nullptr;
    ue_wrap::ParamFrame f(fn);
    return fn && f.valid() && f.Set<void*>(L"player", player) && f.Set<uint8_t>(L"action", kActionDelete) &&
           ue_wrap::Call(eraser, f);
}

bool ReadProcessing(void* eraser, bool& out) {
    if (!Resolve(eraser) || g_offProcessing < 0) return false;
    out = *(static_cast<const uint8_t*>(eraser) + g_offProcessing) != 0;
    return true;
}

bool Present(void* eraser, Show what) {
    if (!Resolve(eraser)) return false;
    switch (what) {
    case Show::Click:
        return E::PlayAudioComponent(Component(eraser, g_offButton));
    case Show::Start: {
        const bool button = E::PlayAudioComponent(Component(eraser, g_offButton));
        const bool wiper = SetPlayRate(Component(eraser, g_offWiper), 2.f);
        return E::PlayAudioComponent(Component(eraser, g_offBegin)) && button && wiper;
    }
    case Show::Done: {
        const bool done = ue_wrap::component_calls::Activate(Component(eraser, g_offDone));
        return SetPlayRate(Component(eraser, g_offWiper), 0.1f) && done;
    }
    case Show::Deny:
        return E::PlayAudioComponent(Component(eraser, g_offDeny));
    case Show::Refused: {
        const bool button = E::PlayAudioComponent(Component(eraser, g_offButton));
        return E::PlayAudioComponent(Component(eraser, g_offDeny)) && button;
    }
    }
    return false;
}

}  // namespace ue_wrap::drive_eraser
