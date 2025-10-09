#include "app/editor/ui/menu_manager.h"

#include "app/editor/editor_manager.h"
#include "app/gui/icons.h"
#include "absl/strings/str_format.h"

namespace yaze {
namespace editor {

MenuManager::MenuManager(EditorManager* editor_manager)
    : editor_manager_(editor_manager) {}

void MenuManager::BuildAndDraw() {
  if (ImGui::BeginMenuBar()) {
    editor_manager_->BuildModernMenu(); // This contains the menu_builder_ logic
    editor_manager_->menu_builder_.Draw();

    // This is the logic from the second half of DrawMenuBar
    auto* current_rom = editor_manager_->GetCurrentRom();
    std::string version_text = absl::StrFormat("v%s", editor_manager_->version().c_str());
    float version_width = ImGui::CalcTextSize(version_text.c_str()).x;
    float session_rom_area_width = 280.0f;
    ImGui::SameLine(ImGui::GetWindowWidth() - version_width - 10 - session_rom_area_width);

    if (editor_manager_->GetActiveSessionCount() > 1) {
        if (ImGui::SmallButton(absl::StrFormat("%s%zu", ICON_MD_TAB, editor_manager_->GetActiveSessionCount()).c_str())) {
            editor_manager_->show_session_switcher_ = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Sessions: %zu active\nClick to switch", editor_manager_->GetActiveSessionCount());
        }
        ImGui::SameLine();
    }

    if (current_rom && current_rom->is_loaded()) {
      if (ImGui::SmallButton(ICON_MD_APPS)) {
        editor_manager_->show_editor_selection_ = true;
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(ICON_MD_DASHBOARD " Editor Selection (Ctrl+E)");
      }
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_DISPLAY_SETTINGS)) {
        editor_manager_->popup_manager_->Show("Display Settings");
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(ICON_MD_TUNE " Display Settings");
      }
      ImGui::SameLine();
      ImGui::Separator();
      ImGui::SameLine();
    }

    if (current_rom && current_rom->is_loaded()) {
      std::string rom_display = current_rom->title();
      if (rom_display.length() > 22) {
        rom_display = rom_display.substr(0, 19) + "...";
      }
      ImVec4 status_color = current_rom->dirty() ? ImVec4(1.0f, 0.5f, 0.0f, 1.0f) : ImVec4(0.0f, 0.8f, 0.0f, 1.0f);
      if (ImGui::SmallButton(absl::StrFormat("%s%s", rom_display.c_str(), current_rom->dirty() ? "*" : "").c_str())) {
        ImGui::OpenPopup("ROM Details");
      }
      // ... (rest of the popup logic from DrawMenuBar)
    } else {
      ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No ROM");
      ImGui::SameLine();
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - version_width - 10);
    ImGui::Text("%s", version_text.c_str());
    ImGui::EndMenuBar();
  }
}

}  // namespace editor
}  // namespace yaze
