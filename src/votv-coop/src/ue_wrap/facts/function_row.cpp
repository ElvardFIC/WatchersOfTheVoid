// ue_wrap/facts/function_row.cpp -- see ue_wrap/facts/function_row.h.

#include "ue_wrap/facts/function_row.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"

namespace ue_wrap::facts {
namespace {

namespace R = ue_wrap::reflection;

// Constant-initialized, so every row's constructor can link itself at DLL load whatever the order of the
// translation units' initializers.
const Function* g_head = nullptr;
size_t g_count = 0;
std::atomic<uint64_t> g_refusals{0};

const char* ModeName(Mode m) {
    switch (m) {
    case Mode::Declared: return "Declared";
    case Mode::Dispatch: return "Dispatch";
    case Mode::AnyClass: return "AnyClass";
    case Mode::RuntimeClass: return "RuntimeClass";
    }
    return "?";
}

}  // namespace

Function::Function(detail::MacroToken, const char* ident, const char* feature, const wchar_t* cls, const wchar_t* fn,
                   Mode mode)
    : ident_(ident), feature_(feature), cls_(cls), fn_(fn), mode_(mode), next_(g_head) {
    g_head = this;
    ++g_count;
}

void Function::NoteUse(Use use, bool accepted) const {
    const uint8_t bit = static_cast<uint8_t>(use);
    const uint8_t was = (accepted ? accepted_ : refused_).fetch_or(bit, std::memory_order_relaxed);
    if (!accepted && !(was & bit)) g_refusals.fetch_add(1, std::memory_order_release);
}

const Function* First() { return g_head; }
size_t Count() { return g_count; }
uint64_t RefusalVersion() { return g_refusals.load(std::memory_order_acquire); }

Resolved Resolve(const Function& row) {
    if (row.GetMode() != Mode::Declared && row.GetMode() != Mode::Dispatch) {
        // One line per row, not one per call: a misuse is a code defect, fixed once.
        if (row.FirstMisuse())
            UE_LOGE("facts: Resolve asked for row %s of mode %s -- only Declared and Dispatch rows resolve so",
                    row.Ident(), ModeName(row.GetMode()));
        return {};
    }
    void* cls = row.Class() ? R::FindClass(row.Class()) : nullptr;
    if (!cls) return {};
    void* fn = nullptr;
    if (row.GetMode() == Mode::Declared) {
        fn = R::FindFunction(cls, row.Name());
    } else {
        void* declarer = nullptr;
        fn = R::FindDispatchFunction(cls, row.Name(), &declarer);
    }
    return fn ? Resolved(fn, &row) : Resolved{};
}

Resolved ResolveOn(const Function& row, void* cls) {
    if (row.GetMode() == Mode::AnyClass || !cls) return {};
    void* fn = nullptr;
    if (row.GetMode() == Mode::Dispatch) {
        void* declarer = nullptr;
        fn = R::FindDispatchFunction(cls, row.Name(), &declarer);
    } else {
        fn = R::FindFunction(cls, row.Name());
    }
    if (!fn) return {};
    if (row.GetMode() == Mode::RuntimeClass) {
        const R::FName& n = R::NameOf(cls);
        row.NoteWitness((static_cast<uint64_t>(static_cast<uint32_t>(n.ComparisonIndex)) << 32) |
                        static_cast<uint32_t>(n.Number));
    }
    return Resolved(fn, &row);
}

}  // namespace ue_wrap::facts
