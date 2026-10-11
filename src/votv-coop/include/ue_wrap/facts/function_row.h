// ue_wrap/facts/function_row.h -- the game-facts registry's function rows: every game function the mod
// watches or hooks is declared once, as a row, in the file that registers it, and the registration APIs
// (script_gate, ufunction_hook, game_thread) accept only rows, so a dependency cannot be added without
// one. The census (harness/fingerprint_census) walks every row to check the running game against the
// build the mod was made for (docs/bytecode-fingerprint.md, "The registry").
//
//   UE_FACT_FN(kGenBreak, "generator_sync", L"generator_C", L"break", Declared);
//
// declares the row kGenBreak: its feature (the lane that relies on it, a lowercase id), the class (null
// for every class), the function and how it resolves. The macro opens an anonymous namespace, so a row
// cannot be declared inside a function; its strings are literals so a source gate can read them.
// Engine-wrapper layer (principle 7): the feature is a label this layer never interprets.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace ue_wrap::facts {

// How a row names its function. Declared: the class declares it (reflection::FindFunction). Dispatch:
// the class or its nearest ancestor (FindDispatchFunction). AnyClass: every class's function of that
// name (a script_gate name watch). RuntimeClass: the class is known only at run time (ResolveOn).
enum class Mode : uint8_t { Declared, Dispatch, AnyClass, RuntimeClass };

// What registered a row, a bit per kind, recorded on the row.
enum Use : uint8_t {
    kUseScriptGate = 1,   // script_gate watch (pointer or name)
    kUseNativeHook = 2,   // ufunction_hook Func patch
    kUseObserver = 4,     // game_thread pre/post observer
    kUseInterceptor = 8,  // game_thread interceptor
};

namespace detail {
struct MacroToken {};  // only UE_FACT_FN names it: a row exists only through the macro
}  // namespace detail

class Function {
public:
    Function(detail::MacroToken, const char* ident, const char* feature, const wchar_t* cls, const wchar_t* fn,
             Mode mode);
    Function(const Function&) = delete;
    Function& operator=(const Function&) = delete;

    const char*    Ident() const { return ident_; }
    const char*    Feature() const { return feature_; }
    const wchar_t* Class() const { return cls_; }
    const wchar_t* Name() const { return fn_; }
    Mode           GetMode() const { return mode_; }
    const Function* Next() const { return next_; }

    // The registration record: which kinds accepted and which refused this row this session. No pointer is
    // kept, so a world change cannot leave one dangling. Any thread.
    void    NoteUse(Use use, bool accepted) const;
    uint8_t Accepted() const { return accepted_.load(std::memory_order_relaxed); }
    uint8_t Refused() const { return refused_.load(std::memory_order_relaxed); }

    // A RuntimeClass row's class as ResolveOn last saw it: the class's name (its FName, packed), never a
    // pointer, so the census can find the class again after a world change. 0 until a site resolved it.
    uint64_t WitnessClass() const { return witness_.load(std::memory_order_relaxed); }
    void     NoteWitness(uint64_t packedName) const { witness_.store(packedName, std::memory_order_relaxed); }

    // True the first time a site asks the row for what its mode cannot give: the misuse is logged once per row.
    bool FirstMisuse() const { return !misused_.exchange(true, std::memory_order_relaxed); }

private:
    const char*     ident_;
    const char*     feature_;
    const wchar_t*  cls_;
    const wchar_t*  fn_;
    Mode            mode_;
    const Function* next_;
    mutable std::atomic<uint8_t> accepted_{0};
    mutable std::atomic<uint8_t> refused_{0};
    mutable std::atomic<uint64_t> witness_{0};
    mutable std::atomic<bool>     misused_{false};
};

// Every row, linked at DLL load in no meaningful order (a report sorts them).
const Function* First();
size_t Count();

// Numbers that move when the registration record gains a bit: RecordVersion on any (a kind accepted or refused a
// row for the first time), RefusalVersion on a refusal alone. A report taken earlier is out of date. Any thread.
uint64_t RecordVersion();
uint64_t RefusalVersion();

// A row's function, resolved. Only Resolve and ResolveOn make one, so a registration that takes it took
// a row. Valid while the function lives, which for a Blueprint class ends with its world: a site re-resolves
// after a world change, as it re-found the pointer before.
class Resolved {
public:
    Resolved() = default;
    void*           Fn() const { return fn_; }
    const Function* Row() const { return row_; }
    explicit operator bool() const { return fn_ != nullptr; }

private:
    friend Resolved Resolve(const Function& row);
    friend Resolved ResolveOn(const Function& row, void* cls);
    Resolved(void* fn, const Function* row) : fn_(fn), row_(row) {}
    void*           fn_ = nullptr;
    const Function* row_ = nullptr;
};

// A Declared or Dispatch row's function, on the game thread; empty when the class is not loaded, does not
// have the function, or the row is of another mode (logged once per row). The class is found as
// reflection::FindClass finds it, which walks the object array the first time and on every miss: a site
// that already looked its class up resolves with ResolveOn.
Resolved Resolve(const Function& row);

// A row's function on a class the caller already holds, by the row's mode: a RuntimeClass row's site
// found its class at run time (and the row records which, for the census); a site or the census that found
// a Declared or Dispatch row's class through the object index resolves it here without a second class
// lookup. Empty for an AnyClass row or when the class lacks it.
Resolved ResolveOn(const Function& row, void* cls);

}  // namespace ue_wrap::facts

#define UE_FACT_FN(ident, feature, cls, fn, mode)                                                          \
    namespace {                                                                                            \
    const ::ue_wrap::facts::Function ident{::ue_wrap::facts::detail::MacroToken{}, #ident, feature, cls, fn, \
                                           ::ue_wrap::facts::Mode::mode};                                  \
    }
