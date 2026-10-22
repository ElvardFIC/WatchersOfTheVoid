#include "../../src/votv-coop/src/ue_wrap/core/script_gate_registry.cpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

namespace ue_wrap::script_gate::detail {
Entry g_nameTable[kSlots];
std::atomic<int> g_nameWatches{0};
std::atomic<int> g_unjudged{0};
int g_nameKeyed = 0;
}

int main() {
    using namespace ue_wrap::script_gate;
    using namespace ue_wrap::script_gate::detail;
    constexpr int watches = 1100;
    std::vector<std::wstring> names;
    names.reserve(watches);
    for (int i = 0; i < watches; ++i) names.push_back(L"world-verb-" + std::to_wstring(i));
    auto callback = +[](const Call&) { return Verdict::Run; };
    for (int i = 0; i < watches; ++i) {
        const auto key = reinterpret_cast<uintptr_t>(names[i].c_str()) | 1u;
        CHECK(Register(g_nameTable, g_nameWatches, g_nameKeyed, key, i,
                       callback, nullptr, names[i].c_str(), nullptr, 0, false));
    }
    for (int i = 0; i < watches; ++i) {
        const auto pending = reinterpret_cast<uintptr_t>(names[i].c_str()) | 1u;
        CHECK(Retire(g_nameTable, g_nameWatches, pending, i, callback, nullptr, names[i].c_str()));
        CHECK(Register(g_nameTable, g_nameWatches, g_nameKeyed, uint64_t(i + 1) << 32, i,
                       callback, nullptr, names[i].c_str(), nullptr, 0, true));
    }
    CHECK(g_nameWatches == watches);
    CHECK(g_nameKeyed == 2 * watches);
    // Reopening a world must reuse resolved registrations instead of spending more slots.
    for (int pass = 0; pass < 8; ++pass) {
        for (int i = 0; i < watches; ++i) {
            const auto key = uint64_t(i + 1) << 32;
            CHECK(Retire(g_nameTable, g_nameWatches, key, i, callback, nullptr, names[i].c_str()));
            CHECK(Register(g_nameTable, g_nameWatches, g_nameKeyed, key, i,
                           callback, nullptr, names[i].c_str(), nullptr, 0, true));
        }
        CHECK(g_nameWatches == watches);
        CHECK(g_nameKeyed == 2 * watches);
    }
    std::puts("PASS production watch registry: 1100 pending/resolved verbs and eight reopens without lost capacity");
}
