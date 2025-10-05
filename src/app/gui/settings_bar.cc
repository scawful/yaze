#include "app/gui/settings_bar.h"

#include "absl/strings/str_format.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/ui_helpers.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

void SettingsBar::SetRomVersion(uint8_t version) {
  rom_version_ = version;
}

void SettingsBar::BeginDraw() {
  // Compact, modern settings bar
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 6));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
  
  // Single row layout using columns for alignment
  ImGui::BeginGroup();
  in_settings_bar_ = true;
  current_column_ = 0;
}

void SettingsBar::EndDraw() {
  ImGui::EndGroup();
  ImGui::PopStyleVar(2);
  ImGui::Separator();
  in_settings_bar_ = false;
}

void SettingsBar::AddVersionBadge(std::function<void()> on_upgrade) {
  if (IsVanilla()) {
    RomVersionBadge("Vanilla ROM", true);
    
    if (on_upgrade) {
      ImGui::SameLine();
      if (IconButton(ICON_MD_UPGRADE, "Upgrade to v3", ImVec2(0, 0))) {
        on_upgrade();
      }
    }
  } else {
    std::string version_text = absl::StrFormat("v%d", rom_version_);
    RomVersionBadge(version_text.c_str(), false);
    
    if (rom_version_ < 3 && on_upgrade) {
      ImGui::SameLine();
      if (IconButton(ICON_MD_UPGRADE, "Upgrade to v3", ImVec2(0, 0))) {
        on_upgrade();
      }
    }
  }
  
  ImGui::SameLine();
  AddSeparator();
}

void SettingsBar::AddWorldSelector(int* current_world) {
  ImGui::Text(ICON_MD_PUBLIC);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(120);
  
  const char* worlds[] = {"Light World", "Dark World", "Extra World"};
  ImGui::Combo("##world", current_world, worlds, 3);
  
  ImGui::SameLine();
}

void SettingsBar::AddHexByteInput(const char* icon, const char* label,
                                 uint8_t* value,
                                 std::function<void()> on_change) {
  ImGui::Text("%s", icon);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(60);
  
  if (InputHexByte(label, value)) {
    if (on_change) on_change();
  }
  
  ImGui::SameLine();
}

void SettingsBar::AddHexWordInput(const char* icon, const char* label,
                                 uint16_t* value,
                                 std::function<void()> on_change) {
  ImGui::Text("%s", icon);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80);
  
  if (InputHexWord(label, value)) {
    if (on_change) on_change();
  }
  
  ImGui::SameLine();
}

void SettingsBar::AddCheckbox(const char* icon, const char* label, bool* value,
                              std::function<void()> on_change) {
  ImGui::Text("%s", icon);
  ImGui::SameLine();
  
  if (ImGui::Checkbox(label, value)) {
    if (on_change) on_change();
  }
  
  ImGui::SameLine();
}

void SettingsBar::AddCombo(const char* icon, const char* label, int* current,
                          const char* const items[], int count,
                          std::function<void()> on_change) {
  ImGui::Text("%s", icon);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(120);
  
  if (ImGui::Combo(label, current, items, count)) {
    if (on_change) on_change();
  }
  
  ImGui::SameLine();
}

void SettingsBar::AddAreaSizeSelector(int* area_size,
                                      std::function<void()> on_change) {
  if (!IsV3()) return;
  
  const char* sizes[] = {"Small (1x1)", "Large (2x2)", "Wide (2x1)", "Tall (1x2)"};
  
  ImGui::Text(ICON_MD_ASPECT_RATIO);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(110);
  
  if (ImGui::Combo("##areasize", area_size, sizes, 4)) {
    if (on_change) on_change();
  }
  
  ImGui::SameLine();
}

void SettingsBar::AddButton(const char* icon, const char* label,
                           std::function<void()> callback) {
  if (IconButton(icon, label)) {
    if (callback) callback();
  }
  
  ImGui::SameLine();
}

void SettingsBar::AddSeparator() {
  ImGui::TextDisabled("|");
  ImGui::SameLine();
}

}  // namespace gui
}  // namespace yaze
