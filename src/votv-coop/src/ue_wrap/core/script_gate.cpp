// ue_wrap/core/script_gate.cpp -- see ue_wrap/core/script_gate.h.
// The watch surface's precedent (Relay's README) is named in the header.
//
// The loop is derived, not pattern-scanned (ue_wrap/core/script_loop): the patch finds it from the
// image alone, before the static initializers have filled the exec-handler table, and the arm
// requires the filled table's two local-call handlers to name the same function.

#include "ue_wrap/core/script_gate.h"

#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/hook.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_loop.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/core/sig_scan.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <mutex>

namespace ue_wrap::script_gate {
namespace {

namespace GT = ue_wrap::game_thread;
namespace R  = ue_wrap::reflection;
namespace P  = ue_wrap::profile;

// The loop's ABI, shared with the exec handlers: (Context, FFrame&, Result). The return is a
// leftover register no caller reads; forwarded unchanged.
using LoopFn = std::uintptr_t(__fastcall*)(void* ctx, void* stack, void* result);

LoopFn g_trampoline = nullptr;
void*  g_target = nullptr;         // the patched loop, set by Patch
std::atomic<bool> g_installed{false};   // armed: the patched loop is the one the handlers name
std::atomic<const ExecHandler*> g_execHandlers{nullptr};  // set at the arm step once the table is filled
std::atomic<bool> g_enabled{false};
std::atomic<bool> g_countOn{false};

// Fixed open-addressing tables, never rehashed and never freed: a slot's key is published last
// with release, so the loop's reader sees an empty slot or a whole entry, and a retired watch
// only clears its enabled flag, leaving the probe chain intact. Two key spaces, two tables: an
// exact watch keys on the UFunction pointer, a name watch on the FName's two indices (its
// unresolved placeholder holds a second slot). What fills a table is its KEYED slots, retired
// ones included, so the cap counts those: at most 3/8 of a table, the load at which a miss
// probes under two slots. A name watch, class-scoped or not, holds two keyed slots, so the name table
// takes 384 name watches: each peer of a two-peer smoke held 163-166, eighteen of them one lane's
// device entries, and every polled state moved onto a watch adds its own.
constexpr int kSlotBits = 11;
constexpr int kSlots = 1 << kSlotBits;
constexpr int kMaxKeyed = kSlots * 3 / 8;

struct Entry {
    std::atomic<std::uint64_t> key{0};
    std::atomic<bool> enabled{false};
    int tag = 0;
    PreFn pre = nullptr;
    PostFn post = nullptr;
    const wchar_t* name = nullptr;   // a name watch's registered literal; null for an exact one
    const wchar_t* className = nullptr;  // a class-scoped name watch's class literal, else null
    std::uint64_t classKey = 0;       // that class's FName key once resolved; 0 matches any owner
    std::atomic<bool> resolved{true}; // a name watch is inert until its FName is known
    std::atomic<bool> dead{false};    // a name watch that can never fire: a full table, or its class has no such body
    std::atomic<bool> unjudged{false};  // a class-scoped watch whose class was not loaded when it resolved
};
Entry g_fnTable[kSlots];
Entry g_nameTable[kSlots];
std::atomic<int> g_fnWatches{0};      // enabled exact watches
std::atomic<int> g_nameWatches{0};    // enabled name watches, placeholders included
std::atomic<int> g_namesPending{0};
std::atomic<int> g_unjudged{0};       // class-scoped watches waiting for their class to load
std::uint64_t g_judgedAtClassSet = 0; // the class set's number at the last judging pass; game thread
int g_fnKeyed = 0;                    // keyed slots, under the registration mutex
int g_nameKeyed = 0;
std::mutex g_regMutex;   // registration only; never on the call path

inline std::uint64_t HashKey(std::uint64_t key) {
    return (key >> 4) * 0x9E3779B97F4A7C15ull;
}
// The hash's top bits pick the slot: exactly kSlotBits of them, so every slot is a home slot.
inline int SlotOf(std::uint64_t key) {
    return static_cast<int>(HashKey(key) >> (64 - kSlotBits));
}
inline std::uint64_t NameKey(const R::FName& n) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(n.ComparisonIndex)) << 32) |
           static_cast<std::uint32_t>(n.Number);
}

// The FName key of the class that owns `fn`'s body, its outer; read only after a class-scoped
// entry has matched the function's name.
inline std::uint64_t OwnerKey(void* fn) {
    void* owner = R::OuterOf(fn);
    return owner ? NameKey(R::NameOf(owner)) : 0;
}

// Whether entry `e`, whose key matched, is scoped to a class other than the one that owns `fn`'s
// body. `owner` caches that class's key across the entries of one probe (0 until first read).
inline bool OtherOwner(const Entry& e, void* fn, std::uint64_t& owner) {
    if (e.classKey == 0) return false;
    if (owner == 0) owner = OwnerKey(fn);
    return owner != e.classKey;
}

// Counters: relaxed atomics, torn reads tolerated by the readers.
std::atomic<unsigned long long> g_calls{0};
std::atomic<unsigned long long> g_callsGT{0};
std::atomic<unsigned long long> g_matched{0};
std::atomic<unsigned long long> g_cancelled{0};
std::atomic<unsigned long long> g_offGT{0};
std::atomic<unsigned long long> g_faults{0};

// The ambient window, per thread, published only around a watched body.
thread_local int            t_depth = 0;
thread_local int            t_tag = 0;
thread_local void*          t_object = nullptr;
thread_local void*          t_function = nullptr;
thread_local const wchar_t* t_name = nullptr;

// The whole chain of watched bodies on this thread, innermost first. CurrentThreadCall answers
// from the head; IsBodyActive walks it. A consumer asking "is MY verb running?" must be able to
// see past an inner watched body some other consumer owns, and -- the reason this is a chain
// rather than a consumer-side counter -- the answer has to unwind with the stack. A consumer
// counting its own pre and post callbacks is exact only while every pre is paired with a post,
// and it is not: a fault absorbed by the ProcessEvent firewall unwinds past the post-callback
// statements below, and any consumer returning Cancel skips the posts for every watch on the
// call. Either leaves such a counter stuck open for the life of the process, with the consumer
// believing its verb is running forever. This destructor runs on all three paths.
struct ActiveScope;
thread_local ActiveScope* t_activeHead = nullptr;

struct ActiveScope {
    ActiveScope* prev;
    int prevTag; void* prevObject; void* prevFunction; const wchar_t* prevName;
    void* self;     // the body THIS node published, so a walk reads each node without re-deriving
    void* caller;   // PreviousFrame->Node: WHO called this body, which a verb shared by several
                    // callers needs to tell a player's route from the world's own
    ActiveScope(int tag, void* object, void* function, const wchar_t* name, void* callerFunction)
        : prev(t_activeHead), prevTag(t_tag), prevObject(t_object), prevFunction(t_function),
          prevName(t_name), self(function), caller(callerFunction) {
        ++t_depth; t_tag = tag; t_object = object; t_function = function; t_name = name;
        t_activeHead = this;
    }
    ~ActiveScope() {
        --t_depth; t_tag = prevTag; t_object = prevObject; t_function = prevFunction; t_name = prevName;
        t_activeHead = prev;
    }
    ActiveScope(const ActiveScope&) = delete;
    ActiveScope& operator=(const ActiveScope&) = delete;
};

template <class T> inline T Read(const void* base, size_t off) {
    T v; std::memcpy(&v, reinterpret_cast<const std::uint8_t*>(base) + off, sizeof(T)); return v;
}

// ---- the crash firewall around a consumer callback -----------------------------------------
// SEH only in these two frames (no C++ unwind may share a frame with __try). A stack overflow is
// passed on: the guard page is gone and absorbing would run the engine on an exhausted stack.
int FaultFilter(EXCEPTION_POINTERS* ep, void** ip) {
    if (ep->ExceptionRecord->ExceptionCode == static_cast<DWORD>(EXCEPTION_STACK_OVERFLOW))
        return EXCEPTION_CONTINUE_SEARCH;
    *ip = ep->ExceptionRecord->ExceptionAddress;
    return EXCEPTION_EXECUTE_HANDLER;
}
int RunPreSEH(PreFn cb, const Call& call, Verdict* out, void** ip) {
    __try { *out = cb(call); return 0; }
    __except (FaultFilter(GetExceptionInformation(), ip)) { return 1; }
}
int RunPostSEH(PostFn cb, const Call& call, void** ip) {
    __try { cb(call); return 0; }
    __except (FaultFilter(GetExceptionInformation(), ip)) { return 1; }
}
// A fault names its callback and its site; the first few per phase print in full.
void LogFault(const char* phase, const Call& call, void* ip) {
    g_faults.fetch_add(1, std::memory_order_relaxed);
    static std::atomic<int> s_printed{0};
    if (s_printed.fetch_add(1, std::memory_order_relaxed) >= 8) return;
    const std::wstring fn = call.function ? R::ToString(R::NameOf(call.function)) : L"<null>";
    UE_LOGE("script_gate: %s callback FAULT absorbed -- function='%ls' object=%p tag=%d ip=%p; "
            "the body runs as if the callback had answered Run", phase, fn.c_str(),
            call.object, call.tag, ip);
}

// ---- the call path ----------------------------------------------------------------------------
// Every matching entry of a table fires, and any Cancel cancels the call: the entries after it still
// run and cannot restore it. Returns true when cancelled.
bool FireTable(Entry* table, std::uint64_t key, const Call& base, bool post) {
    bool cancel = false;
    std::uint64_t owner = 0;
    for (int i = SlotOf(key), n = 0; n < kSlots; ++n, i = (i + 1) & (kSlots - 1)) {
        const std::uint64_t k = table[i].key.load(std::memory_order_acquire);
        if (k == 0) break;
        if (k != key) continue;
        Entry& e = table[i];
        if (!e.enabled.load(std::memory_order_acquire) || !e.resolved.load(std::memory_order_acquire)) continue;
        if (OtherOwner(e, base.function, owner)) continue;
        Call call = base;
        call.tag = e.tag;
        void* ip = nullptr;
        if (post) {
            if (e.post && RunPostSEH(e.post, call, &ip) != 0) LogFault("post", call, ip);
        } else if (e.pre) {
            Verdict v = Verdict::Run;
            if (RunPreSEH(e.pre, call, &v, &ip) != 0) { LogFault("pre", call, ip); v = Verdict::Run; }
            if (v == Verdict::Cancel) cancel = true;
        }
    }
    return cancel;
}

// The innermost entry's identity for the ambient window: the first enabled match in either table,
// a class-scoped entry only when `fn`'s body is its class's.
const Entry* FirstMatch(Entry* table, std::uint64_t key, void* fn) {
    std::uint64_t owner = 0;
    for (int i = SlotOf(key), n = 0; n < kSlots; ++n, i = (i + 1) & (kSlots - 1)) {
        const std::uint64_t k = table[i].key.load(std::memory_order_acquire);
        if (k == 0) return nullptr;
        if (k == key && table[i].enabled.load(std::memory_order_acquire) &&
            table[i].resolved.load(std::memory_order_acquire) && !OtherOwner(table[i], fn, owner))
            return &table[i];
    }
    return nullptr;
}

// Not inlined, so the guard below stays a bare test and two tail jumps.
__declspec(noinline) std::uintptr_t __fastcall LoopDetour(void* ctx, void* stack, void* result) {
    // The tax every script call pays for the life of the process: one relaxed load and a
    // predicted branch while disabled; enabled, one hashed probe per key space, and, on a name hit
    // whose entry is class-scoped, one read of the body's owning class's name.
    if (!g_enabled.load(std::memory_order_relaxed)) return g_trampoline(ctx, stack, result);
    if (g_countOn.load(std::memory_order_relaxed)) {
        g_calls.fetch_add(1, std::memory_order_relaxed);
        if (GT::IsGameThread()) g_callsGT.fetch_add(1, std::memory_order_relaxed);
    }
    void* fn = Read<void*>(stack, P::off::FFrame_Node);
    const std::uint64_t fnKey = reinterpret_cast<std::uintptr_t>(fn);
    const Entry* hit = g_fnWatches.load(std::memory_order_relaxed) > 0 ? FirstMatch(g_fnTable, fnKey, fn) : nullptr;
    std::uint64_t nameKey = 0;
    if (g_nameWatches.load(std::memory_order_relaxed) > 0) {
        nameKey = NameKey(R::NameOf(fn));
        if (!hit) hit = FirstMatch(g_nameTable, nameKey, fn);
    }
    if (!hit) return g_trampoline(ctx, stack, result);

    if (!GT::IsGameThread()) {
        // The consumers reach the engine and our reflection, both game-thread only.
        g_offGT.fetch_add(1, std::memory_order_relaxed);
        return g_trampoline(ctx, stack, result);
    }
    g_matched.fetch_add(1, std::memory_order_relaxed);

    Call call{};
    call.object = Read<void*>(stack, P::off::FFrame_Object);
    call.function = fn;
    call.locals = Read<std::uint8_t*>(stack, P::off::FFrame_Locals);
    call.result = result;
    if (void* prev = Read<void*>(stack, P::off::FFrame_PreviousFrame)) {
        call.callerObject = Read<void*>(prev, P::off::FFrame_Object);
        call.callerFunction = Read<void*>(prev, P::off::FFrame_Node);
        call.callerLocals = Read<std::uint8_t*>(prev, P::off::FFrame_Locals);
    }
    call.stack = stack;
    call.depth = t_depth + 1;
    call.fromOurCode = R::InCoopDispatch();

    bool cancel = FireTable(g_fnTable, fnKey, call, /*post=*/false);
    if (nameKey) cancel = FireTable(g_nameTable, nameKey, call, /*post=*/false) || cancel;
    if (cancel) {
        g_cancelled.fetch_add(1, std::memory_order_relaxed);
        return 0;
    }
    std::uintptr_t rv;
    {
        ActiveScope scope(hit->tag, call.object, fn, hit->name, call.callerFunction);
        rv = g_trampoline(ctx, stack, result);
    }
    FireTable(g_fnTable, fnKey, call, /*post=*/true);
    if (nameKey) FireTable(g_nameTable, nameKey, call, /*post=*/true);
    return rv;
}

// Until the arm, the patched loop runs as if no detour were there: the guard forwards every body.
std::uintptr_t __fastcall LoopGuard(void* ctx, void* stack, void* result) {
    if (g_installed.load(std::memory_order_acquire)) return LoopDetour(ctx, stack, result);
    return g_trampoline(ctx, stack, result);
}

}  // namespace

bool Patch() {
    if (g_target) return true;
    std::uintptr_t* gnatives = script_loop::ResolveGNatives();
    int candidates = 0;
    const std::uintptr_t loop =
        gnatives ? script_loop::ByCode(reinterpret_cast<std::uintptr_t>(gnatives), candidates) : 0;
    uintptr_t base = 0; size_t size = 0;
    ue_wrap::MainModuleRange(base, size);
    if (!loop) {
        UE_LOGE("script_gate: the loop was not found from the image (table %p, %d functions reference it) -- "
                "NOT patched", static_cast<void*>(gnatives), candidates);
        return false;
    }
    if (!hook::Init()) return false;
    // The loop is a function UE4SS's own PolyHook detours for its Lua script hooks, so the relay must
    // be followJmp-immune, as ProcessEvent's is.
    if (!hook::Install(reinterpret_cast<void*>(loop), reinterpret_cast<void*>(&LoopGuard),
                       reinterpret_cast<void**>(&g_trampoline), /*followJmpImmune=*/true)) {
        return false;
    }
    g_target = reinterpret_cast<void*>(loop);
    UE_LOGI("script_gate: patched the VM's script loop at exe+0x%llX (the one of %d functions that reference the "
            "exec-handler table at %p with the loop's shape); inert until armed",
            static_cast<unsigned long long>(loop - base), candidates, static_cast<void*>(gnatives));
    return true;
}

bool Install() {
    if (g_installed.load(std::memory_order_acquire)) return true;
    if (!g_target) {
        UE_LOGE("script_gate: nothing was patched at the loader's call -- NOT armed");
        return false;
    }
    uintptr_t base = 0; size_t size = 0;
    ue_wrap::MainModuleRange(base, size);
    std::uintptr_t* gnatives = script_loop::ResolveGNatives();
    if (!script_loop::TableFilled(gnatives)) {
        UE_LOGE("script_gate: the exec-handler table did not resolve or is not filled (%p) -- NOT armed",
                static_cast<void*>(gnatives));
        return false;
    }
    g_execHandlers.store(reinterpret_cast<const ExecHandler*>(gnatives), std::memory_order_release);
    std::uintptr_t virt = 0, fin = 0;
    const std::uintptr_t loop = script_loop::ByHandlers(gnatives, virt, fin);
    if (loop != reinterpret_cast<std::uintptr_t>(g_target)) {
        UE_LOGE("script_gate: the handlers name another loop than the patched one (local-virtual -> exe+0x%llX, "
                "local-final -> exe+0x%llX, patched exe+0x%llX) -- NOT armed, every watch will be refused",
                static_cast<unsigned long long>(virt ? virt - base : 0),
                static_cast<unsigned long long>(fin ? fin - base : 0),
                static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(g_target) - base));
        return false;
    }
    g_installed.store(true, std::memory_order_release);
    UE_LOGI("script_gate: armed on the VM's script loop at exe+0x%llX (both exec handlers name the patched "
            "function); disabled until something holds it",
            static_cast<unsigned long long>(loop - base));
    return true;
}

bool IsInstalled() { return g_installed.load(std::memory_order_acquire); }

const ExecHandler* ExecHandlers() { return g_execHandlers.load(std::memory_order_acquire); }

namespace {

// Registration under the mutex: an idempotent re-register re-enables the same slot; a new pair
// takes the first empty slot of the probe chain. `enabled` false keeps an entry for a later re-register
// without firing it. Returns false when the chain is full.
bool Register(Entry* table, std::atomic<int>& count, int& keyed, std::uint64_t key, int tag,
              PreFn pre, PostFn post, const wchar_t* name, const wchar_t* className,
              std::uint64_t classKey, bool resolved, bool enabled = true, bool unjudged = false) {
    for (int i = SlotOf(key), n = 0; n < kSlots; ++n, i = (i + 1) & (kSlots - 1)) {
        Entry& e = table[i];
        const std::uint64_t k = e.key.load(std::memory_order_relaxed);
        // A dead entry keeps its literals for the settled question and is never enabled again: a re-watch of it
        // takes a slot of its own, which dies the same way.
        if (k == key && e.tag == tag && e.pre == pre && e.post == post && e.name == name &&
            e.className == className && !e.dead.load(std::memory_order_relaxed)) {
            if (enabled && !e.enabled.exchange(true, std::memory_order_release))
                count.fetch_add(1, std::memory_order_release);
            return true;
        }
        if (k != 0) continue;
        if (keyed >= kMaxKeyed) return false;
        e.tag = tag; e.pre = pre; e.post = post; e.name = name;
        e.className = className; e.classKey = classKey;
        e.resolved.store(resolved, std::memory_order_relaxed);
        e.enabled.store(enabled, std::memory_order_relaxed);
        e.unjudged.store(unjudged, std::memory_order_relaxed);
        e.key.store(key, std::memory_order_release);   // published last: the reader sees a whole entry
        ++keyed;
        if (enabled) count.fetch_add(1, std::memory_order_release);
        if (unjudged) g_unjudged.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

bool Retire(Entry* table, std::atomic<int>& count, std::uint64_t key, int tag, PreFn pre,
            PostFn post, const wchar_t* name) {
    for (int i = SlotOf(key), n = 0; n < kSlots; ++n, i = (i + 1) & (kSlots - 1)) {
        Entry& e = table[i];
        const std::uint64_t k = e.key.load(std::memory_order_relaxed);
        if (k == 0) return false;
        if (k == key && e.tag == tag && e.pre == pre && e.post == post && e.name == name) {
            if (e.enabled.exchange(false, std::memory_order_release)) count.fetch_sub(1, std::memory_order_release);
            return true;
        }
    }
    return false;
}

}  // namespace

// A watch on a gate that never installed is refused, so a consumer's "live" line cannot print
// over a seam that sees nothing; said once, since the consumers retry from their ticks.
bool RefusedUninstalled(const char* what) {
    if (g_installed.load(std::memory_order_acquire)) return false;
    static std::atomic<bool> s_said{false};
    if (!s_said.exchange(true, std::memory_order_relaxed))
        UE_LOGE("script_gate: %s refused -- the gate is not installed, so no watch can fire", what);
    return true;
}

bool Watch(void* ufunction, int tag, PreFn pre, PostFn post) {
    if (!ufunction || (!pre && !post)) return false;
    if (RefusedUninstalled("Watch")) return false;
    if (Read<std::uint32_t>(ufunction, P::off::UFunction_FunctionFlags) & P::off::FUNC_Native) {
        UE_LOGE("script_gate: %p is a native function; it never runs through the script loop -- refused",
                ufunction);
        return false;
    }
    if (Read<std::int32_t>(ufunction, P::off::UStruct_ScriptNum) <= 0)
        UE_LOGW("script_gate: %p has no bytecode; the watch can never fire", ufunction);
    std::lock_guard<std::mutex> lk(g_regMutex);
    const bool ok = Register(g_fnTable, g_fnWatches, g_fnKeyed, reinterpret_cast<std::uintptr_t>(ufunction),
                             tag, pre, post, nullptr, nullptr, 0, /*resolved=*/true);
    if (!ok) UE_LOGE("script_gate: watch table full (%d keyed slots) -- cannot watch %p", kMaxKeyed, ufunction);
    return ok;
}

bool Unwatch(void* ufunction, int tag, PreFn pre, PostFn post) {
    if (!ufunction) return false;
    std::lock_guard<std::mutex> lk(g_regMutex);
    return Retire(g_fnTable, g_fnWatches, reinterpret_cast<std::uintptr_t>(ufunction), tag, pre, post, nullptr);
}

namespace {

inline bool SameLiteral(const wchar_t* a, const wchar_t* b) {
    return a == b || (a && b && std::wcscmp(a, b) == 0);
}

// A name watch, class-scoped when `className` is set. A name's key is its FName, unknown until the
// game thread converts it; until then the entry is keyed on the literal's address and marked
// unresolved, and the resolve re-keys it.
bool WatchNameScoped(const wchar_t* className, const wchar_t* name, int tag, PreFn pre, PostFn post) {
    if (!name || !*name || (className && !*className) || (!pre && !post)) return false;
    if (RefusedUninstalled(className ? "WatchClassName" : "WatchName")) return false;
    std::lock_guard<std::mutex> lk(g_regMutex);
    for (int i = 0; i < kSlots; ++i) {
        Entry& e = g_nameTable[i];
        if (e.key.load(std::memory_order_relaxed) != 0 && e.name && e.tag == tag && e.pre == pre &&
            e.post == post && std::wcscmp(e.name, name) == 0 && SameLiteral(e.className, className) &&
            !e.dead.load(std::memory_order_relaxed)) {
            if (!e.enabled.exchange(true, std::memory_order_release)) g_nameWatches.fetch_add(1, std::memory_order_release);
            return true;
        }
    }
    const bool ok = Register(g_nameTable, g_nameWatches, g_nameKeyed, reinterpret_cast<std::uintptr_t>(name) | 1u,
                             tag, pre, post, name, className, 0, /*resolved=*/false);
    if (!ok) {
        static std::atomic<bool> s_saidFull{false};
        if (!s_saidFull.exchange(true, std::memory_order_relaxed))
            UE_LOGE("script_gate: name watch table full (%d keyed slots) -- cannot watch %ls", kMaxKeyed, name);
        return false;
    }
    g_namesPending.fetch_add(1, std::memory_order_release);
    if (className)
        UE_LOGI("script_gate: watching '%ls' of class '%ls' tag=%d -- pending the game-thread name resolve",
                name, className, tag);
    else
        UE_LOGI("script_gate: watching '%ls' tag=%d -- pending the game-thread name resolve", name, tag);
    GT::Post([] { ResolvePendingNames(); });
    return true;
}

// The whole table, not the probe chain: before the resolve the entry sits at its placeholder key
// and after it at the real one, and this answers across both. The resolve leaves the placeholder
// disabled and nameless, so a name that resolved into a FULL table -- the watch the gate calls dead
// -- matches nothing here and reads as not live, which is the truth.
bool ScopedWatchLive(const wchar_t* className, const wchar_t* name, int tag) {
    if (!name) return false;
    for (int i = 0; i < kSlots; ++i) {
        const Entry& e = g_nameTable[i];
        if (e.key.load(std::memory_order_acquire) == 0) continue;
        if (e.name != name || e.className != className || e.tag != tag) continue;
        if (e.enabled.load(std::memory_order_acquire) && e.resolved.load(std::memory_order_acquire))
            return true;
    }
    return false;
}

// Live, or dead for good: its placeholder keeps the literals and the dead mark after a failed re-key.
bool ScopedWatchSettled(const wchar_t* className, const wchar_t* name, int tag) {
    if (!name) return false;
    for (int i = 0; i < kSlots; ++i) {
        const Entry& e = g_nameTable[i];
        if (e.key.load(std::memory_order_acquire) == 0) continue;
        if (e.name != name || e.className != className || e.tag != tag) continue;
        if (e.dead.load(std::memory_order_acquire)) return true;
        if (e.enabled.load(std::memory_order_acquire) && e.resolved.load(std::memory_order_acquire))
            return true;
    }
    return false;
}

}  // namespace

namespace {
// Disable every entry of that name watch: the resolved one, or the placeholder still waiting for its
// name, which the resolve then re-keys disabled, its literals kept for a re-watch.
bool UnwatchNameScoped(const wchar_t* className, const wchar_t* name, int tag, PreFn pre, PostFn post) {
    if (!name) return false;
    std::lock_guard<std::mutex> lk(g_regMutex);
    bool found = false;
    for (int i = 0; i < kSlots; ++i) {
        Entry& e = g_nameTable[i];
        if (e.key.load(std::memory_order_relaxed) == 0 || !e.name || e.tag != tag || e.pre != pre ||
            e.post != post || std::wcscmp(e.name, name) != 0 || !SameLiteral(e.className, className))
            continue;
        if (e.enabled.exchange(false, std::memory_order_release)) g_nameWatches.fetch_sub(1, std::memory_order_release);
        found = true;
    }
    return found;
}
}  // namespace

bool WatchName(const wchar_t* name, int tag, PreFn pre, PostFn post) {
    return WatchNameScoped(nullptr, name, tag, pre, post);
}

bool UnwatchName(const wchar_t* name, int tag, PreFn pre, PostFn post) {
    return UnwatchNameScoped(nullptr, name, tag, pre, post);
}

bool UnwatchClassName(const wchar_t* className, const wchar_t* name, int tag, PreFn pre, PostFn post) {
    return className && UnwatchNameScoped(className, name, tag, pre, post);
}

bool WatchClassName(const wchar_t* className, const wchar_t* name, int tag, PreFn pre, PostFn post) {
    if (!className) return false;
    return WatchNameScoped(className, name, tag, pre, post);
}

bool NameWatchLive(const wchar_t* name, int tag) { return ScopedWatchLive(nullptr, name, tag); }

bool NameWatchSettled(const wchar_t* name, int tag) { return ScopedWatchSettled(nullptr, name, tag); }

bool ClassNameWatchLive(const wchar_t* className, const wchar_t* name, int tag) {
    return className && ScopedWatchLive(className, name, tag);
}

bool ClassNameWatchSettled(const wchar_t* className, const wchar_t* name, int tag) {
    return className && ScopedWatchSettled(className, name, tag);
}

namespace {
// The class a name-form row scopes its watch to: none for an AnyClass row, its class for a Declared one.
bool ScopeOf(const facts::Function& row, const wchar_t*& cls) {
    if (row.GetMode() == facts::Mode::AnyClass && !row.Class()) {
        cls = nullptr;
        return true;
    }
    if (row.GetMode() == facts::Mode::Declared && row.Class()) {
        cls = row.Class();
        return true;
    }
    UE_LOGE("script-gate: row %s cannot be a name watch (an AnyClass row with no class, or a Declared one with "
            "its class)", row.Ident());
    return false;
}
}  // namespace

bool Watch(const facts::Resolved& fn, int tag, PreFn pre, PostFn post) {
    if (!fn) return false;  // not resolved yet: nothing was asked of the gate, nothing is recorded
    const bool ok = Watch(fn.Fn(), tag, pre, post);
    fn.Row()->NoteUse(facts::kUseScriptGate, ok);
    return ok;
}

bool Unwatch(const facts::Resolved& fn, int tag, PreFn pre, PostFn post) {
    return fn && Unwatch(fn.Fn(), tag, pre, post);
}

bool WatchName(const facts::Function& row, int tag, PreFn pre, PostFn post) {
    const wchar_t* cls = nullptr;
    const bool ok = ScopeOf(row, cls) && WatchNameScoped(cls, row.Name(), tag, pre, post);
    row.NoteUse(facts::kUseScriptGate, ok);
    return ok;
}

bool UnwatchName(const facts::Function& row, int tag, PreFn pre, PostFn post) {
    const wchar_t* cls = nullptr;
    return ScopeOf(row, cls) && UnwatchNameScoped(cls, row.Name(), tag, pre, post);
}

bool NameWatchLive(const facts::Function& row, int tag) {
    const wchar_t* cls = nullptr;
    return ScopeOf(row, cls) && ScopedWatchLive(cls, row.Name(), tag);
}

bool NameWatchSettled(const facts::Function& row, int tag) {
    const wchar_t* cls = nullptr;
    return ScopeOf(row, cls) && ScopedWatchSettled(cls, row.Name(), tag);
}

int PendingNameCount() { return g_namesPending.load(std::memory_order_acquire); }

void DebugForEachWatch(WatchVisitor visit, void* ctx) {
    if (!visit) return;
    for (const Entry& e : g_fnTable)
        if (e.key.load(std::memory_order_acquire) && e.enabled.load(std::memory_order_relaxed))
            visit(ctx, reinterpret_cast<void*>(e.key.load(std::memory_order_relaxed)), nullptr, nullptr, e.tag);
    for (const Entry& e : g_nameTable)
        if (e.key.load(std::memory_order_acquire) && e.enabled.load(std::memory_order_relaxed) && e.name)
            visit(ctx, nullptr, e.className, e.name, e.tag);
}

namespace {
// What a class-scoped watch's class says of its name once it is loaded: a body it declares, which the loop runs;
// none, or a native one, which never runs through the loop, so the watch can never fire; or nothing yet, the class
// not loaded or still loading. Game thread (the object index answers there).
enum class ClassAnswer : uint8_t { NotLoaded, Declared, Undeclared, Native };

ClassAnswer AskClass(const wchar_t* className, const wchar_t* name) {
    void* cls = ue_wrap::object_index::ClassByName(className);
    if (!cls) return ClassAnswer::NotLoaded;
    void* fn = R::FindFunction(cls, name);
    if (!fn) return ClassAnswer::Undeclared;
    return (Read<std::uint32_t>(fn, P::off::UFunction_FunctionFlags) & P::off::FUNC_Native) ? ClassAnswer::Native
                                                                                          : ClassAnswer::Declared;
}

void LogNeverFires(const wchar_t* className, const wchar_t* name, ClassAnswer a) {
    if (a == ClassAnswer::Native)
        UE_LOGE("script_gate: class '%ls' declares '%ls' native, and a native body never runs through the script "
                "loop -- the watch can never fire", className, name);
    else
        UE_LOGE("script_gate: class '%ls' declares no function '%ls' -- the watch can never fire", className, name);
}

// The class-scoped watches whose class was not loaded when they resolved, asked when the class set next moves (a
// class gains its first instance or loses its last), so a watch registered before its class loads is judged all
// the same: a class that declares no such body kills it, as the resolve would have. Asking a class dispatches
// nothing, so it runs under the registration mutex. One load and a compare while none waits or the set has not
// moved. Game thread.
void JudgeWaitingClasses() {
    if (g_unjudged.load(std::memory_order_relaxed) == 0) return;
    const std::uint64_t set = ue_wrap::object_index::ClassSetVersion();
    if (set == g_judgedAtClassSet) return;
    g_judgedAtClassSet = set;
    int declared = 0, dead = 0;
    std::lock_guard<std::mutex> lk(g_regMutex);
    for (Entry& e : g_nameTable) {
        if (e.key.load(std::memory_order_relaxed) == 0 || !e.unjudged.load(std::memory_order_relaxed)) continue;
        const ClassAnswer a = AskClass(e.className, e.name);
        if (a == ClassAnswer::NotLoaded) continue;
        e.unjudged.store(false, std::memory_order_relaxed);
        g_unjudged.fetch_sub(1, std::memory_order_relaxed);
        if (a == ClassAnswer::Declared) {
            ++declared;
            continue;
        }
        if (e.enabled.exchange(false, std::memory_order_release)) g_nameWatches.fetch_sub(1, std::memory_order_release);
        e.dead.store(true, std::memory_order_release);
        ++dead;
        LogNeverFires(e.className, e.name, a);
    }
    if (declared || dead)
        UE_LOGI("script_gate: %d class-scoped watch(es) judged as their classes loaded: %d declared by their class, "
                "%d dead; %d still wait", declared + dead, declared, dead, g_unjudged.load(std::memory_order_relaxed));
}
}  // namespace

void ResolvePendingNames() {
    if (!GT::IsGameThread()) return;
    JudgeWaitingClasses();
    if (g_namesPending.load(std::memory_order_acquire) == 0) return;
    // The string-to-name conversion dispatches ProcessEvent, so it runs OUTSIDE the registration
    // mutex: a registration reached from inside that dispatch would otherwise wait on itself.
    // Under the mutex only the pending literals are collected, and the re-key is done after.
    struct Pending { const wchar_t* name; const wchar_t* className; };
    Pending pending[kMaxKeyed];   // every keyed slot at most
    int n = 0;
    {
        std::lock_guard<std::mutex> lk(g_regMutex);
        for (int i = 0; i < kSlots && n < kMaxKeyed; ++i) {
            const Entry& e = g_nameTable[i];
            if (e.key.load(std::memory_order_relaxed) != 0 && !e.resolved.load(std::memory_order_relaxed) && e.name)
                pending[n++] = {e.name, e.className};
        }
    }
    for (int j = 0; j < n; ++j) {
        const Pending& p = pending[j];
        const R::FName f = ue_wrap::fname_utils::StringToFName(p.name);
        if (f.ComparisonIndex == 0) continue;   // not yet; the next tick retries
        // A class-scoped watch goes live only with its class's name resolved too.
        R::FName c{};
        if (p.className) {
            c = ue_wrap::fname_utils::StringToFName(p.className);
            if (c.ComparisonIndex == 0) continue;
        }
        // A class-scoped watch fires only for a body its class declares (OtherOwner), and the conversion
        // above adds any name it is given, so a misspelt or mis-copied one resolves all the same: its class
        // is asked now if it is loaded, else when the class set moves (JudgeWaitingClasses).
        const ClassAnswer answer = p.className ? AskClass(p.className, p.name) : ClassAnswer::Declared;
        const bool neverFires = answer == ClassAnswer::Undeclared || answer == ClassAnswer::Native;
        const bool unjudged = p.className && answer == ClassAnswer::NotLoaded;
        std::lock_guard<std::mutex> lk(g_regMutex);
        // An unresolved entry sits at the slot of its placeholder key; once the FName is known
        // it moves to the slot of its real key, and the placeholder slot is left disabled, keyed
        // (its chain stays walkable) and nameless, so no registration matches it again.
        for (int i = 0; i < kSlots; ++i) {
            Entry& e = g_nameTable[i];
            if (e.key.load(std::memory_order_relaxed) == 0 || e.resolved.load(std::memory_order_relaxed) ||
                e.name != p.name || e.className != p.className) continue;
            const bool wasEnabled = e.enabled.exchange(false, std::memory_order_release);
            e.name = nullptr;
            e.className = nullptr;
            e.resolved.store(true, std::memory_order_release);
            g_namesPending.fetch_sub(1, std::memory_order_release);
            if (wasEnabled) g_nameWatches.fetch_sub(1, std::memory_order_release);
            const wchar_t* ofClass = p.className ? L" of class " : L"";
            const wchar_t* cls = p.className ? p.className : L"";
            // A dead placeholder keeps its literals and the mark, so a consumer asking whether the watch
            // has settled is told, rather than waiting for one that never comes.
            auto markDead = [&e, &p] {
                e.name = p.name;
                e.className = p.className;
                e.dead.store(true, std::memory_order_release);
            };
            if (neverFires) {
                LogNeverFires(cls, p.name, answer);
                markDead();
                continue;
            }
            // One retired while it waited is re-keyed too, disabled with its literals: a later watch of it
            // re-enables that slot, where a dropped one would take a new placeholder each time.
            if (Register(g_nameTable, g_nameWatches, g_nameKeyed, NameKey(f), e.tag, e.pre, e.post, p.name,
                         p.className, p.className ? NameKey(c) : 0, /*resolved=*/true, /*enabled=*/wasEnabled,
                         unjudged)) {
                const wchar_t* checked = !p.className ? L""
                                         : unjudged ? L"; its class is not loaded yet, and is asked when it loads"
                                                    : L", declared by its class";
                if (wasEnabled)
                    UE_LOGI("script_gate: name '%ls'%ls%ls resolved (cmp=0x%x number=0x%x) -- the watch is live%ls",
                            p.name, ofClass, cls, f.ComparisonIndex, f.Number, checked);
                else
                    UE_LOGI("script_gate: name '%ls'%ls%ls resolved after its watch was retired -- kept for a "
                            "re-watch", p.name, ofClass, cls);
            } else {
                UE_LOGE("script_gate: name '%ls'%ls%ls resolved but the table is full -- the watch is dead",
                        p.name, ofClass, cls);
                markDead();
            }
        }
    }
}

namespace {
// The holders: a count under a lock, not a lone atomic, because the edge has to be decided with
// its store; otherwise a release's disable could land after a racing acquire's enable and leave the
// gate off with a holder alive. Both edges are rare (a session's start or end, a save).
std::mutex g_holdMutex;
int g_holds = 0;
}  // namespace

void Acquire(const char* who) {
    std::lock_guard<std::mutex> lk(g_holdMutex);
    if (g_holds++ == 0) {
        g_enabled.store(true, std::memory_order_release);
        UE_LOGI("script_gate: ENABLED (%s holds it)", who);
        // Past this point a body runs our full detour: the shared entries must still hold our jumps.
        hook::VerifyEntries("the gate's first hold");
    }
}

void Release(const char* who) {
    std::lock_guard<std::mutex> lk(g_holdMutex);
    if (g_holds == 0) {
        UE_LOGE("script_gate: %s released a hold it never took -- ignored", who);
        return;
    }
    if (--g_holds == 0) {
        g_enabled.store(false, std::memory_order_release);
        UE_LOGI("script_gate: DISABLED (%s released the last hold)", who);
    }
}

bool IsEnabled() { return g_enabled.load(std::memory_order_acquire); }

Active CurrentThreadCall() {
    Active a{};
    a.active = t_depth > 0;
    a.tag = t_tag;
    a.depth = t_depth;
    a.object = t_object;
    a.function = t_function;
    a.name = t_name;
    return a;
}

bool IsBodyActive(void* function, void* callerFunction) {
    if (!function) return false;
    for (const ActiveScope* s = t_activeHead; s; s = s->prev)
        if (s->self == function && (!callerFunction || s->caller == callerFunction)) return true;
    return false;
}

std::uint8_t* OutParamPtr(const Call& call, int32_t paramOffset) {
    if (!call.stack || paramOffset < 0) return nullptr;
    for (void* rec = Read<void*>(call.stack, P::off::FFrame_OutParms); rec;
         rec = Read<void*>(rec, P::off::FOutParmRec_Next)) {
        void* prop = Read<void*>(rec, P::off::FOutParmRec_Property);
        if (prop && Read<std::int32_t>(prop, P::off::FProperty_Offset_Internal) == paramOffset)
            return Read<std::uint8_t*>(rec, P::off::FOutParmRec_PropAddr);
    }
    return nullptr;
}

Stats GetStats() {
    Stats s{};
    s.calls = g_calls.load(std::memory_order_relaxed);
    s.callsGameThread = g_callsGT.load(std::memory_order_relaxed);
    s.matched = g_matched.load(std::memory_order_relaxed);
    s.cancelled = g_cancelled.load(std::memory_order_relaxed);
    s.offGameThread = g_offGT.load(std::memory_order_relaxed);
    s.faults = g_faults.load(std::memory_order_relaxed);
    s.watches = g_fnWatches.load(std::memory_order_relaxed);
    // Live name watches: the enabled count less the placeholders still waiting for their name.
    s.nameWatches = g_nameWatches.load(std::memory_order_relaxed) - g_namesPending.load(std::memory_order_relaxed);
    s.enabled = g_enabled.load(std::memory_order_relaxed);
    s.installed = g_installed.load(std::memory_order_relaxed);
    return s;
}

void SetPerfCounting(bool on) { g_countOn.store(on, std::memory_order_relaxed); }

}  // namespace ue_wrap::script_gate
