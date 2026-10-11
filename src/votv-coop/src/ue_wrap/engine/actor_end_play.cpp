// ue_wrap/engine/actor_end_play.cpp -- see actor_end_play.h.

#include "ue_wrap/engine/actor_end_play.h"

#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/hook.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/scan_ledger.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/core/sig_scan.h"

#include <windows.h>  // SEH (__try/__except) -- the firewall around the sinks

#include <atomic>

namespace ue_wrap::actor_end_play {
namespace {

namespace GT = ue_wrap::game_thread;
namespace prof = ue_wrap::profile;

// void AActor::EndPlay(const EEndPlayReason::Type Reason): this in RCX, the reason in EDX.
using EndPlayFn = void(__fastcall*)(void* actor, uint32_t reason);
EndPlayFn g_trampoline = nullptr;
std::uintptr_t g_target = 0;           // the patched AActor::EndPlay, set by Patch

std::atomic<bool> g_installed{false};  // armed: the patched function is the one Actor's vtable names

// The sinks: a slot is written once with release and read with acquire.
std::atomic<Sink> g_sinks[kMaxSinks] = {};
std::atomic<int> g_sinkCount{0};

std::atomic<unsigned long long> g_seen{0};
std::atomic<unsigned long long> g_offGameThread{0};

// A sink that faults loses this end of play, never the engine's: the fault is absorbed here, inside
// an SEH-only frame (no C++ objects to unwind, so the __try is legal), and said the first few times.
void RunSinksSEH(void* actor, Reason reason) {
    __try {
        const int n = g_sinkCount.load(std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
            if (Sink s = g_sinks[i].load(std::memory_order_acquire)) s(actor, reason);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static std::atomic<int> s_said{0};
        if (s_said.fetch_add(1, std::memory_order_relaxed) < 5)
            UE_LOGE("actor_end_play: a sink FAULTED (exception 0x%08lX) -- the sinks after it missed this "
                    "end of play", GetExceptionCode());
    }
}

// Not inlined, so the guard below stays a bare test and two tail jumps.
__declspec(noinline) void __fastcall EndPlayDetour(void* actor, uint32_t reason) {
    if (actor && HasBegunPlay(actor)) {
        g_seen.fetch_add(1, std::memory_order_relaxed);
        if (GT::IsDefinitelyOffGameThread())
            g_offGameThread.fetch_add(1, std::memory_order_relaxed);
        else
            RunSinksSEH(actor, static_cast<Reason>(reason));
    }
    g_trampoline(actor, reason);
}

// Until the arm, EndPlay runs as if no detour were there: the guard forwards every call.
void __fastcall EndPlayGuard(void* actor, uint32_t reason) {
    if (g_installed.load(std::memory_order_acquire)) {
        EndPlayDetour(actor, reason);
        return;
    }
    g_trampoline(actor, reason);
}

// AActor::EndPlay from the image alone: the one function whose body, past its prologue and stack cookie,
// holds the begun-play test at kActorEndPlayBodyOff. Read past the prologue, so an entry another hooker
// already jumps from is found as well. 0 unless exactly one.
std::uintptr_t FindByCode() {
    std::uintptr_t text = 0;
    size_t textSize = 0;
    if (!ue_wrap::MainTextRange(text, textSize)) return 0;
    std::uintptr_t found = 0;
    for (std::uintptr_t from = text; from < text + textSize;) {
        const std::uintptr_t hit = ue_wrap::FindPatternIn(from, text + textSize - from, prof::kActorEndPlayBody);
        if (!hit) break;
        const std::uintptr_t fn = hit - prof::kActorEndPlayBodyOff;
        if (ue_wrap::FunctionStart(hit) == fn) {
            if (found) return 0;
            found = fn;
        }
        from = hit + 1;
    }
    return found;
}

}  // namespace

// The engine's own test, the one Install checks the body for: EndPlay does its work only for an actor
// whose begun-play state reads HasBegunPlay.
bool HasBegunPlay(const void* actor) {
    const uint8_t state = *(static_cast<const uint8_t*>(actor) + prof::kActor_BegunPlayByte);
    return (state & prof::kActor_BegunPlayMask) == prof::kActor_HasBegunPlay;
}

bool Patch() {
    if (g_target) return true;
    const std::uintptr_t addr = FindByCode();
    ue_wrap::scan_ledger::Record(ue_wrap::scan_ledger::Scan::ActorEndPlay, addr != 0);
    if (!addr) {
        UE_LOGE("actor_end_play: AActor::EndPlay was not found from the image once (sdk_profile.h "
                "kActorEndPlayBody) -- NOT patched, no actor's end of play is seen");
        return false;
    }
    ue_wrap::hook::Init();  // idempotent
    // UE4SS detours the same function (its HookEndPlay, on by default). Patched here, before it can run,
    // it finds our jump and follows it into our relay: the immune relay, the one ProcessEvent and the
    // script loop install through, lets both detours compose.
    if (!ue_wrap::hook::Install(reinterpret_cast<void*>(addr), reinterpret_cast<void*>(&EndPlayGuard),
                                reinterpret_cast<void**>(&g_trampoline), /*followJmpImmune=*/true)) {
        UE_LOGE("actor_end_play: the detour on AActor::EndPlay@%p did not install -- no actor's end of play "
                "is seen", reinterpret_cast<void*>(addr));
        return false;
    }
    g_target = addr;
    std::uintptr_t image = 0;
    size_t imageSize = 0;
    ue_wrap::MainModuleRange(image, imageSize);
    UE_LOGI("actor_end_play: patched AActor::EndPlay (%p, exe+0x%zX); inert until armed",
            reinterpret_cast<void*>(addr), static_cast<size_t>(addr - image));
    return true;
}

bool Install() {
    if (g_installed.load(std::memory_order_acquire)) return true;
    if (!g_target) {
        UE_LOGE("actor_end_play: nothing was patched at the loader's call -- no actor's end of play is seen");
        return false;
    }
    // Where the engine finds it: RouteEndPlay calls EndPlay through the vtable, and Actor's own, read
    // from its class default object, names the function every actor reaches through Super. It must be
    // the patched one. The engine builds that object while it initializes, and the boot thread can get
    // here first: arming then found none and left EndPlay unseen for the whole process. So the arm waits
    // for the object to enter the array, bounded only for a build that never makes one, then briefly for
    // its constructor to set Actor's vtable: a vtable still naming another function a second later is the
    // profile's mismatch, said then, and the boot thread's later steps wait no longer for it.
    constexpr ULONGLONG kCdoWaitMs = 30000;
    constexpr ULONGLONG kVtableWaitMs = 1000;
    constexpr DWORD kPollMs = 50;  // the object's poll walks the whole object array by name
    uintptr_t image = 0;
    size_t imageSize = 0;
    ue_wrap::MainModuleRange(image, imageSize);
    const auto endPlayOf = [&](void* obj) -> uintptr_t {
        const auto* const vtbl = *static_cast<const uintptr_t* const*>(obj);
        const auto at = reinterpret_cast<uintptr_t>(vtbl);
        const bool inImage = at >= image && at + prof::kActor_EndPlay_VtblOff + sizeof(uintptr_t) <= image + imageSize;
        return inImage ? vtbl[prof::kActor_EndPlay_VtblOff / sizeof(uintptr_t)] : 0;
    };
    const ULONGLONG start = ::GetTickCount64();
    void* cdo = nullptr;
    while (!(cdo = ue_wrap::reflection::FindClassDefaultObject(L"Actor")) && ::GetTickCount64() - start < kCdoWaitMs)
        ::Sleep(kPollMs);
    if (!cdo) {
        UE_LOGE("actor_end_play: Default__Actor did not appear in the object array in %llu ms -- NOT armed, no "
                "actor's end of play is seen", ::GetTickCount64() - start);
        return false;
    }
    const ULONGLONG found = ::GetTickCount64();
    uintptr_t addr = endPlayOf(cdo);
    while (addr != g_target && ::GetTickCount64() - found < kVtableWaitMs) {
        ::Sleep(kPollMs);
        addr = endPlayOf(cdo);
    }
    if (addr == g_target && ::GetTickCount64() - start >= kPollMs)
        UE_LOGI("actor_end_play: Actor's class default object was ready after %llu ms", ::GetTickCount64() - start);
    if (addr != g_target) {
        UE_LOGE("actor_end_play: Actor's vtable at +0x%zX names %p, not the patched %p (sdk_profile.h "
                "kActor_EndPlay_VtblOff) -- NOT armed, no actor's end of play is seen",
                prof::kActor_EndPlay_VtblOff, reinterpret_cast<void*>(addr), reinterpret_cast<void*>(g_target));
        return false;
    }
    g_installed.store(true, std::memory_order_release);
    UE_LOGI("actor_end_play: armed on AActor::EndPlay (exe+0x%zX, the function Actor's vtable names) -- every "
            "actor that began play is seen ending it", static_cast<size_t>(addr - image));
    return true;
}

bool IsInstalled() { return g_installed.load(std::memory_order_acquire); }

bool AddSink(Sink sink) {
    if (!sink) return false;
    const int n = g_sinkCount.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i)
        if (g_sinks[i].load(std::memory_order_acquire) == sink) return true;
    if (n >= kMaxSinks) {
        UE_LOGE("actor_end_play: the sink table is full (%d) -- a sink was refused", kMaxSinks);
        return false;
    }
    g_sinks[n].store(sink, std::memory_order_release);
    g_sinkCount.store(n + 1, std::memory_order_release);
    return true;
}

Stats GetStats() {
    return {g_seen.load(std::memory_order_relaxed), g_offGameThread.load(std::memory_order_relaxed)};
}

}  // namespace ue_wrap::actor_end_play
