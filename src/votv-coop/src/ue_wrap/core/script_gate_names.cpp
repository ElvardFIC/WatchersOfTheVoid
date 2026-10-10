// ue_wrap/core/script_gate_names.cpp -- the script gate's name watches: registration by a row's names,
// the resolve that turns those names into keys on the game thread, and the judging of a class-scoped
// watch's class. The tables are script_gate.cpp's, shared through script_gate_detail.h.

#include "ue_wrap/core/script_gate.h"

#include "script_gate_detail.h"

#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"

#include <cwchar>

namespace ue_wrap::script_gate {
using namespace detail;

namespace GT = ue_wrap::game_thread;
namespace P  = ue_wrap::profile;

namespace {

inline bool SameLiteral(const wchar_t* a, const wchar_t* b) {
    return a == b || (a && b && std::wcscmp(a, b) == 0);
}

// A name watch, class-scoped when `className` is set. A name's key is its FName, unknown until the
// game thread converts it; until then the entry is keyed on the literal's address and marked
// unresolved, and the resolve re-keys it.
bool WatchNameScoped(const wchar_t* className, const wchar_t* name, int tag, PreFn pre, PostFn post) {
    if (!name || !*name || (className && !*className) || (!pre && !post)) return false;
    if (RefusedUninstalled("WatchName")) return false;
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

}  // namespace ue_wrap::script_gate
