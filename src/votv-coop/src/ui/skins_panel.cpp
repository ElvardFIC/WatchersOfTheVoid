// ui/skins_panel.cpp -- see ui/skins_panel.h.

#include "ui/skins_panel.h"

#include "coop/player/local_body.h"
#include "coop/player/model_folder.h"  // the players' models folder, its import and its pak details
#include "coop/player/skin_registry.h"
#include "coop/player/skin_preview.h"  // the live in-world mannequin preview on hover
#include "ui/overlay_backend.h"
#include "ui/scale.h"

#include <map>
#include <string>

#include "imgui.h"

namespace ui::skins_panel {
namespace {

struct Preview {
    void* srv = nullptr;  // ImTextureID (device-owned; dies with the device)
    int   w = 0, h = 0;
    bool  tried = false;  // one decode attempt per file per rescan
};

std::map<std::string, Preview> g_previews;
bool g_scanned = false;

constexpr float kTileW = 112.f;   // tile content width (1080p-authored; S() at use)
constexpr float kTileImgH = 112.f;

const Preview& PreviewFor(const coop::skins::SkinEntry& e) {
    Preview& p = g_previews[e.name];
    if (!p.tried) {
        p.tried = true;
        if (!e.previewPath.empty())
            p.srv = ui::overlay_backend::CreateTextureFromImageFile(e.previewPath.c_str(), &p.w, &p.h);
    }
    return p;
}

}  // namespace

// The catalog is OURS (render thread); the verdict is the game thread's. Hand it a COPY of
// the names -- never the vector -- so neither thread can see the other's storage move.
void PublishNamesForResolve(const std::vector<coop::skins::SkinEntry>& entries) {
    std::vector<std::string> names;
    names.reserve(entries.size());
    for (const auto& e : entries) names.push_back(e.name);
    coop::skins::RequestUsabilityScan(std::move(names));
}

void Render() {
    if (!g_scanned) {
        g_scanned = true;
        PublishNamesForResolve(coop::skins::Entries(true));
    }
    const auto& entries = coop::skins::Entries();
    const std::string current = coop::local_body::LocalSkinNameCopy();

    ImGui::TextWrapped("Your body skin -- what YOU see looking down and what OTHERS see. "
                       "Saved to multivoid.ini; restored on rejoin.");
    ImGui::TextDisabled("Skins come from converter .pak files under Content/Paks/LogicMods "
                        "(any subfolder). One pak may carry several skins -- the four "
                        "scientists ship together in scientists.pak.");
    ImGui::TextDisabled("Preview tile = <skin name>.png/.bmp beside the pak; its title, one "
                        "line in <skin name>.txt.");
    ImGui::TextDisabled("Peers WITHOUT that pak see the default kel body instead. Hover a tile "
                        "for its fingerprint: the same name and fingerprint means the same file.");
    ImGui::TextDisabled("A pak that carries no skin (another mod's) is dropped from this "
                        "list once the game has been asked -- it is not a skin.");

    const coop::model_folder::Report folder = coop::model_folder::LastReport();
    if (!folder.folder.empty()) {
        ImGui::Separator();
        ImGui::TextUnformatted("Models folder (MultiModel): put your skin paks here.");
        if (ImGui::Button("Open models folder")) coop::model_folder::OpenInExplorer(folder.folder);
        if (!folder.inbox.empty()) {
            ImGui::SameLine();
            if (ImGui::Button("Open import folder")) coop::model_folder::OpenInExplorer(folder.inbox);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("A pak put here is copied into the models folder at the game's "
                                  "start, and survives a mod manager's update.");
        }
        if (!folder.imported.empty()) {
            std::string names;
            for (const std::string& n : folder.imported) names += (names.empty() ? "" : ", ") + n;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 1.0f, 0.6f, 1.0f));
            ImGui::TextWrapped("Imported at this start: %s. Not listed yet? Restart the game: a pak "
                               "is mounted when the game starts.", names.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.35f, 1.0f));
        for (const std::string& n : folder.notes) ImGui::TextWrapped("%s", n.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
    }
    if (ImGui::Button("Refresh list")) {
        PublishNamesForResolve(coop::skins::Entries(true));
        // Release the cached preview textures before dropping the map -- clearing
        // alone leaks one texture+SRV per preview per refresh.
        for (auto& [name, pv] : g_previews)
            ui::overlay_backend::DestroyTexture(pv.srv);
        g_previews.clear();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("current: %s", current.c_str());
    ImGui::Separator();

    // gmod-style tile grid: as many tiles per row as the pane fits.
    using ui::scale::S;
    const float tileW = S(kTileW), tileImgH = S(kTileImgH);
    const float availW = ImGui::GetContentRegionAvail().x;
    const float cellW = tileW + ImGui::GetStyle().ItemSpacing.x;
    int perRow = static_cast<int>(availW / cellW);
    if (perRow < 1) perRow = 1;

    int i = 0;
    std::string hoveredName;  // the tile under the cursor -> live mannequin preview
    for (const auto& e : entries) {
        // A PAK IS NOT A SKIN. The scan lists the stem of every .pak under LogicMods, so
        // another mod's pak lands here and resolves to no mesh anywhere; offering it is how
        // a player picked one and diverged every peer to the native kel. `Unknown` stays
        // VISIBLE on purpose -- it means the game thread has not been asked yet (a couple of
        // entries per tick), not that the skin is bad, and hiding it would make the grid
        // shuffle while the player looks at it.
        if (coop::skins::UsabilityOf(e.name) == coop::skins::Usable::No) continue;
        if (i % perRow != 0) ImGui::SameLine();
        ++i;
        ImGui::PushID(e.name.c_str());
        ImGui::BeginGroup();

        const bool isCurrent = (e.name == current);
        const Preview& pv = PreviewFor(e);
        if (isCurrent) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.42f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.50f, 0.24f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.58f, 0.28f, 1.0f));
        }
        bool clicked;
        if (pv.srv) {
            // Fit the image into the tile box preserving aspect.
            float iw = tileW, ih = tileImgH;
            if (pv.w > 0 && pv.h > 0) {
                const float s = (tileW / pv.w < tileImgH / pv.h) ? tileW / pv.w
                                                                  : tileImgH / pv.h;
                iw = pv.w * s; ih = pv.h * s;
            }
            // ImTextureID is ImU64 since 1.91.4 -- pointer goes through uintptr_t.
            clicked = ImGui::ImageButton("##tile",
                                         static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(pv.srv)),
                                         ImVec2(iw, ih));
        } else {
            clicked = ImGui::Button(e.name == coop::skins::kNativeSkinName ? "Dr. Kel\n(default)"
                                                                           : "(no preview)",
                                    ImVec2(tileW, tileImgH));
        }
        // Hover -> live mannequin preview of this skin, and the file it comes from.
        if (ImGui::IsItemHovered()) {
            hoveredName = e.name;
            if (!e.pakPath.empty()) {
                const coop::model_folder::PakInfo& info = coop::model_folder::InfoFor(e.pakPath);
                ImGui::SetTooltip("%s\n%.1f MB, fingerprint %s", e.name.c_str(),
                                  static_cast<double>(info.size) / (1024.0 * 1024.0),
                                  info.fingerprint.empty() ? "?" : info.fingerprint.c_str());
            }
        }
        if (isCurrent) ImGui::PopStyleColor(3);
        // Name line under the tile, clipped to the tile width: the title when the pak has one.
        const std::string& label = e.title.empty() ? e.name : e.title;
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + tileW);
        if (isCurrent) ImGui::TextColored(ImVec4(0.55f, 1.0f, 0.6f, 1.0f), "%s", label.c_str());
        else           ImGui::TextUnformatted(label.c_str());
        ImGui::PopTextWrapPos();

        if (clicked && !isCurrent) coop::local_body::RequestSkin(e.name);
        ImGui::EndGroup();
        ImGui::PopID();
    }
    // The live in-world mannequin preview -- show the hovered skin, else the
    // current one. Runs every frame this section is visible; skin_preview::Tick
    // (game thread) spawns/despawns the mannequin by this keep-alive.
    coop::skin_preview::Activate(hoveredName.empty() ? current : hoveredName);
}

}  // namespace ui::skins_panel
