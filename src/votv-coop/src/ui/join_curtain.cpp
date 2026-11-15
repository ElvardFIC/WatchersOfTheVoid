// ui/join_curtain.cpp -- see ui/join_curtain.h. The instant-world SHORT curtain.

#include "ui/join_curtain.h"

#include "coop/config/config.h"
#include "coop/config/config_registry.h"

#include "imgui.h"

#include <atomic>
#include <chrono>

namespace coop::join_curtain {
namespace {

// State is WRITTEN on the game thread (Show/BeginDismiss/Reset are called from the event_feed net-drain /
// join lifecycle) and READ on the RENDER thread (Render is the DX Present hook) -- so it is atomic, exactly
// like join_progress's phase. BeginDismiss only RECORDS the dismissal (Up -> DismissRequested): the fade clock
// is ImGui's, and the game thread has no ImGui context before the first presented frame, so Render, inside a
// frame, owns the two transitions that read it: DismissRequested -> Fading (it stamps g_fadeStart first, then
// publishes the state with release) and Fading -> Hidden once the fade is done. Both are compare-exchanges: a
// Show or Reset (plain stores, game thread) between Render's read and its exchange makes the exchange fail,
// and the newer state stands.
enum class State { Hidden, Up, DismissRequested, Fading };
std::atomic<int>    g_state{static_cast<int>(State::Hidden)};
std::atomic<double> g_fadeStart{0.0};
std::atomic<uint64_t> g_passes{0};

double Now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

constexpr float kFadeSeconds = 0.40f;  // smooth 1->0 dismiss (not a hard snap); the world fades in assembled

State Get() { return static_cast<State>(g_state.load(std::memory_order_acquire)); }

}  // namespace

void Show() {
    g_state.store(static_cast<int>(State::Up), std::memory_order_release);
}

void BeginDismiss() {
    // Only from a raised cover; ignore a double-dismiss / not-shown. No ImGui call here: this runs on the game
    // thread, where a window that has never presented has no context (ImGui::GetTime dereferences the null
    // context and faults the whole SnapshotComplete case). Render stamps the fade's start when it takes the
    // request.
    static const bool sClockRed =
        coop::config::ResolveFlag(::coop::config_registry::rows::curtain_dismiss_frame_clock_off);
    int expected = static_cast<int>(State::Up);
    // This runs on the game thread, including when no renderer/ImGui context
    // exists yet. Keep its clock independent of render-thread UI state.
    g_fadeStart.store(Now(), std::memory_order_relaxed);
    g_state.compare_exchange_strong(expected, static_cast<int>(State::Fading),
                                    std::memory_order_release, std::memory_order_relaxed);
}

void Reset() {
    g_state.store(static_cast<int>(State::Hidden), std::memory_order_release);
}

bool IsActive() {
    return Get() != State::Hidden;
}

bool AwaitingFade() {
    const State st = Get();
    return st == State::Up || st == State::DismissRequested;
}

uint64_t RenderPasses() {
    return g_passes.load(std::memory_order_acquire);
}

namespace {

void DrawCover() {
    State st = Get();
    if (st == State::Hidden) return;

    if (st == State::DismissRequested) {
        // Inside a frame, so the context exists. The start is stored before the state is published.
        g_fadeStart.store(ImGui::GetTime(), std::memory_order_relaxed);
        int expected = static_cast<int>(State::DismissRequested);
        g_state.compare_exchange_strong(expected, static_cast<int>(State::Fading),
                                        std::memory_order_release, std::memory_order_relaxed);
        st = Get();  // Fading, or what a Show/Reset made of it in between
        if (st == State::Hidden) return;
    }

    float alpha = 1.0f;
    if (st == State::Fading) {
        const double start = g_fadeStart.load(std::memory_order_relaxed);
        const float t = static_cast<float>(Now() - start) / kFadeSeconds;
        if (t >= 1.0f) return;  // fade complete -> draw nothing (Render is read-only; state stays Fading
                                // until the next Show()/Reset() on the game thread -- harmless, IsActive
                                // is not load-bearing).
        alpha = 1.0f - (t < 0.0f ? 0.0f : t);
    }

    // Full-viewport black cover on the BACKGROUND draw list: ON TOP of the game world, BEHIND the loading
    // panel (an ImGui window). At alpha=1 the world is fully hidden; during the fade the already-assembled
    // world bleeds through smoothly.
    const ImGuiIO& io = ImGui::GetIO();
    const ImU32 col = IM_COL32(0, 0, 0, static_cast<int>(alpha * 255.0f + 0.5f));
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), io.DisplaySize, col);
}

}  // namespace

void Render() {
    DrawCover();
    g_passes.fetch_add(1, std::memory_order_release);  // after the pass: a reader of the count sees its effect
}

}  // namespace coop::join_curtain
