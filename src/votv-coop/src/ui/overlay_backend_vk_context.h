#pragma once

namespace ui::overlay_backend::vk {
// Swapchain/extent/format changes can rebuild resources. A different device or
// window cannot reuse the old dispatch table, fences, or platform backend.
template<class State>
bool SameRenderContext(const State& a, const State& b) {
    return a.viewport == b.viewport && a.device == b.device && a.rhi == b.rhi &&
        a.presentQueue == b.presentQueue && a.hwnd == b.hwnd &&
        a.vkInstance == b.vkInstance && a.vkPhysicalDevice == b.vkPhysicalDevice &&
        a.vkDevice == b.vkDevice && a.vkQueue == b.vkQueue && a.vkSurface == b.vkSurface &&
        a.queueFamily == b.queueFamily && a.queueIndex == b.queueIndex;
}
} // namespace ui::overlay_backend::vk
