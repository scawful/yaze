#include "app/gui/toolset.h"

#include "absl/strings/str_format.h"
#include "app/gui/icons.h"
#include "app/gui/ui_helpers.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

void Toolset::AddTool(const std::string& id, const char* icon,
                     const char* shortcut, std::function<void()> callback,
                     const char* tooltip) {
  Tool tool;
  tool.id = id;
  tool.icon = icon;
  tool.shortcut = shortcut;
  tool.callback = callback;
  
  if (tooltip) {
    tool.tooltip = std::string(icon) + " " + tooltip;
    if (shortcut && strlen(shortcut) > 0) {
      tool.tooltip += " (" + std::string(shortcut) + ")";
    }
  } else {
    tool.tooltip = id;
    if (shortcut && strlen(shortcut) > 0) {
      tool.tooltip += " (" + std::string(shortcut) + ")";
    }
  }
  
  tools_.push_back(tool);
}

void Toolset::AddSeparator() {
  if (!tools_.empty()) {
    tools_.back().separator_after = true;
  }
}

void Toolset::SetSelected(const std::string& id) {
  selected_ = id;
}

bool Toolset::Draw() {
  bool clicked = false;
  
  // Calculate columns based on mode
  int columns = max_columns_;
  if (columns == 0) {
    // Auto-fit to available width
    float button_width = compact_mode_ ? 32.0f : 80.0f;
    columns = static_cast<int>(ImGui::GetContentRegionAvail().x / button_width);
    if (columns < 1) columns = 1;
    if (columns > static_cast<int>(tools_.size())) 
      columns = static_cast<int>(tools_.size());
  }
  
  // Modern toolset with reduced padding
  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(2, 2));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
  
  if (ImGui::BeginTable("##Toolset", columns, 
                       ImGuiTableFlags_SizingFixedFit | 
                       ImGuiTableFlags_BordersInnerV)) {
    
    for (const auto& tool : tools_) {
      if (!tool.enabled) continue;
      
      ImGui::TableNextColumn();
      
      bool is_selected = (tool.id == selected_);
      DrawTool(tool, is_selected);
      
      if (tool.separator_after) {
        ImGui::TableNextColumn();
        ImGui::Dummy(ImVec2(1, 1));  // Small separator space
      }
    }
    
    ImGui::EndTable();
  }
  
  ImGui::PopStyleVar(2);
  
  return clicked;
}

void Toolset::DrawTool(const Tool& tool, bool is_selected) {
  ImVec2 button_size = compact_mode_ ? ImVec2(28, 28) : ImVec2(0, 0);
  
  if (is_selected) {
    ImGui::PushStyleColor(ImGuiCol_Button, GetAccentColor());
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
      ImVec4(GetAccentColor().x * 1.1f, GetAccentColor().y * 1.1f,
             GetAccentColor().z * 1.1f, GetAccentColor().w));
  }
  
  bool clicked = false;
  if (compact_mode_) {
    clicked = ImGui::Button(tool.icon.c_str(), button_size);
  } else {
    std::string label = tool.icon + " " + tool.id;
    clicked = ImGui::Button(label.c_str(), button_size);
  }
  
  if (is_selected) {
    ImGui::PopStyleColor(2);
  }
  
  if (clicked && tool.callback) {
    tool.callback();
    selected_ = tool.id;
  }
  
  if (ImGui::IsItemHovered() && !tool.tooltip.empty()) {
    ImGui::SetTooltip("%s", tool.tooltip.c_str());
  }
}

void Toolset::Clear() {
  tools_.clear();
  selected_.clear();
}

// ============================================================================
// Editor Toolset Factories
// ============================================================================

namespace EditorToolset {

Toolset CreateOverworldToolset() {
  Toolset toolset;
  toolset.SetCompactMode(true);
  
  // Editing tools
  toolset.AddTool("Pan", ICON_MD_PAN_TOOL_ALT, "1", nullptr, 
                 "Pan - Middle click and drag");
  toolset.AddTool("Draw", ICON_MD_DRAW, "2", nullptr,
                 "Draw Tile");
  toolset.AddTool("Entrances", ICON_MD_DOOR_FRONT, "3", nullptr,
                 "Entrances");
  toolset.AddTool("Exits", ICON_MD_DOOR_BACK, "4", nullptr,
                 "Exits");
  toolset.AddTool("Items", ICON_MD_GRASS, "5", nullptr,
                 "Items");
  toolset.AddTool("Sprites", ICON_MD_PEST_CONTROL_RODENT, "6", nullptr,
                 "Sprites");
  toolset.AddTool("Transports", ICON_MD_ADD_LOCATION, "7", nullptr,
                 "Transports");
  toolset.AddTool("Music", ICON_MD_MUSIC_NOTE, "8", nullptr,
                 "Music");
  
  toolset.AddSeparator();
  
  // View controls
  toolset.AddTool("ZoomOut", ICON_MD_ZOOM_OUT, "", nullptr,
                 "Zoom Out");
  toolset.AddTool("ZoomIn", ICON_MD_ZOOM_IN, "", nullptr,
                 "Zoom In");
  toolset.AddTool("Fullscreen", ICON_MD_OPEN_IN_FULL, "F11", nullptr,
                 "Fullscreen Canvas");
  
  toolset.AddSeparator();
  
  // Quick access
  toolset.AddTool("Tile16", ICON_MD_GRID_VIEW, "Ctrl+T", nullptr,
                 "Tile16 Editor");
  toolset.AddTool("Copy", ICON_MD_CONTENT_COPY, "", nullptr,
                 "Copy Map");
  
  return toolset;
}

Toolset CreateDungeonToolset() {
  Toolset toolset;
  toolset.SetCompactMode(true);
  
  toolset.AddTool("Pan", ICON_MD_PAN_TOOL_ALT, "1", nullptr,
                 "Pan");
  toolset.AddTool("Draw", ICON_MD_DRAW, "2", nullptr,
                 "Draw Object");
  toolset.AddTool("Select", ICON_MD_SELECT_ALL, "3", nullptr,
                 "Select");
  
  toolset.AddSeparator();
  
  toolset.AddTool("ZoomOut", ICON_MD_ZOOM_OUT, "", nullptr,
                 "Zoom Out");
  toolset.AddTool("ZoomIn", ICON_MD_ZOOM_IN, "", nullptr,
                 "Zoom In");
  
  return toolset;
}

}  // namespace EditorToolset

}  // namespace gui
}  // namespace yaze
