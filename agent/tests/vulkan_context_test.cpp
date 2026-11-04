#include "ue_wrap/engine/vulkan_rhi.h"
#include "../../src/votv-coop/src/ui/overlay_backend_vk_context.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main() {
    ue_wrap::vulkan_rhi::ViewportState base{};
    using ui::overlay_backend::vk::SameRenderContext;
    CHECK(SameRenderContext(base,base));
#define REPLACED(field) { auto next=base; ++next.field; CHECK(!SameRenderContext(base,next)); }
    REPLACED(viewport); REPLACED(device); REPLACED(rhi); REPLACED(presentQueue);
    REPLACED(hwnd); REPLACED(vkInstance); REPLACED(vkPhysicalDevice); REPLACED(vkDevice);
    REPLACED(vkQueue); REPLACED(vkSurface); REPLACED(queueFamily); REPLACED(queueIndex);
    auto resized=base;
    resized.swapChain=1; resized.vkSwapchain=2; resized.internalW=1920; resized.internalH=1080;
    resized.imageCount=3; resized.vkFormat=44; resized.pixelFormat=2;
    CHECK(SameRenderContext(base,resized));
    std::puts("PASS VI01: 12 context replacements refused; same-context swapchain recreation allowed");
}
