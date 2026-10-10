// harness/fingerprint_census.cpp -- see harness/fingerprint_census.h.

#include "harness/fingerprint_census.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/paths.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_fingerprint.h"
#include "ue_wrap/core/sdk_profile.h"

#include <cstdio>
#include <string>

namespace harness::fingerprint_census {
namespace {

namespace R = ue_wrap::reflection;
namespace P = ue_wrap::profile;
namespace FP = ue_wrap::script_fingerprint;

// Every function of every loaded Blueprint class, with its canonical text: one pass over the object
// array, a few thousand functions, once per boot and only with the dev flag on.
void Capture() {
    void* const bpClasses[] = {R::FindClass(L"BlueprintGeneratedClass"), R::FindClass(L"WidgetBlueprintGeneratedClass"),
                               R::FindClass(L"AnimBlueprintGeneratedClass")};
    void* const fnClasses[] = {R::FindClass(L"Function"), R::FindClass(L"DelegateFunction")};
    const std::wstring path = ue_wrap::paths::ExeDir() + L"\\multivoid-fingerprints-capture.txt";
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) {
        UE_LOGE("fingerprint-census: cannot open '%ls' for the capture", path.c_str());
        return;
    }
    int classes = 0, functions = 0, ok = 0, problems = 0;
    const int32_t n = R::NumObjects();
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
            if (fc != fnClasses[0] && fc != fnClasses[1]) continue;
            const FP::Result r = FP::Of(field);
            ++functions;
            if (r.ok) ++ok; else ++problems;
            std::fprintf(f, "### %s %016llx %s\n", FP::PathOf(field).c_str(), static_cast<unsigned long long>(r.hash),
                         r.ok ? (r.native ? "native" : "ok") : r.problem.c_str());
            std::fwrite(r.text.data(), 1, r.text.size(), f);
        }
    }
    std::fclose(f);
    UE_LOGW("fingerprint-census: CAPTURE %d function(s) of %d loaded Blueprint class(es), %d decoded, %d with a "
            "problem -> '%ls'", functions, classes, ok, problems, path.c_str());
}

}  // namespace

void Run() {
    if (coop::config::ResolveFlag(::coop::config_registry::rows::fingerprint_capture)) Capture();
}

}  // namespace harness::fingerprint_census
