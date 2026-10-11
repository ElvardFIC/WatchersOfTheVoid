// harness/fingerprint_capture.cpp -- [dev] fingerprint_capture: every function of every loaded Blueprint class with
// its canonical text, the input of the agreement test against the cooked assets and the native half of a build's
// reference table (docs/bytecode-fingerprint.md, "The capture"). The census (fingerprint_census.cpp) calls it once
// its first pass has met the loaded classes.

#include "fingerprint_census_detail.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/paths.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_fingerprint.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/facts/function_row.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace harness::fingerprint_census::detail {
namespace {

namespace R = ue_wrap::reflection;
namespace P = ue_wrap::profile;
namespace FP = ue_wrap::script_fingerprint;
namespace F = ue_wrap::facts;
namespace OI = ue_wrap::object_index;

}  // namespace

// Every function of every loaded Blueprint class with its canonical text, sorted by path so two captures
// diff line by line, and every row with what it resolves to (the native half of a reference table).
void Capture() {
    void* const bpClasses[] = {R::FindClass(L"BlueprintGeneratedClass"), R::FindClass(L"WidgetBlueprintGeneratedClass"),
                               R::FindClass(L"AnimBlueprintGeneratedClass")};
    void* const fnClasses[] = {R::FindClass(L"Function"), R::FindClass(L"DelegateFunction")};
    std::vector<std::pair<std::string, void*>> fns;
    const int32_t n = R::NumObjects();
    int classes = 0;
    for (int32_t i = 0; i < n; ++i) {
        void* obj = R::ObjectAt(i);
        if (!obj) continue;
        void* cls = R::ClassOf(obj);
        if (!cls || (cls != bpClasses[0] && cls != bpClasses[1] && cls != bpClasses[2])) continue;
        ++classes;
        void* field = *reinterpret_cast<void**>(static_cast<uint8_t*>(obj) + P::off::UStruct_Children);
        for (int guard = 0; field && guard < 4096;
             ++guard, field = *reinterpret_cast<void**>(static_cast<uint8_t*>(field) + P::off::UField_Next)) {
            void* fc = R::ClassOf(field);
            if (fc == fnClasses[0] || fc == fnClasses[1]) fns.emplace_back(FP::PathOf(field), field);
        }
    }
    // An any-class row's subjects, natives included: the reference's signature set is the union of the
    // pak's Blueprint declarations and these. The first pass has met every class loaded by then.
    ForEachLiveSubject([&fns](void* fn) { fns.emplace_back(FP::PathOf(fn), fn); });
    std::vector<std::string> rowLines;
    for (const F::Function* row = F::First(); row; row = row->Next()) {
        F::Resolved r;
        if (row->GetMode() != F::Mode::AnyClass) {
            const std::wstring witness = row->Class() ? std::wstring() : WitnessName(*row);
            if (void* cls = OI::ClassByName(row->Class() ? row->Class() : witness.c_str())) r = F::ResolveOn(*row, cls);
        }
        static const char* kModes[] = {"Declared", "Dispatch", "AnyClass", "RuntimeClass"};
        rowLines.push_back("#row " + std::string(row->Feature()) + " " + kModes[static_cast<int>(row->GetMode())] + " " +
                           Key(*row) + " -> " + (r ? FP::PathOf(r.Fn()) : std::string("-")));
        if (r) fns.emplace_back(FP::PathOf(r.Fn()), r.Fn());
    }
    std::sort(fns.begin(), fns.end());
    fns.erase(std::unique(fns.begin(), fns.end()), fns.end());
    std::sort(rowLines.begin(), rowLines.end());
    const std::wstring path = ue_wrap::paths::ExeDir() + L"\\multivoid-fingerprints-capture.txt";
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) {
        UE_LOGE("fingerprint-census: cannot open '%ls' for the capture", path.c_str());
        return;
    }
    for (const std::string& l : rowLines) std::fprintf(f, "%s\n", l.c_str());
    int ok = 0, problems = 0;
    for (const auto& [p, fn] : fns) {
        const FP::Result r = FP::Of(fn);
        if (r.ok) ++ok; else ++problems;
        std::fprintf(f, "### %s %016llx %s\n", p.c_str(), static_cast<unsigned long long>(r.hash),
                     r.ok ? (r.native ? "native" : "ok") : r.problem.c_str());
        std::fwrite(r.text.data(), 1, r.text.size(), f);
    }
    std::fclose(f);
    UE_LOGW("fingerprint-census: CAPTURE %zu function(s) (%d loaded Blueprint class(es) and %zu row(s)), %d decoded, "
            "%d with a problem -> '%ls'", fns.size(), classes, rowLines.size(), ok, problems, path.c_str());
}

}  // namespace harness::fingerprint_census::detail
