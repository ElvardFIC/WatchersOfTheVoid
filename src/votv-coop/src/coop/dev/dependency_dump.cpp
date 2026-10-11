// coop/dev/dependency_dump.cpp -- see coop/dev/dependency_dump.h.

#include "coop/dev/dependency_dump.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/paths.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_fingerprint.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/core/ufunction_hook.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

namespace coop::dev::dependency_dump {
namespace {

namespace FP = ue_wrap::script_fingerprint;

std::string Narrow(const wchar_t* s) {
    std::string out;
    for (; s && *s; ++s) out += (*s < 0x80) ? static_cast<char>(*s) : '?';
    return out;
}

// A registration can outlive the class a world change re-created (prop_record_refresh re-creates 127 in a
// same-process rejoin), and PathOf reads the object unguarded: a stale pointer is named as one, not read.
std::string PathOfLive(void* fn) {
    return ue_wrap::reflection::IsLive(fn) ? FP::PathOf(fn) : std::string("(stale function)");
}

void OnWatch(void* ctx, void* fn, const wchar_t* cls, const wchar_t* name, int tag) {
    auto& lines = *static_cast<std::map<std::string, int>*>(ctx);
    const std::string what = fn ? "script_gate exact " + PathOfLive(fn)
                                : "script_gate name " + (cls ? Narrow(cls) : std::string("*")) + "." + Narrow(name);
    ++lines[what + " tag=" + std::to_string(tag)];
}

void OnHook(void* ctx, void* fn, bool pre) {
    ++(*static_cast<std::map<std::string, int>*>(ctx))[std::string("ufunction_hook ") + (pre ? "pre " : "post ") +
                                                        PathOfLive(fn)];
}

void OnRegistration(void* ctx, void* fn, int kind) {
    static const char* kKinds[] = {"interceptor ", "post-observer ", "pre-observer "};
    ++(*static_cast<std::map<std::string, int>*>(ctx))[std::string("game_thread ") + kKinds[kind] + PathOfLive(fn)];
}

}  // namespace

void OnMilestone(const char* milestone) {
    if (!milestone || (std::strcmp(milestone, "joined") != 0 && std::strcmp(milestone, "peer-world-ready") != 0))
        return;
    if (!coop::config::ResolveFlag(::coop::config_registry::rows::dependency_dump)) return;
    std::map<std::string, int> lines;  // sorted, each with its count
    ue_wrap::script_gate::DebugForEachWatch(&OnWatch, &lines);
    ue_wrap::ufunction_hook::DebugForEachHook(&OnHook, &lines);
    ue_wrap::game_thread::DebugForEachRegistration(&OnRegistration, &lines);
    const std::wstring path = ue_wrap::paths::ExeDir() + L"\\multivoid-dependency-dump-" +
                              std::wstring(milestone, milestone + std::strlen(milestone)) + L".txt";
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) {
        UE_LOGE("dependency-dump: cannot open '%ls'", path.c_str());
        return;
    }
    for (const auto& [line, count] : lines) std::fprintf(f, "%s x%d\n", line.c_str(), count);
    std::fclose(f);
    UE_LOGW("dependency-dump: %zu distinct registration(s) at %s -> '%ls'", lines.size(), milestone, path.c_str());
}

}  // namespace coop::dev::dependency_dump
