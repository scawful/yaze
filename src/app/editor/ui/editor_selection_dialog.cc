#include "app/editor/ui/editor_selection_dialog.h"

#include <sstream>
#include <fstream>
#include <algorithm>

#include "absl/strings/str_cat.h"
#include "imgui/imgui.h"
#include "app/gui/icons.h"
#include "app/gui/style.h"
#include "util/file_util.h"

namespace yaze {
namespace editor {

EditorSelectionDialog::EditorSelectionDialog() {
  // Initialize editor metadata with distinct colors
  editors_ = {
    {EditorType::kOverworld, "Overworld", ICON_MD_MAP,
     "Edit overworld maps, entrances, and properties", "Ctrl+1", false, true,
     ImVec4(0.133f, 0.545f, 0.133f, 1.0f)},  // Hyrule green
    
    {EditorType::kDungeon, "Dungeon", ICON_MD_CASTLE,
     "Design dungeon rooms, layouts, and mechanics", "Ctrl+2", false, true,
     ImVec4(0.502f, 0.0f, 0.502f, 1.0f)},  // Ganon purple
    
    {EditorType::kGraphics, "Graphics", ICON_MD_PALETTE,
     "Modify tiles, palettes, and graphics sets", "Ctrl+3", false, true,
     ImVec4(1.0f, 0.843f, 0.0f, 1.0f)},  // Triforce gold
    
    {EditorType::kSprite, "Sprites", ICON_MD_EMOJI_EMOTIONS,
     "Edit sprite graphics and properties", "Ctrl+4", false, true,
     ImVec4(1.0f, 0.647f, 0.0f, 1.0f)},  // Spirit orange
    
    {EditorType::kMessage, "Messages", ICON_MD_CHAT_BUBBLE,
     "Edit dialogue, signs, and text", "Ctrl+5", false, true,
     ImVec4(0.196f, 0.6f, 0.8f, 1.0f)},  // Master sword blue
    
    {EditorType::kMusic, "Music", ICON_MD_MUSIC_NOTE,
     "Configure music and sound effects", "Ctrl+6", false, true,
     ImVec4(0.416f, 0.353f, 0.804f, 1.0f)},  // Shadow purple
    
    {EditorType::kPalette, "Palettes", ICON_MD_COLOR_LENS,
     "Edit color palettes and animations", "Ctrl+7", false, true,
     ImVec4(0.863f, 0.078f, 0.235f, 1.0f)},  // Heart red
    
    {EditorType::kScreen, "Screens", ICON_MD_TV,
     "Edit title screen and ending screens", "Ctrl+8", false, true,
     ImVec4(0.4f, 0.8f, 1.0f, 1.0f)},  // Sky blue
    
    {EditorType::kAssembly, "Assembly", ICON_MD_CODE,
     "Write and edit assembly code", "Ctrl+9", false, false,
     ImVec4(0.8f, 0.8f, 0.8f, 1.0f)},  // Silver

    {EditorType::kHex, "Hex Editor", ICON_MD_DATA_ARRAY,
     "Direct ROM memory editing and comparison", "Ctrl+0", false, true,
     ImVec4(0.2f, 0.8f, 0.4f, 1.0f)},  // Matrix green
    
    {EditorType::kSettings, "Settings", ICON_MD_SETTINGS,
     "Configure ROM and project settings", "", false, true,
     ImVec4(0.6f, 0.6f, 0.6f, 1.0f)},  // Gray
  };
  
  LoadRecentEditors();
}

bool EditorSelectionDialog::Show(bool* p_open) {
  // Sync internal state with external flag
  if (p_open && *p_open && !is_open_) {
    is_open_ = true;
  }
  
  if (!is_open_) {
    if (p_open) *p_open = false;
    return false;
  }
  
  bool editor_selected = false;
  bool* window_open = p_open ? p_open : &is_open_;
  
  // Set window properties immediately before Begin to prevent them from affecting tooltips
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(950, 650), ImGuiCond_Appearing);  // Slightly larger for better layout
  
  if (ImGui::Begin("Editor Selection", window_open,
                   ImGuiWindowFlags_NoCollapse)) {
    DrawWelcomeHeader();
    
    ImGui::Separator();
    ImGui::Spacing();
    
    // Quick access buttons for recently used
    if (!recent_editors_.empty()) {
      DrawQuickAccessButtons();
      ImGui::Separator();
      ImGui::Spacing();
    }
    
    // Main editor grid
    ImGui::Text(ICON_MD_APPS " All Editors");
    ImGui::Spacing();
    
    float button_size = 200.0f;
    int columns = static_cast<int>(ImGui::GetContentRegionAvail().x / button_size);
    columns = std::max(columns, 1);
    
    if (ImGui::BeginTable("##EditorGrid", columns,
                          ImGuiTableFlags_None)) {
      for (size_t i = 0; i < editors_.size(); ++i) {
        ImGui::TableNextColumn();
        
        EditorType prev_selection = selected_editor_;
        DrawEditorCard(editors_[i], static_cast<int>(i));
        
        // Check if an editor was just selected
        if (selected_editor_ != prev_selection) {
          editor_selected = true;
          MarkRecentlyUsed(selected_editor_);
          if (selection_callback_) {
            selection_callback_(selected_editor_);
          }
        }
      }
      ImGui::EndTable();
    }
  }
  ImGui::End();
  
  // Sync state back
  if (p_open && !(*p_open)) {
    is_open_ = false;
  }
  
  if (editor_selected) {
    is_open_ = false;
    if (p_open) *p_open = false;
  }
  
  return editor_selected;
}

void EditorSelectionDialog::DrawWelcomeHeader() {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 header_start = ImGui::GetCursorScreenPos();
  
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);  // Large font
  
  // Colorful gradient title
  ImVec4 title_color = ImVec4(1.0f, 0.843f, 0.0f, 1.0f);  // Triforce gold
  ImGui::TextColored(title_color, ICON_MD_EDIT " Select an Editor");
  
  ImGui::PopFont();
  
  // Subtitle with gradient separator
  ImVec2 subtitle_pos = ImGui::GetCursorScreenPos();
  ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
                    "Choose an editor to begin working on your ROM. "
                    "You can open multiple editors simultaneously.");
}

void EditorSelectionDialog::DrawQuickAccessButtons() {
  ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f), ICON_MD_HISTORY " Recently Used");
  ImGui::Spacing();
  
  for (EditorType type : recent_editors_) {
    // Find editor info
    auto it = std::find_if(editors_.begin(), editors_.end(),
                          [type](const EditorInfo& info) {
                            return info.type == type;
                          });
    
    if (it != editors_.end()) {
      // Use editor's theme color for button
      ImVec4 color = it->color;
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x * 0.5f, color.y * 0.5f, 
                                                     color.z * 0.5f, 0.7f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 0.7f, color.y * 0.7f, 
                                                            color.z * 0.7f, 0.9f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
      
      if (ImGui::Button(absl::StrCat(it->icon, " ", it->name).c_str(),
                       ImVec2(150, 35))) {
        selected_editor_ = type;
      }
      
      ImGui::PopStyleColor(3);
      
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", it->description);
      }
      
      ImGui::SameLine();
    }
  }
  
  ImGui::NewLine();
}

void EditorSelectionDialog::DrawEditorCard(const EditorInfo& info, int index) {
  ImGui::PushID(index);
  
  ImVec2 button_size(180, 120);
  ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  
  // Card styling with gradients
  bool is_recent = std::find(recent_editors_.begin(), recent_editors_.end(),
                             info.type) != recent_editors_.end();
  
  // Create gradient background
  ImVec4 base_color = info.color;
  ImU32 color_top = ImGui::GetColorU32(ImVec4(base_color.x * 0.4f, base_color.y * 0.4f, 
                                               base_color.z * 0.4f, 0.8f));
  ImU32 color_bottom = ImGui::GetColorU32(ImVec4(base_color.x * 0.2f, base_color.y * 0.2f, 
                                                  base_color.z * 0.2f, 0.9f));
  
  // Draw gradient card background
  draw_list->AddRectFilledMultiColor(
      cursor_pos,
      ImVec2(cursor_pos.x + button_size.x, cursor_pos.y + button_size.y),
      color_top, color_top, color_bottom, color_bottom);
  
  // Colored border
  ImU32 border_color = is_recent 
      ? ImGui::GetColorU32(ImVec4(base_color.x, base_color.y, base_color.z, 1.0f))
      : ImGui::GetColorU32(ImVec4(base_color.x * 0.6f, base_color.y * 0.6f, 
                                   base_color.z * 0.6f, 0.7f));
  draw_list->AddRect(cursor_pos, 
                    ImVec2(cursor_pos.x + button_size.x, cursor_pos.y + button_size.y),
                    border_color, 4.0f, 0, is_recent ? 3.0f : 2.0f);
  
  // Recent indicator badge
  if (is_recent) {
    ImVec2 badge_pos(cursor_pos.x + button_size.x - 25, cursor_pos.y + 5);
    draw_list->AddCircleFilled(badge_pos, 12, ImGui::GetColorU32(base_color), 16);
    ImGui::SetCursorScreenPos(ImVec2(badge_pos.x - 6, badge_pos.y - 8));
    ImGui::TextColored(ImVec4(1, 1, 1, 1), ICON_MD_STAR);
  }
  
  // Make button transparent (we draw our own background)
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(base_color.x * 0.3f, base_color.y * 0.3f, 
                                                        base_color.z * 0.3f, 0.5f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(base_color.x * 0.5f, base_color.y * 0.5f, 
                                                       base_color.z * 0.5f, 0.7f));
  
  ImGui::SetCursorScreenPos(cursor_pos);
  bool clicked = ImGui::Button(absl::StrCat("##", info.name).c_str(), button_size);
  bool is_hovered = ImGui::IsItemHovered();
  
  ImGui::PopStyleColor(3);
  
  // Draw icon with colored background circle
  ImVec2 icon_center(cursor_pos.x + button_size.x / 2, cursor_pos.y + 30);
  ImU32 icon_bg = ImGui::GetColorU32(base_color);
  draw_list->AddCircleFilled(icon_center, 22, icon_bg, 32);
  
  // Draw icon
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]); // Larger font for icon
  ImVec2 icon_size = ImGui::CalcTextSize(info.icon);
  ImGui::SetCursorScreenPos(ImVec2(icon_center.x - icon_size.x / 2, icon_center.y - icon_size.y / 2));
  ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", info.icon);
  ImGui::PopFont();
  
  // Draw name
  ImGui::SetCursorScreenPos(ImVec2(cursor_pos.x + 10, cursor_pos.y + 65));
  ImGui::PushTextWrapPos(cursor_pos.x + button_size.x - 10);
  ImVec2 name_size = ImGui::CalcTextSize(info.name);
  ImGui::SetCursorScreenPos(ImVec2(cursor_pos.x + (button_size.x - name_size.x) / 2, 
                                    cursor_pos.y + 65));
  ImGui::TextColored(base_color, "%s", info.name);
  ImGui::PopTextWrapPos();
  
  // Draw shortcut hint if available
  if (info.shortcut && info.shortcut[0]) {
    ImGui::SetCursorScreenPos(ImVec2(cursor_pos.x + 10, cursor_pos.y + button_size.y - 20));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::Text("%s", info.shortcut);
    ImGui::PopStyleColor();
  }
  
  // Hover glow effect
  if (is_hovered) {
    ImU32 glow_color = ImGui::GetColorU32(ImVec4(base_color.x, base_color.y, base_color.z, 0.2f));
    draw_list->AddRectFilled(cursor_pos, 
                            ImVec2(cursor_pos.x + button_size.x, cursor_pos.y + button_size.y),
                            glow_color, 4.0f);
  }
  
  // Enhanced tooltip with fixed sizing
  if (is_hovered) {
    // Force tooltip to have a fixed max width to prevent flickering
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);
    ImGui::BeginTooltip();
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]); // Medium font
    ImGui::TextColored(base_color, "%s %s", info.icon, info.name);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 280);
    ImGui::TextWrapped("%s", info.description);
    ImGui::PopTextWrapPos();
    if (info.shortcut && info.shortcut[0]) {
      ImGui::Spacing();
      ImGui::TextColored(base_color, ICON_MD_KEYBOARD " %s", info.shortcut);
    }
    if (is_recent) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f), ICON_MD_STAR " Recently used");
    }
    ImGui::EndTooltip();
  }
  
  if (clicked) {
    selected_editor_ = info.type;
  }
  
  ImGui::PopID();
}

void EditorSelectionDialog::MarkRecentlyUsed(EditorType type) {
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

void EditorSelectionDialog::LoadRecentEditors() {
  try {
    auto data = util::LoadConfigFile("recent_editors.txt");
    if (!data.empty()) {
      std::istringstream ss(data);
      std::string line;
      while (std::getline(ss, line) && 
             recent_editors_.size() < kMaxRecentEditors) {
        int type_int = std::stoi(line);
        if (type_int >= 0 && type_int < static_cast<int>(EditorType::kSettings)) {
          recent_editors_.push_back(static_cast<EditorType>(type_int));
        }
      }
    }
  } catch (...) {
    // Ignore errors, just start with empty recent list
  }
}

void EditorSelectionDialog::SaveRecentEditors() {
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

}  // namespace editor
}  // namespace yaze
