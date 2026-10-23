#include "ui/host_player_limit.h"

#include "coop/net/peer_limits.h"
#include "coop/config/config.h"
#include "ui/native_screen.h"
#include "ue_wrap/engine/engine.h"

#include <string>
#include <array>
#include <atomic>
#include <mutex>

namespace ui::host_player_limit {
namespace {
namespace NS = ui::native_screen;
namespace E = ue_wrap::engine;
void* g_less = nullptr;
void* g_more = nullptr;
void* g_label = nullptr;
constexpr std::array<int,5> kPresets{2,4,8,16,32};
std::array<void*,kPresets.size()> g_presets{};
std::atomic<int> g_players{0};
std::mutex g_selectionMutex;
bool g_savePending=false;

void Repaint() {
    const auto players=Value();
    const std::wstring label = std::to_wstring(players);
    E::SetWidgetText(g_label, label.c_str());
    E::SetWidgetRenderOpacity(g_less,players>coop::net::kMinPlayers?1.f:.35f);
    E::SetWidgetRenderOpacity(g_more,players<coop::net::kPeerCapacity?1.f:.35f);
    for(size_t i=0;i<g_presets.size();++i)E::SetWidgetRenderOpacity(g_presets[i],players==kPresets[i]?1.f:.65f);
}
}  // namespace

bool Build(void* row, void* buttonDonor) {
    ClearWidgets();
    g_less = NS::BuildButton(row, buttonDonor, L"-", NS::kBtnFontPx);
    g_label = NS::AddText(row, L"", 16, NS::Text(), NS::kJustCenter, 1.f);
    g_more = NS::BuildButton(row, buttonDonor, L"+", NS::kBtnFontPx);
    if (!g_less || !g_label || !g_more) {ClearWidgets();return false;}
    for(size_t i=0;i<g_presets.size();++i) {
        g_presets[i]=NS::BuildButton(row,buttonDonor,std::to_wstring(kPresets[i]).c_str(),16);
        if(!g_presets[i]){ClearWidgets();return false;}
        NS::SetHSlot(NS::SlotOf(g_presets[i]),0.f,NS::kRight,NS::kCenter);
    }
    NS::SetHSlot(NS::SlotOf(g_less), 0.f, NS::kLeft, NS::kCenter);
    NS::SetHSlot(NS::SlotOf(g_more), 0.f, NS::kRight, NS::kCenter);
    Repaint();
    return true;
}

bool HandleClick() {
    for(size_t i=0;i<g_presets.size();++i)if(g_presets[i] && E::WidgetIsHovered(g_presets[i])) {
        Select(kPresets[i]);Repaint();return true;
    }
    const bool less = g_less && E::WidgetIsHovered(g_less);
    const bool more = g_more && E::WidgetIsHovered(g_more);
    if (!less && !more) return false;
    if(less && more)return true;
    Select(Value()+(more?1:-1));
    Repaint();
    return true;
}

int Value() {
    int value=g_players.load(std::memory_order_acquire);if(value)return value;
    const int saved=coop::net::ClampPlayerLimit(static_cast<int>(coop::config::ResolveInt(coop::config_registry::rows::net_player_limit)));
    g_players.compare_exchange_strong(value,saved,std::memory_order_acq_rel);
    return value?value:saved;
}
void Select(int players,bool remember) {
    std::lock_guard<std::mutex> lock(g_selectionMutex);
    const int selected=coop::net::ClampPlayerLimit(players);
    if(g_players.exchange(selected,std::memory_order_acq_rel)!=selected)g_savePending=true;
    if(remember && g_savePending)
        g_savePending=coop::config::SetValue(coop::config_registry::rows::net_player_limit,std::to_string(selected).c_str())!=coop::config::SetResult::Saved;
}
void Remember(){Select(Value());}
void Refresh(){if(g_label)Repaint();}
void ClearWidgets() { g_less = g_more = g_label = nullptr;g_presets={}; }

}  // namespace ui::host_player_limit
