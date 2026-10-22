#include "ui/host_player_limit.h"

#include "coop/net/peer_limits.h"
#include "ui/native_screen.h"
#include "ue_wrap/engine/engine.h"

#include <string>

namespace ui::host_player_limit {
namespace {
namespace NS = ui::native_screen;
namespace E = ue_wrap::engine;
void* g_less = nullptr;
void* g_more = nullptr;
void* g_label = nullptr;
int g_players = coop::net::kDefaultPlayers;

void Repaint() {
    const std::wstring label = L"Players: " + std::to_wstring(g_players) + L" / 32";
    E::SetWidgetText(g_label, label.c_str());
}
}  // namespace

bool Build(void* row, void* buttonDonor) {
    g_less = NS::BuildButton(row, buttonDonor, L"-", NS::kBtnFontPx);
    g_label = NS::AddText(row, L"", 16, NS::Text(), NS::kJustCenter, 1.f);
    g_more = NS::BuildButton(row, buttonDonor, L"+", NS::kBtnFontPx);
    if (!g_less || !g_label || !g_more) return false;
    NS::SetHSlot(NS::SlotOf(g_less), 0.f, NS::kLeft, NS::kCenter);
    NS::SetHSlot(NS::SlotOf(g_more), 0.f, NS::kRight, NS::kCenter);
    Repaint();
    return true;
}

bool HandleClick() {
    const bool less = g_less && E::WidgetIsHovered(g_less);
    const bool more = g_more && E::WidgetIsHovered(g_more);
    if (!less && !more) return false;
    g_players = coop::net::ClampPlayerLimit(g_players + (more ? 1 : -1));
    Repaint();
    return true;
}

int Value() { return g_players; }
void ClearWidgets() { g_less = g_more = g_label = nullptr; }

}  // namespace ui::host_player_limit
