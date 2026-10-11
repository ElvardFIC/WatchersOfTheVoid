// ui/game_build_dialog.cpp -- see ui/game_build_dialog.h.

#include "ui/game_build_dialog.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"
#include "coop/game_build/game_build.h"
#include "coop/net/protocol.h"
#include "coop/version.h"
#include "l10n/l10n.h"
#include "ue_wrap/core/log.h"
#include "ui/menu_sfx.h"  // native VOTV button click + rollover sounds
#include "ui/scale.h"

#include "imgui.h"

#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <atomic>
#include <string>
#include <thread>

namespace ui::game_build_dialog {

using ui::scale::S;

namespace {

namespace GB = coop::game_build;

std::atomic<bool> g_open{false};
// [dev] game_build_dialog_continue: the first frame drawn answers Continue through the button's own handler.
std::atomic<bool> g_autoContinue{false};

void Continue() {
    GB::Acknowledge();
    g_open.store(false, std::memory_order_release);
    UE_LOGI("game_build_dialog: answered Continue anyway -- not asked again for this game build");
}

// The releases page, on a worker of its own: a browser's start can take a moment, and the frame that drew the
// button must not wait for it. ShellExecute wants COM initialized on its thread, apartment-threaded.
void OpenReleasesPage() {
    std::thread([] {
        const std::string url = std::string("https://") + coop::net::kReleasesUrl;
        const std::wstring wurl(url.begin(), url.end());  // ASCII
        const HRESULT com = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const INT_PTR r = reinterpret_cast<INT_PTR>(
            ::ShellExecuteW(nullptr, L"open", wurl.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        if (SUCCEEDED(com)) ::CoUninitialize();
        if (r <= 32)
            UE_LOGW("game_build_dialog: opening %s failed (ShellExecuteW %lld)", url.c_str(),
                    static_cast<long long>(r));
        else
            UE_LOGI("game_build_dialog: opened %s", url.c_str());
    }).detach();
}

// The two sentences of the body, in the player's language: what this Multivoid is made for and what runs,
// then the way out. One msgid per sentence, each argument once (the l10n grammar refuses an argument read twice).
// Returns false when a format was refused (the selftest's question).
bool ComposeBody(GB::Kind kind, const char* display, const char* version, unsigned lastBuild, std::string* out) {
    const std::string build = std::to_string(coop::net::kProtocolVersion);
    const char* target = coop::version::kGameTarget;
    char first[512] = {};
    char second[512] = {};
    int a = 0, b = 0;
    switch (kind) {
    case GB::Kind::Newer:
        a = l10n::Fmt(first, sizeof first,
                      l10n::T("This Multivoid (b%1$s) is made for VOTV %2$s. Your game is VOTV %3$s, a newer version."),
                      build.c_str(), target, display);
        b = l10n::Fmt(second, sizeof second,
                      l10n::T("Multiplayer may not work: you may not see other players, and some features may be "
                              "off. Look for a Multivoid build made for VOTV %1$s."),
                      version);
        break;
    case GB::Kind::Older:
        a = l10n::Fmt(first, sizeof first,
                      l10n::T("This Multivoid (b%1$s) is made for VOTV %2$s. Your game is VOTV %3$s, an older "
                              "version."),
                      build.c_str(), target, display);
        if (lastBuild > 0) {
            const std::string last = std::to_string(lastBuild);
            b = l10n::Fmt(second, sizeof second,
                          l10n::T("Update the game, or use Multivoid b%1$s, the last build for VOTV %2$s."),
                          last.c_str(), version);
        } else {
            b = l10n::Fmt(second, sizeof second, l10n::T("Update the game to VOTV %1$s."), target);
        }
        break;
    case GB::Kind::Untested:
        a = l10n::Fmt(first, sizeof first,
                      l10n::T("This Multivoid (b%1$s) is made for VOTV %2$s. Your game is a version it has not been "
                              "tested on."),
                      build.c_str(), target);
        b = l10n::Fmt(second, sizeof second, "%s",
                      l10n::T("Multiplayer may not work: you may not see other players, and some features may be "
                              "off. Check for a Multivoid update made for your game version."));
        break;
    case GB::Kind::Target:
        return true;  // never armed
    }
    *out = std::string(first) + "\n\n" + second;
    return a >= 0 && b >= 0;
}

std::string Body() {
    const GB::Detected& d = GB::Running();
    std::string body;
    ComposeBody(d.kind, d.row ? d.row->display : "", d.row ? d.row->version : "",
                d.row ? GB::LastBuildFor(d.row->version) : 0, &body);
    return body;
}

}  // namespace

void Arm() {
    if (!GB::AnswerOwed()) return;
    g_autoContinue.store(coop::config::ResolveFlag(coop::config_registry::rows::game_build_dialog_continue),
                         std::memory_order_relaxed);
    g_open.store(true, std::memory_order_release);
    UE_LOGI("game_build_dialog: armed -- the game running is %s", GB::Describe().c_str());
}

bool IsOpen() { return g_open.load(std::memory_order_relaxed); }

void Clear() { g_open.store(false, std::memory_order_release); }

void Render() {
    if (!g_open.load(std::memory_order_acquire)) return;
    ui::menu_sfx::FrameBegin();

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), io.DisplaySize, IM_COL32(0, 0, 0, 130));
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(18.0f), S(16.0f)));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.08f, 0.05f, 0.98f));

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoTitleBar;
    if (ImGui::Begin("###coop_game_build", nullptr, flags)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.78f, 0.35f, 1.0f));
        ImGui::TextUnformatted(l10n::T("UNTESTED GAME VERSION"));
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushTextWrapPos(S(460.0f));
        ImGui::TextWrapped("%s", Body().c_str());
        ImGui::Spacing();
        ImGui::TextDisabled(
            "%s", l10n::T("You can play anyway. This message will not show again for this game version."));
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        ImGui::Spacing();

        const float bw = S(190.0f);
        const float gap = S(12.0f);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 2.0f * bw - gap) * 0.5f);
        if (ui::menu_sfx::Button(l10n::Label(l10n::T("Check for an update"), "coop_gb_update"), ImVec2(bw, S(30.0f))))
            OpenReleasesPage();
        ImGui::SameLine(0.0f, gap);
        const bool autoAnswer = g_autoContinue.exchange(false, std::memory_order_relaxed);
        if (ui::menu_sfx::Button(l10n::Label(l10n::T("Continue anyway"), "coop_gb_continue"), ImVec2(bw, S(30.0f))) ||
            autoAnswer) {
            if (autoAnswer) UE_LOGI("game_build_dialog: [dev] game_build_dialog_continue answers for the player");
            Continue();
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    ui::menu_sfx::FrameEnd();
}

bool RunSelftest() {
    int checks = 0, failed = 0;
    auto ok = [&](bool cond, const char* what) {
        ++checks;
        if (!cond) {
            ++failed;
            UE_LOGE("game_build_dialog selftest FAIL: %s", what);
        }
    };
    std::string body;
    ***REMOVED***
       "the newer build's text formats");
    ok(ComposeBody(GB::Kind::Older, "0.8.2", "0.8.2", 150, &body) && body.find("b150") != std::string::npos,
       "the older build's text, with its last Multivoid build, formats and names it");
    ok(ComposeBody(GB::Kind::Older, "0.8.2", "0.8.2", 0, &body) && !body.empty(),
       "the older build's text without a last build formats");
    ok(ComposeBody(GB::Kind::Untested, "", "", 0, &body) && !body.empty(), "the untested build's text formats");
    if (failed == 0) UE_LOGI("game_build_dialog selftest: ALL PASS (%d checks)", checks);
    else             UE_LOGE("game_build_dialog selftest: %d/%d FAILED", failed, checks);
    return failed == 0;
}

}  // namespace ui::game_build_dialog
