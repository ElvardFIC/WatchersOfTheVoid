#include <windows.h>
#include <vulkan/vulkan.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
std::mutex g_mu,gate;
std::condition_variable changed;
std::atomic<bool> g_stopped=false,down=false,destroyStarted=false;
thread_local void* t_viewport=nullptr;
bool presentEntered=false,finishPresent=false;
std::atomic<int> freed=0,destroyed=0,idled=0,noted=0;
VkSemaphore overlayWait=reinterpret_cast<VkSemaphore>(3);
struct FrameResult{VkSemaphore wait;uint8_t windows;};
struct SwapSet{} set;
struct Context {
 struct{uintptr_t vkDevice=1,vkQueue=2;}st;
 struct{PFN_vkQueueWaitIdle vkQueueWaitIdle;}fns;
} context;
Context& Ctx(){return context;}
template<class T>T As(uintptr_t value){return reinterpret_cast<T>(value);}
bool IsDown(){return down;}
FrameResult RunFrameGuarded(void*,VkQueue,const VkPresentInfoKHR*){return {overlayWait,1};}
void NotePresentResult(uint8_t,VkResult){++noted;}
SwapSet* FindSet(VkSwapchainKHR){return &set;}
void FreeSet(SwapSet&){++freed;}
void Check(VkResult result){CHECK(result==VK_SUCCESS);}
void MarkDown(const char*){down=true;}
VkResult VKAPI_PTR Idle(VkQueue){++idled;return VK_SUCCESS;}
VkResult Present(VkQueue,const VkPresentInfoKHR* info){
 CHECK(info->waitSemaphoreCount==1&&info->pWaitSemaphores&&*info->pWaitSemaphores==overlayWait);
 std::unique_lock lock(gate);presentEntered=true;changed.notify_all();
 CHECK(changed.wait_for(lock,std::chrono::seconds(5),[]{return finishPresent;}));
 CHECK(!freed&&!destroyed);return VK_SUCCESS;
}
void Destroy(VkDevice,VkSwapchainKHR,const VkAllocationCallbacks*){CHECK(freed==1);++destroyed;}
auto g_savedPresent=&Present;
auto g_savedDestroy=&Destroy;
#include "vulkan_present_lifetime.inc"
}
int main(){using namespace test;context.fns.vkQueueWaitIdle=&Idle;
 VkSwapchainKHR chain=reinterpret_cast<VkSwapchainKHR>(4);VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
 info.swapchainCount=1;info.pSwapchains=&chain;
 std::thread present([&]{t_viewport=&set;CHECK(SwappedPresent(reinterpret_cast<VkQueue>(2),&info)==VK_SUCCESS);});
 {std::unique_lock lock(gate);CHECK(changed.wait_for(lock,std::chrono::seconds(5),[]{return presentEntered;}));}
 std::thread destroy([&]{destroyStarted=true;SwappedDestroy(reinterpret_cast<VkDevice>(1),chain,nullptr);});
 while(!destroyStarted)std::this_thread::yield();
 std::this_thread::sleep_for(std::chrono::milliseconds(50));CHECK(!freed&&!destroyed);
 {std::lock_guard lock(gate);finishPresent=true;}changed.notify_all();
 present.join();destroy.join();CHECK(freed==1&&destroyed==1&&idled==0&&noted==1);
 puts("PASS UI07 actual Vulkan Present/Destroy: semaphore lifetime and serialized accounting; GPU quiescence is the engine caller contract");
}
