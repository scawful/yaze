#include "app/editor/ui/dashboard_panel.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/editor/editor_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/platform_keys.h"
#include "app/gui/core/style.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "util/file_util.h"

namespace yaze {
namespace editor {

DashboardPanel::DashboardPanel(EditorManager* editor_manager)
    : editor_manager_(editor_manager),
      window_("Dashboard", ICON_MD_DASHBOARD) {
  window_.SetDefaultSize(950, 650);
  window_.SetPosition(gui::PanelWindow::Position::Center);

  // Initialize editor list with colors matching EditorSelectionDialog
  // Use platform-aware shortcut strings (Cmd on macOS, Ctrl elsewhere)
  const char* ctrl = gui::GetCtrlDisplayName();
  editors_ = {
      {"Overworld", ICON_MD_MAP, "Edit overworld maps, entrances, and properties",
       absl::StrFormat("%s+1", ctrl), EditorType::kOverworld,
       ImVec4(0.133f, 0.545f, 0.133f, 1.0f)}, // Hyrule green
      {"Dungeon", ICON_MD_CASTLE, "Design dungeon rooms, layouts, and mechanics",
       absl::StrFormat("%s+2", ctrl), EditorType::kDungeon,
       ImVec4(0.502f, 0.0f, 0.502f, 1.0f)}, // Ganon purple
      {"Graphics", ICON_MD_PALETTE, "Modify tiles, palettes, and graphics sets",
       absl::StrFormat("%s+3", ctrl), EditorType::kGraphics,
       ImVec4(1.0f, 0.843f, 0.0f, 1.0f)}, // Triforce gold
      {"Sprites", ICON_MD_EMOJI_EMOTIONS, "Edit sprite graphics and properties",
       absl::StrFormat("%s+4", ctrl), EditorType::kSprite,
       ImVec4(1.0f, 0.647f, 0.0f, 1.0f)}, // Spirit orange
      {"Messages", ICON_MD_CHAT_BUBBLE, "Edit dialogue, signs, and text",
       absl::StrFormat("%s+5", ctrl), EditorType::kMessage,
       ImVec4(0.196f, 0.6f, 0.8f, 1.0f)}, // Master sword blue
      {"Music", ICON_MD_MUSIC_NOTE, "Configure music and sound effects",
       absl::StrFormat("%s+6", ctrl), EditorType::kMusic,
       ImVec4(0.416f, 0.353f, 0.804f, 1.0f)}, // Shadow purple
      {"Palettes", ICON_MD_COLOR_LENS, "Edit color palettes and animations",
       absl::StrFormat("%s+7", ctrl), EditorType::kPalette,
       ImVec4(0.863f, 0.078f, 0.235f, 1.0f)}, // Heart red
      {"Screens", ICON_MD_TV, "Edit title screen and ending screens",
       absl::StrFormat("%s+8", ctrl), EditorType::kScreen,
       ImVec4(0.4f, 0.8f, 1.0f, 1.0f)}, // Sky blue
      {"Assembly", ICON_MD_CODE, "Write and edit assembly code",
       absl::StrFormat("%s+9", ctrl), EditorType::kAssembly,
       ImVec4(0.8f, 0.8f, 0.8f, 1.0f)}, // Silver
      {"Hex Editor", ICON_MD_DATA_ARRAY, "Direct ROM memory editing and comparison",
       absl::StrFormat("%s+0", ctrl), EditorType::kHex,
       ImVec4(0.2f, 0.8f, 0.4f, 1.0f)}, // Matrix green
      {"Emulator", ICON_MD_VIDEOGAME_ASSET, "Test and debug your ROM in real-time",
       absl::StrFormat("%s+Shift+E", ctrl), EditorType::kEmulator,
       ImVec4(0.2f, 0.6f, 1.0f, 1.0f)}, // Emulator blue
      {"AI Agent", ICON_MD_SMART_TOY, "Configure AI agent, collaboration, and automation",
       absl::StrFormat("%s+Shift+A", ctrl), EditorType::kAgent,
       ImVec4(0.8f, 0.4f, 1.0f, 1.0f)}, // Purple/magenta
  };

  LoadRecentEditors();
}

void DashboardPanel::Draw() {
  if (!show_) return;

  // Set window properties immediately before Begin
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(950, 650), ImGuiCond_Appearing);

  if (window_.Begin(&show_)) {
    DrawWelcomeHeader();
    ImGui::Separator();
    ImGui::Spacing();
    
    DrawRecentEditors();
    if (!recent_editors_.empty()) {
      ImGui::Separator();
      ImGui::Spacing();
    }
    
    DrawEditorGrid();
  }
  window_.End();
}

void DashboardPanel::DrawWelcomeHeader() {
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);  // Large font
  ImVec4 title_color = ImVec4(1.0f, 0.843f, 0.0f, 1.0f);  // Triforce gold
  ImGui::TextColored(title_color, ICON_MD_EDIT " Select an Editor");
  ImGui::PopFont();

  ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
                     "Choose an editor to begin working on your ROM. "
                     "You can open multiple editors simultaneously.");
}

void DashboardPanel::DrawRecentEditors() {
  if (recent_editors_.empty()) return;

  ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f),
                     ICON_MD_HISTORY " Recently Used");
  ImGui::Spacing();

  for (EditorType type : recent_editors_) {
    // Find editor info
    auto it = std::find_if(
        editors_.begin(), editors_.end(),
        [type](const EditorInfo& info) { return info.type == type; });

    if (it != editors_.end()) {
      // Use editor's theme color for button
      ImVec4 color = it->color;
      ImGui::PushStyleColor(
          ImGuiCol_Button,
          ImVec4(color.x * 0.5f, color.y * 0.5f, color.z * 0.5f, 0.7f));
      ImGui::PushStyleColor(
          ImGuiCol_ButtonHovered,
          ImVec4(color.x * 0.7f, color.y * 0.7f, color.z * 0.7f, 0.9f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);

      if (ImGui::Button(absl::StrCat(it->icon, " ", it->name).c_str(),
                        ImVec2(150, 35))) {
        if (editor_manager_) {
          MarkRecentlyUsed(type);
          editor_manager_->SwitchToEditor(type);
          show_ = false;
        }
      }

      ImGui::PopStyleColor(3);

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", it->description.c_str());
      }

      ImGui::SameLine();
    }
  }

  ImGui::NewLine();
}

void DashboardPanel::DrawEditorGrid() {
  ImGui::Text(ICON_MD_APPS " All Editors");
  ImGui::Spacing();

  const float card_width = 180.0f;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float window_width = ImGui::GetContentRegionAvail().x;
  int columns = static_cast<int>(window_width / (card_width + spacing));
  columns = std::max(columns, 1);

  if (ImGui::BeginTable("EditorGrid", columns)) {
    for (size_t i = 0; i < editors_.size(); ++i) {
      ImGui::TableNextColumn();
      DrawEditorPanel(editors_[i], static_cast<int>(i));
    }
    ImGui::EndTable();
  }
}

void DashboardPanel::DrawEditorPanel(const EditorInfo& info, int index) {
  ImGui::PushID(index);

  ImVec2 button_size(180, 120);
  ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  bool is_recent = std::find(recent_editors_.begin(), recent_editors_.end(),
                             info.type) != recent_editors_.end();

  // Create gradient background
  ImVec4 base_color = info.color;
  ImU32 color_top = ImGui::GetColorU32(ImVec4(
      base_color.x * 0.4f, base_color.y * 0.4f, base_color.z * 0.4f, 0.8f));
  ImU32 color_bottom = ImGui::GetColorU32(ImVec4(
      base_color.x * 0.2f, base_color.y * 0.2f, base_color.z * 0.2f, 0.9f));

  // Draw gradient card background
  draw_list->AddRectFilledMultiColor(
      cursor_pos,
      ImVec2(cursor_pos.x + button_size.x, cursor_pos.y + button_size.y),
      color_top, color_top, color_bottom, color_bottom);

  // Colored border
  ImU32 border_color =
      is_recent
          ? ImGui::GetColorU32(
                ImVec4(base_color.x, base_color.y, base_color.z, 1.0f))
          : ImGui::GetColorU32(ImVec4(base_color.x * 0.6f, base_color.y * 0.6f,
                                      base_color.z * 0.6f, 0.7f));
  draw_list->AddRect(
      cursor_pos,
      ImVec2(cursor_pos.x + button_size.x, cursor_pos.y + button_size.y),
      border_color, 4.0f, 0, is_recent ? 3.0f : 2.0f);

  // Recent indicator badge
  if (is_recent) {
    ImVec2 badge_pos(cursor_pos.x + button_size.x - 25, cursor_pos.y + 5);
    draw_list->AddCircleFilled(badge_pos, 12, ImGui::GetColorU32(base_color),
                               16);
    ImGui::SetCursorScreenPos(ImVec2(badge_pos.x - 6, badge_pos.y - 8));
    ImGui::TextColored(ImVec4(1, 1, 1, 1), ICON_MD_STAR);
  }

  // Make button transparent (we draw our own background)
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(base_color.x * 0.3f, base_color.y * 0.3f,
                               base_color.z * 0.3f, 0.5f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ImVec4(base_color.x * 0.5f, base_color.y * 0.5f,
                               base_color.z * 0.5f, 0.7f));

  ImGui::SetCursorScreenPos(cursor_pos);
  bool clicked =
      ImGui::Button(absl::StrCat("##", info.name).c_str(), button_size);
  bool is_hovered = ImGui::IsItemHovered();

  ImGui::PopStyleColor(3);

  // Draw icon with colored background circle
  ImVec2 icon_center(cursor_pos.x + button_size.x / 2, cursor_pos.y + 30);
  ImU32 icon_bg = ImGui::GetColorU32(base_color);
  draw_list->AddCircleFilled(icon_center, 22, icon_bg, 32);

  // Draw icon
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);  // Larger font for icon
  ImVec2 icon_size = ImGui::CalcTextSize(info.icon.c_str());
  ImGui::SetCursorScreenPos(
      ImVec2(icon_center.x - icon_size.x / 2, icon_center.y - icon_size.y / 2));
  ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", info.icon.c_str());
  ImGui::PopFont();

  // Draw name
  ImGui::SetCursorScreenPos(ImVec2(cursor_pos.x + 10, cursor_pos.y + 65));
  ImGui::PushTextWrapPos(cursor_pos.x + button_size.x - 10);
  ImVec2 name_size = ImGui::CalcTextSize(info.name.c_str());
  ImGui::SetCursorScreenPos(ImVec2(
      cursor_pos.x + (button_size.x - name_size.x) / 2, cursor_pos.y + 65));
  ImGui::TextColored(base_color, "%s", info.name.c_str());
  ImGui::PopTextWrapPos();

  // Draw shortcut hint if available
  if (!info.shortcut.empty()) {
    ImGui::SetCursorScreenPos(
        ImVec2(cursor_pos.x + 10, cursor_pos.y + button_size.y - 20));
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", info.shortcut.c_str());
  }

  // Hover glow effect
  if (is_hovered) {
    ImU32 glow_color = ImGui::GetColorU32(
        ImVec4(base_color.x, base_color.y, base_color.z, 0.2f));
    draw_list->AddRectFilled(
        cursor_pos,
        ImVec2(cursor_pos.x + button_size.x, cursor_pos.y + button_size.y),
        glow_color, 4.0f);
  }

  // Enhanced tooltip
  if (is_hovered) {
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);
    ImGui::BeginTooltip();
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);  // Medium font
    ImGui::TextColored(base_color, "%s %s", info.icon.c_str(), info.name.c_str());
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 280);
    ImGui::TextWrapped("%s", info.description.c_str());
    ImGui::PopTextWrapPos();
    if (!info.shortcut.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(base_color, ICON_MD_KEYBOARD " %s", info.shortcut.c_str());
    }
    if (is_recent) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f),
                         ICON_MD_STAR " Recently used");
    }
    ImGui::EndTooltip();
  }

  if (clicked) {
    if (editor_manager_) {
      MarkRecentlyUsed(info.type);
      editor_manager_->SwitchToEditor(info.type);
      show_ = false;
    }
  }

  ImGui::PopID();
}

void DashboardPanel::MarkRecentlyUsed(EditorType type) {
  // Remove if already in list
  auto it = std::find(recent_editors_.begin(), recent_editors_.end(), type);
  if (it != recent_editors_.end()) {
    recent_editors_.erase(it);
  }

  // Add to front
  recent_editors_.insert(recent_editors_.begin(), type);

  // Limit size
  if (recent_editors_.size() > kMaxRecentEditors) {
    recent_editors_.resize(kMaxRecentEditors);
  }

  SaveRecentEditors();
}

void DashboardPanel::LoadRecentEditors() {
  try {
    auto data = util::LoadFileFromConfigDir("recent_editors.txt");
    if (!data.empty()) {
      std::istringstream ss(data);
      std::string line;
      while (std::getline(ss, line) &&
             recent_editors_.size() < kMaxRecentEditors) {
        int type_int = std::stoi(line);
        if (type_int >= 0 &&
            type_int < static_cast<int>(EditorType::kSettings)) {
          recent_editors_.push_back(static_cast<EditorType>(type_int));
        }
      }
    }
  } catch (...) {
    // Ignore errors
  }
}

void DashboardPanel::SaveRecentEditors() {
  try {
    std::ostringstream ss;
    for (EditorType type : recent_editors_) {
      ss << static_cast<int>(type) << "\n";
    }
    util::SaveFile("recent_editors.txt", ss.str());
  } catch (...) {
    // Ignore save errors
  }
}

void DashboardPanel::ClearRecentEditors() {
  recent_editors_.clear();
  SaveRecentEditors();
}

}  // namespace editor
}  // namespace yaze
