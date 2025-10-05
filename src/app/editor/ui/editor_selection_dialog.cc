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
  // Initialize editor metadata
  editors_ = {
    {EditorType::kOverworld, "Overworld", ICON_MD_MAP,
     "Edit overworld maps, entrances, and properties", "Ctrl+1", false, true},
    
    {EditorType::kDungeon, "Dungeon", ICON_MD_CASTLE,
     "Design dungeon rooms, layouts, and mechanics", "Ctrl+2", false, true},
    
    {EditorType::kGraphics, "Graphics", ICON_MD_PALETTE,
     "Modify tiles, palettes, and graphics sets", "Ctrl+3", false, true},
    
    {EditorType::kSprite, "Sprites", ICON_MD_EMOJI_EMOTIONS,
     "Edit sprite graphics and properties", "Ctrl+4", false, true},
    
    {EditorType::kMessage, "Messages", ICON_MD_CHAT_BUBBLE,
     "Edit dialogue, signs, and text", "Ctrl+5", false, true},
    
    {EditorType::kMusic, "Music", ICON_MD_MUSIC_NOTE,
     "Configure music and sound effects", "Ctrl+6", false, true},
    
    {EditorType::kPalette, "Palettes", ICON_MD_COLOR_LENS,
     "Edit color palettes and animations", "Ctrl+7", false, true},
    
    {EditorType::kScreen, "Screens", ICON_MD_TV,
     "Edit title screen and ending screens", "Ctrl+8", false, true},
    
    {EditorType::kAssembly, "Assembly", ICON_MD_CODE,
     "Write and edit assembly code", "Ctrl+9", false, false},
    
    {EditorType::kMemory, "Hex Editor", ICON_MD_DATA_ARRAY,
     "Direct ROM memory editing", "Ctrl+0", false, true},
    
    {EditorType::kSettings, "Settings", ICON_MD_SETTINGS,
     "Configure ROM and project settings", "", false, true},
  };
  
  LoadRecentEditors();
}

bool EditorSelectionDialog::Show(bool* p_open) {
  if (!is_open_) {
    return false;
  }
  
  bool editor_selected = false;
  
  // Center the dialog
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_Appearing);
  
  if (ImGui::Begin("Editor Selection", p_open ? p_open : &is_open_,
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
        DrawEditorCard(editors_[i], static_cast<int>(i));
        
        if (selected_editor_ != EditorType::kNone) {
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
  
  if (editor_selected) {
    is_open_ = false;
  }
  
  return editor_selected;
}

void EditorSelectionDialog::DrawWelcomeHeader() {
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);  // Larger font if available
  
  ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                     ICON_MD_EDIT " Select an Editor");
  
  ImGui::PopFont();
  
  ImGui::TextWrapped("Choose an editor to begin working on your ROM. "
                    "You can open multiple editors simultaneously.");
}

void EditorSelectionDialog::DrawQuickAccessButtons() {
  ImGui::Text(ICON_MD_HISTORY " Recently Used");
  ImGui::Spacing();
  
  for (EditorType type : recent_editors_) {
    // Find editor info
    auto it = std::find_if(editors_.begin(), editors_.end(),
                          [type](const EditorInfo& info) {
                            return info.type == type;
                          });
    
    if (it != editors_.end()) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 0.6f));
      
      if (ImGui::Button(absl::StrCat(it->icon, " ", it->name).c_str(),
                       ImVec2(150, 30))) {
        selected_editor_ = type;
      }
      
      ImGui::PopStyleColor();
      
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
  
  // Card styling
  bool is_recent = std::find(recent_editors_.begin(), recent_editors_.end(),
                             info.type) != recent_editors_.end();
  
  if (is_recent) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.85f, 0.4f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.65f, 0.95f, 0.8f));
  }
  
  // Editor button with icon
  ImVec2 button_size(180, 100);
  if (ImGui::Button(absl::StrCat(info.icon, "\n", info.name).c_str(),
                   button_size)) {
    selected_editor_ = info.type;
  }
  
  if (is_recent) {
    ImGui::PopStyleColor(3);
  }
  
  // Tooltip with description
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("%s %s", info.icon, info.name);
    ImGui::Separator();
    ImGui::TextWrapped("%s", info.description);
    if (info.shortcut && info.shortcut[0]) {
      ImGui::Spacing();
      ImGui::TextDisabled("Shortcut: %s", info.shortcut);
    }
    ImGui::EndTooltip();
  }
  
  // Recent indicator
  if (is_recent) {
    ImGui::SameLine();
    ImGui::TextDisabled(ICON_MD_STAR);
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
        if (type_int >= 0 && type_int < static_cast<int>(EditorType::kLast)) {
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
